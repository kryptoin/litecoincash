// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_FEERATE_H
#define BITCOIN_POLICY_FEERATE_H

#include <amount.h>
#include <serialize.h>

#include <string>

extern const std::string CURRENCY_UNIT;

class CFeeRate {
private:
  CAmount nSatoshisPerK;

public:
  CFeeRate() : nSatoshisPerK(0) {}
  template <typename I>
  CFeeRate(const I _nSatoshisPerK) : nSatoshisPerK(_nSatoshisPerK) {
    static_assert(std::is_integral<I>::value,
                  "CFeeRate should be used without floats");
  }

  CFeeRate(const CAmount &nFeePaid, size_t nBytes);

  CAmount GetFee(size_t nBytes) const;

  CAmount GetFeePerK() const { return GetFee(1000); }
  friend bool operator<(const CFeeRate &a, const CFeeRate &b) {
    return a.nSatoshisPerK < b.nSatoshisPerK;
  }
  friend bool operator>(const CFeeRate &a, const CFeeRate &b) {
    return a.nSatoshisPerK > b.nSatoshisPerK;
  }
  friend bool operator==(const CFeeRate &a, const CFeeRate &b) {
    return a.nSatoshisPerK == b.nSatoshisPerK;
  }
  friend bool operator<=(const CFeeRate &a, const CFeeRate &b) {
    return a.nSatoshisPerK <= b.nSatoshisPerK;
  }
  friend bool operator>=(const CFeeRate &a, const CFeeRate &b) {
    return a.nSatoshisPerK >= b.nSatoshisPerK;
  }
  friend bool operator!=(const CFeeRate &a, const CFeeRate &b) {
    return a.nSatoshisPerK != b.nSatoshisPerK;
  }
  CFeeRate &operator+=(const CFeeRate &a) {
    nSatoshisPerK += a.nSatoshisPerK;
    return *this;
  }
  std::string ToString() const;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(nSatoshisPerK);
  }
};

#endif
