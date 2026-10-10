// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <httprpc.h>

#include <base58.h>
#include <chainparams.h>
#include <crypto/hmac_sha256.h>
#include <httpserver.h>
#include <random.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <stdio.h>
#include <sync.h>
#include <ui_interface.h>
#include <util.h>
#include <utilstrencodings.h>

#include <boost/algorithm/string.hpp>

static const char *WWW_AUTH_HEADER_DATA = "Basic realm=\"jsonrpc\"";

class HTTPRPCTimer : public RPCTimerBase {
public:
  HTTPRPCTimer(struct event_base *eventBase, std::function<void(void)> &func,
               int64_t millis)
      : ev(eventBase, false, func) {
    struct timeval tv;
    tv.tv_sec = millis / 1000;
    tv.tv_usec = (millis % 1000) * 1000;
    ev.trigger(&tv);
  }

private:
  HTTPEvent ev;
};

class HTTPRPCTimerInterface : public RPCTimerInterface {
public:
  explicit HTTPRPCTimerInterface(struct event_base *_base) : base(_base) {}
  const char *Name() override { return "HTTP"; }
  RPCTimerBase *NewTimer(std::function<void(void)> &func,
                         int64_t millis) override {
    return new HTTPRPCTimer(base, func, millis);
  }

private:
  struct event_base *base;
};

static std::string strRPCUserColonPass;

static std::unique_ptr<HTTPRPCTimerInterface> httpRPCTimerInterface;

static void JSONErrorReply(HTTPRequest *req, const UniValue &objError,
                           const UniValue &id) {
  int nStatus = HTTP_INTERNAL_SERVER_ERROR;
  int code = find_value(objError, "code").get_int();

  if (code == RPC_INVALID_REQUEST)
    nStatus = HTTP_BAD_REQUEST;
  else if (code == RPC_METHOD_NOT_FOUND)
    nStatus = HTTP_NOT_FOUND;

  std::string strReply = JSONRPCReply(NullUniValue, objError, id);

  req->WriteHeader("Content-Type", "application/json");
  req->WriteReply(nStatus, strReply);
}

static bool multiUserAuthorized(std::string strUserPass) {
  if (strUserPass.find(':') == std::string::npos) {
    return false;
  }
  std::string strUser = strUserPass.substr(0, strUserPass.find(':'));
  std::string strPass = strUserPass.substr(strUserPass.find(':') + 1);

  for (const std::string &strRPCAuth : gArgs.GetArgs("-rpcauth")) {
    std::vector<std::string> vFields;
    boost::split(vFields, strRPCAuth, boost::is_any_of(":$"));
    if (vFields.size() != 3) {
      continue;
    }

    std::string strName = vFields[0];
    if (!TimingResistantEqual(strName, strUser)) {
      continue;
    }

    std::string strSalt = vFields[1];
    std::string strHash = vFields[2];

    static const unsigned int KEY_SIZE = 32;
    unsigned char out[KEY_SIZE];

    CHMAC_SHA256(reinterpret_cast<const unsigned char *>(strSalt.c_str()),
                 strSalt.size())
        .Write(reinterpret_cast<const unsigned char *>(strPass.c_str()),
               strPass.size())
        .Finalize(out);
    std::vector<unsigned char> hexvec(out, out + KEY_SIZE);
    std::string strHashFromPass = HexStr(hexvec);

    if (TimingResistantEqual(strHashFromPass, strHash)) {
      return true;
    }
  }
  return false;
}

static bool RPCAuthorized(const std::string &strAuth,
                          std::string &strAuthUsernameOut) {
  if (strRPCUserColonPass.empty())

    return false;
  if (strAuth.substr(0, 6) != "Basic ")
    return false;
  std::string strUserPass64 = strAuth.substr(6);
  boost::trim(strUserPass64);
  std::string strUserPass = DecodeBase64(strUserPass64);

  if (strUserPass.find(':') != std::string::npos)
    strAuthUsernameOut = strUserPass.substr(0, strUserPass.find(':'));

  if (TimingResistantEqual(strUserPass, strRPCUserColonPass)) {
    return true;
  }
  return multiUserAuthorized(strUserPass);
}

