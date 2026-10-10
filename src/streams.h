// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STREAMS_H
#define BITCOIN_STREAMS_H

#include <serialize.h>
#include <support/allocators/zeroafterfree.h>

#include <algorithm>
#include <assert.h>
#include <ios>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <utility>
#include <vector>

template <typename Stream> class OverrideStream {
  Stream *stream;

  const int nType;
  const int nVersion;

public:
  OverrideStream(Stream *stream_, int nType_, int nVersion_)
      : stream(stream_), nType(nType_), nVersion(nVersion_) {}

  template <typename T> OverrideStream<Stream> &operator<<(const T &obj) {
    ::Serialize(*this, obj);
    return (*this);
  }

  template <typename T> OverrideStream<Stream> &operator>>(T &obj) {
    ::Unserialize(*this, obj);
    return (*this);
  }

  void write(const char *pch, size_t nSize) { stream->write(pch, nSize); }

  void read(char *pch, size_t nSize) { stream->read(pch, nSize); }

  int GetVersion() const { return nVersion; }
  int GetType() const { return nType; }
};

template <typename S> OverrideStream<S> WithOrVersion(S *s, int nVersionFlag) {
  return OverrideStream<S>(s, s->GetType(), s->GetVersion() | nVersionFlag);
}

class CVectorWriter {
public:
  CVectorWriter(int nTypeIn, int nVersionIn,
                std::vector<unsigned char> &vchDataIn, size_t nPosIn)
      : nType(nTypeIn), nVersion(nVersionIn), vchData(vchDataIn), nPos(nPosIn) {
    if (nPos > vchData.size())
      vchData.resize(nPos);
  }

  template <typename... Args>
  CVectorWriter(int nTypeIn, int nVersionIn,
                std::vector<unsigned char> &vchDataIn, size_t nPosIn,
                Args &&...args)
      : CVectorWriter(nTypeIn, nVersionIn, vchDataIn, nPosIn) {
    ::SerializeMany(*this, std::forward<Args>(args)...);
  }
  void write(const char *pch, size_t nSize) {
    assert(nPos <= vchData.size());
    size_t nOverwrite = std::min(nSize, vchData.size() - nPos);
    if (nOverwrite) {
      memcpy(vchData.data() + nPos,
             reinterpret_cast<const unsigned char *>(pch), nOverwrite);
    }
    if (nOverwrite < nSize) {
      vchData.insert(vchData.end(),
                     reinterpret_cast<const unsigned char *>(pch) + nOverwrite,
                     reinterpret_cast<const unsigned char *>(pch) + nSize);
    }
    nPos += nSize;
  }
  template <typename T> CVectorWriter &operator<<(const T &obj) {
    ::Serialize(*this, obj);
    return (*this);
  }
  int GetVersion() const { return nVersion; }
  int GetType() const { return nType; }
  void seek(size_t nSize) {
    nPos += nSize;
    if (nPos > vchData.size())
      vchData.resize(nPos);
  }

private:
  const int nType;
  const int nVersion;
  std::vector<unsigned char> &vchData;
  size_t nPos;
};

class CDataStream {
protected:
  typedef CSerializeData vector_type;
  vector_type vch;
  unsigned int nReadPos;

  int nType;
  int nVersion;

public:
  typedef vector_type::allocator_type allocator_type;
  typedef vector_type::size_type size_type;
  typedef vector_type::difference_type difference_type;
  typedef vector_type::reference reference;
  typedef vector_type::const_reference const_reference;
  typedef vector_type::value_type value_type;
  typedef vector_type::iterator iterator;
  typedef vector_type::const_iterator const_iterator;
  typedef vector_type::reverse_iterator reverse_iterator;

  explicit CDataStream(int nTypeIn, int nVersionIn) {
    Init(nTypeIn, nVersionIn);
  }

  CDataStream(const_iterator pbegin, const_iterator pend, int nTypeIn,
              int nVersionIn)
      : vch(pbegin, pend) {
    Init(nTypeIn, nVersionIn);
  }

  CDataStream(const char *pbegin, const char *pend, int nTypeIn, int nVersionIn)
      : vch(pbegin, pend) {
    Init(nTypeIn, nVersionIn);
  }

