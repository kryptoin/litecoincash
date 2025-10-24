// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/coincontroldialog.h>
#include <qt/forms/ui_coincontroldialog.h>

#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/walletmodel.h>
#include <txmempool.h>

#include <init.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <validation.h>
#include <wallet/coincontrol.h>

#include <wallet/fees.h>
#include <wallet/wallet.h>

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFlags>
#include <QIcon>
#include <QSettings>
#include <QTreeWidget>

QList<CAmount> CoinControlDialog::payAmounts;
bool CoinControlDialog::fSubtractFeeFromAmount = false;

bool CCoinControlWidgetItem::operator<(const QTreeWidgetItem &other) const {
  int column = treeWidget()->sortColumn();
  if (column == CoinControlDialog::COLUMN_AMOUNT ||
      column == CoinControlDialog::COLUMN_DATE ||
      column == CoinControlDialog::COLUMN_CONFIRMATIONS)
    return data(column, Qt::UserRole).toLongLong() <
           other.data(column, Qt::UserRole).toLongLong();
  return QTreeWidgetItem::operator<(other);
}

CoinControlDialog::CoinControlDialog(const PlatformStyle *_platformStyle,
                                     QWidget *parent)
    : QDialog(parent), ui(new Ui::CoinControlDialog), model(0),
      platformStyle(_platformStyle) {
  ui->setupUi(this);

  QAction *copyAddressAction = new QAction(tr("Copy address"), this);
  QAction *copyLabelAction = new QAction(tr("Copy label"), this);
  QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
  copyTransactionHashAction = new QAction(tr("Copy transaction ID"), this);

  lockAction = new QAction(tr("Lock unspent"), this);

  unlockAction = new QAction(tr("Unlock unspent"), this);

  contextMenu = new QMenu(this);
  contextMenu->addAction(copyAddressAction);
  contextMenu->addAction(copyLabelAction);
  contextMenu->addAction(copyAmountAction);
  contextMenu->addAction(copyTransactionHashAction);
  contextMenu->addSeparator();
  contextMenu->addAction(lockAction);
  contextMenu->addAction(unlockAction);

  connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this,
          SLOT(showMenu(QPoint)));
  connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
  connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
  connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
  connect(copyTransactionHashAction, SIGNAL(triggered()), this,
          SLOT(copyTransactionHash()));
  connect(lockAction, SIGNAL(triggered()), this, SLOT(lockCoin()));
  connect(unlockAction, SIGNAL(triggered()), this, SLOT(unlockCoin()));

  QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
  QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
  QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
  QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
  QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
  QAction *clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
  QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);

  connect(clipboardQuantityAction, SIGNAL(triggered()), this,
          SLOT(clipboardQuantity()));
  connect(clipboardAmountAction, SIGNAL(triggered()), this,
          SLOT(clipboardAmount()));
  connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(clipboardFee()));
  connect(clipboardAfterFeeAction, SIGNAL(triggered()), this,
          SLOT(clipboardAfterFee()));
  connect(clipboardBytesAction, SIGNAL(triggered()), this,
          SLOT(clipboardBytes()));
  connect(clipboardLowOutputAction, SIGNAL(triggered()), this,
          SLOT(clipboardLowOutput()));
  connect(clipboardChangeAction, SIGNAL(triggered()), this,
          SLOT(clipboardChange()));

  ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
  ui->labelCoinControlAmount->addAction(clipboardAmountAction);
  ui->labelCoinControlFee->addAction(clipboardFeeAction);
  ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
  ui->labelCoinControlBytes->addAction(clipboardBytesAction);
  ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
  ui->labelCoinControlChange->addAction(clipboardChangeAction);

  connect(ui->radioTreeMode, SIGNAL(toggled(bool)), this,
          SLOT(radioTreeMode(bool)));
  connect(ui->radioListMode, SIGNAL(toggled(bool)), this,
          SLOT(radioListMode(bool)));

  connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this,
          SLOT(viewItemChanged(QTreeWidgetItem *, int)));

#if QT_VERSION < 0x050000
  ui->treeWidget->header()->setClickable(true);
#else
  ui->treeWidget->header()->setSectionsClickable(true);
