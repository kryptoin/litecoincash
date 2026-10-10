// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_FEEBUMPER_H
#define BITCOIN_WALLET_FEEBUMPER_H

#include <primitives/transaction.h>

class CWallet;
class CWalletTx;
class uint256;
class CCoinControl;
enum class FeeEstimateMode;

namespace feebumper {
enum class Result {
  OK,
  INVALID_ADDRESS_OR_KEY,
  INVALID_REQUEST,
  INVALID_PARAMETER,
  WALLET_ERROR,
  MISC_ERROR,
};

bool TransactionCanBeBumped(CWallet *wallet, const uint256 &txid);

Result CreateTransaction(const CWallet *wallet, const uint256 &txid,
                         const CCoinControl &coin_control, CAmount total_fee,
                         std::vector<std::string> &errors, CAmount &old_fee,
                         CAmount &new_fee, CMutableTransaction &mtx);

bool SignTransaction(CWallet *wallet, CMutableTransaction &mtx);

Result CommitTransaction(CWallet *wallet, const uint256 &txid,
                         CMutableTransaction &&mtx,
                         std::vector<std::string> &errors,
                         uint256 &bumped_txid);

} // namespace feebumper

#endif
