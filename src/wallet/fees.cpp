// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/fees.h>

#include <policy/policy.h>
#include <txmempool.h>
#include <util.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

CAmount GetRequiredFee(unsigned int nTxBytes) {
  return std::max(CWallet::minTxFee.GetFee(nTxBytes),
                  ::minRelayTxFee.GetFee(nTxBytes));
}

CAmount GetMinimumFee(unsigned int nTxBytes, const CCoinControl &coin_control,
                      const CTxMemPool &pool,
                      const CBlockPolicyEstimator &estimator,
                      FeeCalculation *feeCalc) {
  CAmount fee_needed;
  if (coin_control.m_feerate) {
    fee_needed = coin_control.m_feerate->GetFee(nTxBytes);
    if (feeCalc)
      feeCalc->reason = FeeReason::PAYTXFEE;

    if (coin_control.fOverrideFeeRate)
      return fee_needed;
  } else if (!coin_control.m_confirm_target && ::payTxFee != CFeeRate(0)) {
    fee_needed = ::payTxFee.GetFee(nTxBytes);
    if (feeCalc)
      feeCalc->reason = FeeReason::PAYTXFEE;
  } else {
    unsigned int target = coin_control.m_confirm_target
                              ? *coin_control.m_confirm_target
                              : ::nTxConfirmTarget;

    bool conservative_estimate = !coin_control.signalRbf;

    if (coin_control.m_fee_mode == FeeEstimateMode::CONSERVATIVE)
      conservative_estimate = true;
    else if (coin_control.m_fee_mode == FeeEstimateMode::ECONOMICAL)
      conservative_estimate = false;

    fee_needed =
        estimator.estimateSmartFee(target, feeCalc, conservative_estimate)
            .GetFee(nTxBytes);
    if (fee_needed == 0) {
      fee_needed = CWallet::fallbackFee.GetFee(nTxBytes);
      if (feeCalc)
        feeCalc->reason = FeeReason::FALLBACK;
    }

    CAmount min_mempool_fee =
        pool.GetMinFee(gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) *
                       1000000)
            .GetFee(nTxBytes);
    if (fee_needed < min_mempool_fee) {
      fee_needed = min_mempool_fee;
      if (feeCalc)
        feeCalc->reason = FeeReason::MEMPOOL_MIN;
    }
  }

  CAmount required_fee = GetRequiredFee(nTxBytes);
  if (required_fee > fee_needed) {
    fee_needed = required_fee;
    if (feeCalc)
      feeCalc->reason = FeeReason::REQUIRED;
  }

  if (fee_needed > maxTxFee) {
    fee_needed = maxTxFee;
    if (feeCalc)
      feeCalc->reason = FeeReason::MAXTXFEE;
  }
  return fee_needed;
}

CFeeRate GetDiscardRate(const CBlockPolicyEstimator &estimator) {
  unsigned int highest_target =
      estimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
  CFeeRate discard_rate =
      estimator.estimateSmartFee(highest_target, nullptr, false);

  discard_rate = (discard_rate == CFeeRate(0))
                     ? CWallet::m_discard_rate
                     : std::min(discard_rate, CWallet::m_discard_rate);

  discard_rate = std::max(discard_rate, ::dustRelayFee);
  return discard_rate;
}
