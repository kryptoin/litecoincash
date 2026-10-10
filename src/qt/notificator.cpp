// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/notificator.h>

#include <QApplication>
#include <QByteArray>
#include <QImageWriter>
#include <QMessageBox>
#include <QMetaType>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTemporaryFile>
#include <QVariant>
#ifdef USE_DBUS
#include <QtDBus>
#include <stdint.h>
#endif

#ifdef Q_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#include <qt/macnotificationhandler.h>
#endif

#ifdef USE_DBUS

const int FREEDESKTOP_NOTIFICATION_ICON_SIZE = 128;
#endif

Notificator::Notificator(const QString &_programName,
                         QSystemTrayIcon *_trayIcon, QWidget *_parent)
    : QObject(_parent), parent(_parent), programName(_programName), mode(None),
      trayIcon(_trayIcon)
#ifdef USE_DBUS
      ,
      interface(0)
#endif
{
  if (_trayIcon && _trayIcon->supportsMessages()) {
    mode = QSystemTray;
  }
#ifdef USE_DBUS
  interface = new QDBusInterface("org.freedesktop.Notifications",
                                 "/org/freedesktop/Notifications",
                                 "org.freedesktop.Notifications");
  if (interface->isValid()) {
    mode = Freedesktop;
  }
#endif
#ifdef Q_OS_MAC

  if (MacNotificationHandler::instance()->hasUserNotificationCenterSupport()) {
    mode = UserNotificationCenter;
  }
#endif
}

Notificator::~Notificator() {
#ifdef USE_DBUS
  delete interface;
#endif
}

#ifdef USE_DBUS

class FreedesktopImage {
public:
  FreedesktopImage() {}
  explicit FreedesktopImage(const QImage &img);

  static int metaType();

  static QVariant toVariant(const QImage &img);

private:
  int width, height, stride;
  bool hasAlpha;
  int channels;
  int bitsPerSample;
  QByteArray image;

  friend QDBusArgument &operator<<(QDBusArgument &a, const FreedesktopImage &i);
  friend const QDBusArgument &operator>>(const QDBusArgument &a,
                                         FreedesktopImage &i);
};

Q_DECLARE_METATYPE(FreedesktopImage);

const int CHANNELS = 4;
const int BYTES_PER_PIXEL = 4;
const int BITS_PER_SAMPLE = 8;

FreedesktopImage::FreedesktopImage(const QImage &img)
    : width(img.width()), height(img.height()),
      stride(img.width() * BYTES_PER_PIXEL), hasAlpha(true), channels(CHANNELS),
      bitsPerSample(BITS_PER_SAMPLE) {
  QImage tmp = img.convertToFormat(QImage::Format_ARGB32);
  const uint32_t *data = reinterpret_cast<const uint32_t *>(tmp.bits());

  unsigned int num_pixels = width * height;
  image.resize(num_pixels * BYTES_PER_PIXEL);

  for (unsigned int ptr = 0; ptr < num_pixels; ++ptr) {
    image[ptr * BYTES_PER_PIXEL + 0] = data[ptr] >> 16;

    image[ptr * BYTES_PER_PIXEL + 1] = data[ptr] >> 8;

    image[ptr * BYTES_PER_PIXEL + 2] = data[ptr];

    image[ptr * BYTES_PER_PIXEL + 3] = data[ptr] >> 24;
  }
}

QDBusArgument &operator<<(QDBusArgument &a, const FreedesktopImage &i) {
  a.beginStructure();
  a << i.width << i.height << i.stride << i.hasAlpha << i.bitsPerSample
    << i.channels << i.image;
  a.endStructure();
  return a;
}

const QDBusArgument &operator>>(const QDBusArgument &a, FreedesktopImage &i) {
  a.beginStructure();
  a >> i.width >> i.height >> i.stride >> i.hasAlpha >> i.bitsPerSample >>
      i.channels >> i.image;
  a.endStructure();
  return a;
}

int FreedesktopImage::metaType() {
  return qDBusRegisterMetaType<FreedesktopImage>();
}

QVariant FreedesktopImage::toVariant(const QImage &img) {
  FreedesktopImage fimg(img);
  return QVariant(FreedesktopImage::metaType(), &fimg);
}

void Notificator::notifyDBus(Class cls, const QString &title,
                             const QString &text, const QIcon &icon,
                             int millisTimeout) {
  Q_UNUSED(cls);

  QList<QVariant> args;

  args.append(programName);

  args.append(0U);

  args.append(QString());

  args.append(title);

  args.append(text);

  QStringList actions;
  args.append(actions);

  QVariantMap hints;

  QIcon tmpicon;
  if (icon.isNull()) {
    QStyle::StandardPixmap sicon = QStyle::SP_MessageBoxQuestion;
    switch (cls) {
    case Information:
      sicon = QStyle::SP_MessageBoxInformation;
      break;
    case Warning:
      sicon = QStyle::SP_MessageBoxWarning;
      break;
    case Critical:
      sicon = QStyle::SP_MessageBoxCritical;
      break;
    default:
      break;
    }
    tmpicon = QApplication::style()->standardIcon(sicon);
  } else {
    tmpicon = icon;
  }
  hints["icon_data"] = FreedesktopImage::toVariant(
      tmpicon.pixmap(FREEDESKTOP_NOTIFICATION_ICON_SIZE).toImage());
  args.append(hints);

  args.append(millisTimeout);

  interface->callWithArgumentList(QDBus::NoBlock, "Notify", args);
}
#endif

void Notificator::notifySystray(Class cls, const QString &title,
                                const QString &text, const QIcon &icon,
                                int millisTimeout) {
  Q_UNUSED(icon);
  QSystemTrayIcon::MessageIcon sicon = QSystemTrayIcon::NoIcon;
  switch (cls)

  {
  case Information:
    sicon = QSystemTrayIcon::Information;
    break;
  case Warning:
    sicon = QSystemTrayIcon::Warning;
    break;
  case Critical:
    sicon = QSystemTrayIcon::Critical;
    break;
  }
  trayIcon->showMessage(title, text, sicon, millisTimeout);
}

#ifdef Q_OS_MAC
void Notificator::notifyMacUserNotificationCenter(Class cls,
                                                  const QString &title,
                                                  const QString &text,
                                                  const QIcon &icon) {
  MacNotificationHandler::instance()->showNotification(title, text);
}

#endif

void Notificator::notify(Class cls, const QString &title, const QString &text,
                         const QIcon &icon, int millisTimeout) {
  switch (mode) {
#ifdef USE_DBUS
  case Freedesktop:
    notifyDBus(cls, title, text, icon, millisTimeout);
    break;
#endif
  case QSystemTray:
    notifySystray(cls, title, text, icon, millisTimeout);
    break;
#ifdef Q_OS_MAC
  case UserNotificationCenter:
    notifyMacUserNotificationCenter(cls, title, text, icon);
    break;
#endif
  default:
    if (cls == Critical) {
      QMessageBox::critical(parent, title, text, QMessageBox::Ok,
                            QMessageBox::Ok);
    }
    break;
  }
}
