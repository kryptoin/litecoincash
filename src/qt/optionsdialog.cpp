// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/forms/ui_optionsdialog.h>
#include <qt/optionsdialog.h>

#include <qt/bitcoinunits.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

#include <validation.h>

#include <netbase.h>
#include <txdb.h>

#include <QDataWidgetMapper>
#include <QDir>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QTimer>

OptionsDialog::OptionsDialog(QWidget *parent, bool enableWallet)
    : QDialog(parent), ui(new Ui::OptionsDialog), model(0), mapper(0) {
  ui->setupUi(this);

  ui->databaseCache->setMinimum(nMinDbCache);
  ui->databaseCache->setMaximum(nMaxDbCache);
  ui->threadsScriptVerif->setMinimum(-GetNumCores());
  ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

  ui->hiveCheckThreads->setMaximum(GetNumVirtualCores());

#ifndef USE_UPNP
  ui->mapPortUpnp->setEnabled(false);
#endif

  ui->proxyIp->setEnabled(false);
  ui->proxyPort->setEnabled(false);
  ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));

  ui->proxyIpTor->setEnabled(false);
  ui->proxyPortTor->setEnabled(false);
  ui->proxyPortTor->setValidator(new QIntValidator(1, 65535, this));

  connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp,
          SLOT(setEnabled(bool)));
  connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort,
          SLOT(setEnabled(bool)));
  connect(ui->connectSocks, SIGNAL(toggled(bool)), this,
          SLOT(updateProxyValidationState()));

  connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyIpTor,
          SLOT(setEnabled(bool)));
  connect(ui->connectSocksTor, SIGNAL(toggled(bool)), ui->proxyPortTor,
          SLOT(setEnabled(bool)));
  connect(ui->connectSocksTor, SIGNAL(toggled(bool)), this,
          SLOT(updateProxyValidationState()));

#ifdef Q_OS_MAC

  ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWindow));
#endif

  if (!enableWallet) {
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabWallet));
  }

  QDir translations(":translations");

  ui->bitcoinAtStartup->setToolTip(
      ui->bitcoinAtStartup->toolTip().arg(tr(PACKAGE_NAME)));
  ui->bitcoinAtStartup->setText(
      ui->bitcoinAtStartup->text().arg(tr(PACKAGE_NAME)));

  ui->openBitcoinConfButton->setToolTip(
      ui->openBitcoinConfButton->toolTip().arg(tr(PACKAGE_NAME)));

  ui->lang->setToolTip(ui->lang->toolTip().arg(tr(PACKAGE_NAME)));
  ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
  for (const QString &langStr : translations.entryList()) {
    QLocale locale(langStr);

    if (langStr.contains("_")) {
#if QT_VERSION >= 0x040800

      ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") +
                            locale.nativeCountryName() + QString(" (") +
                            langStr + QString(")"),
                        QVariant(langStr));
#else

      ui->lang->addItem(QLocale::languageToString(locale.language()) +
                            QString(" - ") +
                            QLocale::countryToString(locale.country()) +
                            QString(" (") + langStr + QString(")"),
                        QVariant(langStr));
#endif
    } else {
#if QT_VERSION >= 0x040800

      ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr +
                            QString(")"),
                        QVariant(langStr));
#else

      ui->lang->addItem(QLocale::languageToString(locale.language()) +
                            QString(" (") + langStr + QString(")"),
                        QVariant(langStr));
#endif
    }
  }
#if QT_VERSION >= 0x040700
  ui->thirdPartyTxUrls->setPlaceholderText("https://example.com/tx/%s");
