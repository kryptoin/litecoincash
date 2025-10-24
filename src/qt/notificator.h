// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_NOTIFICATOR_H
#define BITCOIN_QT_NOTIFICATOR_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <QIcon>
#include <QObject>

QT_BEGIN_NAMESPACE
class QSystemTrayIcon;

#ifdef USE_DBUS
class QDBusInterface;
#endif
QT_END_NAMESPACE

class Notificator : public QObject {
  Q_OBJECT

public:
  Notificator(const QString &programName, QSystemTrayIcon *trayIcon,
              QWidget *parent);
  ~Notificator();

  enum Class {
    Information,

    Warning,

    Critical

  };

public Q_SLOTS:

  void notify(Class cls, const QString &title, const QString &text,
              const QIcon &icon = QIcon(), int millisTimeout = 10000);

private:
  QWidget *parent;
  enum Mode {
    None,

    Freedesktop,

    QSystemTray,

    UserNotificationCenter

  };
  QString programName;
  Mode mode;
  QSystemTrayIcon *trayIcon;
#ifdef USE_DBUS
  QDBusInterface *interface;

  void notifyDBus(Class cls, const QString &title, const QString &text,
                  const QIcon &icon, int millisTimeout);
#endif
  void notifySystray(Class cls, const QString &title, const QString &text,
                     const QIcon &icon, int millisTimeout);
#ifdef Q_OS_MAC
  void notifyMacUserNotificationCenter(Class cls, const QString &title,
                                       const QString &text, const QIcon &icon);
#endif
};

#endif
