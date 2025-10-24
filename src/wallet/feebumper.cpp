// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/feebumper.h>
#include <wallet/fees.h>
#include <wallet/wallet.h>

#include <net.h>
#include <txmempool.h>
#include <util.h>
#include <utilmoneystr.h>

static int64_t CalculateMaximumSignedTxSize(const CTransaction &tx,
                                            const CWallet *wallet) {
  CMutableTransaction txNew(tx);
  std::vector<CInputCoin> vCoins;

  for (auto &input : tx.vin) {
    const auto mi = wallet->mapWallet.find(input.prevout.hash);
    assert(mi != wallet->mapWallet.end() &&
           input.prevout.n < mi->second.tx->vout.size());
    vCoins.emplace_back(CInputCoin(&(mi->second), input.prevout.n));
  }
  if (!wallet->DummySignTx(txNew, vCoins)) {
    return -1;
  }
  return GetVirtualTransactionSize(txNew);
}

static feebumper::Result PreconditionChecks(const CWallet *wallet,
                                            const CWalletTx &wtx,
                                            std::vector<std::string> &errors) {
  if (wallet->HasWalletSpend(wtx.GetHash())) {
    errors.push_back("Transaction has descendants in the wallet");
    return feebumper::Result::INVALID_PARAMETER;
  }

  {
    LOCK(mempool.cs);
    auto it_mp = mempool.mapTx.find(wtx.GetHash());
    if (it_mp != mempool.mapTx.end() && it_mp->GetCountWithDescendants() > 1) {
      errors.push_back("Transaction has descendants in the mempool");
      return feebumper::Result::INVALID_PARAMETER;
    }
  }

  if (wtx.GetDepthInMainChain() != 0) {
    errors.push_back("Transaction has been mined, or is conflicted with a "
                     "mined transaction");
    return feebumper::Result::WALLET_ERROR;
  }
  return feebumper::Result::OK;
}

