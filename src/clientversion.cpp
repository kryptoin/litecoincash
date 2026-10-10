// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <clientversion.h>

#include <tinyformat.h>

const std::string CLIENT_NAME("LitecoinCashCore");

#define CLIENT_VERSION_SUFFIX ""

#ifdef HAVE_BUILD_INFO
#include <obj/build.h>
#endif

#ifdef GIT_ARCHIVE
#define GIT_COMMIT_ID "$Format:%h$"
#define GIT_COMMIT_DATE "$Format:%cD$"
#endif

#define BUILD_DESC_WITH_SUFFIX(maj, min, rev, build, suffix)                   \
  "v" DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(                \
      rev) "." DO_STRINGIZE(build) "-" DO_STRINGIZE(suffix)

#define BUILD_DESC_FROM_COMMIT(maj, min, rev, build, commit)                   \
  "v" DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(                \
      rev) "." DO_STRINGIZE(build) "-g" commit

#define BUILD_DESC_FROM_UNKNOWN(maj, min, rev, build)                          \
  "v" DO_STRINGIZE(maj) "." DO_STRINGIZE(min) "." DO_STRINGIZE(                \
      rev) "." DO_STRINGIZE(build) "-unk"

#ifndef BUILD_DESC
#ifdef BUILD_SUFFIX
#define BUILD_DESC                                                             \
  BUILD_DESC_WITH_SUFFIX(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,           \
                         CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD,        \
                         BUILD_SUFFIX)
#elif defined(GIT_COMMIT_ID)
#define BUILD_DESC                                                             \
  BUILD_DESC_FROM_COMMIT(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,           \
                         CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD,        \
                         GIT_COMMIT_ID)
#else
#define BUILD_DESC                                                             \
  BUILD_DESC_FROM_UNKNOWN(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR,          \
                          CLIENT_VERSION_REVISION, CLIENT_VERSION_BUILD)
#endif
#endif

const std::string CLIENT_BUILD(BUILD_DESC CLIENT_VERSION_SUFFIX);

static std::string FormatVersion(int nVersion) {
  if (nVersion % 100 == 0)
    return strprintf("%d.%d.%d", nVersion / 1000000, (nVersion / 10000) % 100,
                     (nVersion / 100) % 100);
  else
    return strprintf("%d.%d.%d.%d", nVersion / 1000000,
                     (nVersion / 10000) % 100, (nVersion / 100) % 100,
                     nVersion % 100);
}

std::string FormatFullVersion() { return CLIENT_BUILD; }

std::string FormatSubVersion(const std::string &name, int nClientVersion,
                             const std::vector<std::string> &comments) {
  std::ostringstream ss;
  ss << "/";
  ss << name << ":" << FormatVersion(nClientVersion);
  if (!comments.empty()) {
    std::vector<std::string>::const_iterator it(comments.begin());
    ss << "(" << *it;
    for (++it; it != comments.end(); ++it)
      ss << "; " << *it;
    ss << ")";
  }
  ss << "/";
  return ss.str();
}
