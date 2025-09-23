// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include <consensus/params.h>
#include <net.h>
#include <rialto.h>
#include <validationinterface.h>

static const int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;
static const int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;

static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;

static constexpr int32_t MAX_OUTBOUND_PEERS_TO_PROTECT_FROM_DISCONNECT = 4;
static constexpr int64_t CHAIN_SYNC_TIMEOUT = 20 * 60;
static constexpr int64_t EXTRA_PEER_CHECK_INTERVAL = 45;
static constexpr int64_t HEADERS_DOWNLOAD_TIMEOUT_BASE = 15 * 60 * 1000000;
static constexpr int64_t HEADERS_DOWNLOAD_TIMEOUT_PER_HEADER = 1000;
static constexpr int64_t MINIMUM_CONNECT_TIME = 30;
static constexpr int64_t STALE_CHECK_INTERVAL = 2.5 * 60;

class PeerLogicValidation : public CValidationInterface,
                            public NetEventsInterface {
private:
  CConnman *const connman;

public:
  explicit PeerLogicValidation(CConnman *connman, CScheduler &scheduler);

  void
  BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                 const CBlockIndex *pindexConnected,
                 const std::vector<CTransactionRef> &vtxConflicted) override;
  void UpdatedBlockTip(const CBlockIndex *pindexNew,
                       const CBlockIndex *pindexFork,
                       bool fInitialDownload) override;
  void BlockChecked(const CBlock &block,
                    const CValidationState &state) override;
  void NewPoWValidBlock(const CBlockIndex *pindex,
                        const std::shared_ptr<const CBlock> &pblock) override;

  void InitializeNode(CNode *pnode) override;
  void FinalizeNode(NodeId nodeid, bool &fUpdateConnectionTime) override;

  bool ProcessMessages(CNode *pfrom, std::atomic<bool> &interrupt) override;

  bool SendMessages(CNode *pto, std::atomic<bool> &interrupt) override;

  void ConsiderEviction(CNode *pto, int64_t time_in_seconds);
  void CheckForStaleTipAndEvictPeers(const Consensus::Params &consensusParams);
  void EvictExtraOutboundPeers(int64_t time_in_seconds);

private:
  int64_t m_stale_tip_check_time;
};

struct CNodeStateStats {
  int nMisbehavior;
  int nSyncHeight;
  int nCommonHeight;
  std::vector<int> vHeightInFlight;
};

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);

void Misbehaving(NodeId nodeid, int howmuch);
void RelayRialtoMessage(const CRialtoMessage message, CConnman *connman,
                        CNode *originNode = nullptr);

#endif