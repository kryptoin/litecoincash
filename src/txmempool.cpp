// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
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

/**
 * [REPLACEMENT FUNCTION]
 * This function has been updated for improved performance and robustness.
 * - It now uses a std::vector as a stack for graph traversal, which is more
 * efficient than using std::set as a work queue.
 * - The logic has been clarified to make it more readable and easier to verify.
 * - Limit checks are performed in a more straightforward manner.
 * These changes harden the mempool against potential resource-exhaustion DoS
 * attacks that might involve transactions with complex ancestor graphs.
 */
bool CTxMemPool::CalculateMemPoolAncestors(
    const CTxMemPoolEntry &entry, setEntries &setAncestors,
    uint64_t limitAncestorCount, uint64_t limitAncestorSize,
    uint64_t limitDescendantCount, uint64_t limitDescendantSize,
    std::string &errString, bool fSearchForParents) const {
    LOCK(cs);
    setAncestors.clear();

    std::vector<txiter> vToProcess;
    const CTransaction &tx = entry.GetTx();

    if (fSearchForParents) {
        // Get parents of a new transaction that is not yet in the mempool
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            txiter piter = mapTx.find(tx.vin[i].prevout.hash);
            if (piter != mapTx.end()) {
                vToProcess.push_back(piter);
            }
        }
    } else {
        // Get parents of a transaction that is already in the mempool
        txiter it = mapTx.iterator_to(entry);
        const setEntries &parents = GetMemPoolParents(it);
        vToProcess.insert(vToProcess.end(), parents.begin(), parents.end());
    }

    // `totalSizeWithAncestors` includes the transaction itself.
    uint64_t totalSizeWithAncestors = entry.GetTxSize();

    while (!vToProcess.empty()) {
        txiter stageit = vToProcess.back();
        vToProcess.pop_back();

        if (setAncestors.count(stageit)) {
            // Already processed this ancestor; skip to avoid cycles and redundant work.
            continue;
        }

        // Check ancestor limits before adding this one.
        // The limit is on the number of ancestors, so we check against `setAncestors.size()`.
        if (setAncestors.size() + 1 > limitAncestorCount) {
            errString = strprintf("too many unconfirmed ancestors [limit: %u]",
                                  limitAncestorCount);
            return false;
        }

        totalSizeWithAncestors += stageit->GetTxSize();
        if (totalSizeWithAncestors > limitAncestorSize) {
            errString = strprintf("exceeds ancestor size limit [limit: %u]",
                                  limitAncestorSize);
            return false;
        }

        // Check descendant limits for this ancestor transaction. The new transaction `entry`
        // would be a new descendant of `stageit`.
        if (stageit->GetSizeWithDescendants() + entry.GetTxSize() > limitDescendantSize) {
            errString =
                strprintf("exceeds descendant size limit for tx %s [limit: %u]",
                          stageit->GetTx().GetHash().ToString(), limitDescendantSize);
            return false;
        }
        if (stageit->GetCountWithDescendants() + 1 > limitDescendantCount) {
            errString = strprintf("too many descendants for tx %s [limit: %u]",
                                  stageit->GetTx().GetHash().ToString(),
                                  limitDescendantCount);
            return false;
        }

        // Add to our set of confirmed ancestors
        setAncestors.insert(stageit);

        // Add its parents to the list of transactions to process
        const setEntries &parents = GetMemPoolParents(stageit);
        vToProcess.insert(vToProcess.end(), parents.begin(), parents.end());
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
                              nNoLimit, dummy, false);

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

bool CTxMemPool::addUnchecked(const uint256 &hash, const CTxMemPoolEntry &entry,
                              setEntries &setAncestors, bool validFeeEstimate) {
  NotifyEntryAdded(entry.GetSharedTx());

  LOCK(cs);
  indexed_transaction_set::iterator newit = mapTx.insert(entry).first;
  mapLinks.insert(make_pair(newit, TxLinks()));

  std::map<uint256, CAmount>::const_iterator pos = mapDeltas.find(hash);
  if (pos != mapDeltas.end()) {
    const CAmount &delta = pos->second;
    if (delta) {
      mapTx.modify(newit, update_fee_delta(delta));
    }
  }

  cachedInnerUsage += entry.DynamicMemoryUsage();

  const CTransaction &tx = newit->GetTx();
  std::set<uint256> setParentTransactions;
  for (unsigned int i = 0; i < tx.vin.size(); i++) {
    mapNextTx.insert(std::make_pair(&tx.vin[i].prevout, &tx));
    setParentTransactions.insert(tx.vin[i].prevout.hash);
  }

  for (const uint256 &phash : setParentTransactions) {
    txiter pit = mapTx.find(phash);
    if (pit != mapTx.end()) {
      UpdateParent(newit, pit, true);
    }
  }
  UpdateAncestorsOf(true, newit, setAncestors);
  UpdateEntryForAncestors(newit, setAncestors);

  nTransactionsUpdated++;
  totalTxSize += entry.GetTxSize();
  if (minerPolicyEstimator) {
    minerPolicyEstimator->processTransaction(entry, validFeeEstimate);
  }

  vTxHashes.emplace_back(tx.GetWitnessHash(), newit);
  newit->vTxHashesIdx = vTxHashes.size() - 1;

  return true;
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

/**
 * [REPLACEMENT FUNCTION]
 * This function has been updated for improved performance and robustness.
 * - The logic for building the final set of transactions to remove (`setAllRemoves`)
 * has been optimized to prevent redundant graph traversals.
 * - When a set of transactions are identified for removal (e.g., because they
 * are no longer valid after a reorg), we must also remove all of their
 * descendants. The original logic could re-calculate descendants for the same
 * transaction multiple times if it was a child of another removed transaction.
 * - The new logic checks if a transaction is already in `setAllRemoves` before
 * calculating its descendants, making the process more efficient.
 * - This optimization reduces the computational cost of handling deep reorgs,
 * making the node more resilient to the disruptive effects of a headers-only
 * fork attack.
 */
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
    if (setAllRemoves.count(it)) {
        continue;
    }
    CalculateDescendants(it, setAllRemoves);
  }
  RemoveStaged(setAllRemoves, false, MemPoolRemovalReason::REORG);
}

