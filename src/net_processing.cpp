// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <net_processing.h>

#include <addrman.h>
#include <arith_uint256.h>
#include <blockencodings.h>
#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <hash.h>
#include <init.h>
#include <merkleblock.h>
#include <netbase.h>
#include <netmessagemaker.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <random.h>
#include <reverse_iterator.h>
#include <rialto.h>
#include <scheduler.h>
#include <tinyformat.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <util.h>
#include <utilmoneystr.h>
#include <utilstrencodings.h>
#include <validation.h>

#if defined(NDEBUG)
#error "LitecoinCash cannot be compiled without assertions."
#endif

std::atomic<int64_t> nTimeBestReceived(0);

struct IteratorComparator {
  template <typename I> bool operator()(const I &a, const I &b) const {
    return &(*a) < &(*b);
  }
};

struct COrphanTx {
  CTransactionRef tx;
  NodeId fromPeer;
  int64_t nTimeExpire;
};
static CCriticalSection g_cs_orphans;
std::map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(g_cs_orphans);
std::map<COutPoint,
         std::set<std::map<uint256, COrphanTx>::iterator, IteratorComparator>>
    mapOrphanTransactionsByPrev GUARDED_BY(g_cs_orphans);
void EraseOrphansFor(NodeId peer);

static size_t vExtraTxnForCompactIt GUARDED_BY(g_cs_orphans) = 0;
static std::vector<std::pair<uint256, CTransactionRef>>
    vExtraTxnForCompact GUARDED_BY(g_cs_orphans);

static const uint64_t RANDOMIZER_ID_ADDRESS_RELAY = 0x3cac0035b5866b90ULL;

static const int STALE_RELAY_AGE_LIMIT = 30 * 24 * 60 * 60;

static const int HISTORICAL_BLOCK_AGE = 7 * 24 * 60 * 60;

namespace {
int nSyncStarted = 0;

std::map<uint256, std::pair<NodeId, bool>> mapBlockSource;

std::unique_ptr<CRollingBloomFilter> recentRejects;
uint256 hashRecentRejectsChainTip;

struct QueuedBlock {
  uint256 hash;
  const CBlockIndex *pindex;

  bool fValidatedHeaders;

  std::unique_ptr<PartiallyDownloadedBlock> partialBlock;
};
std::map<uint256, std::pair<NodeId, std::list<QueuedBlock>::iterator>>
    mapBlocksInFlight;

std::list<NodeId> lNodesAnnouncingHeaderAndIDs;

int nPreferredDownload = 0;

int nPeersWithValidatedDownloads = 0;

int g_outbound_peers_with_protect_from_disconnect = 0;

std::atomic<int64_t> g_last_tip_update(0);

typedef std::map<uint256, CTransactionRef> MapRelay;
MapRelay mapRelay;

std::deque<std::pair<int64_t, MapRelay::iterator>> vRelayExpiration;

typedef std::map<uint256, std::string> MapMessageRelay;
MapMessageRelay mapMessageRelay;

std::deque<std::pair<int64_t, MapMessageRelay::iterator>>
    vMessageRelayExpiration;
} // namespace

namespace {
struct CBlockReject {
  unsigned char chRejectCode;
  std::string strRejectReason;
  uint256 hashBlock;
};

struct CNodeState {
  const CService address;

  bool fCurrentlyConnected;

  int nMisbehavior;

  bool fShouldBan;

  const std::string name;

  std::vector<CBlockReject> rejects;

  const CBlockIndex *pindexBestKnownBlock;

  uint256 hashLastUnknownBlock;

  const CBlockIndex *pindexLastCommonBlock;

  const CBlockIndex *pindexBestHeaderSent;

  int nUnconnectingHeaders;

  bool fSyncStarted;

  int64_t nHeadersSyncTimeout;

  int64_t nStallingSince;
  std::list<QueuedBlock> vBlocksInFlight;

  int64_t nDownloadingSince;
  int nBlocksInFlight;
  int nBlocksInFlightValidHeaders;

  bool fPreferredDownload;

  bool fPreferHeaders;

  bool fPreferHeaderAndIDs;

  bool fProvidesHeaderAndIDs;

  bool fHaveWitness;

  bool fWantsCmpctWitness;

  bool fSupportsDesiredCmpctVersion;

  struct ChainSyncTimeoutState {
    int64_t m_timeout;

    const CBlockIndex *m_work_header;

    bool m_sent_getheaders;

    bool m_protect;
  };

  ChainSyncTimeoutState m_chain_sync;

  int64_t m_last_block_announcement;

  // Introspection hardening: Track suspicious chain mapping behavior
  int nIntrospectionScore;
  int64_t nLastIntrospectionTime;
  int nRecentHeaderRequests;
  int64_t nHeaderRequestWindow;
  int nStaleForkAnnouncements;
  int64_t nLastStaleForkTime;

  // Hardening: Rate limiting trackers
  int64_t nLastInvTime;
  int nInvCount;
  int64_t nLastGetHeadersTime;
  int nGetHeadersCount;
  int64_t nLastMempoolReqTime;
  int nOrphanCount;

  // Phase 2 Hardening: Additional rate limiting
  int64_t nLastAddrTime;
  int nAddrCount;
  int64_t nLastFilterLoadTime;
  int nFilterLoadCount;
  int64_t nLastRejectTime;
  int nRejectCount;
  int nNotFoundCount;
  int64_t nLastNotFoundTime;
  int nSendCmpctCount;
  int nPongMismatchCount;

  CNodeState(CAddress addrIn, std::string addrNameIn)
      : address(addrIn), name(addrNameIn) {
    fCurrentlyConnected = false;
    fHaveWitness = false;
    fPreferHeaderAndIDs = false;
    fPreferHeaders = false;
    fPreferredDownload = false;
    fProvidesHeaderAndIDs = false;
    fShouldBan = false;
    fSupportsDesiredCmpctVersion = false;
    fSyncStarted = false;
    fWantsCmpctWitness = false;
    hashLastUnknownBlock.SetNull();
    m_chain_sync = {0, nullptr, false, false};
    m_last_block_announcement = 0;
    nBlocksInFlight = 0;
    nBlocksInFlightValidHeaders = 0;
    nDownloadingSince = 0;
    nHeadersSyncTimeout = 0;
    nMisbehavior = 0;
    nStallingSince = 0;
    nUnconnectingHeaders = 0;
    nIntrospectionScore = 0;
    nLastIntrospectionTime = 0;
    nRecentHeaderRequests = 0;
    nHeaderRequestWindow = GetTime();
    nStaleForkAnnouncements = 0;
    nLastStaleForkTime = 0;
    nLastInvTime = 0;
    nInvCount = 0;
    nLastGetHeadersTime = 0;
    nGetHeadersCount = 0;
    nLastMempoolReqTime = 0;
    nOrphanCount = 0;
    nLastAddrTime = 0;
    nAddrCount = 0;
    nLastFilterLoadTime = 0;
    nFilterLoadCount = 0;
    nLastRejectTime = 0;
    nRejectCount = 0;
    nNotFoundCount = 0;
    nLastNotFoundTime = 0;
    nSendCmpctCount = 0;
    nPongMismatchCount = 0;
    pindexBestHeaderSent = nullptr;
    pindexBestKnownBlock = nullptr;
    pindexLastCommonBlock = nullptr;
  }
};

std::map<NodeId, CNodeState> mapNodeState;

CNodeState *State(NodeId pnode) {
  std::map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
  if (it == mapNodeState.end())
    return nullptr;
  return &it->second;
}

void UpdatePreferredDownload(CNode *node, CNodeState *state) {
  nPreferredDownload -= state->fPreferredDownload;

  state->fPreferredDownload = (!node->fInbound || node->fWhitelisted) &&
                              !node->fOneShot && !node->fClient;

  nPreferredDownload += state->fPreferredDownload;
}

void PushNodeVersion(CNode *pnode, CConnman *connman, int64_t nTime) {
  ServiceFlags nLocalNodeServices = pnode->GetLocalServices();
  uint64_t nonce = pnode->GetLocalNonce();
  int nNodeStartingHeight = pnode->GetMyStartingHeight();
  NodeId nodeid = pnode->GetId();
  CAddress addr = pnode->addr;

  CAddress addrYou = (addr.IsRoutable() && !IsProxy(addr)
                          ? addr
                          : CAddress(CService(), addr.nServices));
  CAddress addrMe = CAddress(CService(), nLocalNodeServices);

  connman->PushMessage(pnode, CNetMsgMaker(INIT_PROTO_VERSION)
                                  .Make(NetMsgType::VERSION, PROTOCOL_VERSION,
                                        (uint64_t)nLocalNodeServices, nTime,
                                        addrYou, addrMe, nonce, strSubVersion,
                                        nNodeStartingHeight, ::fRelayTxes));

  if (fLogIPs) {
    LogPrint(BCLog::NET,
             "send version message: version %d, blocks=%d, us=%s, them=%s, "
             "peer=%d\n",
             PROTOCOL_VERSION, nNodeStartingHeight, addrMe.ToString(),
             addrYou.ToString(), nodeid);
  } else {
    LogPrint(BCLog::NET,
             "send version message: version %d, blocks=%d, us=%s, peer=%d\n",
             PROTOCOL_VERSION, nNodeStartingHeight, addrMe.ToString(), nodeid);
  }
}

bool MarkBlockAsReceived(const uint256 &hash) {
  std::map<uint256,
           std::pair<NodeId, std::list<QueuedBlock>::iterator>>::iterator
      itInFlight = mapBlocksInFlight.find(hash);
  if (itInFlight != mapBlocksInFlight.end()) {
    CNodeState *state = State(itInFlight->second.first);
    assert(state != nullptr);
    state->nBlocksInFlightValidHeaders -=
        itInFlight->second.second->fValidatedHeaders;
    if (state->nBlocksInFlightValidHeaders == 0 &&
        itInFlight->second.second->fValidatedHeaders) {
      nPeersWithValidatedDownloads--;
    }
    if (state->vBlocksInFlight.begin() == itInFlight->second.second) {
      state->nDownloadingSince =
          std::max(state->nDownloadingSince, GetTimeMicros());
    }
    state->vBlocksInFlight.erase(itInFlight->second.second);
    state->nBlocksInFlight--;
    state->nStallingSince = 0;
    mapBlocksInFlight.erase(itInFlight);
    return true;
  }
  return false;
}

bool MarkBlockAsInFlight(NodeId nodeid, const uint256 &hash,
                         const CBlockIndex *pindex = nullptr,
                         std::list<QueuedBlock>::iterator **pit = nullptr) {
  CNodeState *state = State(nodeid);
  assert(state != nullptr);

  std::map<uint256,
           std::pair<NodeId, std::list<QueuedBlock>::iterator>>::iterator
      itInFlight = mapBlocksInFlight.find(hash);
  if (itInFlight != mapBlocksInFlight.end() &&
      itInFlight->second.first == nodeid) {
    if (pit) {
      *pit = &itInFlight->second.second;
    }
    return false;
  }

  MarkBlockAsReceived(hash);

  std::list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(
      state->vBlocksInFlight.end(),
      {hash, pindex, pindex != nullptr,
       std::unique_ptr<PartiallyDownloadedBlock>(
           pit ? new PartiallyDownloadedBlock(&mempool) : nullptr)});
  state->nBlocksInFlight++;
  state->nBlocksInFlightValidHeaders += it->fValidatedHeaders;
  if (state->nBlocksInFlight == 1) {
    state->nDownloadingSince = GetTimeMicros();
  }
  if (state->nBlocksInFlightValidHeaders == 1 && pindex != nullptr) {
    nPeersWithValidatedDownloads++;
  }
  itInFlight =
      mapBlocksInFlight.insert(std::make_pair(hash, std::make_pair(nodeid, it)))
          .first;
  if (pit)
    *pit = &itInFlight->second.second;
  return true;
}

void ProcessBlockAvailability(NodeId nodeid) {
  CNodeState *state = State(nodeid);
  assert(state != nullptr);

  if (!state->hashLastUnknownBlock.IsNull()) {
    BlockMap::iterator itOld = mapBlockIndex.find(state->hashLastUnknownBlock);
    if (itOld != mapBlockIndex.end() && itOld->second->nChainWork > 0) {
      if (state->pindexBestKnownBlock == nullptr ||
          itOld->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
        state->pindexBestKnownBlock = itOld->second;
      state->hashLastUnknownBlock.SetNull();
    }
  }
}

void UpdateBlockAvailability(NodeId nodeid, const uint256 &hash) {
  CNodeState *state = State(nodeid);
  assert(state != nullptr);

  ProcessBlockAvailability(nodeid);

  BlockMap::iterator it = mapBlockIndex.find(hash);
  if (it != mapBlockIndex.end() && it->second->nChainWork > 0) {
    if (state->pindexBestKnownBlock == nullptr ||
        it->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
      state->pindexBestKnownBlock = it->second;
  } else {
    state->hashLastUnknownBlock = hash;
  }
}

void MaybeSetPeerAsAnnouncingHeaderAndIDs(NodeId nodeid, CConnman *connman) {
  AssertLockHeld(cs_main);
  CNodeState *nodestate = State(nodeid);
  if (!nodestate || !nodestate->fSupportsDesiredCmpctVersion) {
    return;
  }
  if (nodestate->fProvidesHeaderAndIDs) {
    for (std::list<NodeId>::iterator it = lNodesAnnouncingHeaderAndIDs.begin();
         it != lNodesAnnouncingHeaderAndIDs.end(); it++) {
      if (*it == nodeid) {
        lNodesAnnouncingHeaderAndIDs.erase(it);
        lNodesAnnouncingHeaderAndIDs.push_back(nodeid);
        return;
      }
    }
    connman->ForNode(nodeid, [connman](CNode *pfrom) {
      uint64_t nCMPCTBLOCKVersion =
          (pfrom->GetLocalServices() & NODE_WITNESS) ? 2 : 1;
      if (lNodesAnnouncingHeaderAndIDs.size() >= 3) {
        connman->ForNode(lNodesAnnouncingHeaderAndIDs.front(),
                         [connman, nCMPCTBLOCKVersion](CNode *pnodeStop) {
                           connman->PushMessage(
                               pnodeStop,
                               CNetMsgMaker(pnodeStop->GetSendVersion())
                                   .Make(NetMsgType::SENDCMPCT,

                                         false, nCMPCTBLOCKVersion));
                           return true;
                         });
        lNodesAnnouncingHeaderAndIDs.pop_front();
      }
      connman->PushMessage(pfrom, CNetMsgMaker(pfrom->GetSendVersion())
                                      .Make(NetMsgType::SENDCMPCT,

                                            true, nCMPCTBLOCKVersion));
      lNodesAnnouncingHeaderAndIDs.push_back(pfrom->GetId());
      return true;
    });
  }
}

bool TipMayBeStale(const Consensus::Params &consensusParams) {
  AssertLockHeld(cs_main);
  if (g_last_tip_update == 0) {
    g_last_tip_update = GetTime();
  }
  return g_last_tip_update <
             GetTime() - consensusParams.nPowTargetSpacing * 3 &&
         mapBlocksInFlight.empty();
}

bool CanDirectFetch(const Consensus::Params &consensusParams) {
  return chainActive.Tip()->GetBlockTime() >
         GetAdjustedTime() - consensusParams.nPowTargetSpacing * 20;
}

bool PeerHasHeader(CNodeState *state, const CBlockIndex *pindex) {
  if (state->pindexBestKnownBlock &&
      pindex == state->pindexBestKnownBlock->GetAncestor(pindex->nHeight))
    return true;
  if (state->pindexBestHeaderSent &&
      pindex == state->pindexBestHeaderSent->GetAncestor(pindex->nHeight))
    return true;
  return false;
}

void FindNextBlocksToDownload(NodeId nodeid, unsigned int count,
                              std::vector<const CBlockIndex *> &vBlocks,
                              NodeId &nodeStaller,
                              const Consensus::Params &consensusParams) {
  if (count == 0)
    return;

  vBlocks.reserve(vBlocks.size() + count);
  CNodeState *state = State(nodeid);
  assert(state != nullptr);

  ProcessBlockAvailability(nodeid);

  if (state->pindexBestKnownBlock == nullptr ||
      state->pindexBestKnownBlock->nChainWork < chainActive.Tip()->nChainWork ||
      state->pindexBestKnownBlock->nChainWork < nMinimumChainWork) {
    return;
  }

  if (state->pindexLastCommonBlock == nullptr) {
    state->pindexLastCommonBlock = chainActive[std::min(
        state->pindexBestKnownBlock->nHeight, chainActive.Height())];
  }

  state->pindexLastCommonBlock = LastCommonAncestor(
      state->pindexLastCommonBlock, state->pindexBestKnownBlock);
  if (state->pindexLastCommonBlock == state->pindexBestKnownBlock)
    return;

  std::vector<const CBlockIndex *> vToFetch;
  const CBlockIndex *pindexWalk = state->pindexLastCommonBlock;

  int nWindowEnd =
      state->pindexLastCommonBlock->nHeight + BLOCK_DOWNLOAD_WINDOW;
  int nMaxHeight =
      std::min<int>(state->pindexBestKnownBlock->nHeight, nWindowEnd + 1);
  NodeId waitingfor = -1;
  while (pindexWalk->nHeight < nMaxHeight) {
    int nToFetch = std::min(nMaxHeight - pindexWalk->nHeight,
                            std::max<int>(count - vBlocks.size(), 128));
    vToFetch.resize(nToFetch);
    pindexWalk = state->pindexBestKnownBlock->GetAncestor(pindexWalk->nHeight +
                                                          nToFetch);
    vToFetch[nToFetch - 1] = pindexWalk;
    for (unsigned int i = nToFetch - 1; i > 0; i--) {
      vToFetch[i - 1] = vToFetch[i]->pprev;
    }

    for (const CBlockIndex *pindex : vToFetch) {
      if (!pindex->IsValid(BLOCK_VALID_TREE)) {
        return;
      }
      if (!State(nodeid)->fHaveWitness &&
          IsWitnessEnabled(pindex->pprev, consensusParams)) {
        return;
      }
      if (pindex->nStatus & BLOCK_HAVE_DATA || chainActive.Contains(pindex)) {
        if (pindex->nChainTx)
          state->pindexLastCommonBlock = pindex;
      } else if (mapBlocksInFlight.count(pindex->GetBlockHash()) == 0) {
        if (pindex->nHeight > nWindowEnd) {
          if (vBlocks.size() == 0 && waitingfor != nodeid) {
            nodeStaller = waitingfor;
          }
          return;
        }
        vBlocks.push_back(pindex);
        if (vBlocks.size() == count) {
          return;
        }
      } else if (waitingfor == -1) {
        waitingfor = mapBlocksInFlight[pindex->GetBlockHash()].first;
      }
    }
  }
}

} // namespace

void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds) {
  LOCK(cs_main);
  CNodeState *state = State(node);
  if (state)
    state->m_last_block_announcement = time_in_seconds;
}

bool IsOutboundDisconnectionCandidate(const CNode *node) {
  return !(node->fInbound || node->m_manual_connection || node->fFeeler ||
           node->fOneShot);
}

void PeerLogicValidation::InitializeNode(CNode *pnode) {
  CAddress addr = pnode->addr;
  std::string addrName = pnode->GetAddrName();
  NodeId nodeid = pnode->GetId();
  {
    LOCK(cs_main);
    mapNodeState.emplace_hint(mapNodeState.end(), std::piecewise_construct,
                              std::forward_as_tuple(nodeid),
                              std::forward_as_tuple(addr, std::move(addrName)));
  }
  if (!pnode->fInbound)
    PushNodeVersion(pnode, connman, GetTime());
}

void PeerLogicValidation::FinalizeNode(NodeId nodeid,
                                       bool &fUpdateConnectionTime) {
  fUpdateConnectionTime = false;
  LOCK(cs_main);
  CNodeState *state = State(nodeid);
  assert(state != nullptr);

  if (state->fSyncStarted)
    nSyncStarted--;

  if (state->nMisbehavior == 0 && state->fCurrentlyConnected) {
    fUpdateConnectionTime = true;
  }

  for (const QueuedBlock &entry : state->vBlocksInFlight) {
    mapBlocksInFlight.erase(entry.hash);
  }
  EraseOrphansFor(nodeid);
  nPreferredDownload -= state->fPreferredDownload;
  nPeersWithValidatedDownloads -= (state->nBlocksInFlightValidHeaders != 0);
  assert(nPeersWithValidatedDownloads >= 0);
  g_outbound_peers_with_protect_from_disconnect -=
      state->m_chain_sync.m_protect;
  assert(g_outbound_peers_with_protect_from_disconnect >= 0);

  mapNodeState.erase(nodeid);

  if (mapNodeState.empty()) {
    assert(mapBlocksInFlight.empty());
    assert(nPreferredDownload == 0);
    assert(nPeersWithValidatedDownloads == 0);
    assert(g_outbound_peers_with_protect_from_disconnect == 0);
  }
  LogPrint(BCLog::NET, "Cleared nodestate for peer=%d\n", nodeid);
}

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats) {
  LOCK(cs_main);
  CNodeState *state = State(nodeid);
  if (state == nullptr)
    return false;
  stats.nMisbehavior = state->nMisbehavior;
  stats.nSyncHeight =
      state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
  stats.nCommonHeight =
      state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;
  for (const QueuedBlock &queue : state->vBlocksInFlight) {
    if (queue.pindex)
      stats.vHeightInFlight.push_back(queue.pindex->nHeight);
  }
  return true;
}

