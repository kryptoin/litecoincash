// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_COINCONTROL_H
#define BITCOIN_WALLET_COINCONTROL_H

#include <policy/feerate.h>
#include <policy/fees.h>
#include <primitives/transaction.h>
#include <wallet/wallet.h>

#include <boost/optional.hpp>

class CCoinControl {
public:
  CTxDestination destChange;

  OutputType change_type;

  bool fAllowOtherInputs;

  bool fAllowWatchOnly;

  bool fOverrideFeeRate;

  boost::optional<CFeeRate> m_feerate;

  boost::optional<unsigned int> m_confirm_target;

  bool signalRbf;

  FeeEstimateMode m_fee_mode;

  CCoinControl() { SetNull(); }

  void SetNull() {
    destChange = CNoDestination();
    change_type = g_change_type;
    fAllowOtherInputs = false;
    fAllowWatchOnly = false;
    setSelected.clear();
    m_feerate.reset();
    fOverrideFeeRate = false;
    m_confirm_target.reset();
    signalRbf = fWalletRbf;
    m_fee_mode = FeeEstimateMode::UNSET;
  }

  bool HasSelected() const { return (setSelected.size() > 0); }

  bool IsSelected(const COutPoint &output) const {
    return (setSelected.count(output) > 0);
  }

  void Select(const COutPoint &output) { setSelected.insert(output); }

  void UnSelect(const COutPoint &output) { setSelected.erase(output); }

  void UnSelectAll() { setSelected.clear(); }

  void ListSelected(std::vector<COutPoint> &vOutpoints) const {
    vOutpoints.assign(setSelected.begin(), setSelected.end());
  }

private:
  std::set<COutPoint> setSelected;
};

#endif
