// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2025 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <txmempool.h>

#include <consensus/consensus.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <reverse_iterator.h>
#include <streams.h>
#include <timedata.h>
#include <util.h>
#include <utilmoneystr.h>
#include <utiltime.h>
#include <validation.h>

CTxMemPoolEntry::CTxMemPoolEntry(const CTransactionRef &_tx,
                                 const CAmount &_nFee, int64_t _nTime,
                                 unsigned int _entryHeight,
                                 bool _spendsCoinbase, int64_t _sigOpsCost,
                                 LockPoints lp)
    : tx(_tx), nFee(_nFee), nTime(_nTime), entryHeight(_entryHeight),
      spendsCoinbase(_spendsCoinbase), sigOpCost(_sigOpsCost), lockPoints(lp) {
  nTxWeight = GetTransactionWeight(*tx);
  nUsageSize = RecursiveDynamicUsage(tx);

  nCountWithDescendants = 1;
  nSizeWithDescendants = GetTxSize();
  nModFeesWithDescendants = nFee;

  feeDelta = 0;

  nCountWithAncestors = 1;
  nSizeWithAncestors = GetTxSize();
  nModFeesWithAncestors = nFee;
  nSigOpCostWithAncestors = sigOpCost;
}

void CTxMemPoolEntry::UpdateFeeDelta(int64_t newFeeDelta) {
  nModFeesWithDescendants += newFeeDelta - feeDelta;
  nModFeesWithAncestors += newFeeDelta - feeDelta;
  feeDelta = newFeeDelta;
}

void CTxMemPoolEntry::UpdateLockPoints(const LockPoints &lp) {
  lockPoints = lp;
}

size_t CTxMemPoolEntry::GetTxSize() const {
  return GetVirtualTransactionSize(nTxWeight, sigOpCost);
}

void CTxMemPool::UpdateForDescendants(txiter updateIt,
                                      cacheMap &cachedDescendants,
                                      const std::set<uint256> &setExclude) {
  setEntries stageEntries, setAllDescendants;
  stageEntries = GetMemPoolChildren(updateIt);

  while (!stageEntries.empty()) {
    const txiter cit = *stageEntries.begin();
    setAllDescendants.insert(cit);
    stageEntries.erase(cit);
    const setEntries &setChildren = GetMemPoolChildren(cit);
    for (const txiter childEntry : setChildren) {
      cacheMap::iterator cacheIt = cachedDescendants.find(childEntry);
      if (cacheIt != cachedDescendants.end()) {

        for (const txiter cacheEntry : cacheIt->second) {
          setAllDescendants.insert(cacheEntry);
        }
      } else if (!setAllDescendants.count(childEntry)) {

        stageEntries.insert(childEntry);
      }
    }
  }

  int64_t modifySize = 0;
  CAmount modifyFee = 0;
  int64_t modifyCount = 0;
  for (txiter cit : setAllDescendants) {
    if (!setExclude.count(cit->GetTx().GetHash())) {
      modifySize += cit->GetTxSize();
      modifyFee += cit->GetModifiedFee();
      modifyCount++;
      cachedDescendants[updateIt].insert(cit);

      mapTx.modify(cit, update_ancestor_state(updateIt->GetTxSize(),
                                              updateIt->GetModifiedFee(), 1,
                                              updateIt->GetSigOpCost()));
    }
  }
  mapTx.modify(updateIt,
               update_descendant_state(modifySize, modifyFee, modifyCount));
}

void CTxMemPool::UpdateTransactionsFromBlock(
    const std::vector<uint256> &vHashesToUpdate) {
  LOCK(cs);

  cacheMap mapMemPoolDescendantsToUpdate;

  std::set<uint256> setAlreadyIncluded(vHashesToUpdate.begin(),
                                       vHashesToUpdate.end());

  for (const uint256 &hash : reverse_iterate(vHashesToUpdate)) {

    setEntries setChildren;

    txiter it = mapTx.find(hash);
    if (it == mapTx.end()) {
      continue;
    }
    auto iter = mapNextTx.lower_bound(COutPoint(hash, 0));

    for (; iter != mapNextTx.end() && iter->first->hash == hash; ++iter) {
      const uint256 &childHash = iter->second->GetHash();
      txiter childIter = mapTx.find(childHash);
      assert(childIter != mapTx.end());

      if (setChildren.insert(childIter).second &&
          !setAlreadyIncluded.count(childHash)) {
        UpdateChild(it, childIter, true);
        UpdateParent(childIter, it, true);
      }
    }
    UpdateForDescendants(it, mapMemPoolDescendantsToUpdate, setAlreadyIncluded);
  }
}

