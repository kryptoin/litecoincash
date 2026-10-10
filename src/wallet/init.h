// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_INIT_H
#define BITCOIN_WALLET_INIT_H

#include <string>

class CRPCTable;
class CScheduler;

std::string GetWalletHelpString(bool showDebug);

bool WalletParameterInteraction();

void RegisterWalletRPC(CRPCTable &tableRPC);

bool VerifyWallets();

bool OpenWallets();

void StartWallets(CScheduler &scheduler);

void FlushWallets();

void StopWallets();

void CloseWallets();

#endif
