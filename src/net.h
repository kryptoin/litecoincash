// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_H
#define BITCOIN_NET_H

#include <addrdb.h>
#include <addrman.h>
#include <amount.h>
#include <bloom.h>
#include <compat.h>
#include <hash.h>
#include <limitedmap.h>
#include <netaddress.h>
#include <policy/feerate.h>
#include <protocol.h>
#include <random.h>
#include <streams.h>
#include <sync.h>
#include <threadinterrupt.h>
#include <uint256.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <stdint.h>
#include <thread>

#ifndef WIN32
#include <arpa/inet.h>
#endif

class CScheduler;
class CNode;

namespace boost {
class thread_group;
}

static const int PING_INTERVAL = 2 * 60;

static const int TIMEOUT_INTERVAL = 20 * 60;

static const int FEELER_INTERVAL = 120;

static const unsigned int MAX_INV_SZ = 50000;

static const unsigned int MAX_ADDR_TO_SEND = 1000;

static const unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;

static const unsigned int MAX_SUBVERSION_LENGTH = 256;

static const int DEFAULT_MAX_OUTBOUND_CONNECTIONS = 8;

static const int MAX_ADDNODE_CONNECTIONS = 8;

static const bool DEFAULT_LISTEN = true;

#ifdef USE_UPNP
static const bool DEFAULT_UPNP = USE_UPNP;
#else
static const bool DEFAULT_UPNP = false;
#endif

static const size_t MAPASKFOR_MAX_SZ = MAX_INV_SZ;

static const size_t SETASKFOR_MAX_SZ = 2 * MAX_INV_SZ;

static const unsigned int DEFAULT_MAX_PEER_CONNECTIONS = 125;

static const uint64_t DEFAULT_MAX_UPLOAD_TARGET = 0;

static const uint64_t MAX_UPLOAD_TIMEFRAME = 60 * 60 * 24;

static const bool DEFAULT_BLOCKSONLY = false;

static const bool DEFAULT_FORCEDNSSEED = false;
static const size_t DEFAULT_MAXRECEIVEBUFFER = 5 * 1000;
static const size_t DEFAULT_MAXSENDBUFFER = 1 * 1000;

static const unsigned int DEFAULT_MISBEHAVING_BANTIME = 60 * 60 * 24;

typedef int64_t NodeId;

struct AddedNodeInfo {
  std::string strAddedNode;
  CService resolvedAddress;
  bool fConnected;
  bool fInbound;
};

class CNodeStats;
class CClientUIInterface;

struct CSerializedNetMsg {
  CSerializedNetMsg() = default;
  CSerializedNetMsg(CSerializedNetMsg &&) = default;
  CSerializedNetMsg &operator=(CSerializedNetMsg &&) = default;

  CSerializedNetMsg(const CSerializedNetMsg &msg) = delete;
  CSerializedNetMsg &operator=(const CSerializedNetMsg &) = delete;

  std::vector<unsigned char> data;
  std::string command;
};

class NetEventsInterface;
class CConnman {
public:
  enum NumConnections {
    CONNECTIONS_NONE = 0,
    CONNECTIONS_IN = (1U << 0),
    CONNECTIONS_OUT = (1U << 1),
    CONNECTIONS_ALL = (CONNECTIONS_IN | CONNECTIONS_OUT),
  };

  struct Options {
    ServiceFlags nLocalServices = NODE_NONE;
    int nMaxConnections = 0;
    int nMaxOutbound = 0;
    int nMaxAddnode = 0;
    int nMaxFeeler = 0;
    int nBestHeight = 0;
    CClientUIInterface *uiInterface = nullptr;
    NetEventsInterface *m_msgproc = nullptr;
    unsigned int nSendBufferMaxSize = 0;
    unsigned int nReceiveFloodSize = 0;
    uint64_t nMaxOutboundTimeframe = 0;
    uint64_t nMaxOutboundLimit = 0;
    std::vector<std::string> vSeedNodes;
    std::vector<CSubNet> vWhitelistedRange;
    std::vector<CService> vBinds, vWhiteBinds;
    bool m_use_addrman_outgoing = true;
    std::vector<std::string> m_specified_outgoing;
    std::vector<std::string> m_added_nodes;
  };

