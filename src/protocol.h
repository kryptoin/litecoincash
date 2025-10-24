// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __cplusplus
#error This header can only be compiled as C++.
#endif

#ifndef BITCOIN_PROTOCOL_H
#define BITCOIN_PROTOCOL_H

#include <netaddress.h>
#include <serialize.h>
#include <uint256.h>
#include <version.h>

#include <stdint.h>
#include <string>

class CMessageHeader {
public:
  static constexpr size_t MESSAGE_START_SIZE = 4;
  static constexpr size_t COMMAND_SIZE = 12;
  static constexpr size_t MESSAGE_SIZE_SIZE = 4;
  static constexpr size_t CHECKSUM_SIZE = 4;
  static constexpr size_t MESSAGE_SIZE_OFFSET =
      MESSAGE_START_SIZE + COMMAND_SIZE;
  static constexpr size_t CHECKSUM_OFFSET =
      MESSAGE_SIZE_OFFSET + MESSAGE_SIZE_SIZE;
  static constexpr size_t HEADER_SIZE =
      MESSAGE_START_SIZE + COMMAND_SIZE + MESSAGE_SIZE_SIZE + CHECKSUM_SIZE;
  typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

  explicit CMessageHeader(const MessageStartChars &pchMessageStartIn);
  CMessageHeader(const MessageStartChars &pchMessageStartIn,
                 const char *pszCommand, unsigned int nMessageSizeIn);

  std::string GetCommand() const;
  bool IsValid(const MessageStartChars &messageStart) const;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(FLATDATA(pchMessageStart));
    READWRITE(FLATDATA(pchCommand));
    READWRITE(nMessageSize);
    READWRITE(FLATDATA(pchChecksum));
  }

  char pchMessageStart[MESSAGE_START_SIZE];
  char pchCommand[COMMAND_SIZE];
  uint32_t nMessageSize;
  uint8_t pchChecksum[CHECKSUM_SIZE];
};

namespace NetMsgType {
extern const char *VERSION;

extern const char *VERACK;

extern const char *ADDR;

extern const char *INV;

extern const char *GETDATA;

extern const char *MERKLEBLOCK;

extern const char *GETBLOCKS;

extern const char *GETHEADERS;

extern const char *TX;

extern const char *HEADERS;

extern const char *BLOCK;

extern const char *GETADDR;

extern const char *MEMPOOL;

extern const char *PING;

extern const char *PONG;

extern const char *NOTFOUND;

extern const char *FILTERLOAD;

extern const char *FILTERADD;

extern const char *FILTERCLEAR;

extern const char *REJECT;

extern const char *SENDHEADERS;

extern const char *FEEFILTER;

extern const char *SENDCMPCT;

extern const char *CMPCTBLOCK;

extern const char *GETBLOCKTXN;

extern const char *BLOCKTXN;

extern const char *RIALTO;
}; // namespace NetMsgType

const std::vector<std::string> &getAllNetMessageTypes();

enum ServiceFlags : uint64_t {
  NODE_NONE = 0,

  NODE_NETWORK = (1 << 0),

  NODE_GETUTXO = (1 << 1),

  NODE_BLOOM = (1 << 2),

  NODE_WITNESS = (1 << 3),

  NODE_XTHIN = (1 << 4),

  NODE_RIALTO = (1 << 5),

  NODE_NETWORK_LIMITED = (1 << 10),

};

static ServiceFlags GetDesirableServiceFlags(ServiceFlags services) {
  return ServiceFlags(NODE_NETWORK | NODE_WITNESS);
}

static inline bool HasAllDesirableServiceFlags(ServiceFlags services) {
  return !(GetDesirableServiceFlags(services) & (~services));
}

static inline bool MayHaveUsefulAddressDB(ServiceFlags services) {
  return services & NODE_NETWORK;
}

class CAddress : public CService {
public:
  CAddress();
  explicit CAddress(CService ipIn, ServiceFlags nServicesIn);

  void Init();

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    if (ser_action.ForRead())
      Init();
    int nVersion = s.GetVersion();
    if (s.GetType() & SER_DISK)
      READWRITE(nVersion);
    if ((s.GetType() & SER_DISK) ||
        (nVersion >= CADDR_TIME_VERSION && !(s.GetType() & SER_GETHASH)))
      READWRITE(nTime);
    uint64_t nServicesInt = nServices;
    READWRITE(nServicesInt);
    nServices = (ServiceFlags)nServicesInt;
    READWRITE(*(CService *)this);
  }

public:
  ServiceFlags nServices;

  unsigned int nTime;
};

const uint32_t MSG_WITNESS_FLAG = 1 << 30;
const uint32_t MSG_TYPE_MASK = 0xffffffff >> 2;

enum GetDataMsg {
  UNDEFINED = 0,
  MSG_TX = 1,
  MSG_BLOCK = 2,

  MSG_FILTERED_BLOCK = 3,

  MSG_CMPCT_BLOCK = 4,

  MSG_WITNESS_BLOCK = MSG_BLOCK | MSG_WITNESS_FLAG,

  MSG_WITNESS_TX = MSG_TX | MSG_WITNESS_FLAG,

  MSG_FILTERED_WITNESS_BLOCK = MSG_FILTERED_BLOCK | MSG_WITNESS_FLAG,

  MSG_RIALTO = 5,

};

class CInv {
public:
  CInv();
  CInv(int typeIn, const uint256 &hashIn);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(type);
    READWRITE(hash);
  }

  friend bool operator<(const CInv &a, const CInv &b);

  std::string GetCommand() const;
  std::string ToString() const;

public:
  int type;
  uint256 hash;
};

#endif
