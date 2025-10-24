// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINAMOUNTFIELD_H
#define BITCOIN_QT_BITCOINAMOUNTFIELD_H

#include <amount.h>

#include <QWidget>

class AmountSpinBox;

QT_BEGIN_NAMESPACE
class QValueComboBox;
QT_END_NAMESPACE

class BitcoinAmountField : public QWidget {
  Q_OBJECT

  Q_PROPERTY(
      qint64 value READ value WRITE setValue NOTIFY valueChanged USER true)

public:
  explicit BitcoinAmountField(QWidget *parent = 0);

  CAmount value(bool *value = 0) const;
  void setValue(const CAmount &value);

  void setSingleStep(const CAmount &step);

  void setReadOnly(bool fReadOnly);

  void setValid(bool valid);

  bool validate();

  void setDisplayUnit(int unit);

  void clear();

  void setEnabled(bool fEnabled);

  QWidget *setupTabChain(QWidget *prev);

Q_SIGNALS:
  void valueChanged();

protected:
  bool eventFilter(QObject *object, QEvent *event);

private:
  AmountSpinBox *amount;
  QValueComboBox *unit;

private Q_SLOTS:
  void unitChanged(int idx);
};

#endif
