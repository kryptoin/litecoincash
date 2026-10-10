// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SIGN_H
#define BITCOIN_SCRIPT_SIGN_H

#include <script/interpreter.h>

class CKeyID;
class CKeyStore;
class CScript;
class CTransaction;

struct CMutableTransaction;

class BaseSignatureCreator {
protected:
  const CKeyStore *keystore;

public:
  explicit BaseSignatureCreator(const CKeyStore *keystoreIn)
      : keystore(keystoreIn) {}
  const CKeyStore &KeyStore() const { return *keystore; };
  virtual ~BaseSignatureCreator() {}
  virtual const BaseSignatureChecker &Checker() const = 0;

  virtual bool CreateSig(std::vector<unsigned char> &vchSig,
                         const CKeyID &keyid, const CScript &scriptCode,
                         SigVersion sigversion) const = 0;
};

class TransactionSignatureCreator : public BaseSignatureCreator {
  const CTransaction *txTo;
  unsigned int nIn;
  int nHashType;
  CAmount amount;
  const TransactionSignatureChecker checker;

public:
  TransactionSignatureCreator(const CKeyStore *keystoreIn,
                              const CTransaction *txToIn, unsigned int nInIn,
                              const CAmount &amountIn,
                              int nHashTypeIn = SIGHASH_ALL | SIGHASH_FORKID);

  const BaseSignatureChecker &Checker() const override { return checker; }
  bool CreateSig(std::vector<unsigned char> &vchSig, const CKeyID &keyid,
                 const CScript &scriptCode,
                 SigVersion sigversion) const override;
};

class MutableTransactionSignatureCreator : public TransactionSignatureCreator {
  CTransaction tx;

public:
  MutableTransactionSignatureCreator(const CKeyStore *keystoreIn,
                                     const CMutableTransaction *txToIn,
                                     unsigned int nInIn,
                                     const CAmount &amountIn, int nHashTypeIn)
      : TransactionSignatureCreator(keystoreIn, &tx, nInIn, amountIn,
                                    nHashTypeIn),
        tx(*txToIn) {}
};

class DummySignatureCreator : public BaseSignatureCreator {
public:
  explicit DummySignatureCreator(const CKeyStore *keystoreIn)
      : BaseSignatureCreator(keystoreIn) {}
  const BaseSignatureChecker &Checker() const override;
  bool CreateSig(std::vector<unsigned char> &vchSig, const CKeyID &keyid,
                 const CScript &scriptCode,
                 SigVersion sigversion) const override;
};

struct SignatureData {
  CScript scriptSig;
  CScriptWitness scriptWitness;

  SignatureData() {}
  explicit SignatureData(const CScript &script) : scriptSig(script) {}
};

bool ProduceSignature(const BaseSignatureCreator &creator,
                      const CScript &scriptPubKey, SignatureData &sigdata);

bool SignSignature(const CKeyStore &keystore, const CScript &fromPubKey,
                   CMutableTransaction &txTo, unsigned int nIn,
                   const CAmount &amount, int nHashType);
bool SignSignature(const CKeyStore &keystore, const CTransaction &txFrom,
                   CMutableTransaction &txTo, unsigned int nIn, int nHashType);

SignatureData CombineSignatures(const CScript &scriptPubKey,
                                const BaseSignatureChecker &checker,
                                const SignatureData &scriptSig1,
                                const SignatureData &scriptSig2);

SignatureData DataFromTransaction(const CMutableTransaction &tx,
                                  unsigned int nIn);
void UpdateTransaction(CMutableTransaction &tx, unsigned int nIn,
                       const SignatureData &data);

bool IsSolvable(const CKeyStore &store, const CScript &script);

#endif
