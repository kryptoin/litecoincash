// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>

#include <bech32.h>
#include <hash.h>
#include <script/script.h>
#include <uint256.h>
#include <utilstrencodings.h>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include <algorithm>
#include <assert.h>
#include <string.h>

static const char *pszBase58 =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool DecodeBase58(const char *psz, std::vector<unsigned char> &vch) {
  while (*psz && isspace(*psz))
    psz++;

  int zeroes = 0;
  int length = 0;
  while (*psz == '1') {
    zeroes++;
    psz++;
  }

  int size = strlen(psz) * 733 / 1000 + 1;

  std::vector<unsigned char> b256(size);

  while (*psz && !isspace(*psz)) {
    const char *ch = strchr(pszBase58, *psz);
    if (ch == nullptr)
      return false;

    int carry = ch - pszBase58;
    int i = 0;
    for (std::vector<unsigned char>::reverse_iterator it = b256.rbegin();
         (carry != 0 || i < length) && (it != b256.rend()); ++it, ++i) {
      carry += 58 * (*it);
      *it = carry % 256;
      carry /= 256;
    }
    assert(carry == 0);
    length = i;
    psz++;
  }

  while (isspace(*psz))
    psz++;
  if (*psz != 0)
    return false;

  std::vector<unsigned char>::iterator it = b256.begin() + (size - length);
  while (it != b256.end() && *it == 0)
    it++;

  vch.reserve(zeroes + (b256.end() - it));
  vch.assign(zeroes, 0x00);
  while (it != b256.end())
    vch.push_back(*(it++));
  return true;
}

std::string EncodeBase58(const unsigned char *pbegin,
                         const unsigned char *pend) {
  int zeroes = 0;
  int length = 0;
  while (pbegin != pend && *pbegin == 0) {
    pbegin++;
    zeroes++;
  }

  int size = (pend - pbegin) * 138 / 100 + 1;

  std::vector<unsigned char> b58(size);

  while (pbegin != pend) {
    int carry = *pbegin;
    int i = 0;

    for (std::vector<unsigned char>::reverse_iterator it = b58.rbegin();
         (carry != 0 || i < length) && (it != b58.rend()); it++, i++) {
      carry += 256 * (*it);
      *it = carry % 58;
      carry /= 58;
    }

    assert(carry == 0);
    length = i;
    pbegin++;
  }

  std::vector<unsigned char>::iterator it = b58.begin() + (size - length);
  while (it != b58.end() && *it == 0)
    it++;

  std::string str;
  str.reserve(zeroes + (b58.end() - it));
  str.assign(zeroes, '1');
  while (it != b58.end())
    str += pszBase58[*(it++)];
  return str;
}

std::string EncodeBase58(const std::vector<unsigned char> &vch) {
  return EncodeBase58(vch.data(), vch.data() + vch.size());
}

bool DecodeBase58(const std::string &str, std::vector<unsigned char> &vchRet) {
  return DecodeBase58(str.c_str(), vchRet);
}

std::string EncodeBase58Check(const std::vector<unsigned char> &vchIn) {
  std::vector<unsigned char> vch(vchIn);
  uint256 hash = Hash(vch.begin(), vch.end());
  vch.insert(vch.end(), (unsigned char *)&hash, (unsigned char *)&hash + 4);
  return EncodeBase58(vch);
}

bool DecodeBase58Check(const char *psz, std::vector<unsigned char> &vchRet) {
  if (!DecodeBase58(psz, vchRet) || (vchRet.size() < 4)) {
    vchRet.clear();
    return false;
  }

  uint256 hash = Hash(vchRet.begin(), vchRet.end() - 4);
  if (memcmp(&hash, &vchRet[vchRet.size() - 4], 4) != 0) {
    vchRet.clear();
    return false;
  }
  vchRet.resize(vchRet.size() - 4);
  return true;
}

bool DecodeBase58Check(const std::string &str,
                       std::vector<unsigned char> &vchRet) {
  return DecodeBase58Check(str.c_str(), vchRet);
}

CBase58Data::CBase58Data() {
  vchVersion.clear();
  vchData.clear();
}

void CBase58Data::SetData(const std::vector<unsigned char> &vchVersionIn,
                          const void *pdata, size_t nSize) {
  vchVersion = vchVersionIn;
  vchData.resize(nSize);
  if (!vchData.empty())
    memcpy(vchData.data(), pdata, nSize);
}

void CBase58Data::SetData(const std::vector<unsigned char> &vchVersionIn,
                          const unsigned char *pbegin,
                          const unsigned char *pend) {
  SetData(vchVersionIn, (void *)pbegin, pend - pbegin);
}

bool CBase58Data::SetString(const char *psz, unsigned int nVersionBytes) {
  std::vector<unsigned char> vchTemp;
  bool rc58 = DecodeBase58Check(psz, vchTemp);
  if ((!rc58) || (vchTemp.size() < nVersionBytes)) {
    vchData.clear();
    vchVersion.clear();
    return false;
  }
  vchVersion.assign(vchTemp.begin(), vchTemp.begin() + nVersionBytes);
  vchData.resize(vchTemp.size() - nVersionBytes);
  if (!vchData.empty())
    memcpy(vchData.data(), vchTemp.data() + nVersionBytes, vchData.size());
  memory_cleanse(vchTemp.data(), vchTemp.size());
  return true;
}

bool CBase58Data::SetString(const std::string &str) {
  return SetString(str.c_str());
}

std::string CBase58Data::ToString() const {
  std::vector<unsigned char> vch = vchVersion;
  vch.insert(vch.end(), vchData.begin(), vchData.end());
  return EncodeBase58Check(vch);
}

