// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
class AskPassphraseDialog;
}

class AskPassphraseDialog : public QDialog {
  Q_OBJECT

public:
  enum Mode {
    Encrypt,

    UnlockHiveMining,

    Unlock,

    ChangePass,

    Decrypt

  };

  explicit AskPassphraseDialog(Mode mode, QWidget *parent);
  ~AskPassphraseDialog();

  void accept();

  void setModel(WalletModel *model);

private:
  Ui::AskPassphraseDialog *ui;
  Mode mode;
  WalletModel *model;
  bool fCapsLock;
  bool fHiveOnly;

private Q_SLOTS:
  void textChanged();
  void secureClearPassFields();
  void toggleShowPassword(bool);

protected:
  bool event(QEvent *event);
  bool eventFilter(QObject *object, QEvent *event);
};

#endif
