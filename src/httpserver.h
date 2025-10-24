// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HTTPSERVER_H
#define BITCOIN_HTTPSERVER_H

#include <functional>
#include <stdint.h>
#include <string>

static const int DEFAULT_HTTP_THREADS = 4;
static const int DEFAULT_HTTP_WORKQUEUE = 16;
static const int DEFAULT_HTTP_SERVER_TIMEOUT = 30;

struct evhttp_request;
struct event_base;
class CService;
class HTTPRequest;

bool InitHTTPServer();

bool StartHTTPServer();

void InterruptHTTPServer();

void StopHTTPServer();

bool UpdateHTTPServerLogging(bool enable);

typedef std::function<bool(HTTPRequest *req, const std::string &)>
    HTTPRequestHandler;

void RegisterHTTPHandler(const std::string &prefix, bool exactMatch,
                         const HTTPRequestHandler &handler);

void UnregisterHTTPHandler(const std::string &prefix, bool exactMatch);

struct event_base *EventBase();

class HTTPRequest {
private:
  struct evhttp_request *req;
  bool replySent;

public:
  explicit HTTPRequest(struct evhttp_request *req);
  ~HTTPRequest();

  enum RequestMethod { UNKNOWN, GET, POST, HEAD, PUT };

  std::string GetURI();

  CService GetPeer();

  RequestMethod GetRequestMethod();

  std::pair<bool, std::string> GetHeader(const std::string &hdr);

  std::string ReadBody();

  void WriteHeader(const std::string &hdr, const std::string &value);

  void WriteReply(int nStatus, const std::string &strReply = "");
};

class HTTPClosure {
public:
  virtual void operator()() = 0;
  virtual ~HTTPClosure() {}
};

class HTTPEvent {
public:
  HTTPEvent(struct event_base *base, bool deleteWhenTriggered,
            const std::function<void(void)> &handler);
  ~HTTPEvent();

  void trigger(struct timeval *tv);

  bool deleteWhenTriggered;
  std::function<void(void)> handler;

private:
  struct event *ev;
};

std::string urlDecode(const std::string &urlEncoded);

#endif
