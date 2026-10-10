// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCREGISTER_H
#define BITCOIN_RPCREGISTER_H

class CRPCTable;

void RegisterBlockchainRPCCommands(CRPCTable &tableRPC);

void RegisterNetRPCCommands(CRPCTable &tableRPC);

void RegisterMiscRPCCommands(CRPCTable &tableRPC);

void RegisterMiningRPCCommands(CRPCTable &tableRPC);

void RegisterRawTransactionRPCCommands(CRPCTable &tableRPC);

static inline void RegisterAllCoreRPCCommands(CRPCTable &t) {
  RegisterBlockchainRPCCommands(t);
  RegisterNetRPCCommands(t);
  RegisterMiscRPCCommands(t);
  RegisterMiningRPCCommands(t);
  RegisterRawTransactionRPCCommands(t);
}

#endif
