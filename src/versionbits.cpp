// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/params.h>
#include <validation.h>
#include <versionbits.h>

const struct VBDeploymentInfo
    VersionBitsDeploymentInfo[Consensus::MAX_VERSION_BITS_DEPLOYMENTS] = {
        {
            "testdummy",
            true,
        },
        {
            "csv",
            true,
        },
        {
            "segwit",
            true,
        },
        {
            "hive",
            true,
        },
        {
            "hive_1_1",
            true,
        },
        {
            "minotaurx_and_hive_1_2",
            true,
        },
        {
            "rialto",
            true,
        }};

ThresholdState AbstractThresholdConditionChecker::GetStateFor(
    const CBlockIndex *pindexPrev, const Consensus::Params &params,
    ThresholdConditionCache &cache) const {
  int nPeriod = Period(params);
  int nThreshold = Threshold(params);
  int64_t nTimeStart = BeginTime(params);
  int64_t nTimeTimeout = EndTime(params);

  if (nTimeStart == Consensus::BIP9Deployment::ALWAYS_ACTIVE) {
    return THRESHOLD_ACTIVE;
  }

  if (pindexPrev != nullptr) {
    pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight -
                                         ((pindexPrev->nHeight + 1) % nPeriod));
  }

  std::vector<const CBlockIndex *> vToCompute;
  while (cache.count(pindexPrev) == 0) {
    if (pindexPrev == nullptr) {
      cache[pindexPrev] = THRESHOLD_DEFINED;
      break;
    }
    if (pindexPrev->GetMedianTimePast() < nTimeStart) {
      cache[pindexPrev] = THRESHOLD_DEFINED;
      break;
    }
    vToCompute.push_back(pindexPrev);
    pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);
  }

  assert(cache.count(pindexPrev));
  ThresholdState state = cache[pindexPrev];

  while (!vToCompute.empty()) {
    ThresholdState stateNext = state;
    pindexPrev = vToCompute.back();
    vToCompute.pop_back();

    switch (state) {
    case THRESHOLD_DEFINED: {
      if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
        stateNext = THRESHOLD_FAILED;
      } else if (pindexPrev->GetMedianTimePast() >= nTimeStart) {
        stateNext = THRESHOLD_STARTED;
      }
      break;
    }
    case THRESHOLD_STARTED: {
      if (pindexPrev->GetMedianTimePast() >= nTimeTimeout) {
        stateNext = THRESHOLD_FAILED;
        break;
      }

      const CBlockIndex *pindexCount = pindexPrev;
      int count = 0;
      for (int i = 0; i < nPeriod; i++) {
        if (Condition(pindexCount, params)) {
          count++;
        }
        pindexCount = pindexCount->pprev;
      }
      if (count >= nThreshold) {
        stateNext = THRESHOLD_LOCKED_IN;
      }
      break;
    }
    case THRESHOLD_LOCKED_IN: {
      stateNext = THRESHOLD_ACTIVE;
      break;
    }
    case THRESHOLD_FAILED:
    case THRESHOLD_ACTIVE: {
      break;
    }
    }
    cache[pindexPrev] = state = stateNext;
  }

  return state;
}

BIP9Stats AbstractThresholdConditionChecker::GetStateStatisticsFor(
    const CBlockIndex *pindex, const Consensus::Params &params) const {
  BIP9Stats stats = {};

  stats.period = Period(params);
  stats.threshold = Threshold(params);

  if (pindex == nullptr)
    return stats;

  const CBlockIndex *pindexEndOfPrevPeriod = pindex->GetAncestor(
      pindex->nHeight - ((pindex->nHeight + 1) % stats.period));
  stats.elapsed = pindex->nHeight - pindexEndOfPrevPeriod->nHeight;

  int count = 0;
  const CBlockIndex *currentIndex = pindex;
  while (pindexEndOfPrevPeriod->nHeight != currentIndex->nHeight) {
    if (Condition(currentIndex, params))
      count++;
    currentIndex = currentIndex->pprev;
  }

  stats.count = count;
  stats.possible = (stats.period - stats.threshold) >= (stats.elapsed - count);

  return stats;
}

