// Copyright (c) 2016-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_RPCWALLET_H
#define BITCOIN_WALLET_RPCWALLET_H

#include <string>

class CRPCTable;
class CWallet;
class JSONRPCRequest;

struct BeePopGraphPoint;

extern BeePopGraphPoint beePopGraph[1024 * 40];

void RegisterWalletRPCCommands(CRPCTable &t);

CWallet *GetWalletForJSONRPCRequest(const JSONRPCRequest &request);

CWallet *GetWalletForQTKeyImport();

std::string HelpRequiringPassphrase(CWallet *);
void EnsureWalletIsUnlocked(CWallet *);
bool EnsureWalletIsAvailable(CWallet *, bool avoidException);

#endif
