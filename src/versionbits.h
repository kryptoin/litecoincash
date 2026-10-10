// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_VERSIONBITS
#define BITCOIN_CONSENSUS_VERSIONBITS

#include <chain.h>
#include <map>

static const int32_t VERSIONBITS_LAST_OLD_BLOCK_VERSION = 4;

static const int32_t VERSIONBITS_TOP_BITS = 0x20000000UL;

static const int32_t VERSIONBITS_TOP_MASK = 0xE0000000UL;

static const int32_t VERSIONBITS_NUM_BITS = 16;

enum ThresholdState {
  THRESHOLD_DEFINED,
  THRESHOLD_STARTED,
  THRESHOLD_LOCKED_IN,
  THRESHOLD_ACTIVE,
  THRESHOLD_FAILED,
};

typedef std::map<const CBlockIndex *, ThresholdState> ThresholdConditionCache;

struct VBDeploymentInfo {
  const char *name;

  bool gbt_force;
};

struct BIP9Stats {
  int period;
  int threshold;
  int elapsed;
  int count;
  bool possible;
};

extern const struct VBDeploymentInfo VersionBitsDeploymentInfo[];

class AbstractThresholdConditionChecker {
protected:
  virtual bool Condition(const CBlockIndex *pindex,
                         const Consensus::Params &params) const = 0;
  virtual int64_t BeginTime(const Consensus::Params &params) const = 0;
  virtual int64_t EndTime(const Consensus::Params &params) const = 0;
  virtual int Period(const Consensus::Params &params) const = 0;
  virtual int Threshold(const Consensus::Params &params) const = 0;

public:
  BIP9Stats GetStateStatisticsFor(const CBlockIndex *pindex,
                                  const Consensus::Params &params) const;

  ThresholdState GetStateFor(const CBlockIndex *pindexPrev,
                             const Consensus::Params &params,
                             ThresholdConditionCache &cache) const;
  int GetStateSinceHeightFor(const CBlockIndex *pindexPrev,
                             const Consensus::Params &params,
                             ThresholdConditionCache &cache) const;
};

struct VersionBitsCache {
  ThresholdConditionCache caches[Consensus::MAX_VERSION_BITS_DEPLOYMENTS];

  void Clear();
};

ThresholdState VersionBitsState(const CBlockIndex *pindexPrev,
                                const Consensus::Params &params,
                                Consensus::DeploymentPos pos,
                                VersionBitsCache &cache);
BIP9Stats VersionBitsStatistics(const CBlockIndex *pindexPrev,
                                const Consensus::Params &params,
                                Consensus::DeploymentPos pos);
int VersionBitsStateSinceHeight(const CBlockIndex *pindexPrev,
                                const Consensus::Params &params,
                                Consensus::DeploymentPos pos,
                                VersionBitsCache &cache);
uint32_t VersionBitsMask(const Consensus::Params &params,
                         Consensus::DeploymentPos pos);

#endif