bool CTxMemPool::CalculateMemPoolAncestors(const CTxMemPoolEntry &entry,
                                           setEntries &setAncestors,
                                           size_t limit_ancestors,
                                           size_t limit_descendants,
                                           size_t limit_ancestor_size,
                                           size_t limit_descendant_size,
                                           std::string &errString) const {
  AssertLockHeld(cs);
  setAncestors.clear();

  std::deque<txiter> queue;

  for (const CTxIn &txin : entry.GetTx().vin) {
    txiter parentIt = mapTx.find(txin.prevout.hash);
    if (parentIt != mapTx.end()) {
      if (setAncestors.insert(parentIt).second) {
        queue.push_back(parentIt);
      }
    }
  }

  while (!queue.empty()) {
    txiter curr = queue.front();
    queue.pop_front();

    for (const CTxIn &txin : curr->GetTx().vin) {
      txiter parentIt = mapTx.find(txin.prevout.hash);
      if (parentIt != mapTx.end()) {
        if (setAncestors.insert(parentIt).second) {

          if (setAncestors.size() > limit_ancestors) {
            errString = strprintf("too many ancestors: %u > %u",
                                  setAncestors.size(), limit_ancestors);
            return false;
          }
          queue.push_back(parentIt);
        }
      }
    }
  }

  size_t totalAncestorSize = entry.GetSizeWithAncestors();
  CAmount totalAncestorFees = entry.GetModFeesWithAncestors();
  for (const txiter &ae : setAncestors) {
    totalAncestorSize += ae->GetTxSize();
    totalAncestorFees += ae->GetFee();
    if (ae->GetCountWithDescendants() + 1 > limit_descendants) {
      errString = "would exceed descendant limit";
      return false;
    }
    if (ae->GetSizeWithDescendants() + entry.GetTxSize() >
        limit_descendant_size) {
      errString = "would exceed descendant size limit";
      return false;
    }
  }

  const size_t PACKAGE_ANCESTOR_THRESHOLD = 6;

  const CFeeRate MIN_PACKAGE_FEE_RATE =
      std::max(incrementalRelayFee, GetMinFee(mapTx.size()));
  if (setAncestors.size() > PACKAGE_ANCESTOR_THRESHOLD) {

    CFeeRate packageRate(totalAncestorFees, totalAncestorSize > 0
                                                ? totalAncestorSize
                                                : entry.GetTxSize());
    if (packageRate < MIN_PACKAGE_FEE_RATE) {
      errString =
          strprintf("package feerate too low: %s < %s (ancestors=%u)",
                    packageRate.ToString(), MIN_PACKAGE_FEE_RATE.ToString(),
                    setAncestors.size());
      return false;
    }
  }

  return true;
}

void CTxMemPool::UpdateAncestorsOf(bool add, txiter it,
                                   setEntries &setAncestors) {
  setEntries parentIters = GetMemPoolParents(it);

  for (txiter piter : parentIters) {
    UpdateChild(piter, it, add);
  }
  const int64_t updateCount = (add ? 1 : -1);
  const int64_t updateSize = updateCount * it->GetTxSize();
  const CAmount updateFee = updateCount * it->GetModifiedFee();
  for (txiter ancestorIt : setAncestors) {
    mapTx.modify(ancestorIt,
                 update_descendant_state(updateSize, updateFee, updateCount));
  }
}

void CTxMemPool::UpdateEntryForAncestors(txiter it,
                                         const setEntries &setAncestors) {
  int64_t updateCount = setAncestors.size();
  int64_t updateSize = 0;
  CAmount updateFee = 0;
  int64_t updateSigOpsCost = 0;
  for (txiter ancestorIt : setAncestors) {
    updateSize += ancestorIt->GetTxSize();
    updateFee += ancestorIt->GetModifiedFee();
    updateSigOpsCost += ancestorIt->GetSigOpCost();
  }
  mapTx.modify(it, update_ancestor_state(updateSize, updateFee, updateCount,
                                         updateSigOpsCost));
}

void CTxMemPool::UpdateChildrenForRemoval(txiter it) {
  const setEntries &setMemPoolChildren = GetMemPoolChildren(it);
  for (txiter updateIt : setMemPoolChildren) {
    UpdateParent(updateIt, it, false);
  }
}

void CTxMemPool::UpdateForRemoveFromMempool(const setEntries &entriesToRemove,
                                            bool updateDescendants) {

  const uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
  if (updateDescendants) {

    for (txiter removeIt : entriesToRemove) {
      setEntries setDescendants;
      CalculateDescendants(removeIt, setDescendants);
      setDescendants.erase(removeIt);

      int64_t modifySize = -((int64_t)removeIt->GetTxSize());
      CAmount modifyFee = -removeIt->GetModifiedFee();
      int modifySigOps = -removeIt->GetSigOpCost();
      for (txiter dit : setDescendants) {
        mapTx.modify(dit, update_ancestor_state(modifySize, modifyFee, -1,
                                                modifySigOps));
      }
    }
  }

  for (txiter removeIt : entriesToRemove) {
    setEntries setAncestors;
    const CTxMemPoolEntry &entry = *removeIt;
    std::string dummy;

    CalculateMemPoolAncestors(entry, setAncestors, nNoLimit, nNoLimit, nNoLimit,
                              nNoLimit, dummy);

    UpdateAncestorsOf(false, removeIt, setAncestors);
  }

  for (txiter removeIt : entriesToRemove) {
    UpdateChildrenForRemoval(removeIt);
  }
}

