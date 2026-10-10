// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONTABLEMODEL_H
#define BITCOIN_QT_TRANSACTIONTABLEMODEL_H

#include <qt/bitcoinunits.h>

#include <QAbstractTableModel>
#include <QStringList>

class PlatformStyle;
class TransactionRecord;
class TransactionTablePriv;
class WalletModel;

class CWallet;

class TransactionTableModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit TransactionTableModel(const PlatformStyle *platformStyle,
                                 CWallet *wallet, WalletModel *parent = 0);
  ~TransactionTableModel();

  enum ColumnIndex {
    Status = 0,
    Watchonly = 1,
    Date = 2,
    Type = 3,
    ToAddress = 4,
    Amount = 5
  };

  enum RoleIndex {
    TypeRole = Qt::UserRole,

    DateRole,

    WatchonlyRole,

    WatchonlyDecorationRole,

    LongDescriptionRole,

    AddressRole,

    LabelRole,

    AmountRole,

    TxIDRole,

    TxHashRole,

    TxHexRole,

    TxPlainTextRole,

    ConfirmedRole,

    FormattedAmountRole,

    StatusRole,

    RawDecorationRole,
  };

  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  QModelIndex index(int row, int column,
                    const QModelIndex &parent = QModelIndex()) const;
  bool processingQueuedTransactions() const {
    return fProcessingQueuedTransactions;
  }

private:
  CWallet *wallet;
  WalletModel *walletModel;
  QStringList columns;
  TransactionTablePriv *priv;
  bool fProcessingQueuedTransactions;
  const PlatformStyle *platformStyle;

  void subscribeToCoreSignals();
  void unsubscribeFromCoreSignals();

  QString lookupAddress(const std::string &address, bool tooltip) const;
  QVariant addressColor(const TransactionRecord *wtx) const;
  QString formatTxStatus(const TransactionRecord *wtx) const;
  QString formatTxDate(const TransactionRecord *wtx) const;
  QString formatTxType(const TransactionRecord *wtx) const;
  QString formatTxToAddress(const TransactionRecord *wtx, bool tooltip) const;
  QString formatTxAmount(const TransactionRecord *wtx,
                         bool showUnconfirmed = true,
                         BitcoinUnits::SeparatorStyle separators =
                             BitcoinUnits::separatorStandard) const;
  QString formatTooltip(const TransactionRecord *rec) const;
  QVariant txStatusDecoration(const TransactionRecord *wtx) const;
  QVariant txWatchonlyDecoration(const TransactionRecord *wtx) const;
  QVariant txAddressDecoration(const TransactionRecord *wtx) const;

public Q_SLOTS:

  void updateTransaction(const QString &hash, int status, bool showTransaction);
  void updateConfirmations();
  void updateDisplayUnit();

  void updateAmountColumnTitle();

  void setProcessingQueuedTransactions(bool value) {
    fProcessingQueuedTransactions = value;
  }

  friend class TransactionTablePriv;
};

#endif