int CBase58Data::CompareTo(const CBase58Data &b58) const {
  if (vchVersion < b58.vchVersion)
    return -1;
  if (vchVersion > b58.vchVersion)
    return 1;
  if (vchData < b58.vchData)
    return -1;
  if (vchData > b58.vchData)
    return 1;
  return 0;
}

namespace {
class DestinationEncoder : public boost::static_visitor<std::string> {
private:
  const CChainParams &m_params;

public:
  DestinationEncoder(const CChainParams &params) : m_params(params) {}

  std::string operator()(const CKeyID &id) const {
    std::vector<unsigned char> data =
        m_params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    data.insert(data.end(), id.begin(), id.end());
    return EncodeBase58Check(data);
  }

  std::string operator()(const CScriptID &id) const {
    std::vector<unsigned char> data =
        m_params.Base58Prefix(CChainParams::SCRIPT_ADDRESS2);
    data.insert(data.end(), id.begin(), id.end());
    return EncodeBase58Check(data);
  }

  std::string operator()(const WitnessV0KeyHash &id) const {
    std::vector<unsigned char> data = {0};
    ConvertBits<8, 5, true>(data, id.begin(), id.end());
    return bech32::Encode(m_params.Bech32HRP(), data);
  }

  std::string operator()(const WitnessV0ScriptHash &id) const {
    std::vector<unsigned char> data = {0};
    ConvertBits<8, 5, true>(data, id.begin(), id.end());
    return bech32::Encode(m_params.Bech32HRP(), data);
  }

  std::string operator()(const WitnessUnknown &id) const {
    if (id.version < 1 || id.version > 16 || id.length < 2 || id.length > 40) {
      return {};
    }
    std::vector<unsigned char> data = {(unsigned char)id.version};
    ConvertBits<8, 5, true>(data, id.program, id.program + id.length);
    return bech32::Encode(m_params.Bech32HRP(), data);
  }

  std::string operator()(const CNoDestination &no) const { return {}; }
};

CTxDestination DecodeDestination(const std::string &str,
                                 const CChainParams &params) {
  std::vector<unsigned char> data;
  uint160 hash;
  if (DecodeBase58Check(str, data)) {
    const std::vector<unsigned char> &pubkey_prefix =
        params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
    if (data.size() == hash.size() + pubkey_prefix.size() &&
        std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
      std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
      return CKeyID(hash);
    }

    const std::vector<unsigned char> &script_prefix =
        params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
    if (data.size() == hash.size() + script_prefix.size() &&
        std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
      std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
      return CScriptID(hash);
    }

    const std::vector<unsigned char> &script_prefix2 =
        params.Base58Prefix(CChainParams::SCRIPT_ADDRESS2);
    if (data.size() == hash.size() + script_prefix2.size() &&
        std::equal(script_prefix2.begin(), script_prefix2.end(),
                   data.begin())) {
      std::copy(data.begin() + script_prefix2.size(), data.end(), hash.begin());
      return CScriptID(hash);
    }
  }
  data.clear();
  auto bech = bech32::Decode(str);
  if (bech.second.size() > 0 && bech.first == params.Bech32HRP()) {
    int version = bech.second[0];

    if (ConvertBits<5, 8, false>(data, bech.second.begin() + 1,
                                 bech.second.end())) {
      if (version == 0) {
        {
          WitnessV0KeyHash keyid;
          if (data.size() == keyid.size()) {
            std::copy(data.begin(), data.end(), keyid.begin());
            return keyid;
          }
        }
        {
          WitnessV0ScriptHash scriptid;
          if (data.size() == scriptid.size()) {
            std::copy(data.begin(), data.end(), scriptid.begin());
            return scriptid;
          }
        }
        return CNoDestination();
      }
      if (version > 16 || data.size() < 2 || data.size() > 40) {
        return CNoDestination();
      }
      WitnessUnknown unk;
      unk.version = version;
      std::copy(data.begin(), data.end(), unk.program);
      unk.length = data.size();
      return unk;
    }
  }
  return CNoDestination();
}
} // namespace

void CBitcoinSecret::SetKey(const CKey &vchSecret) {
  assert(vchSecret.IsValid());
  SetData(Params().Base58Prefix(CChainParams::SECRET_KEY), vchSecret.begin(),
          vchSecret.size());
  if (vchSecret.IsCompressed())
    vchData.push_back(1);
}

CKey CBitcoinSecret::GetKey() {
  CKey ret;
  assert(vchData.size() >= 32);
  ret.Set(vchData.begin(), vchData.begin() + 32,
          vchData.size() > 32 && vchData[32] == 1);
  return ret;
}

bool CBitcoinSecret::IsValid() const {
  bool fExpectedFormat =
      vchData.size() == 32 || (vchData.size() == 33 && vchData[32] == 1);
  bool fCorrectVersion =
      vchVersion == Params().Base58Prefix(CChainParams::SECRET_KEY);
  return fExpectedFormat && fCorrectVersion;
}

bool CBitcoinSecret::SetString(const char *pszSecret) {
  return CBase58Data::SetString(pszSecret) && IsValid();
}

bool CBitcoinSecret::SetString(const std::string &strSecret) {
  return SetString(strSecret.c_str());
}

std::string EncodeDestination(const CTxDestination &dest) {
  return boost::apply_visitor(DestinationEncoder(Params()), dest);
}

CTxDestination DecodeDestination(const std::string &str) {
  return DecodeDestination(str, Params());
}

bool IsValidDestinationString(const std::string &str,
                              const CChainParams &params) {
  return IsValidDestination(DecodeDestination(str, params));
}

bool IsValidDestinationString(const std::string &str) {
  return IsValidDestinationString(str, Params());
}
