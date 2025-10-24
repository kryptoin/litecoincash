// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/bitcoingui.h>

#include <chainparams.h>
#include <fs.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/intro.h>
#include <qt/networkstyle.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/splashscreen.h>
#include <qt/utilitydialog.h>
#include <qt/winshutdownmonitor.h>

#ifdef ENABLE_WALLET
#include <qt/paymentserver.h>
#include <qt/walletmodel.h>
#endif

#include <init.h>
#include <rpc/server.h>
#include <ui_interface.h>
#include <util.h>
#include <warnings.h>

#ifdef ENABLE_WALLET
#include <wallet/wallet.h>
#endif

#include <stdint.h>

#include <boost/thread.hpp>

#include <QApplication>
#include <QDebug>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSettings>
#include <QSslConfiguration>
#include <QThread>
#include <QTimer>
#include <QTranslator>

#if defined(QT_STATICPLUGIN)
#include <QtPlugin>
#if QT_VERSION < 0x050000
Q_IMPORT_PLUGIN(qcncodecs)
Q_IMPORT_PLUGIN(qjpcodecs)
Q_IMPORT_PLUGIN(qtwcodecs)
Q_IMPORT_PLUGIN(qkrcodecs)
Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#else
#if QT_VERSION < 0x050400
Q_IMPORT_PLUGIN(AccessibleFactory)
#endif
#if defined(QT_QPA_PLATFORM_XCB)
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_WINDOWS)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#elif defined(QT_QPA_PLATFORM_COCOA)
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin);
#endif
#endif
#endif

#if QT_VERSION < 0x050000
#include <QTextCodec>
#endif

Q_DECLARE_METATYPE(bool *)
Q_DECLARE_METATYPE(CAmount)

static void InitMessage(const std::string &message) {
  LogPrintf("init message: %s\n", message);
}

static std::string Translate(const char *psz) {
  return QCoreApplication::translate("bitcoin-core", psz).toStdString();
}

static QString GetLangTerritory() {
  QSettings settings;

  QString lang_territory = QLocale::system().name();

  QString lang_territory_qsettings = settings.value("language", "").toString();
  if (!lang_territory_qsettings.isEmpty())
    lang_territory = lang_territory_qsettings;

  lang_territory = QString::fromStdString(
      gArgs.GetArg("-lang", lang_territory.toStdString()));
  return lang_territory;
}

