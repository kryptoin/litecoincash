// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINGUI_H
#define BITCOIN_QT_BITCOINGUI_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <amount.h>

#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPoint>
#include <QSystemTrayIcon>

class ClientModel;
class NetworkStyle;
class Notificator;
class OptionsModel;
class PlatformStyle;
class RPCConsole;
class SendCoinsRecipient;
class UnitDisplayStatusBarControl;
class WalletFrame;
class WalletModel;
class HelpMessageDialog;
class ModalOverlay;

QT_BEGIN_NAMESPACE
class QAction;
class QProgressBar;
class QProgressDialog;
QT_END_NAMESPACE

class BitcoinGUI : public QMainWindow {
  Q_OBJECT

public:
  static const QString DEFAULT_WALLET;
  static const std::string DEFAULT_UIPLATFORM;

  explicit BitcoinGUI(const PlatformStyle *platformStyle,
                      const NetworkStyle *networkStyle, QWidget *parent = 0);
  ~BitcoinGUI();

  void setClientModel(ClientModel *clientModel);

#ifdef ENABLE_WALLET

  bool addWallet(const QString &name, WalletModel *walletModel);
  bool setCurrentWallet(const QString &name);
  void removeAllWallets();
#endif

  bool enableWallet;

protected:
  void changeEvent(QEvent *e);
  void closeEvent(QCloseEvent *event);
  void showEvent(QShowEvent *event);
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);
  bool eventFilter(QObject *object, QEvent *event);

private:
  ClientModel *clientModel;
  WalletFrame *walletFrame;

  UnitDisplayStatusBarControl *unitDisplayControl;
  QLabel *labelWalletEncryptionIcon;
  QLabel *labelWalletHDStatusIcon;
  QLabel *connectionsControl;
  QLabel *hiveStatusIcon;

  QLabel *labelBlocksIcon;
  QLabel *progressBarLabel;
  QProgressBar *progressBar;
  QProgressDialog *progressDialog;

  QMenuBar *appMenuBar;
  QAction *overviewAction;
  QAction *hiveAction;

  QAction *importPrivateKeyAction;

  QAction *historyAction;
  QAction *quitAction;
  QAction *sendCoinsAction;
  QAction *sendCoinsMenuAction;
  QAction *usedSendingAddressesAction;
  QAction *usedReceivingAddressesAction;
  QAction *signMessageAction;
  QAction *verifyMessageAction;
  QAction *aboutAction;
  QAction *receiveCoinsAction;
  QAction *receiveCoinsMenuAction;
  QAction *optionsAction;
  QAction *toggleHideAction;
  QAction *encryptWalletAction;
  QAction *backupWalletAction;
  QAction *changePassphraseAction;
  QAction *aboutQtAction;
  QAction *openRPCConsoleAction;
  QAction *openAction;
  QAction *showHelpMessageAction;

  QSystemTrayIcon *trayIcon;
  QMenu *trayIconMenu;
  Notificator *notificator;
  RPCConsole *rpcConsole;
  HelpMessageDialog *helpMessageDialog;
  ModalOverlay *modalOverlay;

  int prevBlocks;
  int spinnerFrame;

  const PlatformStyle *platformStyle;

  void createActions();

  void createMenuBar();

  void createToolBars();

  void createTrayIcon(const NetworkStyle *networkStyle);

  void createTrayIconMenu();

  void setWalletActionsEnabled(bool enabled);

  void subscribeToCoreSignals();

  void unsubscribeFromCoreSignals();

  void updateNetworkState();

  void updateHeadersSyncProgressLabel();

Q_SIGNALS:

  void receivedURI(const QString &uri);

public Q_SLOTS:

  void setNumConnections(int count);

  void setNetworkActive(bool networkActive);

  void setNumBlocks(int count, const QDateTime &blockDate,
                    double nVerificationProgress, bool headers);

  void message(const QString &title, const QString &message, unsigned int style,
               bool *ret = nullptr);

  void updateHiveStatusIcon(QString icon, QString tooltip);

#ifdef ENABLE_WALLET

  void setEncryptionStatus(int status);

  void setHDStatus(int hdEnabled);

  bool handlePaymentRequest(const SendCoinsRecipient &recipient);

  void incomingTransaction(const QString &date, int unit, const CAmount &amount,
                           const QString &type, const QString &address,
                           const QString &label);
#endif

private Q_SLOTS:
#ifdef ENABLE_WALLET

  void gotoOverviewPage();

  void gotoHistoryPage();

  void gotoReceiveCoinsPage();

  void gotoSendCoinsPage(QString addr = "");

  void gotoHivePage();

  void gotoSignMessageTab(QString addr = "");

  void gotoVerifyMessageTab(QString addr = "");

  void openClicked();
#endif

  void optionsClicked();

  void aboutClicked();

  void showDebugWindow();

  void showDebugWindowActivateConsole();

  void showHelpMessageClicked();
#ifndef Q_OS_MAC

  void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif

  void showNormalIfMinimized(bool fToggleHidden = false);

  void toggleHidden();

  void detectShutdown();

  void showProgress(const QString &title, int nProgress);

  void setTrayIconVisible(bool);

  void toggleNetworkActive();

  void showModalOverlay();
};

class UnitDisplayStatusBarControl : public QLabel {
  Q_OBJECT

public:
  explicit UnitDisplayStatusBarControl(const PlatformStyle *platformStyle);

  void setOptionsModel(OptionsModel *optionsModel);

protected:
  void mousePressEvent(QMouseEvent *event);

private:
  OptionsModel *optionsModel;
  QMenu *menu;

  void onDisplayUnitsClicked(const QPoint &point);

  void createContextMenu();

private Q_SLOTS:

  void updateDisplayUnit(int newUnits);

  void onMenuSelection(QAction *action);
};

#endif
