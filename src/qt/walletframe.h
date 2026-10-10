// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETFRAME_H
#define BITCOIN_QT_WALLETFRAME_H

#include <QFrame>
#include <QMap>

class BitcoinGUI;
class ClientModel;
class PlatformStyle;
class SendCoinsRecipient;
class WalletModel;
class WalletView;

QT_BEGIN_NAMESPACE
class QStackedWidget;
QT_END_NAMESPACE

class WalletFrame : public QFrame {
  Q_OBJECT

public:
  explicit WalletFrame(const PlatformStyle *platformStyle,
                       BitcoinGUI *_gui = 0);
  ~WalletFrame();

  void setClientModel(ClientModel *clientModel);

  bool addWallet(const QString &name, WalletModel *walletModel);
  bool setCurrentWallet(const QString &name);
  bool removeWallet(const QString &name);
  void removeAllWallets();

  bool handlePaymentRequest(const SendCoinsRecipient &recipient);

  void showOutOfSyncWarning(bool fShow);

Q_SIGNALS:

  void requestedSyncWarningInfo();

private:
  QStackedWidget *walletStack;
  BitcoinGUI *gui;
  ClientModel *clientModel;
  QMap<QString, WalletView *> mapWalletViews;

  bool bOutOfSync;

  const PlatformStyle *platformStyle;

  WalletView *currentWalletView();

public Q_SLOTS:

  void gotoOverviewPage();

  void gotoHistoryPage();

  void gotoReceiveCoinsPage();

  void gotoSendCoinsPage(QString addr = "");

  void gotoHivePage();

  void importPrivateKey();

  void gotoSignMessageTab(QString addr = "");

  void gotoVerifyMessageTab(QString addr = "");

  void encryptWallet(bool status);

  void backupWallet();

  void changePassphrase();

  void unlockWallet();

  void usedSendingAddresses();

  void usedReceivingAddresses();

  void outOfSyncWarningClicked();
};

#endif
