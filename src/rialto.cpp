// Copyright (c) 2024 The Litecoin Cash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/aes.h>
#include <crypto/hmac_sha256.h>
#include <crypto/sha512.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <rialto.h>
#include <rpc/server.h>
#include <timedata.h>
#include <validation.h>

#include <secp256k1/include/secp256k1.h>
#include <secp256k1/include/secp256k1_ecdh.h>

#include <boost/algorithm/string.hpp>

#include <iomanip>

std::vector<RialtoQueuedMessage> receivedMessageQueue;
std::mutex receivedMessageQueueMutex;
std::condition_variable receivedMessageQueueCV;

CRialtoWhitePagesDB::CRialtoWhitePagesDB(std::string dbName, size_t nCacheSize,
                                         bool fMemory, bool fWipe)
    : CDBWrapper(GetDataDir() / dbName, nCacheSize, fMemory, fWipe) {
  LogPrintf("Rialto: DB online: %s\n", dbName);

#ifdef THIS_CODE_DISABLED
  LogPrintf("%s Entry Dump:\n", dbName);
  std::vector<std::pair<std::string, std::string>> all = GetAll();
  for (auto a : all)
    LogPrintf("  %s -> %s\n", a.first, a.second);
#endif
}

bool CRialtoWhitePagesDB::GetPubKeyForNick(const std::string nick,
                                           std::string &pubKey) {
  return Read(nick, pubKey);
}

bool CRialtoWhitePagesDB::SetPubKeyForNick(const std::string nick,
                                           const std::string pubKey) {
  return Write(nick, pubKey);
}

bool CRialtoWhitePagesDB::RemoveNick(const std::string nick) {
  return Erase(nick);
}

bool CRialtoWhitePagesDB::NickExists(const std::string nick) {
  return Exists(nick);
}

std::vector<std::pair<std::string, std::string>> CRialtoWhitePagesDB::GetAll() {
  std::vector<std::pair<std::string, std::string>> results;

  std::unique_ptr<CDBIterator> it(NewIterator());
  it->SeekToFirst();
  while (it->Valid()) {
    std::string k;
    std::string v;
    it->GetKey(k);
    it->GetValue(v);

    results.push_back(std::make_pair(k, v));

    it->Next();
  }

  return results;
}

static void RialtoIncorrectAPIUsageCallback(const char *str, void *data) {
  LogPrint(BCLog::RIALTO,
           "Rialto: WARNING: SECP256K1 INCORRECT API USAGE. str=%s\n", str);
}

template <typename T> std::string IntToHexStr(T i) {
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(sizeof(T) * 2) << std::hex << i;
  return stream.str();
}

bool RialtoIsValidNickFormat(const std::string nick) {
  if (nick.length() < 3 || nick.length() > 20)
    return false;

  for (char c : nick)
    if ((c < 'a' || c > 'z') && c != '_')
      return false;

  return true;
}

bool RialtoIsValidPlaintext(const std::string plaintext) {
  if (plaintext.find_first_not_of(' ') == std::string::npos)
    return false;

  if (plaintext.length() < 1 || plaintext.length() > 160)
    return false;

  for (char c : plaintext) {
    if (c < 32 || c > 126)
      return false;
  }
  return true;
}

bool RialtoParseLayer3Envelope(const std::string ciphertext, std::string &err,
                               std::string *layer2Envelope,
                               uint32_t *timestamp) {
  if (ciphertext.size() < RIALTO_L3_MIN_LENGTH * 2) {
    err = "Layer 3 envelope is too short.";
    return false;
  } else if (ciphertext.size() > RIALTO_L3_MAX_LENGTH * 2) {
    err = "Layer 3 envelope is too long (max " +
          std::to_string(RIALTO_L3_MAX_LENGTH) + ", found " +
          std::to_string(ciphertext.size()) + ").";
    return false;
  }

  uint32_t nonce = std::stoul(ciphertext.substr(0, 8), nullptr, 16);
  uint32_t t = std::stoul(ciphertext.substr(8, 8), nullptr, 16);
  std::string e = ciphertext.substr(16);

  uint32_t now = GetAdjustedTime();
  if (t < now - RIALTO_MESSAGE_TTL) {
    err = "Message timestamp is too old.";
    return false;
  } else if (t > now + RIALTO_MESSAGE_TTL) {
    err = "Message timestamp is too far in the future.";
    return false;
  }

  std::string dataToHash = IntToHexStr(t) + e;
  arith_uint256 hash = arith_uint256(
      CBlockHeader::MinotaurHashString(dataToHash + std::to_string(nonce))
          .ToString());
  if (hash > RIALTO_MESSAGE_POW_TARGET) {
    err = "Message doesn't meet PoW target.";
    return false;
  }

  if (timestamp)
    *timestamp = t;

  if (layer2Envelope)
    *layer2Envelope = e;

  return true;
}