  CDataStream(const vector_type &vchIn, int nTypeIn, int nVersionIn)
      : vch(vchIn.begin(), vchIn.end()) {
    Init(nTypeIn, nVersionIn);
  }

  CDataStream(const std::vector<char> &vchIn, int nTypeIn, int nVersionIn)
      : vch(vchIn.begin(), vchIn.end()) {
    Init(nTypeIn, nVersionIn);
  }

  CDataStream(const std::vector<unsigned char> &vchIn, int nTypeIn,
              int nVersionIn)
      : vch(vchIn.begin(), vchIn.end()) {
    Init(nTypeIn, nVersionIn);
  }

  template <typename... Args>
  CDataStream(int nTypeIn, int nVersionIn, Args &&...args) {
    Init(nTypeIn, nVersionIn);
    ::SerializeMany(*this, std::forward<Args>(args)...);
  }

  void Init(int nTypeIn, int nVersionIn) {
    nReadPos = 0;
    nType = nTypeIn;
    nVersion = nVersionIn;
  }

  CDataStream &operator+=(const CDataStream &b) {
    vch.insert(vch.end(), b.begin(), b.end());
    return *this;
  }

  friend CDataStream operator+(const CDataStream &a, const CDataStream &b) {
    CDataStream ret = a;
    ret += b;
    return (ret);
  }

  std::string str() const { return (std::string(begin(), end())); }

  const_iterator begin() const { return vch.begin() + nReadPos; }
  iterator begin() { return vch.begin() + nReadPos; }
  const_iterator end() const { return vch.end(); }
  iterator end() { return vch.end(); }
  size_type size() const { return vch.size() - nReadPos; }
  bool empty() const { return vch.size() == nReadPos; }
  void resize(size_type n, value_type c = 0) { vch.resize(n + nReadPos, c); }
  void reserve(size_type n) { vch.reserve(n + nReadPos); }
  const_reference operator[](size_type pos) const {
    return vch[pos + nReadPos];
  }
  reference operator[](size_type pos) { return vch[pos + nReadPos]; }
  void clear() {
    vch.clear();
    nReadPos = 0;
  }
  iterator insert(iterator it, const char x = char()) {
    return vch.insert(it, x);
  }
  void insert(iterator it, size_type n, const char x) { vch.insert(it, n, x); }
  value_type *data() { return vch.data() + nReadPos; }
  const value_type *data() const { return vch.data() + nReadPos; }

  void insert(iterator it, std::vector<char>::const_iterator first,
              std::vector<char>::const_iterator last) {
    if (last == first)
      return;
    assert(last - first > 0);
    if (it == vch.begin() + nReadPos &&
        (unsigned int)(last - first) <= nReadPos) {
      nReadPos -= (last - first);
      memcpy(&vch[nReadPos], &first[0], last - first);
    } else
      vch.insert(it, first, last);
  }

  void insert(iterator it, const char *first, const char *last) {
    if (last == first)
      return;
    assert(last - first > 0);
    if (it == vch.begin() + nReadPos &&
        (unsigned int)(last - first) <= nReadPos) {
      nReadPos -= (last - first);
      memcpy(&vch[nReadPos], &first[0], last - first);
    } else
      vch.insert(it, first, last);
  }

  iterator erase(iterator it) {
    if (it == vch.begin() + nReadPos) {
      if (++nReadPos >= vch.size()) {
        nReadPos = 0;
        return vch.erase(vch.begin(), vch.end());
      }
      return vch.begin() + nReadPos;
    } else
      return vch.erase(it);
  }

  iterator erase(iterator first, iterator last) {
    if (first == vch.begin() + nReadPos) {
      if (last == vch.end()) {
        nReadPos = 0;
        return vch.erase(vch.begin(), vch.end());
      } else {
        nReadPos = (last - vch.begin());
        return last;
      }
    } else
      return vch.erase(first, last);
  }

  inline void Compact() {
    vch.erase(vch.begin(), vch.begin() + nReadPos);
    nReadPos = 0;
  }

  bool Rewind(size_type n) {
    if (n > nReadPos)
      return false;
    nReadPos -= n;
    return true;
  }

  bool eof() const { return size() == 0; }
  CDataStream *rdbuf() { return this; }
  int in_avail() const { return size(); }