void AddToCompactExtraTransactions(const CTransactionRef &tx)
    EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans) {
  size_t max_extra_txn = gArgs.GetArg("-blockreconstructionextratxn",
                                      DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN);
  if (max_extra_txn <= 0)
    return;
  if (!vExtraTxnForCompact.size())
    vExtraTxnForCompact.resize(max_extra_txn);
  vExtraTxnForCompact[vExtraTxnForCompactIt] =
      std::make_pair(tx->GetWitnessHash(), tx);
  vExtraTxnForCompactIt = (vExtraTxnForCompactIt + 1) % max_extra_txn;
}

bool AddOrphanTx(const CTransactionRef &tx, NodeId peer)
    EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans) {
  // Hardening: Limit orphans per peer
  CNodeState *state = State(peer);
  if (state && state->nOrphanCount >= 100) {
    LogPrint(BCLog::MEMPOOL,
             "ignoring orphan tx from peer=%d (quota exceeded)\n", peer);
    return false;
  }

  const uint256 &hash = tx->GetHash();
  if (mapOrphanTransactions.count(hash))
    return false;

  unsigned int sz = GetTransactionWeight(*tx);
  if (sz >= MAX_STANDARD_TX_WEIGHT) {
    LogPrint(BCLog::MEMPOOL, "ignoring large orphan tx (size: %u, hash: %s)\n",
             sz, hash.ToString());
    return false;
  }

  auto ret = mapOrphanTransactions.emplace(
      hash, COrphanTx{tx, peer, GetTime() + ORPHAN_TX_EXPIRE_TIME});
  assert(ret.second);
  for (const CTxIn &txin : tx->vin) {
    mapOrphanTransactionsByPrev[txin.prevout].insert(ret.first);
  }

  AddToCompactExtraTransactions(tx);

  LogPrint(BCLog::MEMPOOL, "stored orphan tx %s (mapsz %u outsz %u)\n",
           hash.ToString(), mapOrphanTransactions.size(),
           mapOrphanTransactionsByPrev.size());
  if (state)
    state->nOrphanCount++;
  return true;
}

int static EraseOrphanTx(uint256 hash) EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans) {
  std::map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
  if (it == mapOrphanTransactions.end())
    return 0;

  // Hardening: Decrement orphan count
  NodeId peer = it->second.fromPeer;
  CNodeState *state = State(peer);
  if (state && state->nOrphanCount > 0)
    state->nOrphanCount--;

  for (const CTxIn &txin : it->second.tx->vin) {
    auto itPrev = mapOrphanTransactionsByPrev.find(txin.prevout);
    if (itPrev == mapOrphanTransactionsByPrev.end())
      continue;
    itPrev->second.erase(it);
    if (itPrev->second.empty())
      mapOrphanTransactionsByPrev.erase(itPrev);
  }
  mapOrphanTransactions.erase(it);
  return 1;
}

void EraseOrphansFor(NodeId peer) {
  LOCK(g_cs_orphans);
  int nErased = 0;
  std::map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
  while (iter != mapOrphanTransactions.end()) {
    std::map<uint256, COrphanTx>::iterator maybeErase = iter++;

    if (maybeErase->second.fromPeer == peer) {
      nErased += EraseOrphanTx(maybeErase->second.tx->GetHash());
    }
  }
  if (nErased > 0)
    LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx from peer=%d\n", nErased,
             peer);
}

unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans) {
  LOCK(g_cs_orphans);

  unsigned int nEvicted = 0;
  static int64_t nNextSweep;
  int64_t nNow = GetTime();
  if (nNextSweep <= nNow) {
    int nErased = 0;
    int64_t nMinExpTime =
        nNow + ORPHAN_TX_EXPIRE_TIME - ORPHAN_TX_EXPIRE_INTERVAL;
    std::map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end()) {
      std::map<uint256, COrphanTx>::iterator maybeErase = iter++;
      if (maybeErase->second.nTimeExpire <= nNow) {
        nErased += EraseOrphanTx(maybeErase->second.tx->GetHash());
      } else {
        nMinExpTime = std::min(maybeErase->second.nTimeExpire, nMinExpTime);
      }
    }

    nNextSweep = nMinExpTime + ORPHAN_TX_EXPIRE_INTERVAL;
    if (nErased > 0)
      LogPrint(BCLog::MEMPOOL, "Erased %d orphan tx due to expiration\n",
               nErased);
  }
  // Phase 2 Hardening: Evict orphans from misbehaving peers first
  while (mapOrphanTransactions.size() > nMaxOrphans) {
    int nHighestMisbehavior = -1;
    std::map<uint256, COrphanTx>::iterator itToErase =
        mapOrphanTransactions.end();

    // Find orphan from peer with highest misbehavior score
    for (auto it = mapOrphanTransactions.begin();
         it != mapOrphanTransactions.end(); ++it) {
      CNodeState *peerState = State(it->second.fromPeer);
      int peerMisbehavior = peerState ? peerState->nMisbehavior : 0;
      if (peerMisbehavior > nHighestMisbehavior) {
        nHighestMisbehavior = peerMisbehavior;
        itToErase = it;
      }
    }

    // Fallback to random if no misbehaving peer found
    if (itToErase == mapOrphanTransactions.end() || nHighestMisbehavior == 0) {
      uint256 randomhash = GetRandHash();
      itToErase = mapOrphanTransactions.lower_bound(randomhash);
      if (itToErase == mapOrphanTransactions.end())
        itToErase = mapOrphanTransactions.begin();
    }

    EraseOrphanTx(itToErase->first);
    ++nEvicted;
  }
  return nEvicted;
}

void Misbehaving(NodeId pnode, int howmuch) {
  if (howmuch == 0)
    return;

  CNodeState *state = State(pnode);
  if (state == nullptr)
    return;

  state->nMisbehavior += howmuch;
  int banscore = gArgs.GetArg("-banscore", DEFAULT_BANSCORE_THRESHOLD);
  if (state->nMisbehavior >= banscore &&
      state->nMisbehavior - howmuch < banscore) {
    LogPrintf("%s: %s peer=%d (%d -> %d) BAN THRESHOLD EXCEEDED\n", __func__,
              state->name, pnode, state->nMisbehavior - howmuch,
              state->nMisbehavior);
    state->fShouldBan = true;
  } else
    LogPrintf("%s: %s peer=%d (%d -> %d)\n", __func__, state->name, pnode,
              state->nMisbehavior - howmuch, state->nMisbehavior);
}

static bool BlockRequestAllowed(const CBlockIndex *pindex,
                                const Consensus::Params &consensusParams) {
  AssertLockHeld(cs_main);
  if (chainActive.Contains(pindex))
    return true;
  return pindex->IsValid(BLOCK_VALID_SCRIPTS) &&
         (pindexBestHeader != nullptr) &&
         (pindexBestHeader->GetBlockTime() - pindex->GetBlockTime() <
          STALE_RELAY_AGE_LIMIT) &&
         (GetBlockProofEquivalentTime(*pindexBestHeader, *pindex,
                                      *pindexBestHeader,
                                      consensusParams) < STALE_RELAY_AGE_LIMIT);
}

PeerLogicValidation::PeerLogicValidation(CConnman *connmanIn,
                                         CScheduler &scheduler)
    : connman(connmanIn), m_stale_tip_check_time(0) {
  recentRejects.reset(new CRollingBloomFilter(120000, 0.000001));

  const Consensus::Params &consensusParams = Params().GetConsensus();

  static_assert(
      EXTRA_PEER_CHECK_INTERVAL < STALE_CHECK_INTERVAL,
      "peer eviction timer should be less than stale tip check timer");
  scheduler.scheduleEvery(
      std::bind(&PeerLogicValidation::CheckForStaleTipAndEvictPeers, this,
                consensusParams),
      EXTRA_PEER_CHECK_INTERVAL * 1000);
}

void PeerLogicValidation::BlockConnected(
    const std::shared_ptr<const CBlock> &pblock, const CBlockIndex *pindex,
    const std::vector<CTransactionRef> &vtxConflicted) {
  LOCK(g_cs_orphans);

  std::vector<uint256> vOrphanErase;

  for (const CTransactionRef &ptx : pblock->vtx) {
    const CTransaction &tx = *ptx;

    for (const auto &txin : tx.vin) {
      auto itByPrev = mapOrphanTransactionsByPrev.find(txin.prevout);
      if (itByPrev == mapOrphanTransactionsByPrev.end())
        continue;
      for (auto mi = itByPrev->second.begin(); mi != itByPrev->second.end();
           ++mi) {
        const CTransaction &orphanTx = *(*mi)->second.tx;
        const uint256 &orphanHash = orphanTx.GetHash();
        vOrphanErase.push_back(orphanHash);
      }
    }
  }

  if (vOrphanErase.size()) {
    int nErased = 0;
    for (uint256 &orphanHash : vOrphanErase) {
      nErased += EraseOrphanTx(orphanHash);
    }
    LogPrint(BCLog::MEMPOOL,
             "Erased %d orphan tx included or conflicted by block\n", nErased);
  }

  g_last_tip_update = GetTime();
}

static bool fWitnessesPresentInMostRecentCompactBlock;
static CCriticalSection cs_most_recent_block;
static std::shared_ptr<const CBlock> most_recent_block;
static std::shared_ptr<const CBlockHeaderAndShortTxIDs>
    most_recent_compact_block;
static uint256 most_recent_block_hash;

void PeerLogicValidation::NewPoWValidBlock(
    const CBlockIndex *pindex, const std::shared_ptr<const CBlock> &pblock) {
  std::shared_ptr<const CBlockHeaderAndShortTxIDs> pcmpctblock =
      std::make_shared<const CBlockHeaderAndShortTxIDs>(*pblock, true);
  const CNetMsgMaker msgMaker(PROTOCOL_VERSION);

  LOCK(cs_main);

  static int nHighestFastAnnounce = 0;
  if (pindex->nHeight <= nHighestFastAnnounce)
    return;
  nHighestFastAnnounce = pindex->nHeight;

  bool fWitnessEnabled =
      IsWitnessEnabled(pindex->pprev, Params().GetConsensus());
  uint256 hashBlock(pblock->GetHash());

  {
    LOCK(cs_most_recent_block);
    most_recent_block_hash = hashBlock;
    most_recent_block = pblock;
    most_recent_compact_block = pcmpctblock;
    fWitnessesPresentInMostRecentCompactBlock = fWitnessEnabled;
  }

  connman->ForEachNode([this, &pcmpctblock, pindex, &msgMaker, fWitnessEnabled,
                        &hashBlock](CNode *pnode) {
    if (pnode->nVersion < INVALID_CB_NO_BAN_VERSION || pnode->fDisconnect)
      return;
    ProcessBlockAvailability(pnode->GetId());
    CNodeState &state = *State(pnode->GetId());

    if (state.fPreferHeaderAndIDs &&
        (!fWitnessEnabled || state.fWantsCmpctWitness) &&
        !PeerHasHeader(&state, pindex) &&
        PeerHasHeader(&state, pindex->pprev)) {
      LogPrint(BCLog::NET, "%s sending header-and-ids %s to peer=%d\n",
               "PeerLogicValidation::NewPoWValidBlock", hashBlock.ToString(),
               pnode->GetId());
      connman->PushMessage(pnode,
                           msgMaker.Make(NetMsgType::CMPCTBLOCK, *pcmpctblock));
      state.pindexBestHeaderSent = pindex;
    }
  });
}

void PeerLogicValidation::UpdatedBlockTip(const CBlockIndex *pindexNew,
                                          const CBlockIndex *pindexFork,
                                          bool fInitialDownload) {
  const int nNewHeight = pindexNew->nHeight;
  connman->SetBestHeight(nNewHeight);

  if (!fInitialDownload) {
    std::vector<uint256> vHashes;
    const CBlockIndex *pindexToAnnounce = pindexNew;
    while (pindexToAnnounce != pindexFork) {
      vHashes.push_back(pindexToAnnounce->GetBlockHash());
      pindexToAnnounce = pindexToAnnounce->pprev;
      if (vHashes.size() == MAX_BLOCKS_TO_ANNOUNCE) {
        break;
      }
    }

    connman->ForEachNode([nNewHeight, &vHashes](CNode *pnode) {
      if (nNewHeight >
          (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : 0)) {
        for (const uint256 &hash : reverse_iterate(vHashes)) {
          pnode->PushBlockHash(hash);
        }
      }
    });
    connman->WakeMessageHandler();
  }

  nTimeBestReceived = GetTime();
}

void PeerLogicValidation::BlockChecked(const CBlock &block,
                                       const CValidationState &state) {
  LOCK(cs_main);

  const uint256 hash(block.GetHash());
  std::map<uint256, std::pair<NodeId, bool>>::iterator it =
      mapBlockSource.find(hash);

  int nDoS = 0;
  if (state.IsInvalid(nDoS)) {
    if (it != mapBlockSource.end() && State(it->second.first) &&
        state.GetRejectCode() > 0 && state.GetRejectCode() < REJECT_INTERNAL) {
      CBlockReject reject = {
          (unsigned char)state.GetRejectCode(),
          state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), hash};
      State(it->second.first)->rejects.push_back(reject);
      if (nDoS > 0 && it->second.second)
        Misbehaving(it->second.first, nDoS);
    }
  }

  else if (state.IsValid() && !IsInitialBlockDownload() &&
           mapBlocksInFlight.count(hash) == mapBlocksInFlight.size()) {
    if (it != mapBlockSource.end()) {
      MaybeSetPeerAsAnnouncingHeaderAndIDs(it->second.first, connman);
    }
  }
  if (it != mapBlockSource.end())
    mapBlockSource.erase(it);
}

bool static AlreadyHave(const CInv &inv) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
  switch (inv.type) {
  case MSG_TX:
  case MSG_WITNESS_TX: {
    assert(recentRejects);
    if (chainActive.Tip()->GetBlockHash() != hashRecentRejectsChainTip) {
      hashRecentRejectsChainTip = chainActive.Tip()->GetBlockHash();
      recentRejects->reset();
    }

    {
      LOCK(g_cs_orphans);
      if (mapOrphanTransactions.count(inv.hash))
        return true;
    }

    return recentRejects->contains(inv.hash) || mempool.exists(inv.hash) ||
           pcoinsTip->HaveCoinInCache(COutPoint(inv.hash, 0)) ||

           pcoinsTip->HaveCoinInCache(COutPoint(inv.hash, 1));
  }
  case MSG_BLOCK:
  case MSG_WITNESS_BLOCK:
    return mapBlockIndex.count(inv.hash);

  case MSG_RIALTO:
    return (mapMessageRelay.count(inv.hash) > 0);
  }

  return true;
}

