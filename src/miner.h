// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include <primitives/block.h>
#include <txmempool.h>

#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <memory>
#include <stdint.h>

class CBlockIndex;
class CChainParams;
class CScript;

class arith_uint256;

struct CBeeRange;

namespace Consensus {
struct Params;
};

static const bool DEFAULT_PRINTPRIORITY = false;

static const int DEFAULT_HIVE_CHECK_DELAY = 1;
static const int DEFAULT_HIVE_THREADS = -2;
static const bool DEFAULT_HIVE_EARLY_OUT = true;

static const bool DEFAULT_HIVE_CONTRIB_CF = true;

struct CBlockTemplate {
  CBlock block;
  std::vector<CAmount> vTxFees;
  std::vector<int64_t> vTxSigOpsCost;
  std::vector<unsigned char> vchCoinbaseCommitment;
};

struct CTxMemPoolModifiedEntry {
  explicit CTxMemPoolModifiedEntry(CTxMemPool::txiter entry) {
    iter = entry;
    nSizeWithAncestors = entry->GetSizeWithAncestors();
    nModFeesWithAncestors = entry->GetModFeesWithAncestors();
    nSigOpCostWithAncestors = entry->GetSigOpCostWithAncestors();
  }

  int64_t GetModifiedFee() const { return iter->GetModifiedFee(); }
  uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
  CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
  size_t GetTxSize() const { return iter->GetTxSize(); }
  const CTransaction &GetTx() const { return iter->GetTx(); }

  CTxMemPool::txiter iter;
  uint64_t nSizeWithAncestors;
  CAmount nModFeesWithAncestors;
  int64_t nSigOpCostWithAncestors;
};

struct CompareCTxMemPoolIter {
  bool operator()(const CTxMemPool::txiter &a,
                  const CTxMemPool::txiter &b) const {
    return &(*a) < &(*b);
  }
};

struct modifiedentry_iter {
  typedef CTxMemPool::txiter result_type;
  result_type operator()(const CTxMemPoolModifiedEntry &entry) const {
    return entry.iter;
  }
};

struct CompareTxIterByAncestorCount {
  bool operator()(const CTxMemPool::txiter &a,
                  const CTxMemPool::txiter &b) const {
    if (a->GetCountWithAncestors() != b->GetCountWithAncestors())
      return a->GetCountWithAncestors() < b->GetCountWithAncestors();
    return CTxMemPool::CompareIteratorByHash()(a, b);
  }
};

typedef boost::multi_index_container<
    CTxMemPoolModifiedEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<modifiedentry_iter,
                                           CompareCTxMemPoolIter>,

        boost::multi_index::ordered_non_unique<

            boost::multi_index::tag<ancestor_score>,
            boost::multi_index::identity<CTxMemPoolModifiedEntry>,
            CompareTxMemPoolEntryByAncestorFee>>>
    indexed_modified_transaction_set;

typedef indexed_modified_transaction_set::nth_index<0>::type::iterator
    modtxiter;
typedef indexed_modified_transaction_set::index<ancestor_score>::type::iterator
    modtxscoreiter;

struct update_for_parent_inclusion {
  explicit update_for_parent_inclusion(CTxMemPool::txiter it) : iter(it) {}

  void operator()(CTxMemPoolModifiedEntry &e) {
    e.nModFeesWithAncestors -= iter->GetFee();
    e.nSizeWithAncestors -= iter->GetTxSize();
    e.nSigOpCostWithAncestors -= iter->GetSigOpCost();
  }

  CTxMemPool::txiter iter;
};

class BlockAssembler {
private:
  std::unique_ptr<CBlockTemplate> pblocktemplate;

  CBlock *pblock;

  bool fIncludeWitness;
  bool fIncludeBCTs;

  unsigned int nBlockMaxWeight;
  CFeeRate blockMinFeeRate;

  uint64_t nBlockWeight;
  uint64_t nBlockTx;
  uint64_t nBlockSigOpsCost;
  CAmount nFees;
  CTxMemPool::setEntries inBlock;

  int nHeight;
  int64_t nLockTimeCutoff;
  const CChainParams &chainparams;

public:
  struct Options {
    Options();
    size_t nBlockMaxWeight;
    CFeeRate blockMinFeeRate;
  };

  explicit BlockAssembler(const CChainParams &params);
  BlockAssembler(const CChainParams &params, const Options &options);

  std::unique_ptr<CBlockTemplate>
  CreateNewBlock(const CScript &scriptPubKeyIn, bool fMineWitnessTx = true,
                 const CScript *hiveProofScript = nullptr,
                 const POW_TYPE powType = POW_TYPE_SHA256);

private:
  void resetBlock();

  void AddToBlock(CTxMemPool::txiter iter);

  void addPackageTxs(int &nPackagesSelected, int &nDescendantsUpdated);

  void onlyUnconfirmed(CTxMemPool::setEntries &testSet);

  bool TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const;

  bool TestPackageTransactions(const CTxMemPool::setEntries &package);

  bool SkipMapTxEntry(CTxMemPool::txiter it,
                      indexed_modified_transaction_set &mapModifiedTx,
                      CTxMemPool::setEntries &failedTx);

  void SortForBlock(const CTxMemPool::setEntries &package,
                    CTxMemPool::txiter entry,
                    std::vector<CTxMemPool::txiter> &sortedEntries);

  int UpdatePackagesForAdded(const CTxMemPool::setEntries &alreadyAdded,
                             indexed_modified_transaction_set &mapModifiedTx);
};

void IncrementExtraNonce(CBlock *pblock, const CBlockIndex *pindexPrev,
                         unsigned int &nExtraNonce);
int64_t UpdateTime(CBlockHeader *pblock,
                   const Consensus::Params &consensusParams,
                   const CBlockIndex *pindexPrev);

void BeeKeeper(const CChainParams &chainparams);

bool BusyBees(const Consensus::Params &consensusParams, int height);

void CheckBin(int threadID, std::vector<CBeeRange> bin,
              std::string deterministicRandString, arith_uint256 beeHashTarget);

void CheckBinMinotaurX(int threadID, std::vector<CBeeRange> bin,
                       std::string deterministicRandString,
                       arith_uint256 beeHashTarget);

void AbortWatchThread(int height);

#endif