  void SetType(int n) { nType = n; }
  int GetType() const { return nType; }
  void SetVersion(int n) { nVersion = n; }
  int GetVersion() const { return nVersion; }

  void read(char *pch, size_t nSize) {
    if (nSize == 0)
      return;

    unsigned int nReadPosNext = nReadPos + nSize;
    if (nReadPosNext > vch.size()) {
      throw std::ios_base::failure("CDataStream::read(): end of data");
    }
    memcpy(pch, &vch[nReadPos], nSize);
    if (nReadPosNext == vch.size()) {
      nReadPos = 0;
      vch.clear();
      return;
    }
    nReadPos = nReadPosNext;
  }

  void ignore(int nSize) {
    if (nSize < 0) {
      throw std::ios_base::failure("CDataStream::ignore(): nSize negative");
    }
    unsigned int nReadPosNext = nReadPos + nSize;
    if (nReadPosNext >= vch.size()) {
      if (nReadPosNext > vch.size())
        throw std::ios_base::failure("CDataStream::ignore(): end of data");
      nReadPos = 0;
      vch.clear();
      return;
    }
    nReadPos = nReadPosNext;
  }

  void write(const char *pch, size_t nSize) {
    vch.insert(vch.end(), pch, pch + nSize);
  }

  template <typename Stream> void Serialize(Stream &s) const {
    if (!vch.empty())
      s.write((char *)vch.data(), vch.size() * sizeof(value_type));
  }

  template <typename T> CDataStream &operator<<(const T &obj) {
    ::Serialize(*this, obj);
    return (*this);
  }

  template <typename T> CDataStream &operator>>(T &obj) {
    ::Unserialize(*this, obj);
    return (*this);
  }

  void GetAndClear(CSerializeData &d) {
    d.insert(d.end(), begin(), end());
    clear();
  }

  void Xor(const std::vector<unsigned char> &key) {
    if (key.size() == 0) {
      return;
    }

    for (size_type i = 0, j = 0; i != size(); i++) {
      vch[i] ^= key[j++];

      if (j == key.size())
        j = 0;
    }
  }
};

class CAutoFile {
private:
  const int nType;
  const int nVersion;

  FILE *file;

public:
  CAutoFile(FILE *filenew, int nTypeIn, int nVersionIn)
      : nType(nTypeIn), nVersion(nVersionIn) {
    file = filenew;
  }

  ~CAutoFile() { fclose(); }

  CAutoFile(const CAutoFile &) = delete;
  CAutoFile &operator=(const CAutoFile &) = delete;

  void fclose() {
    if (file) {
      ::fclose(file);
      file = nullptr;
    }
  }

  FILE *release() {
    FILE *ret = file;
    file = nullptr;
    return ret;
  }

  FILE *Get() const { return file; }

  bool IsNull() const { return (file == nullptr); }

  int GetType() const { return nType; }
  int GetVersion() const { return nVersion; }

  void read(char *pch, size_t nSize) {
    if (!file)
      throw std::ios_base::failure("CAutoFile::read: file handle is nullptr");
    if (fread(pch, 1, nSize, file) != nSize)
      throw std::ios_base::failure(feof(file)
                                       ? "CAutoFile::read: end of file"
                                       : "CAutoFile::read: fread failed");
  }

  void ignore(size_t nSize) {
    if (!file)
      throw std::ios_base::failure("CAutoFile::ignore: file handle is nullptr");
    unsigned char data[4096];
    while (nSize > 0) {
      size_t nNow = std::min<size_t>(nSize, sizeof(data));
      if (fread(data, 1, nNow, file) != nNow)
        throw std::ios_base::failure(feof(file)
                                         ? "CAutoFile::ignore: end of file"
                                         : "CAutoFile::read: fread failed");
      nSize -= nNow;
    }
  }

  void write(const char *pch, size_t nSize) {
    if (!file)
      throw std::ios_base::failure("CAutoFile::write: file handle is nullptr");
    if (fwrite(pch, 1, nSize, file) != nSize)
      throw std::ios_base::failure("CAutoFile::write: write failed");
  }

  template <typename T> CAutoFile &operator<<(const T &obj) {
    if (!file)
      throw std::ios_base::failure(
          "CAutoFile::operator<<: file handle is nullptr");
    ::Serialize(*this, obj);
    return (*this);
  }

