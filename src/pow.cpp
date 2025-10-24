// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <base58.h>
#include <chain.h>
#include <core_io.h>
#include <hash.h>
#include <primitives/block.h>
#include <pubkey.h>
#include <script/standard.h>
#include <sync.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>
#include <validation.h>

BeePopGraphPoint beePopGraph[1024 * 40];

unsigned int GetNextWorkRequiredLWMA(const CBlockIndex *pindexLast,
                                     const CBlockHeader *pblock,
                                     const Consensus::Params &params,
                                     const POW_TYPE powType) {
  const bool verbose = LogAcceptCategory(BCLog::MINOTAURX);
  const arith_uint256 powLimit = UintToArith256(params.powTypeLimits[powType]);

  const int64_t T = params.nPowTargetSpacing * 2;
  const int64_t N = params.lwmaAveragingWindow;
  const int64_t k = N * (N + 1) * T / 2;
  const int64_t height = pindexLast->nHeight;

  if (params.fPowAllowMinDifficultyBlocks &&
      pblock->GetBlockTime() > pindexLast->GetBlockTime() + T * 10) {
    if (verbose)
      LogPrintf("* GetNextWorkRequiredLWMA: Allowing %s pow limit (apparent "
                "testnet stall)\n",
                POW_TYPE_NAMES[powType]);
    return powLimit.GetCompact();
  }

  if (height < N) {
    if (verbose)
      LogPrintf(
          "* GetNextWorkRequiredLWMA: Allowing %s pow limit (short chain)\n",
          POW_TYPE_NAMES[powType]);
    return powLimit.GetCompact();
  }

  arith_uint256 avgTarget, nextTarget;
  int64_t thisTimestamp, previousTimestamp;
  int64_t sumWeightedSolvetimes = 0, j = 0, blocksFound = 0;

  std::vector<const CBlockIndex *> wantedBlocks;
  const CBlockIndex *blockPreviousTimestamp = pindexLast;
  while (blocksFound < N) {
    if (blockPreviousTimestamp->GetBlockHeader().nVersion >= 0x20000000) {
      if (verbose)
        LogPrintf("* GetNextWorkRequiredLWMA: Allowing %s pow limit "
                  "(previousTime calc reached forkpoint at height %i)\n",
                  POW_TYPE_NAMES[powType], blockPreviousTimestamp->nHeight);
      return powLimit.GetCompact();
    }

    if (blockPreviousTimestamp->GetBlockHeader().IsHiveMined(params) ||
        blockPreviousTimestamp->GetBlockHeader().GetPoWType() != powType) {
      assert(blockPreviousTimestamp->pprev);
      blockPreviousTimestamp = blockPreviousTimestamp->pprev;
      continue;
    }

    wantedBlocks.push_back(blockPreviousTimestamp);

    blocksFound++;
    if (blocksFound == N)

      break;

    assert(blockPreviousTimestamp->pprev);
    blockPreviousTimestamp = blockPreviousTimestamp->pprev;
  }
  previousTimestamp = blockPreviousTimestamp->GetBlockTime();

  for (auto it = wantedBlocks.rbegin(); it != wantedBlocks.rend(); ++it) {
    const CBlockIndex *block = *it;

    thisTimestamp = (block->GetBlockTime() > previousTimestamp)
                        ? block->GetBlockTime()
                        : previousTimestamp + 1;

    int64_t solvetime = std::min(6 * T, thisTimestamp - previousTimestamp);

    previousTimestamp = thisTimestamp;

    j++;
    sumWeightedSolvetimes += solvetime * j;

    arith_uint256 target;
    target.SetCompact(block->nBits);
    avgTarget += target / N / k;
  }

  nextTarget = avgTarget * sumWeightedSolvetimes;

  if (nextTarget > powLimit) {
    if (verbose)
      LogPrintf("* GetNextWorkRequiredLWMA: Allowing %s pow limit (target too "
                "high)\n",
                POW_TYPE_NAMES[powType]);
    return powLimit.GetCompact();
  }

  return nextTarget.GetCompact();
}