int AbstractThresholdConditionChecker::GetStateSinceHeightFor(
    const CBlockIndex *pindexPrev, const Consensus::Params &params,
    ThresholdConditionCache &cache) const {
  int64_t start_time = BeginTime(params);
  if (start_time == Consensus::BIP9Deployment::ALWAYS_ACTIVE) {
    return 0;
  }

  const ThresholdState initialState = GetStateFor(pindexPrev, params, cache);

  if (initialState == THRESHOLD_DEFINED) {
    return 0;
  }

  const int nPeriod = Period(params);

  pindexPrev = pindexPrev->GetAncestor(pindexPrev->nHeight -
                                       ((pindexPrev->nHeight + 1) % nPeriod));

  const CBlockIndex *previousPeriodParent =
      pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);

  while (previousPeriodParent != nullptr &&
         GetStateFor(previousPeriodParent, params, cache) == initialState) {
    pindexPrev = previousPeriodParent;
    previousPeriodParent =
        pindexPrev->GetAncestor(pindexPrev->nHeight - nPeriod);
  }

  return pindexPrev->nHeight + 1;
}

namespace {
class VersionBitsConditionChecker : public AbstractThresholdConditionChecker {
private:
  const Consensus::DeploymentPos id;

protected:
  int64_t BeginTime(const Consensus::Params &params) const override {
    return params.vDeployments[id].nStartTime;
  }
  int64_t EndTime(const Consensus::Params &params) const override {
    return params.vDeployments[id].nTimeout;
  }
  int Period(const Consensus::Params &params) const override {
    return params.nMinerConfirmationWindow;
  }
  int Threshold(const Consensus::Params &params) const override {
    return params.nRuleChangeActivationThreshold;
  }

  bool Condition(const CBlockIndex *pindex,
                 const Consensus::Params &params) const override {
    if (pindex->nTime > params.powForkTime)
      return (pindex->nVersion & Mask(params)) != 0;
    else
      return (
          ((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) &&
          (pindex->nVersion & Mask(params)) != 0);
  }

public:
  explicit VersionBitsConditionChecker(Consensus::DeploymentPos id_)
      : id(id_) {}
  uint32_t Mask(const Consensus::Params &params) const {
    return ((uint32_t)1) << params.vDeployments[id].bit;
  }
};

} // namespace

ThresholdState VersionBitsState(const CBlockIndex *pindexPrev,
                                const Consensus::Params &params,
                                Consensus::DeploymentPos pos,
                                VersionBitsCache &cache) {
  return VersionBitsConditionChecker(pos).GetStateFor(pindexPrev, params,
                                                      cache.caches[pos]);
}

BIP9Stats VersionBitsStatistics(const CBlockIndex *pindexPrev,
                                const Consensus::Params &params,
                                Consensus::DeploymentPos pos) {
  return VersionBitsConditionChecker(pos).GetStateStatisticsFor(pindexPrev,
                                                                params);
}

int VersionBitsStateSinceHeight(const CBlockIndex *pindexPrev,
                                const Consensus::Params &params,
                                Consensus::DeploymentPos pos,
                                VersionBitsCache &cache) {
  return VersionBitsConditionChecker(pos).GetStateSinceHeightFor(
      pindexPrev, params, cache.caches[pos]);
}

uint32_t VersionBitsMask(const Consensus::Params &params,
                         Consensus::DeploymentPos pos) {
  return VersionBitsConditionChecker(pos).Mask(params);
}

void VersionBitsCache::Clear() {
  for (unsigned int d = 0; d < Consensus::MAX_VERSION_BITS_DEPLOYMENTS; d++) {
    caches[d].clear();
  }
}
