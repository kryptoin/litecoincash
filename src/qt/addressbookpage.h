// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ADDRESSBOOKPAGE_H
#define BITCOIN_QT_ADDRESSBOOKPAGE_H

#include <QDialog>

class AddressTableModel;
class PlatformStyle;

namespace Ui {
class AddressBookPage;
}

QT_BEGIN_NAMESPACE
class QItemSelection;
class QMenu;
class QModelIndex;
class QSortFilterProxyModel;
QT_END_NAMESPACE

class AddressBookPage : public QDialog {
  Q_OBJECT

public:
  enum Tabs { SendingTab = 0, ReceivingTab = 1 };

  enum Mode {
    ForSelection,

    ForEditing

  };

  explicit AddressBookPage(const PlatformStyle *platformStyle, Mode mode,
                           Tabs tab, QWidget *parent);
  ~AddressBookPage();

  void setModel(AddressTableModel *model);
  const QString &getReturnValue() const { return returnValue; }

public Q_SLOTS:
  void done(int retval);

private:
  Ui::AddressBookPage *ui;
  AddressTableModel *model;
  Mode mode;
  Tabs tab;
  QString returnValue;
  QSortFilterProxyModel *proxyModel;
  QMenu *contextMenu;
  QAction *deleteAction;

  QString newAddressToSelect;

private Q_SLOTS:

  void on_deleteAddress_clicked();

  void on_newAddress_clicked();

  void on_copyAddress_clicked();

  void onCopyLabelAction();

  void onEditAction();

  void on_exportButton_clicked();

  void selectionChanged();

  void contextualMenu(const QPoint &point);

  void selectNewAddress(const QModelIndex &parent, int begin, int);

Q_SIGNALS:
  void sendCoins(QString addr);
};

#endif
