// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H
#define BITCOIN_ZMQ_ZMQNOTIFICATIONINTERFACE_H

#include <list>
#include <map>
#include <string>
#include <validationinterface.h>

class CBlockIndex;
class CZMQAbstractNotifier;

class CZMQNotificationInterface final : public CValidationInterface {
public:
  virtual ~CZMQNotificationInterface();

  static CZMQNotificationInterface *Create();

protected:
  bool Initialize();
  void Shutdown();

  void TransactionAddedToMempool(const CTransactionRef &tx) override;
  void
  BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                 const CBlockIndex *pindexConnected,
                 const std::vector<CTransactionRef> &vtxConflicted) override;
  void BlockDisconnected(const std::shared_ptr<const CBlock> &pblock) override;
  void UpdatedBlockTip(const CBlockIndex *pindexNew,
                       const CBlockIndex *pindexFork,
                       bool fInitialDownload) override;

private:
  CZMQNotificationInterface();

  void *pcontext;
  std::list<CZMQAbstractNotifier *> notifiers;
};

#endif