void CTxMemPoolEntry::UpdateDescendantState(int64_t modifySize,
                                            CAmount modifyFee,
                                            int64_t modifyCount) {
  nSizeWithDescendants += modifySize;
  assert(int64_t(nSizeWithDescendants) > 0);
  nModFeesWithDescendants += modifyFee;
  nCountWithDescendants += modifyCount;
  assert(int64_t(nCountWithDescendants) > 0);
}

void CTxMemPoolEntry::UpdateAncestorState(int64_t modifySize, CAmount modifyFee,
                                          int64_t modifyCount,
                                          int64_t modifySigOps) {
  nSizeWithAncestors += modifySize;
  assert(int64_t(nSizeWithAncestors) > 0);
  nModFeesWithAncestors += modifyFee;
  nCountWithAncestors += modifyCount;
  assert(int64_t(nCountWithAncestors) > 0);
  nSigOpCostWithAncestors += modifySigOps;
  assert(int(nSigOpCostWithAncestors) >= 0);
}

CTxMemPool::CTxMemPool(CBlockPolicyEstimator *estimator)
    : nTransactionsUpdated(0), minerPolicyEstimator(estimator) {
  _clear();

  nCheckFrequency = 0;
}

bool CTxMemPool::isSpent(const COutPoint &outpoint) {
  LOCK(cs);
  return mapNextTx.count(outpoint);
}

unsigned int CTxMemPool::GetTransactionsUpdated() const {
  LOCK(cs);
  return nTransactionsUpdated;
}

void CTxMemPool::AddTransactionsUpdated(unsigned int n) {
  LOCK(cs);
  nTransactionsUpdated += n;
}

void CTxMemPool::addUnchecked(const uint256 &hash, CTxMemPoolEntry &entry,
                              bool validFeeEstimate) {
  LOCK(cs);

  size_t MAX_PER_SOURCE = 50;

  uint256 sourceBucket;
  if (!entry.GetTx().vin.empty()) {
    sourceBucket = entry.GetTx().vin[0].prevout.hash;
  } else {

    sourceBucket = hash;
  }

  auto itCount = mapSourceCounts.find(sourceBucket);
  size_t currentCount =
      (itCount == mapSourceCounts.end()) ? 0 : itCount->second;
  if (currentCount >= MAX_PER_SOURCE) {

    LogPrint(BCLog::MEMPOOL,
             "addUnchecked: rejecting tx %s from source %s — per-source cap "
             "reached (%u)\n",
             hash.ToString(), sourceBucket.ToString(), (unsigned)currentCount);

    return;
  }

  auto ret = mapTx.insert(entry);
  txiter it = ret.first;

  mapSourceCounts[sourceBucket] = currentCount + 1;

  NotifyEntryAdded(it, validFeeEstimate);
}

void CTxMemPool::removeUnchecked(txiter it, MemPoolRemovalReason reason) {
  NotifyEntryRemoved(it->GetSharedTx(), reason);
  const uint256 hash = it->GetTx().GetHash();
  for (const CTxIn &txin : it->GetTx().vin)
    mapNextTx.erase(txin.prevout);

  if (vTxHashes.size() > 1) {
    vTxHashes[it->vTxHashesIdx] = std::move(vTxHashes.back());
    vTxHashes[it->vTxHashesIdx].second->vTxHashesIdx = it->vTxHashesIdx;
    vTxHashes.pop_back();
    if (vTxHashes.size() * 2 < vTxHashes.capacity())
      vTxHashes.shrink_to_fit();
  } else
    vTxHashes.clear();

  totalTxSize -= it->GetTxSize();
  cachedInnerUsage -= it->DynamicMemoryUsage();
  cachedInnerUsage -= memusage::DynamicUsage(mapLinks[it].parents) +
                      memusage::DynamicUsage(mapLinks[it].children);
  mapLinks.erase(it);
  mapTx.erase(it);
  nTransactionsUpdated++;
  if (minerPolicyEstimator) {
    minerPolicyEstimator->removeTx(hash, false);
  }
}

void CTxMemPool::CalculateDescendants(txiter entryit,
                                      setEntries &setDescendants) {
  setEntries stage;
  if (setDescendants.count(entryit) == 0) {
    stage.insert(entryit);
  }

  while (!stage.empty()) {
    txiter it = *stage.begin();
    setDescendants.insert(it);
    stage.erase(it);

    const setEntries &setChildren = GetMemPoolChildren(it);
    for (const txiter &childiter : setChildren) {
      if (!setDescendants.count(childiter)) {
        stage.insert(childiter);
      }
    }
  }
}

void CTxMemPool::removeRecursive(const CTransaction &origTx,
                                 MemPoolRemovalReason reason) {

  {
    LOCK(cs);
    setEntries txToRemove;
    txiter origit = mapTx.find(origTx.GetHash());
    if (origit != mapTx.end()) {
      txToRemove.insert(origit);
    } else {

      for (unsigned int i = 0; i < origTx.vout.size(); i++) {
        auto it = mapNextTx.find(COutPoint(origTx.GetHash(), i));
        if (it == mapNextTx.end())
          continue;
        txiter nextit = mapTx.find(it->second->GetHash());
        assert(nextit != mapTx.end());
        txToRemove.insert(nextit);
      }
    }
    setEntries setAllRemoves;
    for (txiter it : txToRemove) {
      CalculateDescendants(it, setAllRemoves);
    }

    RemoveStaged(setAllRemoves, false, reason);
  }
}

