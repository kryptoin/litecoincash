// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/forms/ui_sendcoinsdialog.h>
#include <qt/sendcoinsdialog.h>

#include <qt/addresstablemodel.h>
#include <qt/bitcoinunits.h>
#include <qt/clientmodel.h>
#include <qt/coincontroldialog.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/sendcoinsentry.h>

#include <base58.h>
#include <chainparams.h>
#include <validation.h>
#include <wallet/coincontrol.h>

#include <policy/fees.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <wallet/fees.h>

#include <QFontMetrics>
#include <QScrollBar>
#include <QSettings>
#include <QTextDocument>

static const std::array<int, 9> confTargets = {
    {2, 4, 6, 12, 24, 48, 144, 504, 1008}};
int getConfTargetForIndex(int index) {
  if (index + 1 > static_cast<int>(confTargets.size())) {
    return confTargets.back();
  }
  if (index < 0) {
    return confTargets[0];
  }
  return confTargets[index];
}
int getIndexForConfTarget(int target) {
  for (unsigned int i = 0; i < confTargets.size(); i++) {
    if (confTargets[i] >= target) {
      return i;
    }
  }
  return confTargets.size() - 1;
}

SendCoinsDialog::SendCoinsDialog(const PlatformStyle *_platformStyle,
                                 QWidget *parent)
    : QDialog(parent), ui(new Ui::SendCoinsDialog), clientModel(0), model(0),
      fNewRecipientAllowed(true), fFeeMinimized(true),
      platformStyle(_platformStyle) {
  ui->setupUi(this);

  if (!_platformStyle->getImagesOnButtons()) {
    ui->addButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->sendButton->setIcon(QIcon());
  } else {
    ui->addButton->setIcon(_platformStyle->SingleColorIcon(":/icons/add"));
    ui->clearButton->setIcon(_platformStyle->SingleColorIcon(":/icons/remove"));
    ui->sendButton->setIcon(_platformStyle->SingleColorIcon(":/icons/send"));
  }

  GUIUtil::setupAddressWidget(ui->lineEditCoinControlChange, this);

  addEntry();

  connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
  connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));

  connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this,
          SLOT(coinControlButtonClicked()));
  connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this,
          SLOT(coinControlChangeChecked(int)));
  connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString &)),
          this, SLOT(coinControlChangeEdited(const QString &)));

  QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
  QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
  QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
  QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
  QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
  QAction *clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
  QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);
  connect(clipboardQuantityAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardQuantity()));
  connect(clipboardAmountAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardAmount()));
  connect(clipboardFeeAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardFee()));
  connect(clipboardAfterFeeAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardAfterFee()));
  connect(clipboardBytesAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardBytes()));
  connect(clipboardLowOutputAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardLowOutput()));
  connect(clipboardChangeAction, SIGNAL(triggered()), this,
          SLOT(coinControlClipboardChange()));
  ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
  ui->labelCoinControlAmount->addAction(clipboardAmountAction);
  ui->labelCoinControlFee->addAction(clipboardFeeAction);
  ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
  ui->labelCoinControlBytes->addAction(clipboardBytesAction);
  ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
  ui->labelCoinControlChange->addAction(clipboardChangeAction);

  QSettings settings;
  if (!settings.contains("fFeeSectionMinimized"))
    settings.setValue("fFeeSectionMinimized", true);
  if (!settings.contains("nFeeRadio") && settings.contains("nTransactionFee") &&
      settings.value("nTransactionFee").toLongLong() > 0)

    settings.setValue("nFeeRadio", 1);

  if (!settings.contains("nFeeRadio"))
    settings.setValue("nFeeRadio", 0);

  if (!settings.contains("nSmartFeeSliderPosition"))
    settings.setValue("nSmartFeeSliderPosition", 0);
  if (!settings.contains("nTransactionFee"))
    settings.setValue("nTransactionFee", (qint64)DEFAULT_TRANSACTION_FEE);
  if (!settings.contains("fPayOnlyMinFee"))
    settings.setValue("fPayOnlyMinFee", false);
  ui->groupFee->setId(ui->radioSmartFee, 0);
  ui->groupFee->setId(ui->radioCustomFee, 1);
  ui->groupFee
      ->button(
          (int)std::max(0, std::min(1, settings.value("nFeeRadio").toInt())))
      ->setChecked(true);
  ui->customFee->setValue(settings.value("nTransactionFee").toLongLong());
  ui->checkBoxMinimumFee->setChecked(settings.value("fPayOnlyMinFee").toBool());
  minimizeFeeSection(settings.value("fFeeSectionMinimized").toBool());
}

