// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2025 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXMEMPOOL_H
#define BITCOIN_TXMEMPOOL_H

#include <amount.h>
#include <coins.h>
#include <indirectmap.h>
#include <map>
#include <memory>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <random.h>
#include <set>
#include <string>
#include <sync.h>
#include <utility>
#include <vector>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/signals2/signal.hpp>

class CBlockIndex;

static const uint32_t MEMPOOL_HEIGHT = 0x7FFFFFFF;

struct LockPoints {

  int height;
  int64_t time;

  CBlockIndex *maxInputBlock;

  LockPoints() : height(0), time(0), maxInputBlock(nullptr) {}
};

class CTxMemPool;

class CTxMemPoolEntry {
private:
  bool spendsCoinbase;
  CAmount nFee;
  CAmount nModFeesWithAncestors;
  CAmount nModFeesWithDescendants;
  CTransactionRef tx;

  int64_t feeDelta;
  int64_t nSigOpCostWithAncestors;
  int64_t nTime;
  int64_t sigOpCost;

  LockPoints lockPoints;

  size_t nTxWeight;
  size_t nUsageSize;

  uint64_t nCountWithAncestors;
  uint64_t nCountWithDescendants;
  uint64_t nSizeWithAncestors;
  uint64_t nSizeWithDescendants;

  unsigned int entryHeight;

public:
  CTxMemPoolEntry(const CTransactionRef &_tx, const CAmount &_nFee,
                  int64_t _nTime, unsigned int _entryHeight,
                  bool spendsCoinbase, int64_t nSigOpsCost, LockPoints lp);

  bool GetSpendsCoinbase() const { return spendsCoinbase; }

  CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
  CAmount GetModFeesWithDescendants() const { return nModFeesWithDescendants; }

  const CAmount &GetFee() const { return nFee; }
  const CTransaction &GetTx() const { return *this->tx; }
  const LockPoints &GetLockPoints() const { return lockPoints; }

  CTransactionRef GetSharedTx() const { return this->tx; }

  int64_t GetModifiedFee() const { return nFee + feeDelta; }
  int64_t GetSigOpCost() const { return sigOpCost; }
  int64_t GetSigOpCostWithAncestors() const { return nSigOpCostWithAncestors; }
  int64_t GetTime() const { return nTime; }

  mutable size_t vTxHashesIdx;

  size_t DynamicMemoryUsage() const { return nUsageSize; }
  size_t GetTxSize() const;
  size_t GetTxWeight() const { return nTxWeight; }

  uint64_t GetCountWithAncestors() const { return nCountWithAncestors; }
  uint64_t GetCountWithDescendants() const { return nCountWithDescendants; }
  uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
  uint64_t GetSizeWithDescendants() const { return nSizeWithDescendants; }

  unsigned int GetHeight() const { return entryHeight; }

  void UpdateAncestorState(int64_t modifySize, CAmount modifyFee,
                           int64_t modifyCount, int64_t modifySigOps);
  void UpdateDescendantState(int64_t modifySize, CAmount modifyFee,
                             int64_t modifyCount);
  void UpdateFeeDelta(int64_t feeDelta);
  void UpdateLockPoints(const LockPoints &lp);
};

struct update_descendant_state {
  update_descendant_state(int64_t _modifySize, CAmount _modifyFee,
                          int64_t _modifyCount)
      : modifySize(_modifySize), modifyFee(_modifyFee),
        modifyCount(_modifyCount) {}

  void operator()(CTxMemPoolEntry &e) {
    e.UpdateDescendantState(modifySize, modifyFee, modifyCount);
  }

private:
  int64_t modifySize;
  CAmount modifyFee;
  int64_t modifyCount;
};

struct update_ancestor_state {
  update_ancestor_state(int64_t _modifySize, CAmount _modifyFee,
                        int64_t _modifyCount, int64_t _modifySigOpsCost)
      : modifySize(_modifySize), modifyFee(_modifyFee),
        modifyCount(_modifyCount), modifySigOpsCost(_modifySigOpsCost) {}

  void operator()(CTxMemPoolEntry &e) {
    e.UpdateAncestorState(modifySize, modifyFee, modifyCount, modifySigOpsCost);
  }

private:
  int64_t modifySize;
  CAmount modifyFee;
  int64_t modifyCount;
  int64_t modifySigOpsCost;
};