static void RelayTransaction(const CTransaction &tx, CConnman *connman) {
  CInv inv(MSG_TX, tx.GetHash());
  connman->ForEachNode([&inv](CNode *pnode) { pnode->PushInventory(inv); });
}

void RelayRialtoMessage(const CRialtoMessage message, CConnman *connman,
                        CNode *originNode) {
  if ((connman->GetLocalServices() & NODE_RIALTO) != NODE_RIALTO) {
    LogPrint(BCLog::RIALTO,
             "Not relaying Rialto message as we don't support relaying.\n");
    return;
  }

  uint256 hash = message.GetHash();
  CInv inv(MSG_RIALTO, hash);
  connman->ForEachNode([&inv, originNode](CNode *pnode) {
    if (pnode != originNode &&
        (pnode->nServices & NODE_RIALTO) == NODE_RIALTO) {
      LogPrint(BCLog::RIALTO, "Relaying Rialto message to peer=%d\n",
               pnode->GetId());
      pnode->PushInventory(inv);
    }
  });

  int64_t nNow = GetTime();

  LOCK(cs_main);
  auto ret = mapMessageRelay.insert(std::make_pair(hash, message.GetMessage()));
  if (ret.second)
    vMessageRelayExpiration.push_back(
        std::make_pair(nNow + RIALTO_MESSAGE_TTL, ret.first));
}

static void RelayAddress(const CAddress &addr, bool fReachable,
                         CConnman *connman) {
  unsigned int nRelayNodes = fReachable ? 2 : 1;

  uint64_t hashAddr = addr.GetHash();
  const CSipHasher hasher =
      connman->GetDeterministicRandomizer(RANDOMIZER_ID_ADDRESS_RELAY)
          .Write(hashAddr << 32)
          .Write((GetTime() + hashAddr) / (24 * 60 * 60));
  FastRandomContext insecure_rand;

  std::array<std::pair<uint64_t, CNode *>, 2> best{
      {{0, nullptr}, {0, nullptr}}};
  assert(nRelayNodes <= best.size());

  auto sortfunc = [&best, &hasher, nRelayNodes](CNode *pnode) {
    if (pnode->nVersion >= CADDR_TIME_VERSION) {
      uint64_t hashKey = CSipHasher(hasher).Write(pnode->GetId()).Finalize();
      for (unsigned int i = 0; i < nRelayNodes; i++) {
        if (hashKey > best[i].first) {
          std::copy(best.begin() + i, best.begin() + nRelayNodes - 1,
                    best.begin() + i + 1);
          best[i] = std::make_pair(hashKey, pnode);
          break;
        }
      }
    }
  };

  auto pushfunc = [&addr, &best, nRelayNodes, &insecure_rand] {
    for (unsigned int i = 0; i < nRelayNodes && best[i].first != 0; i++) {
      best[i].second->PushAddress(addr, insecure_rand);
    }
  };

  connman->ForEachNodeThen(std::move(sortfunc), std::move(pushfunc));
}

void static ProcessGetBlockData(CNode *pfrom,
                                const Consensus::Params &consensusParams,
                                const CInv &inv, CConnman *connman,
                                const std::atomic<bool> &interruptMsgProc) {
  bool send = false;
  std::shared_ptr<const CBlock> a_recent_block;
  std::shared_ptr<const CBlockHeaderAndShortTxIDs> a_recent_compact_block;
  bool fWitnessesPresentInARecentCompactBlock;
  {
    LOCK(cs_most_recent_block);
    a_recent_block = most_recent_block;
    a_recent_compact_block = most_recent_compact_block;
    fWitnessesPresentInARecentCompactBlock =
        fWitnessesPresentInMostRecentCompactBlock;
  }

  bool need_activate_chain = false;
  {
    LOCK(cs_main);
    BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
    if (mi != mapBlockIndex.end()) {
      if (mi->second->nChainTx && !mi->second->IsValid(BLOCK_VALID_SCRIPTS) &&
          mi->second->IsValid(BLOCK_VALID_TREE)) {
        need_activate_chain = true;
      }
    }
  }

  if (need_activate_chain) {
    CValidationState dummy;
    ActivateBestChain(dummy, Params(), a_recent_block);
  }

  LOCK(cs_main);
  BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
  if (mi != mapBlockIndex.end()) {
    send = BlockRequestAllowed(mi->second, consensusParams);
    if (!send) {
      LogPrint(BCLog::NET,
               "%s: ignoring request from peer=%i for old block that isn't in "
               "the main chain\n",
               __func__, pfrom->GetId());
    }
  }
  const CNetMsgMaker msgMaker(pfrom->GetSendVersion());

  if (send && connman->OutboundTargetReached(true) &&
      (((pindexBestHeader != nullptr) &&
        (pindexBestHeader->GetBlockTime() - mi->second->GetBlockTime() >
         HISTORICAL_BLOCK_AGE)) ||
       inv.type == MSG_FILTERED_BLOCK) &&
      !pfrom->fWhitelisted) {
    LogPrint(BCLog::NET,
             "historical block serving limit reached, disconnect peer=%d\n",
             pfrom->GetId());

    pfrom->fDisconnect = true;
    send = false;
  }

  if (send && !pfrom->fWhitelisted &&
      ((((pfrom->GetLocalServices() & NODE_NETWORK_LIMITED) ==
         NODE_NETWORK_LIMITED) &&
        ((pfrom->GetLocalServices() & NODE_NETWORK) != NODE_NETWORK) &&
        (chainActive.Tip()->nHeight - mi->second->nHeight >
         (int)NODE_NETWORK_LIMITED_MIN_BLOCKS + 2)))) {
    LogPrint(BCLog::NET,
             "Ignore block request below NODE_NETWORK_LIMITED threshold from "
             "peer=%d\n",
             pfrom->GetId());

    pfrom->fDisconnect = true;
    send = false;
  }

  if (send && (mi->second->nStatus & BLOCK_HAVE_DATA)) {
    std::shared_ptr<const CBlock> pblock;
    if (a_recent_block &&
        a_recent_block->GetHash() == (*mi).second->GetBlockHash()) {
      pblock = a_recent_block;
    } else {
      std::shared_ptr<CBlock> pblockRead = std::make_shared<CBlock>();
      if (!ReadBlockFromDisk(*pblockRead, (*mi).second, consensusParams))
        assert(!"cannot load block from disk");
      pblock = pblockRead;
    }
    if (inv.type == MSG_BLOCK)
      connman->PushMessage(pfrom,
                           msgMaker.Make(SERIALIZE_TRANSACTION_NO_WITNESS,
                                         NetMsgType::BLOCK, *pblock));
    else if (inv.type == MSG_WITNESS_BLOCK)
      connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::BLOCK, *pblock));
    else if (inv.type == MSG_FILTERED_BLOCK) {
      bool sendMerkleBlock = false;
      CMerkleBlock merkleBlock;
      {
        LOCK(pfrom->cs_filter);
        if (pfrom->pfilter) {
          sendMerkleBlock = true;
          merkleBlock = CMerkleBlock(*pblock, *pfrom->pfilter);
        }
      }
      if (sendMerkleBlock) {
        connman->PushMessage(
            pfrom, msgMaker.Make(NetMsgType::MERKLEBLOCK, merkleBlock));

        typedef std::pair<unsigned int, uint256> PairType;
        for (PairType &pair : merkleBlock.vMatchedTxn)
          connman->PushMessage(
              pfrom, msgMaker.Make(SERIALIZE_TRANSACTION_NO_WITNESS,
                                   NetMsgType::TX, *pblock->vtx[pair.first]));
      }

    } else if (inv.type == MSG_CMPCT_BLOCK) {
      bool fPeerWantsWitness = State(pfrom->GetId())->fWantsCmpctWitness;
      int nSendFlags = fPeerWantsWitness ? 0 : SERIALIZE_TRANSACTION_NO_WITNESS;
      if (CanDirectFetch(consensusParams) &&
          mi->second->nHeight >= chainActive.Height() - MAX_CMPCTBLOCK_DEPTH) {
        if ((fPeerWantsWitness || !fWitnessesPresentInARecentCompactBlock) &&
            a_recent_compact_block &&
            a_recent_compact_block->header.GetHash() ==
                mi->second->GetBlockHash()) {
          connman->PushMessage(pfrom,
                               msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK,
                                             *a_recent_compact_block));
        } else {
          CBlockHeaderAndShortTxIDs cmpctblock(*pblock, fPeerWantsWitness);
          connman->PushMessage(
              pfrom,
              msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK, cmpctblock));
        }
      } else {
        connman->PushMessage(
            pfrom, msgMaker.Make(nSendFlags, NetMsgType::BLOCK, *pblock));
      }
    }

    if (inv.hash == pfrom->hashContinue) {
      std::vector<CInv> vInv;
      vInv.push_back(CInv(MSG_BLOCK, chainActive.Tip()->GetBlockHash()));
      connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::INV, vInv));
      pfrom->hashContinue.SetNull();
    }
  }
}

void static ProcessGetData(CNode *pfrom,
                           const Consensus::Params &consensusParams,
                           CConnman *connman,
                           const std::atomic<bool> &interruptMsgProc) {
  AssertLockNotHeld(cs_main);

  std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();
  std::vector<CInv> vNotFound;
  const CNetMsgMaker msgMaker(pfrom->GetSendVersion());
  {
    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end() &&
           (it->type == MSG_TX || it->type == MSG_WITNESS_TX ||
            it->type == MSG_RIALTO)) {
      if (interruptMsgProc)
        return;

      if (pfrom->fPauseSend)
        break;

      const CInv &inv = *it;
      it++;

      if (inv.type == MSG_RIALTO) {
        auto mi = mapMessageRelay.find(inv.hash);
        if (mi != mapMessageRelay.end()) {
          connman->PushMessage(pfrom,
                               msgMaker.Make(NetMsgType::RIALTO, mi->second));
        } else {
          vNotFound.push_back(inv);
        }
      } else {
        bool push = false;
        auto mi = mapRelay.find(inv.hash);
        int nSendFlags =
            (inv.type == MSG_TX ? SERIALIZE_TRANSACTION_NO_WITNESS : 0);
        if (mi != mapRelay.end()) {
          connman->PushMessage(
              pfrom, msgMaker.Make(nSendFlags, NetMsgType::TX, *mi->second));
          push = true;
        } else if (pfrom->timeLastMempoolReq) {
          auto txinfo = mempool.info(inv.hash);

          if (txinfo.tx && txinfo.nTime <= pfrom->timeLastMempoolReq) {
            connman->PushMessage(
                pfrom, msgMaker.Make(nSendFlags, NetMsgType::TX, *txinfo.tx));
            push = true;
          }
        }
        if (!push) {
          vNotFound.push_back(inv);
        }
      }

      GetMainSignals().Inventory(inv.hash);
    }
  }

  if (it != pfrom->vRecvGetData.end() && !pfrom->fPauseSend) {
    const CInv &inv = *it;
    if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK ||
        inv.type == MSG_CMPCT_BLOCK || inv.type == MSG_WITNESS_BLOCK) {
      it++;
      ProcessGetBlockData(pfrom, consensusParams, inv, connman,
                          interruptMsgProc);
    }
  }

  pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

  if (!vNotFound.empty()) {
    connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::NOTFOUND, vNotFound));
  }
}

uint32_t GetFetchFlags(CNode *pfrom) {
  uint32_t nFetchFlags = 0;
  if ((pfrom->GetLocalServices() & NODE_WITNESS) &&
      State(pfrom->GetId())->fHaveWitness) {
    nFetchFlags |= MSG_WITNESS_FLAG;
  }
  return nFetchFlags;
}

inline void static SendBlockTransactions(const CBlock &block,
                                         const BlockTransactionsRequest &req,
                                         CNode *pfrom, CConnman *connman) {
  BlockTransactions resp(req);
  for (size_t i = 0; i < req.indexes.size(); i++) {
    if (req.indexes[i] >= block.vtx.size()) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 100);
      LogPrintf("Peer %d sent us a getblocktxn with out-of-bounds tx indices",
                pfrom->GetId());
      return;
    }
    resp.txn[i] = block.vtx[req.indexes[i]];
  }
  LOCK(cs_main);
  const CNetMsgMaker msgMaker(pfrom->GetSendVersion());
  int nSendFlags = State(pfrom->GetId())->fWantsCmpctWitness
                       ? 0
                       : SERIALIZE_TRANSACTION_NO_WITNESS;
  connman->PushMessage(pfrom,
                       msgMaker.Make(nSendFlags, NetMsgType::BLOCKTXN, resp));
}

bool static ProcessHeadersMessage(CNode *pfrom, CConnman *connman,
                                  const std::vector<CBlockHeader> &headers,
                                  const CChainParams &chainparams,
                                  bool punish_duplicate_invalid) {
  const CNetMsgMaker msgMaker(pfrom->GetSendVersion());
  size_t nCount = headers.size();

  if (nCount == 0) {
    return true;
  }

  bool received_new_header = false;
  const CBlockIndex *pindexLast = nullptr;
  {
    LOCK(cs_main);
    CNodeState *nodestate = State(pfrom->GetId());

    if (mapBlockIndex.find(headers[0].hashPrevBlock) == mapBlockIndex.end() &&
        nCount < MAX_BLOCKS_TO_ANNOUNCE) {
      nodestate->nUnconnectingHeaders++;
      connman->PushMessage(
          pfrom,
          msgMaker.Make(NetMsgType::GETHEADERS,
                        chainActive.GetLocator(pindexBestHeader), uint256()));
      LogPrint(BCLog::NET,
               "received header %s: missing prev block %s, sending getheaders "
               "(%d) to end (peer=%d, nUnconnectingHeaders=%d)\n",
               headers[0].GetHash().ToString(),
               headers[0].hashPrevBlock.ToString(), pindexBestHeader->nHeight,
               pfrom->GetId(), nodestate->nUnconnectingHeaders);

      UpdateBlockAvailability(pfrom->GetId(), headers.back().GetHash());

      if (nodestate->nUnconnectingHeaders % MAX_UNCONNECTING_HEADERS == 0) {
        Misbehaving(pfrom->GetId(), 20);
      }
      return true;
    }

    uint256 hashLastBlock;
    for (const CBlockHeader &header : headers) {
      if (!hashLastBlock.IsNull() && header.hashPrevBlock != hashLastBlock) {
        Misbehaving(pfrom->GetId(), 20);
        return error("non-continuous headers sequence");
      }
      hashLastBlock = header.GetHash();
    }

    if (mapBlockIndex.find(hashLastBlock) == mapBlockIndex.end()) {
      received_new_header = true;
    }
  }

  CValidationState state;
  CBlockHeader first_invalid_header;
  if (!ProcessNewBlockHeaders(headers, state, chainparams, &pindexLast,
                              &first_invalid_header)) {
    int nDoS;
    if (state.IsInvalid(nDoS)) {
      LOCK(cs_main);
      if (nDoS > 0) {
        Misbehaving(pfrom->GetId(), nDoS);
      }
      if (punish_duplicate_invalid &&
          mapBlockIndex.find(first_invalid_header.GetHash()) !=
              mapBlockIndex.end()) {
        pfrom->fDisconnect = true;
      }
      return error("invalid header received");
    }
  }

  {
    LOCK(cs_main);
    CNodeState *nodestate = State(pfrom->GetId());
    if (nodestate->nUnconnectingHeaders > 0) {
      LogPrint(BCLog::NET,
               "peer=%d: resetting nUnconnectingHeaders (%d -> 0)\n",
               pfrom->GetId(), nodestate->nUnconnectingHeaders);
    }
    nodestate->nUnconnectingHeaders = 0;

    assert(pindexLast);
    UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockHash());

    if (received_new_header &&
        pindexLast->nChainWork > chainActive.Tip()->nChainWork) {
      nodestate->m_last_block_announcement = GetTime();
    }

    // Introspection hardening: Detect stale fork announcements
    // Peers repeatedly advertising old forks may be mapping the chain
    if (gArgs.GetBoolArg("-introspectionhardening",
                         DEFAULT_ENABLE_INTROSPECTION_HARDENING)) {
      if (received_new_header && pindexLast) {
        // Check if this is a stale fork (significantly behind our tip)
        if (pindexLast->nChainWork < chainActive.Tip()->nChainWork) {
          int nHeightDiff = chainActive.Height() - pindexLast->nHeight;

          // Consider it stale if it's >6 blocks behind
          if (nHeightDiff > 6) {
            nodestate->nStaleForkAnnouncements++;
            nodestate->nLastStaleForkTime = GetTime();

            // Increase introspection score for stale fork announcements
            nodestate->nIntrospectionScore += 5;

            LogPrint(BCLog::NET,
                     "Peer %d announced stale fork: height %d vs our %d "
                     "(stale count: %d, introspection score: %d)\n",
                     pfrom->GetId(), pindexLast->nHeight, chainActive.Height(),
                     nodestate->nStaleForkAnnouncements,
                     nodestate->nIntrospectionScore);

            // Disconnect if too many stale forks announced
            if (nodestate->nStaleForkAnnouncements > 3) {
              LogPrintf(
                  "WARNING: Disconnecting peer %d for repeated stale fork "
                  "announcements (%d times) - possible chain mapping\n",
                  pfrom->GetId(), nodestate->nStaleForkAnnouncements);
              pfrom->fDisconnect = true;
            }
          }
        }
      }
    }

    if (nCount == MAX_HEADERS_RESULTS) {
      LogPrint(BCLog::NET,
               "more getheaders (%d) to end to peer=%d (startheight:%d)\n",
               pindexLast->nHeight, pfrom->GetId(), pfrom->nStartingHeight);
      connman->PushMessage(
          pfrom, msgMaker.Make(NetMsgType::GETHEADERS,
                               chainActive.GetLocator(pindexLast), uint256()));
    }

    bool fCanDirectFetch = CanDirectFetch(chainparams.GetConsensus());

    if (fCanDirectFetch && pindexLast->IsValid(BLOCK_VALID_TREE) &&
        chainActive.Tip()->nChainWork <= pindexLast->nChainWork) {
      std::vector<const CBlockIndex *> vToFetch;
      const CBlockIndex *pindexWalk = pindexLast;

      while (pindexWalk && !chainActive.Contains(pindexWalk) &&
             vToFetch.size() <= MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
        if (!(pindexWalk->nStatus & BLOCK_HAVE_DATA) &&
            !mapBlocksInFlight.count(pindexWalk->GetBlockHash()) &&
            (!IsWitnessEnabled(pindexWalk->pprev, chainparams.GetConsensus()) ||
             State(pfrom->GetId())->fHaveWitness)) {
          vToFetch.push_back(pindexWalk);
        }
        pindexWalk = pindexWalk->pprev;
      }

      if (!chainActive.Contains(pindexWalk)) {
        LogPrint(BCLog::NET, "Large reorg, won't direct fetch to %s (%d)\n",
                 pindexLast->GetBlockHash().ToString(), pindexLast->nHeight);
      } else {
        std::vector<CInv> vGetData;

        for (const CBlockIndex *pindex : reverse_iterate(vToFetch)) {
          if (nodestate->nBlocksInFlight >= MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            break;
          }
          uint32_t nFetchFlags = GetFetchFlags(pfrom);
          vGetData.push_back(
              CInv(MSG_BLOCK | nFetchFlags, pindex->GetBlockHash()));
          MarkBlockAsInFlight(pfrom->GetId(), pindex->GetBlockHash(), pindex);
          LogPrint(BCLog::NET, "Requesting block %s from  peer=%d\n",
                   pindex->GetBlockHash().ToString(), pfrom->GetId());
        }
        if (vGetData.size() > 1) {
          LogPrint(
              BCLog::NET,
              "Downloading blocks toward %s (%d) via headers direct fetch\n",
              pindexLast->GetBlockHash().ToString(), pindexLast->nHeight);
        }
        if (vGetData.size() > 0) {
          if (nodestate->fSupportsDesiredCmpctVersion && vGetData.size() == 1 &&
              mapBlocksInFlight.size() == 1 &&
              pindexLast->pprev->IsValid(BLOCK_VALID_CHAIN)) {
            vGetData[0] = CInv(MSG_CMPCT_BLOCK, vGetData[0].hash);
          }
          connman->PushMessage(pfrom,
                               msgMaker.Make(NetMsgType::GETDATA, vGetData));
        }
      }
    }

    // Introspection hardening: Check chainwork even after IBD
    // Disconnect peers with significantly weaker chains to prevent attack
    // attempts
    if (nCount != MAX_HEADERS_RESULTS) {
      if (nodestate->pindexBestKnownBlock &&
          nodestate->pindexBestKnownBlock->nChainWork < nMinimumChainWork) {
        if (IsOutboundDisconnectionCandidate(pfrom)) {
          LogPrintf("Disconnecting outbound peer %d -- headers chain has "
                    "insufficient work\n",
                    pfrom->GetId());
          pfrom->fDisconnect = true;
        }
      }

      // Additional check: Disconnect if peer's best chain is far behind ours
      // This protects against peers advertising weak forks post-IBD
      if (gArgs.GetBoolArg("-introspectionhardening",
                           DEFAULT_ENABLE_INTROSPECTION_HARDENING)) {
        if (nodestate->pindexBestKnownBlock && chainActive.Tip()) {
          // Peer's chain must be within reasonable distance of our chainwork
          // Allow for up to 144 blocks worth of work difference (~1 day)
          arith_uint256 nOurChainWork = chainActive.Tip()->nChainWork;
          arith_uint256 nPeerChainWork =
              nodestate->pindexBestKnownBlock->nChainWork;

          // Calculate acceptable minimum (our work minus ~144 blocks of average
          // work)
          arith_uint256 nWorkPerBlock =
              nOurChainWork / std::max(chainActive.Height(), 1);
          arith_uint256 nMinAcceptableWork =
              nOurChainWork - (nWorkPerBlock * 144);

          if (nPeerChainWork < nMinAcceptableWork &&
              IsOutboundDisconnectionCandidate(pfrom) &&
              !IsInitialBlockDownload()) {
            LogPrintf(
                "WARNING: Disconnecting outbound peer %d -- chain work "
                "significantly behind ours (peer: %s, ours: %s, min: %s)\n",
                pfrom->GetId(), nPeerChainWork.ToString(),
                nOurChainWork.ToString(), nMinAcceptableWork.ToString());
            pfrom->fDisconnect = true;
          }
        }
      }
    }

    if (!pfrom->fDisconnect && IsOutboundDisconnectionCandidate(pfrom) &&
        nodestate->pindexBestKnownBlock != nullptr) {
      if (g_outbound_peers_with_protect_from_disconnect <
              MAX_OUTBOUND_PEERS_TO_PROTECT_FROM_DISCONNECT &&
          nodestate->pindexBestKnownBlock->nChainWork >=
              chainActive.Tip()->nChainWork &&
          !nodestate->m_chain_sync.m_protect) {
        LogPrint(BCLog::NET, "Protecting outbound peer=%d from eviction\n",
                 pfrom->GetId());
        nodestate->m_chain_sync.m_protect = true;
        ++g_outbound_peers_with_protect_from_disconnect;
      }
    }
  }

  return true;
}

