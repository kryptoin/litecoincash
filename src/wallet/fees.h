// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_FEES_H
#define BITCOIN_WALLET_FEES_H

#include <amount.h>

class CBlockPolicyEstimator;
class CCoinControl;
class CFeeRate;
class CTxMemPool;
struct FeeCalculation;

CAmount GetRequiredFee(unsigned int nTxBytes);

CAmount GetMinimumFee(unsigned int nTxBytes, const CCoinControl &coin_control,
                      const CTxMemPool &pool,
                      const CBlockPolicyEstimator &estimator,
                      FeeCalculation *feeCalc);

CFeeRate GetDiscardRate(const CBlockPolicyEstimator &estimator);

#endif