  void Init(const Options &connOptions) {
    nLocalServices = connOptions.nLocalServices;
    nMaxConnections = connOptions.nMaxConnections;
    nMaxOutbound =
        std::min(connOptions.nMaxOutbound, connOptions.nMaxConnections);
    nMaxAddnode = connOptions.nMaxAddnode;
    nMaxFeeler = connOptions.nMaxFeeler;
    nBestHeight = connOptions.nBestHeight;
    clientInterface = connOptions.uiInterface;
    m_msgproc = connOptions.m_msgproc;
    nSendBufferMaxSize = connOptions.nSendBufferMaxSize;
    nReceiveFloodSize = connOptions.nReceiveFloodSize;
    {
      LOCK(cs_totalBytesSent);
      nMaxOutboundTimeframe = connOptions.nMaxOutboundTimeframe;
      nMaxOutboundLimit = connOptions.nMaxOutboundLimit;
    }
    vWhitelistedRange = connOptions.vWhitelistedRange;
    {
      LOCK(cs_vAddedNodes);
      vAddedNodes = connOptions.m_added_nodes;
    }
  }

  CConnman(uint64_t seed0, uint64_t seed1);
  ~CConnman();
  bool Start(CScheduler &scheduler, const Options &options);
  void Stop();
  void Interrupt();
  bool GetNetworkActive() const { return fNetworkActive; };
  void SetNetworkActive(bool active);
  void OpenNetworkConnection(const CAddress &addrConnect, bool fCountFailure,
                             CSemaphoreGrant *grantOutbound = nullptr,
                             const char *strDest = nullptr,
                             bool fOneShot = false, bool fFeeler = false,
                             bool manual_connection = false);
  bool CheckIncomingNonce(uint64_t nonce);

  bool ForNode(NodeId id, std::function<bool(CNode *pnode)> func);

  void PushMessage(CNode *pnode, CSerializedNetMsg &&msg);

  template <typename Callable> void ForEachNode(Callable &&func) {
    LOCK(cs_vNodes);
    for (auto &&node : vNodes) {
      if (NodeFullyConnected(node))
        func(node);
    }
  };

  template <typename Callable> void ForEachNode(Callable &&func) const {
    LOCK(cs_vNodes);
    for (auto &&node : vNodes) {
      if (NodeFullyConnected(node))
        func(node);
    }
  };

  template <typename Callable, typename CallableAfter>
  void ForEachNodeThen(Callable &&pre, CallableAfter &&post) {
    LOCK(cs_vNodes);
    for (auto &&node : vNodes) {
      if (NodeFullyConnected(node))
        pre(node);
    }
    post();
  };

  template <typename Callable, typename CallableAfter>
  void ForEachNodeThen(Callable &&pre, CallableAfter &&post) const {
    LOCK(cs_vNodes);
    for (auto &&node : vNodes) {
      if (NodeFullyConnected(node))
        pre(node);
    }
    post();
  };

  size_t GetAddressCount() const;
  void SetServices(const CService &addr, ServiceFlags nServices);
  void MarkAddressGood(const CAddress &addr);
  void AddNewAddresses(const std::vector<CAddress> &vAddr,
                       const CAddress &addrFrom, int64_t nTimePenalty = 0);
  std::vector<CAddress> GetAddresses();

  void Ban(const CNetAddr &netAddr, const BanReason &reason,
           int64_t bantimeoffset = 0, bool sinceUnixEpoch = false);
  void Ban(const CSubNet &subNet, const BanReason &reason,
           int64_t bantimeoffset = 0, bool sinceUnixEpoch = false);
  void ClearBanned();

  bool IsBanned(CNetAddr ip);
  bool IsBanned(CSubNet subnet);
  bool Unban(const CNetAddr &ip);
  bool Unban(const CSubNet &ip);
  void GetBanned(banmap_t &banmap);
  void SetBanned(const banmap_t &banmap);

  void SetTryNewOutboundPeer(bool flag);
  bool GetTryNewOutboundPeer();

  int GetExtraOutboundCount();

  bool AddNode(const std::string &node);
  bool RemoveAddedNode(const std::string &node);
  std::vector<AddedNodeInfo> GetAddedNodeInfo();

