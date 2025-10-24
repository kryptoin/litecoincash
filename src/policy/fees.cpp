// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/fees.h>
#include <policy/policy.h>

#include <clientversion.h>
#include <primitives/transaction.h>
#include <streams.h>
#include <txmempool.h>
#include <util.h>

static constexpr double INF_FEERATE = 1e99;

std::string StringForFeeEstimateHorizon(FeeEstimateHorizon horizon) {
  static const std::map<FeeEstimateHorizon, std::string> horizon_strings = {
      {FeeEstimateHorizon::SHORT_HALFLIFE, "short"},
      {FeeEstimateHorizon::MED_HALFLIFE, "medium"},
      {FeeEstimateHorizon::LONG_HALFLIFE, "long"},
  };
  auto horizon_string = horizon_strings.find(horizon);

  if (horizon_string == horizon_strings.end())
    return "unknown";

  return horizon_string->second;
}

std::string StringForFeeReason(FeeReason reason) {
  static const std::map<FeeReason, std::string> fee_reason_strings = {
      {FeeReason::NONE, "None"},
      {FeeReason::HALF_ESTIMATE, "Half Target 60% Threshold"},
      {FeeReason::FULL_ESTIMATE, "Target 85% Threshold"},
      {FeeReason::DOUBLE_ESTIMATE, "Double Target 95% Threshold"},
      {FeeReason::CONSERVATIVE, "Conservative Double Target longer horizon"},
      {FeeReason::MEMPOOL_MIN, "Mempool Min Fee"},
      {FeeReason::PAYTXFEE, "PayTxFee set"},
      {FeeReason::FALLBACK, "Fallback fee"},
      {FeeReason::REQUIRED, "Minimum Required Fee"},
      {FeeReason::MAXTXFEE, "MaxTxFee limit"}};
  auto reason_string = fee_reason_strings.find(reason);

  if (reason_string == fee_reason_strings.end())
    return "Unknown";

  return reason_string->second;
}

bool FeeModeFromString(const std::string &mode_string,
                       FeeEstimateMode &fee_estimate_mode) {
  static const std::map<std::string, FeeEstimateMode> fee_modes = {
      {"UNSET", FeeEstimateMode::UNSET},
      {"ECONOMICAL", FeeEstimateMode::ECONOMICAL},
      {"CONSERVATIVE", FeeEstimateMode::CONSERVATIVE},
  };
  auto mode = fee_modes.find(mode_string);

  if (mode == fee_modes.end())
    return false;

  fee_estimate_mode = mode->second;
  return true;
}

class TxConfirmStats {
private:
  const std::vector<double> &buckets;

  const std::map<double, unsigned int> &bucketMap;

  std::vector<double> txCtAvg;

  std::vector<std::vector<double>> confAvg;

  std::vector<std::vector<double>> failAvg;

  std::vector<double> avg;

  double decay;

  unsigned int scale;

  std::vector<std::vector<int>> unconfTxs;

  std::vector<int> oldUnconfTxs;

  void resizeInMemoryCounters(size_t newbuckets);

public:
  TxConfirmStats(const std::vector<double> &defaultBuckets,
                 const std::map<double, unsigned int> &defaultBucketMap,
                 unsigned int maxPeriods, double decay, unsigned int scale);

  void ClearCurrent(unsigned int nBlockHeight);

  void Record(int blocksToConfirm, double val);

  unsigned int NewTx(unsigned int nBlockHeight, double val);

  void removeTx(unsigned int entryHeight, unsigned int nBestSeenHeight,
                unsigned int bucketIndex, bool inBlock);

  void UpdateMovingAverages();

  double EstimateMedianVal(int confTarget, double sufficientTxVal,
                           double minSuccess, bool requireGreater,
                           unsigned int nBlockHeight,
                           EstimationResult *result = nullptr) const;

  unsigned int GetMaxConfirms() const { return scale * confAvg.size(); }

  void Write(CAutoFile &fileout) const;

  void Read(CAutoFile &filein, int nFileVersion, size_t numBuckets);
};

TxConfirmStats::TxConfirmStats(
    const std::vector<double> &defaultBuckets,
    const std::map<double, unsigned int> &defaultBucketMap,
    unsigned int maxPeriods, double _decay, unsigned int _scale)
    : buckets(defaultBuckets), bucketMap(defaultBucketMap) {
  decay = _decay;
  assert(_scale != 0 && "_scale must be non-zero");
  scale = _scale;
  confAvg.resize(maxPeriods);
  for (unsigned int i = 0; i < maxPeriods; i++) {
    confAvg[i].resize(buckets.size());
  }
  failAvg.resize(maxPeriods);
  for (unsigned int i = 0; i < maxPeriods; i++) {
    failAvg[i].resize(buckets.size());
  }

  txCtAvg.resize(buckets.size());
  avg.resize(buckets.size());

  resizeInMemoryCounters(buckets.size());
}