unsigned int DarkGravityWave(const CBlockIndex *pindexLast,
                             const CBlockHeader *pblock,
                             const Consensus::Params &params) {
  const arith_uint256 bnPowLimit = UintToArith256(params.powLimitSHA);
  int64_t nPastBlocks = 24;

  if (params.fPowAllowMinDifficultyBlocks &&
      pblock->GetBlockTime() >
          pindexLast->GetBlockTime() + params.nPowTargetSpacing * 10)
    return bnPowLimit.GetCompact();

  if (IsHive11Enabled(pindexLast, params)) {
    while (pindexLast->GetBlockHeader().IsHiveMined(params)) {
      assert(pindexLast->pprev);

      pindexLast = pindexLast->pprev;
    }
  }

  if (!pindexLast || pindexLast->nHeight - params.lastScryptBlock < nPastBlocks)
    return bnPowLimit.GetCompact();

  const CBlockIndex *pindex = pindexLast;
  arith_uint256 bnPastTargetAvg;

  for (unsigned int nCountBlocks = 1; nCountBlocks <= nPastBlocks;
       nCountBlocks++) {
    while (pindex->GetBlockHeader().IsHiveMined(params)) {
      assert(pindex->pprev);

      pindex = pindex->pprev;
    }

    arith_uint256 bnTarget = arith_uint256().SetCompact(pindex->nBits);
    if (nCountBlocks == 1) {
      bnPastTargetAvg = bnTarget;
    } else {
      bnPastTargetAvg =
          (bnPastTargetAvg * nCountBlocks + bnTarget) / (nCountBlocks + 1);
    }

    if (nCountBlocks != nPastBlocks) {
      assert(pindex->pprev);

      pindex = pindex->pprev;
    }
  }

  arith_uint256 bnNew(bnPastTargetAvg);

  int64_t nActualTimespan = pindexLast->GetBlockTime() - pindex->GetBlockTime();

  int64_t nTargetTimespan = nPastBlocks * params.nPowTargetSpacing;

  if (nActualTimespan < nTargetTimespan / 3)
    nActualTimespan = nTargetTimespan / 3;
  if (nActualTimespan > nTargetTimespan * 3)
    nActualTimespan = nTargetTimespan * 3;

  bnNew *= nActualTimespan;
  bnNew /= nTargetTimespan;

  if (bnNew > bnPowLimit) {
    bnNew = bnPowLimit;
  }

  return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex *pindexLast,
                                 const CBlockHeader *pblock,
                                 const Consensus::Params &params) {
  assert(pindexLast != nullptr);

  if (pindexLast->nHeight >= params.lastScryptBlock)
    return DarkGravityWave(pindexLast, pblock, params);
  else
    return GetNextWorkRequiredLTC(pindexLast, pblock, params);
}