static bool HTTPReq_JSONRPC(HTTPRequest *req, const std::string &) {
  if (req->GetRequestMethod() != HTTPRequest::POST) {
    req->WriteReply(HTTP_BAD_METHOD,
                    "JSONRPC server handles only POST requests");
    return false;
  }

  std::pair<bool, std::string> authHeader = req->GetHeader("authorization");
  if (!authHeader.first) {
    req->WriteHeader("WWW-Authenticate", WWW_AUTH_HEADER_DATA);
    req->WriteReply(HTTP_UNAUTHORIZED);
    return false;
  }

  JSONRPCRequest jreq;
  if (!RPCAuthorized(authHeader.second, jreq.authUser)) {
    LogPrintf("ThreadRPCServer incorrect password attempt from %s\n",
              req->GetPeer().ToString());

    MilliSleep(250);

    req->WriteHeader("WWW-Authenticate", WWW_AUTH_HEADER_DATA);
    req->WriteReply(HTTP_UNAUTHORIZED);
    return false;
  }

  try {
    UniValue valRequest;
    if (!valRequest.read(req->ReadBody()))
      throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

    jreq.URI = req->GetURI();

    std::string strReply;

    if (valRequest.isObject()) {
      jreq.parse(valRequest);

      UniValue result = tableRPC.execute(jreq);

      strReply = JSONRPCReply(result, NullUniValue, jreq.id);

    } else if (valRequest.isArray())
      strReply = JSONRPCExecBatch(jreq, valRequest.get_array());
    else
      throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

    req->WriteHeader("Content-Type", "application/json");
    req->WriteReply(HTTP_OK, strReply);
  } catch (const UniValue &objError) {
    JSONErrorReply(req, objError, jreq.id);
    return false;
  } catch (const std::exception &e) {
    JSONErrorReply(req, JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
    return false;
  }
  return true;
}

static bool InitRPCAuthentication() {
  if (gArgs.GetArg("-rpcpassword", "") == "") {
    LogPrintf("No rpcpassword set - using random cookie authentication\n");
    if (!GenerateAuthCookie(&strRPCUserColonPass)) {
      uiInterface.ThreadSafeMessageBox(_("Error: A fatal internal error "
                                         "occurred, see debug.log for details"),

                                       "", CClientUIInterface::MSG_ERROR);
      return false;
    }
  } else {
    LogPrintf("Config options rpcuser and rpcpassword will soon be deprecated. "
              "Locally-run instances may remove rpcuser to use cookie-based "
              "auth, or may be replaced with rpcauth. Please see share/rpcuser "
              "for rpcauth auth generation.\n");
    strRPCUserColonPass =
        gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
  }
  return true;
}

bool StartHTTPRPC() {
  LogPrint(BCLog::RPC, "Starting HTTP RPC server\n");
  if (!InitRPCAuthentication())
    return false;

  RegisterHTTPHandler("/", true, HTTPReq_JSONRPC);
#ifdef ENABLE_WALLET

  RegisterHTTPHandler("/wallet/", false, HTTPReq_JSONRPC);
#endif
  assert(EventBase());
  httpRPCTimerInterface = MakeUnique<HTTPRPCTimerInterface>(EventBase());
  RPCSetTimerInterface(httpRPCTimerInterface.get());
  return true;
}

void InterruptHTTPRPC() {
  LogPrint(BCLog::RPC, "Interrupting HTTP RPC server\n");
}

void StopHTTPRPC() {
  LogPrint(BCLog::RPC, "Stopping HTTP RPC server\n");
  UnregisterHTTPHandler("/", true);
  if (httpRPCTimerInterface) {
    RPCUnsetTimerInterface(httpRPCTimerInterface.get());
    httpRPCTimerInterface.reset();
  }
}
