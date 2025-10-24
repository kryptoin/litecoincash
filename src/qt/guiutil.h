// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUIUTIL_H
#define BITCOIN_QT_GUIUTIL_H

#include <amount.h>
#include <fs.h>

#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QProgressBar>
#include <QString>
#include <QTableView>

class QValidatedLineEdit;
class SendCoinsRecipient;

QT_BEGIN_NAMESPACE
class QAbstractItemView;
class QDateTime;
class QFont;
class QLineEdit;
class QUrl;
class QWidget;
QT_END_NAMESPACE

namespace GUIUtil {
QString dateTimeStr(const QDateTime &datetime);
QString dateTimeStr(qint64 nTime);

QFont fixedPitchFont();

void setupAddressWidget(QValidatedLineEdit *widget, QWidget *parent);
void setupAmountWidget(QLineEdit *widget, QWidget *parent);

bool parseBitcoinURI(const QUrl &uri, SendCoinsRecipient *out);
bool parseBitcoinURI(QString uri, SendCoinsRecipient *out);
QString formatBitcoinURI(const SendCoinsRecipient &info);

bool isDust(const QString &address, const CAmount &amount);

QString HtmlEscape(const QString &str, bool fMultiLine = false);
QString HtmlEscape(const std::string &str, bool fMultiLine = false);

void copyEntryData(QAbstractItemView *view, int column,
                   int role = Qt::EditRole);

QList<QModelIndex> getEntryData(QAbstractItemView *view, int column);

void setClipboard(const QString &str);

QString getSaveFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut);

QString getOpenFileName(QWidget *parent, const QString &caption,
                        const QString &dir, const QString &filter,
                        QString *selectedSuffixOut);

Qt::ConnectionType blockingGUIThreadConnection();

bool isObscured(QWidget *w);

void openDebugLogfile();

bool openBitcoinConf();

void SubstituteFonts(const QString &language);

class ToolTipToRichTextFilter : public QObject {
  Q_OBJECT

public:
  explicit ToolTipToRichTextFilter(int size_threshold, QObject *parent = 0);

protected:
  bool eventFilter(QObject *obj, QEvent *evt);

private:
  int size_threshold;
};

class TableViewLastColumnResizingFixer : public QObject {
  Q_OBJECT

public:
  TableViewLastColumnResizingFixer(QTableView *table, int lastColMinimumWidth,
                                   int allColsMinimumWidth, QObject *parent);
  void stretchColumnWidth(int column);

private:
  QTableView *tableView;
  int lastColumnMinimumWidth;
  int allColumnsMinimumWidth;
  int lastColumnIndex;
  int columnCount;
  int secondToLastColumnIndex;

  void adjustTableColumnsWidth();
  int getAvailableWidthForColumn(int column);
  int getColumnsWidth();
  void connectViewHeadersSignals();
  void disconnectViewHeadersSignals();
  void setViewHeaderResizeMode(int logicalIndex,
                               QHeaderView::ResizeMode resizeMode);
  void resizeColumn(int nColumnIndex, int width);

private Q_SLOTS:
  void on_sectionResized(int logicalIndex, int oldSize, int newSize);
  void on_geometriesChanged();
};

bool GetStartOnSystemStartup();
bool SetStartOnSystemStartup(bool fAutoStart);

fs::path qstringToBoostPath(const QString &path);

QString boostPathToQString(const fs::path &path);

QString formatDurationStr(int secs);

QString formatServicesStr(quint64 mask);

QString formatPingTime(double dPingTime);

QString formatTimeOffset(int64_t nTimeOffset);

QString formatNiceTimeOffset(qint64 secs);

QString formatBytes(uint64_t bytes);

qreal calculateIdealFontSize(int width, const QString &text, QFont font,
                             qreal minPointSize = 4, qreal startPointSize = 14);

class ClickableLabel : public QLabel {
  Q_OBJECT

Q_SIGNALS:

  void clicked(const QPoint &point);

protected:
  void mouseReleaseEvent(QMouseEvent *event);
};

class ClickableProgressBar : public QProgressBar {
  Q_OBJECT

Q_SIGNALS:

  void clicked(const QPoint &point);

protected:
  void mouseReleaseEvent(QMouseEvent *event);
};

#if defined(Q_OS_MAC) && QT_VERSION >= 0x050000

class ProgressBar : public ClickableProgressBar {
  bool event(QEvent *e) {
    return (e->type() != QEvent::StyleAnimationUpdate) ? QProgressBar::event(e)
                                                       : false;
  }
};
#else
typedef ClickableProgressBar ProgressBar;
#endif

} // namespace GUIUtil

#endif
