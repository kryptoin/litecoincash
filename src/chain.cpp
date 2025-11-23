// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>

#include <util.h>

#include <rpc/blockchain.h>

#include <validation.h>

#include <consensus/consensus.h>

void CChain::SetTip(CBlockIndex *pindex) {
  if (pindex == nullptr) {
    vChain.clear();
    return;
  }
  vChain.resize(pindex->nHeight + 1);
  while (pindex && vChain[pindex->nHeight] != pindex) {
    vChain[pindex->nHeight] = pindex;
    pindex = pindex->pprev;
  }
}

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex) const {
  int nStep = 1;
  std::vector<uint256> vHave;
  vHave.reserve(32);

  if (!pindex) {
    pindex = Tip();
  }

  while (pindex) {
    vHave.push_back(pindex->GetBlockHash());
    if (pindex->nHeight == 0) {
      break;
    }
    int nHeight = std::max(pindex->nHeight - nStep, 0);

    pindex = (*this)[nHeight];

    if (vHave.size() > 10) {
      nStep *= 2;
    }
  }

  return CBlockLocator(vHave);
}

const CBlockIndex *CChain::FindFork(const CBlockIndex *pindex) const {
  if (pindex == nullptr) {
    return nullptr;
  }
  if (pindex->nHeight > Height())
    pindex = pindex->GetAncestor(Height());
  
  // Introspection hardening: Limit backward traversal depth to prevent
  // attackers from mapping deep forks. This is a soft limit - legitimate
  // use cases in validation will still work, but external queries are bounded.
  int nTraversalLimit = gArgs.GetArg("-maxforktraversal", DEFAULT_MAX_FORK_TRAVERSAL);
  int nTraversed = 0;
  
  while (pindex && !Contains(pindex)) {
    pindex = pindex->pprev;
    nTraversed++;
    
    // Prevent excessive backward traversal for introspection attacks
    if (nTraversed > nTraversalLimit) {
      LogPrint(BCLog::NET, "FindFork: Excessive traversal limit reached (%d blocks), "
               "returning nullptr (introspection hardening)\n", nTraversed);
      return nullptr;
    }
  }
  return pindex;
}

CBlockIndex *CChain::FindEarliestAtLeast(int64_t nTime) const {
  std::vector<CBlockIndex *>::const_iterator lower =
      std::lower_bound(vChain.begin(), vChain.end(), nTime,
                       [](CBlockIndex *pBlock, const int64_t &time) -> bool {
                         return pBlock->GetBlockTimeMax() < time;
                       });
  return (lower == vChain.end() ? nullptr : *lower);
}

int static inline InvertLowestOne(int n) { return n & (n - 1); }

int static inline GetSkipHeight(int height) {
  if (height < 2)
    return 0;

  return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1
                      : InvertLowestOne(height);
}

const CBlockIndex *CBlockIndex::GetAncestor(int height) const {
  if (height > nHeight || height < 0) {
    return nullptr;
  }

  const CBlockIndex *pindexWalk = this;
  int heightWalk = nHeight;
  while (heightWalk > height) {
    int heightSkip = GetSkipHeight(heightWalk);
    int heightSkipPrev = GetSkipHeight(heightWalk - 1);
    if (pindexWalk->pskip != nullptr &&
        (heightSkip == height ||
         (heightSkip > height &&
          !(heightSkipPrev < heightSkip - 2 && heightSkipPrev >= height)))) {
      pindexWalk = pindexWalk->pskip;
      heightWalk = heightSkip;
    } else {
      assert(pindexWalk->pprev);
      pindexWalk = pindexWalk->pprev;
      heightWalk--;
    }
  }
  return pindexWalk;
}

CBlockIndex *CBlockIndex::GetAncestor(int height) {
  return const_cast<CBlockIndex *>(
      static_cast<const CBlockIndex *>(this)->GetAncestor(height));
}

