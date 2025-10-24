// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/guiutil.h>

#include <qt/bitcoinaddressvalidator.h>
#include <qt/bitcoinunits.h>
#include <qt/qvalidatedlineedit.h>
#include <qt/walletmodel.h>

#include <init.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <script/script.h>
#include <script/standard.h>
#include <util.h>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif

#include <boost/scoped_array.hpp>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFont>
#include <QLineEdit>
#include <QSettings>
#include <QTextDocument>

#include <QMouseEvent>
#include <QThread>

#if QT_VERSION < 0x050000
#include <QUrl>
#else
#include <QUrlQuery>
#endif

#if QT_VERSION >= 0x50200
#include <QFontDatabase>
#endif

static fs::detail::utf8_codecvt_facet utf8;

#if defined(Q_OS_MAC)
extern double NSAppKitVersionNumber;
#if !defined(NSAppKitVersionNumber10_8)
#define NSAppKitVersionNumber10_8 1187
#endif
#if !defined(NSAppKitVersionNumber10_9)
#define NSAppKitVersionNumber10_9 1265
#endif
#endif

namespace GUIUtil {
QString dateTimeStr(const QDateTime &date) {
  return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") +
         date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime) {
  return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

QFont fixedPitchFont() {
#if QT_VERSION >= 0x50200
  return QFontDatabase::systemFont(QFontDatabase::FixedFont);
#else
  QFont font("Monospace");
#if QT_VERSION >= 0x040800
  font.setStyleHint(QFont::Monospace);
#else
  font.setStyleHint(QFont::TypeWriter);
#endif
  return font;
#endif
}

static const uint8_t dummydata[] = {
    0xeb, 0x15, 0x23, 0x1d, 0xfc, 0xeb, 0x60, 0x92, 0x58, 0x86, 0xb6, 0x7d,
    0x06, 0x52, 0x99, 0x92, 0x59, 0x15, 0xae, 0xb1, 0x72, 0xc0, 0x66, 0x47};

static std::string DummyAddress(const CChainParams &params) {
  std::vector<unsigned char> sourcedata =
      params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
  sourcedata.insert(sourcedata.end(), dummydata, dummydata + sizeof(dummydata));
  for (int i = 0; i < 256; ++i) {
    std::string s =
        EncodeBase58(sourcedata.data(), sourcedata.data() + sourcedata.size());
    if (!IsValidDestinationString(s)) {
      return s;
    }
    sourcedata[sourcedata.size() - 1] += 1;
  }
  return "";
}

void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent) {
  parent->setFocusProxy(widget);

  widget->setFont(fixedPitchFont());
#if QT_VERSION >= 0x040700

  widget->setPlaceholderText(
      QObject::tr("Enter a LitecoinCash address (e.g. %1)")
          .arg(QString::fromStdString(DummyAddress(Params()))));
#endif
  widget->setValidator(new BitcoinAddressEntryValidator(parent));
  widget->setCheckValidator(new BitcoinAddressCheckValidator(parent));
}

void setupAmountWidget(QLineEdit *widget, QWidget *parent) {
  QDoubleValidator *amountValidator = new QDoubleValidator(parent);
  amountValidator->setDecimals(8);
  amountValidator->setBottom(0.0);
  widget->setValidator(amountValidator);
  widget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out) {
  if (!uri.isValid() || uri.scheme() != QString("litecoincash"))
    return false;

  SendCoinsRecipient rv;
  rv.address = uri.path();

  if (rv.address.endsWith("/")) {
    rv.address.truncate(rv.address.length() - 1);
  }
  rv.amount = 0;

#if QT_VERSION < 0x050000
  QList<QPair<QString, QString>> items = uri.queryItems();
#else
  QUrlQuery uriQuery(uri);
  QList<QPair<QString, QString>> items = uriQuery.queryItems();
#endif
  for (QList<QPair<QString, QString>>::iterator i = items.begin();
       i != items.end(); i++) {
    bool fShouldReturnFalse = false;
    if (i->first.startsWith("req-")) {
      i->first.remove(0, 4);
      fShouldReturnFalse = true;
    }

    if (i->first == "label") {
      rv.label = i->second;
      fShouldReturnFalse = false;
    }
    if (i->first == "message") {
      rv.message = i->second;
      fShouldReturnFalse = false;
    } else if (i->first == "amount") {
      if (!i->second.isEmpty()) {
        if (!BitcoinUnits::parse(BitcoinUnits::BTC, i->second, &rv.amount)) {
          return false;
        }
      }
      fShouldReturnFalse = false;
    }

    if (fShouldReturnFalse)
      return false;
  }
  if (out) {
    *out = rv;
  }
  return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient *out) {
    if(uri.startsWith("litecoincash:

    {
    uri.replace(0, 15, "litecoincash:");    

    }
    QUrl uriInstance(uri);
    return parseBitcoinURI(uriInstance, out);
}

QString formatBitcoinURI(const SendCoinsRecipient &info) {
  QString ret = QString("litecoincash:%1").arg(info.address);
  int paramCount = 0;

  if (info.amount) {
    ret += QString("?amount=%1")
               .arg(BitcoinUnits::format(BitcoinUnits::BTC, info.amount, false,
                                         BitcoinUnits::separatorNever));
    paramCount++;
  }

  if (!info.label.isEmpty()) {
    QString lbl(QUrl::toPercentEncoding(info.label));
    ret += QString("%1label=%2").arg(paramCount == 0 ? "?" : "&").arg(lbl);
    paramCount++;
  }

  if (!info.message.isEmpty()) {
    QString msg(QUrl::toPercentEncoding(info.message));
    ret += QString("%1message=%2").arg(paramCount == 0 ? "?" : "&").arg(msg);
    paramCount++;
  }

  return ret;
}

bool isDust(const QString &address, const CAmount &amount) {
  CTxDestination dest = DecodeDestination(address.toStdString());
  CScript script = GetScriptForDestination(dest);
  CTxOut txOut(amount, script);
  return IsDust(txOut, ::dustRelayFee);
}

QString HtmlEscape(const QString &str, bool fMultiLine) {
#if QT_VERSION < 0x050000
  QString escaped = Qt::escape(str);
#else
  QString escaped = str.toHtmlEscaped();
#endif
  if (fMultiLine) {
    escaped = escaped.replace("\n", "<br>\n");
  }
  return escaped;
}

QString HtmlEscape(const std::string &str, bool fMultiLine) {
  return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView *view, int column, int role) {
  if (!view || !view->selectionModel())
    return;
  QModelIndexList selection = view->selectionModel()->selectedRows(column);

  if (!selection.isEmpty()) {
    setClipboard(selection.at(0).data(role).toString());
  }
}

QList<QModelIndex> getEntryData(QAbstractItemView *view, int column) {
  if (!view || !view->selectionModel())
    return QList<QModelIndex>();
  return view->selectionModel()->selectedRows(column);
}

QString getSaveFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut) {
  QString selectedFilter;
  QString myDir;
  if (dir.isEmpty())

  {
#if QT_VERSION < 0x050000
    myDir =
        QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
    myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
  } else {
    myDir = dir;
  }

  QString result = QDir::toNativeSeparators(QFileDialog::getSaveFileName(
      parent, caption, myDir, filter, &selectedFilter));

  QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
  QString selectedSuffix;
  if (filter_re.exactMatch(selectedFilter)) {
    selectedSuffix = filter_re.cap(1);
  }

  QFileInfo info(result);
  if (!result.isEmpty()) {
    if (info.suffix().isEmpty() && !selectedSuffix.isEmpty()) {
      if (!result.endsWith("."))
        result.append(".");
      result.append(selectedSuffix);
    }
  }

  if (selectedSuffixOut) {
    *selectedSuffixOut = selectedSuffix;
  }
  return result;
}

QString getOpenFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut) {
  QString selectedFilter;
  QString myDir;
  if (dir.isEmpty())

  {
#if QT_VERSION < 0x050000
    myDir =
        QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
    myDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
  } else {
    myDir = dir;
  }

  QString result = QDir::toNativeSeparators(QFileDialog::getOpenFileName(
      parent, caption, myDir, filter, &selectedFilter));

  if (selectedSuffixOut) {
    QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
    QString selectedSuffix;
    if (filter_re.exactMatch(selectedFilter)) {
      selectedSuffix = filter_re.cap(1);
    }
    *selectedSuffixOut = selectedSuffix;
  }
  return result;
}

Qt::ConnectionType blockingGUIThreadConnection() {
  if (QThread::currentThread() != qApp->thread()) {
    return Qt::BlockingQueuedConnection;
  } else {
    return Qt::DirectConnection;
  }
}

bool checkPoint(const QPoint &p, const QWidget *w) {
  QWidget *atW = QApplication::widgetAt(w->mapToGlobal(p));
  if (!atW)
    return false;
  return atW->topLevelWidget() == w;
}

bool isObscured(QWidget *w) {
  return !(checkPoint(QPoint(0, 0), w) &&
           checkPoint(QPoint(w->width() - 1, 0), w) &&
           checkPoint(QPoint(0, w->height() - 1), w) &&
           checkPoint(QPoint(w->width() - 1, w->height() - 1), w) &&
           checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void openDebugLogfile() {
  fs::path pathDebug = GetDataDir() / "debug.log";

  if (fs::exists(pathDebug))
    QDesktopServices::openUrl(
        QUrl::fromLocalFile(boostPathToQString(pathDebug)));
}

bool openBitcoinConf() {
  boost::filesystem::path pathConfig = GetConfigFile(BITCOIN_CONF_FILENAME);

  boost::filesystem::ofstream configFile(pathConfig, std::ios_base::app);

  if (!configFile.good())
    return false;

  configFile.close();

  return QDesktopServices::openUrl(
      QUrl::fromLocalFile(boostPathToQString(pathConfig)));
}

void SubstituteFonts(const QString &language) {
#if defined(Q_OS_MAC)

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&                                   \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8
  if (floor(NSAppKitVersionNumber) > NSAppKitVersionNumber10_8) {
    if (floor(NSAppKitVersionNumber) <= NSAppKitVersionNumber10_9)

      QFont::insertSubstitution(".Lucida Grande UI", "Lucida Grande");
    else {
      if (language == "zh_CN" || language == "zh_TW" || language == "zh_HK")

        QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Heiti SC");
      else if (language == "ja")

        QFont::insertSubstitution(".Helvetica Neue DeskInterface", "Songti SC");
      else
        QFont::insertSubstitution(".Helvetica Neue DeskInterface",
                                  "Lucida Grande");
    }
  }
#endif
#endif
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int _size_threshold,
                                                 QObject *parent)
    : QObject(parent), size_threshold(_size_threshold) {}

bool ToolTipToRichTextFilter::eventFilter(QObject *obj, QEvent *evt) {
  if (evt->type() == QEvent::ToolTipChange) {
    QWidget *widget = static_cast<QWidget *>(obj);
    QString tooltip = widget->toolTip();
    if (tooltip.size() > size_threshold && !tooltip.startsWith("<qt") &&
        !Qt::mightBeRichText(tooltip)) {
      tooltip = "<qt>" + HtmlEscape(tooltip, true) + "</qt>";
      widget->setToolTip(tooltip);
      return true;
    }
  }
  return QObject::eventFilter(obj, evt);
}

void TableViewLastColumnResizingFixer::connectViewHeadersSignals() {
  connect(tableView->horizontalHeader(), SIGNAL(sectionResized(int, int, int)),
          this, SLOT(on_sectionResized(int, int, int)));
  connect(tableView->horizontalHeader(), SIGNAL(geometriesChanged()), this,
          SLOT(on_geometriesChanged()));
}

void TableViewLastColumnResizingFixer::disconnectViewHeadersSignals() {
  disconnect(tableView->horizontalHeader(),
             SIGNAL(sectionResized(int, int, int)), this,
             SLOT(on_sectionResized(int, int, int)));
  disconnect(tableView->horizontalHeader(), SIGNAL(geometriesChanged()), this,
             SLOT(on_geometriesChanged()));
}

void TableViewLastColumnResizingFixer::setViewHeaderResizeMode(
    int logicalIndex, QHeaderView::ResizeMode resizeMode) {
#if QT_VERSION < 0x050000
  tableView->horizontalHeader()->setResizeMode(logicalIndex, resizeMode);
#else
  tableView->horizontalHeader()->setSectionResizeMode(logicalIndex, resizeMode);
#endif
}

void TableViewLastColumnResizingFixer::resizeColumn(int nColumnIndex,
                                                    int width) {
  tableView->setColumnWidth(nColumnIndex, width);
  tableView->horizontalHeader()->resizeSection(nColumnIndex, width);
}

int TableViewLastColumnResizingFixer::getColumnsWidth() {
  int nColumnsWidthSum = 0;
  for (int i = 0; i < columnCount; i++) {
    nColumnsWidthSum += tableView->horizontalHeader()->sectionSize(i);
  }
  return nColumnsWidthSum;
}

int TableViewLastColumnResizingFixer::getAvailableWidthForColumn(int column) {
  int nResult = lastColumnMinimumWidth;
  int nTableWidth = tableView->horizontalHeader()->width();

  if (nTableWidth > 0) {
    int nOtherColsWidth =
        getColumnsWidth() - tableView->horizontalHeader()->sectionSize(column);
    nResult = std::max(nResult, nTableWidth - nOtherColsWidth);
  }

  return nResult;
}

void TableViewLastColumnResizingFixer::adjustTableColumnsWidth() {
  disconnectViewHeadersSignals();
  resizeColumn(lastColumnIndex, getAvailableWidthForColumn(lastColumnIndex));
  connectViewHeadersSignals();

  int nTableWidth = tableView->horizontalHeader()->width();
  int nColsWidth = getColumnsWidth();
  if (nColsWidth > nTableWidth) {
    resizeColumn(secondToLastColumnIndex,
                 getAvailableWidthForColumn(secondToLastColumnIndex));
  }
}

void TableViewLastColumnResizingFixer::stretchColumnWidth(int column) {
  disconnectViewHeadersSignals();
  resizeColumn(column, getAvailableWidthForColumn(column));
  connectViewHeadersSignals();
}

void TableViewLastColumnResizingFixer::on_sectionResized(int logicalIndex,
                                                         int oldSize,
                                                         int newSize) {
  adjustTableColumnsWidth();
  int remainingWidth = getAvailableWidthForColumn(logicalIndex);
  if (newSize > remainingWidth) {
    resizeColumn(logicalIndex, remainingWidth);
  }
}

void TableViewLastColumnResizingFixer::on_geometriesChanged() {
  if ((getColumnsWidth() - this->tableView->horizontalHeader()->width()) != 0) {
    disconnectViewHeadersSignals();
    resizeColumn(secondToLastColumnIndex,
                 getAvailableWidthForColumn(secondToLastColumnIndex));
    connectViewHeadersSignals();
  }
}

TableViewLastColumnResizingFixer::TableViewLastColumnResizingFixer(
    QTableView *table, int lastColMinimumWidth, int allColsMinimumWidth,
    QObject *parent)
    : QObject(parent), tableView(table),
      lastColumnMinimumWidth(lastColMinimumWidth),
      allColumnsMinimumWidth(allColsMinimumWidth) {
  columnCount = tableView->horizontalHeader()->count();
  lastColumnIndex = columnCount - 1;
  secondToLastColumnIndex = columnCount - 2;
  tableView->horizontalHeader()->setMinimumSectionSize(allColumnsMinimumWidth);
  setViewHeaderResizeMode(secondToLastColumnIndex, QHeaderView::Interactive);
  setViewHeaderResizeMode(lastColumnIndex, QHeaderView::Interactive);
}

#ifdef WIN32
fs::path static StartupShortcutPath() {
  std::string chain = ChainNameFromCommandLine();
  if (chain == CBaseChainParams::MAIN)
    return GetSpecialFolderPath(CSIDL_STARTUP) / "LitecoinCash.lnk";
  if (chain == CBaseChainParams::TESTNET)

    return GetSpecialFolderPath(CSIDL_STARTUP) / "LitecoinCash (testnet).lnk";
  return GetSpecialFolderPath(CSIDL_STARTUP) /
         strprintf("LitecoinCash (%s).lnk", chain);
}

bool GetStartOnSystemStartup() { return fs::exists(StartupShortcutPath()); }

bool SetStartOnSystemStartup(bool fAutoStart) {
  fs::remove(StartupShortcutPath());

  if (fAutoStart) {
    CoInitialize(nullptr);

    IShellLink *psl = nullptr;
    HRESULT hres =
        CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                         IID_IShellLink, reinterpret_cast<void **>(&psl));

    if (SUCCEEDED(hres)) {
      TCHAR pszExePath[MAX_PATH];
      GetModuleFileName(nullptr, pszExePath, sizeof(pszExePath));

      QString strArgs = "-min";

      strArgs += QString::fromStdString(strprintf(
          " -testnet=%d -regtest=%d", gArgs.GetBoolArg("-testnet", false),
          gArgs.GetBoolArg("-regtest", false)));

#ifdef UNICODE
      boost::scoped_array<TCHAR> args(new TCHAR[strArgs.length() + 1]);

      strArgs.toWCharArray(args.get());

      args[strArgs.length()] = '\0';
#endif

      psl->SetPath(pszExePath);
      PathRemoveFileSpec(pszExePath);
      psl->SetWorkingDirectory(pszExePath);
      psl->SetShowCmd(SW_SHOWMINNOACTIVE);
#ifndef UNICODE
      psl->SetArguments(strArgs.toStdString().c_str());
#else
      psl->SetArguments(args.get());
#endif

      IPersistFile *ppf = nullptr;
      hres = psl->QueryInterface(IID_IPersistFile,
                                 reinterpret_cast<void **>(&ppf));
      if (SUCCEEDED(hres)) {
        WCHAR pwsz[MAX_PATH];

        MultiByteToWideChar(CP_ACP, 0, StartupShortcutPath().string().c_str(),
                            -1, pwsz, MAX_PATH);

        hres = ppf->Save(pwsz, TRUE);
        ppf->Release();
        psl->Release();
        CoUninitialize();
        return true;
      }
      psl->Release();
    }
    CoUninitialize();
    return false;
  }
  return true;
}
#elif defined(Q_OS_LINUX)

fs::path static GetAutostartDir() {
  char *pszConfigHome = getenv("XDG_CONFIG_HOME");
  if (pszConfigHome)
    return fs::path(pszConfigHome) / "autostart";
  char *pszHome = getenv("HOME");
  if (pszHome)
    return fs::path(pszHome) / ".config" / "autostart";
  return fs::path();
}

fs::path static GetAutostartFilePath() {
  std::string chain = ChainNameFromCommandLine();
  if (chain == CBaseChainParams::MAIN)
    return GetAutostartDir() / "litecoincash.desktop";
  return GetAutostartDir() / strprintf("litecoincash-%s.lnk", chain);
}

bool GetStartOnSystemStartup() {
  fs::ifstream optionFile(GetAutostartFilePath());
  if (!optionFile.good())
    return false;

  std::string line;
  while (!optionFile.eof()) {
    getline(optionFile, line);
    if (line.find("Hidden") != std::string::npos &&
        line.find("true") != std::string::npos)
      return false;
  }
  optionFile.close();

  return true;
}

bool SetStartOnSystemStartup(bool fAutoStart) {
  if (!fAutoStart)
    fs::remove(GetAutostartFilePath());
  else {
    char pszExePath[MAX_PATH + 1];
    ssize_t r = readlink("/proc/self/exe", pszExePath, sizeof(pszExePath) - 1);
    if (r == -1)
      return false;
    pszExePath[r] = '\0';

    fs::create_directories(GetAutostartDir());

    fs::ofstream optionFile(GetAutostartFilePath(),
                            std::ios_base::out | std::ios_base::trunc);
    if (!optionFile.good())
      return false;
    std::string chain = ChainNameFromCommandLine();

    optionFile << "[Desktop Entry]\n";
    optionFile << "Type=Application\n";
    if (chain == CBaseChainParams::MAIN)
      optionFile << "Name=LitecoinCash\n";
    else
      optionFile << strprintf("Name=LitecoinCash (%s)\n", chain);
    optionFile << "Exec=" << pszExePath
               << strprintf(" -min -testnet=%d -regtest=%d\n",
                            gArgs.GetBoolArg("-testnet", false),
                            gArgs.GetBoolArg("-regtest", false));
    optionFile << "Terminal=false\n";
    optionFile << "Hidden=false\n";
    optionFile.close();
  }
  return true;
}

#elif defined(Q_OS_MAC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

LSSharedFileListItemRef findStartupItemInList(LSSharedFileListRef list,
                                              CFURLRef findUrl);
LSSharedFileListItemRef findStartupItemInList(LSSharedFileListRef list,
                                              CFURLRef findUrl) {
  CFArrayRef listSnapshot = LSSharedFileListCopySnapshot(list, nullptr);
  if (listSnapshot == nullptr) {
    return nullptr;
  }

  for (int i = 0; i < CFArrayGetCount(listSnapshot); i++) {
    LSSharedFileListItemRef item =
        (LSSharedFileListItemRef)CFArrayGetValueAtIndex(listSnapshot, i);
    UInt32 resolutionFlags =
        kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;
    CFURLRef currentItemURL = nullptr;

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&                                   \
    MAC_OS_X_VERSION_MAX_ALLOWED >= 10100
    if (&LSSharedFileListItemCopyResolvedURL)
      currentItemURL =
          LSSharedFileListItemCopyResolvedURL(item, resolutionFlags, nullptr);
#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) &&                                  \
    MAC_OS_X_VERSION_MIN_REQUIRED < 10100
    else
      LSSharedFileListItemResolve(item, resolutionFlags, &currentItemURL,
                                  nullptr);
#endif
#else
    LSSharedFileListItemResolve(item, resolutionFlags, &currentItemURL,
                                nullptr);
#endif

    if (currentItemURL) {
      if (CFEqual(currentItemURL, findUrl)) {
        CFRelease(listSnapshot);
        CFRelease(currentItemURL);
        return item;
      }
      CFRelease(currentItemURL);
    }
  }

  CFRelease(listSnapshot);
  return nullptr;
}

bool GetStartOnSystemStartup() {
  CFURLRef bitcoinAppUrl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
  if (bitcoinAppUrl == nullptr) {
    return false;
  }

  LSSharedFileListRef loginItems = LSSharedFileListCreate(
      nullptr, kLSSharedFileListSessionLoginItems, nullptr);
  LSSharedFileListItemRef foundItem =
      findStartupItemInList(loginItems, bitcoinAppUrl);

  CFRelease(bitcoinAppUrl);
  return !!foundItem;
}

bool SetStartOnSystemStartup(bool fAutoStart) {
  CFURLRef bitcoinAppUrl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
  if (bitcoinAppUrl == nullptr) {
    return false;
  }

  LSSharedFileListRef loginItems = LSSharedFileListCreate(
      nullptr, kLSSharedFileListSessionLoginItems, nullptr);
  LSSharedFileListItemRef foundItem =
      findStartupItemInList(loginItems, bitcoinAppUrl);

  if (fAutoStart && !foundItem) {
    LSSharedFileListInsertItemURL(loginItems, kLSSharedFileListItemBeforeFirst,
                                  nullptr, nullptr, bitcoinAppUrl, nullptr,
                                  nullptr);
  } else if (!fAutoStart && foundItem) {
    LSSharedFileListItemRemove(loginItems, foundItem);
  }

  CFRelease(bitcoinAppUrl);
  return true;
}
#pragma GCC diagnostic pop
#else

bool GetStartOnSystemStartup() { return false; }
bool SetStartOnSystemStartup(bool fAutoStart) { return false; }

#endif

void setClipboard(const QString &str) {
  QApplication::clipboard()->setText(str, QClipboard::Clipboard);
  QApplication::clipboard()->setText(str, QClipboard::Selection);
}

fs::path qstringToBoostPath(const QString &path) {
  return fs::path(path.toStdString(), utf8);
}

QString boostPathToQString(const fs::path &path) {
  return QString::fromStdString(path.string(utf8));
}

QString formatDurationStr(int secs) {
  QStringList strList;
  int days = secs / 86400;
  int hours = (secs % 86400) / 3600;
  int mins = (secs % 3600) / 60;
  int seconds = secs % 60;

  if (days)
    strList.append(QString(QObject::tr("%1 d")).arg(days));
  if (hours)
    strList.append(QString(QObject::tr("%1 h")).arg(hours));
  if (mins)
    strList.append(QString(QObject::tr("%1 m")).arg(mins));
  if (seconds || (!days && !hours && !mins))
    strList.append(QString(QObject::tr("%1 s")).arg(seconds));

  return strList.join(" ");
}

QString formatServicesStr(quint64 mask) {
  QStringList strList;

  for (int i = 0; i < 8; i++) {
    uint64_t check = 1 << i;
    if (mask & check) {
      switch (check) {
      case NODE_NETWORK:
        strList.append("NETWORK");
        break;
      case NODE_GETUTXO:
        strList.append("GETUTXO");
        break;
      case NODE_BLOOM:
        strList.append("BLOOM");
        break;
      case NODE_WITNESS:
        strList.append("WITNESS");
        break;
      case NODE_RIALTO:

        strList.append("RIALTO");
        break;
      case NODE_XTHIN:
        strList.append("XTHIN");
        break;
      default:
        strList.append(QString("%1[%2]").arg("UNKNOWN").arg(check));
      }
    }
  }

  if (strList.size())
    return strList.join(" & ");
  else
    return QObject::tr("None");
}

QString formatPingTime(double dPingTime) {
  return (dPingTime == std::numeric_limits<int64_t>::max() / 1e6 ||
          dPingTime == 0)
             ? QObject::tr("N/A")
             : QString(QObject::tr("%1 ms"))
                   .arg(QString::number((int)(dPingTime * 1000), 10));
}

QString formatTimeOffset(int64_t nTimeOffset) {
  return QString(QObject::tr("%1 s"))
      .arg(QString::number((int)nTimeOffset, 10));
}

QString formatNiceTimeOffset(qint64 secs) {
  QString timeBehindText;
  const int HOUR_IN_SECONDS = 60 * 60;
  const int DAY_IN_SECONDS = 24 * 60 * 60;
  const int WEEK_IN_SECONDS = 7 * 24 * 60 * 60;
  const int YEAR_IN_SECONDS = 31556952;

  if (secs < 60) {
    timeBehindText = QObject::tr("%n second(s)", "", secs);
  } else if (secs < 2 * HOUR_IN_SECONDS) {
    timeBehindText = QObject::tr("%n minute(s)", "", secs / 60);
  } else if (secs < 2 * DAY_IN_SECONDS) {
    timeBehindText = QObject::tr("%n hour(s)", "", secs / HOUR_IN_SECONDS);
  } else if (secs < 2 * WEEK_IN_SECONDS) {
    timeBehindText = QObject::tr("%n day(s)", "", secs / DAY_IN_SECONDS);
  } else if (secs < YEAR_IN_SECONDS) {
    timeBehindText = QObject::tr("%n week(s)", "", secs / WEEK_IN_SECONDS);
  } else {
    qint64 years = secs / YEAR_IN_SECONDS;
    qint64 remainder = secs % YEAR_IN_SECONDS;
    timeBehindText =
        QObject::tr("%1 and %2")
            .arg(QObject::tr("%n year(s)", "", years))
            .arg(QObject::tr("%n week(s)", "", remainder / WEEK_IN_SECONDS));
  }
  return timeBehindText;
}

QString formatBytes(uint64_t bytes) {
  if (bytes < 1024)
    return QString(QObject::tr("%1 B")).arg(bytes);
  if (bytes < 1024 * 1024)
    return QString(QObject::tr("%1 KB")).arg(bytes / 1024);
  if (bytes < 1024 * 1024 * 1024)
    return QString(QObject::tr("%1 MB")).arg(bytes / 1024 / 1024);

  return QString(QObject::tr("%1 GB")).arg(bytes / 1024 / 1024 / 1024);
}

qreal calculateIdealFontSize(int width, const QString &text, QFont font,
                             qreal minPointSize, qreal font_size) {
  while (font_size >= minPointSize) {
    font.setPointSizeF(font_size);
    QFontMetrics fm(font);
    if (fm.width(text) < width) {
      break;
    }
    font_size -= 0.5;
  }
  return font_size;
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent *event) {
  Q_EMIT clicked(event->pos());
}

void ClickableProgressBar::mouseReleaseEvent(QMouseEvent *event) {
  Q_EMIT clicked(event->pos());
}

} // namespace GUIUtil
