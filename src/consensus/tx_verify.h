// Copyright (c) 2017-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_TX_VERIFY_H
#define BITCOIN_CONSENSUS_TX_VERIFY_H

#include <amount.h>

#include <stdint.h>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;
class CTransaction;
class CValidationState;

bool CheckTransaction(const CTransaction &tx, CValidationState &state,
                      bool fCheckDuplicateInputs = true);

namespace Consensus {
bool CheckTxInputs(const CTransaction &tx, CValidationState &state,
                   const CCoinsViewCache &inputs, int nSpendHeight,
                   CAmount &txfee);
}

unsigned int GetLegacySigOpCount(const CTransaction &tx);

unsigned int GetP2SHSigOpCount(const CTransaction &tx,
                               const CCoinsViewCache &mapInputs);

int64_t GetTransactionSigOpCost(const CTransaction &tx,
                                const CCoinsViewCache &inputs, int flags);

bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction &tx,
                                               int flags,
                                               std::vector<int> *prevHeights,
                                               const CBlockIndex &block);

bool EvaluateSequenceLocks(const CBlockIndex &block,
                           std::pair<int, int64_t> lockPair);

bool SequenceLocks(const CTransaction &tx, int flags,
                   std::vector<int> *prevHeights, const CBlockIndex &block);

#endif
