// Copyright (c) 2015-2017 The Bitcoin Core developers
// Copyright (c) 2025 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/hmac_sha256.h>
#include <net.h>
#include <netbase.h>
#include <torcontrol.h>
#include <util.h>
#include <utilstrencodings.h>

#include <deque>
#include <set>
#include <stdlib.h>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/bind.hpp>
#include <boost/signals2/signal.hpp>

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <event2/util.h>

const std::string DEFAULT_TOR_CONTROL = "127.0.0.1:9051";

static const float RECONNECT_TIMEOUT_EXP = 1.5;
static const float RECONNECT_TIMEOUT_START = 1.0;
static const int MAX_LINE_LENGTH = 100000;
static const int TOR_COOKIE_SIZE = 32;
static const int TOR_NONCE_SIZE = 32;

static const std::string TOR_SAFE_CLIENTKEY =
    "Tor safe cookie authentication controller-to-server hash";
static const std::string TOR_SAFE_SERVERKEY =
    "Tor safe cookie authentication server-to-controller hash";

class TorControlReply {
public:
  TorControlReply() { Clear(); }

  int code;
  std::vector<std::string> lines;

  void Clear() {
    code = 0;
    lines.clear();
  }
};

class TorControlConnection {
public:
  typedef std::function<void(TorControlConnection &)> ConnectionCB;
  typedef std::function<void(TorControlConnection &, const TorControlReply &)>
      ReplyHandlerCB;

  explicit TorControlConnection(struct event_base *base);
  ~TorControlConnection();

  bool Connect(const std::string &target, const ConnectionCB &connected,
               const ConnectionCB &disconnected);

  bool Disconnect();

  bool Command(const std::string &cmd, const ReplyHandlerCB &reply_handler);

  boost::signals2::signal<void(TorControlConnection &, const TorControlReply &)>
      async_handler;

private:
  std::function<void(TorControlConnection &)> connected;

  std::function<void(TorControlConnection &)> disconnected;

  struct event_base *base;

  struct bufferevent *b_conn;

  TorControlReply message;

  std::deque<ReplyHandlerCB> reply_handlers;

  static void readcb(struct bufferevent *bev, void *ctx);
  static void eventcb(struct bufferevent *bev, short what, void *ctx);
};

TorControlConnection::TorControlConnection(struct event_base *_base)
    : base(_base), b_conn(nullptr) {}

TorControlConnection::~TorControlConnection() {
  if (b_conn)
    bufferevent_free(b_conn);
}

void TorControlConnection::readcb(struct bufferevent *bev, void *ctx) {
  TorControlConnection *self = (TorControlConnection *)ctx;
  struct evbuffer *input = bufferevent_get_input(bev);
  size_t n_read_out = 0;
  char *line;
  assert(input);

  while ((line = evbuffer_readln(input, &n_read_out, EVBUFFER_EOL_CRLF)) !=
         nullptr) {
    std::string s(line, n_read_out);
    free(line);
    if (s.size() < 4)

      continue;

    self->message.code = atoi(s.substr(0, 3));
    self->message.lines.push_back(s.substr(4));
    char ch = s[3];

    if (ch == ' ') {
      if (self->message.code >= 600) {
        self->async_handler(*self, self->message);
      } else {
        if (!self->reply_handlers.empty()) {
          self->reply_handlers.front()(*self, self->message);
          self->reply_handlers.pop_front();
        } else {
          LogPrint(BCLog::TOR, "tor: Received unexpected sync reply %i\n",
                   self->message.code);
        }
      }
      self->message.Clear();
    }
  }

  if (evbuffer_get_length(input) > MAX_LINE_LENGTH) {
    LogPrintf("tor: Disconnecting because MAX_LINE_LENGTH exceeded\n");
    self->Disconnect();
  }
}