bool static ProcessMessage(CNode *pfrom, const std::string &strCommand,
                           CDataStream &vRecv, int64_t nTimeReceived,
                           const CChainParams &chainparams, CConnman *connman,
                           const std::atomic<bool> &interruptMsgProc) {
  LogPrint(BCLog::NET, "received: %s (%u bytes) peer=%d\n",
           SanitizeString(strCommand), vRecv.size(), pfrom->GetId());
  if (gArgs.IsArgSet("-dropmessagestest") &&
      GetRand(gArgs.GetArg("-dropmessagestest", 0)) == 0) {
    LogPrintf("dropmessagestest DROPPING RECV MESSAGE\n");
    return true;
  }

  if (!(pfrom->GetLocalServices() & NODE_BLOOM) &&
      (strCommand == NetMsgType::FILTERLOAD ||
       strCommand == NetMsgType::FILTERADD)) {
    if (pfrom->nVersion >= NO_BLOOM_VERSION) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 100);
      return false;
    } else {
      pfrom->fDisconnect = true;
      return false;
    }
  }

  if (strCommand == NetMsgType::REJECT) {
    // Phase 2 Hardening: Rate limit REJECT message logging
    LOCK(cs_main);
    CNodeState *state = State(pfrom->GetId());
    if (state) {
      int64_t nNow = GetTime();
      if (nNow - state->nLastRejectTime > 60) {
        state->nLastRejectTime = nNow;
        state->nRejectCount = 0;
      }
      state->nRejectCount++;
      if (state->nRejectCount > 10) {
        LogPrint(BCLog::NET, "Suppressing REJECT logs from peer=%d (flood)\n",
                 pfrom->GetId());
        return true; // Silently ignore further REJECT spam
      }
    }

    if (LogAcceptCategory(BCLog::NET)) {
      try {
        std::string strMsg;
        unsigned char ccode;
        std::string strReason;
        vRecv >> LIMITED_STRING(strMsg, CMessageHeader::COMMAND_SIZE) >>
            ccode >> LIMITED_STRING(strReason, MAX_REJECT_MESSAGE_LENGTH);

        std::ostringstream ss;
        ss << strMsg << " code " << itostr(ccode) << ": " << strReason;

        if (strMsg == NetMsgType::BLOCK || strMsg == NetMsgType::TX) {
          uint256 hash;
          vRecv >> hash;
          ss << ": hash " << hash.ToString();
        }
        LogPrint(BCLog::NET, "Reject %s\n", SanitizeString(ss.str()));
      } catch (const std::ios_base::failure &) {
        LogPrint(BCLog::NET, "Unparseable reject message received\n");
      }
    }
  }

  else if (strCommand == NetMsgType::VERSION) {
    if (pfrom->nVersion != 0) {
      connman->PushMessage(
          pfrom, CNetMsgMaker(INIT_PROTO_VERSION)
                     .Make(NetMsgType::REJECT, strCommand, REJECT_DUPLICATE,
                           std::string("Duplicate version message")));
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 1);
      return false;
    }

    bool fRelay = true;
    CAddress addrFrom;
    CAddress addrMe;
    int nSendVersion;
    int nStartingHeight = -1;
    int nVersion;
    int64_t nTime;
    ServiceFlags nServices;
    std::string cleanSubVer;
    std::string strSubVer;
    uint64_t nNonce = 1;
    uint64_t nServiceInt;

    vRecv >> nVersion >> nServiceInt >> nTime >> addrMe;
    nSendVersion = std::min(nVersion, PROTOCOL_VERSION);
    nServices = ServiceFlags(nServiceInt);
    if (!pfrom->fInbound) {
      connman->SetServices(pfrom->addr, nServices);
    }
    if (!pfrom->fInbound && !pfrom->fFeeler && !pfrom->m_manual_connection &&
        !HasAllDesirableServiceFlags(nServices)) {
      LogPrint(BCLog::NET,
               "peer=%d does not offer the expected services (%08x offered, "
               "%08x expected); disconnecting\n",
               pfrom->GetId(), nServices, GetDesirableServiceFlags(nServices));
      connman->PushMessage(
          pfrom, CNetMsgMaker(INIT_PROTO_VERSION)
                     .Make(NetMsgType::REJECT, strCommand, REJECT_NONSTANDARD,
                           strprintf("Expected to offer services %08x",
                                     GetDesirableServiceFlags(nServices))));
      pfrom->fDisconnect = true;
      return false;
    }

    if (nServices & ((1 << 7) | (1 << 5))) {
      if (GetTime() < 1533096000) {
        pfrom->fDisconnect = true;
        return false;
      }
    }

    if (nVersion < MIN_PEER_PROTO_VERSION) {
      LogPrint(BCLog::NET, "peer=%d using obsolete version %i; disconnecting\n",
               pfrom->GetId(), nVersion);
      connman->PushMessage(
          pfrom, CNetMsgMaker(INIT_PROTO_VERSION)
                     .Make(NetMsgType::REJECT, strCommand, REJECT_OBSOLETE,
                           strprintf("Version must be %d or greater",
                                     MIN_PEER_PROTO_VERSION)));
      pfrom->fDisconnect = true;
      return false;
    }

    if (nVersion == 10300)
      nVersion = 300;
    if (!vRecv.empty())
      vRecv >> addrFrom >> nNonce;
    if (!vRecv.empty()) {
      vRecv >> LIMITED_STRING(strSubVer, MAX_SUBVERSION_LENGTH);
      cleanSubVer = SanitizeString(strSubVer);
    }
    if (!vRecv.empty()) {
      vRecv >> nStartingHeight;
    }
    if (!vRecv.empty())
      vRecv >> fRelay;

    if (pfrom->fInbound && !connman->CheckIncomingNonce(nNonce)) {
      LogPrintf("connected to self at %s, disconnecting\n",
                pfrom->addr.ToString());
      pfrom->fDisconnect = true;
      return true;
    }

    if (pfrom->fInbound && addrMe.IsRoutable()) {
      SeenLocal(addrMe);
    }

    if (pfrom->fInbound)
      PushNodeVersion(pfrom, connman, GetAdjustedTime());

    connman->PushMessage(
        pfrom, CNetMsgMaker(INIT_PROTO_VERSION).Make(NetMsgType::VERACK));

    pfrom->nServices = nServices;
    pfrom->SetAddrLocal(addrMe);
    {
      LOCK(pfrom->cs_SubVer);
      pfrom->strSubVer = strSubVer;
      pfrom->cleanSubVer = cleanSubVer;
    }
    pfrom->nStartingHeight = nStartingHeight;
    pfrom->fClient = !(nServices & NODE_NETWORK);
    {
      LOCK(pfrom->cs_filter);
      pfrom->fRelayTxes = fRelay;
    }

    pfrom->SetSendVersion(nSendVersion);
    pfrom->nVersion = nVersion;

    if ((nServices & NODE_WITNESS)) {
      LOCK(cs_main);
      State(pfrom->GetId())->fHaveWitness = true;
    }

    {
      LOCK(cs_main);
      UpdatePreferredDownload(pfrom, State(pfrom->GetId()));
    }

    if (!pfrom->fInbound) {
      if (fListen && !IsInitialBlockDownload()) {
        CAddress addr =
            GetLocalAddress(&pfrom->addr, pfrom->GetLocalServices());
        FastRandomContext insecure_rand;
        if (addr.IsRoutable()) {
          LogPrint(BCLog::NET, "ProcessMessages: advertising address %s\n",
                   addr.ToString());
          pfrom->PushAddress(addr, insecure_rand);
        } else if (IsPeerAddrLocalGood(pfrom)) {
          addr.SetIP(addrMe);
          LogPrint(BCLog::NET, "ProcessMessages: advertising address %s\n",
                   addr.ToString());
          pfrom->PushAddress(addr, insecure_rand);
        }
      }

      if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION ||
          connman->GetAddressCount() < 1000) {
        connman->PushMessage(
            pfrom, CNetMsgMaker(nSendVersion).Make(NetMsgType::GETADDR));
        pfrom->fGetAddr = true;
      }
      connman->MarkAddressGood(pfrom->addr);
    }

    std::string remoteAddr;
    if (fLogIPs)
      remoteAddr = ", peeraddr=" + pfrom->addr.ToString();

    LogPrint(BCLog::NET,
             "receive version message: %s: version %d, blocks=%d, us=%s, "
             "peer=%d%s\n",
             cleanSubVer, pfrom->nVersion, pfrom->nStartingHeight,
             addrMe.ToString(), pfrom->GetId(), remoteAddr);

    int64_t nTimeOffset = nTime - GetTime();
    pfrom->nTimeOffset = nTimeOffset;
    AddTimeData(pfrom->addr, nTimeOffset);

    if (pfrom->nVersion <= 70012) {
      CDataStream finalAlert(
          ParseHex("5c0100000015f7675900000000ffffff7f00000000ffffff7ffeffff7f0"
                   "000000000ffffff7f00ffffff7f002f555247454e543a20416c65727420"
                   "6b657920636f6d70726f6d697365642c207570677261646520726571756"
                   "9726564004630440220405f7e7572b176f3316d4e12deab75ad4ff97884"
                   "4f7a7bcd5ed06f6aa094eb6602207880fcc07d0a78e0f46f188d115e04e"
                   "d4ad48980ea3572cb0e0cb97921048095"),
          SER_NETWORK, PROTOCOL_VERSION);
      connman->PushMessage(
          pfrom, CNetMsgMaker(nSendVersion).Make("alert", finalAlert));
    }

    if (pfrom->fFeeler) {
      assert(pfrom->fInbound == false);
      pfrom->fDisconnect = true;
    }
    return true;
  }

  else if (pfrom->nVersion == 0) {
    LOCK(cs_main);
    Misbehaving(pfrom->GetId(), 1);
    return false;
  }

  const CNetMsgMaker msgMaker(pfrom->GetSendVersion());

  if (strCommand == NetMsgType::VERACK) {
    pfrom->SetRecvVersion(std::min(pfrom->nVersion.load(), PROTOCOL_VERSION));

    if (!pfrom->fInbound) {
      LOCK(cs_main);
      State(pfrom->GetId())->fCurrentlyConnected = true;
      LogPrintf(
          "New outbound peer connected: version: %d, blocks=%d, peer=%d%s\n",
          pfrom->nVersion.load(), pfrom->nStartingHeight, pfrom->GetId(),
          (fLogIPs ? strprintf(", peeraddr=%s", pfrom->addr.ToString()) : ""));
    }

    if (pfrom->nVersion >= SENDHEADERS_VERSION) {
      connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SENDHEADERS));
    }
    if (pfrom->nVersion >= SHORT_IDS_BLOCKS_VERSION) {
      bool fAnnounceUsingCMPCTBLOCK = false;
      uint64_t nCMPCTBLOCKVersion = 2;
      if (pfrom->GetLocalServices() & NODE_WITNESS)
        connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SENDCMPCT,
                                                  fAnnounceUsingCMPCTBLOCK,
                                                  nCMPCTBLOCKVersion));
      nCMPCTBLOCKVersion = 1;
      connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::SENDCMPCT,
                                                fAnnounceUsingCMPCTBLOCK,
                                                nCMPCTBLOCKVersion));
    }
    pfrom->fSuccessfullyConnected = true;
  }

  else if (!pfrom->fSuccessfullyConnected) {
    LOCK(cs_main);
    Misbehaving(pfrom->GetId(), 1);
    return false;
  }

  else if (strCommand == NetMsgType::ADDR) {
    std::vector<CAddress> vAddr;
    vRecv >> vAddr;

    // Phase 2 Hardening: Rate limit ADDR messages
    {
      LOCK(cs_main);
      CNodeState *state = State(pfrom->GetId());
      if (state) {
        int64_t nNow = GetTime();
        if (nNow - state->nLastAddrTime > 60) {
          state->nLastAddrTime = nNow;
          state->nAddrCount = 0;
        }
        state->nAddrCount += vAddr.size();
        if (state->nAddrCount > 1000) {
          Misbehaving(pfrom->GetId(), 20);
          LogPrint(BCLog::NET, "Peer %d ADDR flood: %d addrs in window\n",
                   pfrom->GetId(), state->nAddrCount);
          return error("addr flood from peer=%d", pfrom->GetId());
        }
      }
    }

    if (pfrom->nVersion < CADDR_TIME_VERSION &&
        connman->GetAddressCount() > 1000)
      return true;
    if (vAddr.size() > 1000) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 20);
      return error("message addr size() = %u", vAddr.size());
    }

    std::vector<CAddress> vAddrOk;
    int64_t nNow = GetAdjustedTime();
    int64_t nSince = nNow - 10 * 60;
    for (CAddress &addr : vAddr) {
      if (interruptMsgProc)
        return true;

      if (!MayHaveUsefulAddressDB(addr.nServices))
        continue;

      if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
        addr.nTime = nNow - 5 * 24 * 60 * 60;
      pfrom->AddAddressKnown(addr);
      bool fReachable = IsReachable(addr);
      if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 &&
          addr.IsRoutable()) {
        RelayAddress(addr, fReachable, connman);
      }

      if (fReachable)
        vAddrOk.push_back(addr);
    }
    connman->AddNewAddresses(vAddrOk, pfrom->addr, 2 * 60 * 60);
    if (vAddr.size() < 1000)
      pfrom->fGetAddr = false;
    if (pfrom->fOneShot)
      pfrom->fDisconnect = true;
  }

  else if (strCommand == NetMsgType::SENDHEADERS) {
    LOCK(cs_main);
    State(pfrom->GetId())->fPreferHeaders = true;
  }

  else if (strCommand == NetMsgType::SENDCMPCT) {
    bool fAnnounceUsingCMPCTBLOCK = false;
    uint64_t nCMPCTBLOCKVersion = 0;
    vRecv >> fAnnounceUsingCMPCTBLOCK >> nCMPCTBLOCKVersion;

    // Phase 2 Hardening: Limit SENDCMPCT to 5 per session
    {
      LOCK(cs_main);
      CNodeState *state = State(pfrom->GetId());
      if (state) {
        state->nSendCmpctCount++;
        if (state->nSendCmpctCount > 5) {
          Misbehaving(pfrom->GetId(), 10);
          LogPrint(BCLog::NET, "Peer %d SENDCMPCT spam: %d messages\n",
                   pfrom->GetId(), state->nSendCmpctCount);
          return true;
        }
      }
    }

    if (nCMPCTBLOCKVersion == 1 ||
        ((pfrom->GetLocalServices() & NODE_WITNESS) &&
         nCMPCTBLOCKVersion == 2)) {
      LOCK(cs_main);

      if (!State(pfrom->GetId())->fProvidesHeaderAndIDs) {
        State(pfrom->GetId())->fProvidesHeaderAndIDs = true;
        State(pfrom->GetId())->fWantsCmpctWitness = nCMPCTBLOCKVersion == 2;
      }
      if (State(pfrom->GetId())->fWantsCmpctWitness ==
          (nCMPCTBLOCKVersion == 2))

        State(pfrom->GetId())->fPreferHeaderAndIDs = fAnnounceUsingCMPCTBLOCK;
      if (!State(pfrom->GetId())->fSupportsDesiredCmpctVersion) {
        if (pfrom->GetLocalServices() & NODE_WITNESS)
          State(pfrom->GetId())->fSupportsDesiredCmpctVersion =
              (nCMPCTBLOCKVersion == 2);
        else
          State(pfrom->GetId())->fSupportsDesiredCmpctVersion =
              (nCMPCTBLOCKVersion == 1);
      }
    }
  }

  else if (strCommand == NetMsgType::INV) {
    std::vector<CInv> vInv;
    vRecv >> vInv;

    // Hardening: Rate limit INVs
    CNodeState *state = State(pfrom->GetId());
    if (state) {
      int64_t nNow = GetTime();
      if (state->nLastInvTime < nNow) {
        state->nLastInvTime = nNow;
        state->nInvCount = 0;
      }
      state->nInvCount += vInv.size();
      if (state->nInvCount > 1000) {
        Misbehaving(pfrom->GetId(), 20);
        return error("peer sent too many invs");
      }
    }

    if (vInv.size() > MAX_INV_SZ) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 20);
      return error("message inv size() = %u", vInv.size());
    }

    bool fBlocksOnly = !fRelayTxes;

    if (pfrom->fWhitelisted &&
        gArgs.GetBoolArg("-whitelistrelay", DEFAULT_WHITELISTRELAY))
      fBlocksOnly = false;

    LOCK(cs_main);

    uint32_t nFetchFlags = GetFetchFlags(pfrom);

    for (CInv &inv : vInv) {
      if (interruptMsgProc)
        return true;

      bool fAlreadyHave = AlreadyHave(inv);
      LogPrint(BCLog::NET, "got inv: %s  %s peer=%d\n", inv.ToString(),
               fAlreadyHave ? "have" : "new", pfrom->GetId());

      if (inv.type == MSG_TX) {
        inv.type |= nFetchFlags;
      }

      if (inv.type == MSG_BLOCK) {
        UpdateBlockAvailability(pfrom->GetId(), inv.hash);
        if (!fAlreadyHave && !fImporting && !fReindex &&
            !mapBlocksInFlight.count(inv.hash)) {
          connman->PushMessage(
              pfrom, msgMaker.Make(NetMsgType::GETHEADERS,
                                   chainActive.GetLocator(pindexBestHeader),
                                   inv.hash));
          LogPrint(BCLog::NET, "getheaders (%d) %s to peer=%d\n",
                   pindexBestHeader->nHeight, inv.hash.ToString(),
                   pfrom->GetId());
        }
      } else {
        pfrom->AddInventoryKnown(inv);

        if ((connman->GetLocalServices() & NODE_RIALTO) != NODE_RIALTO) {
          LogPrint(
              BCLog::NET,
              "rialto message (%s) inv sent, but we're not relaying. peer=%d\n",
              inv.hash.ToString(), pfrom->GetId());
          Misbehaving(pfrom->GetId(), 20);
        } else if (inv.type == MSG_RIALTO &&
                   (pfrom->nServices & NODE_RIALTO) != NODE_RIALTO) {
          LogPrint(BCLog::NET,
                   "rialto message (%s) inv sent, but they're not relaying. "
                   "peer=%d\n",
                   inv.hash.ToString(), pfrom->GetId());
          Misbehaving(pfrom->GetId(), 20);
        } else if (fBlocksOnly) {
          LogPrint(
              BCLog::NET,
              "transaction (%s) inv sent in violation of protocol peer=%d\n",
              inv.hash.ToString(), pfrom->GetId());
        } else if (!fAlreadyHave && !fImporting && !fReindex &&
                   !IsInitialBlockDownload()) {
          pfrom->AskFor(inv);
        }
      }

      GetMainSignals().Inventory(inv.hash);
    }
  }

  else if (strCommand == NetMsgType::GETDATA) {
    std::vector<CInv> vInv;
    vRecv >> vInv;

    // Hardening: Check for duplicates
    std::set<CInv> setInv;
    for (const CInv &inv : vInv) {
      if (setInv.count(inv)) {
        Misbehaving(pfrom->GetId(), 20);
        return error("duplicate getdata");
      }
      setInv.insert(inv);
    }

    if (vInv.size() > MAX_INV_SZ) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 20);
      return error("message getdata size() = %u", vInv.size());
    }

    LogPrint(BCLog::NET, "received getdata (%u invsz) peer=%d\n", vInv.size(),
             pfrom->GetId());

    if (vInv.size() > 0) {
      LogPrint(BCLog::NET, "received getdata for: %s peer=%d\n",
               vInv[0].ToString(), pfrom->GetId());
    }

    pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(),
                               vInv.end());
    ProcessGetData(pfrom, chainparams.GetConsensus(), connman,
                   interruptMsgProc);
  }

  else if (strCommand == NetMsgType::GETBLOCKS) {
    CBlockLocator locator;
    uint256 hashStop;
    vRecv >> locator >> hashStop;

    {
      std::shared_ptr<const CBlock> a_recent_block;
      {
        LOCK(cs_most_recent_block);
        a_recent_block = most_recent_block;
      }
      CValidationState dummy;
      ActivateBestChain(dummy, Params(), a_recent_block);
    }

    LOCK(cs_main);

    const CBlockIndex *pindex = FindForkInGlobalIndex(chainActive, locator);

    if (pindex)
      pindex = chainActive.Next(pindex);
    int nLimit = 500;
    LogPrint(BCLog::NET, "getblocks %d to %s limit %d from peer=%d\n",
             (pindex ? pindex->nHeight : -1),
             hashStop.IsNull() ? "end" : hashStop.ToString(), nLimit,
             pfrom->GetId());
    for (; pindex; pindex = chainActive.Next(pindex)) {
      if (pindex->GetBlockHash() == hashStop) {
        LogPrint(BCLog::NET, "  getblocks stopping at %d %s\n", pindex->nHeight,
                 pindex->GetBlockHash().ToString());
        break;
      }

      const int nPrunedBlocksLikelyToHave =
          MIN_BLOCKS_TO_KEEP -
          3600 / chainparams.GetConsensus().nPowTargetSpacing;
      if (fPruneMode && (!(pindex->nStatus & BLOCK_HAVE_DATA) ||
                         pindex->nHeight <= chainActive.Tip()->nHeight -
                                                nPrunedBlocksLikelyToHave)) {
        LogPrint(BCLog::NET,
                 " getblocks stopping, pruned or too old block at %d %s\n",
                 pindex->nHeight, pindex->GetBlockHash().ToString());
        break;
      }
      pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
      if (--nLimit <= 0) {
        LogPrint(BCLog::NET, "  getblocks stopping at limit %d %s\n",
                 pindex->nHeight, pindex->GetBlockHash().ToString());
        pfrom->hashContinue = pindex->GetBlockHash();
        break;
      }
    }
  }

  else if (strCommand == NetMsgType::GETBLOCKTXN) {
    BlockTransactionsRequest req;
    vRecv >> req;

    std::shared_ptr<const CBlock> recent_block;
    {
      LOCK(cs_most_recent_block);
      if (most_recent_block_hash == req.blockhash)
        recent_block = most_recent_block;
    }
    if (recent_block) {
      SendBlockTransactions(*recent_block, req, pfrom, connman);
      return true;
    }

    LOCK(cs_main);

    BlockMap::iterator it = mapBlockIndex.find(req.blockhash);
    if (it == mapBlockIndex.end() || !(it->second->nStatus & BLOCK_HAVE_DATA)) {
      LogPrint(BCLog::NET,
               "Peer %d sent us a getblocktxn for a block we don't have",
               pfrom->GetId());
      return true;
    }

    if (it->second->nHeight < chainActive.Height() - MAX_BLOCKTXN_DEPTH) {
      LogPrint(BCLog::NET,
               "Peer %d sent us a getblocktxn for a block > %i deep",
               pfrom->GetId(), MAX_BLOCKTXN_DEPTH);
      CInv inv;
      inv.type = State(pfrom->GetId())->fWantsCmpctWitness ? MSG_WITNESS_BLOCK
                                                           : MSG_BLOCK;
      inv.hash = req.blockhash;
      pfrom->vRecvGetData.push_back(inv);

      return true;
    }

    CBlock block;
    bool ret = ReadBlockFromDisk(block, it->second, chainparams.GetConsensus());
    assert(ret);

    SendBlockTransactions(block, req, pfrom, connman);
  }

  else if (strCommand == NetMsgType::GETHEADERS) {
    CBlockLocator locator;
    uint256 hashStop;
    vRecv >> locator >> hashStop;

    LOCK(cs_main);

    // Hardening: Rate limit GETHEADERS
    CNodeState *state = State(pfrom->GetId());
    if (state) {
      if (GetTime() - state->nLastGetHeadersTime < 60) {
        if (state->nGetHeadersCount > 20) {
          Misbehaving(pfrom->GetId(), 20);
          return error("too many getheaders");
        }
        state->nGetHeadersCount++;
      } else {
        state->nLastGetHeadersTime = GetTime();
        state->nGetHeadersCount = 1;
      }
    }
    if (IsInitialBlockDownload() && !pfrom->fWhitelisted) {
      LogPrint(BCLog::NET,
               "Ignoring getheaders from peer=%d because node is in initial "
               "block download\n",
               pfrom->GetId());
      return true;
    }

    CNodeState *nodestate = State(pfrom->GetId());

    // Introspection hardening: Track header request patterns
    if (gArgs.GetBoolArg("-introspectionhardening",
                         DEFAULT_ENABLE_INTROSPECTION_HARDENING)) {
      int64_t nNow = GetTime();
      const int64_t HEADER_REQUEST_WINDOW = 60; // 1 minute window
      const int MAX_HEADER_REQUESTS_PER_WINDOW = 20;
      const int INTROSPECTION_SCORE_THRESHOLD = 100;

      // Reset window if enough time has passed
      if (nNow - nodestate->nHeaderRequestWindow > HEADER_REQUEST_WINDOW) {
        nodestate->nRecentHeaderRequests = 0;
        nodestate->nHeaderRequestWindow = nNow;
      }

      nodestate->nRecentHeaderRequests++;

      // Check for excessive header requests (potential chain mapping)
      if (nodestate->nRecentHeaderRequests > MAX_HEADER_REQUESTS_PER_WINDOW) {
        nodestate->nIntrospectionScore += 10;
        nodestate->nLastIntrospectionTime = nNow;

        LogPrint(BCLog::NET,
                 "Peer %d excessive GETHEADERS requests: %d in window "
                 "(introspection score: %d)\n",
                 pfrom->GetId(), nodestate->nRecentHeaderRequests,
                 nodestate->nIntrospectionScore);

        // Disconnect peers with high introspection score
        if (nodestate->nIntrospectionScore >= INTROSPECTION_SCORE_THRESHOLD) {
          LogPrintf("WARNING: Disconnecting peer %d for suspicious chain "
                    "introspection behavior (score: %d)\n",
                    pfrom->GetId(), nodestate->nIntrospectionScore);
          pfrom->fDisconnect = true;
          return true;
        }
      }
    }
    const CBlockIndex *pindex = nullptr;
    if (locator.IsNull()) {
      BlockMap::iterator mi = mapBlockIndex.find(hashStop);
      if (mi == mapBlockIndex.end())
        return true;
      pindex = (*mi).second;

      if (!BlockRequestAllowed(pindex, chainparams.GetConsensus())) {
        LogPrint(BCLog::NET,
                 "%s: ignoring request from peer=%i for old block header that "
                 "isn't in the main chain\n",
                 __func__, pfrom->GetId());
        return true;
      }
    } else {
      pindex = FindForkInGlobalIndex(chainActive, locator);
      if (pindex)
        pindex = chainActive.Next(pindex);
    }

    std::vector<CBlock> vHeaders;
    int nLimit = MAX_HEADERS_RESULTS;
    LogPrint(BCLog::NET, "getheaders %d to %s from peer=%d\n",
             (pindex ? pindex->nHeight : -1),
             hashStop.IsNull() ? "end" : hashStop.ToString(), pfrom->GetId());
    for (; pindex; pindex = chainActive.Next(pindex)) {
      vHeaders.push_back(pindex->GetBlockHeader());
      if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
        break;
    }

    nodestate->pindexBestHeaderSent = pindex ? pindex : chainActive.Tip();
    connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::HEADERS, vHeaders));
  }

  else if (strCommand == NetMsgType::TX) {
    if (!fRelayTxes &&
        (!pfrom->fWhitelisted ||
         !gArgs.GetBoolArg("-whitelistrelay", DEFAULT_WHITELISTRELAY))) {
      LogPrint(BCLog::NET,
               "transaction sent in violation of protocol peer=%d\n",
               pfrom->GetId());
      Misbehaving(pfrom->GetId(), 10);
      return true;
    }

    std::deque<COutPoint> vWorkQueue;
    std::vector<uint256> vEraseQueue;
    CTransactionRef ptx;
    vRecv >> ptx;
    const CTransaction &tx = *ptx;

    CInv inv(MSG_TX, tx.GetHash());
    pfrom->AddInventoryKnown(inv);

    LOCK2(cs_main, g_cs_orphans);

    bool fMissingInputs = false;
    CValidationState state;

    pfrom->setAskFor.erase(inv.hash);
    mapAlreadyAskedFor.erase(inv.hash);

    std::list<CTransactionRef> lRemovedTxn;

    if (!AlreadyHave(inv) &&
        AcceptToMemoryPool(mempool, state, ptx, &fMissingInputs, &lRemovedTxn,
                           false, 0)) {
      mempool.check(pcoinsTip.get());
      RelayTransaction(tx, connman);
      for (unsigned int i = 0; i < tx.vout.size(); i++) {
        vWorkQueue.emplace_back(inv.hash, i);
      }

      pfrom->nLastTXTime = GetTime();

      LogPrint(
          BCLog::MEMPOOL,
          "AcceptToMemoryPool: peer=%d: accepted %s (poolsz %u txn, %u kB)\n",
          pfrom->GetId(), tx.GetHash().ToString(), mempool.size(),
          mempool.DynamicMemoryUsage() / 1000);

      std::set<NodeId> setMisbehaving;
      while (!vWorkQueue.empty()) {
        auto itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue.front());
        vWorkQueue.pop_front();
        if (itByPrev == mapOrphanTransactionsByPrev.end())
          continue;
        for (auto mi = itByPrev->second.begin(); mi != itByPrev->second.end();
             ++mi) {
          const CTransactionRef &porphanTx = (*mi)->second.tx;
          const CTransaction &orphanTx = *porphanTx;
          const uint256 &orphanHash = orphanTx.GetHash();
          NodeId fromPeer = (*mi)->second.fromPeer;
          bool fMissingInputs2 = false;

          CValidationState stateDummy;

          if (setMisbehaving.count(fromPeer))
            continue;
          if (AcceptToMemoryPool(mempool, stateDummy, porphanTx,
                                 &fMissingInputs2, &lRemovedTxn, false, 0)) {
            LogPrint(BCLog::MEMPOOL, "   accepted orphan tx %s\n",
                     orphanHash.ToString());
            RelayTransaction(orphanTx, connman);
            for (unsigned int i = 0; i < orphanTx.vout.size(); i++) {
              vWorkQueue.emplace_back(orphanHash, i);
            }
            vEraseQueue.push_back(orphanHash);
          } else if (!fMissingInputs2) {
            int nDos = 0;
            if (stateDummy.IsInvalid(nDos) && nDos > 0) {
              Misbehaving(fromPeer, nDos);
              setMisbehaving.insert(fromPeer);
              LogPrint(BCLog::MEMPOOL, "   invalid orphan tx %s\n",
                       orphanHash.ToString());
            }

            LogPrint(BCLog::MEMPOOL, "   removed orphan tx %s\n",
                     orphanHash.ToString());
            vEraseQueue.push_back(orphanHash);
            if (!orphanTx.HasWitness() && !stateDummy.CorruptionPossible()) {
              assert(recentRejects);
              recentRejects->insert(orphanHash);
            }
          }
          mempool.check(pcoinsTip.get());
        }
      }

      for (uint256 hash : vEraseQueue)
        EraseOrphanTx(hash);
    } else if (fMissingInputs) {
      bool fRejectedParents = false;

      for (const CTxIn &txin : tx.vin) {
        if (recentRejects->contains(txin.prevout.hash)) {
          fRejectedParents = true;
          break;
        }
      }
      if (!fRejectedParents) {
        uint32_t nFetchFlags = GetFetchFlags(pfrom);
        for (const CTxIn &txin : tx.vin) {
          CInv _inv(MSG_TX | nFetchFlags, txin.prevout.hash);
          pfrom->AddInventoryKnown(_inv);
          if (!AlreadyHave(_inv))
            pfrom->AskFor(_inv);
        }
        AddOrphanTx(ptx, pfrom->GetId());

        unsigned int nMaxOrphanTx = (unsigned int)std::max(
            (int64_t)0,
            gArgs.GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
        unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
        if (nEvicted > 0) {
          LogPrint(BCLog::MEMPOOL, "mapOrphan overflow, removed %u tx\n",
                   nEvicted);
        }
      } else {
        LogPrint(BCLog::MEMPOOL,
                 "not keeping orphan with rejected parents %s\n",
                 tx.GetHash().ToString());

        recentRejects->insert(tx.GetHash());
      }
    } else {
      if (!tx.HasWitness() && !state.CorruptionPossible()) {
        assert(recentRejects);
        recentRejects->insert(tx.GetHash());
        if (RecursiveDynamicUsage(*ptx) < 100000) {
          AddToCompactExtraTransactions(ptx);
        }
      } else if (tx.HasWitness() && RecursiveDynamicUsage(*ptx) < 100000) {
        AddToCompactExtraTransactions(ptx);
      }

      if (pfrom->fWhitelisted &&
          gArgs.GetBoolArg("-whitelistforcerelay",
                           DEFAULT_WHITELISTFORCERELAY)) {
        int nDoS = 0;
        if (!state.IsInvalid(nDoS) || nDoS == 0) {
          LogPrintf("Force relaying tx %s from whitelisted peer=%d\n",
                    tx.GetHash().ToString(), pfrom->GetId());
          RelayTransaction(tx, connman);
        } else {
          LogPrintf("Not relaying invalid transaction %s from whitelisted "
                    "peer=%d (%s)\n",
                    tx.GetHash().ToString(), pfrom->GetId(),
                    FormatStateMessage(state));
        }
      }
    }

    for (const CTransactionRef &removedTx : lRemovedTxn)
      AddToCompactExtraTransactions(removedTx);

    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
      LogPrint(BCLog::MEMPOOLREJ, "%s from peer=%d was not accepted: %s\n",
               tx.GetHash().ToString(), pfrom->GetId(),
               FormatStateMessage(state));
      if (state.GetRejectCode() > 0 && state.GetRejectCode() < REJECT_INTERNAL)

        connman->PushMessage(pfrom,
                             msgMaker.Make(NetMsgType::REJECT, strCommand,
                                           (unsigned char)state.GetRejectCode(),
                                           state.GetRejectReason().substr(
                                               0, MAX_REJECT_MESSAGE_LENGTH),
                                           inv.hash));
      if (nDoS > 0) {
        Misbehaving(pfrom->GetId(), nDoS);
      }
    }
  }

  else if (strCommand == NetMsgType::CMPCTBLOCK && !fImporting && !fReindex)

  {
    CBlockHeaderAndShortTxIDs cmpctblock;
    vRecv >> cmpctblock;

    bool received_new_header = false;

    {
      LOCK(cs_main);

      if (mapBlockIndex.find(cmpctblock.header.hashPrevBlock) ==
          mapBlockIndex.end()) {
        if (!IsInitialBlockDownload())
          connman->PushMessage(
              pfrom, msgMaker.Make(NetMsgType::GETHEADERS,
                                   chainActive.GetLocator(pindexBestHeader),
                                   uint256()));
        return true;
      }

      if (mapBlockIndex.find(cmpctblock.header.GetHash()) ==
          mapBlockIndex.end()) {
        received_new_header = true;
      }
    }

    const CBlockIndex *pindex = nullptr;
    CValidationState state;
    if (!ProcessNewBlockHeaders({cmpctblock.header}, state, chainparams,
                                &pindex)) {
      int nDoS;
      if (state.IsInvalid(nDoS)) {
        if (nDoS > 0) {
          LogPrintf("Peer %d sent us invalid header via cmpctblock\n",
                    pfrom->GetId());
          LOCK(cs_main);
          Misbehaving(pfrom->GetId(), nDoS);
        } else {
          LogPrint(BCLog::NET,
                   "Peer %d sent us invalid header via cmpctblock\n",
                   pfrom->GetId());
        }
        return true;
      }
    }

    bool fProcessBLOCKTXN = false;
    CDataStream blockTxnMsg(SER_NETWORK, PROTOCOL_VERSION);

    bool fRevertToHeaderProcessing = false;

    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
    bool fBlockReconstructed = false;

    {
      LOCK2(cs_main, g_cs_orphans);

      assert(pindex);
      UpdateBlockAvailability(pfrom->GetId(), pindex->GetBlockHash());

      CNodeState *nodestate = State(pfrom->GetId());

      if (received_new_header &&
          pindex->nChainWork > chainActive.Tip()->nChainWork) {
        nodestate->m_last_block_announcement = GetTime();
      }

      std::map<uint256,
               std::pair<NodeId, std::list<QueuedBlock>::iterator>>::iterator
          blockInFlightIt = mapBlocksInFlight.find(pindex->GetBlockHash());
      bool fAlreadyInFlight = blockInFlightIt != mapBlocksInFlight.end();

      if (pindex->nStatus & BLOCK_HAVE_DATA)

        return true;

      if (pindex->nChainWork <= chainActive.Tip()->nChainWork ||

          pindex->nTx != 0) {
        if (fAlreadyInFlight) {
          std::vector<CInv> vInv(1);
          vInv[0] = CInv(MSG_BLOCK | GetFetchFlags(pfrom),
                         cmpctblock.header.GetHash());
          connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::GETDATA, vInv));
        }
        return true;
      }

      if (!fAlreadyInFlight && !CanDirectFetch(chainparams.GetConsensus()))
        return true;

      if (IsWitnessEnabled(pindex->pprev, chainparams.GetConsensus()) &&
          !nodestate->fSupportsDesiredCmpctVersion) {
        return true;
      }

      if (pindex->nHeight <= chainActive.Height() + 2) {
        if ((!fAlreadyInFlight &&
             nodestate->nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) ||
            (fAlreadyInFlight &&
             blockInFlightIt->second.first == pfrom->GetId())) {
          std::list<QueuedBlock>::iterator *queuedBlockIt = nullptr;
          if (!MarkBlockAsInFlight(pfrom->GetId(), pindex->GetBlockHash(),
                                   pindex, &queuedBlockIt)) {
            if (!(*queuedBlockIt)->partialBlock)
              (*queuedBlockIt)
                  ->partialBlock.reset(new PartiallyDownloadedBlock(&mempool));
            else {
              LogPrint(BCLog::NET,
                       "Peer sent us compact block we were already syncing!\n");
              return true;
            }
          }

          PartiallyDownloadedBlock &partialBlock =
              *(*queuedBlockIt)->partialBlock;
          ReadStatus status =
              partialBlock.InitData(cmpctblock, vExtraTxnForCompact);
          if (status == READ_STATUS_INVALID) {
            MarkBlockAsReceived(pindex->GetBlockHash());

            Misbehaving(pfrom->GetId(), 100);
            LogPrintf("Peer %d sent us invalid compact block\n",
                      pfrom->GetId());
            return true;
          } else if (status == READ_STATUS_FAILED) {
            std::vector<CInv> vInv(1);
            vInv[0] = CInv(MSG_BLOCK | GetFetchFlags(pfrom),
                           cmpctblock.header.GetHash());
            connman->PushMessage(pfrom,
                                 msgMaker.Make(NetMsgType::GETDATA, vInv));
            return true;
          }

          BlockTransactionsRequest req;
          for (size_t i = 0; i < cmpctblock.BlockTxCount(); i++) {
            if (!partialBlock.IsTxAvailable(i))
              req.indexes.push_back(i);
          }
          if (req.indexes.empty()) {
            BlockTransactions txn;
            txn.blockhash = cmpctblock.header.GetHash();
            blockTxnMsg << txn;
            fProcessBLOCKTXN = true;
          } else {
            req.blockhash = pindex->GetBlockHash();
            connman->PushMessage(pfrom,
                                 msgMaker.Make(NetMsgType::GETBLOCKTXN, req));
          }
        } else {
          PartiallyDownloadedBlock tempBlock(&mempool);
          ReadStatus status =
              tempBlock.InitData(cmpctblock, vExtraTxnForCompact);
          if (status != READ_STATUS_OK) {
            return true;
          }
          std::vector<CTransactionRef> dummy;
          status = tempBlock.FillBlock(*pblock, dummy);
          if (status == READ_STATUS_OK) {
            fBlockReconstructed = true;
          }
        }
      } else {
        if (fAlreadyInFlight) {
          std::vector<CInv> vInv(1);
          vInv[0] = CInv(MSG_BLOCK | GetFetchFlags(pfrom),
                         cmpctblock.header.GetHash());
          connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::GETDATA, vInv));
          return true;
        } else {
          fRevertToHeaderProcessing = true;
        }
      }
    }

    if (fProcessBLOCKTXN)
      return ProcessMessage(pfrom, NetMsgType::BLOCKTXN, blockTxnMsg,
                            nTimeReceived, chainparams, connman,
                            interruptMsgProc);

    if (fRevertToHeaderProcessing) {
      return ProcessHeadersMessage(pfrom, connman, {cmpctblock.header},
                                   chainparams,

                                   false);
    }

    if (fBlockReconstructed) {
      {
        LOCK(cs_main);
        mapBlockSource.emplace(pblock->GetHash(),
                               std::make_pair(pfrom->GetId(), false));
      }
      bool fNewBlock = false;

      ProcessNewBlock(chainparams, pblock, true, &fNewBlock);
      if (fNewBlock) {
        pfrom->nLastBlockTime = GetTime();
      } else {
        LOCK(cs_main);
        mapBlockSource.erase(pblock->GetHash());
      }
      LOCK(cs_main);

      if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS)) {
        MarkBlockAsReceived(pblock->GetHash());
      }
    }

  }

  else if (strCommand == NetMsgType::BLOCKTXN && !fImporting && !fReindex)

  {
    BlockTransactions resp;
    vRecv >> resp;

    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
    bool fBlockRead = false;
    {
      LOCK(cs_main);

      std::map<uint256,
               std::pair<NodeId, std::list<QueuedBlock>::iterator>>::iterator
          it = mapBlocksInFlight.find(resp.blockhash);
      if (it == mapBlocksInFlight.end() || !it->second.second->partialBlock ||
          it->second.first != pfrom->GetId()) {
        LogPrint(BCLog::NET,
                 "Peer %d sent us block transactions for block we weren't "
                 "expecting\n",
                 pfrom->GetId());
        return true;
      }

      PartiallyDownloadedBlock &partialBlock = *it->second.second->partialBlock;
      ReadStatus status = partialBlock.FillBlock(*pblock, resp.txn);
      if (status == READ_STATUS_INVALID) {
        MarkBlockAsReceived(resp.blockhash);

        Misbehaving(pfrom->GetId(), 100);
        LogPrintf("Peer %d sent us invalid compact block/non-matching block "
                  "transactions\n",
                  pfrom->GetId());
        return true;
      } else if (status == READ_STATUS_FAILED) {
        std::vector<CInv> invs;
        invs.push_back(CInv(MSG_BLOCK | GetFetchFlags(pfrom), resp.blockhash));
        connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::GETDATA, invs));
      } else {
        MarkBlockAsReceived(resp.blockhash);

        fBlockRead = true;

        mapBlockSource.emplace(resp.blockhash,
                               std::make_pair(pfrom->GetId(), false));
      }
    }

    if (fBlockRead) {
      bool fNewBlock = false;

      ProcessNewBlock(chainparams, pblock, true, &fNewBlock);
      if (fNewBlock) {
        pfrom->nLastBlockTime = GetTime();
      } else {
        LOCK(cs_main);
        mapBlockSource.erase(pblock->GetHash());
      }
    }
  }

  else if (strCommand == NetMsgType::HEADERS && !fImporting && !fReindex)

  {
    std::vector<CBlockHeader> headers;

    unsigned int nCount = ReadCompactSize(vRecv);
    if (nCount > MAX_HEADERS_RESULTS) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 20);
      return error("headers message size = %u", nCount);
    }
    headers.resize(nCount);
    for (unsigned int n = 0; n < nCount; n++) {
      vRecv >> headers[n];
      ReadCompactSize(vRecv);
    }

    bool should_punish = !pfrom->fInbound && !pfrom->m_manual_connection;
    return ProcessHeadersMessage(pfrom, connman, headers, chainparams,
                                 should_punish);
  }

  else if (strCommand == NetMsgType::BLOCK && !fImporting && !fReindex)

  {
    std::shared_ptr<CBlock> pblock = std::make_shared<CBlock>();
    vRecv >> *pblock;

    // Hardening: Warn on future blocks
    if (pblock->GetBlockTime() > GetAdjustedTime() + 60 * 60) {
      LogPrintf("Warning: Future block received from peer=%d\n",
                pfrom->GetId());
    }

    LogPrint(BCLog::NET, "received block %s peer=%d\n",
             pblock->GetHash().ToString(), pfrom->GetId());

    bool forceProcessing = false;
    const uint256 hash(pblock->GetHash());
    {
      LOCK(cs_main);

      forceProcessing |= MarkBlockAsReceived(hash);

      mapBlockSource.emplace(hash, std::make_pair(pfrom->GetId(), true));
    }
    bool fNewBlock = false;
    ProcessNewBlock(chainparams, pblock, forceProcessing, &fNewBlock);
    if (fNewBlock) {
      pfrom->nLastBlockTime = GetTime();
    } else {
      LOCK(cs_main);
      mapBlockSource.erase(pblock->GetHash());
    }
  }

  else if (strCommand == NetMsgType::GETADDR) {
    if (!pfrom->fInbound) {
      LogPrint(BCLog::NET,
               "Ignoring \"getaddr\" from outbound connection. peer=%d\n",
               pfrom->GetId());
      return true;
    }

    if (pfrom->fSentAddr) {
      LogPrint(BCLog::NET, "Ignoring repeated \"getaddr\". peer=%d\n",
               pfrom->GetId());
      return true;
    }
    pfrom->fSentAddr = true;

    pfrom->vAddrToSend.clear();
    std::vector<CAddress> vAddr = connman->GetAddresses();
    FastRandomContext insecure_rand;
    for (const CAddress &addr : vAddr)
      pfrom->PushAddress(addr, insecure_rand);
  }

  else if (strCommand == NetMsgType::MEMPOOL) {
    // Hardening: Rate limit MEMPOOL
    CNodeState *state = State(pfrom->GetId());
    if (state) {
      if (GetTime() - state->nLastMempoolReqTime < 60 * 60) {
        Misbehaving(pfrom->GetId(), 10);
        return true;
      }
      state->nLastMempoolReqTime = GetTime();
    }

    if (!(pfrom->GetLocalServices() & NODE_BLOOM) && !pfrom->fWhitelisted) {
      LogPrint(
          BCLog::NET,
          "mempool request with bloom filters disabled, disconnect peer=%d\n",
          pfrom->GetId());
      pfrom->fDisconnect = true;
      return true;
    }

    if (connman->OutboundTargetReached(false) && !pfrom->fWhitelisted) {
      LogPrint(
          BCLog::NET,
          "mempool request with bandwidth limit reached, disconnect peer=%d\n",
          pfrom->GetId());
      pfrom->fDisconnect = true;
      return true;
    }

    LOCK(pfrom->cs_inventory);
    pfrom->fSendMempool = true;
  }

  else if (strCommand == NetMsgType::PING) {
    if (pfrom->nVersion > BIP0031_VERSION) {
      uint64_t nonce = 0;
      vRecv >> nonce;

      connman->PushMessage(pfrom, msgMaker.Make(NetMsgType::PONG, nonce));
    }
  }

  else if (strCommand == NetMsgType::PONG) {
    int64_t pingUsecEnd = nTimeReceived;
    uint64_t nonce = 0;
    size_t nAvail = vRecv.in_avail();
    bool bPingFinished = false;
    std::string sProblem;

    if (nAvail >= sizeof(nonce)) {
      vRecv >> nonce;

      if (pfrom->nPingNonceSent != 0) {
        if (nonce == pfrom->nPingNonceSent) {
          bPingFinished = true;
          int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
          if (pingUsecTime > 0) {
            pfrom->nPingUsecTime = pingUsecTime;
            pfrom->nMinPingUsecTime =
                std::min(pfrom->nMinPingUsecTime.load(), pingUsecTime);
          } else {
            sProblem = "Timing mishap";
          }
        } else {
          sProblem = "Nonce mismatch";
          // Phase 2 Hardening: Penalize PONG mismatches
          {
            LOCK(cs_main);
            CNodeState *state = State(pfrom->GetId());
            if (state) {
              state->nPongMismatchCount++;
              if (state->nPongMismatchCount > 3) {
                Misbehaving(pfrom->GetId(), 10);
                LogPrint(BCLog::NET, "Peer %d repeated PONG mismatch: %d\n",
                         pfrom->GetId(), state->nPongMismatchCount);
              }
            }
          }
          if (nonce == 0) {
            bPingFinished = true;
            sProblem = "Nonce zero";
          }
        }
      } else {
        sProblem = "Unsolicited pong without ping";
      }
    } else {
      bPingFinished = true;
      sProblem = "Short payload";
    }

    if (!(sProblem.empty())) {
      LogPrint(BCLog::NET,
               "pong peer=%d: %s, %x expected, %x received, %u bytes\n",
               pfrom->GetId(), sProblem, pfrom->nPingNonceSent, nonce, nAvail);
    }
    if (bPingFinished) {
      pfrom->nPingNonceSent = 0;
    }
  }

  else if (strCommand == NetMsgType::FILTERLOAD) {
    // Phase 2 Hardening: Rate limit FILTERLOAD (CPU-intensive)
    {
      LOCK(cs_main);
      CNodeState *state = State(pfrom->GetId());
      if (state) {
        int64_t nNow = GetTime();
        if (nNow - state->nLastFilterLoadTime < 600) { // 10 minute window
          state->nFilterLoadCount++;
          if (state->nFilterLoadCount > 1) {
            Misbehaving(pfrom->GetId(), 50);
            LogPrint(BCLog::NET, "Peer %d FILTERLOAD spam: %d in window\n",
                     pfrom->GetId(), state->nFilterLoadCount);
            return true;
          }
        } else {
          state->nLastFilterLoadTime = nNow;
          state->nFilterLoadCount = 1;
        }
      }
    }

    CBloomFilter filter;
    vRecv >> filter;

    if (!filter.IsWithinSizeConstraints()) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 100);
    } else {
      LOCK(pfrom->cs_filter);
      pfrom->pfilter.reset(new CBloomFilter(filter));
      pfrom->pfilter->UpdateEmptyFull();
      pfrom->fRelayTxes = true;
    }
  }

  else if (strCommand == NetMsgType::FILTERADD) {
    std::vector<unsigned char> vData;
    vRecv >> vData;

    bool bad = false;
    if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE) {
      bad = true;
    } else {
      LOCK(pfrom->cs_filter);
      if (pfrom->pfilter) {
        pfrom->pfilter->insert(vData);
      } else {
        bad = true;
      }
    }
    if (bad) {
      LOCK(cs_main);
      Misbehaving(pfrom->GetId(), 100);
    }
  }

  else if (strCommand == NetMsgType::FILTERCLEAR) {
    LOCK(pfrom->cs_filter);
    if (pfrom->GetLocalServices() & NODE_BLOOM) {
      pfrom->pfilter.reset(new CBloomFilter());
    }
    pfrom->fRelayTxes = true;
  }

  else if (strCommand == NetMsgType::FEEFILTER) {
    CAmount newFeeFilter = 0;
    vRecv >> newFeeFilter;
    if (MoneyRange(newFeeFilter)) {
      {
        LOCK(pfrom->cs_feeFilter);
        pfrom->minFeeFilter = newFeeFilter;
      }
      LogPrint(BCLog::NET, "received: feefilter of %s from peer=%d\n",
               CFeeRate(newFeeFilter).ToString(), pfrom->GetId());
    }
  }

  else if (strCommand == NetMsgType::NOTFOUND) {
    // Phase 2 Hardening: Rate limit NOTFOUND messages
    LOCK(cs_main);
    CNodeState *state = State(pfrom->GetId());
    if (state) {
      int64_t nNow = GetTime();
      if (nNow - state->nLastNotFoundTime > 60) {
        state->nLastNotFoundTime = nNow;
        state->nNotFoundCount = 0;
      }
      state->nNotFoundCount++;
      if (state->nNotFoundCount > 100) {
        Misbehaving(pfrom->GetId(), 10);
        LogPrint(BCLog::NET, "Peer %d NOTFOUND spam: %d in window\n",
                 pfrom->GetId(), state->nNotFoundCount);
      }
    }
  }

  else if (strCommand == NetMsgType::RIALTO) {
    if ((g_connman->GetLocalServices() & NODE_RIALTO) != NODE_RIALTO) {
      LogPrintf("Rialto: Message received from peer=%d, but Rialto is not "
                "enabled on this node. Punishing peer.\n",
                pfrom->GetId());
      Misbehaving(pfrom->GetId(), 20);
      return true;
    }

    std::string strMsg;
    vRecv >> LIMITED_STRING(strMsg, (RIALTO_L3_MAX_LENGTH * 2));

    std::string err;
    if (!RialtoParseLayer3Envelope(strMsg, err)) {
      LogPrintf("Rialto: Invalid message received from peer=%d; punishing. "
                "Error: %s\n",
                pfrom->GetId(), err);
      Misbehaving(pfrom->GetId(), 20);
      return true;
    }

    if (RialtoDecryptMessage(strMsg, err))
      LogPrint(BCLog::RIALTO, "Rialto: Message added to receive queue\n");
    else
      LogPrint(BCLog::RIALTO, "Rialto: Message decrypt error: %s\n", err);

    CRialtoMessage message(strMsg);
    RelayRialtoMessage(message, connman, pfrom);
  }

  else {
    LogPrint(BCLog::NET, "Unknown command \"%s\" from peer=%d\n",
             SanitizeString(strCommand), pfrom->GetId());
  }

  return true;
}