void SendCoinsDialog::setClientModel(ClientModel *_clientModel) {
  this->clientModel = _clientModel;

  if (_clientModel) {
    connect(_clientModel,
            SIGNAL(numBlocksChanged(int, QDateTime, double, bool)), this,
            SLOT(updateSmartFeeLabel()));
  }
}

void SendCoinsDialog::setModel(WalletModel *_model) {
  this->model = _model;

  if (_model && _model->getOptionsModel()) {
    for (int i = 0; i < ui->entries->count(); ++i) {
      SendCoinsEntry *entry =
          qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(i)->widget());
      if (entry) {
        entry->setModel(_model);
      }
    }

    setBalance(_model->getBalance(), _model->getUnconfirmedBalance(),
               _model->getImmatureBalance(), _model->getWatchBalance(),
               _model->getWatchUnconfirmedBalance(),
               _model->getWatchImmatureBalance());
    connect(
        _model,
        SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount,
                              CAmount)),
        this,
        SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));
    connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this,
            SLOT(updateDisplayUnit()));
    updateDisplayUnit();

    connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this,
            SLOT(coinControlUpdateLabels()));
    connect(_model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)),
            this, SLOT(coinControlFeatureChanged(bool)));
    ui->frameCoinControl->setVisible(
        _model->getOptionsModel()->getCoinControlFeatures());
    coinControlUpdateLabels();

    for (const int n : confTargets) {
      ui->confTargetSelector->addItem(
          tr("%1 (%2 blocks)")
              .arg(GUIUtil::formatNiceTimeOffset(
                  n * Params().GetConsensus().nPowTargetSpacing))
              .arg(n));
    }
    connect(ui->confTargetSelector, SIGNAL(currentIndexChanged(int)), this,
            SLOT(updateSmartFeeLabel()));
    connect(ui->confTargetSelector, SIGNAL(currentIndexChanged(int)), this,
            SLOT(coinControlUpdateLabels()));
    connect(ui->groupFee, SIGNAL(buttonClicked(int)), this,
            SLOT(updateFeeSectionControls()));
    connect(ui->groupFee, SIGNAL(buttonClicked(int)), this,
            SLOT(coinControlUpdateLabels()));
    connect(ui->customFee, SIGNAL(valueChanged()), this,
            SLOT(coinControlUpdateLabels()));
    connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this,
            SLOT(setMinimumFee()));
    connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this,
            SLOT(updateFeeSectionControls()));
    connect(ui->checkBoxMinimumFee, SIGNAL(stateChanged(int)), this,
            SLOT(coinControlUpdateLabels()));

    ui->customFee->setSingleStep(GetRequiredFee(1000));
    updateFeeSectionControls();
    updateMinFeeLabel();
    updateSmartFeeLabel();

    QSettings settings;
    if (settings.value("nSmartFeeSliderPosition").toInt() != 0) {
      int nConfirmTarget =
          25 - settings.value("nSmartFeeSliderPosition").toInt();

      settings.setValue("nConfTarget", nConfirmTarget);
      settings.remove("nSmartFeeSliderPosition");
    }
    if (settings.value("nConfTarget").toInt() == 0)
      ui->confTargetSelector->setCurrentIndex(
          getIndexForConfTarget(model->getDefaultConfirmTarget()));
    else
      ui->confTargetSelector->setCurrentIndex(
          getIndexForConfTarget(settings.value("nConfTarget").toInt()));
  }
}

SendCoinsDialog::~SendCoinsDialog() {
  QSettings settings;
  settings.setValue("fFeeSectionMinimized", fFeeMinimized);
  settings.setValue("nFeeRadio", ui->groupFee->checkedId());
  settings.setValue("nConfTarget", getConfTargetForIndex(
                                       ui->confTargetSelector->currentIndex()));
  settings.setValue("nTransactionFee", (qint64)ui->customFee->value());
  settings.setValue("fPayOnlyMinFee", ui->checkBoxMinimumFee->isChecked());

  delete ui;
}