static void initTranslations(QTranslator &qtTranslatorBase,
                             QTranslator &qtTranslator,
                             QTranslator &translatorBase,
                             QTranslator &translator) {
  QApplication::removeTranslator(&qtTranslatorBase);
  QApplication::removeTranslator(&qtTranslator);
  QApplication::removeTranslator(&translatorBase);
  QApplication::removeTranslator(&translator);

  QString lang_territory = GetLangTerritory();

  QString lang = lang_territory;
  lang.truncate(lang_territory.lastIndexOf('_'));

  if (qtTranslatorBase.load(
          "qt_" + lang, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    QApplication::installTranslator(&qtTranslatorBase);

  if (qtTranslator.load("qt_" + lang_territory,
                        QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
    QApplication::installTranslator(&qtTranslator);

  if (translatorBase.load(lang, ":/translations/"))
    QApplication::installTranslator(&translatorBase);

  if (translator.load(lang_territory, ":/translations/"))
    QApplication::installTranslator(&translator);
}

#if QT_VERSION < 0x050000
void DebugMessageHandler(QtMsgType type, const char *msg) {
  if (type == QtDebugMsg) {
    LogPrint(BCLog::QT, "GUI: %s\n", msg);
  } else {
    LogPrintf("GUI: %s\n", msg);
  }
}
#else
void DebugMessageHandler(QtMsgType type, const QMessageLogContext &context,
                         const QString &msg) {
  Q_UNUSED(context);
  if (type == QtDebugMsg) {
    LogPrint(BCLog::QT, "GUI: %s\n", msg.toStdString());
  } else {
    LogPrintf("GUI: %s\n", msg.toStdString());
  }
}
#endif

class BitcoinCore : public QObject {
  Q_OBJECT
public:
  explicit BitcoinCore();

  static bool baseInitialize();

public Q_SLOTS:
  void initialize();
  void shutdown();

Q_SIGNALS:
  void initializeResult(bool success);
  void shutdownResult();
  void runawayException(const QString &message);

private:
  void handleRunawayException(const std::exception *e);
};

class BitcoinApplication : public QApplication {
  Q_OBJECT
public:
  explicit BitcoinApplication(int &argc, char **argv);
  ~BitcoinApplication();

#ifdef ENABLE_WALLET

  void createPaymentServer();
#endif

  void parameterSetup();

  void createOptionsModel(bool resetSettings);

  void createWindow(const NetworkStyle *networkStyle);

  void createSplashScreen(const NetworkStyle *networkStyle);

  void requestInitialize();

  void requestShutdown();

  int getReturnValue() const { return returnValue; }

  WId getMainWinId() const;

public Q_SLOTS:
  void initializeResult(bool success);
  void shutdownResult();

  void handleRunawayException(const QString &message);

Q_SIGNALS:
  void requestedInitialize();
  void requestedShutdown();
  void stopThread();
  void splashFinished(QWidget *window);

private:
  QThread *coreThread;
  OptionsModel *optionsModel;
  ClientModel *clientModel;
  BitcoinGUI *window;
  QTimer *pollShutdownTimer;
#ifdef ENABLE_WALLET
  PaymentServer *paymentServer;
  WalletModel *walletModel;
#endif
  int returnValue;
  const PlatformStyle *platformStyle;
  std::unique_ptr<QWidget> shutdownWindow;

  void startThread();
};

#include <qt/bitcoin.moc>

BitcoinCore::BitcoinCore() : QObject() {}

void BitcoinCore::handleRunawayException(const std::exception *e) {
  PrintExceptionContinue(e, "Runaway exception");
  Q_EMIT runawayException(QString::fromStdString(GetWarnings("gui")));
}

bool BitcoinCore::baseInitialize() {
  if (!AppInitBasicSetup()) {
    return false;
  }
  if (!AppInitParameterInteraction()) {
    return false;
  }
  if (!AppInitSanityChecks()) {
    return false;
  }
  if (!AppInitLockDataDirectory()) {
    return false;
  }
  return true;
}

void BitcoinCore::initialize() {
  try {
    qDebug() << __func__ << ": Running initialization in thread";
    bool rv = AppInitMain();
    Q_EMIT initializeResult(rv);
  } catch (const std::exception &e) {
    handleRunawayException(&e);
  } catch (...) {
    handleRunawayException(nullptr);
  }
}

void BitcoinCore::shutdown() {
  try {
    qDebug() << __func__ << ": Running Shutdown in thread";
    Interrupt();
    Shutdown();
    qDebug() << __func__ << ": Shutdown finished";
    Q_EMIT shutdownResult();
  } catch (const std::exception &e) {
    handleRunawayException(&e);
  } catch (...) {
    handleRunawayException(nullptr);
  }
}

BitcoinApplication::BitcoinApplication(int &argc, char **argv)
    : QApplication(argc, argv), coreThread(0), optionsModel(0), clientModel(0),
      window(0), pollShutdownTimer(0),
#ifdef ENABLE_WALLET
      paymentServer(0), walletModel(0),
#endif
      returnValue(0) {
  setQuitOnLastWindowClosed(false);

  std::string platformName;
  platformName = gArgs.GetArg("-uiplatform", BitcoinGUI::DEFAULT_UIPLATFORM);
  platformStyle =
      PlatformStyle::instantiate(QString::fromStdString(platformName));
  if (!platformStyle)

    platformStyle = PlatformStyle::instantiate("other");
  assert(platformStyle);
}

BitcoinApplication::~BitcoinApplication() {
  if (coreThread) {
    qDebug() << __func__ << ": Stopping thread";
    Q_EMIT stopThread();
    coreThread->wait();
    qDebug() << __func__ << ": Stopped thread";
  }

  delete window;
  window = 0;
#ifdef ENABLE_WALLET
  delete paymentServer;
  paymentServer = 0;
#endif
  delete optionsModel;
  optionsModel = 0;
  delete platformStyle;
  platformStyle = 0;
}

#ifdef ENABLE_WALLET
void BitcoinApplication::createPaymentServer() {
  paymentServer = new PaymentServer(this);
}
#endif

void BitcoinApplication::createOptionsModel(bool resetSettings) {
  optionsModel = new OptionsModel(nullptr, resetSettings);
}

void BitcoinApplication::createWindow(const NetworkStyle *networkStyle) {
  window = new BitcoinGUI(platformStyle, networkStyle, 0);

  pollShutdownTimer = new QTimer(window);
  connect(pollShutdownTimer, SIGNAL(timeout()), window, SLOT(detectShutdown()));
}

void BitcoinApplication::createSplashScreen(const NetworkStyle *networkStyle) {
  SplashScreen *splash = new SplashScreen(0, networkStyle);

  splash->show();
  connect(this, SIGNAL(splashFinished(QWidget *)), splash,
          SLOT(slotFinish(QWidget *)));
  connect(this, SIGNAL(requestedShutdown()), splash, SLOT(close()));
}

void BitcoinApplication::startThread() {
  if (coreThread)
    return;
  coreThread = new QThread(this);
  BitcoinCore *executor = new BitcoinCore();
  executor->moveToThread(coreThread);

  connect(executor, SIGNAL(initializeResult(bool)), this,
          SLOT(initializeResult(bool)));
  connect(executor, SIGNAL(shutdownResult()), this, SLOT(shutdownResult()));
  connect(executor, SIGNAL(runawayException(QString)), this,
          SLOT(handleRunawayException(QString)));
  connect(this, SIGNAL(requestedInitialize()), executor, SLOT(initialize()));
  connect(this, SIGNAL(requestedShutdown()), executor, SLOT(shutdown()));

  connect(this, SIGNAL(stopThread()), executor, SLOT(deleteLater()));
  connect(this, SIGNAL(stopThread()), coreThread, SLOT(quit()));

  coreThread->start();
}

void BitcoinApplication::parameterSetup() {
  InitLogging();
  InitParameterInteraction();
}

void BitcoinApplication::requestInitialize() {
  qDebug() << __func__ << ": Requesting initialize";
  startThread();
  Q_EMIT requestedInitialize();
}

void BitcoinApplication::requestShutdown() {
  shutdownWindow.reset(ShutdownWindow::showShutdownWindow(window));

  qDebug() << __func__ << ": Requesting shutdown";
  startThread();
  window->hide();
  window->setClientModel(0);
  pollShutdownTimer->stop();

#ifdef ENABLE_WALLET
  window->removeAllWallets();
  delete walletModel;
  walletModel = 0;
#endif
  delete clientModel;
  clientModel = 0;

  StartShutdown();

  Q_EMIT requestedShutdown();
}

void BitcoinApplication::initializeResult(bool success) {
  qDebug() << __func__ << ": Initialization result: " << success;

  returnValue = success ? EXIT_SUCCESS : EXIT_FAILURE;
  if (success) {
    qWarning() << "Platform customization:" << platformStyle->getName();
#ifdef ENABLE_WALLET
    PaymentServer::LoadRootCAs();
    paymentServer->setOptionsModel(optionsModel);
#endif

    clientModel = new ClientModel(optionsModel);
    window->setClientModel(clientModel);

#ifdef ENABLE_WALLET

    if (!vpwallets.empty()) {
      walletModel = new WalletModel(platformStyle, vpwallets[0], optionsModel);

      window->addWallet(BitcoinGUI::DEFAULT_WALLET, walletModel);
      window->setCurrentWallet(BitcoinGUI::DEFAULT_WALLET);

      connect(walletModel,
              SIGNAL(coinsSent(CWallet *, SendCoinsRecipient, QByteArray)),
              paymentServer,
              SLOT(fetchPaymentACK(CWallet *, const SendCoinsRecipient &,
                                   QByteArray)));
    }
#endif

    if (gArgs.GetBoolArg("-min", false)) {
      window->showMinimized();
    } else {
      window->show();
    }
    Q_EMIT splashFinished(window);

#ifdef ENABLE_WALLET

    connect(paymentServer, SIGNAL(receivedPaymentRequest(SendCoinsRecipient)),
            window, SLOT(handlePaymentRequest(SendCoinsRecipient)));
    connect(window, SIGNAL(receivedURI(QString)), paymentServer,
            SLOT(handleURIOrFile(QString)));
    connect(paymentServer, SIGNAL(message(QString, QString, unsigned int)),
            window, SLOT(message(QString, QString, unsigned int)));
    QTimer::singleShot(100, paymentServer, SLOT(uiReady()));
#endif
    pollShutdownTimer->start(200);
  } else {
    Q_EMIT splashFinished(window);

    quit();
  }
}

void BitcoinApplication::shutdownResult() { quit(); }

void BitcoinApplication::handleRunawayException(const QString &message) {
  QMessageBox::critical(
      0, "Runaway exception",
      BitcoinGUI::tr("A fatal error occurred. LitecoinCash can no longer "
                     "continue safely and will quit.") +
          QString("\n\n") + message);
  ::exit(EXIT_FAILURE);
}

WId BitcoinApplication::getMainWinId() const {
  if (!window)
    return 0;

  return window->winId();
}

#ifndef BITCOIN_QT_TEST
int main(int argc, char *argv[]) {
  SetupEnvironment();

  gArgs.ParseParameters(argc, argv);

#if QT_VERSION < 0x050000

  QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
  QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());
#endif

  Q_INIT_RESOURCE(bitcoin);
  Q_INIT_RESOURCE(bitcoin_locale);

  BitcoinApplication app(argc, argv);
#if QT_VERSION > 0x050100

  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
#if QT_VERSION >= 0x050600
  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
#ifdef Q_OS_MAC
  QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
#if QT_VERSION >= 0x050500

  QSslConfiguration sslconf = QSslConfiguration::defaultConfiguration();
  sslconf.setProtocol(QSsl::TlsV1_0OrLater);
  QSslConfiguration::setDefaultConfiguration(sslconf);
#endif

  qRegisterMetaType<bool *>();

  qRegisterMetaType<CAmount>("CAmount");
  qRegisterMetaType<std::function<void(void)>>("std::function<void(void)>");

  QApplication::setOrganizationName(QAPP_ORG_NAME);
  QApplication::setOrganizationDomain(QAPP_ORG_DOMAIN);
  QApplication::setApplicationName(QAPP_APP_NAME_DEFAULT);
  GUIUtil::SubstituteFonts(GetLangTerritory());

  QTranslator qtTranslatorBase, qtTranslator, translatorBase, translator;
  initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);
  translationInterface.Translate.connect(Translate);

  if (gArgs.IsArgSet("-?") || gArgs.IsArgSet("-h") || gArgs.IsArgSet("-help") ||
      gArgs.IsArgSet("-version")) {
    HelpMessageDialog help(nullptr, gArgs.IsArgSet("-version"));
    help.showOrPrint();
    return EXIT_SUCCESS;
  }

  if (!Intro::pickDataDirectory())
    return EXIT_SUCCESS;

  if (!fs::is_directory(GetDataDir(false))) {
    QMessageBox::critical(
        0, QObject::tr(PACKAGE_NAME),
        QObject::tr("Error: Specified data directory \"%1\" does not exist.")
            .arg(QString::fromStdString(gArgs.GetArg("-datadir", ""))));
    return EXIT_FAILURE;
  }
  try {
    gArgs.ReadConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));
  } catch (const std::exception &e) {
    QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
                          QObject::tr("Error: Cannot parse configuration file: "
                                      "%1. Only use key=value syntax.")
                              .arg(e.what()));
    return EXIT_FAILURE;
  }

  try {
    SelectParams(ChainNameFromCommandLine());
  } catch (std::exception &e) {
    QMessageBox::critical(0, QObject::tr(PACKAGE_NAME),
                          QObject::tr("Error: %1").arg(e.what()));
    return EXIT_FAILURE;
  }
