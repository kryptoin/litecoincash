// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_INTRO_H
#define BITCOIN_QT_INTRO_H

#include <QDialog>
#include <QMutex>
#include <QThread>

static const bool DEFAULT_CHOOSE_DATADIR = false;

class FreespaceChecker;

namespace Ui {
class Intro;
}

class Intro : public QDialog {
  Q_OBJECT

public:
  explicit Intro(QWidget *parent = 0);
  ~Intro();

  QString getDataDirectory();
  void setDataDirectory(const QString &dataDir);

  static bool pickDataDirectory();

  static QString getDefaultDataDirectory();

Q_SIGNALS:
  void requestCheck();
  void stopThread();

public Q_SLOTS:
  void setStatus(int status, const QString &message, quint64 bytesAvailable);

private Q_SLOTS:
  void on_dataDirectory_textChanged(const QString &arg1);
  void on_ellipsisButton_clicked();
  void on_dataDirDefault_clicked();
  void on_dataDirCustom_clicked();

private:
  Ui::Intro *ui;
  QThread *thread;
  QMutex mutex;
  bool signalled;
  QString pathToCheck;

  void startThread();
  void checkPath(const QString &dataDir);
  QString getPathToCheck();

  friend class FreespaceChecker;
};

#endif