void SendCoinsDialog::on_sendButton_clicked() {
  if (!model || !model->getOptionsModel())
    return;

  QList<SendCoinsRecipient> recipients;
  bool valid = true;

  for (int i = 0; i < ui->entries->count(); ++i) {
    SendCoinsEntry *entry =
        qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(i)->widget());
    if (entry) {
      if (entry->validate()) {
        recipients.append(entry->getValue());
      } else {
        valid = false;
      }
    }
  }

  if (!valid || recipients.isEmpty()) {
    return;
  }

  fNewRecipientAllowed = false;
  WalletModel::UnlockContext ctx(model->requestUnlock());
  if (!ctx.isValid()) {
    fNewRecipientAllowed = true;
    return;
  }

  WalletModelTransaction currentTransaction(recipients);
  WalletModel::SendCoinsReturn prepareStatus;

  CCoinControl ctrl;
  if (model->getOptionsModel()->getCoinControlFeatures())
    ctrl = *CoinControlDialog::coinControl();

  updateCoinControlState(ctrl);

  prepareStatus = model->prepareTransaction(currentTransaction, ctrl);

  processSendCoinsReturn(
      prepareStatus,
      BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                   currentTransaction.getTransactionFee()));

  if (prepareStatus.status != WalletModel::OK) {
    fNewRecipientAllowed = true;
    return;
  }

  CAmount txFee = currentTransaction.getTransactionFee();

  QStringList formatted;
  for (const SendCoinsRecipient &rcp : currentTransaction.getRecipients()) {
    QString amount =
        "<b>" + BitcoinUnits::formatHtmlWithUnit(
                    model->getOptionsModel()->getDisplayUnit(), rcp.amount);
    amount.append("</b>");

    QString address = "<span style='font-family: monospace;'>" + rcp.address;
    address.append("</span>");

    QString recipientElement;

    if (!rcp.paymentRequest.IsInitialized())

    {
      if (rcp.label.length() > 0)

      {
        recipientElement =
            tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.label));
        recipientElement.append(QString(" (%1)").arg(address));
      } else

      {
        recipientElement = tr("%1 to %2").arg(amount, address);
      }
    } else if (!rcp.authenticatedMerchant.isEmpty())

    {
      recipientElement =
          tr("%1 to %2")
              .arg(amount, GUIUtil::HtmlEscape(rcp.authenticatedMerchant));
    } else

    {
      recipientElement = tr("%1 to %2").arg(amount, address);
    }

    formatted.append(recipientElement);
  }

  QString questionString = tr("Are you sure you want to send?");
  questionString.append("<br /><br />%1");

  if (txFee > 0) {
    questionString.append("<hr /><span style='color:#aa0000;'>");
    questionString.append(BitcoinUnits::formatHtmlWithUnit(
        model->getOptionsModel()->getDisplayUnit(), txFee));
    questionString.append("</span> ");
    questionString.append(tr("added as transaction fee"));

    questionString.append(
        " (" +
        QString::number((double)currentTransaction.getTransactionSize() /
                        1000) +
        " kB)");
  }

  questionString.append("<hr />");
  CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
  QStringList alternativeUnits;
  for (BitcoinUnits::Unit u : BitcoinUnits::availableUnits()) {
    if (u != model->getOptionsModel()->getDisplayUnit())
      alternativeUnits.append(BitcoinUnits::formatHtmlWithUnit(u, totalAmount));
  }
  questionString.append(
      tr("Total Amount %1")
          .arg(BitcoinUnits::formatHtmlWithUnit(
              model->getOptionsModel()->getDisplayUnit(), totalAmount)));
  questionString.append(
      QString(
          "<span style='font-size:10pt;font-weight:normal;'><br />(=%1)</span>")
          .arg(alternativeUnits.join(" " + tr("or") + "<br />")));

  SendConfirmationDialog confirmationDialog(
      tr("Confirm send coins"), questionString.arg(formatted.join("<br />")),
      SEND_CONFIRM_DELAY, this);
  confirmationDialog.exec();
  QMessageBox::StandardButton retval =
      (QMessageBox::StandardButton)confirmationDialog.result();

  if (retval != QMessageBox::Yes) {
    fNewRecipientAllowed = true;
    return;
  }

  WalletModel::SendCoinsReturn sendStatus =
      model->sendCoins(currentTransaction);

  processSendCoinsReturn(sendStatus);

  if (sendStatus.status == WalletModel::OK) {
    accept();
    CoinControlDialog::coinControl()->UnSelectAll();
    coinControlUpdateLabels();
  }
  fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear() {
  while (ui->entries->count()) {
    ui->entries->takeAt(0)->widget()->deleteLater();
  }
  addEntry();

  updateTabsAndLabels();
}

