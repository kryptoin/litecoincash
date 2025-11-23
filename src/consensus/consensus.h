// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_CONSENSUS_H
#define BITCOIN_CONSENSUS_CONSENSUS_H

#include <stdint.h>
#include <stdlib.h>

static const unsigned int MAX_BLOCK_SERIALIZED_SIZE = 4000000;

static const unsigned int MAX_BLOCK_WEIGHT = 4000000;

static const int64_t MAX_BLOCK_SIGOPS_COST = 80000;

static const int COINBASE_MATURITY = 100;

// Introspection hardening: Default anchor depth for reorg protection
// Reorgs deeper than this trigger additional scrutiny (policy-level, not consensus)
static const int DEFAULT_ANCHOR_DEPTH = 12;

// Enable introspection hardening features by default
static const bool DEFAULT_ENABLE_INTROSPECTION_HARDENING = true;

// Maximum fork traversal depth (introspection hardening)
// Limits backward traversal in FindFork to prevent chain topology mapping
static const int DEFAULT_MAX_FORK_TRAVERSAL = 1000;

// Exposure delay depth for shallow blocks (introspection hardening)
// Blocks within this depth may have metadata redacted in certain contexts
static const int DEFAULT_EXPOSURE_DELAY_DEPTH = 3;

static const int WITNESS_SCALE_FACTOR = 4;

static const size_t MIN_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 60;

static const size_t MIN_SERIALIZABLE_TRANSACTION_WEIGHT =
    WITNESS_SCALE_FACTOR * 10;

static constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);

static constexpr unsigned int LOCKTIME_MEDIAN_TIME_PAST = (1 << 1);

#endif
