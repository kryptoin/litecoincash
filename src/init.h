// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include <string>

class CScheduler;
class CWallet;

namespace boost {
class thread_group;
}

void StartShutdown();
bool ShutdownRequested();

void Interrupt();
void Shutdown();

void InitLogging();

void InitParameterInteraction();

bool AppInitBasicSetup();

bool AppInitParameterInteraction();

bool AppInitSanityChecks();

bool AppInitLockDataDirectory();

bool AppInitMain();

enum HelpMessageMode { HMM_BITCOIND, HMM_BITCOIN_QT };

std::string HelpMessage(HelpMessageMode mode);

std::string LicenseInfo();

#endif