namespace feebumper {
bool TransactionCanBeBumped(CWallet *wallet, const uint256 &txid) {
  LOCK2(cs_main, wallet->cs_wallet);
  const CWalletTx *wtx = wallet->GetWalletTx(txid);
  return wtx && SignalsOptInRBF(*wtx->tx) &&
         !wtx->mapValue.count("replaced_by_txid");
}

Result CreateTransaction(const CWallet *wallet, const uint256 &txid,
                         const CCoinControl &coin_control, CAmount total_fee,
                         std::vector<std::string> &errors, CAmount &old_fee,
                         CAmount &new_fee, CMutableTransaction &mtx) {
  LOCK2(cs_main, wallet->cs_wallet);
  errors.clear();
  auto it = wallet->mapWallet.find(txid);
  if (it == wallet->mapWallet.end()) {
    errors.push_back("Invalid or non-wallet transaction id");
    return Result::INVALID_ADDRESS_OR_KEY;
  }
  const CWalletTx &wtx = it->second;

  Result result = PreconditionChecks(wallet, wtx, errors);
  if (result != Result::OK) {
    return result;
  }

  if (!SignalsOptInRBF(*wtx.tx)) {
    errors.push_back("Transaction is not BIP 125 replaceable");
    return Result::WALLET_ERROR;
  }

  if (wtx.mapValue.count("replaced_by_txid")) {
    errors.push_back(strprintf(
        "Cannot bump transaction %s which was already bumped by transaction %s",
        txid.ToString(), wtx.mapValue.at("replaced_by_txid")));
    return Result::WALLET_ERROR;
  }

  if (!wallet->IsAllFromMe(*wtx.tx, ISMINE_SPENDABLE)) {
    errors.push_back(
        "Transaction contains inputs that don't belong to this wallet");
    return Result::WALLET_ERROR;
  }

  int nOutput = -1;
  for (size_t i = 0; i < wtx.tx->vout.size(); ++i) {
    if (wallet->IsChange(wtx.tx->vout[i])) {
      if (nOutput != -1) {
        errors.push_back("Transaction has multiple change outputs");
        return Result::WALLET_ERROR;
      }
      nOutput = i;
    }
  }
  if (nOutput == -1) {
    errors.push_back("Transaction does not have a change output");
    return Result::WALLET_ERROR;
  }

  int64_t txSize = GetVirtualTransactionSize(*(wtx.tx));
  const int64_t maxNewTxSize = CalculateMaximumSignedTxSize(*wtx.tx, wallet);
  if (maxNewTxSize < 0) {
    errors.push_back("Transaction contains inputs that cannot be signed");
    return Result::INVALID_ADDRESS_OR_KEY;
  }

  old_fee = wtx.GetDebit(ISMINE_SPENDABLE) - wtx.tx->GetValueOut();
  CFeeRate nOldFeeRate(old_fee, txSize);
  CFeeRate nNewFeeRate;

  CFeeRate walletIncrementalRelayFee = CFeeRate(WALLET_INCREMENTAL_RELAY_FEE);
  if (::incrementalRelayFee > walletIncrementalRelayFee) {
    walletIncrementalRelayFee = ::incrementalRelayFee;
  }

  if (total_fee > 0) {
    CAmount minTotalFee = nOldFeeRate.GetFee(maxNewTxSize) +
                          ::incrementalRelayFee.GetFee(maxNewTxSize);
    if (total_fee < minTotalFee) {
      errors.push_back(
          strprintf("Insufficient totalFee, must be at least %s (oldFee %s + "
                    "incrementalFee %s)",
                    FormatMoney(minTotalFee),
                    FormatMoney(nOldFeeRate.GetFee(maxNewTxSize)),
                    FormatMoney(::incrementalRelayFee.GetFee(maxNewTxSize))));
      return Result::INVALID_PARAMETER;
    }
    CAmount requiredFee = GetRequiredFee(maxNewTxSize);
    if (total_fee < requiredFee) {
      errors.push_back(strprintf(
          "Insufficient totalFee (cannot be less than required fee %s)",
          FormatMoney(requiredFee)));
      return Result::INVALID_PARAMETER;
    }
    new_fee = total_fee;
    nNewFeeRate = CFeeRate(total_fee, maxNewTxSize);
  } else {
    new_fee = GetMinimumFee(maxNewTxSize, coin_control, mempool, ::feeEstimator,
                            nullptr);
    nNewFeeRate = CFeeRate(new_fee, maxNewTxSize);

    if (nNewFeeRate.GetFeePerK() <
        nOldFeeRate.GetFeePerK() + 1 + walletIncrementalRelayFee.GetFeePerK()) {
      nNewFeeRate = CFeeRate(nOldFeeRate.GetFeePerK() + 1 +
                             walletIncrementalRelayFee.GetFeePerK());
      new_fee = nNewFeeRate.GetFee(maxNewTxSize);
    }
  }

  if (new_fee > maxTxFee) {
    errors.push_back(strprintf("Specified or calculated fee %s is too high "
                               "(cannot be higher than maxTxFee %s)",
                               FormatMoney(new_fee), FormatMoney(maxTxFee)));
    return Result::WALLET_ERROR;
  }

  CFeeRate minMempoolFeeRate = mempool.GetMinFee(
      gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000);
  if (nNewFeeRate.GetFeePerK() < minMempoolFeeRate.GetFeePerK()) {
    errors.push_back(
        strprintf("New fee rate (%s) is lower than the minimum fee rate (%s) "
                  "to get into the mempool -- "
                  "the totalFee value should be at least %s or the settxfee "
                  "value should be at least %s to add transaction",
                  FormatMoney(nNewFeeRate.GetFeePerK()),
                  FormatMoney(minMempoolFeeRate.GetFeePerK()),
                  FormatMoney(minMempoolFeeRate.GetFee(maxNewTxSize)),
                  FormatMoney(minMempoolFeeRate.GetFeePerK())));
    return Result::WALLET_ERROR;
  }

  CAmount nDelta = new_fee - old_fee;
  assert(nDelta > 0);
  mtx = *wtx.tx;
  CTxOut *poutput = &(mtx.vout[nOutput]);
  if (poutput->nValue < nDelta) {
    errors.push_back("Change output is too small to bump the fee");
    return Result::WALLET_ERROR;
  }

  poutput->nValue -= nDelta;
  if (poutput->nValue <= GetDustThreshold(*poutput, ::dustRelayFee)) {
    LogPrint(BCLog::RPC, "Bumping fee and discarding dust output\n");
    new_fee += poutput->nValue;
    mtx.vout.erase(mtx.vout.begin() + nOutput);
  }

  if (!coin_control.signalRbf) {
    for (auto &input : mtx.vin) {
      if (input.nSequence < 0xfffffffe)
        input.nSequence = 0xfffffffe;
    }
  }

  return Result::OK;
}

bool SignTransaction(CWallet *wallet, CMutableTransaction &mtx) {
  LOCK2(cs_main, wallet->cs_wallet);
  return wallet->SignTransaction(mtx);
}

Result CommitTransaction(CWallet *wallet, const uint256 &txid,
                         CMutableTransaction &&mtx,
                         std::vector<std::string> &errors,
                         uint256 &bumped_txid) {
  LOCK2(cs_main, wallet->cs_wallet);
  if (!errors.empty()) {
    return Result::MISC_ERROR;
  }
  auto it =
      txid.IsNull() ? wallet->mapWallet.end() : wallet->mapWallet.find(txid);
  if (it == wallet->mapWallet.end()) {
    errors.push_back("Invalid or non-wallet transaction id");
    return Result::MISC_ERROR;
  }
  CWalletTx &oldWtx = it->second;

  Result result = PreconditionChecks(wallet, oldWtx, errors);
  if (result != Result::OK) {
    return result;
  }

  CWalletTx wtxBumped(wallet, MakeTransactionRef(std::move(mtx)));

  CReserveKey reservekey(wallet);
  wtxBumped.mapValue = oldWtx.mapValue;
  wtxBumped.mapValue["replaces_txid"] = oldWtx.GetHash().ToString();
  wtxBumped.vOrderForm = oldWtx.vOrderForm;
  wtxBumped.strFromAccount = oldWtx.strFromAccount;
  wtxBumped.fTimeReceivedIsTxTime = true;
  wtxBumped.fFromMe = true;
  CValidationState state;
  if (!wallet->CommitTransaction(wtxBumped, reservekey, g_connman.get(),
                                 state)) {
    errors.push_back(
        strprintf("The transaction was rejected: %s", state.GetRejectReason()));
    return Result::WALLET_ERROR;
  }

  bumped_txid = wtxBumped.GetHash();
  if (state.IsInvalid()) {
    errors.push_back(strprintf("Error: The transaction was rejected: %s",
                               FormatStateMessage(state)));
  }

  if (!wallet->MarkReplaced(oldWtx.GetHash(), wtxBumped.GetHash())) {
    errors.push_back("Created new bumpfee transaction but could not mark the "
                     "original transaction as replaced");
  }
  return Result::OK;
}

} // namespace feebumper