#endif

  ui->unit->setModel(new BitcoinUnits(this));

  mapper = new QDataWidgetMapper(this);
  mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
  mapper->setOrientation(Qt::Vertical);

  ui->proxyIp->setCheckValidator(new ProxyAddressValidator(parent));
  ui->proxyIpTor->setCheckValidator(new ProxyAddressValidator(parent));
  connect(ui->proxyIp, SIGNAL(validationDidChange(QValidatedLineEdit *)), this,
          SLOT(updateProxyValidationState()));
  connect(ui->proxyIpTor, SIGNAL(validationDidChange(QValidatedLineEdit *)),
          this, SLOT(updateProxyValidationState()));
  connect(ui->proxyPort, SIGNAL(textChanged(const QString &)), this,
          SLOT(updateProxyValidationState()));
  connect(ui->proxyPortTor, SIGNAL(textChanged(const QString &)), this,
          SLOT(updateProxyValidationState()));
}

OptionsDialog::~OptionsDialog() { delete ui; }

void OptionsDialog::setModel(OptionsModel *_model) {
  this->model = _model;

  if (_model) {
    if (_model->isRestartRequired())
      showRestartWarning(true);

    QString strLabel = _model->getOverriddenByCommandLine();
    if (strLabel.isEmpty())
      strLabel = tr("none");
    ui->overriddenByCommandLineLabel->setText(strLabel);

    mapper->setModel(_model);
    setMapper();
    mapper->toFirst();

    updateDefaultProxyNets();
  }

  connect(ui->databaseCache, SIGNAL(valueChanged(int)), this,
          SLOT(showRestartWarning()));
  connect(ui->threadsScriptVerif, SIGNAL(valueChanged(int)), this,
          SLOT(showRestartWarning()));

  connect(ui->spendZeroConfChange, SIGNAL(clicked(bool)), this,
          SLOT(showRestartWarning()));

  connect(ui->allowIncoming, SIGNAL(clicked(bool)), this,
          SLOT(showRestartWarning()));
  connect(ui->connectSocks, SIGNAL(clicked(bool)), this,
          SLOT(showRestartWarning()));
  connect(ui->connectSocksTor, SIGNAL(clicked(bool)), this,
          SLOT(showRestartWarning()));

  connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning()));
  connect(ui->thirdPartyTxUrls, SIGNAL(textChanged(const QString &)), this,
          SLOT(showRestartWarning()));
}

void OptionsDialog::setMapper() {
  mapper->addMapping(ui->bitcoinAtStartup, OptionsModel::StartAtStartup);
  mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
  mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);

  mapper->addMapping(ui->spendZeroConfChange,
                     OptionsModel::SpendZeroConfChange);
  mapper->addMapping(ui->coinControlFeatures,
                     OptionsModel::CoinControlFeatures);

  mapper->addMapping(ui->hiveCheckThreads, OptionsModel::HiveCheckThreads);
  mapper->addMapping(ui->hiveCheckDelay, OptionsModel::HiveCheckDelay);
  mapper->addMapping(ui->hiveCheckEarlyOut, OptionsModel::HiveCheckEarlyOut);

  mapper->addMapping(ui->hiveContribCF, OptionsModel::HiveContribCF);

  mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);
  mapper->addMapping(ui->allowIncoming, OptionsModel::Listen);

  mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
  mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
  mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);

  mapper->addMapping(ui->connectSocksTor, OptionsModel::ProxyUseTor);
  mapper->addMapping(ui->proxyIpTor, OptionsModel::ProxyIPTor);
  mapper->addMapping(ui->proxyPortTor, OptionsModel::ProxyPortTor);

#ifndef Q_OS_MAC
  mapper->addMapping(ui->hideTrayIcon, OptionsModel::HideTrayIcon);
  mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
  mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

  mapper->addMapping(ui->lang, OptionsModel::Language);
  mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
  mapper->addMapping(ui->thirdPartyTxUrls, OptionsModel::ThirdPartyTxUrls);
}