unsigned int GetNextWorkRequiredLTC(const CBlockIndex *pindexLast,
                                    const CBlockHeader *pblock,
                                    const Consensus::Params &params) {
  assert(pindexLast != nullptr);
  unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

  if ((pindexLast->nHeight + 1) % params.DifficultyAdjustmentInterval() != 0) {
    if (params.fPowAllowMinDifficultyBlocks) {
      if (pblock->GetBlockTime() >
          pindexLast->GetBlockTime() + params.nPowTargetSpacing * 2) {
        return nProofOfWorkLimit;
      } else {
        const CBlockIndex *pindex = pindexLast;
        int nBlocksToSearch = params.DifficultyAdjustmentInterval() - 1;
        while (pindex->pprev &&
               pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 &&
               pindex->nBits == nProofOfWorkLimit && nBlocksToSearch > 0) {
          pindex = pindex->pprev;
          nBlocksToSearch--;
        }
        return pindex->nBits;
      }
    }
    return pindexLast->nBits;
  }

  int blockstogoback = params.DifficultyAdjustmentInterval() - 1;
  if ((pindexLast->nHeight + 1) != params.DifficultyAdjustmentInterval()) {
    blockstogoback = params.DifficultyAdjustmentInterval();
  }

  const CBlockIndex *pindexFirst = pindexLast;
  for (int i = 0; pindexFirst && i < blockstogoback; i++) {
    pindexFirst = pindexFirst->pprev;
  }

  assert(pindexFirst);

  return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(),
                                   params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex *pindexLast,
                                       int64_t nFirstBlockTime,
                                       const Consensus::Params &params) {
  if (params.fPowNoRetargeting)
    return pindexLast->nBits;

  int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
  if (nActualTimespan < params.nPowTargetTimespan / 4)
    nActualTimespan = params.nPowTargetTimespan / 4;
  if (nActualTimespan > params.nPowTargetTimespan * 4)
    nActualTimespan = params.nPowTargetTimespan * 4;

  arith_uint256 bnNew;
  arith_uint256 bnOld;
  bnNew.SetCompact(pindexLast->nBits);
  bnOld = bnNew;

  const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
  bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
  if (fShift)
    bnNew >>= 1;
  bnNew *= nActualTimespan;
  bnNew /= params.nPowTargetTimespan;
  if (fShift)
    bnNew <<= 1;

  if (bnNew > bnPowLimit)
    bnNew = bnPowLimit;

  return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits,
                      const Consensus::Params &params) {
  bool fNegative;
  bool fOverflow;
  arith_uint256 bnTarget;

  bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

  arith_uint256 powLimit = 0;
  for (int i = 0; i < NUM_BLOCK_TYPES; i++)
    if (UintToArith256(params.powTypeLimits[i]) > powLimit)
      powLimit = UintToArith256(params.powTypeLimits[i]);

  if (fNegative || bnTarget == 0 || fOverflow || bnTarget > powLimit)
    return false;

  if (UintToArith256(hash) > bnTarget)
    return false;

  return true;
}

unsigned int GetNextHive11WorkRequired(const CBlockIndex *pindexLast,
                                       const Consensus::Params &params) {
  const arith_uint256 bnPowLimit = UintToArith256(params.powLimitHive);

  arith_uint256 beeHashTarget = 0;
  int hiveBlockCount = 0;
  int totalBlockCount = 0;

  while (hiveBlockCount < params.hiveDifficultyWindow && pindexLast->pprev &&
         pindexLast->nHeight >= params.minHiveCheckBlock) {
    if (pindexLast->GetBlockHeader().IsHiveMined(params)) {
      beeHashTarget += arith_uint256().SetCompact(pindexLast->nBits);
      hiveBlockCount++;
    }
    totalBlockCount++;
    pindexLast = pindexLast->pprev;
  }

  if (hiveBlockCount == 0) {
    LogPrintf("GetNextHive11WorkRequired: No previous hive blocks found.\n");
    return bnPowLimit.GetCompact();
  }

  beeHashTarget /= hiveBlockCount;

  int targetTotalBlockCount = hiveBlockCount * params.hiveBlockSpacingTarget;
  beeHashTarget *= totalBlockCount;
  beeHashTarget /= targetTotalBlockCount;

  if (beeHashTarget > bnPowLimit)
    beeHashTarget = bnPowLimit;

  return beeHashTarget.GetCompact();
}

unsigned int GetNextHive12WorkRequired(const CBlockIndex *pindexLast,
                                       const Consensus::Params &params) {
  const arith_uint256 bnPowLimit = UintToArith256(params.powLimitHive);

  arith_uint256 beeHashTarget = 0;
  int hiveBlockCount = 0;
  int totalBlockCount = 0;

  while (hiveBlockCount < params.hiveDifficultyWindow && pindexLast->pprev &&
         IsMinotaurXEnabled(pindexLast, params)) {
    if (pindexLast->GetBlockHeader().IsHiveMined(params)) {
      beeHashTarget += arith_uint256().SetCompact(pindexLast->nBits);
      hiveBlockCount++;
    }
    totalBlockCount++;
    pindexLast = pindexLast->pprev;
  }

  if (hiveBlockCount < params.hiveDifficultyWindow) {
    LogPrintf("GetNextHive12WorkRequired: Insufficient hive blocks.\n");
    return bnPowLimit.GetCompact();
  }

  beeHashTarget /= hiveBlockCount;

  int targetTotalBlockCount = hiveBlockCount * params.hiveBlockSpacingTarget;
  beeHashTarget *= totalBlockCount;
  beeHashTarget /= targetTotalBlockCount;

  if (beeHashTarget > bnPowLimit)
    beeHashTarget = bnPowLimit;

  return beeHashTarget.GetCompact();
}

