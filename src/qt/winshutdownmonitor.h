// Copyright (c) 2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WINSHUTDOWNMONITOR_H
#define BITCOIN_QT_WINSHUTDOWNMONITOR_H

#ifdef WIN32
#include <QByteArray>
#include <QString>

#if QT_VERSION >= 0x050000
#include <windef.h>

#include <QAbstractNativeEventFilter>

class WinShutdownMonitor : public QAbstractNativeEventFilter {
public:
  bool nativeEventFilter(const QByteArray &eventType, void *pMessage,
                         long *pnResult);

  static void registerShutdownBlockReason(const QString &strReason,
                                          const HWND &mainWinId);
};
#endif
#endif

#endif