  template <typename T> CAutoFile &operator>>(T &obj) {
    if (!file)
      throw std::ios_base::failure(
          "CAutoFile::operator>>: file handle is nullptr");
    ::Unserialize(*this, obj);
    return (*this);
  }
};

class CBufferedFile {
private:
  const int nType;
  const int nVersion;

  FILE *src;

  uint64_t nSrcPos;

  uint64_t nReadPos;

  uint64_t nReadLimit;

  uint64_t nRewind;

  std::vector<char> vchBuf;

protected:
  bool Fill() {
    unsigned int pos = nSrcPos % vchBuf.size();
    unsigned int readNow = vchBuf.size() - pos;
    unsigned int nAvail = vchBuf.size() - (nSrcPos - nReadPos) - nRewind;
    if (nAvail < readNow)
      readNow = nAvail;
    if (readNow == 0)
      return false;
    size_t nBytes = fread((void *)&vchBuf[pos], 1, readNow, src);
    if (nBytes == 0) {
      throw std::ios_base::failure(feof(src)
                                       ? "CBufferedFile::Fill: end of file"
                                       : "CBufferedFile::Fill: fread failed");
    } else {
      nSrcPos += nBytes;
      return true;
    }
  }

public:
  CBufferedFile(FILE *fileIn, uint64_t nBufSize, uint64_t nRewindIn,
                int nTypeIn, int nVersionIn)
      : nType(nTypeIn), nVersion(nVersionIn), nSrcPos(0), nReadPos(0),
        nReadLimit((uint64_t)(-1)), nRewind(nRewindIn), vchBuf(nBufSize, 0) {
    src = fileIn;
  }

  ~CBufferedFile() { fclose(); }

  CBufferedFile(const CBufferedFile &) = delete;
  CBufferedFile &operator=(const CBufferedFile &) = delete;

  int GetVersion() const { return nVersion; }
  int GetType() const { return nType; }

  void fclose() {
    if (src) {
      ::fclose(src);
      src = nullptr;
    }
  }

  bool eof() const { return nReadPos == nSrcPos && feof(src); }

  void read(char *pch, size_t nSize) {
    if (nSize + nReadPos > nReadLimit)
      throw std::ios_base::failure("Read attempted past buffer limit");
    if (nSize + nRewind > vchBuf.size())
      throw std::ios_base::failure("Read larger than buffer size");
    while (nSize > 0) {
      if (nReadPos == nSrcPos)
        Fill();
      unsigned int pos = nReadPos % vchBuf.size();
      size_t nNow = nSize;
      if (nNow + pos > vchBuf.size())
        nNow = vchBuf.size() - pos;
      if (nNow + nReadPos > nSrcPos)
        nNow = nSrcPos - nReadPos;
      memcpy(pch, &vchBuf[pos], nNow);
      nReadPos += nNow;
      pch += nNow;
      nSize -= nNow;
    }
  }

  uint64_t GetPos() const { return nReadPos; }

  bool SetPos(uint64_t nPos) {
    nReadPos = nPos;
    if (nReadPos + nRewind < nSrcPos) {
      nReadPos = nSrcPos - nRewind;
      return false;
    } else if (nReadPos > nSrcPos) {
      nReadPos = nSrcPos;
      return false;
    } else {
      return true;
    }
  }

  bool Seek(uint64_t nPos) {
    long nLongPos = nPos;
    if (nPos != (uint64_t)nLongPos)
      return false;
    if (fseek(src, nLongPos, SEEK_SET))
      return false;
    nLongPos = ftell(src);
    nSrcPos = nLongPos;
    nReadPos = nLongPos;
    return true;
  }

  bool SetLimit(uint64_t nPos = (uint64_t)(-1)) {
    if (nPos < nReadPos)
      return false;
    nReadLimit = nPos;
    return true;
  }

  template <typename T> CBufferedFile &operator>>(T &obj) {
    ::Unserialize(*this, obj);
    return (*this);
  }

  void FindByte(char ch) {
    while (true) {
      if (nReadPos == nSrcPos)
        Fill();
      if (vchBuf[nReadPos % vchBuf.size()] == ch)
        break;
      nReadPos++;
    }
  }
};

#endif
