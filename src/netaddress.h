// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NETADDRESS_H
#define BITCOIN_NETADDRESS_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <compat.h>
#include <serialize.h>

#include <stdint.h>
#include <string>
#include <vector>

enum Network {
  NET_UNROUTABLE = 0,
  NET_IPV4,
  NET_IPV6,
  NET_TOR,
  NET_INTERNAL,

  NET_MAX,
};

class CNetAddr {
protected:
  unsigned char ip[16];

  uint32_t scopeId;

public:
  CNetAddr();
  explicit CNetAddr(const struct in_addr &ipv4Addr);
  void Init();
  void SetIP(const CNetAddr &ip);

  void SetRaw(Network network, const uint8_t *data);

  bool SetInternal(const std::string &name);

  bool SetSpecial(const std::string &strName);

  bool IsIPv4() const;

  bool IsIPv6() const;

  bool IsRFC1918() const;

  bool IsRFC2544() const;

  bool IsRFC6598() const;

  bool IsRFC5737() const;

  bool IsRFC3849() const;

  bool IsRFC3927() const;

  bool IsRFC3964() const;

  bool IsRFC4193() const;

  bool IsRFC4380() const;

  bool IsRFC4843() const;

  bool IsRFC4862() const;

  bool IsRFC6052() const;

  bool IsRFC6145() const;

  bool IsTor() const;
  bool IsLocal() const;
  bool IsRoutable() const;
  bool IsInternal() const;
  bool IsValid() const;
  enum Network GetNetwork() const;
  std::string ToString() const;
  std::string ToStringIP() const;
  unsigned int GetByte(int n) const;
  uint64_t GetHash() const;
  bool GetInAddr(struct in_addr *pipv4Addr) const;
  std::vector<unsigned char> GetGroup() const;
  int GetReachabilityFrom(const CNetAddr *paddrPartner = nullptr) const;

  explicit CNetAddr(const struct in6_addr &pipv6Addr, const uint32_t scope = 0);
  bool GetIn6Addr(struct in6_addr *pipv6Addr) const;

  friend bool operator==(const CNetAddr &a, const CNetAddr &b);
  friend bool operator!=(const CNetAddr &a, const CNetAddr &b);
  friend bool operator<(const CNetAddr &a, const CNetAddr &b);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(FLATDATA(ip));
  }

  friend class CSubNet;
};

class CSubNet {
protected:
  CNetAddr network;

  uint8_t netmask[16];

  bool valid;

public:
  CSubNet();
  CSubNet(const CNetAddr &addr, int32_t mask);
  CSubNet(const CNetAddr &addr, const CNetAddr &mask);

  explicit CSubNet(const CNetAddr &addr);

  bool Match(const CNetAddr &addr) const;

  std::string ToString() const;
  bool IsValid() const;

  friend bool operator==(const CSubNet &a, const CSubNet &b);
  friend bool operator!=(const CSubNet &a, const CSubNet &b);
  friend bool operator<(const CSubNet &a, const CSubNet &b);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(network);
    READWRITE(FLATDATA(netmask));
    READWRITE(FLATDATA(valid));
  }
};

class CService : public CNetAddr {
protected:
  unsigned short port;

public:
  CService();
  CService(const CNetAddr &ip, unsigned short port);
  CService(const struct in_addr &ipv4Addr, unsigned short port);
  explicit CService(const struct sockaddr_in &addr);
  void Init();
  unsigned short GetPort() const;
  bool GetSockAddr(struct sockaddr *paddr, socklen_t *addrlen) const;
  bool SetSockAddr(const struct sockaddr *paddr);
  friend bool operator==(const CService &a, const CService &b);
  friend bool operator!=(const CService &a, const CService &b);
  friend bool operator<(const CService &a, const CService &b);
  std::vector<unsigned char> GetKey() const;
  std::string ToString() const;
  std::string ToStringPort() const;
  std::string ToStringIPPort() const;

  CService(const struct in6_addr &ipv6Addr, unsigned short port);
  explicit CService(const struct sockaddr_in6 &addr);

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(FLATDATA(ip));
    unsigned short portN = htons(port);
    READWRITE(FLATDATA(portN));
    if (ser_action.ForRead())
      port = ntohs(portN);
  }
};

#endif
