// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_AMOUNT_H
#define BITCOIN_AMOUNT_H

#include <stdint.h>

typedef int64_t CAmount;

static const CAmount COIN_SCALE = 10;

static const CAmount COIN = 100000000 / COIN_SCALE;

static const CAmount CENT = 1000000 / COIN_SCALE;

static const CAmount MAX_MONEY = 84000000 * COIN * COIN_SCALE;

inline bool MoneyRange(const CAmount &nValue) {
  return (nValue >= 0 && nValue <= MAX_MONEY);
}

#endif