static bool SendRejectsAndCheckIfBanned(CNode *pnode, CConnman *connman) {
  AssertLockHeld(cs_main);
  CNodeState &state = *State(pnode->GetId());

  for (const CBlockReject &reject : state.rejects) {
    connman->PushMessage(
        pnode, CNetMsgMaker(INIT_PROTO_VERSION)
                   .Make(NetMsgType::REJECT, (std::string)NetMsgType::BLOCK,
                         reject.chRejectCode, reject.strRejectReason,
                         reject.hashBlock));
  }
  state.rejects.clear();

  if (state.fShouldBan) {
    state.fShouldBan = false;
    if (pnode->fWhitelisted)
      LogPrintf("Warning: not punishing whitelisted peer %s!\n",
                pnode->addr.ToString());
    else if (pnode->m_manual_connection)
      LogPrintf("Warning: not punishing manually-connected peer %s!\n",
                pnode->addr.ToString());
    else {
      pnode->fDisconnect = true;
      if (pnode->addr.IsLocal())
        LogPrintf("Warning: not banning local peer %s!\n",
                  pnode->addr.ToString());
      else {
        connman->Ban(pnode->addr, BanReasonNodeMisbehaving);
      }
    }
    return true;
  }
  return false;
}

bool PeerLogicValidation::ProcessMessages(CNode *pfrom,
                                          std::atomic<bool> &interruptMsgProc) {
  const CChainParams &chainparams = Params();

  bool fMoreWork = false;

  if (!pfrom->vRecvGetData.empty())
    ProcessGetData(pfrom, chainparams.GetConsensus(), connman,
                   interruptMsgProc);

  if (pfrom->fDisconnect)
    return false;

  if (!pfrom->vRecvGetData.empty())
    return true;

  if (pfrom->fPauseSend)
    return false;

  std::list<CNetMessage> msgs;
  {
    LOCK(pfrom->cs_vProcessMsg);
    if (pfrom->vProcessMsg.empty())
      return false;

    msgs.splice(msgs.begin(), pfrom->vProcessMsg, pfrom->vProcessMsg.begin());
    pfrom->nProcessQueueSize -=
        msgs.front().vRecv.size() + CMessageHeader::HEADER_SIZE;
    pfrom->fPauseRecv =
        pfrom->nProcessQueueSize > connman->GetReceiveFloodSize();
    fMoreWork = !pfrom->vProcessMsg.empty();
  }
  CNetMessage &msg(msgs.front());

  msg.SetVersion(pfrom->GetRecvVersion());

  if (memcmp(msg.hdr.pchMessageStart, chainparams.MessageStart(),
             CMessageHeader::MESSAGE_START_SIZE) != 0) {
    LogPrint(BCLog::NET, "PROCESSMESSAGE: INVALID MESSAGESTART %s peer=%d\n",
             SanitizeString(msg.hdr.GetCommand()), pfrom->GetId());
    pfrom->fDisconnect = true;
    return false;
  }

  CMessageHeader &hdr = msg.hdr;
  if (!hdr.IsValid(chainparams.MessageStart())) {
    LogPrint(BCLog::NET, "PROCESSMESSAGE: ERRORS IN HEADER %s peer=%d\n",
             SanitizeString(hdr.GetCommand()), pfrom->GetId());
    return fMoreWork;
  }
  std::string strCommand = hdr.GetCommand();

  unsigned int nMessageSize = hdr.nMessageSize;

  CDataStream &vRecv = msg.vRecv;
  const uint256 &hash = msg.GetMessageHash();
  if (memcmp(hash.begin(), hdr.pchChecksum, CMessageHeader::CHECKSUM_SIZE) !=
      0) {
    LogPrint(BCLog::NET,
             "%s(%s, %u bytes): CHECKSUM ERROR expected %s was %s\n", __func__,
             SanitizeString(strCommand), nMessageSize,
             HexStr(hash.begin(), hash.begin() + CMessageHeader::CHECKSUM_SIZE),
             HexStr(hdr.pchChecksum,
                    hdr.pchChecksum + CMessageHeader::CHECKSUM_SIZE));
    return fMoreWork;
  }

  bool fRet = false;
  try {
    fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime, chainparams,
                          connman, interruptMsgProc);
    if (interruptMsgProc)
      return false;
    if (!pfrom->vRecvGetData.empty())
      fMoreWork = true;
  } catch (const std::ios_base::failure &e) {
    connman->PushMessage(
        pfrom, CNetMsgMaker(INIT_PROTO_VERSION)
                   .Make(NetMsgType::REJECT, strCommand, REJECT_MALFORMED,
                         std::string("error parsing message")));
    if (strstr(e.what(), "end of data")) {
      LogPrint(BCLog::NET,
               "%s(%s, %u bytes): Exception '%s' caught, normally caused by a "
               "message being shorter than its stated length\n",
               __func__, SanitizeString(strCommand), nMessageSize, e.what());
    } else if (strstr(e.what(), "size too large")) {
      LogPrint(BCLog::NET, "%s(%s, %u bytes): Exception '%s' caught\n",
               __func__, SanitizeString(strCommand), nMessageSize, e.what());
    } else if (strstr(e.what(), "non-canonical ReadCompactSize()")) {
      LogPrint(BCLog::NET, "%s(%s, %u bytes): Exception '%s' caught\n",
               __func__, SanitizeString(strCommand), nMessageSize, e.what());
    } else {
      PrintExceptionContinue(&e, "ProcessMessages()");
    }
  } catch (const std::exception &e) {
    PrintExceptionContinue(&e, "ProcessMessages()");
  } catch (...) {
    PrintExceptionContinue(nullptr, "ProcessMessages()");
  }

  if (!fRet) {
    LogPrint(BCLog::NET, "%s(%s, %u bytes) FAILED peer=%d\n", __func__,
             SanitizeString(strCommand), nMessageSize, pfrom->GetId());
  }

  LOCK(cs_main);
  SendRejectsAndCheckIfBanned(pfrom, connman);

  return fMoreWork;
}

