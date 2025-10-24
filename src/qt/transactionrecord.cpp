// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/transactionrecord.h>

#include <base58.h>
#include <consensus/consensus.h>
#include <timedata.h>
#include <validation.h>
#include <wallet/wallet.h>

#include <stdint.h>

bool TransactionRecord::showTransaction(const CWalletTx &wtx) { return true; }

QList<TransactionRecord>
TransactionRecord::decomposeTransaction(const CWallet *wallet,
                                        const CWalletTx &wtx) {
  QList<TransactionRecord> parts;
  int64_t nTime = wtx.GetTxTime();
  CAmount nCredit = wtx.GetCredit(ISMINE_ALL);
  CAmount nDebit = wtx.GetDebit(ISMINE_ALL);
  CAmount nNet = nCredit - nDebit;
  uint256 hash = wtx.GetHash();
  std::map<std::string, std::string> mapValue = wtx.mapValue;

  if (nNet > 0 || wtx.IsCoinBase()) {
    for (unsigned int i = 0; i < wtx.tx->vout.size(); i++) {
      const CTxOut &txout = wtx.tx->vout[i];
      isminetype mine = wallet->IsMine(txout);
      if (mine) {
        TransactionRecord sub(hash, nTime);
        CTxDestination address;
        sub.idx = i;

        sub.credit = txout.nValue;
        sub.involvesWatchAddress = mine & ISMINE_WATCH_ONLY;
        if (ExtractDestination(txout.scriptPubKey, address) &&
            IsMine(*wallet, address)) {
          sub.type = TransactionRecord::RecvWithAddress;
          sub.address = EncodeDestination(address);
        } else {
          sub.type = TransactionRecord::RecvFromOther;
          sub.address = mapValue["from"];
        }
        if (wtx.IsCoinBase()) {
          if (wtx.IsHiveCoinBase())

            sub.type = TransactionRecord::HiveHoney;
          else

            sub.type = TransactionRecord::Generated;
        }

        parts.append(sub);
      }
    }
  } else {
    bool involvesWatchAddress = false;
    isminetype fAllFromMe = ISMINE_SPENDABLE;
    for (const CTxIn &txin : wtx.tx->vin) {
      isminetype mine = wallet->IsMine(txin);
      if (mine & ISMINE_WATCH_ONLY)
        involvesWatchAddress = true;
      if (fAllFromMe > mine)
        fAllFromMe = mine;
    }

    isminetype fAllToMe = ISMINE_SPENDABLE;
    for (const CTxOut &txout : wtx.tx->vout) {
      isminetype mine = wallet->IsMine(txout);
      if (mine & ISMINE_WATCH_ONLY)
        involvesWatchAddress = true;
      if (fAllToMe > mine)
        fAllToMe = mine;
    }

    if (fAllFromMe && fAllToMe) {
      CAmount nChange = wtx.GetChange();

      parts.append(TransactionRecord(hash, nTime, TransactionRecord::SendToSelf,
                                     "", -(nDebit - nChange),
                                     nCredit - nChange));
      parts.last().involvesWatchAddress = involvesWatchAddress;

    } else if (fAllFromMe) {
      CAmount nTxFee = nDebit - wtx.tx->GetValueOut();

      for (unsigned int nOut = 0; nOut < wtx.tx->vout.size(); nOut++) {
        const CTxOut &txout = wtx.tx->vout[nOut];
        TransactionRecord sub(hash, nTime);
        sub.idx = nOut;
        sub.involvesWatchAddress = involvesWatchAddress;

        if (wallet->IsMine(txout)) {
          continue;
        }

        CTxDestination address;

        if (CScript::IsBCTScript(
                txout.scriptPubKey,
                GetScriptForDestination(DecodeDestination(
                    Params().GetConsensus().beeCreationAddress)))) {
          sub.type = TransactionRecord::HiveBeeCreation;
        } else if (ExtractDestination(txout.scriptPubKey, address)) {
          sub.type = TransactionRecord::SendToAddress;
          sub.address = EncodeDestination(address);

          if (sub.address == Params().GetConsensus().hiveCommunityAddress)
            sub.type = TransactionRecord::HiveCommunityFund;
        } else {
          sub.type = TransactionRecord::SendToOther;
          sub.address = mapValue["to"];
        }

        CAmount nValue = txout.nValue;

        if (nTxFee > 0) {
          nValue += nTxFee;
          nTxFee = 0;
        }
        sub.debit = -nValue;

        parts.append(sub);
      }
    } else {
      parts.append(TransactionRecord(hash, nTime, TransactionRecord::Other, "",
                                     nNet, 0));
      parts.last().involvesWatchAddress = involvesWatchAddress;
    }
  }

  return parts;
}

void TransactionRecord::updateStatus(const CWalletTx &wtx) {
  AssertLockHeld(cs_main);

  CBlockIndex *pindex = nullptr;
  BlockMap::iterator mi = mapBlockIndex.find(wtx.hashBlock);
  if (mi != mapBlockIndex.end())
    pindex = (*mi).second;

  status.sortKey =
      strprintf("%010d-%01d-%010u-%03d",
                (pindex ? pindex->nHeight : std::numeric_limits<int>::max()),
                (wtx.IsCoinBase() ? 1 : 0), wtx.nTimeReceived, idx);
  status.countsForBalance = wtx.IsTrusted() && !(wtx.GetBlocksToMaturity() > 0);
  status.depth = wtx.GetDepthInMainChain();
  status.cur_num_blocks = chainActive.Height();

  if (!CheckFinalTx(*wtx.tx)) {
    if (wtx.tx->nLockTime < LOCKTIME_THRESHOLD) {
      status.status = TransactionStatus::OpenUntilBlock;
      status.open_for = wtx.tx->nLockTime - chainActive.Height();
    } else {
      status.status = TransactionStatus::OpenUntilDate;
      status.open_for = wtx.tx->nLockTime;
    }
  }

  else if (type == TransactionRecord::Generated ||
           type == TransactionRecord::HiveHoney) {
    if (wtx.GetBlocksToMaturity() > 0) {
      status.status = TransactionStatus::Immature;

      if (wtx.IsInMainChain()) {
        status.matures_in = wtx.GetBlocksToMaturity();

        if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 &&
            wtx.GetRequestCount() == 0)
          status.status = TransactionStatus::MaturesWarning;
      } else {
        status.status = TransactionStatus::NotAccepted;
      }
    } else {
      status.status = TransactionStatus::Confirmed;
    }
  } else {
    if (status.depth < 0) {
      status.status = TransactionStatus::Conflicted;
    } else if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 &&
               wtx.GetRequestCount() == 0) {
      status.status = TransactionStatus::Offline;
    } else if (status.depth == 0) {
      status.status = TransactionStatus::Unconfirmed;
      if (wtx.isAbandoned())
        status.status = TransactionStatus::Abandoned;
    } else if (status.depth < RecommendedNumConfirmations) {
      status.status = TransactionStatus::Confirming;
    } else {
      status.status = TransactionStatus::Confirmed;
    }
  }
  status.needsUpdate = false;
}

bool TransactionRecord::statusUpdateNeeded() const {
  AssertLockHeld(cs_main);
  return status.cur_num_blocks != chainActive.Height() || status.needsUpdate;
}

QString TransactionRecord::getTxID() const {
  return QString::fromStdString(hash.ToString());
}

int TransactionRecord::getOutputIndex() const { return idx; }
