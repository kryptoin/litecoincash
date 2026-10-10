// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_ISMINE_H
#define BITCOIN_SCRIPT_ISMINE_H

#include <script/standard.h>

#include <stdint.h>

class CKeyStore;
class CScript;

enum isminetype {
  ISMINE_NO = 0,

  ISMINE_WATCH_UNSOLVABLE = 1,

  ISMINE_WATCH_SOLVABLE = 2,
  ISMINE_WATCH_ONLY = ISMINE_WATCH_SOLVABLE | ISMINE_WATCH_UNSOLVABLE,
  ISMINE_SPENDABLE = 4,
  ISMINE_ALL = ISMINE_WATCH_ONLY | ISMINE_SPENDABLE
};

typedef uint8_t isminefilter;

isminetype IsMine(const CKeyStore &keystore, const CScript &scriptPubKey,
                  bool &isInvalid, SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore &keystore, const CScript &scriptPubKey,
                  SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore &keystore, const CTxDestination &dest,
                  bool &isInvalid, SigVersion = SIGVERSION_BASE);
isminetype IsMine(const CKeyStore &keystore, const CTxDestination &dest,
                  SigVersion = SIGVERSION_BASE);

#endif
