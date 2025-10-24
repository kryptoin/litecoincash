// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/walletview.h>

#include <qt/addressbookpage.h>
#include <qt/askpassphrasedialog.h>
#include <qt/bitcoingui.h>
#include <qt/clientmodel.h>
#include <qt/guiutil.h>
#include <qt/hivedialog.h>
#include <qt/optionsmodel.h>
#include <qt/overviewpage.h>

#include <boost/thread.hpp>
#include <qt/platformstyle.h>
#include <qt/receivecoinsdialog.h>
#include <qt/sendcoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/transactiontablemodel.h>
#include <qt/transactionview.h>
#include <qt/walletmodel.h>

#include <wallet/rpcwallet.h>

#include <wallet/wallet.h>

#include <validation.h>

#include <ui_interface.h>

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>

WalletView::WalletView(const PlatformStyle *_platformStyle, QWidget *parent)
    : QStackedWidget(parent), clientModel(0), walletModel(0),
      platformStyle(_platformStyle) {
  overviewPage = new OverviewPage(platformStyle);
  hivePage = new HiveDialog(platformStyle);

  transactionsPage = new QWidget(this);
  QVBoxLayout *vbox = new QVBoxLayout();
  QHBoxLayout *hbox_buttons = new QHBoxLayout();
  transactionView = new TransactionView(platformStyle, this);
  vbox->addWidget(transactionView);
  QPushButton *exportButton = new QPushButton(tr("&Export"), this);
  exportButton->setToolTip(tr("Export the data in the current tab to a file"));
  if (platformStyle->getImagesOnButtons()) {
    exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
  }
  hbox_buttons->addStretch();
  hbox_buttons->addWidget(exportButton);
  vbox->addLayout(hbox_buttons);
  transactionsPage->setLayout(vbox);

  receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
  sendCoinsPage = new SendCoinsDialog(platformStyle);

  usedSendingAddressesPage =
      new AddressBookPage(platformStyle, AddressBookPage::ForEditing,
                          AddressBookPage::SendingTab, this);
  usedReceivingAddressesPage =
      new AddressBookPage(platformStyle, AddressBookPage::ForEditing,
                          AddressBookPage::ReceivingTab, this);

  addWidget(overviewPage);
  addWidget(transactionsPage);
  addWidget(receiveCoinsPage);
  addWidget(sendCoinsPage);
  addWidget(hivePage);

  connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)),
          transactionView, SLOT(focusTransaction(QModelIndex)));
  connect(overviewPage, SIGNAL(outOfSyncWarningClicked()), this,
          SLOT(requestedSyncWarningInfo()));

  connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView,
          SLOT(showDetails()));

  connect(exportButton, SIGNAL(clicked()), transactionView,
          SLOT(exportClicked()));

  connect(sendCoinsPage, SIGNAL(message(QString, QString, unsigned int)), this,
          SIGNAL(message(QString, QString, unsigned int)));

  connect(transactionView, SIGNAL(message(QString, QString, unsigned int)),
          this, SIGNAL(message(QString, QString, unsigned int)));
}

WalletView::~WalletView() {}

void WalletView::setBitcoinGUI(BitcoinGUI *gui) {
  if (gui) {
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), gui,
            SLOT(gotoHistoryPage()));

    connect(overviewPage, SIGNAL(beeButtonClicked()), gui,
            SLOT(gotoHivePage()));

    connect(this, SIGNAL(message(QString, QString, unsigned int)), gui,
            SLOT(message(QString, QString, unsigned int)));

    connect(this, SIGNAL(encryptionStatusChanged(int)), gui,
            SLOT(setEncryptionStatus(int)));

    connect(this,
            SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString,
                                       QString)),
            gui,
            SLOT(incomingTransaction(QString, int, CAmount, QString, QString,
                                     QString)));

    connect(this, SIGNAL(hdEnabledStatusChanged(int)), gui,
            SLOT(setHDStatus(int)));

    connect(hivePage, SIGNAL(hiveStatusIconChanged(QString, QString)), gui,
            SLOT(updateHiveStatusIcon(QString, QString)));
  }
}

void WalletView::setClientModel(ClientModel *_clientModel) {
  this->clientModel = _clientModel;

  overviewPage->setClientModel(_clientModel);
  sendCoinsPage->setClientModel(_clientModel);
  hivePage->setClientModel(_clientModel);
}