void TxConfirmStats::resizeInMemoryCounters(size_t newbuckets) {
  unconfTxs.resize(GetMaxConfirms());
  for (unsigned int i = 0; i < unconfTxs.size(); i++) {
    unconfTxs[i].resize(newbuckets);
  }
  oldUnconfTxs.resize(newbuckets);
}

void TxConfirmStats::ClearCurrent(unsigned int nBlockHeight) {
  for (unsigned int j = 0; j < buckets.size(); j++) {
    oldUnconfTxs[j] += unconfTxs[nBlockHeight % unconfTxs.size()][j];
    unconfTxs[nBlockHeight % unconfTxs.size()][j] = 0;
  }
}

void TxConfirmStats::Record(int blocksToConfirm, double val) {
  if (blocksToConfirm < 1)
    return;
  int periodsToConfirm = (blocksToConfirm + scale - 1) / scale;
  unsigned int bucketindex = bucketMap.lower_bound(val)->second;
  for (size_t i = periodsToConfirm; i <= confAvg.size(); i++) {
    confAvg[i - 1][bucketindex]++;
  }
  txCtAvg[bucketindex]++;
  avg[bucketindex] += val;
}

void TxConfirmStats::UpdateMovingAverages() {
  for (unsigned int j = 0; j < buckets.size(); j++) {
    for (unsigned int i = 0; i < confAvg.size(); i++)
      confAvg[i][j] = confAvg[i][j] * decay;
    for (unsigned int i = 0; i < failAvg.size(); i++)
      failAvg[i][j] = failAvg[i][j] * decay;
    avg[j] = avg[j] * decay;
    txCtAvg[j] = txCtAvg[j] * decay;
  }
}

