// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONFILTERPROXY_H
#define BITCOIN_QT_TRANSACTIONFILTERPROXY_H

#include <amount.h>

#include <QDateTime>
#include <QSortFilterProxyModel>

class TransactionFilterProxy : public QSortFilterProxyModel {
  Q_OBJECT

public:
  explicit TransactionFilterProxy(QObject *parent = 0);

  static const QDateTime MIN_DATE;

  static const QDateTime MAX_DATE;

  static const quint32 ALL_TYPES = 0xFFFFFFFF;

  static quint32 TYPE(int type) { return 1 << type; }

  enum WatchOnlyFilter {
    WatchOnlyFilter_All,
    WatchOnlyFilter_Yes,
    WatchOnlyFilter_No
  };

  void setDateRange(const QDateTime &from, const QDateTime &to);
  void setSearchString(const QString &);

  void setTypeFilter(quint32 modes);
  void setMinAmount(const CAmount &minimum);
  void setWatchOnlyFilter(WatchOnlyFilter filter);

  void setLimit(int limit);

  void setShowInactive(bool showInactive);

  int rowCount(const QModelIndex &parent = QModelIndex()) const;

protected:
  bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const;

private:
  QDateTime dateFrom;
  QDateTime dateTo;
  QString m_search_string;
  quint32 typeFilter;
  WatchOnlyFilter watchOnlyFilter;
  CAmount minAmount;
  int limitRows;
  bool showInactive;
};

#endif
