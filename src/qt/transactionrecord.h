// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONRECORD_H
#define BITCOIN_QT_TRANSACTIONRECORD_H

#include <amount.h>
#include <uint256.h>

#include <QList>
#include <QString>

class CWallet;
class CWalletTx;

class TransactionStatus {
public:
  TransactionStatus()
      : countsForBalance(false), sortKey(""), matures_in(0), status(Offline),
        depth(0), open_for(0), cur_num_blocks(-1) {}

  enum Status {
    Confirmed,

    OpenUntilDate,

    OpenUntilBlock,

    Offline,

    Unconfirmed,

    Confirming,

    Conflicted,

    Abandoned,

    Immature,

    MaturesWarning,

    NotAccepted

  };

  bool countsForBalance;

  std::string sortKey;

  int matures_in;

  Status status;
  qint64 depth;
  qint64 open_for;

  int cur_num_blocks;

  bool needsUpdate;
};

class TransactionRecord {
public:
  enum Type {
    Other,
    Generated,
    SendToAddress,
    SendToOther,
    RecvWithAddress,
    RecvFromOther,
    SendToSelf,
    HiveBeeCreation,

    HiveCommunityFund,

    HiveHoney

  };

  static const int RecommendedNumConfirmations = 6;

  TransactionRecord()
      : hash(), time(0), type(Other), address(""), debit(0), credit(0), idx(0) {
  }

  TransactionRecord(uint256 _hash, qint64 _time)
      : hash(_hash), time(_time), type(Other), address(""), debit(0), credit(0),
        idx(0) {}

  TransactionRecord(uint256 _hash, qint64 _time, Type _type,
                    const std::string &_address, const CAmount &_debit,
                    const CAmount &_credit)
      : hash(_hash), time(_time), type(_type), address(_address), debit(_debit),
        credit(_credit), idx(0) {}

  static bool showTransaction(const CWalletTx &wtx);
  static QList<TransactionRecord> decomposeTransaction(const CWallet *wallet,
                                                       const CWalletTx &wtx);

  uint256 hash;
  qint64 time;
  Type type;
  std::string address;
  CAmount debit;
  CAmount credit;

  int idx;

  TransactionStatus status;

  bool involvesWatchAddress;

  QString getTxID() const;

  int getOutputIndex() const;

  void updateStatus(const CWalletTx &wtx);

  bool statusUpdateNeeded() const;
};

#endif