void PeerLogicValidation::ConsiderEviction(CNode *pto,
                                           int64_t time_in_seconds) {
  AssertLockHeld(cs_main);

  CNodeState &state = *State(pto->GetId());
  const CNetMsgMaker msgMaker(pto->GetSendVersion());

  if (!state.m_chain_sync.m_protect && IsOutboundDisconnectionCandidate(pto) &&
      state.fSyncStarted) {
    if (state.pindexBestKnownBlock != nullptr &&
        state.pindexBestKnownBlock->nChainWork >=
            chainActive.Tip()->nChainWork) {
      if (state.m_chain_sync.m_timeout != 0) {
        state.m_chain_sync.m_timeout = 0;
        state.m_chain_sync.m_work_header = nullptr;
        state.m_chain_sync.m_sent_getheaders = false;
      }
    } else if (state.m_chain_sync.m_timeout == 0 ||
               (state.m_chain_sync.m_work_header != nullptr &&
                state.pindexBestKnownBlock != nullptr &&
                state.pindexBestKnownBlock->nChainWork >=
                    state.m_chain_sync.m_work_header->nChainWork)) {
      state.m_chain_sync.m_timeout = time_in_seconds + CHAIN_SYNC_TIMEOUT;
      state.m_chain_sync.m_work_header = chainActive.Tip();
      state.m_chain_sync.m_sent_getheaders = false;
    } else if (state.m_chain_sync.m_timeout > 0 &&
               time_in_seconds > state.m_chain_sync.m_timeout) {
      if (state.m_chain_sync.m_sent_getheaders) {
        LogPrintf("Disconnecting outbound peer %d for old chain, best known "
                  "block = %s\n",
                  pto->GetId(),
                  state.pindexBestKnownBlock != nullptr
                      ? state.pindexBestKnownBlock->GetBlockHash().ToString()
                      : "<none>");
        pto->fDisconnect = true;
      } else {
        assert(state.m_chain_sync.m_work_header);
        LogPrint(BCLog::NET,
                 "sending getheaders to outbound peer=%d to verify chain work "
                 "(current best known block:%s, benchmark blockhash: %s)\n",
                 pto->GetId(),
                 state.pindexBestKnownBlock != nullptr
                     ? state.pindexBestKnownBlock->GetBlockHash().ToString()
                     : "<none>",
                 state.m_chain_sync.m_work_header->GetBlockHash().ToString());
        connman->PushMessage(
            pto, msgMaker.Make(NetMsgType::GETHEADERS,
                               chainActive.GetLocator(
                                   state.m_chain_sync.m_work_header->pprev),
                               uint256()));
        state.m_chain_sync.m_sent_getheaders = true;
        constexpr int64_t HEADERS_RESPONSE_TIME = 120;

        state.m_chain_sync.m_timeout = time_in_seconds + HEADERS_RESPONSE_TIME;
      }
    }
  }
}

