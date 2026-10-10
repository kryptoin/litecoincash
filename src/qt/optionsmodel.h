// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_OPTIONSMODEL_H
#define BITCOIN_QT_OPTIONSMODEL_H

#include <amount.h>

#include <QAbstractListModel>

QT_BEGIN_NAMESPACE
class QNetworkProxy;
QT_END_NAMESPACE

extern const char *DEFAULT_GUI_PROXY_HOST;
static constexpr unsigned short DEFAULT_GUI_PROXY_PORT = 9050;

class OptionsModel : public QAbstractListModel {
  Q_OBJECT

public:
  explicit OptionsModel(QObject *parent = 0, bool resetSettings = false);

  enum OptionID {
    StartAtStartup,

    HideTrayIcon,

    MinimizeToTray,

    MapPortUPnP,

    MinimizeOnClose,

    ProxyUse,

    ProxyIP,

    ProxyPort,

    ProxyUseTor,

    ProxyIPTor,

    ProxyPortTor,

    DisplayUnit,

    ThirdPartyTxUrls,

    Language,

    CoinControlFeatures,

    ThreadsScriptVerif,

    DatabaseCache,

    SpendZeroConfChange,

    Listen,

    HiveCheckDelay,

    HiveCheckThreads,

    HiveCheckEarlyOut,

    HiveContribCF,

    OptionIDRowCount,
  };

  void Init(bool resetSettings = false);
  void Reset();

  int rowCount(const QModelIndex &parent = QModelIndex()) const;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
  bool setData(const QModelIndex &index, const QVariant &value,
               int role = Qt::EditRole);

  void setDisplayUnit(const QVariant &value);

  bool getHideTrayIcon() const { return fHideTrayIcon; }
  bool getMinimizeToTray() const { return fMinimizeToTray; }
  bool getMinimizeOnClose() const { return fMinimizeOnClose; }
  int getDisplayUnit() const { return nDisplayUnit; }
  QString getThirdPartyTxUrls() const { return strThirdPartyTxUrls; }
  bool getProxySettings(QNetworkProxy &proxy) const;
  bool getCoinControlFeatures() const { return fCoinControlFeatures; }
  const QString &getOverriddenByCommandLine() {
    return strOverriddenByCommandLine;
  }
  bool getHiveContribCF() const { return fHiveContribCF; }

  void setRestartRequired(bool fRequired);
  bool isRestartRequired() const;

private:
  bool fHideTrayIcon;
  bool fMinimizeToTray;
  bool fMinimizeOnClose;
  bool fHiveContribCF;

  QString language;
  int nDisplayUnit;
  QString strThirdPartyTxUrls;
  bool fCoinControlFeatures;

  QString strOverriddenByCommandLine;

  void addOverriddenOption(const std::string &option);

  void checkAndMigrate();
Q_SIGNALS:
  void displayUnitChanged(int unit);
  void coinControlFeaturesChanged(bool);
  void hideTrayIconChanged(bool);
};

#endif