bool RialtoEncryptMessage(const std::string nickFrom, const std::string nickTo,
                          const std::string plaintext, std::string &ciphertext,
                          uint32_t &timestampSent, std::string &err) {
  if (!RialtoIsValidPlaintext(plaintext)) {
    err = "Plaintext is invalid; 1-160 printable characters only. Cannot "
          "contain only spaces.";
    return false;
  }

  if (nickFrom == "") {
    err = "From nick is empty.";
    return false;
  }

  if (RialtoNickIsLocal(nickTo)) {
    err = "You can chat to yourself in the mirror, but not here! (Destination "
          "nick is local)";
    return false;
  }

  if (RialtoNickIsBlocked(nickTo)) {
    err = "Destination nick is blocked.";
    return false;
  }

  uint32_t now = GetAdjustedTime();
  std::string nowStr = IntToHexStr(now);

  std::vector<unsigned char, secure_allocator<unsigned char>> layer1EnvelopeVec(
      0);

  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), plaintext.begin(),
                           plaintext.end());
  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), '\0');
  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), nowStr.begin(),
                           nowStr.end());
  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), nickFrom.begin(),
                           nickFrom.end());
  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), '\0');
  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), nickTo.begin(),
                           nickTo.end());
  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), '\0');

  std::vector<unsigned char, secure_allocator<unsigned char>> fromPrivKeyData(
      32);
  if (!RialtoGetLocalPrivKeyForNick(nickFrom, fromPrivKeyData.data())) {
    err = "Can't find local privkey for sending nick.";
    return false;
  }

  CKey key;
  key.Set(fromPrivKeyData.begin(), fromPrivKeyData.end(), true);

  const uint256 messageHash =
      Hash(layer1EnvelopeVec.begin(), layer1EnvelopeVec.end());
  std::vector<unsigned char> messageSig;
  if (!key.SignCompact(messageHash, messageSig)) {
    err = "Couldn't sign the message.";
    return false;
  }

  layer1EnvelopeVec.insert(layer1EnvelopeVec.end(), messageSig.begin(),
                           messageSig.end());

  memory_cleanse(fromPrivKeyData.data(), 32);
  memory_cleanse((void *)key.begin(), 32);

  std::string destPubKeyStr;
  if (!RialtoGetGlobalPubKeyForNick(nickTo, destPubKeyStr)) {
    err = "Can't find recipient pubkey in white pages.";
    return false;
  }
  CPubKey destPubKey(ParseHex(destPubKeyStr));

  std::vector<unsigned char, secure_allocator<unsigned char>> IV(16);
  GetStrongRandBytes(IV.data(), 16);

  std::vector<unsigned char, secure_allocator<unsigned char>> contextSeed(32);
  GetStrongRandBytes(contextSeed.data(), 32);

  secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

  if (!ctx) {
    err = "Couldn't create secp256k1 context.";
    return false;
  }

  secp256k1_context_set_illegal_callback(ctx, RialtoIncorrectAPIUsageCallback,
                                         NULL);

  if (!secp256k1_context_randomize(ctx, contextSeed.data())) {
    err = "Couldn't randomise context.";
    secp256k1_context_destroy(ctx);
    return false;
  }

  secp256k1_pubkey destPubKeyParsed;
  if (!secp256k1_ec_pubkey_parse(ctx, &destPubKeyParsed, destPubKey.begin(),
                                 destPubKey.size())) {
    err = "Couldn't parse the destination pubkey.";
    secp256k1_context_destroy(ctx);
    return false;
  }

  CKey *ephemeralKey = new CKey();
  ephemeralKey->MakeNewKey(true);

  CPubKey ephemeralPubKey = ephemeralKey->GetPubKey();

  std::vector<unsigned char, secure_allocator<unsigned char>> sharedSecret(32);
  if (!secp256k1_ecdh(ctx, sharedSecret.data(), &destPubKeyParsed,
                      ephemeralKey->begin())) {
    err = "Couldn't perform ECDH to get shared secret.";
    secp256k1_context_destroy(ctx);
    return false;
  }

  memory_cleanse((void *)ephemeralKey->begin(), 32);
  secp256k1_context_destroy(ctx);

  std::vector<unsigned char, secure_allocator<unsigned char>> sharedSecretHash(
      64);
  CSHA512 hasher;
  hasher.Write(sharedSecret.data(), 32).Finalize(sharedSecretHash.data());

  std::vector<unsigned char, secure_allocator<unsigned char>> keyEncryption(32);
  std::vector<unsigned char, secure_allocator<unsigned char>> keyMAC(32);

  memcpy(keyEncryption.data(), sharedSecretHash.data(), 32);
  memcpy(keyMAC.data(), sharedSecretHash.data() + 32, 32);

  memory_cleanse(sharedSecret.data(), 32);
  memory_cleanse(sharedSecretHash.data(), 64);

  std::vector<unsigned char, secure_allocator<unsigned char>> encrypted;
  encrypted.resize(layer1EnvelopeVec.size() + AES_BLOCKSIZE);

  AES256CBCEncrypt crypter(keyEncryption.data(), IV.data(), true);
  int encryptedSize = crypter.Encrypt(
      layer1EnvelopeVec.data(), layer1EnvelopeVec.size(), encrypted.data());
  encrypted.resize(encryptedSize);

  memory_cleanse(layer1EnvelopeVec.data(), layer1EnvelopeVec.size());
  memory_cleanse(keyEncryption.data(), 32);

  CHMAC_SHA256 macer(keyMAC.data(), 32);
  macer.Write(IV.data(), 16)
      .Write(ephemeralPubKey.begin(), ephemeralPubKey.size())
      .Write(encrypted.data(), encrypted.size());

  std::vector<unsigned char, secure_allocator<unsigned char>> mac;
  mac.resize(32);
  macer.Finalize(mac.data());

  std::vector<unsigned char, secure_allocator<unsigned char>> layer2EnvelopeVec;
  layer2EnvelopeVec.insert(layer2EnvelopeVec.end(), IV.begin(), IV.end());
  layer2EnvelopeVec.insert(layer2EnvelopeVec.end(), ephemeralPubKey.begin(),
                           ephemeralPubKey.end());
  layer2EnvelopeVec.insert(layer2EnvelopeVec.end(), encrypted.begin(),
                           encrypted.end());
  layer2EnvelopeVec.insert(layer2EnvelopeVec.end(), mac.begin(), mac.end());

  uint32_t nonce = 0;
  std::string dataToHash =
      nowStr + HexStr(layer2EnvelopeVec.begin(), layer2EnvelopeVec.end());

  while (true) {
    arith_uint256 currentHash = arith_uint256(
        CBlockHeader::MinotaurHashString(dataToHash + std::to_string(nonce))
            .ToString());
    if (currentHash <= RIALTO_MESSAGE_POW_TARGET)
      break;

    nonce++;
    if (nonce == 0) {
      err = "PoW Nonce overflow.";
      return false;
    }
  }

  ciphertext = IntToHexStr(nonce) + nowStr +
               HexStr(layer2EnvelopeVec.begin(), layer2EnvelopeVec.end());
  timestampSent = now;
  return true;
}

