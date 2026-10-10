// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCSERVER_H
#define BITCOIN_RPCSERVER_H

#include <amount.h>
#include <rpc/protocol.h>
#include <uint256.h>

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <univalue.h>

static const unsigned int DEFAULT_RPC_SERIALIZE_VERSION = 1;

class CRPCCommand;

namespace RPCServer {
void OnStarted(std::function<void()> slot);
void OnStopped(std::function<void()> slot);
} // namespace RPCServer

struct UniValueType {
  explicit UniValueType(UniValue::VType _type) : typeAny(false), type(_type) {}
  UniValueType() : typeAny(true) {}
  bool typeAny;
  UniValue::VType type;
};

class JSONRPCRequest {
public:
  UniValue id;
  std::string strMethod;
  UniValue params;
  bool fHelp;
  std::string URI;
  std::string authUser;

  JSONRPCRequest() : id(NullUniValue), params(NullUniValue), fHelp(false) {}
  void parse(const UniValue &valRequest);
};

bool IsRPCRunning();

void SetRPCWarmupStatus(const std::string &newStatus);

void SetRPCWarmupFinished();

bool RPCIsInWarmup(std::string *outStatus);

void RPCTypeCheck(const UniValue &params,
                  const std::list<UniValue::VType> &typesExpected,
                  bool fAllowNull = false);

void RPCTypeCheckArgument(const UniValue &value, UniValue::VType typeExpected);

void RPCTypeCheckObj(const UniValue &o,
                     const std::map<std::string, UniValueType> &typesExpected,
                     bool fAllowNull = false, bool fStrict = false);

class RPCTimerBase {
public:
  virtual ~RPCTimerBase() {}
};

class RPCTimerInterface {
public:
  virtual ~RPCTimerInterface() {}

  virtual const char *Name() = 0;

  virtual RPCTimerBase *NewTimer(std::function<void(void)> &func,
                                 int64_t millis) = 0;
};

void RPCSetTimerInterface(RPCTimerInterface *iface);

void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface);

void RPCUnsetTimerInterface(RPCTimerInterface *iface);

void RPCRunLater(const std::string &name, std::function<void(void)> func,
                 int64_t nSeconds);

typedef UniValue (*rpcfn_type)(const JSONRPCRequest &jsonRequest);

class CRPCCommand {
public:
  std::string category;
  std::string name;
  rpcfn_type actor;
  std::vector<std::string> argNames;
};

class CRPCTable {
private:
  std::map<std::string, const CRPCCommand *> mapCommands;

public:
  CRPCTable();
  const CRPCCommand *operator[](const std::string &name) const;
  std::string help(const std::string &name,
                   const JSONRPCRequest &helpreq) const;

  UniValue execute(const JSONRPCRequest &request) const;

  std::vector<std::string> listCommands() const;

  bool appendCommand(const std::string &name, const CRPCCommand *pcmd);
};

bool IsDeprecatedRPCEnabled(const std::string &method);

extern CRPCTable tableRPC;

extern uint256 ParseHashV(const UniValue &v, std::string strName);
extern uint256 ParseHashO(const UniValue &o, std::string strKey);
extern std::vector<unsigned char> ParseHexV(const UniValue &v,
                                            std::string strName);
extern std::vector<unsigned char> ParseHexO(const UniValue &o,
                                            std::string strKey);

extern CAmount AmountFromValue(const UniValue &value);
extern std::string HelpExampleCli(const std::string &methodname,
                                  const std::string &args);
extern std::string HelpExampleRpc(const std::string &methodname,
                                  const std::string &args);

bool StartRPC();
void InterruptRPC();
void StopRPC();
std::string JSONRPCExecBatch(const JSONRPCRequest &jreq, const UniValue &vReq);

int RPCSerializationFlags();

#endif