void SendCoinsDialog::reject() { clear(); }

void SendCoinsDialog::accept() { clear(); }

SendCoinsEntry *SendCoinsDialog::addEntry() {
  SendCoinsEntry *entry = new SendCoinsEntry(platformStyle, this);
  entry->setModel(model);
  ui->entries->addWidget(entry);
  connect(entry, SIGNAL(removeEntry(SendCoinsEntry *)), this,
          SLOT(removeEntry(SendCoinsEntry *)));
  connect(entry, SIGNAL(useAvailableBalance(SendCoinsEntry *)), this,
          SLOT(useAvailableBalance(SendCoinsEntry *)));
  connect(entry, SIGNAL(payAmountChanged()), this,
          SLOT(coinControlUpdateLabels()));
  connect(entry, SIGNAL(subtractFeeFromAmountChanged()), this,
          SLOT(coinControlUpdateLabels()));

  entry->clear();
  entry->setFocus();
  ui->scrollAreaWidgetContents->resize(
      ui->scrollAreaWidgetContents->sizeHint());
  qApp->processEvents();
  QScrollBar *bar = ui->scrollArea->verticalScrollBar();
  if (bar)
    bar->setSliderPosition(bar->maximum());

  updateTabsAndLabels();
  return entry;
}

void SendCoinsDialog::updateTabsAndLabels() {
  setupTabChain(0);
  coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry *entry) {
  entry->hide();

  if (ui->entries->count() == 1)
    addEntry();

  entry->deleteLater();

  updateTabsAndLabels();
}

QWidget *SendCoinsDialog::setupTabChain(QWidget *prev) {
  for (int i = 0; i < ui->entries->count(); ++i) {
    SendCoinsEntry *entry =
        qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(i)->widget());
    if (entry) {
      prev = entry->setupTabChain(prev);
    }
  }
  QWidget::setTabOrder(prev, ui->sendButton);
  QWidget::setTabOrder(ui->sendButton, ui->clearButton);
  QWidget::setTabOrder(ui->clearButton, ui->addButton);
  return ui->addButton;
}

void SendCoinsDialog::setAddress(const QString &address) {
  SendCoinsEntry *entry = 0;

  if (ui->entries->count() == 1) {
    SendCoinsEntry *first =
        qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(0)->widget());
    if (first->isClear()) {
      entry = first;
    }
  }
  if (!entry) {
    entry = addEntry();
  }

  entry->setAddress(address);
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient &rv) {
  if (!fNewRecipientAllowed)
    return;

  SendCoinsEntry *entry = 0;

  if (ui->entries->count() == 1) {
    SendCoinsEntry *first =
        qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(0)->widget());
    if (first->isClear()) {
      entry = first;
    }
  }
  if (!entry) {
    entry = addEntry();
  }

  entry->setValue(rv);
  updateTabsAndLabels();
}

bool SendCoinsDialog::handlePaymentRequest(const SendCoinsRecipient &rv) {
  pasteEntry(rv);
  return true;
}

void SendCoinsDialog::setBalance(const CAmount &balance,
                                 const CAmount &unconfirmedBalance,
                                 const CAmount &immatureBalance,
                                 const CAmount &watchBalance,
                                 const CAmount &watchUnconfirmedBalance,
                                 const CAmount &watchImmatureBalance) {
  Q_UNUSED(unconfirmedBalance);
  Q_UNUSED(immatureBalance);
  Q_UNUSED(watchBalance);
  Q_UNUSED(watchUnconfirmedBalance);
  Q_UNUSED(watchImmatureBalance);

  if (model && model->getOptionsModel()) {
    ui->labelBalance->setText(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), balance));
  }
}

void SendCoinsDialog::updateDisplayUnit() {
  setBalance(model->getBalance(), 0, 0, 0, 0, 0);
  ui->customFee->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
  updateMinFeeLabel();
  updateSmartFeeLabel();
}

