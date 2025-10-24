// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HTTPRPC_H
#define BITCOIN_HTTPRPC_H

#include <map>
#include <string>

bool StartHTTPRPC();

void InterruptHTTPRPC();

void StopHTTPRPC();

bool StartREST();

void InterruptREST();

void StopREST();

#endif