double TxConfirmStats::EstimateMedianVal(int confTarget, double sufficientTxVal,
                                         double successBreakPoint,
                                         bool requireGreater,
                                         unsigned int nBlockHeight,
                                         EstimationResult *result) const {
  double nConf = 0;

  double totalNum = 0;

  int extraNum = 0;

  double failNum = 0;

  int periodTarget = (confTarget + scale - 1) / scale;

  int maxbucketindex = buckets.size() - 1;

  unsigned int startbucket = requireGreater ? maxbucketindex : 0;
  int step = requireGreater ? -1 : 1;

  unsigned int curNearBucket = startbucket;
  unsigned int bestNearBucket = startbucket;
  unsigned int curFarBucket = startbucket;
  unsigned int bestFarBucket = startbucket;

  bool foundAnswer = false;
  unsigned int bins = unconfTxs.size();
  bool newBucketRange = true;
  bool passing = true;
  EstimatorBucket passBucket;
  EstimatorBucket failBucket;

  for (int bucket = startbucket; bucket >= 0 && bucket <= maxbucketindex;
       bucket += step) {
    if (newBucketRange) {
      curNearBucket = bucket;
      newBucketRange = false;
    }
    curFarBucket = bucket;
    nConf += confAvg[periodTarget - 1][bucket];
    totalNum += txCtAvg[bucket];
    failNum += failAvg[periodTarget - 1][bucket];
    for (unsigned int confct = confTarget; confct < GetMaxConfirms(); confct++)
      extraNum += unconfTxs[(nBlockHeight - confct) % bins][bucket];
    extraNum += oldUnconfTxs[bucket];

    if (totalNum >= sufficientTxVal / (1 - decay)) {
      double curPct = nConf / (totalNum + failNum + extraNum);

      if ((requireGreater && curPct < successBreakPoint) ||
          (!requireGreater && curPct > successBreakPoint)) {
        if (passing == true) {
          unsigned int failMinBucket = std::min(curNearBucket, curFarBucket);
          unsigned int failMaxBucket = std::max(curNearBucket, curFarBucket);
          failBucket.start = failMinBucket ? buckets[failMinBucket - 1] : 0;
          failBucket.end = buckets[failMaxBucket];
          failBucket.withinTarget = nConf;
          failBucket.totalConfirmed = totalNum;
          failBucket.inMempool = extraNum;
          failBucket.leftMempool = failNum;
          passing = false;
        }
        continue;
      }

      else {
        failBucket = EstimatorBucket();

        foundAnswer = true;
        passing = true;
        passBucket.withinTarget = nConf;
        nConf = 0;
        passBucket.totalConfirmed = totalNum;
        totalNum = 0;
        passBucket.inMempool = extraNum;
        passBucket.leftMempool = failNum;
        failNum = 0;
        extraNum = 0;
        bestNearBucket = curNearBucket;
        bestFarBucket = curFarBucket;
        newBucketRange = true;
      }
    }
  }

  double median = -1;
  double txSum = 0;

  unsigned int minBucket = std::min(bestNearBucket, bestFarBucket);
  unsigned int maxBucket = std::max(bestNearBucket, bestFarBucket);
  for (unsigned int j = minBucket; j <= maxBucket; j++) {
    txSum += txCtAvg[j];
  }
  if (foundAnswer && txSum != 0) {
    txSum = txSum / 2;
    for (unsigned int j = minBucket; j <= maxBucket; j++) {
      if (txCtAvg[j] < txSum)
        txSum -= txCtAvg[j];
      else {
        median = avg[j] / txCtAvg[j];
        break;
      }
    }

    passBucket.start = minBucket ? buckets[minBucket - 1] : 0;
    passBucket.end = buckets[maxBucket];
  }

  if (passing && !newBucketRange) {
    unsigned int failMinBucket = std::min(curNearBucket, curFarBucket);
    unsigned int failMaxBucket = std::max(curNearBucket, curFarBucket);
    failBucket.start = failMinBucket ? buckets[failMinBucket - 1] : 0;
    failBucket.end = buckets[failMaxBucket];
    failBucket.withinTarget = nConf;
    failBucket.totalConfirmed = totalNum;
    failBucket.inMempool = extraNum;
    failBucket.leftMempool = failNum;
  }

  LogPrint(BCLog::ESTIMATEFEE,
           "FeeEst: %d %s%.0f%% decay %.5f: feerate: %g from (%g - %g) %.2f%% "
           "%.1f/(%.1f %d mem %.1f out) Fail: (%g - %g) %.2f%% %.1f/(%.1f %d "
           "mem %.1f out)\n",
           confTarget, requireGreater ? ">" : "<", 100.0 * successBreakPoint,
           decay, median, passBucket.start, passBucket.end,
           100 * passBucket.withinTarget /
               (passBucket.totalConfirmed + passBucket.inMempool +
                passBucket.leftMempool),
           passBucket.withinTarget, passBucket.totalConfirmed,
           passBucket.inMempool, passBucket.leftMempool, failBucket.start,
           failBucket.end,
           100 * failBucket.withinTarget /
               (failBucket.totalConfirmed + failBucket.inMempool +
                failBucket.leftMempool),
           failBucket.withinTarget, failBucket.totalConfirmed,
           failBucket.inMempool, failBucket.leftMempool);

  if (result) {
    result->pass = passBucket;
    result->fail = failBucket;
    result->decay = decay;
    result->scale = scale;
  }
  return median;
}

void TxConfirmStats::Write(CAutoFile &fileout) const {
  fileout << decay;
  fileout << scale;
  fileout << avg;
  fileout << txCtAvg;
  fileout << confAvg;
  fileout << failAvg;
}

void TxConfirmStats::Read(CAutoFile &filein, int nFileVersion,
                          size_t numBuckets) {
  size_t maxConfirms, maxPeriods;

  filein >> decay;
  if (decay <= 0 || decay >= 1) {
    throw std::runtime_error("Corrupt estimates file. Decay must be between 0 "
                             "and 1 (non-inclusive)");
  }
  filein >> scale;
  if (scale == 0) {
    throw std::runtime_error("Corrupt estimates file. Scale must be non-zero");
  }

  filein >> avg;
  if (avg.size() != numBuckets) {
    throw std::runtime_error(
        "Corrupt estimates file. Mismatch in feerate average bucket count");
  }
  filein >> txCtAvg;
  if (txCtAvg.size() != numBuckets) {
    throw std::runtime_error(
        "Corrupt estimates file. Mismatch in tx count bucket count");
  }
  filein >> confAvg;
  maxPeriods = confAvg.size();
  maxConfirms = scale * maxPeriods;

  if (maxConfirms <= 0 || maxConfirms > 6 * 24 * 7) {
    throw std::runtime_error("Corrupt estimates file.  Must maintain estimates "
                             "for between 1 and 1008 (one week) confirms");
  }
  for (unsigned int i = 0; i < maxPeriods; i++) {
    if (confAvg[i].size() != numBuckets) {
      throw std::runtime_error("Corrupt estimates file. Mismatch in feerate "
                               "conf average bucket count");
    }
  }

  filein >> failAvg;
  if (maxPeriods != failAvg.size()) {
    throw std::runtime_error(
        "Corrupt estimates file. Mismatch in confirms tracked for failures");
  }
  for (unsigned int i = 0; i < maxPeriods; i++) {
    if (failAvg[i].size() != numBuckets) {
      throw std::runtime_error("Corrupt estimates file. Mismatch in one of "
                               "failure average bucket counts");
    }
  }

  resizeInMemoryCounters(numBuckets);

  LogPrint(BCLog::ESTIMATEFEE,
           "Reading estimates: %u buckets counting confirms up to %u blocks\n",
           numBuckets, maxConfirms);
}