unsigned int GetNextHiveWorkRequired(const CBlockIndex *pindexLast,
                                     const Consensus::Params &params) {
  if (IsMinotaurXEnabled(pindexLast, params))
    return GetNextHive12WorkRequired(pindexLast, params);

  if (IsHive11Enabled(pindexLast, params))
    return GetNextHive11WorkRequired(pindexLast, params);

  const arith_uint256 bnPowLimit = UintToArith256(params.powLimitHive);
  const arith_uint256 bnImpossible = 0;
  arith_uint256 beeHashTarget;

  int numPowBlocks = 0;
  CBlockHeader block;
  while (true) {
    if (!pindexLast->pprev || pindexLast->nHeight < params.minHiveCheckBlock) {
      LogPrintf(
          "GetNextHiveWorkRequired: No hivemined blocks found in history\n");

      return bnPowLimit.GetCompact();
    }

    block = pindexLast->GetBlockHeader();
    if (block.IsHiveMined(params)) {
      beeHashTarget.SetCompact(block.nBits);
      break;
    }

    pindexLast = pindexLast->pprev;
    numPowBlocks++;
  }

  if (numPowBlocks == 0)
    return bnImpossible.GetCompact();

  int interval =
      params.hiveTargetAdjustAggression / params.hiveBlockSpacingTarget;
  beeHashTarget *= (interval - 1) * params.hiveBlockSpacingTarget +
                   numPowBlocks + numPowBlocks;
  beeHashTarget /= (interval + 1) * params.hiveBlockSpacingTarget;

  if (beeHashTarget > bnPowLimit)
    beeHashTarget = bnPowLimit;

  return beeHashTarget.GetCompact();
}

