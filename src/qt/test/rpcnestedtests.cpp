// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/rpcnestedtests.h>

#include <chainparams.h>
#include <consensus/validation.h>
#include <fs.h>
#include <qt/rpcconsole.h>
#include <rpc/register.h>
#include <rpc/server.h>
#include <test/test_bitcoin.h>
#include <univalue.h>
#include <util.h>
#include <validation.h>

#include <QDir>
#include <QtGlobal>

static UniValue rpcNestedTest_rpc(const JSONRPCRequest &request) {
  if (request.fHelp) {
    return "help message";
  }
  return request.params.write(0, 0);
}

static const CRPCCommand vRPCCommands[] = {
    {"test", "rpcNestedTest", &rpcNestedTest_rpc, {}},
};

void RPCNestedTests::rpcNestedTests() {
  tableRPC.appendCommand("rpcNestedTest", &vRPCCommands[0]);

  TestingSetup test;

  SetRPCWarmupFinished();

  std::string result;
  std::string result2;
  std::string filtered;
  RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo()[chain]",
                                    &filtered);

  QVERIFY(result == "main");
  QVERIFY(filtered == "getblockchaininfo()[chain]");

  RPCConsole::RPCExecuteCommandLine(result, "getblock(getbestblockhash())");

  RPCConsole::RPCExecuteCommandLine(
      result, "getblock(getblock(getbestblockhash())[hash], true)");

  RPCConsole::RPCExecuteCommandLine(
      result,
      "getblock( getblock( getblock(getbestblockhash())[hash] )[hash], true)");

  RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo");
  QVERIFY(result.substr(0, 1) == "{");

  RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo()");
  QVERIFY(result.substr(0, 1) == "{");

  RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo ");

  QVERIFY(result.substr(0, 1) == "{");

  (RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo()[\"chain\"]"));

  QVERIFY(result == "null");

  (RPCConsole::RPCExecuteCommandLine(result, "createrawtransaction [] {} 0"));

  (RPCConsole::RPCExecuteCommandLine(result2, "createrawtransaction([],{},0)"));

  QVERIFY(result == result2);
  (RPCConsole::RPCExecuteCommandLine(result2,
                                     "createrawtransaction( [],  {} , 0   )"));

  QVERIFY(result == result2);

  RPCConsole::RPCExecuteCommandLine(
      result, "getblock(getbestblockhash())[tx][0]", &filtered);
  QVERIFY(result ==
          "97ddfbbae6be97fd6cdf3e7ca13232a3afff2353e29badfab7f73011edd4ced9");
  QVERIFY(filtered == "getblock(getbestblockhash())[tx][0]");

  RPCConsole::RPCParseCommandLine(result, "importprivkey", false, &filtered);
  QVERIFY(filtered == "importprivkey(…)");
  RPCConsole::RPCParseCommandLine(result, "signmessagewithprivkey abc", false,
                                  &filtered);
  QVERIFY(filtered == "signmessagewithprivkey(…)");
  RPCConsole::RPCParseCommandLine(result, "signmessagewithprivkey abc,def",
                                  false, &filtered);
  QVERIFY(filtered == "signmessagewithprivkey(…)");
  RPCConsole::RPCParseCommandLine(result, "signrawtransaction(abc)", false,
                                  &filtered);
  QVERIFY(filtered == "signrawtransaction(…)");
  RPCConsole::RPCParseCommandLine(result, "walletpassphrase(help())", false,
                                  &filtered);
  QVERIFY(filtered == "walletpassphrase(…)");
  RPCConsole::RPCParseCommandLine(
      result, "walletpassphrasechange(help(walletpassphrasechange(abc)))",
      false, &filtered);
  QVERIFY(filtered == "walletpassphrasechange(…)");
  RPCConsole::RPCParseCommandLine(result, "help(encryptwallet(abc, def))",
                                  false, &filtered);
  QVERIFY(filtered == "help(encryptwallet(…))");
  RPCConsole::RPCParseCommandLine(result, "help(importprivkey())", false,
                                  &filtered);
  QVERIFY(filtered == "help(importprivkey(…))");
  RPCConsole::RPCParseCommandLine(result, "help(importprivkey(help()))", false,
                                  &filtered);
  QVERIFY(filtered == "help(importprivkey(…))");
  RPCConsole::RPCParseCommandLine(
      result, "help(importprivkey(abc), walletpassphrase(def))", false,
      &filtered);
  QVERIFY(filtered == "help(importprivkey(…), walletpassphrase(…))");

  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest");
  QVERIFY(result == "[]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest ''");
  QVERIFY(result == "[\"\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest \"\"");
  QVERIFY(result == "[\"\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest '' abc");
  QVERIFY(result == "[\"\",\"abc\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest abc '' abc");
  QVERIFY(result == "[\"abc\",\"\",\"abc\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest abc  abc");
  QVERIFY(result == "[\"abc\",\"abc\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest abc\t\tabc");
  QVERIFY(result == "[\"abc\",\"abc\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest(abc )");
  QVERIFY(result == "[\"abc\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest( abc )");
  QVERIFY(result == "[\"abc\"]");
  RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest(   abc   ,   cba )");
  QVERIFY(result == "[\"abc\",\"cba\"]");

#if QT_VERSION >= 0x050300

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo() .\n"),
      std::runtime_error);

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(
          result, "getblockchaininfo() getblockchaininfo()"),
      std::runtime_error);

  (RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo("));

  (RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo()()()"));

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(result, "getblockchaininfo(True)"),
      UniValue);

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(result, "a(getblockchaininfo(True))"),
      UniValue);

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest abc,,abc"),
      std::runtime_error);

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest(abc,,abc)"),
      std::runtime_error);

  QVERIFY_EXCEPTION_THROWN(
      RPCConsole::RPCExecuteCommandLine(result, "rpcNestedTest(abc,,)"),
      std::runtime_error);

#endif
}