#endif
  connect(ui->treeWidget->header(), SIGNAL(sectionClicked(int)), this,
          SLOT(headerSectionClicked(int)));

  connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton *)), this,
          SLOT(buttonBoxClicked(QAbstractButton *)));

  connect(ui->pushButtonSelectAll, SIGNAL(clicked()), this,
          SLOT(buttonSelectAllClicked()));

  ui->treeWidget->headerItem()->setText(COLUMN_CHECKBOX, QString());

  ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, 84);
  ui->treeWidget->setColumnWidth(COLUMN_AMOUNT, 110);
  ui->treeWidget->setColumnWidth(COLUMN_LABEL, 190);
  ui->treeWidget->setColumnWidth(COLUMN_ADDRESS, 320);
  ui->treeWidget->setColumnWidth(COLUMN_DATE, 130);
  ui->treeWidget->setColumnWidth(COLUMN_CONFIRMATIONS, 110);
  ui->treeWidget->setColumnHidden(COLUMN_TXHASH, true);

  ui->treeWidget->setColumnHidden(COLUMN_VOUT_INDEX, true);

  sortView(COLUMN_AMOUNT, Qt::DescendingOrder);

  QSettings settings;
  if (settings.contains("nCoinControlMode") &&
      !settings.value("nCoinControlMode").toBool())
    ui->radioTreeMode->click();
  if (settings.contains("nCoinControlSortColumn") &&
      settings.contains("nCoinControlSortOrder"))
    sortView(settings.value("nCoinControlSortColumn").toInt(),
             ((Qt::SortOrder)settings.value("nCoinControlSortOrder").toInt()));
}

CoinControlDialog::~CoinControlDialog() {
  QSettings settings;
  settings.setValue("nCoinControlMode", ui->radioListMode->isChecked());
  settings.setValue("nCoinControlSortColumn", sortColumn);
  settings.setValue("nCoinControlSortOrder", (int)sortOrder);

  delete ui;
}

void CoinControlDialog::setModel(WalletModel *_model) {
  this->model = _model;

  if (_model && _model->getOptionsModel() && _model->getAddressTableModel()) {
    updateView();
    updateLabelLocked();
    CoinControlDialog::updateLabels(_model, this);
  }
}

void CoinControlDialog::buttonBoxClicked(QAbstractButton *button) {
  if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
    done(QDialog::Accepted);
}

void CoinControlDialog::buttonSelectAllClicked() {
  Qt::CheckState state = Qt::Checked;
  for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
    if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) !=
        Qt::Unchecked) {
      state = Qt::Unchecked;
      break;
    }
  }
  ui->treeWidget->setEnabled(false);
  for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
    if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state)
      ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
  ui->treeWidget->setEnabled(true);
  if (state == Qt::Unchecked)
    coinControl()->UnSelectAll();

  CoinControlDialog::updateLabels(model, this);
}

void CoinControlDialog::showMenu(const QPoint &point) {
  QTreeWidgetItem *item = ui->treeWidget->itemAt(point);
  if (item) {
    contextMenuItem = item;

    if (item->text(COLUMN_TXHASH).length() == 64)

    {
      copyTransactionHashAction->setEnabled(true);
      if (model->isLockedCoin(uint256S(item->text(COLUMN_TXHASH).toStdString()),
                              item->text(COLUMN_VOUT_INDEX).toUInt())) {
        lockAction->setEnabled(false);
        unlockAction->setEnabled(true);
      } else {
        lockAction->setEnabled(true);
        unlockAction->setEnabled(false);
      }
    } else

    {
      copyTransactionHashAction->setEnabled(false);
      lockAction->setEnabled(false);
      unlockAction->setEnabled(false);
    }

    contextMenu->exec(QCursor::pos());
  }
}

void CoinControlDialog::copyAmount() {
  GUIUtil::setClipboard(
      BitcoinUnits::removeSpaces(contextMenuItem->text(COLUMN_AMOUNT)));
}

void CoinControlDialog::copyLabel() {
  if (ui->radioTreeMode->isChecked() &&
      contextMenuItem->text(COLUMN_LABEL).length() == 0 &&
      contextMenuItem->parent())
    GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_LABEL));
  else
    GUIUtil::setClipboard(contextMenuItem->text(COLUMN_LABEL));
}

