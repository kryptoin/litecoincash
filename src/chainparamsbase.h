// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMSBASE_H
#define BITCOIN_CHAINPARAMSBASE_H

#include <memory>
#include <string>
#include <vector>

class CBaseChainParams {
public:
  static const std::string MAIN;
  static const std::string TESTNET;
  static const std::string REGTEST;

  const std::string &DataDir() const { return strDataDir; }
  int RPCPort() const { return nRPCPort; }

protected:
  CBaseChainParams() {}

  int nRPCPort;
  std::string strDataDir;
};

std::unique_ptr<CBaseChainParams>
CreateBaseChainParams(const std::string &chain);

void AppendParamsHelpMessages(std::string &strUsage, bool debugHelp = true);

const CBaseChainParams &BaseParams();

void SelectBaseParams(const std::string &chain);

std::string ChainNameFromCommandLine();

#endif