void CTxMemPool::removeForReorg(const CCoinsViewCache *pcoins,
                                unsigned int nMemPoolHeight, int flags) {

  LOCK(cs);
  setEntries txToRemove;
  for (indexed_transaction_set::const_iterator it = mapTx.begin();
       it != mapTx.end(); it++) {
    const CTransaction &tx = it->GetTx();
    LockPoints lp = it->GetLockPoints();
    bool validLP = TestLockPointValidity(&lp);
    if (!CheckFinalTx(tx, flags) ||
        !CheckSequenceLocks(tx, flags, &lp, validLP)) {

      txToRemove.insert(it);
    } else if (it->GetSpendsCoinbase()) {
      for (const CTxIn &txin : tx.vin) {
        indexed_transaction_set::const_iterator it2 =
            mapTx.find(txin.prevout.hash);
        if (it2 != mapTx.end())
          continue;
        const Coin &coin = pcoins->AccessCoin(txin.prevout);
        if (nCheckFrequency != 0)
          assert(!coin.IsSpent());
        if (coin.IsSpent() ||
            (coin.IsCoinBase() && ((signed long)nMemPoolHeight) - coin.nHeight <
                                      COINBASE_MATURITY)) {
          txToRemove.insert(it);
          break;
        }
      }
    }
    if (!validLP) {
      mapTx.modify(it, update_lock_points(lp));
    }
  }
  setEntries setAllRemoves;
  for (txiter it : txToRemove) {
    CalculateDescendants(it, setAllRemoves);
  }
  RemoveStaged(setAllRemoves, false, MemPoolRemovalReason::REORG);
}

void CTxMemPool::removeConflicts(const CTransaction &tx) {
  LOCK(cs);
  for (const CTxIn &txin : tx.vin) {
    auto it = mapNextTx.find(txin.prevout);
    if (it == mapNextTx.end())
      continue;
    const CTransaction &txConflict = *it->second;

    if (txConflict.GetHash() != tx.GetHash()) {
      ClearPrioritisation(txConflict.GetHash());
      removeRecursive(txConflict, MemPoolRemovalReason::CONFLICT);
    }
  }
}

void CTxMemPool::removeForBlock(const std::vector<CTransactionRef> &vtx,
                                unsigned int nBlockHeight) {
  LOCK(cs);
  std::vector<const CTxMemPoolEntry *> entries;
  for (const auto &tx : vtx) {
    uint256 hash = tx->GetHash();

    indexed_transaction_set::iterator i = mapTx.find(hash);
    if (i != mapTx.end())
      entries.push_back(&*i);
  }

  if (minerPolicyEstimator) {
    minerPolicyEstimator->processBlock(nBlockHeight, entries);
  }
  for (const auto &tx : vtx) {
    txiter it = mapTx.find(tx->GetHash());
    if (it != mapTx.end()) {
      setEntries stage;
      stage.insert(it);
      RemoveStaged(stage, true, MemPoolRemovalReason::BLOCK);
    }
    removeConflicts(*tx);
    ClearPrioritisation(tx->GetHash());
  }
  lastRollingFeeUpdate = GetTime();
  blockSinceLastRollingFeeBump = true;
}

void CTxMemPool::_clear() {
  mapLinks.clear();
  mapTx.clear();
  mapNextTx.clear();
  totalTxSize = 0;
  cachedInnerUsage = 0;
  lastRollingFeeUpdate = GetTime();
  blockSinceLastRollingFeeBump = false;
  rollingMinimumFeeRate = 0;
  ++nTransactionsUpdated;
}

void CTxMemPool::clear() {
  LOCK(cs);
  _clear();
}

static void CheckInputsAndUpdateCoins(const CTransaction &tx,
                                      CCoinsViewCache &mempoolDuplicate,
                                      const int64_t spendheight) {
  CValidationState state;
  CAmount txfee = 0;
  bool fCheckResult =
      tx.IsCoinBase() ||
      Consensus::CheckTxInputs(tx, state, mempoolDuplicate, spendheight, txfee);
  assert(fCheckResult);
  UpdateCoins(tx, mempoolDuplicate, 1000000);
}

