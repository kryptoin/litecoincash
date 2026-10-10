// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCPROTOCOL_H
#define BITCOIN_RPCPROTOCOL_H

#include <fs.h>

#include <list>
#include <map>
#include <stdint.h>
#include <string>

#include <univalue.h>

enum HTTPStatusCode {
  HTTP_OK = 200,
  HTTP_BAD_REQUEST = 400,
  HTTP_UNAUTHORIZED = 401,
  HTTP_FORBIDDEN = 403,
  HTTP_NOT_FOUND = 404,
  HTTP_BAD_METHOD = 405,
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_SERVICE_UNAVAILABLE = 503,
};

enum RPCErrorCode {
  RPC_INVALID_REQUEST = -32600,

  RPC_METHOD_NOT_FOUND = -32601,
  RPC_INVALID_PARAMS = -32602,

  RPC_INTERNAL_ERROR = -32603,
  RPC_PARSE_ERROR = -32700,

  RPC_MISC_ERROR = -1,

  RPC_FORBIDDEN_BY_SAFE_MODE = -2,

  RPC_TYPE_ERROR = -3,

  RPC_INVALID_ADDRESS_OR_KEY = -5,

  RPC_OUT_OF_MEMORY = -7,

  RPC_INVALID_PARAMETER = -8,

  RPC_DATABASE_ERROR = -20,

  RPC_DESERIALIZATION_ERROR = -22,

  RPC_VERIFY_ERROR = -25,

  RPC_VERIFY_REJECTED = -26,

  RPC_VERIFY_ALREADY_IN_CHAIN = -27,

  RPC_IN_WARMUP = -28,

  RPC_METHOD_DEPRECATED = -32,

  RPC_TRANSACTION_ERROR = RPC_VERIFY_ERROR,
  RPC_TRANSACTION_REJECTED = RPC_VERIFY_REJECTED,
  RPC_TRANSACTION_ALREADY_IN_CHAIN = RPC_VERIFY_ALREADY_IN_CHAIN,

  RPC_CLIENT_NOT_CONNECTED = -9,

  RPC_CLIENT_IN_INITIAL_DOWNLOAD = -10,

  RPC_CLIENT_NODE_ALREADY_ADDED = -23,

  RPC_CLIENT_NODE_NOT_ADDED = -24,

  RPC_CLIENT_NODE_NOT_CONNECTED = -29,

  RPC_CLIENT_INVALID_IP_OR_SUBNET = -30,

  RPC_CLIENT_P2P_DISABLED = -31,

  RPC_WALLET_ERROR = -4,

  RPC_WALLET_INSUFFICIENT_FUNDS = -6,

  RPC_WALLET_INVALID_ACCOUNT_NAME = -11,

  RPC_WALLET_KEYPOOL_RAN_OUT = -12,

  RPC_WALLET_UNLOCK_NEEDED = -13,

  RPC_WALLET_PASSPHRASE_INCORRECT = -14,

  RPC_WALLET_WRONG_ENC_STATE = -15,

  RPC_WALLET_ENCRYPTION_FAILED = -16,

  RPC_WALLET_ALREADY_UNLOCKED = -17,

  RPC_WALLET_NOT_FOUND = -18,

  RPC_WALLET_NOT_SPECIFIED = -19,

  RPC_WALLET_BCT_FAIL = -64,

  RPC_WALLET_NCT_FAIL = -65,

  RPC_RIALTO_ERROR = -66,

};

UniValue JSONRPCRequestObj(const std::string &strMethod, const UniValue &params,
                           const UniValue &id);
UniValue JSONRPCReplyObj(const UniValue &result, const UniValue &error,
                         const UniValue &id);
std::string JSONRPCReply(const UniValue &result, const UniValue &error,
                         const UniValue &id);
UniValue JSONRPCError(int code, const std::string &message);

bool GenerateAuthCookie(std::string *cookie_out);

bool GetAuthCookie(std::string *cookie_out);

void DeleteAuthCookie();

std::vector<UniValue> JSONRPCProcessBatchReply(const UniValue &in, size_t num);

#endif