void WalletView::setWalletModel(WalletModel *_walletModel) {
  this->walletModel = _walletModel;

  transactionView->setModel(_walletModel);
  overviewPage->setWalletModel(_walletModel);
  hivePage->setModel(_walletModel);

  receiveCoinsPage->setModel(_walletModel);
  sendCoinsPage->setModel(_walletModel);
  usedReceivingAddressesPage->setModel(
      _walletModel ? _walletModel->getAddressTableModel() : nullptr);
  usedSendingAddressesPage->setModel(
      _walletModel ? _walletModel->getAddressTableModel() : nullptr);

  if (_walletModel) {
    connect(_walletModel, SIGNAL(message(QString, QString, unsigned int)), this,
            SIGNAL(message(QString, QString, unsigned int)));

    connect(_walletModel, SIGNAL(encryptionStatusChanged(int)), this,
            SIGNAL(encryptionStatusChanged(int)));
    updateEncryptionStatus();

    Q_EMIT hdEnabledStatusChanged(_walletModel->hdEnabled());

    connect(_walletModel->getTransactionTableModel(),
            SIGNAL(rowsInserted(QModelIndex, int, int)), this,
            SLOT(processNewTransaction(QModelIndex, int, int)));

    connect(_walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

    connect(_walletModel, SIGNAL(requireUnlockHive()), this,
            SLOT(unlockWalletHive()));

    connect(_walletModel, SIGNAL(showProgress(QString, int)), this,
            SLOT(showProgress(QString, int)));
  }
}

void WalletView::processNewTransaction(const QModelIndex &parent, int start,
                                       int) {
  if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
    return;

  TransactionTableModel *ttm = walletModel->getTransactionTableModel();
  if (!ttm || ttm->processingQueuedTransactions())
    return;

  QString date =
      ttm->index(start, TransactionTableModel::Date, parent).data().toString();
  qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                      .data(Qt::EditRole)
                      .toULongLong();
  QString type =
      ttm->index(start, TransactionTableModel::Type, parent).data().toString();
  QModelIndex index = ttm->index(start, 0, parent);
  QString address =
      ttm->data(index, TransactionTableModel::AddressRole).toString();
  QString label = ttm->data(index, TransactionTableModel::LabelRole).toString();

  Q_EMIT incomingTransaction(date,
                             walletModel->getOptionsModel()->getDisplayUnit(),
                             amount, type, address, label);
}

void WalletView::gotoOverviewPage() { setCurrentWidget(overviewPage); }

void WalletView::gotoHivePage() {
  hivePage->updateData();
  setCurrentWidget(hivePage);
}

void WalletView::gotoHistoryPage() { setCurrentWidget(transactionsPage); }

void WalletView::gotoReceiveCoinsPage() { setCurrentWidget(receiveCoinsPage); }

void WalletView::gotoSendCoinsPage(QString addr) {
  setCurrentWidget(sendCoinsPage);

  if (!addr.isEmpty())
    sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr) {
  SignVerifyMessageDialog *signVerifyMessageDialog =
      new SignVerifyMessageDialog(platformStyle, this);
  signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
  signVerifyMessageDialog->setModel(walletModel);
  signVerifyMessageDialog->showTab_SM(true);

  if (!addr.isEmpty())
    signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr) {
  SignVerifyMessageDialog *signVerifyMessageDialog =
      new SignVerifyMessageDialog(platformStyle, this);
  signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
  signVerifyMessageDialog->setModel(walletModel);
  signVerifyMessageDialog->showTab_VM(true);

  if (!addr.isEmpty())
    signVerifyMessageDialog->setAddress_VM(addr);
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient &recipient) {
  return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow) {
  overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus() {
  Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status) {
  if (!walletModel)
    return;
  AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt
                                 : AskPassphraseDialog::Decrypt,
                          this);
  dlg.setModel(walletModel);
  dlg.exec();

  updateEncryptionStatus();
}

void WalletView::backupWallet() {
  QString filename = GUIUtil::getSaveFileName(
      this, tr("Backup Wallet"), QString(), tr("Wallet Data (*.dat)"), nullptr);

  if (filename.isEmpty())
    return;

  if (!walletModel->backupWallet(filename)) {
    Q_EMIT message(
        tr("Backup Failed"),
        tr("There was an error trying to save the wallet data to %1.")
            .arg(filename),
        CClientUIInterface::MSG_ERROR);
  } else {
    Q_EMIT message(
        tr("Backup Successful"),
        tr("The wallet data was successfully saved to %1.").arg(filename),
        CClientUIInterface::MSG_INFORMATION);
  }
}

void WalletView::changePassphrase() {
  AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
  dlg.setModel(walletModel);
  dlg.exec();
}

void WalletView::unlockWallet() {
  if (!walletModel)
    return;

  if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
    AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
    dlg.setModel(walletModel);
    dlg.exec();
  }
}

