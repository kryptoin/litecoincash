// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETVIEW_H
#define BITCOIN_QT_WALLETVIEW_H

#include <amount.h>
#include <wallet/wallet.h>

#include <QStackedWidget>

class BitcoinGUI;
class ClientModel;
class OverviewPage;
class HiveDialog;

class PlatformStyle;
class ReceiveCoinsDialog;
class SendCoinsDialog;
class SendCoinsRecipient;
class TransactionView;
class WalletModel;
class AddressBookPage;

QT_BEGIN_NAMESPACE
class QModelIndex;
class QProgressDialog;
QT_END_NAMESPACE

class WalletView : public QStackedWidget {
  Q_OBJECT

public:
  explicit WalletView(const PlatformStyle *platformStyle, QWidget *parent);
  ~WalletView();

  void setBitcoinGUI(BitcoinGUI *gui);

  void setClientModel(ClientModel *clientModel);

  void setWalletModel(WalletModel *walletModel);

  bool handlePaymentRequest(const SendCoinsRecipient &recipient);

  void showOutOfSyncWarning(bool fShow);

  static void doRescan(CWallet *pwallet, int64_t startTime);

private:
  ClientModel *clientModel;
  WalletModel *walletModel;

  OverviewPage *overviewPage;
  HiveDialog *hivePage;

  QWidget *transactionsPage;
  ReceiveCoinsDialog *receiveCoinsPage;
  SendCoinsDialog *sendCoinsPage;
  AddressBookPage *usedSendingAddressesPage;
  AddressBookPage *usedReceivingAddressesPage;

  TransactionView *transactionView;

  QProgressDialog *progressDialog;
  const PlatformStyle *platformStyle;

public Q_SLOTS:

  void gotoOverviewPage();

  void gotoHistoryPage();

  void gotoReceiveCoinsPage();

  void gotoSendCoinsPage(QString addr = "");

  void gotoHivePage();

  void gotoSignMessageTab(QString addr = "");

  void gotoVerifyMessageTab(QString addr = "");

  void processNewTransaction(const QModelIndex &parent, int start, int);

  void encryptWallet(bool status);

  void backupWallet();

  void changePassphrase();

  void unlockWallet();

  void unlockWalletHive();

  void importPrivateKey();

  void usedSendingAddresses();

  void usedReceivingAddresses();

  void updateEncryptionStatus();

  void showProgress(const QString &title, int nProgress);

  void requestedSyncWarningInfo();

Q_SIGNALS:

  void showNormalIfMinimized();

  void message(const QString &title, const QString &message,
               unsigned int style);

  void encryptionStatusChanged(int status);

  void hdEnabledStatusChanged(int hdEnabled);

  void incomingTransaction(const QString &date, int unit, const CAmount &amount,
                           const QString &type, const QString &address,
                           const QString &label);

  void outOfSyncWarningClicked();
};

#endif