  size_t GetNodeCount(NumConnections num);
  void GetNodeStats(std::vector<CNodeStats> &vstats);
  bool DisconnectNode(const std::string &node);
  bool DisconnectNode(NodeId id);

  ServiceFlags GetLocalServices() const;

  void SetMaxOutboundTarget(uint64_t limit);
  uint64_t GetMaxOutboundTarget();

  void SetMaxOutboundTimeframe(uint64_t timeframe);
  uint64_t GetMaxOutboundTimeframe();

  bool OutboundTargetReached(bool historicalBlockServingLimit);

  uint64_t GetOutboundTargetBytesLeft();

  uint64_t GetMaxOutboundTimeLeftInCycle();

  uint64_t GetTotalBytesRecv();
  uint64_t GetTotalBytesSent();

  void SetBestHeight(int height);
  int GetBestHeight() const;

  CSipHasher GetDeterministicRandomizer(uint64_t id) const;

  unsigned int GetReceiveFloodSize() const;

  void WakeMessageHandler();

private:
  struct ListenSocket {
    SOCKET socket;
    bool whitelisted;

    ListenSocket(SOCKET socket_, bool whitelisted_)
        : socket(socket_), whitelisted(whitelisted_) {}
  };

  bool BindListenPort(const CService &bindAddr, std::string &strError,
                      bool fWhitelisted = false);
  bool Bind(const CService &addr, unsigned int flags);
  bool InitBinds(const std::vector<CService> &binds,
                 const std::vector<CService> &whiteBinds);
  void ThreadOpenAddedConnections();
  void AddOneShot(const std::string &strDest);
  void ProcessOneShot();
  void ThreadOpenConnections(std::vector<std::string> connect);
  void ThreadMessageHandler();
  void AcceptConnection(const ListenSocket &hListenSocket);
  void ThreadSocketHandler();
  void ThreadDNSAddressSeed();

  uint64_t CalculateKeyedNetGroup(const CAddress &ad) const;

  CNode *FindNode(const CNetAddr &ip);
  CNode *FindNode(const CSubNet &subNet);
  CNode *FindNode(const std::string &addrName);
  CNode *FindNode(const CService &addr);

  bool AttemptToEvictConnection();
  CNode *ConnectNode(CAddress addrConnect, const char *pszDest,
                     bool fCountFailure);
  bool IsWhitelistedRange(const CNetAddr &addr);

  void DeleteNode(CNode *pnode);

  NodeId GetNewNodeId();

  size_t SocketSendData(CNode *pnode) const;

  bool BannedSetIsDirty();

  void SetBannedSetDirty(bool dirty = true);

  void SweepBanned();
  void DumpAddresses();
  void DumpData();
  void DumpBanlist();

  void RecordBytesRecv(uint64_t bytes);
  void RecordBytesSent(uint64_t bytes);

  static bool NodeFullyConnected(const CNode *pnode);

  CCriticalSection cs_totalBytesRecv;
  CCriticalSection cs_totalBytesSent;
  uint64_t nTotalBytesRecv GUARDED_BY(cs_totalBytesRecv);
  uint64_t nTotalBytesSent GUARDED_BY(cs_totalBytesSent);

  uint64_t nMaxOutboundTotalBytesSentInCycle GUARDED_BY(cs_totalBytesSent);
  uint64_t nMaxOutboundCycleStartTime GUARDED_BY(cs_totalBytesSent);
  uint64_t nMaxOutboundLimit GUARDED_BY(cs_totalBytesSent);
  uint64_t nMaxOutboundTimeframe GUARDED_BY(cs_totalBytesSent);

  std::vector<CSubNet> vWhitelistedRange;

  unsigned int nSendBufferMaxSize;
  unsigned int nReceiveFloodSize;

  std::vector<ListenSocket> vhListenSocket;
  std::atomic<bool> fNetworkActive;
  banmap_t setBanned;
  CCriticalSection cs_setBanned;
  bool setBannedIsDirty;
  bool fAddressesInitialized;
  CAddrMan addrman;
  std::deque<std::string> vOneShots;
  CCriticalSection cs_vOneShots;
  std::vector<std::string> vAddedNodes GUARDED_BY(cs_vAddedNodes);
  CCriticalSection cs_vAddedNodes;
  std::vector<CNode *> vNodes;
  std::list<CNode *> vNodesDisconnected;
  mutable CCriticalSection cs_vNodes;
  std::atomic<NodeId> nLastNodeId;