void CTxMemPool::check(const CCoinsViewCache *pcoins) const {
  if (nCheckFrequency == 0)
    return;

  if (GetRand(std::numeric_limits<uint32_t>::max()) >= nCheckFrequency)
    return;

  LogPrint(BCLog::MEMPOOL,
           "Checking mempool with %u transactions and %u inputs\n",
           (unsigned int)mapTx.size(), (unsigned int)mapNextTx.size());

  uint64_t checkTotal = 0;
  uint64_t innerUsage = 0;

  CCoinsViewCache mempoolDuplicate(const_cast<CCoinsViewCache *>(pcoins));
  const int64_t spendheight = GetSpendHeight(mempoolDuplicate);

  LOCK(cs);
  std::list<const CTxMemPoolEntry *> waitingOnDependants;
  for (indexed_transaction_set::const_iterator it = mapTx.begin();
       it != mapTx.end(); it++) {
    unsigned int i = 0;
    checkTotal += it->GetTxSize();
    innerUsage += it->DynamicMemoryUsage();
    const CTransaction &tx = it->GetTx();
    txlinksMap::const_iterator linksiter = mapLinks.find(it);
    assert(linksiter != mapLinks.end());
    const TxLinks &links = linksiter->second;
    innerUsage += memusage::DynamicUsage(links.parents) +
                  memusage::DynamicUsage(links.children);
    bool fDependsWait = false;
    setEntries setParentCheck;
    int64_t parentSizes = 0;
    int64_t parentSigOpCost = 0;
    for (const CTxIn &txin : tx.vin) {

      indexed_transaction_set::const_iterator it2 =
          mapTx.find(txin.prevout.hash);
      if (it2 != mapTx.end()) {
        const CTransaction &tx2 = it2->GetTx();
        assert(tx2.vout.size() > txin.prevout.n &&
               !tx2.vout[txin.prevout.n].IsNull());
        fDependsWait = true;
        if (setParentCheck.insert(it2).second) {
          parentSizes += it2->GetTxSize();
          parentSigOpCost += it2->GetSigOpCost();
        }
      } else {
        assert(pcoins->HaveCoin(txin.prevout));
      }

      auto it3 = mapNextTx.find(txin.prevout);
      assert(it3 != mapNextTx.end());
      assert(it3->first == &txin.prevout);
      assert(it3->second == &tx);
      i++;
    }
    assert(setParentCheck == GetMemPoolParents(it));

    setEntries setAncestors;
    uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    CalculateMemPoolAncestors(*it, setAncestors, nNoLimit, nNoLimit, nNoLimit,
                              nNoLimit, dummy);
    uint64_t nCountCheck = setAncestors.size() + 1;
    uint64_t nSizeCheck = it->GetTxSize();
    CAmount nFeesCheck = it->GetModifiedFee();
    int64_t nSigOpCheck = it->GetSigOpCost();

    for (txiter ancestorIt : setAncestors) {
      nSizeCheck += ancestorIt->GetTxSize();
      nFeesCheck += ancestorIt->GetModifiedFee();
      nSigOpCheck += ancestorIt->GetSigOpCost();
    }

    assert(it->GetCountWithAncestors() == nCountCheck);
    assert(it->GetSizeWithAncestors() == nSizeCheck);
    assert(it->GetSigOpCostWithAncestors() == nSigOpCheck);
    assert(it->GetModFeesWithAncestors() == nFeesCheck);

    CTxMemPool::setEntries setChildrenCheck;
    auto iter = mapNextTx.lower_bound(COutPoint(it->GetTx().GetHash(), 0));
    int64_t childSizes = 0;
    for (;
         iter != mapNextTx.end() && iter->first->hash == it->GetTx().GetHash();
         ++iter) {
      txiter childit = mapTx.find(iter->second->GetHash());
      assert(childit != mapTx.end());

      if (setChildrenCheck.insert(childit).second) {
        childSizes += childit->GetTxSize();
      }
    }
    assert(setChildrenCheck == GetMemPoolChildren(it));

    assert(it->GetSizeWithDescendants() >= childSizes + it->GetTxSize());

    if (fDependsWait)
      waitingOnDependants.push_back(&(*it));
    else {
      CheckInputsAndUpdateCoins(tx, mempoolDuplicate, spendheight);
    }
  }
  unsigned int stepsSinceLastRemove = 0;
  while (!waitingOnDependants.empty()) {
    const CTxMemPoolEntry *entry = waitingOnDependants.front();
    waitingOnDependants.pop_front();
    CValidationState state;
    if (!mempoolDuplicate.HaveInputs(entry->GetTx())) {
      waitingOnDependants.push_back(entry);
      stepsSinceLastRemove++;
      assert(stepsSinceLastRemove < waitingOnDependants.size());
    } else {
      CheckInputsAndUpdateCoins(entry->GetTx(), mempoolDuplicate, spendheight);
      stepsSinceLastRemove = 0;
    }
  }
  for (auto it = mapNextTx.cbegin(); it != mapNextTx.cend(); it++) {
    uint256 hash = it->second->GetHash();
    indexed_transaction_set::const_iterator it2 = mapTx.find(hash);
    const CTransaction &tx = it2->GetTx();
    assert(it2 != mapTx.end());
    assert(&tx == it->second);
  }

  assert(totalTxSize == checkTotal);
  assert(innerUsage == cachedInnerUsage);
}

