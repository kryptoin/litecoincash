// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include <chainparamsbase.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <protocol.h>

#include <memory>
#include <vector>

struct SeedSpec6 {
  uint8_t addr[16];
  uint16_t port;
};

typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
  MapCheckpoints mapCheckpoints;
};

struct ChainTxData {
  int64_t nTime;
  int64_t nTxCount;
  double dTxRate;
};

class CChainParams {
public:
  enum Base58Type {
    PUBKEY_ADDRESS,
    SCRIPT_ADDRESS,
    SCRIPT_ADDRESS2,
    SECRET_KEY,
    EXT_PUBLIC_KEY,
    EXT_SECRET_KEY,

    MAX_BASE58_TYPES
  };

  const Consensus::Params &GetConsensus() const { return consensus; }
  const CMessageHeader::MessageStartChars &MessageStart() const {
    return pchMessageStart;
  }
  int GetDefaultPort() const { return nDefaultPort; }

  const CBlock &GenesisBlock() const { return genesis; }

  bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }

  bool RequireStandard() const { return fRequireStandard; }
  uint64_t PruneAfterHeight() const { return nPruneAfterHeight; }

  bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }

  std::string NetworkIDString() const { return strNetworkID; }

  const std::vector<std::string> &DNSSeeds() const { return vSeeds; }
  const std::vector<unsigned char> &Base58Prefix(Base58Type type) const {
    return base58Prefixes[type];
  }
  const std::string &Bech32HRP() const { return bech32_hrp; }
  const std::vector<SeedSpec6> &FixedSeeds() const { return vFixedSeeds; }
  const CCheckpointData &Checkpoints() const { return checkpointData; }
  const ChainTxData &TxData() const { return chainTxData; }
  void UpdateVersionBitsParameters(Consensus::DeploymentPos d,
                                   int64_t nStartTime, int64_t nTimeout);

protected:
  CChainParams() {}

  Consensus::Params consensus;
  CMessageHeader::MessageStartChars pchMessageStart;
  int nDefaultPort;
  uint64_t nPruneAfterHeight;
  std::vector<std::string> vSeeds;
  std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
  std::string bech32_hrp;
  std::string strNetworkID;
  CBlock genesis;
  std::vector<SeedSpec6> vFixedSeeds;
  bool fDefaultConsistencyChecks;
  bool fRequireStandard;
  bool fMineBlocksOnDemand;
  CCheckpointData checkpointData;
  ChainTxData chainTxData;
};

std::unique_ptr<CChainParams> CreateChainParams(const std::string &chain);

const CChainParams &Params();

void SelectParams(const std::string &chain);

void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime,
                                 int64_t nTimeout);

#endif