bool RialtoDecryptMessage(const std::string layer3Envelope, std::string &err) {
  std::string layer2Envelope;
  uint32_t layer3timestamp;
  if (!RialtoParseLayer3Envelope(layer3Envelope, err, &layer2Envelope,
                                 &layer3timestamp))
    return false;

  if (layer2Envelope.size() < RIALTO_L2_MIN_LENGTH * 2) {
    err = "Layer 2 envelope is too short.";
    return false;
  } else if (layer2Envelope.size() > RIALTO_L2_MAX_LENGTH * 2) {
    err = "Layer 2 envelope is too long.";
    return false;
  }

  std::vector<unsigned char> IV = ParseHex(layer2Envelope.substr(0, 32));
  std::vector<unsigned char> ephemeralPubKey =
      ParseHex(layer2Envelope.substr(32, 66));
  std::vector<unsigned char> encrypted =
      ParseHex(layer2Envelope.substr(98, layer2Envelope.size() - 98 - 64));
  std::vector<unsigned char> mac =
      ParseHex(layer2Envelope.substr(layer2Envelope.size() - 64, 64));

  if (encrypted.size() % AES_BLOCKSIZE != 0) {
    err = "Encrypted data is not a multiple of AES_BLOCKSIZE bytes.";
    return false;
  }

  secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);

  if (!ctx) {
    err = "Couldn't create secp256k1 context.";
    return false;
  }
  secp256k1_context_set_illegal_callback(ctx, RialtoIncorrectAPIUsageCallback,
                                         NULL);

  secp256k1_pubkey ephemeralPubKeyParsed;
  if (!secp256k1_ec_pubkey_parse(ctx, &ephemeralPubKeyParsed,
                                 ephemeralPubKey.data(),
                                 ephemeralPubKey.size())) {
    err = "Couldn't parse the ephemeral pubkey.";
    secp256k1_context_destroy(ctx);
    return false;
  }

  std::vector<std::pair<std::string, std::string>> nicks = RialtoGetAllLocal();
  for (auto n : nicks) {
    std::vector<unsigned char, secure_allocator<unsigned char>> privKeyData(32);
    if (!RialtoGetLocalPrivKeyForNick(n.first, privKeyData.data())) {
      LogPrint(BCLog::RIALTO,
               "Error: Can't find local privkey for nick %s. IS THE WALLET "
               "LOCKED?\n",
               n.first);
      continue;
    }

    std::vector<unsigned char, secure_allocator<unsigned char>> contextSeed(32);
    GetStrongRandBytes(contextSeed.data(), 32);

    if (!secp256k1_context_randomize(ctx, contextSeed.data())) {
      LogPrint(BCLog::RIALTO,
               "Error: Couldn't randomise context when trying as %s\n",
               n.first);
      memory_cleanse(privKeyData.data(), 32);

      continue;
    }

    std::vector<unsigned char, secure_allocator<unsigned char>> sharedSecret(
        32);
    if (!secp256k1_ecdh(ctx, sharedSecret.data(), &ephemeralPubKeyParsed,
                        privKeyData.data())) {
      LogPrint(BCLog::RIALTO,
               "Error: Couldn't perform ECDH to get shared secret when trying "
               "as %s\n",
               n.first);
      memory_cleanse(privKeyData.data(), 32);

      memory_cleanse(sharedSecret.data(), 32);

      continue;
    }

    memory_cleanse(privKeyData.data(), 32);

    std::vector<unsigned char, secure_allocator<unsigned char>>
        sharedSecretHash(64);
    CSHA512 hasher;
    hasher.Write(sharedSecret.data(), 32).Finalize(sharedSecretHash.data());

    std::vector<unsigned char, secure_allocator<unsigned char>> keyEncryption(
        32);
    std::vector<unsigned char, secure_allocator<unsigned char>> keyMAC(32);

    memcpy(keyEncryption.data(), sharedSecretHash.data(), 32);
    memcpy(keyMAC.data(), sharedSecretHash.data() + 32, 32);

    memory_cleanse(sharedSecret.data(), 32);
    memory_cleanse(sharedSecretHash.data(), 64);

    CHMAC_SHA256 macer(keyMAC.data(), 32);
    macer.Write(IV.data(), 16)
        .Write(ephemeralPubKey.data(), ephemeralPubKey.size())
        .Write(encrypted.data(), encrypted.size());

    std::vector<unsigned char, secure_allocator<unsigned char>> macCalc;
    macCalc.resize(32);
    macer.Finalize(macCalc.data());

    memory_cleanse(keyMAC.data(), 32);

    if (memcmp(mac.data(), macCalc.data(), 32) != 0)
      continue;

    std::vector<unsigned char, secure_allocator<unsigned char>>
        layer1EnvelopeVec;
    layer1EnvelopeVec.resize(RIALTO_L1_MAX_LENGTH);
    AES256CBCDecrypt dec(keyEncryption.data(), IV.data(), true);
    int size = dec.Decrypt(encrypted.data(), encrypted.size(),
                           layer1EnvelopeVec.data());
    layer1EnvelopeVec.resize(size);

    memory_cleanse(keyEncryption.data(), 32);

    if (layer1EnvelopeVec.size() < RIALTO_L1_MIN_LENGTH) {
      err = "Layer 1 envelope is too short.";
      secp256k1_context_destroy(ctx);
      return false;
    } else if (layer1EnvelopeVec.size() > RIALTO_L1_MAX_LENGTH) {
      err = "Layer 1 envelope is too long.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    size_t firstNull = 0, secondNull = 0, thirdNull = 0;
    for (size_t i = 0; i < layer1EnvelopeVec.size(); i++) {
      if (layer1EnvelopeVec[i] == '\0') {
        if (firstNull == 0)
          firstNull = i;
        else if (secondNull == 0)
          secondNull = i;
        else if (thirdNull == 0) {
          thirdNull = i;
          break;
        }
      }
    }

    if (firstNull == 0 || secondNull == 0 || thirdNull == 0) {
      err = "Nulls missing in layer1EnvelopeVec.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    std::string unconfirmedPlaintext = std::string(
        layer1EnvelopeVec.begin(), layer1EnvelopeVec.begin() + firstNull);
    std::string layer1timestampStr =
        std::string(layer1EnvelopeVec.begin() + firstNull + 1,
                    layer1EnvelopeVec.begin() + firstNull + 9);
    std::string unconfirmedSenderNick =
        std::string(layer1EnvelopeVec.begin() + firstNull + 9,
                    layer1EnvelopeVec.begin() + secondNull);
    std::string unconfirmedDestinationNick =
        std::string(layer1EnvelopeVec.begin() + secondNull + 1,
                    layer1EnvelopeVec.begin() + thirdNull);
    std::vector<unsigned char> messageSig(
        layer1EnvelopeVec.begin() + thirdNull + 1, layer1EnvelopeVec.end());

    std::vector<unsigned char> encapsulatedMessage(
        layer1EnvelopeVec.begin(), layer1EnvelopeVec.begin() + thirdNull + 1);

    uint32_t layer1timestamp = std::stoul(layer1timestampStr, nullptr, 16);
    if (layer1timestamp != layer3timestamp) {
      err = "Layer 1 / Layer 3 Envelope timestamp mismatch.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    if (!RialtoIsValidNickFormat(unconfirmedDestinationNick)) {
      err = "Invalid destination nick format. Shenanigans!";
      secp256k1_context_destroy(ctx);
      return false;
    }
    if (!RialtoIsValidNickFormat(unconfirmedSenderNick)) {
      err = "Invalid sender nick. Shenanigans!";
      secp256k1_context_destroy(ctx);
      return false;
    }

    if (unconfirmedDestinationNick != n.first) {
      err = "Destination nick doesn't match the nick we're trying to decrypt "
            "as. Possible repackaged-L1 replay attack. Shenanigans!";
      secp256k1_context_destroy(ctx);
      return false;
    }

    if (RialtoNickIsBlocked(unconfirmedSenderNick)) {
      err = "Sender nick is blocked.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    if (!RialtoIsValidPlaintext(unconfirmedPlaintext)) {
      err = "Invalid plaintext.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    std::string whitePagesPubKey;
    if (!RialtoGetGlobalPubKeyForNick(unconfirmedSenderNick,
                                      whitePagesPubKey)) {
      err = "Can't find pubkey for sending nick in White Pages.";
      secp256k1_context_destroy(ctx);
      return false;
    }
    std::vector<unsigned char> whitePagesPubKeyVec = ParseHex(whitePagesPubKey);

    const uint256 messageHash =
        Hash(encapsulatedMessage.begin(), encapsulatedMessage.end());
    CPubKey sigPubKey;
    if (!sigPubKey.RecoverCompact(messageHash, messageSig)) {
      err = "Strange format. Couldn't recover a pubkey from the message sig.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    if (memcmp(sigPubKey.begin(), whitePagesPubKeyVec.data(), 33) != 0) {
      err = "Forgery. Pubkey from sig doesn't match pubkey from white pages.";
      secp256k1_context_destroy(ctx);
      return false;
    }

    RialtoQueuedMessage qm;
    qm.fromNick = std::vector<unsigned char, secure_allocator<unsigned char>>(
        unconfirmedSenderNick.begin(), unconfirmedSenderNick.end());
    qm.toNick = std::vector<unsigned char, secure_allocator<unsigned char>>(
        unconfirmedDestinationNick.begin(), unconfirmedDestinationNick.end());
    qm.message = std::vector<unsigned char, secure_allocator<unsigned char>>(
        unconfirmedPlaintext.begin(), unconfirmedPlaintext.end());
    qm.timestamp = layer1timestamp;

    std::lock_guard<std::mutex> lock(receivedMessageQueueMutex);
    receivedMessageQueue.push_back(qm);
    receivedMessageQueueCV.notify_one();

    secp256k1_context_destroy(ctx);
    return true;
  }

  err = "Not for us.";
  secp256k1_context_destroy(ctx);
  return false;
}

std::vector<RialtoQueuedMessage> RialtoGetQueuedMessages() {
  std::unique_lock<std::mutex> lock(receivedMessageQueueMutex);

  receivedMessageQueueCV.wait_for(lock, std::chrono::milliseconds(10000), [] {
    return !receivedMessageQueue.empty() || !IsRPCRunning();
  });

  if (receivedMessageQueue.empty()) {
    return std::vector<RialtoQueuedMessage>();
  }

  std::vector<RialtoQueuedMessage> messages = receivedMessageQueue;
  receivedMessageQueue.clear();
  LogPrint(BCLog::RIALTO, "Rialto: Queued messages retrieved\n");

  return messages;
}