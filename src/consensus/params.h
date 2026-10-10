// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include <limits>
#include <map>
#include <string>
#include <uint256.h>

#include <script/script.h>

#include <amount.h>

namespace Consensus {
enum DeploymentPos {
  DEPLOYMENT_TESTDUMMY,
  DEPLOYMENT_CSV,

  DEPLOYMENT_SEGWIT,

  DEPLOYMENT_HIVE,

  DEPLOYMENT_HIVE_1_1,

  DEPLOYMENT_MINOTAURX,

  DEPLOYMENT_RIALTO,

  MAX_VERSION_BITS_DEPLOYMENTS
};

struct BIP9Deployment {
  int bit;

  int64_t nStartTime;

  int64_t nTimeout;

  static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

  static constexpr int64_t ALWAYS_ACTIVE = -1;
};

struct Params {
  uint256 hashGenesisBlock;
  int nSubsidyHalvingInterval;

  int BIP16Height;

  int BIP34Height;
  uint256 BIP34Hash;

  int BIP65Height;

  int BIP66Height;

  uint32_t nRuleChangeActivationThreshold;
  uint32_t nMinerConfirmationWindow;
  BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];

  uint256 powLimit;
  bool fPowAllowMinDifficultyBlocks;
  bool fPowNoRetargeting;
  int64_t nPowTargetSpacing;
  int64_t nPowTargetTimespan;
  int64_t DifficultyAdjustmentInterval() const {
    return nPowTargetTimespan / nPowTargetSpacing;
  }
  uint256 nMinimumChainWork;
  uint256 defaultAssumeValid;

  uint32_t powForkTime;

  int lastScryptBlock;

  int slowStartBlocks;

  int totalMoneySupplyHeight;

  uint256 powLimitSHA;

  CAmount premineAmount;

  CScript premineOutputScript;

  CAmount minBeeCost;

  int beeCostFactor;

  std::string beeCreationAddress;

  std::string hiveCommunityAddress;

  int communityContribFactor;

  int beeGestationBlocks;

  int beeLifespanBlocks;

  uint256 powLimitHive;

  uint32_t hiveNonceMarker;

  int minHiveCheckBlock;

  int hiveTargetAdjustAggression;

  int hiveBlockSpacingTarget;

  int hiveBlockSpacingTargetTypical;

  int hiveBlockSpacingTargetTypical_1_1;

  int minK;

  int maxK;

  double maxHiveDiff;

  int maxKPow;

  double powSplit1;

  double powSplit2;

  int maxConsecutiveHiveBlocks;

  int hiveDifficultyWindow;

  int64_t lwmaAveragingWindow;

  std::vector<uint256> powTypeLimits;

  std::string nickCreationAddress;

  CAmount nickCreationCost3Char;

  CAmount nickCreationCost4Char;
  CAmount nickCreationCostStandard;
  CAmount nickCreationAntiDust;
};
} // namespace Consensus

#endif