struct update_fee_delta {
  explicit update_fee_delta(int64_t _feeDelta) : feeDelta(_feeDelta) {}

  void operator()(CTxMemPoolEntry &e) { e.UpdateFeeDelta(feeDelta); }

private:
  int64_t feeDelta;
};

struct update_lock_points {
  explicit update_lock_points(const LockPoints &_lp) : lp(_lp) {}

  void operator()(CTxMemPoolEntry &e) { e.UpdateLockPoints(lp); }

private:
  const LockPoints &lp;
};

struct mempoolentry_txid {
  typedef uint256 result_type;
  result_type operator()(const CTxMemPoolEntry &entry) const {
    return entry.GetTx().GetHash();
  }

  result_type operator()(const CTransactionRef &tx) const {
    return tx->GetHash();
  }
};

class CompareTxMemPoolEntryByDescendantScore {
public:
  bool operator()(const CTxMemPoolEntry &a, const CTxMemPoolEntry &b) const {
    double a_mod_fee, a_size, b_mod_fee, b_size;

    GetModFeeAndSize(a, a_mod_fee, a_size);
    GetModFeeAndSize(b, b_mod_fee, b_size);

    double f1 = a_mod_fee * b_size;
    double f2 = a_size * b_mod_fee;

    if (f1 == f2) {
      return a.GetTime() >= b.GetTime();
    }
    return f1 < f2;
  }

  void GetModFeeAndSize(const CTxMemPoolEntry &a, double &mod_fee,
                        double &size) const {

    double f1 = (double)a.GetModifiedFee() * a.GetSizeWithDescendants();
    double f2 = (double)a.GetModFeesWithDescendants() * a.GetTxSize();

    if (f2 > f1) {
      mod_fee = a.GetModFeesWithDescendants();
      size = a.GetSizeWithDescendants();
    } else {
      mod_fee = a.GetModifiedFee();
      size = a.GetTxSize();
    }
  }
};

class CompareTxMemPoolEntryByScore {
public:
  bool operator()(const CTxMemPoolEntry &a, const CTxMemPoolEntry &b) const {
    double f1 = (double)a.GetModifiedFee() * b.GetTxSize();
    double f2 = (double)b.GetModifiedFee() * a.GetTxSize();
    if (f1 == f2) {
      return b.GetTx().GetHash() < a.GetTx().GetHash();
    }
    return f1 > f2;
  }
};

class CompareTxMemPoolEntryByEntryTime {
public:
  bool operator()(const CTxMemPoolEntry &a, const CTxMemPoolEntry &b) const {
    return a.GetTime() < b.GetTime();
  }
};

class CompareTxMemPoolEntryByAncestorFee {
public:
  template <typename T> bool operator()(const T &a, const T &b) const {
    double a_mod_fee, a_size, b_mod_fee, b_size;

    GetModFeeAndSize(a, a_mod_fee, a_size);
    GetModFeeAndSize(b, b_mod_fee, b_size);

    double f1 = a_mod_fee * b_size;
    double f2 = a_size * b_mod_fee;

    if (f1 == f2) {
      return a.GetTx().GetHash() < b.GetTx().GetHash();
    }
    return f1 > f2;
  }

  template <typename T>
  void GetModFeeAndSize(const T &a, double &mod_fee, double &size) const {

    double f1 = (double)a.GetModifiedFee() * a.GetSizeWithAncestors();
    double f2 = (double)a.GetModFeesWithAncestors() * a.GetTxSize();

    if (f1 > f2) {
      mod_fee = a.GetModFeesWithAncestors();
      size = a.GetSizeWithAncestors();
    } else {
      mod_fee = a.GetModifiedFee();
      size = a.GetTxSize();
    }
  }
};

struct descendant_score {};
struct entry_time {};
struct ancestor_score {};

class CBlockPolicyEstimator;

struct TxMempoolInfo {

  CTransactionRef tx;

  int64_t nTime;

  CFeeRate feeRate;

  int64_t nFeeDelta;
};

enum class MemPoolRemovalReason {
  UNKNOWN = 0,
  EXPIRY,
  SIZELIMIT,
  REORG,
  BLOCK,
  CONFLICT,
  REPLACED
};

class SaltedTxidHasher {
private:
  const uint64_t k0, k1;

public:
  SaltedTxidHasher();

  size_t operator()(const uint256 &txid) const {
    return SipHashUint256(k0, k1, txid);
  }
};

