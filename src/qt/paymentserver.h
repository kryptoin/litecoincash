// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_PAYMENTSERVER_H
#define BITCOIN_QT_PAYMENTSERVER_H

#include <qt/paymentrequestplus.h>
#include <qt/walletmodel.h>

#include <QObject>
#include <QString>

class OptionsModel;

class CWallet;

QT_BEGIN_NAMESPACE
class QApplication;
class QByteArray;
class QLocalServer;
class QNetworkAccessManager;
class QNetworkReply;
class QSslError;
class QUrl;
QT_END_NAMESPACE

static const qint64 BIP70_MAX_PAYMENTREQUEST_SIZE = 50000;

class PaymentServer : public QObject {
  Q_OBJECT

public:
  static void ipcParseCommandLine(int argc, char *argv[]);

  static bool ipcSendCommandLine();

  explicit PaymentServer(QObject *parent, bool startLocalServer = true);
  ~PaymentServer();

  static void LoadRootCAs(X509_STORE *store = nullptr);

  static X509_STORE *getCertStore();

  void setOptionsModel(OptionsModel *optionsModel);

  static bool verifyNetwork(const payments::PaymentDetails &requestDetails);

  static bool verifyExpired(const payments::PaymentDetails &requestDetails);

  static bool verifySize(qint64 requestSize);

  static bool verifyAmount(const CAmount &requestAmount);

Q_SIGNALS:

  void receivedPaymentRequest(SendCoinsRecipient);

  void receivedPaymentACK(const QString &paymentACKMsg);

  void message(const QString &title, const QString &message,
               unsigned int style);

public Q_SLOTS:

  void uiReady();

  void fetchPaymentACK(CWallet *wallet, const SendCoinsRecipient &recipient,
                       QByteArray transaction);

  void handleURIOrFile(const QString &s);

private Q_SLOTS:
  void handleURIConnection();
  void netRequestFinished(QNetworkReply *);
  void reportSslErrors(QNetworkReply *, const QList<QSslError> &);
  void handlePaymentACK(const QString &paymentACKMsg);

protected:
  bool eventFilter(QObject *object, QEvent *event);

private:
  static bool readPaymentRequestFromFile(const QString &filename,
                                         PaymentRequestPlus &request);
  bool processPaymentRequest(const PaymentRequestPlus &request,
                             SendCoinsRecipient &recipient);
  void fetchRequest(const QUrl &url);

  void initNetManager();

  bool saveURIs;

  QLocalServer *uriServer;

  QNetworkAccessManager *netManager;

  OptionsModel *optionsModel;
};

#endif
