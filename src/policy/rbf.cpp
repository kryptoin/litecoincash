// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/rbf.h>

bool SignalsOptInRBF(const CTransaction &tx) {
  for (const CTxIn &txin : tx.vin) {
    if (txin.nSequence < std::numeric_limits<unsigned int>::max() - 1) {
      return true;
    }
  }
  return false;
}

RBFTransactionState IsRBFOptIn(const CTransaction &tx, CTxMemPool &pool) {
  AssertLockHeld(pool.cs);

  CTxMemPool::setEntries setAncestors;

  if (SignalsOptInRBF(tx)) {
    return RBF_TRANSACTIONSTATE_REPLACEABLE_BIP125;
  }

  if (!pool.exists(tx.GetHash())) {
    return RBF_TRANSACTIONSTATE_UNKNOWN;
  }

  uint64_t noLimit = std::numeric_limits<uint64_t>::max();
  std::string dummy;
  CTxMemPoolEntry entry = *pool.mapTx.find(tx.GetHash());
  pool.CalculateMemPoolAncestors(entry, setAncestors, noLimit, noLimit, noLimit,
                                 noLimit, dummy);

  for (CTxMemPool::txiter it : setAncestors) {
    if (SignalsOptInRBF(it->GetTx())) {
      return RBF_TRANSACTIONSTATE_REPLACEABLE_BIP125;
    }
  }
  return RBF_TRANSACTIONSTATE_FINAL;
}