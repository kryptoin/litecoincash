// Copyright (c) 2011-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QVALIDATEDLINEEDIT_H
#define BITCOIN_QT_QVALIDATEDLINEEDIT_H

#include <QLineEdit>

class QValidatedLineEdit : public QLineEdit {
  Q_OBJECT

public:
  explicit QValidatedLineEdit(QWidget *parent);
  void clear();
  void setCheckValidator(const QValidator *v);
  bool isValid();

protected:
  void focusInEvent(QFocusEvent *evt);
  void focusOutEvent(QFocusEvent *evt);

private:
  bool valid;
  const QValidator *checkValidator;

public Q_SLOTS:
  void setValid(bool valid);
  void setEnabled(bool enabled);

Q_SIGNALS:
  void validationDidChange(QValidatedLineEdit *validatedLineEdit);

private Q_SLOTS:
  void markValid();
  void checkValidity();
};

#endif
