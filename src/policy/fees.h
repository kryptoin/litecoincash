// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICYESTIMATOR_H
#define BITCOIN_POLICYESTIMATOR_H

#include <amount.h>
#include <policy/feerate.h>
#include <random.h>
#include <sync.h>
#include <uint256.h>

#include <map>
#include <string>
#include <vector>

class CAutoFile;
class CFeeRate;
class CTxMemPoolEntry;
class CTxMemPool;
class TxConfirmStats;

enum FeeEstimateHorizon {
  SHORT_HALFLIFE = 0,
  MED_HALFLIFE = 1,
  LONG_HALFLIFE = 2
};

std::string StringForFeeEstimateHorizon(FeeEstimateHorizon horizon);

enum class FeeReason {
  NONE,
  HALF_ESTIMATE,
  FULL_ESTIMATE,
  DOUBLE_ESTIMATE,
  CONSERVATIVE,
  MEMPOOL_MIN,
  PAYTXFEE,
  FALLBACK,
  REQUIRED,
  MAXTXFEE,
};

std::string StringForFeeReason(FeeReason reason);

enum class FeeEstimateMode {
  UNSET,

  ECONOMICAL,

  CONSERVATIVE,

};

bool FeeModeFromString(const std::string &mode_string,
                       FeeEstimateMode &fee_estimate_mode);

struct EstimatorBucket {
  double start = -1;
  double end = -1;
  double withinTarget = 0;
  double totalConfirmed = 0;
  double inMempool = 0;
  double leftMempool = 0;
};

struct EstimationResult {
  EstimatorBucket pass;
  EstimatorBucket fail;
  double decay = 0;
  unsigned int scale = 0;
};

struct FeeCalculation {
  EstimationResult est;
  FeeReason reason = FeeReason::NONE;
  int desiredTarget = 0;
  int returnedTarget = 0;
};

class CBlockPolicyEstimator {
private:
  static constexpr unsigned int SHORT_BLOCK_PERIODS = 12;
  static constexpr unsigned int SHORT_SCALE = 1;

  static constexpr unsigned int MED_BLOCK_PERIODS = 24;
  static constexpr unsigned int MED_SCALE = 2;

  static constexpr unsigned int LONG_BLOCK_PERIODS = 42;
  static constexpr unsigned int LONG_SCALE = 24;

  static const unsigned int OLDEST_ESTIMATE_HISTORY = 6 * 1008;

  static constexpr double SHORT_DECAY = .962;

  static constexpr double MED_DECAY = .9952;

  static constexpr double LONG_DECAY = .99931;

  static constexpr double HALF_SUCCESS_PCT = .6;

  static constexpr double SUCCESS_PCT = .85;

  static constexpr double DOUBLE_SUCCESS_PCT = .95;

  static constexpr double SUFFICIENT_FEETXS = 0.1;

  static constexpr double SUFFICIENT_TXS_SHORT = 0.5;

  static constexpr double MIN_BUCKET_FEERATE = 1000;
  static constexpr double MAX_BUCKET_FEERATE = 1e7;

  static constexpr double FEE_SPACING = 1.05;

public:
  CBlockPolicyEstimator();
  ~CBlockPolicyEstimator();

  void processBlock(unsigned int nBlockHeight,
                    std::vector<const CTxMemPoolEntry *> &entries);

  void processTransaction(const CTxMemPoolEntry &entry, bool validFeeEstimate);

  bool removeTx(uint256 hash, bool inBlock);

  CFeeRate estimateFee(int confTarget) const;

  CFeeRate estimateSmartFee(int confTarget, FeeCalculation *feeCalc,
                            bool conservative) const;

  CFeeRate estimateRawFee(int confTarget, double successThreshold,
                          FeeEstimateHorizon horizon,
                          EstimationResult *result = nullptr) const;

  bool Write(CAutoFile &fileout) const;

  bool Read(CAutoFile &filein);

  void FlushUnconfirmed(CTxMemPool &pool);

  unsigned int HighestTargetTracked(FeeEstimateHorizon horizon) const;

private:
  unsigned int nBestSeenHeight;
  unsigned int firstRecordedHeight;
  unsigned int historicalFirst;
  unsigned int historicalBest;

  struct TxStatsInfo {
    unsigned int blockHeight;
    unsigned int bucketIndex;
    TxStatsInfo() : blockHeight(0), bucketIndex(0) {}
  };

  std::map<uint256, TxStatsInfo> mapMemPoolTxs;

  std::unique_ptr<TxConfirmStats> feeStats;
  std::unique_ptr<TxConfirmStats> shortStats;
  std::unique_ptr<TxConfirmStats> longStats;

  unsigned int trackedTxs;
  unsigned int untrackedTxs;

  std::vector<double> buckets;

  std::map<double, unsigned int> bucketMap;

  mutable CCriticalSection cs_feeEstimator;

  bool processBlockTx(unsigned int nBlockHeight, const CTxMemPoolEntry *entry);

  double estimateCombinedFee(unsigned int confTarget, double successThreshold,
                             bool checkShorterHorizon,
                             EstimationResult *result) const;

  double estimateConservativeFee(unsigned int doubleTarget,
                                 EstimationResult *result) const;

  unsigned int BlockSpan() const;

  unsigned int HistoricalBlockSpan() const;

  unsigned int MaxUsableEstimate() const;
};

class FeeFilterRounder {
private:
  static constexpr double MAX_FILTER_FEERATE = 1e7;

  static constexpr double FEE_FILTER_SPACING = 1.1;

public:
  explicit FeeFilterRounder(const CFeeRate &minIncrementalFee);

  CAmount round(CAmount currentMinFee);

private:
  std::set<double> feeset;
  FastRandomContext insecure_rand;
};

#endif
