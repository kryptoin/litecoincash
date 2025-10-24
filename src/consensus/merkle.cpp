// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/merkle.h>
#include <hash.h>
#include <utilstrencodings.h>

static void MerkleComputation(const std::vector<uint256> &leaves,
                              uint256 *proot, bool *pmutated,
                              uint32_t branchpos,
                              std::vector<uint256> *pbranch) {
  if (pbranch)
    pbranch->clear();
  if (leaves.size() == 0) {
    if (pmutated)
      *pmutated = false;
    if (proot)
      *proot = uint256();
    return;
  }
  bool mutated = false;

  uint32_t count = 0;

  uint256 inner[32];

  int matchlevel = -1;

  while (count < leaves.size()) {
    uint256 h = leaves[count];
    bool matchh = count == branchpos;
    count++;
    int level;

    for (level = 0; !(count & (((uint32_t)1) << level)); level++) {
      if (pbranch) {
        if (matchh) {
          pbranch->push_back(inner[level]);
        } else if (matchlevel == level) {
          pbranch->push_back(h);
          matchh = true;
        }
      }
      mutated |= (inner[level] == h);
      CHash256()
          .Write(inner[level].begin(), 32)
          .Write(h.begin(), 32)
          .Finalize(h.begin());
    }

    inner[level] = h;
    if (matchh) {
      matchlevel = level;
    }
  }

  int level = 0;

  while (!(count & (((uint32_t)1) << level))) {
    level++;
  }
  uint256 h = inner[level];
  bool matchh = matchlevel == level;
  while (count != (((uint32_t)1) << level)) {
    if (pbranch && matchh) {
      pbranch->push_back(h);
    }
    CHash256().Write(h.begin(), 32).Write(h.begin(), 32).Finalize(h.begin());

    count += (((uint32_t)1) << level);
    level++;

    while (!(count & (((uint32_t)1) << level))) {
      if (pbranch) {
        if (matchh) {
          pbranch->push_back(inner[level]);
        } else if (matchlevel == level) {
          pbranch->push_back(h);
          matchh = true;
        }
      }
      CHash256()
          .Write(inner[level].begin(), 32)
          .Write(h.begin(), 32)
          .Finalize(h.begin());
      level++;
    }
  }

  if (pmutated)
    *pmutated = mutated;
  if (proot)
    *proot = h;
}

uint256 ComputeMerkleRoot(const std::vector<uint256> &leaves, bool *mutated) {
  uint256 hash;
  MerkleComputation(leaves, &hash, mutated, -1, nullptr);
  return hash;
}

std::vector<uint256> ComputeMerkleBranch(const std::vector<uint256> &leaves,
                                         uint32_t position) {
  std::vector<uint256> ret;
  MerkleComputation(leaves, nullptr, nullptr, position, &ret);
  return ret;
}

uint256 ComputeMerkleRootFromBranch(const uint256 &leaf,
                                    const std::vector<uint256> &vMerkleBranch,
                                    uint32_t nIndex) {
  uint256 hash = leaf;
  for (std::vector<uint256>::const_iterator it = vMerkleBranch.begin();
       it != vMerkleBranch.end(); ++it) {
    if (nIndex & 1) {
      hash = Hash(BEGIN(*it), END(*it), BEGIN(hash), END(hash));
    } else {
      hash = Hash(BEGIN(hash), END(hash), BEGIN(*it), END(*it));
    }
    nIndex >>= 1;
  }
  return hash;
}

uint256 BlockMerkleRoot(const CBlock &block, bool *mutated) {
  std::vector<uint256> leaves;
  leaves.resize(block.vtx.size());
  for (size_t s = 0; s < block.vtx.size(); s++) {
    leaves[s] = block.vtx[s]->GetHash();
  }
  return ComputeMerkleRoot(leaves, mutated);
}

uint256 BlockWitnessMerkleRoot(const CBlock &block, bool *mutated) {
  std::vector<uint256> leaves;
  leaves.resize(block.vtx.size());
  leaves[0].SetNull();

  for (size_t s = 1; s < block.vtx.size(); s++) {
    leaves[s] = block.vtx[s]->GetWitnessHash();
  }
  return ComputeMerkleRoot(leaves, mutated);
}

std::vector<uint256> BlockMerkleBranch(const CBlock &block, uint32_t position) {
  std::vector<uint256> leaves;
  leaves.resize(block.vtx.size());
  for (size_t s = 0; s < block.vtx.size(); s++) {
    leaves[s] = block.vtx[s]->GetHash();
  }
  return ComputeMerkleBranch(leaves, position);
}
