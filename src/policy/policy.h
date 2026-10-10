// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_POLICY_H
#define BITCOIN_POLICY_POLICY_H

#include <consensus/consensus.h>
#include <policy/feerate.h>
#include <script/interpreter.h>
#include <script/standard.h>

#include <string>

class CCoinsViewCache;
class CTxOut;

static const unsigned int DEFAULT_BLOCK_MAX_WEIGHT = MAX_BLOCK_WEIGHT - 4000;

static const unsigned int DEFAULT_BLOCK_MIN_TX_FEE = 1000;

static const unsigned int MAX_STANDARD_TX_WEIGHT = 400000;

static const unsigned int MAX_P2SH_SIGOPS = 15;

static const unsigned int MAX_STANDARD_TX_SIGOPS_COST =
    MAX_BLOCK_SIGOPS_COST / 5;

static const unsigned int DEFAULT_MAX_MEMPOOL_SIZE = 300;

static const unsigned int DEFAULT_INCREMENTAL_RELAY_FEE = 1000;

static const unsigned int DEFAULT_BYTES_PER_SIGOP = 20;

static const unsigned int MAX_STANDARD_P2WSH_STACK_ITEMS = 100;

static const unsigned int MAX_STANDARD_P2WSH_STACK_ITEM_SIZE = 80;

static const unsigned int MAX_STANDARD_P2WSH_SCRIPT_SIZE = 3600;

static const unsigned int DUST_RELAY_TX_FEE = 3000;

static constexpr unsigned int STANDARD_SCRIPT_VERIFY_FLAGS =
    MANDATORY_SCRIPT_VERIFY_FLAGS | SCRIPT_VERIFY_DERSIG |
    SCRIPT_VERIFY_STRICTENC | SCRIPT_VERIFY_MINIMALDATA |
    SCRIPT_VERIFY_NULLDUMMY | SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
    SCRIPT_VERIFY_CLEANSTACK | SCRIPT_VERIFY_MINIMALIF |
    SCRIPT_VERIFY_NULLFAIL | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
    SCRIPT_VERIFY_CHECKSEQUENCEVERIFY | SCRIPT_VERIFY_LOW_S |
    SCRIPT_VERIFY_WITNESS |
    SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM |
    SCRIPT_VERIFY_WITNESS_PUBKEYTYPE | SCRIPT_ENABLE_SIGHASH_FORKID;

static constexpr unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS =
    STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS;

static constexpr unsigned int STANDARD_LOCKTIME_VERIFY_FLAGS =
    LOCKTIME_VERIFY_SEQUENCE | LOCKTIME_MEDIAN_TIME_PAST;

CAmount GetDustThreshold(const CTxOut &txout, const CFeeRate &dustRelayFee);

bool IsDust(const CTxOut &txout, const CFeeRate &dustRelayFee);

bool IsStandard(const CScript &scriptPubKey, txnouttype &whichType,
                const bool witnessEnabled = false);

bool IsStandardTx(const CTransaction &tx, std::string &reason,
                  const bool witnessEnabled = false);

bool AreInputsStandard(const CTransaction &tx,
                       const CCoinsViewCache &mapInputs);

bool IsWitnessStandard(const CTransaction &tx,
                       const CCoinsViewCache &mapInputs);

extern CFeeRate incrementalRelayFee;
extern CFeeRate dustRelayFee;
extern unsigned int nBytesPerSigOp;

int64_t GetVirtualTransactionSize(int64_t nWeight, int64_t nSigOpCost);
int64_t GetVirtualTransactionSize(const CTransaction &tx,
                                  int64_t nSigOpCost = 0);

#endif