void SendCoinsDialog::processSendCoinsReturn(
    const WalletModel::SendCoinsReturn &sendCoinsReturn,
    const QString &msgArg) {
  QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;

  msgParams.second = CClientUIInterface::MSG_WARNING;

  switch (sendCoinsReturn.status) {
  case WalletModel::InvalidAddress:
    msgParams.first = tr("The recipient address is not valid. Please recheck.");
    break;
  case WalletModel::InvalidAmount:
    msgParams.first = tr("The amount to pay must be larger than 0.");
    break;
  case WalletModel::AmountExceedsBalance:
    msgParams.first = tr("The amount exceeds your balance.");
    break;
  case WalletModel::AmountWithFeeExceedsBalance:
    msgParams.first = tr("The total exceeds your balance when the %1 "
                         "transaction fee is included.")
                          .arg(msgArg);
    break;
  case WalletModel::DuplicateAddress:
    msgParams.first =
        tr("Duplicate address found: addresses should only be used once each.");
    break;
  case WalletModel::TransactionCreationFailed:
    msgParams.first = tr("Transaction creation failed!");
    msgParams.second = CClientUIInterface::MSG_ERROR;
    break;
  case WalletModel::TransactionCommitFailed:
    msgParams.first =
        tr("The transaction was rejected with the following reason: %1")
            .arg(sendCoinsReturn.reasonCommitFailed);
    msgParams.second = CClientUIInterface::MSG_ERROR;
    break;
  case WalletModel::AbsurdFee:
    msgParams.first =
        tr("A fee higher than %1 is considered an absurdly high fee.")
            .arg(BitcoinUnits::formatWithUnit(
                model->getOptionsModel()->getDisplayUnit(), maxTxFee));
    break;
  case WalletModel::PaymentRequestExpired:
    msgParams.first = tr("Payment request expired.");
    msgParams.second = CClientUIInterface::MSG_ERROR;
    break;

  case WalletModel::OK:
  default:
    return;
  }

  Q_EMIT message(tr("Send Coins"), msgParams.first, msgParams.second);
}

void SendCoinsDialog::minimizeFeeSection(bool fMinimize) {
  ui->labelFeeMinimized->setVisible(fMinimize);
  ui->buttonChooseFee->setVisible(fMinimize);
  ui->buttonMinimizeFee->setVisible(!fMinimize);
  ui->frameFeeSelection->setVisible(!fMinimize);
  ui->horizontalLayoutSmartFee->setContentsMargins(0, (fMinimize ? 0 : 6), 0,
                                                   0);
  fFeeMinimized = fMinimize;
}

void SendCoinsDialog::on_buttonChooseFee_clicked() {
  minimizeFeeSection(false);
}

void SendCoinsDialog::on_buttonMinimizeFee_clicked() {
  updateFeeMinimizedLabel();
  minimizeFeeSection(true);
}

void SendCoinsDialog::useAvailableBalance(SendCoinsEntry *entry) {
  CCoinControl coin_control;
  if (model->getOptionsModel()->getCoinControlFeatures()) {
    coin_control = *CoinControlDialog::coinControl();
  }

  CAmount amount = model->getBalance(&coin_control);
  for (int i = 0; i < ui->entries->count(); ++i) {
    SendCoinsEntry *e =
        qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(i)->widget());
    if (e && !e->isHidden() && e != entry) {
      amount -= e->getValue().amount;
    }
  }

  if (amount > 0) {
    entry->checkSubtractFeeFromAmount();
    entry->setAmount(amount);
  } else {
    entry->setAmount(0);
  }
}

void SendCoinsDialog::setMinimumFee() {
  ui->customFee->setValue(GetRequiredFee(1000));
}

void SendCoinsDialog::updateFeeSectionControls() {
  ui->confTargetSelector->setEnabled(ui->radioSmartFee->isChecked());
  ui->labelSmartFee->setEnabled(ui->radioSmartFee->isChecked());
  ui->labelSmartFee2->setEnabled(ui->radioSmartFee->isChecked());
  ui->labelSmartFee3->setEnabled(ui->radioSmartFee->isChecked());
  ui->labelFeeEstimation->setEnabled(ui->radioSmartFee->isChecked());
  ui->checkBoxMinimumFee->setEnabled(ui->radioCustomFee->isChecked());
  ui->labelMinFeeWarning->setEnabled(ui->radioCustomFee->isChecked());
  ui->labelCustomPerKilobyte->setEnabled(ui->radioCustomFee->isChecked() &&
                                         !ui->checkBoxMinimumFee->isChecked());
  ui->customFee->setEnabled(ui->radioCustomFee->isChecked() &&
                            !ui->checkBoxMinimumFee->isChecked());
}

