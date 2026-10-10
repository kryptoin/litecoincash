// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KEY_H
#define BITCOIN_KEY_H

#include <pubkey.h>
#include <serialize.h>
#include <support/allocators/secure.h>
#include <uint256.h>

#include <stdexcept>
#include <vector>

typedef std::vector<unsigned char, secure_allocator<unsigned char>> CPrivKey;

class CKey {
public:
  static const unsigned int PRIVATE_KEY_SIZE = 279;
  static const unsigned int COMPRESSED_PRIVATE_KEY_SIZE = 214;

  static_assert(PRIVATE_KEY_SIZE >= COMPRESSED_PRIVATE_KEY_SIZE,
                "COMPRESSED_PRIVATE_KEY_SIZE is larger than PRIVATE_KEY_SIZE");

private:
  bool fValid;

  bool fCompressed;

  std::vector<unsigned char, secure_allocator<unsigned char>> keydata;

  bool static Check(const unsigned char *vch);

public:
  CKey() : fValid(false), fCompressed(false) { keydata.resize(32); }

  friend bool operator==(const CKey &a, const CKey &b) {
    return a.fCompressed == b.fCompressed && a.size() == b.size() &&
           memcmp(a.keydata.data(), b.keydata.data(), a.size()) == 0;
  }

  template <typename T>
  void Set(const T pbegin, const T pend, bool fCompressedIn) {
    if (size_t(pend - pbegin) != keydata.size()) {
      fValid = false;
    } else if (Check(&pbegin[0])) {
      memcpy(keydata.data(), (unsigned char *)&pbegin[0], keydata.size());
      fValid = true;
      fCompressed = fCompressedIn;
    } else {
      fValid = false;
    }
  }

  unsigned int size() const { return (fValid ? keydata.size() : 0); }
  const unsigned char *begin() const { return keydata.data(); }
  const unsigned char *end() const { return keydata.data() + size(); }

  bool IsValid() const { return fValid; }

  bool IsCompressed() const { return fCompressed; }

  void MakeNewKey(bool fCompressed);

  CPrivKey GetPrivKey() const;

  CPubKey GetPubKey() const;

  bool Sign(const uint256 &hash, std::vector<unsigned char> &vchSig,
            uint32_t test_case = 0) const;

  bool SignCompact(const uint256 &hash,
                   std::vector<unsigned char> &vchSig) const;

  bool Derive(CKey &keyChild, ChainCode &ccChild, unsigned int nChild,
              const ChainCode &cc) const;

  bool VerifyPubKey(const CPubKey &vchPubKey) const;

  bool Load(const CPrivKey &privkey, const CPubKey &vchPubKey, bool fSkipCheck);
};

struct CExtKey {
  unsigned char nDepth;
  unsigned char vchFingerprint[4];
  unsigned int nChild;
  ChainCode chaincode;
  CKey key;

  friend bool operator==(const CExtKey &a, const CExtKey &b) {
    return a.nDepth == b.nDepth &&
           memcmp(&a.vchFingerprint[0], &b.vchFingerprint[0],
                  sizeof(vchFingerprint)) == 0 &&
           a.nChild == b.nChild && a.chaincode == b.chaincode && a.key == b.key;
  }

  void Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const;
  void Decode(const unsigned char code[BIP32_EXTKEY_SIZE]);
  bool Derive(CExtKey &out, unsigned int nChild) const;
  CExtPubKey Neuter() const;
  void SetMaster(const unsigned char *seed, unsigned int nSeedLen);
  template <typename Stream> void Serialize(Stream &s) const {
    unsigned int len = BIP32_EXTKEY_SIZE;
    ::WriteCompactSize(s, len);
    unsigned char code[BIP32_EXTKEY_SIZE];
    Encode(code);
    s.write((const char *)&code[0], len);
  }
  template <typename Stream> void Unserialize(Stream &s) {
    unsigned int len = ::ReadCompactSize(s);
    unsigned char code[BIP32_EXTKEY_SIZE];
    if (len != BIP32_EXTKEY_SIZE)
      throw std::runtime_error("Invalid extended key size\n");
    s.read((char *)&code[0], len);
    Decode(code);
  }
};

void ECC_Start(void);

void ECC_Stop(void);

bool ECC_InitSanityCheck(void);

#endif