void OptionsDialog::setOkButtonState(bool fState) {
  ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_resetButton_clicked() {
  if (model) {
    QMessageBox::StandardButton btnRetVal = QMessageBox::question(
        this, tr("Confirm options reset"),
        tr("Client restart required to activate changes.") + "<br><br>" +
            tr("Client will be shut down. Do you want to proceed?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

    if (btnRetVal == QMessageBox::Cancel)
      return;

    model->Reset();
    QApplication::quit();
  }
}

void OptionsDialog::on_openBitcoinConfButton_clicked() {
  QMessageBox::information(
      this, tr("Configuration options"),
      tr("The configuration file is used to specify advanced user options "
         "which override GUI settings. "
         "Additionally, any command-line options will override this "
         "configuration file."));

  if (!GUIUtil::openBitcoinConf())
    QMessageBox::critical(this, tr("Error"),
                          tr("The configuration file could not be opened."));
}

void OptionsDialog::on_okButton_clicked() {
  mapper->submit();
  accept();
  updateDefaultProxyNets();
}

void OptionsDialog::on_cancelButton_clicked() { reject(); }

void OptionsDialog::on_hideTrayIcon_stateChanged(int fState) {
  if (fState) {
    ui->minimizeToTray->setChecked(false);
    ui->minimizeToTray->setEnabled(false);
  } else {
    ui->minimizeToTray->setEnabled(true);
  }
}

void OptionsDialog::showRestartWarning(bool fPersistent) {
  ui->statusLabel->setStyleSheet("QLabel { color: red; }");

  if (fPersistent) {
    ui->statusLabel->setText(
        tr("Client restart required to activate changes."));
  } else {
    ui->statusLabel->setText(tr("This change would require a client restart."));

    QTimer::singleShot(10000, this, SLOT(clearStatusLabel()));
  }
}

void OptionsDialog::clearStatusLabel() {
  ui->statusLabel->clear();
  if (model && model->isRestartRequired()) {
    showRestartWarning(true);
  }
}

void OptionsDialog::updateProxyValidationState() {
  QValidatedLineEdit *pUiProxyIp = ui->proxyIp;
  QValidatedLineEdit *otherProxyWidget =
      (pUiProxyIp == ui->proxyIpTor) ? ui->proxyIp : ui->proxyIpTor;
  if (pUiProxyIp->isValid() &&
      (!ui->proxyPort->isEnabled() || ui->proxyPort->text().toInt() > 0) &&
      (!ui->proxyPortTor->isEnabled() ||
       ui->proxyPortTor->text().toInt() > 0)) {
    setOkButtonState(otherProxyWidget->isValid());

    clearStatusLabel();
  } else {
    setOkButtonState(false);
    ui->statusLabel->setStyleSheet("QLabel { color: red; }");
    ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
  }
}

void OptionsDialog::updateDefaultProxyNets() {
  proxyType proxy;
  std::string strProxy;
  QString strDefaultProxyGUI;

  GetProxy(NET_IPV4, proxy);
  strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
  strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
  (strProxy == strDefaultProxyGUI.toStdString())
      ? ui->proxyReachIPv4->setChecked(true)
      : ui->proxyReachIPv4->setChecked(false);

  GetProxy(NET_IPV6, proxy);
  strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
  strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
  (strProxy == strDefaultProxyGUI.toStdString())
      ? ui->proxyReachIPv6->setChecked(true)
      : ui->proxyReachIPv6->setChecked(false);

  GetProxy(NET_TOR, proxy);
  strProxy = proxy.proxy.ToStringIP() + ":" + proxy.proxy.ToStringPort();
  strDefaultProxyGUI = ui->proxyIp->text() + ":" + ui->proxyPort->text();
  (strProxy == strDefaultProxyGUI.toStdString())
      ? ui->proxyReachTor->setChecked(true)
      : ui->proxyReachTor->setChecked(false);
}

ProxyAddressValidator::ProxyAddressValidator(QObject *parent)
    : QValidator(parent) {}

QValidator::State ProxyAddressValidator::validate(QString &input,
                                                  int &pos) const {
  Q_UNUSED(pos);

  CService serv(
      LookupNumeric(input.toStdString().c_str(), DEFAULT_GUI_PROXY_PORT));
  proxyType addrProxy = proxyType(serv, true);
  if (addrProxy.IsValid())
    return QValidator::Acceptable;

  return QValidator::Invalid;
}