void SendCoinsDialog::updateFeeMinimizedLabel() {
  if (!model || !model->getOptionsModel())
    return;

  if (ui->radioSmartFee->isChecked())
    ui->labelFeeMinimized->setText(ui->labelSmartFee->text());
  else {
    ui->labelFeeMinimized->setText(
        BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                     ui->customFee->value()) +
        "/kB");
  }
}

void SendCoinsDialog::updateMinFeeLabel() {
  if (model && model->getOptionsModel())
    ui->checkBoxMinimumFee->setText(
        tr("Pay only the required fee of %1")
            .arg(BitcoinUnits::formatWithUnit(
                     model->getOptionsModel()->getDisplayUnit(),
                     GetRequiredFee(1000)) +
                 "/kB"));
}

void SendCoinsDialog::updateCoinControlState(CCoinControl &ctrl) {
  if (ui->radioCustomFee->isChecked()) {
    ctrl.m_feerate = CFeeRate(ui->customFee->value());
  } else {
    ctrl.m_feerate.reset();
  }

  ctrl.m_confirm_target =
      getConfTargetForIndex(ui->confTargetSelector->currentIndex());
}

void SendCoinsDialog::updateSmartFeeLabel() {
  if (!model || !model->getOptionsModel())
    return;
  CCoinControl coin_control;
  updateCoinControlState(coin_control);
  coin_control.m_feerate.reset();

  FeeCalculation feeCalc;
  CFeeRate feeRate = CFeeRate(
      GetMinimumFee(1000, coin_control, ::mempool, ::feeEstimator, &feeCalc));

  ui->labelSmartFee->setText(
      BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(),
                                   feeRate.GetFeePerK()) +
      "/kB");

  if (feeCalc.reason == FeeReason::FALLBACK) {
    ui->labelSmartFee2->show();

    ui->labelFeeEstimation->setText("");
    ui->fallbackFeeWarningLabel->setVisible(true);
    int lightness = ui->fallbackFeeWarningLabel->palette()
                        .color(QPalette::WindowText)
                        .lightness();
    QColor warning_colour(255 - (lightness / 5), 176 - (lightness / 3),
                          48 - (lightness / 14));
    ui->fallbackFeeWarningLabel->setStyleSheet(
        "QLabel { color: " + warning_colour.name() + "; }");
    ui->fallbackFeeWarningLabel->setIndent(
        QFontMetrics(ui->fallbackFeeWarningLabel->font()).width("x"));
  } else {
    ui->labelSmartFee2->hide();
    ui->labelFeeEstimation->setText(
        tr("Estimated to begin confirmation within %n block(s).", "",
           feeCalc.returnedTarget));
    ui->fallbackFeeWarningLabel->setVisible(false);
  }

  updateFeeMinimizedLabel();
}

void SendCoinsDialog::coinControlClipboardQuantity() {
  GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

void SendCoinsDialog::coinControlClipboardAmount() {
  GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(
      ui->labelCoinControlAmount->text().indexOf(" ")));
}

