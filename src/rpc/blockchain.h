// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_BLOCKCHAIN_H
#define BITCOIN_RPC_BLOCKCHAIN_H

class CBlock;
class CBlockIndex;
class UniValue;

double GetDifficulty(const CBlockIndex *blockindex = nullptr,
                     bool getHiveDifficulty = false,
                     POW_TYPE powType = POW_TYPE_SHA256);

void RPCNotifyBlockChange(bool ibd, const CBlockIndex *);

UniValue blockToJSON(const CBlock &block, const CBlockIndex *blockindex,
                     bool txDetails = false);

UniValue mempoolInfoToJSON();

UniValue mempoolToJSON(bool fVerbose = false);

UniValue blockheaderToJSON(const CBlockIndex *blockindex);

#endif
