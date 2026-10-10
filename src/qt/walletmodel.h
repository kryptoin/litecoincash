// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETMODEL_H
#define BITCOIN_QT_WALLETMODEL_H

#include <qt/paymentrequestplus.h>
#include <qt/walletmodeltransaction.h>

#include <support/allocators/secure.h>

#include <wallet/wallet.h>

#include <map>
#include <vector>

#include <QObject>

enum OutputType : int;

class AddressTableModel;
class OptionsModel;
class PlatformStyle;
class RecentRequestsTableModel;
class HiveTableModel;

class TransactionTableModel;
class WalletModelTransaction;

class CCoinControl;
class CKeyID;
class COutPoint;
class COutput;
class CPubKey;
class CWallet;
class uint256;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

class SendCoinsRecipient {
public:
  explicit SendCoinsRecipient()
      : amount(0), fSubtractFeeFromAmount(false),
        nVersion(SendCoinsRecipient::CURRENT_VERSION) {}
  explicit SendCoinsRecipient(const QString &addr, const QString &_label,
                              const CAmount &_amount, const QString &_message)
      : address(addr), label(_label), amount(_amount), message(_message),
        fSubtractFeeFromAmount(false),
        nVersion(SendCoinsRecipient::CURRENT_VERSION) {}

  QString address;
  QString label;
  CAmount amount;

  QString message;

  PaymentRequestPlus paymentRequest;

  QString authenticatedMerchant;

  bool fSubtractFeeFromAmount;

  static const int CURRENT_VERSION = 1;
  int nVersion;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    std::string sAddress = address.toStdString();
    std::string sLabel = label.toStdString();
    std::string sMessage = message.toStdString();
    std::string sPaymentRequest;
    if (!ser_action.ForRead() && paymentRequest.IsInitialized())
      paymentRequest.SerializeToString(&sPaymentRequest);
    std::string sAuthenticatedMerchant = authenticatedMerchant.toStdString();

    READWRITE(this->nVersion);
    READWRITE(sAddress);
    READWRITE(sLabel);
    READWRITE(amount);
    READWRITE(sMessage);
    READWRITE(sPaymentRequest);
    READWRITE(sAuthenticatedMerchant);

    if (ser_action.ForRead()) {
      address = QString::fromStdString(sAddress);
      label = QString::fromStdString(sLabel);
      message = QString::fromStdString(sMessage);
      if (!sPaymentRequest.empty())
        paymentRequest.parse(QByteArray::fromRawData(sPaymentRequest.data(),
                                                     sPaymentRequest.size()));
      authenticatedMerchant = QString::fromStdString(sAuthenticatedMerchant);
    }
  }
};

class WalletModel : public QObject {
  Q_OBJECT

public:
  explicit WalletModel(const PlatformStyle *platformStyle, CWallet *wallet,
                       OptionsModel *optionsModel, QObject *parent = 0);
  ~WalletModel();

  enum StatusCode

  {
    OK,
    InvalidAmount,
    InvalidAddress,
    AmountExceedsBalance,
    AmountWithFeeExceedsBalance,
    DuplicateAddress,
    TransactionCreationFailed,

    TransactionCommitFailed,
    AbsurdFee,
    PaymentRequestExpired
  };

  enum EncryptionStatus {
    Unencrypted,

    Locked,

    Unlocked

  };

  OptionsModel *getOptionsModel();
  AddressTableModel *getAddressTableModel();
  TransactionTableModel *getTransactionTableModel();
  RecentRequestsTableModel *getRecentRequestsTableModel();
  HiveTableModel *getHiveTableModel();

  CAmount getBalance(const CCoinControl *coinControl = nullptr) const;
  CAmount getUnconfirmedBalance() const;
  CAmount getImmatureBalance() const;
  bool haveWatchOnly() const;
  CAmount getWatchBalance() const;
  CAmount getWatchUnconfirmedBalance() const;
  CAmount getWatchImmatureBalance() const;
  EncryptionStatus getEncryptionStatus() const;

  bool validateAddress(const QString &address);

  struct SendCoinsReturn {
    SendCoinsReturn(StatusCode _status = OK, QString _reasonCommitFailed = "")
        : status(_status), reasonCommitFailed(_reasonCommitFailed) {}
    StatusCode status;
    QString reasonCommitFailed;
  };

  SendCoinsReturn prepareTransaction(WalletModelTransaction &transaction,
                                     const CCoinControl &coinControl);