void PeerLogicValidation::EvictExtraOutboundPeers(int64_t time_in_seconds) {
  int extra_peers = connman->GetExtraOutboundCount();
  if (extra_peers > 0) {
    NodeId worst_peer = -1;
    int64_t oldest_block_announcement = std::numeric_limits<int64_t>::max();

    LOCK(cs_main);

    connman->ForEachNode([&](CNode *pnode) {
      if (!IsOutboundDisconnectionCandidate(pnode) || pnode->fDisconnect)
        return;
      CNodeState *state = State(pnode->GetId());
      if (state == nullptr)
        return;

      if (state->m_chain_sync.m_protect)
        return;
      if (state->m_last_block_announcement < oldest_block_announcement ||
          (state->m_last_block_announcement == oldest_block_announcement &&
           pnode->GetId() > worst_peer)) {
        worst_peer = pnode->GetId();
        oldest_block_announcement = state->m_last_block_announcement;
      }
    });
    if (worst_peer != -1) {
      bool disconnected = connman->ForNode(worst_peer, [&](CNode *pnode) {
        CNodeState &state = *State(pnode->GetId());
        if (time_in_seconds - pnode->nTimeConnected > MINIMUM_CONNECT_TIME &&
            state.nBlocksInFlight == 0) {
          LogPrint(BCLog::NET,
                   "disconnecting extra outbound peer=%d (last block "
                   "announcement received at time %d)\n",
                   pnode->GetId(), oldest_block_announcement);
          pnode->fDisconnect = true;
          return true;
        } else {
          LogPrint(BCLog::NET,
                   "keeping outbound peer=%d chosen for eviction (connect "
                   "time: %d, blocks_in_flight: %d)\n",
                   pnode->GetId(), pnode->nTimeConnected,
                   state.nBlocksInFlight);
          return false;
        }
      });
      if (disconnected) {
        connman->SetTryNewOutboundPeer(false);
      }
    }
  }
}

void PeerLogicValidation::CheckForStaleTipAndEvictPeers(
    const Consensus::Params &consensusParams) {
  if (connman == nullptr)
    return;

  int64_t time_in_seconds = GetTime();

  EvictExtraOutboundPeers(time_in_seconds);

  if (time_in_seconds > m_stale_tip_check_time) {
    LOCK(cs_main);

    if (TipMayBeStale(consensusParams)) {
      LogPrintf("Potential stale tip detected, will try using extra outbound "
                "peer (last tip update: %d seconds ago)\n",
                time_in_seconds - g_last_tip_update);
      connman->SetTryNewOutboundPeer(true);
    } else if (connman->GetTryNewOutboundPeer()) {
      connman->SetTryNewOutboundPeer(false);
    }
    m_stale_tip_check_time = time_in_seconds + STALE_CHECK_INTERVAL;
  }
}

class CompareInvMempoolOrder {
  CTxMemPool *mp;

public:
  explicit CompareInvMempoolOrder(CTxMemPool *_mempool) { mp = _mempool; }

  bool operator()(std::set<uint256>::iterator a,
                  std::set<uint256>::iterator b) {
    return mp->CompareDepthAndScore(*b, *a);
  }
};