void SendCoinsDialog::coinControlClipboardFee() {
  GUIUtil::setClipboard(ui->labelCoinControlFee->text()
                            .left(ui->labelCoinControlFee->text().indexOf(" "))
                            .replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlClipboardAfterFee() {
  GUIUtil::setClipboard(
      ui->labelCoinControlAfterFee->text()
          .left(ui->labelCoinControlAfterFee->text().indexOf(" "))
          .replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlClipboardBytes() {
  GUIUtil::setClipboard(
      ui->labelCoinControlBytes->text().replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlClipboardLowOutput() {
  GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

void SendCoinsDialog::coinControlClipboardChange() {
  GUIUtil::setClipboard(
      ui->labelCoinControlChange->text()
          .left(ui->labelCoinControlChange->text().indexOf(" "))
          .replace(ASYMP_UTF8, ""));
}

void SendCoinsDialog::coinControlFeatureChanged(bool checked) {
  ui->frameCoinControl->setVisible(checked);

  if (!checked && model)

    CoinControlDialog::coinControl()->SetNull();

  coinControlUpdateLabels();
}

void SendCoinsDialog::coinControlButtonClicked() {
  CoinControlDialog dlg(platformStyle);
  dlg.setModel(model);
  dlg.exec();
  coinControlUpdateLabels();
}

void SendCoinsDialog::coinControlChangeChecked(int state) {
  if (state == Qt::Unchecked) {
    CoinControlDialog::coinControl()->destChange = CNoDestination();
    ui->labelCoinControlChangeLabel->clear();
  } else

    coinControlChangeEdited(ui->lineEditCoinControlChange->text());

  ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
}

void SendCoinsDialog::coinControlChangeEdited(const QString &text) {
  if (model && model->getAddressTableModel()) {
    CoinControlDialog::coinControl()->destChange = CNoDestination();
    ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:red;}");

    const CTxDestination dest = DecodeDestination(text.toStdString());

    if (text.isEmpty())

    {
      ui->labelCoinControlChangeLabel->setText("");
    } else if (!IsValidDestination(dest))

    {
      ui->labelCoinControlChangeLabel->setText(
          tr("Warning: Invalid LitecoinCash address"));
    } else

    {
      if (!model->IsSpendable(dest)) {
        ui->labelCoinControlChangeLabel->setText(
            tr("Warning: Unknown change address"));

        QMessageBox::StandardButton btnRetVal = QMessageBox::question(
            this, tr("Confirm custom change address"),
            tr("The address you selected for change is not part of this "
               "wallet. Any or all funds in your wallet may be sent to this "
               "address. Are you sure?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if (btnRetVal == QMessageBox::Yes)
          CoinControlDialog::coinControl()->destChange = dest;
        else {
          ui->lineEditCoinControlChange->setText("");
          ui->labelCoinControlChangeLabel->setStyleSheet(
              "QLabel{color:black;}");
          ui->labelCoinControlChangeLabel->setText("");
        }
      } else

      {
        ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");

        QString associatedLabel =
            model->getAddressTableModel()->labelForAddress(text);
        if (!associatedLabel.isEmpty())
          ui->labelCoinControlChangeLabel->setText(associatedLabel);
        else
          ui->labelCoinControlChangeLabel->setText(tr("(no label)"));

        CoinControlDialog::coinControl()->destChange = dest;
      }
    }
  }
}

void SendCoinsDialog::coinControlUpdateLabels() {
  if (!model || !model->getOptionsModel())
    return;

  updateCoinControlState(*CoinControlDialog::coinControl());

  CoinControlDialog::payAmounts.clear();
  CoinControlDialog::fSubtractFeeFromAmount = false;

  for (int i = 0; i < ui->entries->count(); ++i) {
    SendCoinsEntry *entry =
        qobject_cast<SendCoinsEntry *>(ui->entries->itemAt(i)->widget());
    if (entry && !entry->isHidden()) {
      SendCoinsRecipient rcp = entry->getValue();
      CoinControlDialog::payAmounts.append(rcp.amount);
      if (rcp.fSubtractFeeFromAmount)
        CoinControlDialog::fSubtractFeeFromAmount = true;
    }
  }

  if (CoinControlDialog::coinControl()->HasSelected()) {
    CoinControlDialog::updateLabels(model, this);

    ui->labelCoinControlAutomaticallySelected->hide();
    ui->widgetCoinControl->show();
  } else {
    ui->labelCoinControlAutomaticallySelected->show();
    ui->widgetCoinControl->hide();
    ui->labelCoinControlInsuffFunds->hide();
  }
}

SendConfirmationDialog::SendConfirmationDialog(const QString &title,
                                               const QString &text,
                                               int _secDelay, QWidget *parent)
    : QMessageBox(QMessageBox::Question, title, text,
                  QMessageBox::Yes | QMessageBox::Cancel, parent),
      secDelay(_secDelay) {
  setDefaultButton(QMessageBox::Cancel);
  yesButton = button(QMessageBox::Yes);
  updateYesButton();
  connect(&countDownTimer, SIGNAL(timeout()), this, SLOT(countDown()));
}

int SendConfirmationDialog::exec() {
  updateYesButton();
  countDownTimer.start(1000);
  return QMessageBox::exec();
}

void SendConfirmationDialog::countDown() {
  secDelay--;
  updateYesButton();

  if (secDelay <= 0) {
    countDownTimer.stop();
  }
}

void SendConfirmationDialog::updateYesButton() {
  if (secDelay > 0) {
    yesButton->setEnabled(false);
    yesButton->setText(tr("Yes") + " (" + QString::number(secDelay) + ")");
  } else {
    yesButton->setEnabled(true);
    yesButton->setText(tr("Yes"));
  }
}