void WalletView::unlockWalletHive() {
  if (!walletModel)
    return;

  if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
    AskPassphraseDialog dlg(AskPassphraseDialog::UnlockHiveMining, this);
    dlg.setModel(walletModel);
    dlg.exec();
  }
}

void WalletView::usedSendingAddresses() {
  if (!walletModel)
    return;

  usedSendingAddressesPage->show();
  usedSendingAddressesPage->raise();
  usedSendingAddressesPage->activateWindow();
}

void WalletView::usedReceivingAddresses() {
  if (!walletModel)
    return;

  usedReceivingAddressesPage->show();
  usedReceivingAddressesPage->raise();
  usedReceivingAddressesPage->activateWindow();
}

void WalletView::showProgress(const QString &title, int nProgress) {
  if (nProgress == 0) {
    progressDialog = new QProgressDialog(title, "", 0, 100);
    progressDialog->setWindowModality(Qt::ApplicationModal);
    progressDialog->setMinimumDuration(0);
    progressDialog->setCancelButton(0);
    progressDialog->setAutoClose(false);
    progressDialog->setValue(0);
  } else if (nProgress == 100) {
    if (progressDialog) {
      progressDialog->close();
      progressDialog->deleteLater();
    }
  } else if (progressDialog)
    progressDialog->setValue(nProgress);
}

void WalletView::requestedSyncWarningInfo() {
  Q_EMIT outOfSyncWarningClicked();
}

void WalletView::doRescan(CWallet *pwallet, int64_t startTime) {
  WalletRescanReserver reserver(pwallet);
  if (!reserver.reserve()) {
    QMessageBox::critical(
        0, tr(PACKAGE_NAME),
        tr("Wallet is currently rescanning. Abort existing rescan or wait."));
    return;
  }
  pwallet->RescanFromTime(TIMESTAMP_MIN, reserver, true);
  QMessageBox::information(0, tr(PACKAGE_NAME), tr("Rescan complete."));
}

void WalletView::importPrivateKey() {
  bool ok;
  QString privKey =
      QInputDialog::getText(0, tr(PACKAGE_NAME),
                            tr("Enter a Litecoin/Litecoin Cash private key to "
                               "import into your wallet."),
                            QLineEdit::Normal, "", &ok);
  if (ok && !privKey.isEmpty()) {
    CWallet *pwallet = GetWalletForQTKeyImport();

    if (!pwallet) {
      QMessageBox::critical(0, tr(PACKAGE_NAME),
                            tr("Couldn't select valid wallet."));
      return;
    }

    if (!EnsureWalletIsAvailable(pwallet, false)) {
      QMessageBox::critical(0, tr(PACKAGE_NAME), tr("Wallet isn't open."));
      return;
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    WalletModel::UnlockContext ctx(walletModel->requestUnlock());
    if (!ctx.isValid())

      return;

    CBitcoinSecret vchSecret;
    if (!vchSecret.SetString(privKey.toStdString())) {
      QMessageBox::critical(
          0, tr(PACKAGE_NAME),
          tr("This doesn't appear to be a Litecoin/LitecoinCash private key."));
      return;
    }

    CKey key = vchSecret.GetKey();
    if (!key.IsValid()) {
      QMessageBox::critical(0, tr(PACKAGE_NAME),
                            tr("Private key outside allowed range."));
      return;
    }

    CPubKey pubkey = key.GetPubKey();
    assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();
    {
      pwallet->MarkDirty();
      pwallet->SetAddressBook(vchAddress, "", "receive");

      if (pwallet->HaveKey(vchAddress)) {
        QMessageBox::critical(0, tr(PACKAGE_NAME),
                              tr("This key has already been added."));
        return;
      }

      pwallet->mapKeyMetadata[vchAddress].nCreateTime = 1;

      if (!pwallet->AddKeyPubKey(key, pubkey)) {
        QMessageBox::critical(0, tr(PACKAGE_NAME),
                              tr("Error adding key to wallet."));
        return;
      }

      pwallet->UpdateTimeFirstKey(1);

      QMessageBox msgBox;
      msgBox.setText(tr("Key successfully added to wallet."));
      msgBox.setInformativeText(
          tr("Rescan now? (Select No if you have more keys to import)"));
      msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      msgBox.setDefaultButton(QMessageBox::No);

      if (msgBox.exec() == QMessageBox::Yes)
        boost::thread t{WalletView::doRescan, pwallet, TIMESTAMP_MIN};
    }
    return;
  }
}