void TorControlConnection::eventcb(struct bufferevent *bev, short what,
                                   void *ctx) {
  TorControlConnection *self = (TorControlConnection *)ctx;
  if (what & BEV_EVENT_CONNECTED) {
    LogPrint(BCLog::TOR, "tor: Successfully connected!\n");
    self->connected(*self);
  } else if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
    if (what & BEV_EVENT_ERROR) {
      LogPrint(BCLog::TOR, "tor: Error connecting to Tor control socket\n");
    } else {
      LogPrint(BCLog::TOR, "tor: End of stream\n");
    }
    self->Disconnect();
    self->disconnected(*self);
  }
}

bool TorControlConnection::Connect(const std::string &target,
                                   const ConnectionCB &_connected,
                                   const ConnectionCB &_disconnected) {
  if (b_conn)
    Disconnect();

  struct sockaddr_storage connect_to_addr;
  int connect_to_addrlen = sizeof(connect_to_addr);
  if (evutil_parse_sockaddr_port(target.c_str(),
                                 (struct sockaddr *)&connect_to_addr,
                                 &connect_to_addrlen) < 0) {
    LogPrintf("tor: Error parsing socket address %s\n", target);
    return false;
  }

  b_conn = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
  if (!b_conn)
    return false;
  bufferevent_setcb(b_conn, TorControlConnection::readcb, nullptr,
                    TorControlConnection::eventcb, this);
  bufferevent_enable(b_conn, EV_READ | EV_WRITE);
  this->connected = _connected;
  this->disconnected = _disconnected;

  if (bufferevent_socket_connect(b_conn, (struct sockaddr *)&connect_to_addr,
                                 connect_to_addrlen) < 0) {
    LogPrintf("tor: Error connecting to address %s\n", target);
    return false;
  }
  return true;
}

bool TorControlConnection::Disconnect() {
  if (b_conn)
    bufferevent_free(b_conn);
  b_conn = nullptr;
  return true;
}

bool TorControlConnection::Command(const std::string &cmd,
                                   const ReplyHandlerCB &reply_handler) {
  if (!b_conn)
    return false;
  struct evbuffer *buf = bufferevent_get_output(b_conn);
  if (!buf)
    return false;
  evbuffer_add(buf, cmd.data(), cmd.size());
  evbuffer_add(buf, "\r\n", 2);
  reply_handlers.push_back(reply_handler);
  return true;
}

static std::pair<std::string, std::string>
SplitTorReplyLine(const std::string &s) {
  size_t ptr = 0;
  std::string type;
  while (ptr < s.size() && s[ptr] != ' ') {
    type.push_back(s[ptr]);
    ++ptr;
  }
  if (ptr < s.size())
    ++ptr;

  return make_pair(type, s.substr(ptr));
}

static std::map<std::string, std::string>
ParseTorReplyMapping(const std::string &s) {
  std::map<std::string, std::string> mapping;
  size_t ptr = 0;
  while (ptr < s.size()) {
    std::string key, value;
    while (ptr < s.size() && s[ptr] != '=' && s[ptr] != ' ') {
      key.push_back(s[ptr]);
      ++ptr;
    }
    if (ptr == s.size())

      return std::map<std::string, std::string>();
    if (s[ptr] == ' ')

      break;
    ++ptr;

    if (ptr < s.size() && s[ptr] == '"') {
      ++ptr;

      bool escape_next = false;
      while (ptr < s.size() && (escape_next || s[ptr] != '"')) {
        escape_next = (s[ptr] == '\\' && !escape_next);
        value.push_back(s[ptr]);
        ++ptr;
      }
      if (ptr == s.size())

        return std::map<std::string, std::string>();
      ++ptr;

      std::string escaped_value;
      for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\') {
          ++i;
          if (value[i] == 'n') {
            escaped_value.push_back('\n');
          } else if (value[i] == 't') {
            escaped_value.push_back('\t');
          } else if (value[i] == 'r') {
            escaped_value.push_back('\r');
          } else if ('0' <= value[i] && value[i] <= '7') {
            size_t j;

            for (j = 1; j < 3 && (i + j) < value.size() &&
                        '0' <= value[i + j] && value[i + j] <= '7';
                 ++j) {
            }

            if (j == 3 && value[i] > '3') {
              j--;
            }
            escaped_value.push_back(
                strtol(value.substr(i, j).c_str(), nullptr, 8));

            i += j - 1;
          } else {
            escaped_value.push_back(value[i]);
          }
        } else {
          escaped_value.push_back(value[i]);
        }
      }
      value = escaped_value;
    } else {
      while (ptr < s.size() && s[ptr] != ' ') {
        value.push_back(s[ptr]);
        ++ptr;
      }
    }
    if (ptr < s.size() && s[ptr] == ' ')
      ++ptr;

    mapping[key] = value;
  }
  return mapping;
}

