// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_CSVMODELWRITER_H
#define BITCOIN_QT_CSVMODELWRITER_H

#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QAbstractItemModel;
QT_END_NAMESPACE

class CSVModelWriter : public QObject {
  Q_OBJECT

public:
  explicit CSVModelWriter(const QString &filename, QObject *parent = 0);

  void setModel(const QAbstractItemModel *model);
  void addColumn(const QString &title, int column, int role = Qt::EditRole);

  bool write();

private:
  QString filename;
  const QAbstractItemModel *model;

  struct Column {
    QString title;
    int column;
    int role;
  };
  QList<Column> columns;
};

#endif
