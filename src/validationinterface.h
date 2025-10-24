// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATIONINTERFACE_H
#define BITCOIN_VALIDATIONINTERFACE_H

#include <primitives/transaction.h>

#include <functional>
#include <memory>

class CBlock;
class CBlockIndex;
struct CBlockLocator;
class CBlockIndex;
class CConnman;
class CReserveScript;
class CValidationInterface;
class CValidationState;
class uint256;
class CScheduler;
class CTxMemPool;
enum class MemPoolRemovalReason;

void RegisterValidationInterface(CValidationInterface *pwalletIn);

void UnregisterValidationInterface(CValidationInterface *pwalletIn);

void UnregisterAllValidationInterfaces();

void CallFunctionInValidationInterfaceQueue(std::function<void()> func);

void SyncWithValidationInterfaceQueue();

class CValidationInterface {
protected:
  virtual void UpdatedBlockTip(const CBlockIndex *pindexNew,
                               const CBlockIndex *pindexFork,
                               bool fInitialDownload) {}

  virtual void TransactionAddedToMempool(const CTransactionRef &ptxn) {}

  virtual void TransactionRemovedFromMempool(const CTransactionRef &ptx) {}

  virtual void
  BlockConnected(const std::shared_ptr<const CBlock> &block,
                 const CBlockIndex *pindex,
                 const std::vector<CTransactionRef> &txnConflicted) {}

  virtual void BlockDisconnected(const std::shared_ptr<const CBlock> &block) {}

  virtual void SetBestChain(const CBlockLocator &locator) {}

  virtual void Inventory(const uint256 &hash) {}

  virtual void ResendWalletTransactions(int64_t nBestBlockTime,
                                        CConnman *connman) {}

  virtual void BlockChecked(const CBlock &, const CValidationState &) {}

  virtual void NewPoWValidBlock(const CBlockIndex *pindex,
                                const std::shared_ptr<const CBlock> &block) {};
  friend void ::RegisterValidationInterface(CValidationInterface *);
  friend void ::UnregisterValidationInterface(CValidationInterface *);
  friend void ::UnregisterAllValidationInterfaces();
};

struct MainSignalsInstance;
class CMainSignals {
private:
  std::unique_ptr<MainSignalsInstance> m_internals;

  friend void ::RegisterValidationInterface(CValidationInterface *);
  friend void ::UnregisterValidationInterface(CValidationInterface *);
  friend void ::UnregisterAllValidationInterfaces();
  friend void ::CallFunctionInValidationInterfaceQueue(
      std::function<void()> func);

  void MempoolEntryRemoved(CTransactionRef tx, MemPoolRemovalReason reason);

public:
  void RegisterBackgroundSignalScheduler(CScheduler &scheduler);

  void UnregisterBackgroundSignalScheduler();

  void FlushBackgroundCallbacks();

  size_t CallbacksPending();

  void RegisterWithMempoolSignals(CTxMemPool &pool);

  void UnregisterWithMempoolSignals(CTxMemPool &pool);

  void UpdatedBlockTip(const CBlockIndex *, const CBlockIndex *,
                       bool fInitialDownload);
  void TransactionAddedToMempool(const CTransactionRef &);
  void
  BlockConnected(const std::shared_ptr<const CBlock> &,
                 const CBlockIndex *pindex,
                 const std::shared_ptr<const std::vector<CTransactionRef>> &);
  void BlockDisconnected(const std::shared_ptr<const CBlock> &);
  void SetBestChain(const CBlockLocator &);
  void Inventory(const uint256 &);
  void Broadcast(int64_t nBestBlockTime, CConnman *connman);
  void BlockChecked(const CBlock &, const CValidationState &);
  void NewPoWValidBlock(const CBlockIndex *,
                        const std::shared_ptr<const CBlock> &);
};

CMainSignals &GetMainSignals();

#endif
