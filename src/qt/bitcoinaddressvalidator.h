// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINADDRESSVALIDATOR_H
#define BITCOIN_QT_BITCOINADDRESSVALIDATOR_H

#include <QValidator>

class BitcoinAddressEntryValidator : public QValidator {
  Q_OBJECT

public:
  explicit BitcoinAddressEntryValidator(QObject *parent);

  State validate(QString &input, int &pos) const;
};

class BitcoinAddressCheckValidator : public QValidator {
  Q_OBJECT

public:
  explicit BitcoinAddressCheckValidator(QObject *parent);

  State validate(QString &input, int &pos) const;
};

#endif