unsigned int TxConfirmStats::NewTx(unsigned int nBlockHeight, double val) {
  unsigned int bucketindex = bucketMap.lower_bound(val)->second;
  unsigned int blockIndex = nBlockHeight % unconfTxs.size();
  unconfTxs[blockIndex][bucketindex]++;
  return bucketindex;
}

void TxConfirmStats::removeTx(unsigned int entryHeight,
                              unsigned int nBestSeenHeight,
                              unsigned int bucketindex, bool inBlock) {
  int blocksAgo = nBestSeenHeight - entryHeight;
  if (nBestSeenHeight == 0)

    blocksAgo = 0;
  if (blocksAgo < 0) {
    LogPrint(BCLog::ESTIMATEFEE,
             "Blockpolicy error, blocks ago is negative for mempool tx\n");
    return;
  }

  if (blocksAgo >= (int)unconfTxs.size()) {
    if (oldUnconfTxs[bucketindex] > 0) {
      oldUnconfTxs[bucketindex]--;
    } else {
      LogPrint(BCLog::ESTIMATEFEE,
               "Blockpolicy error, mempool tx removed from >25 "
               "blocks,bucketIndex=%u already\n",
               bucketindex);
    }
  } else {
    unsigned int blockIndex = entryHeight % unconfTxs.size();
    if (unconfTxs[blockIndex][bucketindex] > 0) {
      unconfTxs[blockIndex][bucketindex]--;
    } else {
      LogPrint(BCLog::ESTIMATEFEE,
               "Blockpolicy error, mempool tx removed from "
               "blockIndex=%u,bucketIndex=%u already\n",
               blockIndex, bucketindex);
    }
  }
  if (!inBlock && (unsigned int)blocksAgo >= scale) {
    assert(scale != 0);
    unsigned int periodsAgo = blocksAgo / scale;
    for (size_t i = 0; i < periodsAgo && i < failAvg.size(); i++) {
      failAvg[i][bucketindex]++;
    }
  }
}

bool CBlockPolicyEstimator::removeTx(uint256 hash, bool inBlock) {
  LOCK(cs_feeEstimator);
  std::map<uint256, TxStatsInfo>::iterator pos = mapMemPoolTxs.find(hash);
  if (pos != mapMemPoolTxs.end()) {
    feeStats->removeTx(pos->second.blockHeight, nBestSeenHeight,
                       pos->second.bucketIndex, inBlock);
    shortStats->removeTx(pos->second.blockHeight, nBestSeenHeight,
                         pos->second.bucketIndex, inBlock);
    longStats->removeTx(pos->second.blockHeight, nBestSeenHeight,
                        pos->second.bucketIndex, inBlock);
    mapMemPoolTxs.erase(hash);
    return true;
  } else {
    return false;
  }
}

CBlockPolicyEstimator::CBlockPolicyEstimator()
    : nBestSeenHeight(0), firstRecordedHeight(0), historicalFirst(0),
      historicalBest(0), trackedTxs(0), untrackedTxs(0) {
  static_assert(MIN_BUCKET_FEERATE > 0, "Min feerate must be nonzero");
  size_t bucketIndex = 0;
  for (double bucketBoundary = MIN_BUCKET_FEERATE;
       bucketBoundary <= MAX_BUCKET_FEERATE;
       bucketBoundary *= FEE_SPACING, bucketIndex++) {
    buckets.push_back(bucketBoundary);
    bucketMap[bucketBoundary] = bucketIndex;
  }
  buckets.push_back(INF_FEERATE);
  bucketMap[INF_FEERATE] = bucketIndex;
  assert(bucketMap.size() == buckets.size());

  feeStats = std::unique_ptr<TxConfirmStats>(new TxConfirmStats(
      buckets, bucketMap, MED_BLOCK_PERIODS, MED_DECAY, MED_SCALE));
  shortStats = std::unique_ptr<TxConfirmStats>(new TxConfirmStats(
      buckets, bucketMap, SHORT_BLOCK_PERIODS, SHORT_DECAY, SHORT_SCALE));
  longStats = std::unique_ptr<TxConfirmStats>(new TxConfirmStats(
      buckets, bucketMap, LONG_BLOCK_PERIODS, LONG_DECAY, LONG_SCALE));
}