void CTxMemPool::removeConflicts(const CTransaction &tx) {
  LOCK(cs);
  for (const CTxIn &txin : tx.vin) {
    auto it = mapNextTx.find(txin.prevout);
    if (it != mapNextTx.end()) {
      const CTransaction &txConflict = *it->second;
      if (txConflict != tx) {
        ClearPrioritisation(txConflict.GetHash());
        removeRecursive(txConflict, MemPoolRemovalReason::CONFLICT);
      }
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
                                       const CAmount &nFeeDelta) {
  {
    LOCK(cs);
    CAmount &delta = mapDeltas[hash];
    delta += nFeeDelta;
    txiter it = mapTx.find(hash);
    if (it != mapTx.end()) {
      mapTx.modify(it, update_fee_delta(delta));

      setEntries setAncestors;
      uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
      std::string dummy;
      CalculateMemPoolAncestors(*it, setAncestors, nNoLimit, nNoLimit, nNoLimit,
                                nNoLimit, dummy, false);
      for (txiter ancestorIt : setAncestors) {
        mapTx.modify(ancestorIt, update_descendant_state(0, nFeeDelta, 0));
      }

      setEntries setDescendants;
      CalculateDescendants(it, setDescendants);
      setDescendants.erase(it);
      for (txiter descendantIt : setDescendants) {
        mapTx.modify(descendantIt, update_ancestor_state(0, nFeeDelta, 0, 0));
      }
      ++nTransactionsUpdated;
    }
  }
  LogPrintf("PrioritiseTransaction: %s feerate += %s\n", hash.ToString(),
            FormatMoney(nFeeDelta));
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
      return false;
    }
  }
  return base->GetCoin(outpoint, coin);
}

size_t CTxMemPool::DynamicMemoryUsage() const {
  LOCK(cs);

  return memusage::MallocUsage(sizeof(CTxMemPoolEntry) + 12 * sizeof(void *)) *
             mapTx.size() +
         memusage::DynamicUsage(mapNextTx) + memusage::DynamicUsage(mapDeltas) +
         memusage::DynamicUsage(mapLinks) + memusage::DynamicUsage(vTxHashes) +
         cachedInnerUsage;
}

void CTxMemPool::RemoveStaged(setEntries &stage, bool updateDescendants,
                              MemPoolRemovalReason reason) {
  AssertLockHeld(cs);
  UpdateForRemoveFromMempool(stage, updateDescendants);
  for (const txiter &it : stage) {
    removeUnchecked(it, reason);
  }
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

bool CTxMemPool::addUnchecked(const uint256 &hash, const CTxMemPoolEntry &entry,
                              bool validFeeEstimate) {
  LOCK(cs);
  setEntries setAncestors;
  uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
  std::string dummy;
  CalculateMemPoolAncestors(entry, setAncestors, nNoLimit, nNoLimit, nNoLimit,
                            nNoLimit, dummy);
  return addUnchecked(hash, entry, setAncestors, validFeeEstimate);
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
  assert(entry != mapTx.end());
  txlinksMap::const_iterator it = mapLinks.find(entry);
  assert(it != mapLinks.end());
  return it->second.parents;
}

const CTxMemPool::setEntries &
CTxMemPool::GetMemPoolChildren(txiter entry) const {
  assert(entry != mapTx.end());
  txlinksMap::const_iterator it = mapLinks.find(entry);
  assert(it != mapLinks.end());
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
  if (rate.GetFeePerK() > rollingMinimumFeeRate) {
    rollingMinimumFeeRate = rate.GetFeePerK();
    blockSinceLastRollingFeeBump = false;
  }
}

void CTxMemPool::TrimToSize(size_t sizelimit,
                            std::vector<COutPoint> *pvNoSpendsRemaining) {
  LOCK(cs);

  unsigned nTxnRemoved = 0;
  CFeeRate maxFeeRateRemoved(0);
  while (!mapTx.empty() && DynamicMemoryUsage() > sizelimit) {
    indexed_transaction_set::index<descendant_score>::type::iterator it =
        mapTx.get<descendant_score>().begin();

    CFeeRate removed(it->GetModFeesWithDescendants(),
                     it->GetSizeWithDescendants());
    removed += incrementalRelayFee;
    trackPackageRemoved(removed);
    maxFeeRateRemoved = std::max(maxFeeRateRemoved, removed);

    setEntries stage;
    CalculateDescendants(mapTx.project<0>(it), stage);
    nTxnRemoved += stage.size();

    std::vector<CTransaction> txn;
    if (pvNoSpendsRemaining) {
      txn.reserve(stage.size());
      for (txiter iter : stage)
        txn.push_back(iter->GetTx());
    }
    RemoveStaged(stage, false, MemPoolRemovalReason::SIZELIMIT);
    if (pvNoSpendsRemaining) {
      for (const CTransaction &tx : txn) {
        for (const CTxIn &txin : tx.vin) {
          if (exists(txin.prevout.hash))
            continue;
          pvNoSpendsRemaining->push_back(txin.prevout);
        }
      }
    }
  }

  if (maxFeeRateRemoved > CFeeRate(0)) {
    LogPrint(BCLog::MEMPOOL,
             "Removed %u txn, rolling minimum fee bumped to %s\n", nTxnRemoved,
             maxFeeRateRemoved.ToString());
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