void CoinControlDialog::copyAddress() {
  if (ui->radioTreeMode->isChecked() &&
      contextMenuItem->text(COLUMN_ADDRESS).length() == 0 &&
      contextMenuItem->parent())
    GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_ADDRESS));
  else
    GUIUtil::setClipboard(contextMenuItem->text(COLUMN_ADDRESS));
}

void CoinControlDialog::copyTransactionHash() {
  GUIUtil::setClipboard(contextMenuItem->text(COLUMN_TXHASH));
}

void CoinControlDialog::lockCoin() {
  if (contextMenuItem->checkState(COLUMN_CHECKBOX) == Qt::Checked)
    contextMenuItem->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

  COutPoint outpt(uint256S(contextMenuItem->text(COLUMN_TXHASH).toStdString()),
                  contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
  model->lockCoin(outpt);
  contextMenuItem->setDisabled(true);
  contextMenuItem->setIcon(
      COLUMN_CHECKBOX, platformStyle->SingleColorIcon(":/icons/lock_closed"));
  updateLabelLocked();
}

void CoinControlDialog::unlockCoin() {
  COutPoint outpt(uint256S(contextMenuItem->text(COLUMN_TXHASH).toStdString()),
                  contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
  model->unlockCoin(outpt);
  contextMenuItem->setDisabled(false);
  contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon());
  updateLabelLocked();
}

void CoinControlDialog::clipboardQuantity() {
  GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

void CoinControlDialog::clipboardAmount() {
  GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(
      ui->labelCoinControlAmount->text().indexOf(" ")));
}

void CoinControlDialog::clipboardFee() {
  GUIUtil::setClipboard(ui->labelCoinControlFee->text()
                            .left(ui->labelCoinControlFee->text().indexOf(" "))
                            .replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::clipboardAfterFee() {
  GUIUtil::setClipboard(
      ui->labelCoinControlAfterFee->text()
          .left(ui->labelCoinControlAfterFee->text().indexOf(" "))
          .replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::clipboardBytes() {
  GUIUtil::setClipboard(
      ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::clipboardLowOutput() {
  GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

void CoinControlDialog::clipboardChange() {
  GUIUtil::setClipboard(
      ui->labelCoinControlChange->text()
          .left(ui->labelCoinControlChange->text().indexOf(" "))
          .replace(ASYMP_UTF8, ""));
}

void CoinControlDialog::sortView(int column, Qt::SortOrder order) {
  sortColumn = column;
  sortOrder = order;
  ui->treeWidget->sortItems(column, order);
  ui->treeWidget->header()->setSortIndicator(sortColumn, sortOrder);
}

void CoinControlDialog::headerSectionClicked(int logicalIndex) {
  if (logicalIndex == COLUMN_CHECKBOX)

  {
    ui->treeWidget->header()->setSortIndicator(sortColumn, sortOrder);
  } else {
    if (sortColumn == logicalIndex)
      sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder
                                                     : Qt::AscendingOrder);
    else {
      sortColumn = logicalIndex;
      sortOrder = ((sortColumn == COLUMN_LABEL || sortColumn == COLUMN_ADDRESS)
                       ? Qt::AscendingOrder
                       : Qt::DescendingOrder);
    }

    sortView(sortColumn, sortOrder);
  }
}

void CoinControlDialog::radioTreeMode(bool checked) {
  if (checked && model)
    updateView();
}

void CoinControlDialog::radioListMode(bool checked) {
  if (checked && model)
    updateView();
}

void CoinControlDialog::viewItemChanged(QTreeWidgetItem *item, int column) {
  if (column == COLUMN_CHECKBOX && item->text(COLUMN_TXHASH).length() == 64)

  {
    COutPoint outpt(uint256S(item->text(COLUMN_TXHASH).toStdString()),
                    item->text(COLUMN_VOUT_INDEX).toUInt());

    if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked)
      coinControl()->UnSelect(outpt);
    else if (item->isDisabled())

      item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
    else
      coinControl()->Select(outpt);

    if (ui->treeWidget->isEnabled())

      CoinControlDialog::updateLabels(model, this);
  }

#if QT_VERSION >= 0x050000
  else if (column == COLUMN_CHECKBOX && item->childCount() > 0) {
    if (item->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked &&
        item->child(0)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
      item->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
  }
#endif
}

void CoinControlDialog::updateLabelLocked() {
  std::vector<COutPoint> vOutpts;
  model->listLockedCoins(vOutpts);
  if (vOutpts.size() > 0) {
    ui->labelLocked->setText(tr("(%1 locked)").arg(vOutpts.size()));
    ui->labelLocked->setVisible(true);
  } else
    ui->labelLocked->setVisible(false);
}

void CoinControlDialog::updateLabels(WalletModel *model, QDialog *dialog) {
  if (!model)
    return;

  CAmount nPayAmount = 0;
  bool fDust = false;
  CMutableTransaction txDummy;
  for (const CAmount &amount : CoinControlDialog::payAmounts) {
    nPayAmount += amount;

    if (amount > 0) {
      CTxOut txout(amount, (CScript)std::vector<unsigned char>(24, 0));
      txDummy.vout.push_back(txout);
      fDust |= IsDust(txout, ::dustRelayFee);
    }
  }

  CAmount nAmount = 0;
  CAmount nPayFee = 0;
  CAmount nAfterFee = 0;
  CAmount nChange = 0;
  unsigned int nBytes = 0;
  unsigned int nBytesInputs = 0;
  unsigned int nQuantity = 0;
  bool fWitness = false;

  std::vector<COutPoint> vCoinControl;
  std::vector<COutput> vOutputs;
  coinControl()->ListSelected(vCoinControl);
  model->getOutputs(vCoinControl, vOutputs);

  for (const COutput &out : vOutputs) {
    uint256 txhash = out.tx->GetHash();
    COutPoint outpt(txhash, out.i);
    if (model->isSpent(outpt)) {
      coinControl()->UnSelect(outpt);
      continue;
    }

    nQuantity++;

    nAmount += out.tx->tx->vout[out.i].nValue;

    CTxDestination address;
    int witnessversion = 0;
    std::vector<unsigned char> witnessprogram;
    if (out.tx->tx->vout[out.i].scriptPubKey.IsWitnessProgram(witnessversion,
                                                              witnessprogram)) {
      nBytesInputs += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
      fWitness = true;
    } else if (ExtractDestination(out.tx->tx->vout[out.i].scriptPubKey,
                                  address)) {
      CPubKey pubkey;
      CKeyID *keyid = boost::get<CKeyID>(&address);
      if (keyid && model->getPubKey(*keyid, pubkey)) {
        nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
      } else
        nBytesInputs += 148;

    } else
      nBytesInputs += 148;
  }

  if (nQuantity > 0) {
    nBytes = nBytesInputs +
             ((CoinControlDialog::payAmounts.size() > 0
                   ? CoinControlDialog::payAmounts.size() + 1
                   : 2) *
              34) +
             10;

    if (fWitness) {
      nBytes += 2;

      nBytes += nQuantity;
    }

    if (CoinControlDialog::fSubtractFeeFromAmount)
      if (nAmount - nPayAmount == 0)
        nBytes -= 34;

    nPayFee = GetMinimumFee(nBytes, *coinControl(), ::mempool, ::feeEstimator,
                            nullptr);

    if (nPayAmount > 0) {
      nChange = nAmount - nPayAmount;
      if (!CoinControlDialog::fSubtractFeeFromAmount)
        nChange -= nPayFee;

      if (nChange > 0 && nChange < MIN_CHANGE) {
        CTxOut txout(nChange, (CScript)std::vector<unsigned char>(24, 0));
        if (IsDust(txout, ::dustRelayFee)) {
          nPayFee += nChange;
          nChange = 0;
          if (CoinControlDialog::fSubtractFeeFromAmount)
            nBytes -= 34;
        }
      }

      if (nChange == 0 && !CoinControlDialog::fSubtractFeeFromAmount)
        nBytes -= 34;
    }

    nAfterFee = std::max<CAmount>(nAmount - nPayFee, 0);
  }

  int nDisplayUnit = BitcoinUnits::BTC;
  if (model && model->getOptionsModel())
    nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

  QLabel *l1 = dialog->findChild<QLabel *>("labelCoinControlQuantity");
  QLabel *l2 = dialog->findChild<QLabel *>("labelCoinControlAmount");
  QLabel *l3 = dialog->findChild<QLabel *>("labelCoinControlFee");
  QLabel *l4 = dialog->findChild<QLabel *>("labelCoinControlAfterFee");
  QLabel *l5 = dialog->findChild<QLabel *>("labelCoinControlBytes");
  QLabel *l7 = dialog->findChild<QLabel *>("labelCoinControlLowOutput");
  QLabel *l8 = dialog->findChild<QLabel *>("labelCoinControlChange");

  dialog->findChild<QLabel *>("labelCoinControlLowOutputText")
      ->setEnabled(nPayAmount > 0);
  dialog->findChild<QLabel *>("labelCoinControlLowOutput")
      ->setEnabled(nPayAmount > 0);
  dialog->findChild<QLabel *>("labelCoinControlChangeText")
      ->setEnabled(nPayAmount > 0);
  dialog->findChild<QLabel *>("labelCoinControlChange")
      ->setEnabled(nPayAmount > 0);

  l1->setText(QString::number(nQuantity));

  l2->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAmount));

  l3->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nPayFee));

  l4->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAfterFee));

  l5->setText(((nBytes > 0) ? ASYMP_UTF8 : "") + QString::number(nBytes));

  l7->setText(fDust ? tr("yes") : tr("no"));

  l8->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nChange));

  if (nPayFee > 0) {
    l3->setText(ASYMP_UTF8 + l3->text());
    l4->setText(ASYMP_UTF8 + l4->text());
    if (nChange > 0 && !CoinControlDialog::fSubtractFeeFromAmount)
      l8->setText(ASYMP_UTF8 + l8->text());
  }

  l7->setStyleSheet((fDust) ? "color:red;" : "");

  QString toolTipDust = tr("This label turns red if any recipient receives an "
                           "amount smaller than the current dust threshold.");

  double dFeeVary = (nBytes != 0) ? (double)nPayFee / nBytes : 0;

  QString toolTip4 = tr("Can vary +/- %1 satoshi(s) per input.").arg(dFeeVary);

  l3->setToolTip(toolTip4);
  l4->setToolTip(toolTip4);
  l7->setToolTip(toolTipDust);
  l8->setToolTip(toolTip4);
  dialog->findChild<QLabel *>("labelCoinControlFeeText")
      ->setToolTip(l3->toolTip());
  dialog->findChild<QLabel *>("labelCoinControlAfterFeeText")
      ->setToolTip(l4->toolTip());
  dialog->findChild<QLabel *>("labelCoinControlBytesText")
      ->setToolTip(l5->toolTip());
  dialog->findChild<QLabel *>("labelCoinControlLowOutputText")
      ->setToolTip(l7->toolTip());
  dialog->findChild<QLabel *>("labelCoinControlChangeText")
      ->setToolTip(l8->toolTip());

  QLabel *label = dialog->findChild<QLabel *>("labelCoinControlInsuffFunds");
  if (label)
    label->setVisible(nChange < 0);
}