static std::pair<bool, std::string>
ReadBinaryFile(const fs::path &filename,
               size_t maxsize = std::numeric_limits<size_t>::max()) {
  FILE *f = fsbridge::fopen(filename, "rb");
  if (f == nullptr)
    return std::make_pair(false, "");
  std::string retval;
  char buffer[128];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
    if (ferror(f)) {
      fclose(f);
      return std::make_pair(false, "");
    }
    retval.append(buffer, buffer + n);
    if (retval.size() > maxsize)
      break;
  }
  fclose(f);
  return std::make_pair(true, retval);
}

static bool WriteBinaryFile(const fs::path &filename, const std::string &data) {
  FILE *f = fsbridge::fopen(filename, "wb");
  if (f == nullptr)
    return false;
  if (fwrite(data.data(), 1, data.size(), f) != data.size()) {
    fclose(f);
    return false;
  }
  fclose(f);
  return true;
}

class TorController {
public:
  TorController(struct event_base *base, const std::string &target);
  ~TorController();

  fs::path GetPrivateKeyFile();

  void Reconnect();

private:
  struct event_base *base;
  std::string target;
  TorControlConnection conn;
  std::string private_key;
  std::string service_id;
  bool reconnect;
  struct event *reconnect_ev;
  float reconnect_timeout;
  CService service;

  std::vector<uint8_t> cookie;

  std::vector<uint8_t> clientNonce;

  void add_onion_cb(TorControlConnection &conn, const TorControlReply &reply);

  void auth_cb(TorControlConnection &conn, const TorControlReply &reply);

  void authchallenge_cb(TorControlConnection &conn,
                        const TorControlReply &reply);

  void protocolinfo_cb(TorControlConnection &conn,
                       const TorControlReply &reply);

  void connected_cb(TorControlConnection &conn);

  void disconnected_cb(TorControlConnection &conn);

  static void reconnect_cb(evutil_socket_t fd, short what, void *arg);
};

TorController::TorController(struct event_base *_base,
                             const std::string &_target)
    : base(_base), target(_target), conn(base), reconnect(true),
      reconnect_ev(0), reconnect_timeout(RECONNECT_TIMEOUT_START) {
  reconnect_ev = event_new(base, -1, 0, reconnect_cb, this);
  if (!reconnect_ev)
    LogPrintf("tor: Failed to create event for reconnection: out of memory?\n");

  if (!conn.Connect(_target,
                    boost::bind(&TorController::connected_cb, this, _1),
                    boost::bind(&TorController::disconnected_cb, this, _1))) {
    LogPrintf("tor: Initiating connection to Tor control port %s failed\n",
              _target);
  }

  std::pair<bool, std::string> pkf = ReadBinaryFile(GetPrivateKeyFile());
  if (pkf.first) {
    LogPrint(BCLog::TOR, "tor: Reading cached private key from %s\n",
             GetPrivateKeyFile().string());
    private_key = pkf.second;
  }
}

TorController::~TorController() {
  if (reconnect_ev) {
    event_free(reconnect_ev);
    reconnect_ev = nullptr;
  }
  if (service.IsValid()) {
    RemoveLocal(service);
  }
}