class CTxMemPool {
private:
  uint32_t nCheckFrequency;

  unsigned int nTransactionsUpdated;

  CBlockPolicyEstimator *minerPolicyEstimator;

  uint64_t totalTxSize;

  uint64_t cachedInnerUsage;

  mutable int64_t lastRollingFeeUpdate;
  mutable bool blockSinceLastRollingFeeBump;
  mutable double rollingMinimumFeeRate;
  std::deque<double> recentRemovedRates;

  void trackPackageRemoved(const CFeeRate &rate);

public:
  static const int ROLLING_FEE_HALFLIFE = 60 * 60 * 12;

  typedef boost::multi_index_container<
      CTxMemPoolEntry, boost::multi_index::indexed_by<

                           boost::multi_index::hashed_unique<mempoolentry_txid,
                                                             SaltedTxidHasher>,

                           boost::multi_index::ordered_non_unique<
                               boost::multi_index::tag<descendant_score>,
                               boost::multi_index::identity<CTxMemPoolEntry>,
                               CompareTxMemPoolEntryByDescendantScore>,

                           boost::multi_index::ordered_non_unique<
                               boost::multi_index::tag<entry_time>,
                               boost::multi_index::identity<CTxMemPoolEntry>,
                               CompareTxMemPoolEntryByEntryTime>,

                           boost::multi_index::ordered_non_unique<
                               boost::multi_index::tag<ancestor_score>,
                               boost::multi_index::identity<CTxMemPoolEntry>,
                               CompareTxMemPoolEntryByAncestorFee>>>
      indexed_transaction_set;

  mutable CCriticalSection cs;
  indexed_transaction_set mapTx;

  typedef indexed_transaction_set::nth_index<0>::type::iterator txiter;
  std::vector<std::pair<uint256, txiter>> vTxHashes;

  struct CompareIteratorByHash {
    bool operator()(const txiter &a, const txiter &b) const {
      return a->GetTx().GetHash() < b->GetTx().GetHash();
    }
  };
  typedef std::set<txiter, CompareIteratorByHash> setEntries;

  const setEntries &GetMemPoolParents(txiter entry) const;
  const setEntries &GetMemPoolChildren(txiter entry) const;

private:
  typedef std::map<txiter, setEntries, CompareIteratorByHash> cacheMap;

  struct TxLinks {
    setEntries parents;
    setEntries children;
  };

  typedef std::map<txiter, TxLinks, CompareIteratorByHash> txlinksMap;
  txlinksMap mapLinks;

  void UpdateParent(txiter entry, txiter parent, bool add);
  void UpdateChild(txiter entry, txiter child, bool add);

  std::vector<indexed_transaction_set::const_iterator>
  GetSortedDepthAndScore() const;

public:
  void addUnchecked(const uint256 &hash, CTxMemPoolEntry &entry,
                    bool validFeeEstimate = true);

  bool CompareDepthAndScore(const uint256 &hasha, const uint256 &hashb);
  bool HasNoInputsOf(const CTransaction &tx) const;
  bool isSpent(const COutPoint &outpoint);

  explicit CTxMemPool(CBlockPolicyEstimator *estimator = nullptr);

  indirectmap<COutPoint, const CTransaction *> mapNextTx;
  std::map<uint256, CAmount> mapDeltas;
  std::map<uint256, size_t> mapSourceCounts;
  std::map<uint256, int64_t> mapDeltasTime;
  std::map<uint256, int64_t> mapLastPrioritiseRecalc;

  unsigned int GetTransactionsUpdated() const;

  void _clear();
  void AddTransactionsUpdated(unsigned int n);
  void ApplyDelta(const uint256 hash, CAmount &nFeeDelta) const;
  void check(const CCoinsViewCache *pcoins) const;
  void clear();
  void ClearPrioritisation(const uint256 hash);
  void PrioritiseTransaction(const uint256 &hash, const CAmount nFeeDelta);
  void queryHashes(std::vector<uint256> &vtxid);
  void removeConflicts(const CTransaction &tx);
  void removeForBlock(const std::vector<CTransactionRef> &vtx,
                      unsigned int nBlockHeight);
  void removeForReorg(const CCoinsViewCache *pcoins,
                      unsigned int nMemPoolHeight, int flags);
  void
  removeRecursive(const CTransaction &tx,
                  MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);
  void setSanityCheck(double dFrequency = 1.0) {
    nCheckFrequency = static_cast<uint32_t>(dFrequency * 4294967295.0);
  }

public:
  bool CalculateMemPoolAncestors(const CTxMemPoolEntry &entry,
                                 setEntries &setAncestors,
                                 size_t limit_ancestors,
                                 size_t limit_descendants,
                                 size_t limit_ancestor_size,
                                 size_t limit_descendant_size,
                                 std::string &errString) const;
  bool TransactionWithinChainLimit(const uint256 &txid,
                                   size_t chainLimit) const;

