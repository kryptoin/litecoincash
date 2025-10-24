// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <chainparams.h>
#include <crypto/common.h>
#include <crypto/scrypt.h>
#include <hash.h>
#include <tinyformat.h>
#include <utilstrencodings.h>

#include <crypto/minotaurx/minotaur.h>

#include <validation.h>

#include <util.h>

uint256 CBlockHeader::GetHash() const { return SerializeHash(*this); }

uint256 CBlockHeader::MinotaurHashArbitrary(const char *data) {
  return Minotaur(data, data + strlen(data), false);
}

uint256 CBlockHeader::MinotaurHashString(std::string data) {
  return Minotaur(data.begin(), data.end(), false);
}

uint256 CBlockHeader::GetPoWHash() const {
  if (nTime > Params().GetConsensus().powForkTime) {
    if (nVersion >= 0x20000000)

      return GetHash();

    switch (GetPoWType()) {
    case POW_TYPE_SHA256:
      return GetHash();
      break;
    case POW_TYPE_MINOTAURX:
      return Minotaur(BEGIN(nVersion), END(nNonce), true);
      break;
    default:

      return HIGH_HASH;
    }
  }

  uint256 thash;
  scrypt_1024_1_1_256(BEGIN(nVersion), BEGIN(thash));
  return thash;
}

std::string CBlock::ToString() const {
  std::stringstream s;

  bool isHive = IsHiveMined(Params().GetConsensus());
  s << strprintf("CBlock(type=%s, hash=%s, powHash=%s, powType=%s, ver=0x%08x, "
                 "hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, "
                 "nNonce=%u, vtx=%u)\n",
                 isHive ? "hive" : "pow", GetHash().ToString(),
                 GetPoWHash().ToString(), isHive ? "n/a" : GetPoWTypeName(),

                 nVersion, hashPrevBlock.ToString(), hashMerkleRoot.ToString(),
                 nTime, nBits, nNonce, vtx.size());
  for (const auto &tx : vtx) {
    s << "  " << tx->ToString() << "\n";
  }
  return s.str();
}