#ifdef ENABLE_WALLET

  PaymentServer::ipcParseCommandLine(argc, argv);
#endif

  QScopedPointer<const NetworkStyle> networkStyle(NetworkStyle::instantiate(
      QString::fromStdString(Params().NetworkIDString())));
  assert(!networkStyle.isNull());

  QApplication::setApplicationName(networkStyle->getAppName());

  initTranslations(qtTranslatorBase, qtTranslator, translatorBase, translator);

#ifdef ENABLE_WALLET

  if (PaymentServer::ipcSendCommandLine())
    exit(EXIT_SUCCESS);

  app.createPaymentServer();
#endif

  app.installEventFilter(
      new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));
#if QT_VERSION < 0x050000

  qInstallMsgHandler(DebugMessageHandler);
#else
#if defined(Q_OS_WIN)

  qApp->installNativeEventFilter(new WinShutdownMonitor());
#endif

  qInstallMessageHandler(DebugMessageHandler);
#endif

  app.parameterSetup();

  app.createOptionsModel(gArgs.IsArgSet("-resetguisettings"));

  uiInterface.InitMessage.connect(InitMessage);

  if (gArgs.GetBoolArg("-splash", DEFAULT_SPLASHSCREEN) &&
      !gArgs.GetBoolArg("-min", false))
    app.createSplashScreen(networkStyle.data());

  int rv = EXIT_SUCCESS;
  try {
    app.createWindow(networkStyle.data());

    if (BitcoinCore::baseInitialize()) {
      app.requestInitialize();
#if defined(Q_OS_WIN) && QT_VERSION >= 0x050000
      WinShutdownMonitor::registerShutdownBlockReason(
          QObject::tr("%1 didn't yet exit safely...")
              .arg(QObject::tr(PACKAGE_NAME)),
          (HWND)app.getMainWinId());
#endif
      app.exec();
      app.requestShutdown();
      app.exec();
      rv = app.getReturnValue();
    } else {
      rv = EXIT_FAILURE;
    }
  } catch (const std::exception &e) {
    PrintExceptionContinue(&e, "Runaway exception");
    app.handleRunawayException(QString::fromStdString(GetWarnings("gui")));
  } catch (...) {
    PrintExceptionContinue(nullptr, "Runaway exception");
    app.handleRunawayException(QString::fromStdString(GetWarnings("gui")));
  }
  return rv;
}
#endif
