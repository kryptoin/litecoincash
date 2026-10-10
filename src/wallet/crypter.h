// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_CRYPTER_H
#define BITCOIN_WALLET_CRYPTER_H

#include <keystore.h>
#include <serialize.h>
#include <support/allocators/secure.h>

#include <atomic>

const unsigned int WALLET_CRYPTO_KEY_SIZE = 32;
const unsigned int WALLET_CRYPTO_SALT_SIZE = 8;
const unsigned int WALLET_CRYPTO_IV_SIZE = 16;

class CMasterKey {
public:
  std::vector<unsigned char> vchCryptedKey;
  std::vector<unsigned char> vchSalt;

  unsigned int nDerivationMethod;
  unsigned int nDeriveIterations;

  std::vector<unsigned char> vchOtherDerivationParameters;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  inline void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(vchCryptedKey);
    READWRITE(vchSalt);
    READWRITE(nDerivationMethod);
    READWRITE(nDeriveIterations);
    READWRITE(vchOtherDerivationParameters);
  }

  CMasterKey() {
    nDeriveIterations = 25000;
    nDerivationMethod = 0;
    vchOtherDerivationParameters = std::vector<unsigned char>(0);
  }
};

typedef std::vector<unsigned char, secure_allocator<unsigned char>>
    CKeyingMaterial;

namespace wallet_crypto {
class TestCrypter;
}

class CCrypter {
  friend class wallet_crypto::TestCrypter;

private:
  std::vector<unsigned char, secure_allocator<unsigned char>> vchKey;
  std::vector<unsigned char, secure_allocator<unsigned char>> vchIV;
  bool fKeySet;

  int BytesToKeySHA512AES(const std::vector<unsigned char> &chSalt,
                          const SecureString &strKeyData, int count,
                          unsigned char *key, unsigned char *iv) const;

public:
  bool SetKeyFromPassphrase(const SecureString &strKeyData,
                            const std::vector<unsigned char> &chSalt,
                            const unsigned int nRounds,
                            const unsigned int nDerivationMethod);
  bool Encrypt(const CKeyingMaterial &vchPlaintext,
               std::vector<unsigned char> &vchCiphertext) const;
  bool Decrypt(const std::vector<unsigned char> &vchCiphertext,
               CKeyingMaterial &vchPlaintext) const;
  bool SetKey(const CKeyingMaterial &chNewKey,
              const std::vector<unsigned char> &chNewIV);

  void CleanKey() {
    memory_cleanse(vchKey.data(), vchKey.size());
    memory_cleanse(vchIV.data(), vchIV.size());
    fKeySet = false;
  }

  CCrypter() {
    fKeySet = false;
    vchKey.resize(WALLET_CRYPTO_KEY_SIZE);
    vchIV.resize(WALLET_CRYPTO_IV_SIZE);
  }

  ~CCrypter() { CleanKey(); }
};

class CCryptoKeyStore : public CBasicKeyStore {
private:
  CKeyingMaterial vMasterKey;

  std::atomic<bool> fUseCrypto;

  bool fDecryptionThoroughlyChecked;

protected:
  bool SetCrypted();

  bool EncryptKeys(CKeyingMaterial &vMasterKeyIn);

  bool Unlock(const CKeyingMaterial &vMasterKeyIn);
  CryptedKeyMap mapCryptedKeys;

public:
  CCryptoKeyStore() : fUseCrypto(false), fDecryptionThoroughlyChecked(false) {}

  bool IsCrypted() const { return fUseCrypto; }
  bool IsLocked() const;
  bool Lock();

  virtual bool
  AddCryptedKey(const CPubKey &vchPubKey,
                const std::vector<unsigned char> &vchCryptedSecret);
  bool AddKeyPubKey(const CKey &key, const CPubKey &pubkey) override;
  bool HaveKey(const CKeyID &address) const override;
  bool GetKey(const CKeyID &address, CKey &keyOut) const override;
  bool GetPubKey(const CKeyID &address, CPubKey &vchPubKeyOut) const override;
  std::set<CKeyID> GetKeys() const override;

  boost::signals2::signal<void(CCryptoKeyStore *wallet)> NotifyStatusChanged;
};

#endif