CCoinControl *CoinControlDialog::coinControl() {
  static CCoinControl coin_control;
  return &coin_control;
}

void CoinControlDialog::updateView() {
  if (!model || !model->getOptionsModel() || !model->getAddressTableModel())
    return;

  bool treeMode = ui->radioTreeMode->isChecked();

  ui->treeWidget->clear();
  ui->treeWidget->setEnabled(false);

  ui->treeWidget->setAlternatingRowColors(!treeMode);
  QFlags<Qt::ItemFlag> flgCheckbox =
      Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
  QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                                     Qt::ItemIsUserCheckable |
                                     Qt::ItemIsTristate;

  int nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

  std::map<QString, std::vector<COutput>> mapCoins;
  model->listCoins(mapCoins);

  for (const std::pair<QString, std::vector<COutput>> &coins : mapCoins) {
    CCoinControlWidgetItem *itemWalletAddress = new CCoinControlWidgetItem();
    itemWalletAddress->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
    QString sWalletAddress = coins.first;
    QString sWalletLabel =
        model->getAddressTableModel()->labelForAddress(sWalletAddress);
    if (sWalletLabel.isEmpty())
      sWalletLabel = tr("(no label)");

    if (treeMode) {
      ui->treeWidget->addTopLevelItem(itemWalletAddress);

      itemWalletAddress->setFlags(flgTristate);
      itemWalletAddress->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

      itemWalletAddress->setText(COLUMN_LABEL, sWalletLabel);

      itemWalletAddress->setText(COLUMN_ADDRESS, sWalletAddress);
    }

    CAmount nSum = 0;
    int nChildren = 0;
    for (const COutput &out : coins.second) {
      nSum += out.tx->tx->vout[out.i].nValue;
      nChildren++;

      CCoinControlWidgetItem *itemOutput;
      if (treeMode)
        itemOutput = new CCoinControlWidgetItem(itemWalletAddress);
      else
        itemOutput = new CCoinControlWidgetItem(ui->treeWidget);
      itemOutput->setFlags(flgCheckbox);
      itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

      CTxDestination outputAddress;
      QString sAddress = "";
      if (ExtractDestination(out.tx->tx->vout[out.i].scriptPubKey,
                             outputAddress)) {
        sAddress = QString::fromStdString(EncodeDestination(outputAddress));

        if (!treeMode || (!(sAddress == sWalletAddress)))
          itemOutput->setText(COLUMN_ADDRESS, sAddress);
      }

      if (!(sAddress == sWalletAddress))

      {
        itemOutput->setToolTip(
            COLUMN_LABEL,
            tr("change from %1 (%2)").arg(sWalletLabel).arg(sWalletAddress));
        itemOutput->setText(COLUMN_LABEL, tr("(change)"));
      } else if (!treeMode) {
        QString sLabel =
            model->getAddressTableModel()->labelForAddress(sAddress);
        if (sLabel.isEmpty())
          sLabel = tr("(no label)");
        itemOutput->setText(COLUMN_LABEL, sLabel);
      }

      itemOutput->setText(
          COLUMN_AMOUNT,
          BitcoinUnits::format(nDisplayUnit, out.tx->tx->vout[out.i].nValue));
      itemOutput->setData(COLUMN_AMOUNT, Qt::UserRole,
                          QVariant((qlonglong)out.tx->tx->vout[out.i].nValue));

      itemOutput->setText(COLUMN_DATE,
                          GUIUtil::dateTimeStr(out.tx->GetTxTime()));
      itemOutput->setData(COLUMN_DATE, Qt::UserRole,
                          QVariant((qlonglong)out.tx->GetTxTime()));

      itemOutput->setText(COLUMN_CONFIRMATIONS, QString::number(out.nDepth));
      itemOutput->setData(COLUMN_CONFIRMATIONS, Qt::UserRole,
                          QVariant((qlonglong)out.nDepth));

      uint256 txhash = out.tx->GetHash();
      itemOutput->setText(COLUMN_TXHASH,
                          QString::fromStdString(txhash.GetHex()));

      itemOutput->setText(COLUMN_VOUT_INDEX, QString::number(out.i));

      if (model->isLockedCoin(txhash, out.i)) {
        COutPoint outpt(txhash, out.i);
        coinControl()->UnSelect(outpt);

        itemOutput->setDisabled(true);
        itemOutput->setIcon(COLUMN_CHECKBOX, platformStyle->SingleColorIcon(
                                                 ":/icons/lock_closed"));
      }

      if (coinControl()->IsSelected(COutPoint(txhash, out.i)))
        itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
    }

    if (treeMode) {
      itemWalletAddress->setText(COLUMN_CHECKBOX,
                                 "(" + QString::number(nChildren) + ")");
      itemWalletAddress->setText(COLUMN_AMOUNT,
                                 BitcoinUnits::format(nDisplayUnit, nSum));
      itemWalletAddress->setData(COLUMN_AMOUNT, Qt::UserRole,
                                 QVariant((qlonglong)nSum));
    }
  }

  if (treeMode) {
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
      if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) ==
          Qt::PartiallyChecked)
        ui->treeWidget->topLevelItem(i)->setExpanded(true);
  }

  sortView(sortColumn, sortOrder);
  ui->treeWidget->setEnabled(true);
}