  CFeeRate GetMinFee(size_t sizelimit) const;

  int Expire(int64_t time);

  void CalculateDescendants(txiter it, setEntries &setDescendants);
  void
  RemoveStaged(setEntries &stage, bool updateDescendants,
               MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);
  void TrimToSize(size_t sizelimit,
                  std::vector<COutPoint> *pvNoSpendsRemaining = nullptr);
  void UpdateTransactionsFromBlock(const std::vector<uint256> &vHashesToUpdate);

  unsigned long size() {
    LOCK(cs);
    return mapTx.size();
  }

  uint64_t GetTotalTxSize() const {
    LOCK(cs);
    return totalTxSize;
  }

  bool exists(uint256 hash) const {
    LOCK(cs);
    return (mapTx.count(hash) != 0);
  }

  CTransactionRef get(const uint256 &hash) const;
  TxMempoolInfo info(const uint256 &hash) const;
  std::vector<TxMempoolInfo> infoAll() const;

  size_t DynamicMemoryUsage() const;

  boost::signals2::signal<void(txiter, bool)> NotifyEntryAdded;
  boost::signals2::signal<void(CTransactionRef, MemPoolRemovalReason)>
      NotifyEntryRemoved;

private:
  void UpdateForDescendants(txiter updateIt, cacheMap &cachedDescendants,
                            const std::set<uint256> &setExclude);

  void
  removeUnchecked(txiter entry,
                  MemPoolRemovalReason reason = MemPoolRemovalReason::UNKNOWN);

  void UpdateAncestorsOf(bool add, txiter hash, setEntries &setAncestors);
  void UpdateChildrenForRemoval(txiter entry);
  void UpdateEntryForAncestors(txiter it, const setEntries &setAncestors);
  void UpdateForRemoveFromMempool(const setEntries &entriesToRemove,
                                  bool updateDescendants);
};

class CCoinsViewMemPool : public CCoinsViewBacked {
protected:
  const CTxMemPool &mempool;

public:
  CCoinsViewMemPool(CCoinsView *baseIn, const CTxMemPool &mempoolIn);
  bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
};

struct txid_index {};
struct insertion_order {};

struct DisconnectedBlockTransactions {
  typedef boost::multi_index_container<
      CTransactionRef, boost::multi_index::indexed_by<

                           boost::multi_index::hashed_unique<
                               boost::multi_index::tag<txid_index>,
                               mempoolentry_txid, SaltedTxidHasher>,

                           boost::multi_index::sequenced<
                               boost::multi_index::tag<insertion_order>>>>
      indexed_disconnected_transactions;

  ~DisconnectedBlockTransactions() { assert(queuedTx.empty()); }

  indexed_disconnected_transactions queuedTx;
  uint64_t cachedInnerUsage = 0;

  size_t DynamicMemoryUsage() const {
    return memusage::MallocUsage(sizeof(CTransactionRef) + 6 * sizeof(void *)) *
               queuedTx.size() +
           cachedInnerUsage;
  }

  void addTransaction(const CTransactionRef &tx) {
    queuedTx.insert(tx);
    cachedInnerUsage += RecursiveDynamicUsage(tx);
  }

  void removeForBlock(const std::vector<CTransactionRef> &vtx) {
    if (queuedTx.empty()) {
      return;
    }
    for (auto const &tx : vtx) {
      auto it = queuedTx.find(tx->GetHash());
      if (it != queuedTx.end()) {
        cachedInnerUsage -= RecursiveDynamicUsage(*it);
        queuedTx.erase(it);
      }
    }
  }

  void removeEntry(
      indexed_disconnected_transactions::index<insertion_order>::type::iterator
          entry) {
    cachedInnerUsage -= RecursiveDynamicUsage(*entry);
    queuedTx.get<insertion_order>().erase(entry);
  }

  void clear() {
    cachedInnerUsage = 0;
    queuedTx.clear();
  }
};

#endif