bool CTxMemPool::CompareDepthAndScore(const uint256 &hasha,
                                      const uint256 &hashb) {
  LOCK(cs);
  indexed_transaction_set::const_iterator i = mapTx.find(hasha);
  if (i == mapTx.end())
    return false;
  indexed_transaction_set::const_iterator j = mapTx.find(hashb);
  if (j == mapTx.end())
    return true;
  uint64_t counta = i->GetCountWithAncestors();
  uint64_t countb = j->GetCountWithAncestors();
  if (counta == countb) {
    return CompareTxMemPoolEntryByScore()(*i, *j);
  }
  return counta < countb;
}

namespace {
class DepthAndScoreComparator {
public:
  bool
  operator()(const CTxMemPool::indexed_transaction_set::const_iterator &a,
             const CTxMemPool::indexed_transaction_set::const_iterator &b) {
    uint64_t counta = a->GetCountWithAncestors();
    uint64_t countb = b->GetCountWithAncestors();
    if (counta == countb) {
      return CompareTxMemPoolEntryByScore()(*a, *b);
    }
    return counta < countb;
  }
};
} // namespace

std::vector<CTxMemPool::indexed_transaction_set::const_iterator>
CTxMemPool::GetSortedDepthAndScore() const {
  std::vector<indexed_transaction_set::const_iterator> iters;
  AssertLockHeld(cs);

  iters.reserve(mapTx.size());

  for (indexed_transaction_set::iterator mi = mapTx.begin(); mi != mapTx.end();
       ++mi) {
    iters.push_back(mi);
  }
  std::sort(iters.begin(), iters.end(), DepthAndScoreComparator());
  return iters;
}

void CTxMemPool::queryHashes(std::vector<uint256> &vtxid) {
  LOCK(cs);
  auto iters = GetSortedDepthAndScore();

  vtxid.clear();
  vtxid.reserve(mapTx.size());

  for (auto it : iters) {
    vtxid.push_back(it->GetTx().GetHash());
  }
}

static TxMempoolInfo
GetInfo(CTxMemPool::indexed_transaction_set::const_iterator it) {
  return TxMempoolInfo{it->GetSharedTx(), it->GetTime(),
                       CFeeRate(it->GetFee(), it->GetTxSize()),
                       it->GetModifiedFee() - it->GetFee()};
}

std::vector<TxMempoolInfo> CTxMemPool::infoAll() const {
  LOCK(cs);
  auto iters = GetSortedDepthAndScore();

  std::vector<TxMempoolInfo> ret;
  ret.reserve(mapTx.size());
  for (auto it : iters) {
    ret.push_back(GetInfo(it));
  }

  return ret;
}

CTransactionRef CTxMemPool::get(const uint256 &hash) const {
  LOCK(cs);
  indexed_transaction_set::const_iterator i = mapTx.find(hash);
  if (i == mapTx.end())
    return nullptr;
  return i->GetSharedTx();
}

TxMempoolInfo CTxMemPool::info(const uint256 &hash) const {
  LOCK(cs);
  indexed_transaction_set::const_iterator i = mapTx.find(hash);
  if (i == mapTx.end())
    return TxMempoolInfo();
  return GetInfo(i);
}

void CTxMemPool::PrioritiseTransaction(const uint256 &hash,
                                       const CAmount nFeeDelta) {
  LOCK(cs);

  const CAmount MAX_EFFECTIVE_DELTA = 1000 * COIN;

  const int64_t DELTA_EXPIRY_SECONDS = 24 * 60 * 60;

  auto it = mapDeltas.find(hash);
  int64_t now = GetTime();
  if (it == mapDeltas.end()) {

    mapDeltas[hash] = nFeeDelta;
    mapDeltasTime[hash] = now;
  } else {

    __int128 sum = (__int128)it->second + (__int128)nFeeDelta;
    if (sum > MAX_EFFECTIVE_DELTA)
      it->second = MAX_EFFECTIVE_DELTA;
    else if (sum < std::numeric_limits<CAmount>::min())
      it->second = std::numeric_limits<CAmount>::min();
    else
      it->second = (CAmount)sum;
    mapDeltasTime[hash] = now;
  }

  auto tIt = mapDeltasTime.find(hash);
  if (tIt != mapDeltasTime.end() &&
      (now - tIt->second) > DELTA_EXPIRY_SECONDS) {

    mapDeltas.erase(hash);
    mapDeltasTime.erase(tIt);
  }

  auto txit = mapTx.find(hash);
  const int64_t PRIORITISE_MIN_RECALC_INTERVAL = 5;

  if (txit != mapTx.end()) {
    int64_t &lastRecalc = mapLastPrioritiseRecalc[hash];
    if (GetTime() - lastRecalc > PRIORITISE_MIN_RECALC_INTERVAL) {

      mapTx.modify(txit, update_fee_delta(mapDeltas[hash]));
      lastRecalc = GetTime();
    } else {
      LogPrint(BCLog::MEMPOOL,
               "PrioritiseTransaction: skipping expensive recalc for %s "
               "(rate-limited)\n",
               hash.ToString());
    }
  }
}

void CTxMemPool::ApplyDelta(const uint256 hash, CAmount &nFeeDelta) const {
  LOCK(cs);
  std::map<uint256, CAmount>::const_iterator pos = mapDeltas.find(hash);
  if (pos == mapDeltas.end())
    return;
  const CAmount &delta = pos->second;
  nFeeDelta += delta;
}