  ServiceFlags nLocalServices;

  std::unique_ptr<CSemaphore> semOutbound;
  std::unique_ptr<CSemaphore> semAddnode;
  int nMaxConnections;
  int nMaxOutbound;
  int nMaxAddnode;
  int nMaxFeeler;
  std::atomic<int> nBestHeight;
  CClientUIInterface *clientInterface;
  NetEventsInterface *m_msgproc;

  const uint64_t nSeed0, nSeed1;

  bool fMsgProcWake;

  std::condition_variable condMsgProc;
  std::mutex mutexMsgProc;
  std::atomic<bool> flagInterruptMsgProc;

  CThreadInterrupt interruptNet;

  std::thread threadDNSAddressSeed;
  std::thread threadSocketHandler;
  std::thread threadOpenAddedConnections;
  std::thread threadOpenConnections;
  std::thread threadMessageHandler;

  std::atomic_bool m_try_another_outbound_peer;

  friend struct CConnmanTest;
};
extern std::unique_ptr<CConnman> g_connman;
void Discover(boost::thread_group &threadGroup);
void MapPort(bool fUseUPnP);
unsigned short GetListenPort();
bool BindListenPort(const CService &bindAddr, std::string &strError,
                    bool fWhitelisted = false);

struct CombinerAll {
  typedef bool result_type;

  template <typename I> bool operator()(I first, I last) const {
    while (first != last) {
      if (!(*first))
        return false;
      ++first;
    }
    return true;
  }
};

class NetEventsInterface {
public:
  virtual bool ProcessMessages(CNode *pnode, std::atomic<bool> &interrupt) = 0;
  virtual bool SendMessages(CNode *pnode, std::atomic<bool> &interrupt) = 0;
  virtual void InitializeNode(CNode *pnode) = 0;
  virtual void FinalizeNode(NodeId id, bool &update_connection_time) = 0;
};

enum {
  LOCAL_NONE,

  LOCAL_IF,

  LOCAL_BIND,

  LOCAL_UPNP,

  LOCAL_MANUAL,

  LOCAL_MAX
};

bool IsPeerAddrLocalGood(CNode *pnode);
void AdvertiseLocal(CNode *pnode);
void SetLimited(enum Network net, bool fLimited = true);
bool IsLimited(enum Network net);
bool IsLimited(const CNetAddr &addr);
bool AddLocal(const CService &addr, int nScore = LOCAL_NONE);
bool AddLocal(const CNetAddr &addr, int nScore = LOCAL_NONE);
bool RemoveLocal(const CService &addr);
bool SeenLocal(const CService &addr);
bool IsLocal(const CService &addr);
bool GetLocal(CService &addr, const CNetAddr *paddrPeer = nullptr);
bool IsReachable(enum Network net);
bool IsReachable(const CNetAddr &addr);
CAddress GetLocalAddress(const CNetAddr *paddrPeer,
                         ServiceFlags nLocalServices);

extern bool fDiscover;
extern bool fListen;
extern bool fRelayTxes;

extern limitedmap<uint256, int64_t> mapAlreadyAskedFor;

extern std::string strSubVersion;

struct LocalServiceInfo {
  int nScore;
  int nPort;
};

extern CCriticalSection cs_mapLocalHost;
extern std::map<CNetAddr, LocalServiceInfo> mapLocalHost;
typedef std::map<std::string, uint64_t> mapMsgCmdSize;

class CNodeStats {
public:
  bool fInbound;
  bool fRelayTxes;
  bool fWhitelisted;
  bool m_manual_connection;

  double dMinPing;
  double dPingTime;
  double dPingWait;

  int nStartingHeight;
  int nVersion;

  int64_t nLastRecv;
  int64_t nLastSend;
  int64_t nTimeConnected;
  int64_t nTimeOffset;

  mapMsgCmdSize mapRecvBytesPerMsgCmd;
  mapMsgCmdSize mapSendBytesPerMsgCmd;

