// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoinaddressvalidator.h>

#include <base58.h>

BitcoinAddressEntryValidator::BitcoinAddressEntryValidator(QObject *parent)
    : QValidator(parent) {}

QValidator::State BitcoinAddressEntryValidator::validate(QString &input,
                                                         int &pos) const {
  Q_UNUSED(pos);

  if (input.isEmpty())
    return QValidator::Intermediate;

  for (int idx = 0; idx < input.size();) {
    bool removeChar = false;
    QChar ch = input.at(idx);

    switch (ch.unicode()) {
    case 0x200B:

    case 0xFEFF:

      removeChar = true;
      break;
    default:
      break;
    }

    if (ch.isSpace())
      removeChar = true;

    if (removeChar)
      input.remove(idx, 1);
    else
      ++idx;
  }

  QValidator::State state = QValidator::Acceptable;
  for (int idx = 0; idx < input.size(); ++idx) {
    int ch = input.at(idx).unicode();

    if (((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
         (ch >= 'A' && ch <= 'Z')) &&
        ch != 'I' && ch != 'O')

    {
    } else {
      state = QValidator::Invalid;
    }
  }

  return state;
}

BitcoinAddressCheckValidator::BitcoinAddressCheckValidator(QObject *parent)
    : QValidator(parent) {}

QValidator::State BitcoinAddressCheckValidator::validate(QString &input,
                                                         int &pos) const {
  Q_UNUSED(pos);

  if (IsValidDestinationString(input.toStdString())) {
    return QValidator::Acceptable;
  }

  return QValidator::Invalid;
}