void CTxMemPool::ClearPrioritisation(const uint256 hash) {
  LOCK(cs);
  mapDeltas.erase(hash);
}

bool CTxMemPool::HasNoInputsOf(const CTransaction &tx) const {
  for (unsigned int i = 0; i < tx.vin.size(); i++)
    if (exists(tx.vin[i].prevout.hash))
      return false;
  return true;
}

CCoinsViewMemPool::CCoinsViewMemPool(CCoinsView *baseIn,
                                     const CTxMemPool &mempoolIn)
    : CCoinsViewBacked(baseIn), mempool(mempoolIn) {}

bool CCoinsViewMemPool::GetCoin(const COutPoint &outpoint, Coin &coin) const {
  CTransactionRef ptx = mempool.get(outpoint.hash);
  if (ptx) {
    if (outpoint.n < ptx->vout.size()) {
      coin = Coin(ptx->vout[outpoint.n], MEMPOOL_HEIGHT, false);
      return true;
    } else {

      LogPrint(BCLog::MEMPOOL,
               "CCoinsViewMemPool::GetCoin: outpoint %s index %u >= "
               "vout.size()=%u\n",
               outpoint.hash.ToString(), outpoint.n,
               (unsigned)ptx->vout.size());
      return false;
    }
  }
  return base->GetCoin(outpoint, coin);
}

void CTxMemPool::RemoveStaged(setEntries &stage, bool updateDescendants,
                              MemPoolRemovalReason reason) {
  AssertLockHeld(cs);
  UpdateForRemoveFromMempool(stage, updateDescendants);
  for (const txiter &it : stage) {
    removeUnchecked(it, reason);
  }
}

size_t CTxMemPool::DynamicMemoryUsage() const {
  LOCK(cs);
  const size_t elemSize =
      memusage::MallocUsage(sizeof(CTxMemPoolEntry) + 12 * sizeof(void *));
  const size_t count = mapTx.size();
  size_t baseUsage = 0;
  if (count == 0) {
    baseUsage = 0;
  } else {
    if (elemSize > 0 && count > std::numeric_limits<size_t>::max() / elemSize) {

      baseUsage = std::numeric_limits<size_t>::max();
      LogPrint(
          BCLog::MEMPOOL,
          "DynamicMemoryUsage: multiplication overflow detected, clamped\n");
    } else {
      baseUsage = elemSize * count;
    }
  }

  baseUsage += memusage::DynamicUsage(mapNextTx);
  baseUsage += memusage::DynamicUsage(mapDeltas);
  baseUsage += memusage::DynamicUsage(mapLinks);
  baseUsage += memusage::DynamicUsage(vTxHashes);
  baseUsage += cachedInnerUsage;
  return baseUsage;
}

int CTxMemPool::Expire(int64_t time) {
  LOCK(cs);
  indexed_transaction_set::index<entry_time>::type::iterator it =
      mapTx.get<entry_time>().begin();
  setEntries toremove;
  while (it != mapTx.get<entry_time>().end() && it->GetTime() < time) {
    toremove.insert(mapTx.project<0>(it));
    it++;
  }
  setEntries stage;
  for (txiter removeit : toremove) {
    CalculateDescendants(removeit, stage);
  }
  RemoveStaged(stage, false, MemPoolRemovalReason::EXPIRY);
  return stage.size();
}

void CTxMemPool::UpdateChild(txiter entry, txiter child, bool add) {
  setEntries s;
  if (add && mapLinks[entry].children.insert(child).second) {
    cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
  } else if (!add && mapLinks[entry].children.erase(child)) {
    cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
  }
}

void CTxMemPool::UpdateParent(txiter entry, txiter parent, bool add) {
  setEntries s;
  if (add && mapLinks[entry].parents.insert(parent).second) {
    cachedInnerUsage += memusage::IncrementalDynamicUsage(s);
  } else if (!add && mapLinks[entry].parents.erase(parent)) {
    cachedInnerUsage -= memusage::IncrementalDynamicUsage(s);
  }
}

const CTxMemPool::setEntries &
CTxMemPool::GetMemPoolParents(txiter entry) const {

  static const CTxMemPool::setEntries emptySet;

  if (entry == mapTx.end()) {
    LogPrint(
        BCLog::MEMPOOL,
        "GetMemPoolParents: called with mapTx.end(), returning empty set\n");
    return emptySet;
  }

  txlinksMap::const_iterator it = mapLinks.find(entry);
  if (it == mapLinks.end()) {

    LogPrint(
        BCLog::MEMPOOL,
        "GetMemPoolParents: no mapLinks entry for tx %s; returning empty set\n",
        entry->GetTx().GetHash().ToString());
    return emptySet;
  }

  return it->second.parents;
}