void TorController::add_onion_cb(TorControlConnection &_conn,
                                 const TorControlReply &reply) {
  if (reply.code == 250) {
    LogPrint(BCLog::TOR, "tor: ADD_ONION successful\n");
    for (const std::string &s : reply.lines) {
      std::map<std::string, std::string> m = ParseTorReplyMapping(s);
      std::map<std::string, std::string>::iterator i;
      if ((i = m.find("ServiceID")) != m.end())
        service_id = i->second;
      if ((i = m.find("PrivateKey")) != m.end())
        private_key = i->second;
    }
    if (service_id.empty()) {
      LogPrintf("tor: Error parsing ADD_ONION parameters:\n");
      for (const std::string &s : reply.lines) {
        LogPrintf("    %s\n", SanitizeString(s));
      }
      return;
    }
    service = LookupNumeric(std::string(service_id + ".onion").c_str(),
                            GetListenPort());
    LogPrintf("tor: Got service ID %s, advertising service %s\n", service_id,
              service.ToString());
    if (WriteBinaryFile(GetPrivateKeyFile(), private_key)) {
      LogPrint(BCLog::TOR, "tor: Cached service private key to %s\n",
               GetPrivateKeyFile().string());
    } else {
      LogPrintf("tor: Error writing service private key to %s\n",
                GetPrivateKeyFile().string());
    }
    AddLocal(service, LOCAL_MANUAL);

  } else if (reply.code == 510) {
    LogPrintf("tor: Add onion failed with unrecognized command (You probably "
              "need to upgrade Tor)\n");
  } else {
    LogPrintf("tor: Add onion failed; error code %d\n", reply.code);
  }
}

void TorController::auth_cb(TorControlConnection &_conn,
                            const TorControlReply &reply) {
  if (reply.code == 250) {
    LogPrint(BCLog::TOR, "tor: Authentication successful\n");

    if (gArgs.GetArg("-onion", "") == "" && gArgs.GetArg("-proxy", "") == "") {
      CService resolved(LookupNumeric("127.0.0.1", 9050));
      proxyType addrOnion = proxyType(resolved, true);
      SetProxy(NET_TOR, addrOnion);
      SetLimited(NET_TOR, false);
    }

    if (private_key.empty())

      private_key = "NEW:RSA1024";

    _conn.Command(strprintf("ADD_ONION %s Port=%i,127.0.0.1:%i", private_key,
                            GetListenPort(), GetListenPort()),
                  boost::bind(&TorController::add_onion_cb, this, _1, _2));
  } else {
    LogPrintf("tor: Authentication failed\n");
  }
}

static std::vector<uint8_t>
ComputeResponse(const std::string &key, const std::vector<uint8_t> &cookie,
                const std::vector<uint8_t> &clientNonce,
                const std::vector<uint8_t> &serverNonce) {
  CHMAC_SHA256 computeHash((const uint8_t *)key.data(), key.size());
  std::vector<uint8_t> computedHash(CHMAC_SHA256::OUTPUT_SIZE, 0);
  computeHash.Write(cookie.data(), cookie.size());
  computeHash.Write(clientNonce.data(), clientNonce.size());
  computeHash.Write(serverNonce.data(), serverNonce.size());
  computeHash.Finalize(computedHash.data());
  return computedHash;
}

