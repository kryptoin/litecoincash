// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SPLASHSCREEN_H
#define BITCOIN_QT_SPLASHSCREEN_H

#include <QSplashScreen>
#include <functional>

class CWallet;
class NetworkStyle;

class SplashScreen : public QWidget {
  Q_OBJECT

public:
  explicit SplashScreen(Qt::WindowFlags f, const NetworkStyle *networkStyle);
  ~SplashScreen();

protected:
  void paintEvent(QPaintEvent *event);
  void closeEvent(QCloseEvent *event);

public Q_SLOTS:

  void slotFinish(QWidget *mainWin);

  void showMessage(const QString &message, int alignment, const QColor &color);

protected:
  bool eventFilter(QObject *obj, QEvent *ev);

private:
  void subscribeToCoreSignals();

  void unsubscribeFromCoreSignals();

  void ConnectWallet(CWallet *);

  QPixmap pixmap;
  QString curMessage;
  QColor curColor;
  int curAlignment;

  QList<CWallet *> connectedWallets;
};

#endif