CBlockPolicyEstimator::~CBlockPolicyEstimator() {}

void CBlockPolicyEstimator::processTransaction(const CTxMemPoolEntry &entry,
                                               bool validFeeEstimate) {
  LOCK(cs_feeEstimator);
  unsigned int txHeight = entry.GetHeight();
  uint256 hash = entry.GetTx().GetHash();
  if (mapMemPoolTxs.count(hash)) {
    LogPrint(BCLog::ESTIMATEFEE,
             "Blockpolicy error mempool tx %s already being tracked\n",
             hash.ToString().c_str());
    return;
  }

  if (txHeight != nBestSeenHeight) {
    return;
  }

  if (!validFeeEstimate) {
    untrackedTxs++;
    return;
  }
  trackedTxs++;

  CFeeRate feeRate(entry.GetFee(), entry.GetTxSize());

  mapMemPoolTxs[hash].blockHeight = txHeight;
  unsigned int bucketIndex =
      feeStats->NewTx(txHeight, (double)feeRate.GetFeePerK());
  mapMemPoolTxs[hash].bucketIndex = bucketIndex;
  unsigned int bucketIndex2 =
      shortStats->NewTx(txHeight, (double)feeRate.GetFeePerK());
  assert(bucketIndex == bucketIndex2);
  unsigned int bucketIndex3 =
      longStats->NewTx(txHeight, (double)feeRate.GetFeePerK());
  assert(bucketIndex == bucketIndex3);
}

bool CBlockPolicyEstimator::processBlockTx(unsigned int nBlockHeight,
                                           const CTxMemPoolEntry *entry) {
  if (!removeTx(entry->GetTx().GetHash(), true)) {
    return false;
  }

  int blocksToConfirm = nBlockHeight - entry->GetHeight();
  if (blocksToConfirm <= 0) {
    LogPrint(BCLog::ESTIMATEFEE,
             "Blockpolicy error Transaction had negative blocksToConfirm\n");
    return false;
  }

  CFeeRate feeRate(entry->GetFee(), entry->GetTxSize());

  feeStats->Record(blocksToConfirm, (double)feeRate.GetFeePerK());
  shortStats->Record(blocksToConfirm, (double)feeRate.GetFeePerK());
  longStats->Record(blocksToConfirm, (double)feeRate.GetFeePerK());
  return true;
}

void CBlockPolicyEstimator::processBlock(
    unsigned int nBlockHeight, std::vector<const CTxMemPoolEntry *> &entries) {
  LOCK(cs_feeEstimator);
  if (nBlockHeight <= nBestSeenHeight) {
    return;
  }

  nBestSeenHeight = nBlockHeight;

  feeStats->ClearCurrent(nBlockHeight);
  shortStats->ClearCurrent(nBlockHeight);
  longStats->ClearCurrent(nBlockHeight);

  feeStats->UpdateMovingAverages();
  shortStats->UpdateMovingAverages();
  longStats->UpdateMovingAverages();

  unsigned int countedTxs = 0;

  for (const auto &entry : entries) {
    if (processBlockTx(nBlockHeight, entry))
      countedTxs++;
  }

  if (firstRecordedHeight == 0 && countedTxs > 0) {
    firstRecordedHeight = nBestSeenHeight;
    LogPrint(BCLog::ESTIMATEFEE, "Blockpolicy first recorded height %u\n",
             firstRecordedHeight);
  }

  LogPrint(
      BCLog::ESTIMATEFEE,
      "Blockpolicy estimates updated by %u of %u block txs, since last block "
      "%u of %u tracked, mempool map size %u, max target %u from %s\n",
      countedTxs, entries.size(), trackedTxs, trackedTxs + untrackedTxs,
      mapMemPoolTxs.size(), MaxUsableEstimate(),
      HistoricalBlockSpan() > BlockSpan() ? "historical" : "current");

  trackedTxs = 0;
  untrackedTxs = 0;
}