void TorController::authchallenge_cb(TorControlConnection &_conn,
                                     const TorControlReply &reply) {
  if (reply.code == 250) {
    LogPrint(BCLog::TOR,
             "tor: SAFECOOKIE authentication challenge successful\n");
    std::pair<std::string, std::string> l = SplitTorReplyLine(reply.lines[0]);
    if (l.first == "AUTHCHALLENGE") {
      std::map<std::string, std::string> m = ParseTorReplyMapping(l.second);
      if (m.empty()) {
        LogPrintf("tor: Error parsing AUTHCHALLENGE parameters: %s\n",
                  SanitizeString(l.second));
        return;
      }
      std::vector<uint8_t> serverHash = ParseHex(m["SERVERHASH"]);
      std::vector<uint8_t> serverNonce = ParseHex(m["SERVERNONCE"]);
      LogPrint(BCLog::TOR, "tor: AUTHCHALLENGE ServerHash %s ServerNonce %s\n",
               HexStr(serverHash), HexStr(serverNonce));
      if (serverNonce.size() != 32) {
        LogPrintf("tor: ServerNonce is not 32 bytes, as required by spec\n");
        return;
      }

      std::vector<uint8_t> computedServerHash =
          ComputeResponse(TOR_SAFE_SERVERKEY, cookie, clientNonce, serverNonce);
      if (computedServerHash != serverHash) {
        LogPrintf("tor: ServerHash %s does not match expected ServerHash %s\n",
                  HexStr(serverHash), HexStr(computedServerHash));
        return;
      }

      std::vector<uint8_t> computedClientHash =
          ComputeResponse(TOR_SAFE_CLIENTKEY, cookie, clientNonce, serverNonce);
      _conn.Command("AUTHENTICATE " + HexStr(computedClientHash),
                    boost::bind(&TorController::auth_cb, this, _1, _2));
    } else {
      LogPrintf("tor: Invalid reply to AUTHCHALLENGE\n");
    }
  } else {
    LogPrintf("tor: SAFECOOKIE authentication challenge failed\n");
  }
}

void TorController::protocolinfo_cb(TorControlConnection &_conn,
                                    const TorControlReply &reply) {
  if (reply.code == 250) {
    std::set<std::string> methods;
    std::string cookiefile;

    for (const std::string &s : reply.lines) {
      std::pair<std::string, std::string> l = SplitTorReplyLine(s);
      if (l.first == "AUTH") {
        std::map<std::string, std::string> m = ParseTorReplyMapping(l.second);
        std::map<std::string, std::string>::iterator i;
        if ((i = m.find("METHODS")) != m.end())
          boost::split(methods, i->second, boost::is_any_of(","));
        if ((i = m.find("COOKIEFILE")) != m.end())
          cookiefile = i->second;
      } else if (l.first == "VERSION") {
        std::map<std::string, std::string> m = ParseTorReplyMapping(l.second);
        std::map<std::string, std::string>::iterator i;
        if ((i = m.find("Tor")) != m.end()) {
          LogPrint(BCLog::TOR, "tor: Connected to Tor version %s\n", i->second);
        }
      }
    }
    for (const std::string &s : methods) {
      LogPrint(BCLog::TOR, "tor: Supported authentication method: %s\n", s);
    }

    std::string torpassword = gArgs.GetArg("-torpassword", "");
    if (!torpassword.empty()) {
      if (methods.count("HASHEDPASSWORD")) {
        LogPrint(BCLog::TOR, "tor: Using HASHEDPASSWORD authentication\n");
        boost::replace_all(torpassword, "\"", "\\\"");
        _conn.Command("AUTHENTICATE \"" + torpassword + "\"",
                      boost::bind(&TorController::auth_cb, this, _1, _2));
      } else {
        LogPrintf("tor: Password provided with -torpassword, but "
                  "HASHEDPASSWORD authentication is not available\n");
      }
    } else if (methods.count("NULL")) {
      LogPrint(BCLog::TOR, "tor: Using NULL authentication\n");
      _conn.Command("AUTHENTICATE",
                    boost::bind(&TorController::auth_cb, this, _1, _2));
    } else if (methods.count("SAFECOOKIE")) {
      LogPrint(BCLog::TOR,
               "tor: Using SAFECOOKIE authentication, reading cookie "
               "authentication from %s\n",
               cookiefile);
      std::pair<bool, std::string> status_cookie =
          ReadBinaryFile(cookiefile, TOR_COOKIE_SIZE);
      if (status_cookie.first &&
          status_cookie.second.size() == TOR_COOKIE_SIZE) {
        cookie = std::vector<uint8_t>(status_cookie.second.begin(),
                                      status_cookie.second.end());
        clientNonce = std::vector<uint8_t>(TOR_NONCE_SIZE, 0);
        GetRandBytes(clientNonce.data(), TOR_NONCE_SIZE);
        _conn.Command(
            "AUTHCHALLENGE SAFECOOKIE " + HexStr(clientNonce),
            boost::bind(&TorController::authchallenge_cb, this, _1, _2));
      } else {
        if (status_cookie.first) {
          LogPrintf("tor: Authentication cookie %s is not exactly %i bytes, as "
                    "is required by the spec\n",
                    cookiefile, TOR_COOKIE_SIZE);
        } else {
          LogPrintf("tor: Authentication cookie %s could not be opened (check "
                    "permissions)\n",
                    cookiefile);
        }
      }
    } else if (methods.count("HASHEDPASSWORD")) {
      LogPrintf("tor: The only supported authentication mechanism left is "
                "password, but no password provided with -torpassword\n");
    } else {
      LogPrintf("tor: No supported authentication method\n");
    }
  } else {
    LogPrintf("tor: Requesting protocol info failed\n");
  }
}

