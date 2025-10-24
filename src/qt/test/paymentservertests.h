// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TEST_PAYMENTSERVERTESTS_H
#define BITCOIN_QT_TEST_PAYMENTSERVERTESTS_H

#include <qt/paymentserver.h>

#include <QObject>
#include <QTest>

class PaymentServerTests : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void paymentServerTests();
};

class RecipientCatcher : public QObject {
  Q_OBJECT

public Q_SLOTS:
  void getRecipient(const SendCoinsRecipient &r);

public:
  SendCoinsRecipient recipient;
};

#endif