CFeeRate CBlockPolicyEstimator::estimateFee(int confTarget) const {
  if (confTarget <= 1)
    return CFeeRate(0);

  return estimateRawFee(confTarget, DOUBLE_SUCCESS_PCT,
                        FeeEstimateHorizon::MED_HALFLIFE);
}

CFeeRate CBlockPolicyEstimator::estimateRawFee(int confTarget,
                                               double successThreshold,
                                               FeeEstimateHorizon horizon,
                                               EstimationResult *result) const {
  TxConfirmStats *stats;
  double sufficientTxs = SUFFICIENT_FEETXS;
  switch (horizon) {
  case FeeEstimateHorizon::SHORT_HALFLIFE: {
    stats = shortStats.get();
    sufficientTxs = SUFFICIENT_TXS_SHORT;
    break;
  }
  case FeeEstimateHorizon::MED_HALFLIFE: {
    stats = feeStats.get();
    break;
  }
  case FeeEstimateHorizon::LONG_HALFLIFE: {
    stats = longStats.get();
    break;
  }
  default: {
    throw std::out_of_range(
        "CBlockPolicyEstimator::estimateRawFee unknown FeeEstimateHorizon");
  }
  }

  LOCK(cs_feeEstimator);

  if (confTarget <= 0 || (unsigned int)confTarget > stats->GetMaxConfirms())
    return CFeeRate(0);
  if (successThreshold > 1)
    return CFeeRate(0);

  double median =
      stats->EstimateMedianVal(confTarget, sufficientTxs, successThreshold,
                               true, nBestSeenHeight, result);

  if (median < 0)
    return CFeeRate(0);

  return CFeeRate(llround(median));
}

unsigned int
CBlockPolicyEstimator::HighestTargetTracked(FeeEstimateHorizon horizon) const {
  switch (horizon) {
  case FeeEstimateHorizon::SHORT_HALFLIFE: {
    return shortStats->GetMaxConfirms();
  }
  case FeeEstimateHorizon::MED_HALFLIFE: {
    return feeStats->GetMaxConfirms();
  }
  case FeeEstimateHorizon::LONG_HALFLIFE: {
    return longStats->GetMaxConfirms();
  }
  default: {
    throw std::out_of_range("CBlockPolicyEstimator::HighestTargetTracked "
                            "unknown FeeEstimateHorizon");
  }
  }
}

unsigned int CBlockPolicyEstimator::BlockSpan() const {
  if (firstRecordedHeight == 0)
    return 0;
  assert(nBestSeenHeight >= firstRecordedHeight);

  return nBestSeenHeight - firstRecordedHeight;
}

unsigned int CBlockPolicyEstimator::HistoricalBlockSpan() const {
  if (historicalFirst == 0)
    return 0;
  assert(historicalBest >= historicalFirst);

  if (nBestSeenHeight - historicalBest > OLDEST_ESTIMATE_HISTORY)
    return 0;

  return historicalBest - historicalFirst;
}

unsigned int CBlockPolicyEstimator::MaxUsableEstimate() const {
  return std::min(longStats->GetMaxConfirms(),
                  std::max(BlockSpan(), HistoricalBlockSpan()) / 2);
}

double CBlockPolicyEstimator::estimateCombinedFee(
    unsigned int confTarget, double successThreshold, bool checkShorterHorizon,
    EstimationResult *result) const {
  double estimate = -1;
  if (confTarget >= 1 && confTarget <= longStats->GetMaxConfirms()) {
    if (confTarget <= shortStats->GetMaxConfirms()) {
      estimate = shortStats->EstimateMedianVal(confTarget, SUFFICIENT_TXS_SHORT,
                                               successThreshold, true,
                                               nBestSeenHeight, result);
    } else if (confTarget <= feeStats->GetMaxConfirms()) {
      estimate = feeStats->EstimateMedianVal(confTarget, SUFFICIENT_FEETXS,
                                             successThreshold, true,
                                             nBestSeenHeight, result);
    } else {
      estimate = longStats->EstimateMedianVal(confTarget, SUFFICIENT_FEETXS,
                                              successThreshold, true,
                                              nBestSeenHeight, result);
    }
    if (checkShorterHorizon) {
      EstimationResult tempResult;

      if (confTarget > feeStats->GetMaxConfirms()) {
        double medMax = feeStats->EstimateMedianVal(
            feeStats->GetMaxConfirms(), SUFFICIENT_FEETXS, successThreshold,
            true, nBestSeenHeight, &tempResult);
        if (medMax > 0 && (estimate == -1 || medMax < estimate)) {
          estimate = medMax;
          if (result)
            *result = tempResult;
        }
      }
      if (confTarget > shortStats->GetMaxConfirms()) {
        double shortMax = shortStats->EstimateMedianVal(
            shortStats->GetMaxConfirms(), SUFFICIENT_TXS_SHORT,
            successThreshold, true, nBestSeenHeight, &tempResult);
        if (shortMax > 0 && (estimate == -1 || shortMax < estimate)) {
          estimate = shortMax;
          if (result)
            *result = tempResult;
        }
      }
    }
  }
  return estimate;
}