  NodeId nodeid;
  ServiceFlags nServices;

  std::string addrLocal;
  std::string addrName;
  std::string cleanSubVer;

  uint64_t nRecvBytes;
  uint64_t nSendBytes;

  CAddress addr;
  CAddress addrBind;
};

class CNetMessage {
private:
  mutable CHash256 hasher;
  mutable uint256 data_hash;

public:
  bool in_data;

  CDataStream hdrbuf;
  CDataStream vRecv;
  CMessageHeader hdr;

  int64_t nTime;

  unsigned int nDataPos;
  unsigned int nHdrPos;

  CNetMessage(const CMessageHeader::MessageStartChars &pchMessageStartIn,
              int nTypeIn, int nVersionIn)
      : hdrbuf(nTypeIn, nVersionIn), hdr(pchMessageStartIn),
        vRecv(nTypeIn, nVersionIn) {
    hdrbuf.resize(24);
    in_data = false;
    nHdrPos = 0;
    nDataPos = 0;
    nTime = 0;
  }

  bool complete() const {
    if (!in_data)
      return false;
    return (hdr.nMessageSize == nDataPos);
  }

  const uint256 &GetMessageHash() const;

  void SetVersion(int nVersionIn) {
    hdrbuf.SetVersion(nVersionIn);
    vRecv.SetVersion(nVersionIn);
  }

  int readHeader(const char *pch, unsigned int nBytes);
  int readData(const char *pch, unsigned int nBytes);
};

class CNode {
  friend class CConnman;

public:
  bool fClient;
  bool fFeeler;
  bool fOneShot;
  bool fRelayTxes;
  bool fSentAddr;
  bool fWhitelisted;
  bool m_manual_connection;

  CCriticalSection cs_filter;
  CCriticalSection cs_hSocket;
  CCriticalSection cs_sendProcessing;
  CCriticalSection cs_SubVer;
  CCriticalSection cs_vProcessMsg;
  CCriticalSection cs_vRecv;
  CCriticalSection cs_vSend;

  const bool fInbound;
  const CAddress addr;
  const CAddress addrBind;
  const int64_t nTimeConnected;
  const uint64_t nKeyedNetGroup;

  CSemaphoreGrant grantOutbound;

  size_t nProcessQueueSize;
  size_t nSendOffset;
  size_t nSendSize;

  SOCKET hSocket;

  std::atomic<int64_t> nLastRecv;
  std::atomic<int64_t> nLastSend;
  std::atomic<int64_t> nTimeOffset;
  std::atomic<int> nRecvVersion;
  std::atomic<int> nRefCount;
  std::atomic<int> nVersion;
  std::atomic<ServiceFlags> nServices;
  std::atomic_bool fDisconnect;
  std::atomic_bool fPauseRecv;
  std::atomic_bool fPauseSend;
  std::atomic_bool fSuccessfullyConnected;
  std::deque<CInv> vRecvGetData;
  std::deque<std::vector<unsigned char>> vSendMsg;
  std::list<CNetMessage> vProcessMsg;
  std::string strSubVer, cleanSubVer;
  std::unique_ptr<CBloomFilter> pfilter;

  uint64_t nRecvBytes;
  uint64_t nSendBytes;

protected:
  mapMsgCmdSize mapSendBytesPerMsgCmd;
  mapMsgCmdSize mapRecvBytesPerMsgCmd;

public:
  bool fGetAddr;
  bool fSendMempool;

  CAmount lastSentFeeFilter;
  CAmount minFeeFilter;
  CCriticalSection cs_feeFilter;
  CCriticalSection cs_inventory;
  CRollingBloomFilter addrKnown;
  CRollingBloomFilter filterInventoryKnown;
  CRollingBloomFilter filterInventoryKnownRialto;

  int64_t nextSendTimeFeeFilter;
  int64_t nNextAddrSend;
  int64_t nNextInvSend;
  int64_t nNextLocalAddrSend;

  std::atomic<bool> fPingQueued;

  std::atomic<int64_t> nLastBlockTime;
  std::atomic<int64_t> nLastTXTime;
  std::atomic<int64_t> nMinPingUsecTime;
  std::atomic<int64_t> nPingUsecStart;
  std::atomic<int64_t> nPingUsecTime;
  std::atomic<int64_t> timeLastMempoolReq;

