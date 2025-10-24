// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MACNOTIFICATIONHANDLER_H
#define BITCOIN_QT_MACNOTIFICATIONHANDLER_H

#include <QObject>

class MacNotificationHandler : public QObject {
  Q_OBJECT

public:
  void showNotification(const QString &title, const QString &text);

  bool hasUserNotificationCenterSupport(void);
  static MacNotificationHandler *instance();
};

#endif
