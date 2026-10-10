// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCCLIENT_H
#define BITCOIN_RPCCLIENT_H

#include <univalue.h>

UniValue RPCConvertValues(const std::string &strMethod,
                          const std::vector<std::string> &strParams);

UniValue RPCConvertNamedValues(const std::string &strMethod,
                               const std::vector<std::string> &strParams);

UniValue ParseNonRFCJSONValue(const std::string &strVal);

#endif