double
CBlockPolicyEstimator::estimateConservativeFee(unsigned int doubleTarget,
                                               EstimationResult *result) const {
  double estimate = -1;
  EstimationResult tempResult;
  if (doubleTarget <= shortStats->GetMaxConfirms()) {
    estimate = feeStats->EstimateMedianVal(doubleTarget, SUFFICIENT_FEETXS,
                                           DOUBLE_SUCCESS_PCT, true,
                                           nBestSeenHeight, result);
  }
  if (doubleTarget <= feeStats->GetMaxConfirms()) {
    double longEstimate = longStats->EstimateMedianVal(
        doubleTarget, SUFFICIENT_FEETXS, DOUBLE_SUCCESS_PCT, true,
        nBestSeenHeight, &tempResult);
    if (longEstimate > estimate) {
      estimate = longEstimate;
      if (result)
        *result = tempResult;
    }
  }
  return estimate;
}

CFeeRate CBlockPolicyEstimator::estimateSmartFee(int confTarget,
                                                 FeeCalculation *feeCalc,
                                                 bool conservative) const {
  LOCK(cs_feeEstimator);

  if (feeCalc) {
    feeCalc->desiredTarget = confTarget;
    feeCalc->returnedTarget = confTarget;
  }

  double median = -1;
  EstimationResult tempResult;

  if (confTarget <= 0 ||
      (unsigned int)confTarget > longStats->GetMaxConfirms()) {
    return CFeeRate(0);
  }

  if (confTarget == 1)
    confTarget = 2;

  unsigned int maxUsableEstimate = MaxUsableEstimate();
  if ((unsigned int)confTarget > maxUsableEstimate) {
    confTarget = maxUsableEstimate;
  }
  if (feeCalc)
    feeCalc->returnedTarget = confTarget;

  if (confTarget <= 1)
    return CFeeRate(0);

  assert(confTarget > 0);

  double halfEst =
      estimateCombinedFee(confTarget / 2, HALF_SUCCESS_PCT, true, &tempResult);
  if (feeCalc) {
    feeCalc->est = tempResult;
    feeCalc->reason = FeeReason::HALF_ESTIMATE;
  }
  median = halfEst;
  double actualEst =
      estimateCombinedFee(confTarget, SUCCESS_PCT, true, &tempResult);
  if (actualEst > median) {
    median = actualEst;
    if (feeCalc) {
      feeCalc->est = tempResult;
      feeCalc->reason = FeeReason::FULL_ESTIMATE;
    }
  }
  double doubleEst = estimateCombinedFee(2 * confTarget, DOUBLE_SUCCESS_PCT,
                                         !conservative, &tempResult);
  if (doubleEst > median) {
    median = doubleEst;
    if (feeCalc) {
      feeCalc->est = tempResult;
      feeCalc->reason = FeeReason::DOUBLE_ESTIMATE;
    }
  }

  if (conservative || median == -1) {
    double consEst = estimateConservativeFee(2 * confTarget, &tempResult);
    if (consEst > median) {
      median = consEst;
      if (feeCalc) {
        feeCalc->est = tempResult;
        feeCalc->reason = FeeReason::CONSERVATIVE;
      }
    }
  }

  if (median < 0)
    return CFeeRate(0);

  return CFeeRate(llround(median));
}

bool CBlockPolicyEstimator::Write(CAutoFile &fileout) const {
  try {
    LOCK(cs_feeEstimator);
    fileout << 149900;

    fileout << CLIENT_VERSION;

    fileout << nBestSeenHeight;
    if (BlockSpan() > HistoricalBlockSpan() / 2) {
      fileout << firstRecordedHeight << nBestSeenHeight;
    } else {
      fileout << historicalFirst << historicalBest;
    }
    fileout << buckets;
    feeStats->Write(fileout);
    shortStats->Write(fileout);
    longStats->Write(fileout);
  } catch (const std::exception &) {
    LogPrintf("CBlockPolicyEstimator::Write(): unable to write policy "
              "estimator data (non-fatal)\n");
    return false;
  }
  return true;
}