void TorController::connected_cb(TorControlConnection &_conn) {
  reconnect_timeout = RECONNECT_TIMEOUT_START;

  if (!_conn.Command(
          "PROTOCOLINFO 1",
          boost::bind(&TorController::protocolinfo_cb, this, _1, _2)))
    LogPrintf("tor: Error sending initial protocolinfo command\n");
}

void TorController::disconnected_cb(TorControlConnection &_conn) {
  if (service.IsValid())
    RemoveLocal(service);
  service = CService();
  if (!reconnect)
    return;

  LogPrint(BCLog::TOR,
           "tor: Not connected to Tor control port %s, trying to reconnect\n",
           target);

  struct timeval time = MillisToTimeval(int64_t(reconnect_timeout * 1000.0));
  if (reconnect_ev)
    event_add(reconnect_ev, &time);
  reconnect_timeout *= RECONNECT_TIMEOUT_EXP;
}

void TorController::Reconnect() {
  if (!conn.Connect(target, boost::bind(&TorController::connected_cb, this, _1),
                    boost::bind(&TorController::disconnected_cb, this, _1))) {
    LogPrintf("tor: Re-initiating connection to Tor control port %s failed\n",
              target);
  }
}

fs::path TorController::GetPrivateKeyFile() {
  return GetDataDir() / "onion_private_key";
}

void TorController::reconnect_cb(evutil_socket_t fd, short what, void *arg) {
  TorController *self = (TorController *)arg;
  self->Reconnect();
}

static struct event_base *gBase;
static boost::thread torControlThread;

static void TorControlThread() {
  TorController ctrl(gBase, gArgs.GetArg("-torcontrol", DEFAULT_TOR_CONTROL));

  event_base_dispatch(gBase);
}

void StartTorControl(boost::thread_group &threadGroup, CScheduler &scheduler) {
  assert(!gBase);
#ifdef WIN32
  evthread_use_windows_threads();
#else
  evthread_use_pthreads();
#endif
  gBase = event_base_new();
  if (!gBase) {
    LogPrintf("tor: Unable to create event_base\n");
    return;
  }

  torControlThread = boost::thread(
      boost::bind(&TraceThread<void (*)()>, "torcontrol", &TorControlThread));
}

void InterruptTorControl() {
  if (gBase) {
    LogPrintf("tor: Thread interrupt\n");
    event_base_loopbreak(gBase);
  }
}

void StopTorControl() {
  if (gBase) {
    torControlThread.join();
    event_base_free(gBase);
    gBase = nullptr;
  }
}