bool GetNetworkHiveInfo(int &immatureBees, int &immatureBCTs, int &matureBees,
                        int &matureBCTs, CAmount &potentialLifespanRewards,
                        const Consensus::Params &consensusParams,
                        bool recalcGraph) {
  int totalBeeLifespan =
      consensusParams.beeLifespanBlocks + consensusParams.beeGestationBlocks;
  immatureBees = immatureBCTs = matureBees = matureBCTs = 0;

  CBlockIndex *pindexPrev = chainActive.Tip();
  assert(pindexPrev != nullptr);
  int tipHeight = pindexPrev->nHeight;

  auto blockReward = GetBlockSubsidy(pindexPrev->nHeight, consensusParams);
  if (IsMinotaurXEnabled(pindexPrev, consensusParams))
    blockReward += blockReward >> 1;

  if (IsHive11Enabled(pindexPrev, consensusParams))
    potentialLifespanRewards =
        (consensusParams.beeLifespanBlocks * blockReward) /
        consensusParams.hiveBlockSpacingTargetTypical_1_1;
  else
    potentialLifespanRewards =
        (consensusParams.beeLifespanBlocks * blockReward) /
        consensusParams.hiveBlockSpacingTargetTypical;

  if (recalcGraph) {
    for (int i = 0; i < totalBeeLifespan; i++) {
      beePopGraph[i].immaturePop = 0;
      beePopGraph[i].maturePop = 0;
    }
  }

  if (IsInitialBlockDownload())

    return false;

  CBlock block;
  CScript scriptPubKeyBCF = GetScriptForDestination(
      DecodeDestination(consensusParams.beeCreationAddress));
  CScript scriptPubKeyCF = GetScriptForDestination(
      DecodeDestination(consensusParams.hiveCommunityAddress));

  for (int i = 0; i < totalBeeLifespan; i++) {
    if (fHavePruned && !(pindexPrev->nStatus & BLOCK_HAVE_DATA) &&
        pindexPrev->nTx > 0) {
      LogPrintf("! GetNetworkHiveInfo: Warn: Block not available (pruned "
                "data); can't calculate network bee count.");
      return false;
    }

    if (!pindexPrev->GetBlockHeader().IsHiveMined(consensusParams)) {
      if (!ReadBlockFromDisk(block, pindexPrev, consensusParams)) {
        LogPrintf("! GetNetworkHiveInfo: Warn: Block not available (not found "
                  "on disk); can't calculate network bee count.");
        return false;
      }
      int blockHeight = pindexPrev->nHeight;
      CAmount beeCost = GetBeeCost(blockHeight, consensusParams);
      if (block.vtx.size() > 0) {
        for (const auto &tx : block.vtx) {
          CAmount beeFeePaid;
          if (tx->IsBCT(consensusParams, scriptPubKeyBCF, &beeFeePaid)) {
            if (tx->vout.size() > 1 &&
                tx->vout[1].scriptPubKey == scriptPubKeyCF) {
              CAmount donationAmount = tx->vout[1].nValue;
              CAmount expectedDonationAmount =
                  (beeFeePaid + donationAmount) /
                  consensusParams.communityContribFactor;

              if (IsMinotaurXEnabled(pindexPrev, consensusParams))
                expectedDonationAmount += expectedDonationAmount >> 1;
              if (donationAmount != expectedDonationAmount)
                continue;
              beeFeePaid += donationAmount;
            }
            int beeCount = beeFeePaid / beeCost;
            if (i < consensusParams.beeGestationBlocks) {
              immatureBees += beeCount;
              immatureBCTs++;
            } else {
              matureBees += beeCount;
              matureBCTs++;
            }

            if (recalcGraph) {
              int beeBornBlock = blockHeight;
              int beeMaturesBlock =
                  beeBornBlock + consensusParams.beeGestationBlocks;
              int beeDiesBlock =
                  beeMaturesBlock + consensusParams.beeLifespanBlocks;
              for (int j = beeBornBlock; j < beeDiesBlock; j++) {
                int graphPos = j - tipHeight;
                if (graphPos > 0 && graphPos < totalBeeLifespan) {
                  if (j < beeMaturesBlock)
                    beePopGraph[graphPos].immaturePop += beeCount;
                  else
                    beePopGraph[graphPos].maturePop += beeCount;
                }
              }
            }
          }
        }
      }
    }

    if (!pindexPrev->pprev)

      return true;

    pindexPrev = pindexPrev->pprev;
  }

  return true;
}

