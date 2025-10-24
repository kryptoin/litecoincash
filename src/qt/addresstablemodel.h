// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ADDRESSTABLEMODEL_H
#define BITCOIN_QT_ADDRESSTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

enum OutputType : int;

class AddressTablePriv;
class WalletModel;

class CWallet;

class AddressTableModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit AddressTableModel(CWallet *wallet, WalletModel *parent = 0);
  ~AddressTableModel();

  enum ColumnIndex {
    Label = 0,

    Address = 1

  };

  enum RoleIndex {
    TypeRole = Qt::UserRole

  };

  enum EditStatus {
    OK,

    NO_CHANGES,

    INVALID_ADDRESS,

    DUPLICATE_ADDRESS,

    WALLET_UNLOCK_FAILURE,

    KEY_GENERATION_FAILURE

  };

  static const QString Send;

  static const QString Receive;

  int rowCount(const QModelIndex &parent) const;
  int columnCount(const QModelIndex &parent) const;
  QVariant data(const QModelIndex &index, int role) const;
  bool setData(const QModelIndex &index, const QVariant &value, int role);
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;
  QModelIndex index(int row, int column, const QModelIndex &parent) const;
  bool removeRows(int row, int count,
                  const QModelIndex &parent = QModelIndex());
  Qt::ItemFlags flags(const QModelIndex &index) const;

  QString addRow(const QString &type, const QString &label,
                 const QString &address, const OutputType address_type);

  QString labelForAddress(const QString &address) const;

  int lookupAddress(const QString &address) const;

  EditStatus getEditStatus() const { return editStatus; }

private:
  WalletModel *walletModel;
  CWallet *wallet;
  AddressTablePriv *priv;
  QStringList columns;
  EditStatus editStatus;

  void emitDataChanged(int index);

public Q_SLOTS:

  void updateEntry(const QString &address, const QString &label, bool isMine,
                   const QString &purpose, int status);

  friend class AddressTablePriv;
};

#endif