void CBlockIndex::BuildSkip() {
  if (pprev)
    pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

arith_uint256 GetBlockProof(const CBlockIndex &block) {
  const Consensus::Params &consensusParams = Params().GetConsensus();
  bool verbose = false;

  arith_uint256 bnTarget;
  bool fNegative;
  bool fOverflow;

  bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
  if (fNegative || fOverflow || bnTarget == 0)
    return 0;

  arith_uint256 bnTargetScaled = (~bnTarget / (bnTarget + 1)) + 1;

  if (block.GetBlockHeader().IsHiveMined(consensusParams)) {
    assert(block.pprev);

    CBlockIndex *pindexTemp = block.pprev;
    while (pindexTemp->GetBlockHeader().IsHiveMined(consensusParams)) {
      assert(pindexTemp->pprev);
      pindexTemp = pindexTemp->pprev;
    }

    arith_uint256 bnPreviousTarget;
    bnPreviousTarget.SetCompact(pindexTemp->nBits, &fNegative, &fOverflow);

    if (fNegative || fOverflow || bnPreviousTarget == 0)
      return 0;
    bnTargetScaled += (~bnPreviousTarget / (bnPreviousTarget + 1)) + 1;

    if (IsHive11Enabled(&block, consensusParams)) {
      if (verbose) {
        LogPrintf("**** HIVE-1.1: ENABLING BONUS CHAINWORK ON HIVE BLOCK %s\n",
                  block.GetBlockHash().ToString());
        LogPrintf("**** Initial block chainwork = %s\n",
                  bnTargetScaled.ToString());
      }
      double hiveDiff = GetDifficulty(&block, true);

      if (verbose)
        LogPrintf("**** Hive diff = %.12f\n", hiveDiff);
      unsigned int k =
          floor(std::min(hiveDiff / consensusParams.maxHiveDiff, 1.0) *
                    (consensusParams.maxK - consensusParams.minK) +
                consensusParams.minK);

      bnTargetScaled *= k;

      if (verbose) {
        LogPrintf("**** k = %d\n", k);
        LogPrintf("**** Final scaled chainwork =  %s\n",
                  bnTargetScaled.ToString());
      }
    }

  } else if (IsHive11Enabled(&block, consensusParams)) {
    if (verbose) {
      LogPrintf("**** HIVE-1.1: CHECKING FOR BONUS CHAINWORK ON POW BLOCK %s\n",
                block.GetBlockHash().ToString());
      LogPrintf("**** Initial block chainwork = %s\n",
                bnTargetScaled.ToString());
    }

    CBlockIndex *currBlock = block.pprev;
    int blocksSinceHive;
    double lastHiveDifficulty = 0;

    for (blocksSinceHive = 0; blocksSinceHive < consensusParams.maxKPow;
         blocksSinceHive++) {
      if (currBlock->GetBlockHeader().IsHiveMined(consensusParams)) {
        lastHiveDifficulty = GetDifficulty(currBlock, true);
        if (verbose)
          LogPrintf("**** Got last Hive diff = %.12f, at %s\n",
                    lastHiveDifficulty, currBlock->GetBlockHash().ToString());
        break;
      }

      assert(currBlock->pprev);
      currBlock = currBlock->pprev;
    }

    if (verbose)
      LogPrintf("**** Pow blocks since last Hive block = %d\n",
                blocksSinceHive);

    unsigned int k = consensusParams.maxKPow - blocksSinceHive;
    if (lastHiveDifficulty < consensusParams.powSplit1)
      k = k >> 1;
    if (lastHiveDifficulty < consensusParams.powSplit2)
      k = k >> 1;

    if (k < 1)
      k = 1;

    bnTargetScaled *= k;

    if (verbose) {
      LogPrintf("**** k = %d\n", k);
      LogPrintf("**** Final scaled chainwork =  %s\n",
                bnTargetScaled.ToString());
    }
  }

  return bnTargetScaled;
}

arith_uint256 GetNumHashes(const CBlockIndex &block, POW_TYPE powType) {
  arith_uint256 bnTarget;
  bool fNegative;
  bool fOverflow;

  bnTarget.SetCompact(block.nBits, &fNegative, &fOverflow);
  if (fNegative || fOverflow || bnTarget == 0 ||
      block.GetBlockHeader().IsHiveMined(Params().GetConsensus()))
    return 0;

  if (IsMinotaurXEnabled(&block, Params().GetConsensus()) &&
      block.GetBlockHeader().GetPoWType() != powType)
    return 0;

  if (!IsMinotaurXEnabled(&block, Params().GetConsensus()) &&
      powType == POW_TYPE_MINOTAURX)
    return 0;

  return (~bnTarget / (bnTarget + 1)) + 1;
}

int64_t GetBlockProofEquivalentTime(const CBlockIndex &to,
                                    const CBlockIndex &from,
                                    const CBlockIndex &tip,
                                    const Consensus::Params &params) {
  arith_uint256 r;
  int sign = 1;
  if (to.nChainWork > from.nChainWork) {
    r = to.nChainWork - from.nChainWork;
  } else {
    r = from.nChainWork - to.nChainWork;
    sign = -1;
  }
  r = r * arith_uint256(params.nPowTargetSpacing) / GetBlockProof(tip);
  if (r.bits() > 63) {
    return sign * std::numeric_limits<int64_t>::max();
  }
  return sign * r.GetLow64();
}

const CBlockIndex *LastCommonAncestor(const CBlockIndex *pa,
                                      const CBlockIndex *pb) {
  if (pa->nHeight > pb->nHeight) {
    pa = pa->GetAncestor(pb->nHeight);
  } else if (pb->nHeight > pa->nHeight) {
    pb = pb->GetAncestor(pa->nHeight);
  }

  while (pa != pb && pa && pb) {
    if (pa->pskip && pb->pskip && pa->pskip != pb->pskip) {
      pa = pa->pskip;
      pb = pb->pskip;
    } else {
      pa = pa->pprev;
      pb = pb->pprev;
    }
  }

  assert(pa == pb);
  return pa;
}