  std::atomic<int> nStartingHeight;
  std::atomic<uint64_t> nPingNonceSent;
  std::multimap<int64_t, CInv> mapAskFor;

  std::set<uint256> rialtoInventoryToSend;
  std::set<uint256> setAskFor;
  std::set<uint256> setInventoryTxToSend;
  std::set<uint256> setKnown;

  std::vector<CAddress> vAddrToSend;
  std::vector<uint256> vBlockHashesToAnnounce;
  std::vector<uint256> vInventoryBlockToSend;

  uint256 hashContinue;

  CNode(NodeId id, ServiceFlags nLocalServicesIn, int nMyStartingHeightIn,
        SOCKET hSocketIn, const CAddress &addrIn, uint64_t nKeyedNetGroupIn,
        uint64_t nLocalHostNonceIn, const CAddress &addrBindIn,
        const std::string &addrNameIn = "", bool fInboundIn = false);
  ~CNode();
  CNode(const CNode &) = delete;
  CNode &operator=(const CNode &) = delete;

private:
  const int nMyStartingHeight;
  const NodeId id;
  const ServiceFlags nLocalServices;
  const uint64_t nLocalHostNonce;

  CService addrLocal;
  int nSendVersion;

  mutable CCriticalSection cs_addrLocal;
  mutable CCriticalSection cs_addrName;

  std::list<CNetMessage> vRecvMsg;
  std::string addrName;

public:
  NodeId GetId() const { return id; }

  uint64_t GetLocalNonce() const { return nLocalHostNonce; }

  int GetMyStartingHeight() const { return nMyStartingHeight; }

  int GetRefCount() const {
    assert(nRefCount >= 0);
    return nRefCount;
  }

  bool ReceiveMsgBytes(const char *pch, unsigned int nBytes, bool &complete);
  CService GetAddrLocal() const;

  int GetRecvVersion() const { return nRecvVersion; }
  int GetSendVersion() const;

  void SetAddrLocal(const CService &addrLocalIn);
  void SetRecvVersion(int nVersionIn) { nRecvVersion = nVersionIn; }
  void SetSendVersion(int nVersionIn);

  CNode *AddRef() {
    nRefCount++;
    return this;
  }

  void Release() { nRefCount--; }

  void AddAddressKnown(const CAddress &_addr) {
    addrKnown.insert(_addr.GetKey());
  }

  void PushAddress(const CAddress &_addr, FastRandomContext &insecure_rand) {
    if (_addr.IsValid() && !addrKnown.contains(_addr.GetKey())) {
      if (vAddrToSend.size() >= MAX_ADDR_TO_SEND) {
        vAddrToSend[insecure_rand.randrange(vAddrToSend.size())] = _addr;
      } else {
        vAddrToSend.push_back(_addr);
      }
    }
  }

  void AddInventoryKnown(const CInv &inv) {
    {
      LOCK(cs_inventory);
      filterInventoryKnown.insert(inv.hash);
    }
  }

  void PushInventory(const CInv &inv) {
    LOCK(cs_inventory);
    if (inv.type == MSG_TX) {
      if (!filterInventoryKnown.contains(inv.hash)) {
        setInventoryTxToSend.insert(inv.hash);
      }

    } else if (inv.type == MSG_RIALTO) {
      if (!filterInventoryKnownRialto.contains(inv.hash)) {
        rialtoInventoryToSend.insert(inv.hash);
      }
    } else if (inv.type == MSG_BLOCK) {
      vInventoryBlockToSend.push_back(inv.hash);
    }
  }

  void PushBlockHash(const uint256 &hash) {
    LOCK(cs_inventory);
    vBlockHashesToAnnounce.push_back(hash);
  }

  void AskFor(const CInv &inv);

  void CloseSocketDisconnect();

  void copyStats(CNodeStats &stats);

  ServiceFlags GetLocalServices() const { return nLocalServices; }

  std::string GetAddrName() const;

  void MaybeSetAddrName(const std::string &addrNameIn);
};

int64_t PoissonNextSend(int64_t nNow, int average_interval_seconds);

#endif