bool PeerLogicValidation::SendMessages(CNode *pto,
                                       std::atomic<bool> &interruptMsgProc) {
  const Consensus::Params &consensusParams = Params().GetConsensus();
  {
    if (!pto->fSuccessfullyConnected || pto->fDisconnect)
      return true;

    const CNetMsgMaker msgMaker(pto->GetSendVersion());

    bool pingSend = false;
    if (pto->fPingQueued) {
      pingSend = true;
    }
    if (pto->nPingNonceSent == 0 &&
        pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {
      pingSend = true;
    }
    if (pingSend) {
      uint64_t nonce = 0;
      while (nonce == 0) {
        GetRandBytes((unsigned char *)&nonce, sizeof(nonce));
      }
      pto->fPingQueued = false;
      pto->nPingUsecStart = GetTimeMicros();
      if (pto->nVersion > BIP0031_VERSION) {
        pto->nPingNonceSent = nonce;
        connman->PushMessage(pto, msgMaker.Make(NetMsgType::PING, nonce));
      } else {
        pto->nPingNonceSent = 0;
        connman->PushMessage(pto, msgMaker.Make(NetMsgType::PING));
      }
    }

    TRY_LOCK(cs_main, lockMain);

    if (!lockMain)
      return true;

    if (SendRejectsAndCheckIfBanned(pto, connman))
      return true;
    CNodeState &state = *State(pto->GetId());

    int64_t nNow = GetTimeMicros();
    if (!IsInitialBlockDownload() && pto->nNextLocalAddrSend < nNow) {
      AdvertiseLocal(pto);
      pto->nNextLocalAddrSend =
          PoissonNextSend(nNow, AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL);
    }

    if (pto->nNextAddrSend < nNow) {
      pto->nNextAddrSend =
          PoissonNextSend(nNow, AVG_ADDRESS_BROADCAST_INTERVAL);
      std::vector<CAddress> vAddr;
      vAddr.reserve(pto->vAddrToSend.size());
      for (const CAddress &addr : pto->vAddrToSend) {
        if (!pto->addrKnown.contains(addr.GetKey())) {
          pto->addrKnown.insert(addr.GetKey());
          vAddr.push_back(addr);

          if (vAddr.size() >= 1000) {
            connman->PushMessage(pto, msgMaker.Make(NetMsgType::ADDR, vAddr));
            vAddr.clear();
          }
        }
      }
      pto->vAddrToSend.clear();
      if (!vAddr.empty())
        connman->PushMessage(pto, msgMaker.Make(NetMsgType::ADDR, vAddr));

      if (pto->vAddrToSend.capacity() > 40)
        pto->vAddrToSend.shrink_to_fit();
    }

    if (pindexBestHeader == nullptr)
      pindexBestHeader = chainActive.Tip();
    bool fFetch = state.fPreferredDownload ||
                  (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot);

    if (!state.fSyncStarted && !pto->fClient && !fImporting && !fReindex) {
      if ((nSyncStarted == 0 && fFetch) ||
          pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 24 * 60 * 60) {
        state.fSyncStarted = true;
        state.nHeadersSyncTimeout =
            GetTimeMicros() + HEADERS_DOWNLOAD_TIMEOUT_BASE +
            HEADERS_DOWNLOAD_TIMEOUT_PER_HEADER *
                (GetAdjustedTime() - pindexBestHeader->GetBlockTime()) /
                (consensusParams.nPowTargetSpacing);
        nSyncStarted++;
        const CBlockIndex *pindexStart = pindexBestHeader;

        if (pindexStart->pprev)
          pindexStart = pindexStart->pprev;
        LogPrint(BCLog::NET,
                 "initial getheaders (%d) to peer=%d (startheight:%d)\n",
                 pindexStart->nHeight, pto->GetId(), pto->nStartingHeight);
        connman->PushMessage(
            pto, msgMaker.Make(NetMsgType::GETHEADERS,
                               chainActive.GetLocator(pindexStart), uint256()));
      }
    }

    if (!fReindex && !fImporting && !IsInitialBlockDownload()) {
      GetMainSignals().Broadcast(nTimeBestReceived, connman);
    }

    {
      LOCK(pto->cs_inventory);
      std::vector<CBlock> vHeaders;
      bool fRevertToInv =
          ((!state.fPreferHeaders &&
            (!state.fPreferHeaderAndIDs ||
             pto->vBlockHashesToAnnounce.size() > 1)) ||
           pto->vBlockHashesToAnnounce.size() > MAX_BLOCKS_TO_ANNOUNCE);
      const CBlockIndex *pBestIndex = nullptr;

      ProcessBlockAvailability(pto->GetId());

      if (!fRevertToInv) {
        bool fFoundStartingHeader = false;

        for (const uint256 &hash : pto->vBlockHashesToAnnounce) {
          BlockMap::iterator mi = mapBlockIndex.find(hash);
          assert(mi != mapBlockIndex.end());
          const CBlockIndex *pindex = mi->second;
          if (chainActive[pindex->nHeight] != pindex) {
            fRevertToInv = true;
            break;
          }
          if (pBestIndex != nullptr && pindex->pprev != pBestIndex) {
            fRevertToInv = true;
            break;
          }
          pBestIndex = pindex;
          if (fFoundStartingHeader) {
            vHeaders.push_back(pindex->GetBlockHeader());
          } else if (PeerHasHeader(&state, pindex)) {
            continue;

          } else if (pindex->pprev == nullptr ||
                     PeerHasHeader(&state, pindex->pprev)) {
            fFoundStartingHeader = true;
            vHeaders.push_back(pindex->GetBlockHeader());
          } else {
            fRevertToInv = true;
            break;
          }
        }
      }
      if (!fRevertToInv && !vHeaders.empty()) {
        if (vHeaders.size() == 1 && state.fPreferHeaderAndIDs) {
          LogPrint(BCLog::NET, "%s sending header-and-ids %s to peer=%d\n",
                   __func__, vHeaders.front().GetHash().ToString(),
                   pto->GetId());

          int nSendFlags =
              state.fWantsCmpctWitness ? 0 : SERIALIZE_TRANSACTION_NO_WITNESS;

          bool fGotBlockFromCache = false;
          {
            LOCK(cs_most_recent_block);
            if (most_recent_block_hash == pBestIndex->GetBlockHash()) {
              if (state.fWantsCmpctWitness ||
                  !fWitnessesPresentInMostRecentCompactBlock)
                connman->PushMessage(
                    pto, msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK,
                                       *most_recent_compact_block));
              else {
                CBlockHeaderAndShortTxIDs cmpctblock(*most_recent_block,
                                                     state.fWantsCmpctWitness);
                connman->PushMessage(pto, msgMaker.Make(nSendFlags,
                                                        NetMsgType::CMPCTBLOCK,
                                                        cmpctblock));
              }
              fGotBlockFromCache = true;
            }
          }
          if (!fGotBlockFromCache) {
            CBlock block;
            bool ret = ReadBlockFromDisk(block, pBestIndex, consensusParams);
            assert(ret);
            CBlockHeaderAndShortTxIDs cmpctblock(block,
                                                 state.fWantsCmpctWitness);
            connman->PushMessage(
                pto,
                msgMaker.Make(nSendFlags, NetMsgType::CMPCTBLOCK, cmpctblock));
          }
          state.pindexBestHeaderSent = pBestIndex;
        } else if (state.fPreferHeaders) {
          if (vHeaders.size() > 1) {
            LogPrint(BCLog::NET, "%s: %u headers, range (%s, %s), to peer=%d\n",
                     __func__, vHeaders.size(),
                     vHeaders.front().GetHash().ToString(),
                     vHeaders.back().GetHash().ToString(), pto->GetId());
          } else {
            LogPrint(BCLog::NET, "%s: sending header %s to peer=%d\n", __func__,
                     vHeaders.front().GetHash().ToString(), pto->GetId());
          }
          connman->PushMessage(pto,
                               msgMaker.Make(NetMsgType::HEADERS, vHeaders));
          state.pindexBestHeaderSent = pBestIndex;
        } else
          fRevertToInv = true;
      }
      if (fRevertToInv) {
        if (!pto->vBlockHashesToAnnounce.empty()) {
          const uint256 &hashToAnnounce = pto->vBlockHashesToAnnounce.back();
          BlockMap::iterator mi = mapBlockIndex.find(hashToAnnounce);
          assert(mi != mapBlockIndex.end());
          const CBlockIndex *pindex = mi->second;

          if (chainActive[pindex->nHeight] != pindex) {
            LogPrint(BCLog::NET,
                     "Announcing block %s not on main chain (tip=%s)\n",
                     hashToAnnounce.ToString(),
                     chainActive.Tip()->GetBlockHash().ToString());
          }

          if (!PeerHasHeader(&state, pindex)) {
            pto->PushInventory(CInv(MSG_BLOCK, hashToAnnounce));
            LogPrint(BCLog::NET, "%s: sending inv peer=%d hash=%s\n", __func__,
                     pto->GetId(), hashToAnnounce.ToString());
          }
        }
      }
      pto->vBlockHashesToAnnounce.clear();
    }

    std::vector<CInv> vInv;
    {
      LOCK(pto->cs_inventory);
      vInv.reserve(std::max<size_t>(pto->vInventoryBlockToSend.size(),
                                    INVENTORY_BROADCAST_MAX));

      for (const uint256 &hash : pto->rialtoInventoryToSend)
        vInv.push_back(CInv(MSG_RIALTO, hash));
      pto->rialtoInventoryToSend.clear();

      for (const uint256 &hash : pto->vInventoryBlockToSend) {
        vInv.push_back(CInv(MSG_BLOCK, hash));
        if (vInv.size() == MAX_INV_SZ) {
          connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));
          vInv.clear();
        }
      }
      pto->vInventoryBlockToSend.clear();

      bool fSendTrickle = pto->fWhitelisted;
      if (pto->nNextInvSend < nNow) {
        fSendTrickle = true;

        pto->nNextInvSend = PoissonNextSend(
            nNow, INVENTORY_BROADCAST_INTERVAL >> !pto->fInbound);
      }

      if (fSendTrickle) {
        LOCK(pto->cs_filter);
        if (!pto->fRelayTxes)
          pto->setInventoryTxToSend.clear();
      }

      if (fSendTrickle && pto->fSendMempool) {
        auto vtxinfo = mempool.infoAll();
        pto->fSendMempool = false;
        CAmount filterrate = 0;
        {
          LOCK(pto->cs_feeFilter);
          filterrate = pto->minFeeFilter;
        }

        LOCK(pto->cs_filter);

        for (const auto &txinfo : vtxinfo) {
          const uint256 &hash = txinfo.tx->GetHash();
          CInv inv(MSG_TX, hash);
          pto->setInventoryTxToSend.erase(hash);
          if (filterrate) {
            if (txinfo.feeRate.GetFeePerK() < filterrate)
              continue;
          }
          if (pto->pfilter) {
            if (!pto->pfilter->IsRelevantAndUpdate(*txinfo.tx))
              continue;
          }
          pto->filterInventoryKnown.insert(hash);
          vInv.push_back(inv);
          if (vInv.size() == MAX_INV_SZ) {
            connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));
            vInv.clear();
          }
        }
        pto->timeLastMempoolReq = GetTime();
      }

      if (fSendTrickle) {
        std::vector<std::set<uint256>::iterator> vInvTx;
        vInvTx.reserve(pto->setInventoryTxToSend.size());
        for (std::set<uint256>::iterator it = pto->setInventoryTxToSend.begin();
             it != pto->setInventoryTxToSend.end(); it++) {
          vInvTx.push_back(it);
        }
        CAmount filterrate = 0;
        {
          LOCK(pto->cs_feeFilter);
          filterrate = pto->minFeeFilter;
        }

        CompareInvMempoolOrder compareInvMempoolOrder(&mempool);
        std::make_heap(vInvTx.begin(), vInvTx.end(), compareInvMempoolOrder);

        unsigned int nRelayedTransactions = 0;
        LOCK(pto->cs_filter);
        while (!vInvTx.empty() &&
               nRelayedTransactions < INVENTORY_BROADCAST_MAX) {
          std::pop_heap(vInvTx.begin(), vInvTx.end(), compareInvMempoolOrder);
          std::set<uint256>::iterator it = vInvTx.back();
          vInvTx.pop_back();
          uint256 hash = *it;

          pto->setInventoryTxToSend.erase(it);

          if (pto->filterInventoryKnown.contains(hash)) {
            continue;
          }

          auto txinfo = mempool.info(hash);
          if (!txinfo.tx) {
            continue;
          }
          if (filterrate && txinfo.feeRate.GetFeePerK() < filterrate) {
            continue;
          }
          if (pto->pfilter && !pto->pfilter->IsRelevantAndUpdate(*txinfo.tx))
            continue;

          vInv.push_back(CInv(MSG_TX, hash));
          nRelayedTransactions++;
          {
            while (!vRelayExpiration.empty() &&
                   vRelayExpiration.front().first < nNow) {
              mapRelay.erase(vRelayExpiration.front().second);
              vRelayExpiration.pop_front();
            }

            auto ret =
                mapRelay.insert(std::make_pair(hash, std::move(txinfo.tx)));
            if (ret.second) {
              vRelayExpiration.push_back(
                  std::make_pair(nNow + 15 * 60 * 1000000, ret.first));
            }
          }
          if (vInv.size() == MAX_INV_SZ) {
            connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));
            vInv.clear();
          }
          pto->filterInventoryKnown.insert(hash);
        }
      }
    }
    if (!vInv.empty())
      connman->PushMessage(pto, msgMaker.Make(NetMsgType::INV, vInv));

    nNow = GetTimeMicros();
    if (state.nStallingSince &&
        state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {
      LogPrintf("Peer=%d is stalling block download, disconnecting\n",
                pto->GetId());
      pto->fDisconnect = true;
      return true;
    }

    if (state.vBlocksInFlight.size() > 0) {
      QueuedBlock &queuedBlock = state.vBlocksInFlight.front();
      int nOtherPeersWithValidatedDownloads =
          nPeersWithValidatedDownloads -
          (state.nBlocksInFlightValidHeaders > 0);
      if (nNow > state.nDownloadingSince +
                     consensusParams.nPowTargetSpacing *
                         (BLOCK_DOWNLOAD_TIMEOUT_BASE +
                          BLOCK_DOWNLOAD_TIMEOUT_PER_PEER *
                              nOtherPeersWithValidatedDownloads)) {
        LogPrintf("Timeout downloading block %s from peer=%d, disconnecting\n",
                  queuedBlock.hash.ToString(), pto->GetId());
        pto->fDisconnect = true;
        return true;
      }
    }

    if (state.fSyncStarted &&
        state.nHeadersSyncTimeout < std::numeric_limits<int64_t>::max()) {
      if (pindexBestHeader->GetBlockTime() <=
          GetAdjustedTime() - 24 * 60 * 60) {
        if (nNow > state.nHeadersSyncTimeout && nSyncStarted == 1 &&
            (nPreferredDownload - state.fPreferredDownload >= 1)) {
          if (!pto->fWhitelisted) {
            LogPrintf(
                "Timeout downloading headers from peer=%d, disconnecting\n",
                pto->GetId());
            pto->fDisconnect = true;
            return true;
          } else {
            LogPrintf("Timeout downloading headers from whitelisted peer=%d, "
                      "not disconnecting\n",
                      pto->GetId());

            state.fSyncStarted = false;
            nSyncStarted--;
            state.nHeadersSyncTimeout = 0;
          }
        }
      } else {
        state.nHeadersSyncTimeout = std::numeric_limits<int64_t>::max();
      }
    }

    ConsiderEviction(pto, GetTime());

    std::vector<CInv> vGetData;
    if (!pto->fClient && (fFetch || !IsInitialBlockDownload()) &&
        state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
      std::vector<const CBlockIndex *> vToDownload;
      NodeId staller = -1;
      FindNextBlocksToDownload(
          pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight,
          vToDownload, staller, consensusParams);
      for (const CBlockIndex *pindex : vToDownload) {
        uint32_t nFetchFlags = GetFetchFlags(pto);
        vGetData.push_back(
            CInv(MSG_BLOCK | nFetchFlags, pindex->GetBlockHash()));
        MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockHash(), pindex);
        LogPrint(BCLog::NET, "Requesting block %s (%d) peer=%d\n",
                 pindex->GetBlockHash().ToString(), pindex->nHeight,
                 pto->GetId());
      }
      if (state.nBlocksInFlight == 0 && staller != -1) {
        if (State(staller)->nStallingSince == 0) {
          State(staller)->nStallingSince = nNow;
          LogPrint(BCLog::NET, "Stall started peer=%d\n", staller);
        }
      }
    }

    while (!pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow) {
      const CInv &inv = (*pto->mapAskFor.begin()).second;
      if (!AlreadyHave(inv)) {
        LogPrint(BCLog::NET, "Requesting %s peer=%d\n", inv.ToString(),
                 pto->GetId());
        vGetData.push_back(inv);
        if (vGetData.size() >= 1000) {
          connman->PushMessage(pto,
                               msgMaker.Make(NetMsgType::GETDATA, vGetData));
          vGetData.clear();
        }
      } else {
        pto->setAskFor.erase(inv.hash);
      }
      pto->mapAskFor.erase(pto->mapAskFor.begin());
    }
    if (!vGetData.empty())
      connman->PushMessage(pto, msgMaker.Make(NetMsgType::GETDATA, vGetData));

    if (pto->nVersion >= FEEFILTER_VERSION &&
        gArgs.GetBoolArg("-feefilter", DEFAULT_FEEFILTER) &&
        !(pto->fWhitelisted && gArgs.GetBoolArg("-whitelistforcerelay",
                                                DEFAULT_WHITELISTFORCERELAY))) {
      CAmount currentFilter =
          mempool
              .GetMinFee(gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) *
                         1000000)
              .GetFeePerK();
      int64_t timeNow = GetTimeMicros();
      if (timeNow > pto->nextSendTimeFeeFilter) {
        static CFeeRate default_feerate(DEFAULT_MIN_RELAY_TX_FEE);
        static FeeFilterRounder filterRounder(default_feerate);
        CAmount filterToSend = filterRounder.round(currentFilter);

        filterToSend = std::max(filterToSend, ::minRelayTxFee.GetFeePerK());
        if (filterToSend != pto->lastSentFeeFilter) {
          connman->PushMessage(
              pto, msgMaker.Make(NetMsgType::FEEFILTER, filterToSend));
          pto->lastSentFeeFilter = filterToSend;
        }
        pto->nextSendTimeFeeFilter =
            PoissonNextSend(timeNow, AVG_FEEFILTER_BROADCAST_INTERVAL);
      }

      else if (timeNow + MAX_FEEFILTER_CHANGE_DELAY * 1000000 <
                   pto->nextSendTimeFeeFilter &&
               (currentFilter < 3 * pto->lastSentFeeFilter / 4 ||
                currentFilter > 4 * pto->lastSentFeeFilter / 3)) {
        pto->nextSendTimeFeeFilter =
            timeNow + GetRandInt(MAX_FEEFILTER_CHANGE_DELAY) * 1000000;
      }
    }
  }
  return true;
}

class CNetProcessingCleanup {
public:
  CNetProcessingCleanup() {}
  ~CNetProcessingCleanup() {
    mapOrphanTransactions.clear();
    mapOrphanTransactionsByPrev.clear();
  }
} instance_of_cnetprocessingcleanup;