const CTxMemPool::setEntries &
CTxMemPool::GetMemPoolChildren(txiter entry) const {

  static const CTxMemPool::setEntries emptySet;

  if (entry == mapTx.end()) {
    LogPrint(
        BCLog::MEMPOOL,
        "GetMemPoolChildren: called with mapTx.end(), returning empty set\n");
    return emptySet;
  }

  txlinksMap::const_iterator it = mapLinks.find(entry);
  if (it == mapLinks.end()) {

    LogPrint(BCLog::MEMPOOL,
             "GetMemPoolChildren: no mapLinks entry for tx %s; returning empty "
             "set\n",
             entry->GetTx().GetHash().ToString());
    return emptySet;
  }

  return it->second.children;
}

CFeeRate CTxMemPool::GetMinFee(size_t sizelimit) const {
  LOCK(cs);
  if (!blockSinceLastRollingFeeBump || rollingMinimumFeeRate == 0)
    return CFeeRate(llround(rollingMinimumFeeRate));

  int64_t time = GetTime();
  if (time > lastRollingFeeUpdate + 10) {
    double halflife = ROLLING_FEE_HALFLIFE;
    if (DynamicMemoryUsage() < sizelimit / 4)
      halflife /= 4;
    else if (DynamicMemoryUsage() < sizelimit / 2)
      halflife /= 2;

    rollingMinimumFeeRate = rollingMinimumFeeRate /
                            pow(2.0, (time - lastRollingFeeUpdate) / halflife);
    lastRollingFeeUpdate = time;

    if (rollingMinimumFeeRate < (double)incrementalRelayFee.GetFeePerK() / 2) {
      rollingMinimumFeeRate = 0;
      return CFeeRate(0);
    }
  }
  return std::max(CFeeRate(llround(rollingMinimumFeeRate)),
                  incrementalRelayFee);
}

void CTxMemPool::trackPackageRemoved(const CFeeRate &rate) {
  AssertLockHeld(cs);

  const size_t RECENT_REMOVED_HISTORY = 16;
  recentRemovedRates.push_back(rate.GetFeePerK());
  if (recentRemovedRates.size() > RECENT_REMOVED_HISTORY) {
    recentRemovedRates.pop_front();
  }

  std::vector<double> copyRates(recentRemovedRates.begin(),
                                recentRemovedRates.end());
  std::sort(copyRates.begin(), copyRates.end());
  size_t idx = (copyRates.size() * 75) / 100;
  double percentile75 = copyRates.empty()
                            ? rate.GetFeePerK()
                            : copyRates[std::min(idx, copyRates.size() - 1)];

  const double MAX_BUMP_FACTOR = 2.0;
  double current = rollingMinimumFeeRate;
  double newRate = std::max(current, percentile75);
  if (current > 0 && newRate > current * MAX_BUMP_FACTOR) {
    newRate = current * MAX_BUMP_FACTOR;
    LogPrint(BCLog::MEMPOOL,
             "trackPackageRemoved: clamped fee bump to %g (from %g)\n", newRate,
             percentile75);
  }

  int removalEvents = (int)recentRemovedRates.size();
  if (removalEvents >= 2 && newRate > rollingMinimumFeeRate) {
    rollingMinimumFeeRate = newRate;
    blockSinceLastRollingFeeBump = false;
  }
}

void CTxMemPool::TrimToSize(size_t _limit, std::vector<COutPoint> *pvNoSpends) {
  LOCK(cs);

  if (DynamicMemoryUsage() <= _limit)
    return;

  std::vector<double> removedRates;
  size_t iterations = 0;
  const size_t MAX_ITERATIONS_PER_TRIM = 1000;

  while (DynamicMemoryUsage() > _limit &&
         iterations < MAX_ITERATIONS_PER_TRIM) {
    ++iterations;

    auto it = vTxHashes.rbegin();
    if (it == vTxHashes.rend())
      break;
    txiter worst = it->second;

    CFeeRate packageRate(worst->GetModFeesWithAncestors(),
                         worst->GetSizeWithAncestors());
    removedRates.push_back(packageRate.GetFeePerK());

    std::set<uint256> removed;
    removeRecursive(worst->GetTx(), MemPoolRemovalReason::SIZELIMIT);
    if (pvNoSpends) {
      for (const auto &txin : worst->GetTx().vin) {
        pvNoSpends->push_back(txin.prevout);
      }
    }
  }

  for (double r : removedRates) {
    trackPackageRemoved(CFeeRate((int64_t)r));
  }

  if (iterations >= MAX_ITERATIONS_PER_TRIM) {
    LogPrint(
        BCLog::MEMPOOL,
        "TrimToSize: reached iteration cap (%u) while trimming to %u bytes\n",
        (unsigned)iterations, (unsigned)_limit);
  }
}

bool CTxMemPool::TransactionWithinChainLimit(const uint256 &txid,
                                             size_t chainLimit) const {
  LOCK(cs);
  auto it = mapTx.find(txid);
  return it == mapTx.end() || (it->GetCountWithAncestors() < chainLimit &&
                               it->GetCountWithDescendants() < chainLimit);
}

SaltedTxidHasher::SaltedTxidHasher()
    : k0(GetRand(std::numeric_limits<uint64_t>::max())),
      k1(GetRand(std::numeric_limits<uint64_t>::max())) {}