bool CheckHiveProof(const CBlock *pblock,
                    const Consensus::Params &consensusParams) {
  bool verbose = LogAcceptCategory(BCLog::HIVE);

  if (verbose)
    LogPrintf(
        "********************* Hive: CheckHiveProof *********************\n");

  int blockHeight;
  CBlockIndex *pindexPrev;
  {
    LOCK(cs_main);
    pindexPrev = mapBlockIndex[pblock->hashPrevBlock];
    blockHeight = pindexPrev->nHeight + 1;
  }
  if (!pindexPrev) {
    LogPrintf("CheckHiveProof: Couldn't get previous block's CBlockIndex!\n");
    return false;
  }
  if (verbose)
    LogPrintf("CheckHiveProof: nHeight             = %i\n", blockHeight);

  if (!IsHiveEnabled(pindexPrev, consensusParams)) {
    LogPrintf("CheckHiveProof: Can't accept a Hive block; Hive is not yet "
              "enabled on the network.\n");
    return false;
  }

  if (IsHive11Enabled(pindexPrev, consensusParams)) {
    int hiveBlocksAtTip = 0;
    CBlockIndex *pindexTemp = pindexPrev;
    while (pindexTemp->GetBlockHeader().IsHiveMined(consensusParams)) {
      assert(pindexTemp->pprev);
      pindexTemp = pindexTemp->pprev;
      hiveBlocksAtTip++;
    }
    if (hiveBlocksAtTip >= consensusParams.maxConsecutiveHiveBlocks) {
      LogPrintf("CheckHiveProof: Too many Hive blocks without a POW block.\n");
      return false;
    }
  } else {
    if (pindexPrev->GetBlockHeader().IsHiveMined(consensusParams)) {
      LogPrint(BCLog::HIVE,
               "CheckHiveProof: Hive block must follow a POW block.\n");
      return false;
    }
  }

  CScript scriptPubKeyBCF = GetScriptForDestination(
      DecodeDestination(consensusParams.beeCreationAddress));
  if (pblock->vtx.size() > 1)
    for (unsigned int i = 1; i < pblock->vtx.size(); i++)
      if (pblock->vtx[i]->IsBCT(consensusParams, scriptPubKeyBCF)) {
        LogPrintf("CheckHiveProof: Hivemined block contains BCTs!\n");
        return false;
      }

  CTransactionRef txCoinbase = pblock->vtx[0];

  if (!txCoinbase->IsCoinBase()) {
    LogPrintf("CheckHiveProof: Coinbase tx isn't valid!\n");
    return false;
  }

  if (txCoinbase->vout.size() < 2 || txCoinbase->vout.size() > 3) {
    LogPrintf("CheckHiveProof: Didn't expect %i vouts!\n",
              txCoinbase->vout.size());
    return false;
  }

  if (txCoinbase->vout[0].scriptPubKey.size() < 144) {
    LogPrintf("CheckHiveProof: vout[0].scriptPubKey isn't long enough to "
              "contain hive proof encodings\n");
    return false;
  }

  if (txCoinbase->vout[0].scriptPubKey[0] != OP_RETURN ||
      txCoinbase->vout[0].scriptPubKey[1] != OP_BEE) {
    LogPrintf("CheckHiveProof: vout[0].scriptPubKey doesn't start OP_RETURN "
              "OP_BEE\n");
    return false;
  }

  uint32_t beeNonce = ReadLE32(&txCoinbase->vout[0].scriptPubKey[3]);
  if (verbose)
    LogPrintf("CheckHiveProof: beeNonce            = %i\n", beeNonce);

  uint32_t bctClaimedHeight = ReadLE32(&txCoinbase->vout[0].scriptPubKey[8]);
  if (verbose)
    LogPrintf("CheckHiveProof: bctHeight           = %i\n", bctClaimedHeight);

  bool communityContrib = txCoinbase->vout[0].scriptPubKey[12] == OP_TRUE;
  if (verbose)
    LogPrintf("CheckHiveProof: communityContrib    = %s\n",
              communityContrib ? "true" : "false");

  std::vector<unsigned char> txid(&txCoinbase->vout[0].scriptPubKey[14],
                                  &txCoinbase->vout[0].scriptPubKey[14 + 64]);
  std::string txidStr = std::string(txid.begin(), txid.end());
  if (verbose)
    LogPrintf("CheckHiveProof: bctTxId             = %s\n", txidStr);

  std::string deterministicRandString = GetDeterministicRandString(pindexPrev);
  if (verbose)
    LogPrintf("CheckHiveProof: detRandString       = %s\n",
              deterministicRandString);
  arith_uint256 beeHashTarget;
  beeHashTarget.SetCompact(
      GetNextHiveWorkRequired(pindexPrev, consensusParams));
  if (verbose)
    LogPrintf("CheckHiveProof: beeHashTarget       = %s\n",
              beeHashTarget.ToString());

  if (!IsMinotaurXEnabled(pindexPrev, consensusParams)) {
    std::string hashHex = (CHashWriter(SER_GETHASH, 0)
                           << deterministicRandString << txidStr << beeNonce)
                              .GetHash()
                              .GetHex();
    arith_uint256 beeHash = arith_uint256(hashHex);
    if (verbose)
      LogPrintf("CheckHiveProof: beeHash             = %s\n", beeHash.GetHex());
    if (beeHash >= beeHashTarget) {
      LogPrintf("CheckHiveProof: Bee does not meet hash target!\n");
      return false;
    }
  } else {
    arith_uint256 beeHash(CBlockHeader::MinotaurHashArbitrary(
                              std::string(deterministicRandString + txidStr +
                                          std::to_string(beeNonce))
                                  .c_str())
                              .ToString());
    if (verbose)
      LogPrintf("CheckHive12Proof: beeHash           = %s\n", beeHash.GetHex());
    if (beeHash >= beeHashTarget) {
      LogPrintf("CheckHive12Proof: Bee does not meet hash target!\n");
      return false;
    }
  }

  std::vector<unsigned char> messageSig(
      &txCoinbase->vout[0].scriptPubKey[79],
      &txCoinbase->vout[0].scriptPubKey[79 + 65]);
  if (verbose)
    LogPrintf("CheckHiveProof: messageSig          = %s\n",
              HexStr(&messageSig[0], &messageSig[messageSig.size()]));

  CTxDestination honeyDestination;
  if (!ExtractDestination(txCoinbase->vout[1].scriptPubKey, honeyDestination)) {
    LogPrintf("CheckHiveProof: Couldn't extract honey address\n");
    return false;
  }
  if (!IsValidDestination(honeyDestination)) {
    LogPrintf("CheckHiveProof: Honey address is invalid\n");
    return false;
  }
  if (verbose)
    LogPrintf("CheckHiveProof: honeyAddress        = %s\n",
              EncodeDestination(honeyDestination));

  const CKeyID *keyID = boost::get<CKeyID>(&honeyDestination);
  if (!keyID) {
    LogPrintf("CheckHiveProof: Can't get pubkey for honey address\n");
    return false;
  }
  CHashWriter ss(SER_GETHASH, 0);
  ss << deterministicRandString;
  uint256 mhash = ss.GetHash();
  CPubKey pubkey;
  if (!pubkey.RecoverCompact(mhash, messageSig)) {
    LogPrintf("CheckHiveProof: Couldn't recover pubkey from hash\n");
    return false;
  }
  if (pubkey.GetID() != *keyID) {
    LogPrintf("CheckHiveProof: Signature mismatch! GetID() = %s, *keyID = %s\n",
              pubkey.GetID().ToString(), (*keyID).ToString());
    return false;
  }

  bool deepDrill = false;
  uint32_t bctFoundHeight;
  CAmount bctValue;
  CScript bctScriptPubKey;
  bool bctWasMinotaurXEnabled;

  {
    LOCK(cs_main);

    COutPoint outBeeCreation(uint256S(txidStr), 0);
    COutPoint outCommFund(uint256S(txidStr), 1);
    Coin coin;
    CTransactionRef bct = nullptr;
    CBlockIndex foundAt;

    if (pcoinsTip && pcoinsTip->GetCoin(outBeeCreation, coin)) {
      if (verbose)
        LogPrintf("CheckHiveProof: Using UTXO set for outBeeCreation\n");
      bctValue = coin.out.nValue;
      bctScriptPubKey = coin.out.scriptPubKey;
      bctFoundHeight = coin.nHeight;
      bctWasMinotaurXEnabled =
          IsMinotaurXEnabled(chainActive[bctFoundHeight], consensusParams);

    } else {
      if (verbose)
        LogPrintf(
            "! CheckHiveProof: Warn: Using deep drill for outBeeCreation\n");
      if (!GetTxByHashAndHeight(uint256S(txidStr), bctClaimedHeight, bct,
                                foundAt, pindexPrev, consensusParams)) {
        LogPrintf("CheckHiveProof: Couldn't locate indicated BCT\n");
        return false;
      }
      deepDrill = true;
      bctFoundHeight = foundAt.nHeight;
      bctValue = bct->vout[0].nValue;
      bctScriptPubKey = bct->vout[0].scriptPubKey;
      bctWasMinotaurXEnabled = IsMinotaurXEnabled(&foundAt, consensusParams);
    }

    if (communityContrib) {
      CScript scriptPubKeyCF = GetScriptForDestination(
          DecodeDestination(consensusParams.hiveCommunityAddress));
      CAmount donationAmount;

      if (bct == nullptr) {
        if (pcoinsTip && pcoinsTip->GetCoin(outCommFund, coin)) {
          if (verbose)
            LogPrintf("CheckHiveProof: Using UTXO set for outCommFund\n");
          if (coin.out.scriptPubKey != scriptPubKeyCF) {
            LogPrintf("CheckHiveProof: Community contrib was indicated but not "
                      "found\n");
            return false;
          }
          donationAmount = coin.out.nValue;
        } else {
          if (verbose)
            LogPrintf(
                "! CheckHiveProof: Warn: Using deep drill for outCommFund\n");
          if (!GetTxByHashAndHeight(uint256S(txidStr), bctClaimedHeight, bct,
                                    foundAt, pindexPrev, consensusParams)) {
            LogPrintf("CheckHiveProof: Couldn't locate indicated BCT\n");

            return false;
          }
          deepDrill = true;
        }
      }
      if (bct != nullptr) {
        if (bct->vout.size() < 2 ||
            bct->vout[1].scriptPubKey != scriptPubKeyCF) {
          LogPrintf("CheckHiveProof: Community contrib was indicated but not "
                    "found\n");
          return false;
        }
        donationAmount = bct->vout[1].nValue;
      }

      CAmount expectedDonationAmount =
          (bctValue + donationAmount) / consensusParams.communityContribFactor;

      if (bctWasMinotaurXEnabled)
        expectedDonationAmount += expectedDonationAmount >> 1;

      if (donationAmount != expectedDonationAmount) {
        LogPrintf("CheckHiveProof: BCT pays community fund incorrect amount %i "
                  "(expected %i)\n",
                  donationAmount, expectedDonationAmount);
        return false;
      }

      bctValue += donationAmount;
    }
  }

  if (bctFoundHeight != bctClaimedHeight) {
    LogPrintf("CheckHiveProof: Claimed BCT height of %i conflicts with found "
              "height of %i\n",
              bctClaimedHeight, bctFoundHeight);
    return false;
  }

  int bctDepth = blockHeight - bctFoundHeight;
  if (bctDepth < consensusParams.beeGestationBlocks) {
    LogPrintf("CheckHiveProof: Indicated BCT is immature.\n");
    return false;
  }
  if (bctDepth >
      consensusParams.beeGestationBlocks + consensusParams.beeLifespanBlocks) {
    LogPrintf("CheckHiveProof: Indicated BCT is too old.\n");
    return false;
  }

  CScript scriptPubKeyHoney;
  if (!CScript::IsBCTScript(bctScriptPubKey, scriptPubKeyBCF,
                            &scriptPubKeyHoney)) {
    LogPrintf("CheckHiveProof: Indicated utxo is not a valid BCT script\n");
    return false;
  }

  CTxDestination honeyDestinationBCT;
  if (!ExtractDestination(scriptPubKeyHoney, honeyDestinationBCT)) {
    LogPrintf("CheckHiveProof: Couldn't extract honey address from BCT UTXO\n");
    return false;
  }

  if (honeyDestination != honeyDestinationBCT) {
    LogPrintf("CheckHiveProof: BCT's honey address does not match claimed "
              "honey address!\n");
    return false;
  }

  CAmount beeCost = GetBeeCost(bctFoundHeight, consensusParams);
  if (bctValue < consensusParams.minBeeCost) {
    LogPrintf(
        "CheckHiveProof: BCT fee is less than the minimum possible bee cost\n");
    return false;
  }
  if (bctValue < beeCost) {
    LogPrintf(
        "CheckHiveProof: BCT fee is less than the cost for a single bee\n");
    return false;
  }
  unsigned int beeCount = bctValue / beeCost;
  if (verbose) {
    LogPrintf("CheckHiveProof: bctValue            = %i\n", bctValue);
    LogPrintf("CheckHiveProof: beeCost             = %i\n", beeCost);
    LogPrintf("CheckHiveProof: beeCount            = %i\n", beeCount);
  }

  if (beeNonce >= beeCount) {
    LogPrintf(
        "CheckHiveProof: BCT did not create enough bees for claimed nonce!\n");
    return false;
  }

  if (verbose)
    LogPrintf("CheckHiveProof: Pass at %i%s\n", blockHeight,
              deepDrill ? " (used deepdrill)" : "");

  return true;
}