  SendCoinsReturn sendCoins(WalletModelTransaction &transaction);

  bool setWalletEncrypted(bool encrypted, const SecureString &passphrase);

  bool setWalletLocked(bool locked,
                       const SecureString &passPhrase = SecureString());
  bool changePassphrase(const SecureString &oldPass,
                        const SecureString &newPass);

  bool backupWallet(const QString &filename);

  class UnlockContext {
  public:
    UnlockContext(WalletModel *wallet, bool valid, bool relock);
    ~UnlockContext();

    bool isValid() const { return valid; }

    UnlockContext(const UnlockContext &obj) { CopyFrom(obj); }
    UnlockContext &operator=(const UnlockContext &rhs) {
      CopyFrom(rhs);
      return *this;
    }

  private:
    WalletModel *wallet;
    bool valid;
    mutable bool relock;

    void CopyFrom(const UnlockContext &rhs);
  };

  UnlockContext requestUnlock(bool hiveOnly = false);

  bool getPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const;
  bool IsSpendable(const CTxDestination &dest) const;
  bool getPrivKey(const CKeyID &address, CKey &vchPrivKeyOut) const;
  void getOutputs(const std::vector<COutPoint> &vOutpoints,
                  std::vector<COutput> &vOutputs);
  bool isSpent(const COutPoint &outpoint) const;
  void listCoins(std::map<QString, std::vector<COutput>> &mapCoins) const;

  bool isLockedCoin(uint256 hash, unsigned int n) const;
  void lockCoin(COutPoint &output);
  void unlockCoin(COutPoint &output);
  void listLockedCoins(std::vector<COutPoint> &vOutpts);

  void loadReceiveRequests(std::vector<std::string> &vReceiveRequests);
  bool saveReceiveRequest(const std::string &sAddress, const int64_t nId,
                          const std::string &sRequest);

  void
  getBCTs(std::vector<CBeeCreationTransactionInfo> &vBeeCreationTransactions,
          bool includeDeadBees);

  bool createBees(int beeCount, bool communityContrib, QWidget *parent,
                  double beePopIndex);

  bool isHiveEnabled();

  bool transactionCanBeAbandoned(uint256 hash) const;
  bool abandonTransaction(uint256 hash) const;

  bool transactionCanBeBumped(uint256 hash) const;
  bool bumpFee(uint256 hash);

  static bool isWalletEnabled();

  bool hdEnabled() const;

  OutputType getDefaultAddressType() const;

  int getDefaultConfirmTarget() const;

private:
  CWallet *wallet;
  bool fHaveWatchOnly;
  bool fForceCheckBalanceChanged;

  OptionsModel *optionsModel;

  AddressTableModel *addressTableModel;
  TransactionTableModel *transactionTableModel;
  RecentRequestsTableModel *recentRequestsTableModel;
  HiveTableModel *hiveTableModel;

  CAmount cachedBalance;
  CAmount cachedUnconfirmedBalance;
  CAmount cachedImmatureBalance;
  CAmount cachedWatchOnlyBalance;
  CAmount cachedWatchUnconfBalance;
  CAmount cachedWatchImmatureBalance;
  EncryptionStatus cachedEncryptionStatus;
  int cachedNumBlocks;

  QTimer *pollTimer;

  void subscribeToCoreSignals();
  void unsubscribeFromCoreSignals();
  void checkBalanceChanged();

Q_SIGNALS:

  void balanceChanged(const CAmount &balance, const CAmount &unconfirmedBalance,
                      const CAmount &immatureBalance,
                      const CAmount &watchOnlyBalance,
                      const CAmount &watchUnconfBalance,
                      const CAmount &watchImmatureBalance);

  void encryptionStatusChanged(int status);

  void requireUnlock();

  void requireUnlockHive();

  void message(const QString &title, const QString &message,
               unsigned int style);

  void coinsSent(CWallet *wallet, SendCoinsRecipient recipient,
                 QByteArray transaction);

  void showProgress(const QString &title, int nProgress);

  void notifyWatchonlyChanged(bool fHaveWatchonly);

  void newHiveSummaryAvailable();

public Q_SLOTS:

  void updateStatus();

  void updateTransaction();

  void updateAddressBook(const QString &address, const QString &label,
                         bool isMine, const QString &purpose, int status);

  void updateWatchOnlyFlag(bool fHaveWatchonly);

  void pollBalanceChanged();
};

#endif