bool CBlockPolicyEstimator::Read(CAutoFile &filein) {
  try {
    LOCK(cs_feeEstimator);
    int nVersionRequired, nVersionThatWrote;
    filein >> nVersionRequired >> nVersionThatWrote;
    if (nVersionRequired > CLIENT_VERSION)
      return error(
          "CBlockPolicyEstimator::Read(): up-version (%d) fee estimate file",
          nVersionRequired);

    unsigned int nFileBestSeenHeight;
    filein >> nFileBestSeenHeight;

    if (nVersionRequired < 149900) {
      LogPrintf(
          "%s: incompatible old fee estimation data (non-fatal). Version: %d\n",
          __func__, nVersionRequired);
    } else {
      unsigned int nFileHistoricalFirst, nFileHistoricalBest;
      filein >> nFileHistoricalFirst >> nFileHistoricalBest;
      if (nFileHistoricalFirst > nFileHistoricalBest ||
          nFileHistoricalBest > nFileBestSeenHeight) {
        throw std::runtime_error("Corrupt estimates file. Historical block "
                                 "range for estimates is invalid");
      }
      std::vector<double> fileBuckets;
      filein >> fileBuckets;
      size_t numBuckets = fileBuckets.size();
      if (numBuckets <= 1 || numBuckets > 1000)
        throw std::runtime_error("Corrupt estimates file. Must have between 2 "
                                 "and 1000 feerate buckets");

      std::unique_ptr<TxConfirmStats> fileFeeStats(new TxConfirmStats(
          buckets, bucketMap, MED_BLOCK_PERIODS, MED_DECAY, MED_SCALE));
      std::unique_ptr<TxConfirmStats> fileShortStats(new TxConfirmStats(
          buckets, bucketMap, SHORT_BLOCK_PERIODS, SHORT_DECAY, SHORT_SCALE));
      std::unique_ptr<TxConfirmStats> fileLongStats(new TxConfirmStats(
          buckets, bucketMap, LONG_BLOCK_PERIODS, LONG_DECAY, LONG_SCALE));
      fileFeeStats->Read(filein, nVersionThatWrote, numBuckets);
      fileShortStats->Read(filein, nVersionThatWrote, numBuckets);
      fileLongStats->Read(filein, nVersionThatWrote, numBuckets);

      buckets = fileBuckets;
      bucketMap.clear();
      for (unsigned int i = 0; i < buckets.size(); i++) {
        bucketMap[buckets[i]] = i;
      }

      feeStats = std::move(fileFeeStats);
      shortStats = std::move(fileShortStats);
      longStats = std::move(fileLongStats);

      nBestSeenHeight = nFileBestSeenHeight;
      historicalFirst = nFileHistoricalFirst;
      historicalBest = nFileHistoricalBest;
    }
  } catch (const std::exception &e) {
    LogPrintf("CBlockPolicyEstimator::Read(): unable to read policy estimator "
              "data (non-fatal): %s\n",
              e.what());
    return false;
  }
  return true;
}

void CBlockPolicyEstimator::FlushUnconfirmed(CTxMemPool &pool) {
  int64_t startclear = GetTimeMicros();
  std::vector<uint256> txids;
  pool.queryHashes(txids);
  LOCK(cs_feeEstimator);
  for (auto &txid : txids) {
    removeTx(txid, false);
  }
  int64_t endclear = GetTimeMicros();
  LogPrint(BCLog::ESTIMATEFEE,
           "Recorded %u unconfirmed txs from mempool in %gs\n", txids.size(),
           (endclear - startclear) * 0.000001);
}

FeeFilterRounder::FeeFilterRounder(const CFeeRate &minIncrementalFee) {
  CAmount minFeeLimit =
      std::max(CAmount(1), minIncrementalFee.GetFeePerK() / 2);
  feeset.insert(0);
  for (double bucketBoundary = minFeeLimit;
       bucketBoundary <= MAX_FILTER_FEERATE;
       bucketBoundary *= FEE_FILTER_SPACING) {
    feeset.insert(bucketBoundary);
  }
}

CAmount FeeFilterRounder::round(CAmount currentMinFee) {
  std::set<double>::iterator it = feeset.lower_bound(currentMinFee);
  if ((it != feeset.begin() && insecure_rand.rand32() % 3 != 0) ||
      it == feeset.end()) {
    it--;
  }
  return static_cast<CAmount>(*it);
}
