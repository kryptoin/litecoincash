// Copyright (c) 2024 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LITECOINCASH_RIALTO_H
#define LITECOINCASH_RIALTO_H

#include <arith_uint256.h>
#include <dbwrapper.h>
#include <support/allocators/secure.h>

#include <string>

const arith_uint256 RIALTO_MESSAGE_POW_TARGET = arith_uint256(
    "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

const int RIALTO_MESSAGE_TTL = 15 * 60;

const int RIALTO_L1_MIN_LENGTH = 1 + 1 + 8 + 3 + 1 + 3 + 1 + 65;

const int RIALTO_L1_MAX_LENGTH = 160 + 1 + 8 + 20 + 1 + 20 + 1 + 65;

const int RIALTO_L2_MIN_LENGTH = 16 + 33 + RIALTO_L1_MIN_LENGTH + 32;

const int RIALTO_L2_MAX_LENGTH = 16 + 33 + RIALTO_L1_MAX_LENGTH + 32;

const int RIALTO_L3_MIN_LENGTH = 8 + 8 + RIALTO_L2_MIN_LENGTH;

const int RIALTO_L3_MAX_LENGTH = 8 + 8 + RIALTO_L2_MAX_LENGTH;

class CRialtoWhitePagesDB : public CDBWrapper {
public:
  CRialtoWhitePagesDB(std::string dbName, size_t nCacheSize,
                      bool fMemory = false, bool fWipe = false);

  bool GetPubKeyForNick(const std::string nick, std::string &pubKey);
  bool SetPubKeyForNick(const std::string nick, const std::string pubKey);
  bool RemoveNick(const std::string nick);
  bool NickExists(const std::string nick);

  std::vector<std::pair<std::string, std::string>> GetAll();
};

class CRialtoMessage {
private:
  std::string message;

public:
  CRialtoMessage(const std::string m) { message = m; }

  const uint256 GetHash() const {
    CHashWriter ss(SER_GETHASH, 0);

    ss << message;
    return ss.GetHash();
  }

  const std::string GetMessage() const { return message; }
};

struct RialtoQueuedMessage {
  std::vector<unsigned char, secure_allocator<unsigned char>> fromNick;
  std::vector<unsigned char, secure_allocator<unsigned char>> toNick;
  std::vector<unsigned char, secure_allocator<unsigned char>> message;
  uint32_t timestamp;
};

extern std::vector<RialtoQueuedMessage> receivedMessageQueue;
extern std::mutex receivedMessageQueueMutex;
extern std::condition_variable receivedMessageQueueCV;

bool RialtoIsValidPlaintext(const std::string plaintext);

bool RialtoIsValidNickFormat(const std::string nick);

bool RialtoParseLayer3Envelope(const std::string ciphertext, std::string &err,
                               std::string *layer2Envelope = NULL,
                               uint32_t *timestamp = NULL);

bool RialtoEncryptMessage(const std::string nickFrom,

                          const std::string nickTo,

                          const std::string plaintext,

                          std::string &ciphertext,

                          uint32_t &timestampSent,

                          std::string &err

);

bool RialtoDecryptMessage(const std::string layer3Envelope,

                          std::string &err

);

std::vector<RialtoQueuedMessage> RialtoGetQueuedMessages();

#endif
