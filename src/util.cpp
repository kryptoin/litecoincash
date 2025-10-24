// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util.h>

#include <chainparamsbase.h>
#include <random.h>
#include <serialize.h>
#include <utilstrencodings.h>

#include <stdarg.h>

#if (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
#include <pthread.h>
#include <pthread_np.h>
#endif

#ifndef WIN32

#ifdef __linux__

#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif

#define _POSIX_C_SOURCE 200112L

#endif

#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

#else

#ifdef _MSC_VER
#pragma warning(disable : 4786)
#pragma warning(disable : 4804)
#pragma warning(disable : 4805)
#pragma warning(disable : 4717)
#endif

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501

#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <io.h>

#include <shlobj.h>
#endif

#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#ifdef HAVE_MALLOPT_ARENA_MAX
#include <malloc.h>
#endif

#include <boost/algorithm/string/case_conv.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/program_options/detail/config_file.hpp>
#include <boost/thread.hpp>
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/rand.h>

const int64_t nStartupTime = GetTime();

const char *const BITCOIN_CONF_FILENAME = "litecoincash.conf";
const char *const BITCOIN_PID_FILENAME = "litecoincashd.pid";
const char *const DEFAULT_DEBUGLOGFILE = "debug.log";

ArgsManager gArgs;
bool fPrintToConsole = false;
bool fPrintToDebugLog = true;

bool fLogTimestamps = DEFAULT_LOGTIMESTAMPS;
bool fLogTimeMicros = DEFAULT_LOGTIMEMICROS;
bool fLogIPs = DEFAULT_LOGIPS;
std::atomic<bool> fReopenDebugLog(false);
CTranslationInterface translationInterface;

std::atomic<uint32_t> logCategories(0);

static std::unique_ptr<CCriticalSection[]> ppmutexOpenSSL;
void locking_callback(int mode, int i, const char *file,
                      int line) NO_THREAD_SAFETY_ANALYSIS {
  if (mode & CRYPTO_LOCK) {
    ENTER_CRITICAL_SECTION(ppmutexOpenSSL[i]);
  } else {
    LEAVE_CRITICAL_SECTION(ppmutexOpenSSL[i]);
  }
}

class CInit {
public:
  CInit() {
    ppmutexOpenSSL.reset(new CCriticalSection[CRYPTO_num_locks()]);
    CRYPTO_set_locking_callback(locking_callback);

    OPENSSL_no_config();

#ifdef WIN32

    RAND_screen();
#endif

    RandAddSeed();
  }
  ~CInit() {
    RAND_cleanup();

    CRYPTO_set_locking_callback(nullptr);

    ppmutexOpenSSL.reset();
  }
} instance_of_cinit;

static boost::once_flag debugPrintInitFlag = BOOST_ONCE_INIT;

static FILE *fileout = nullptr;
static boost::mutex *mutexDebugLog = nullptr;
static std::list<std::string> *vMsgsBeforeOpenLog;

static int FileWriteStr(const std::string &str, FILE *fp) {
  return fwrite(str.data(), 1, str.size(), fp);
}

static void DebugPrintInit() {
  assert(mutexDebugLog == nullptr);
  mutexDebugLog = new boost::mutex();
  vMsgsBeforeOpenLog = new std::list<std::string>;
}

fs::path GetDebugLogPath() {
  fs::path logfile(gArgs.GetArg("-debuglogfile", DEFAULT_DEBUGLOGFILE));
  if (logfile.is_absolute()) {
    return logfile;
  } else {
    return GetDataDir() / logfile;
  }
}

bool OpenDebugLog() {
  boost::call_once(&DebugPrintInit, debugPrintInitFlag);
  boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);

  assert(fileout == nullptr);
  assert(vMsgsBeforeOpenLog);
  fs::path pathDebug = GetDebugLogPath();

  fileout = fsbridge::fopen(pathDebug, "a");
  if (!fileout) {
    return false;
  }

  setbuf(fileout, nullptr);

  while (!vMsgsBeforeOpenLog->empty()) {
    FileWriteStr(vMsgsBeforeOpenLog->front(), fileout);
    vMsgsBeforeOpenLog->pop_front();
  }

  delete vMsgsBeforeOpenLog;
  vMsgsBeforeOpenLog = nullptr;
  return true;
}

struct CLogCategoryDesc {
  uint32_t flag;
  std::string category;
};

const CLogCategoryDesc LogCategories[] = {
    {BCLog::NONE, "0"},
    {BCLog::NONE, "none"},
    {BCLog::NET, "net"},
    {BCLog::TOR, "tor"},
    {BCLog::MEMPOOL, "mempool"},
    {BCLog::HTTP, "http"},
    {BCLog::BENCH, "bench"},
    {BCLog::ZMQ, "zmq"},
    {BCLog::DB, "db"},
    {BCLog::RPC, "rpc"},
    {BCLog::ESTIMATEFEE, "estimatefee"},
    {BCLog::ADDRMAN, "addrman"},
    {BCLog::SELECTCOINS, "selectcoins"},
    {BCLog::REINDEX, "reindex"},
    {BCLog::CMPCTBLOCK, "cmpctblock"},
    {BCLog::RAND, "rand"},
    {BCLog::PRUNE, "prune"},
    {BCLog::PROXY, "proxy"},
    {BCLog::MEMPOOLREJ, "mempoolrej"},
    {BCLog::LIBEVENT, "libevent"},
    {BCLog::COINDB, "coindb"},
    {BCLog::QT, "qt"},
    {BCLog::LEVELDB, "leveldb"},
    {BCLog::HIVE, "hive"},

    {BCLog::MINOTAURX, "minotaurx"},

    {BCLog::RIALTO, "rialto"},

    {BCLog::ALL, "1"},
    {BCLog::ALL, "all"},
};

bool GetLogCategory(uint32_t *f, const std::string *str) {
  if (f && str) {
    if (*str == "") {
      *f = BCLog::ALL;
      return true;
    }
    for (unsigned int i = 0; i < ARRAYLEN(LogCategories); i++) {
      if (LogCategories[i].category == *str) {
        *f = LogCategories[i].flag;
        return true;
      }
    }
  }
  return false;
}

std::string ListLogCategories() {
  std::string ret;
  int outcount = 0;
  for (unsigned int i = 0; i < ARRAYLEN(LogCategories); i++) {
    if (LogCategories[i].flag != BCLog::NONE &&
        LogCategories[i].flag != BCLog::ALL) {
      if (outcount != 0)
        ret += ", ";
      ret += LogCategories[i].category;
      outcount++;
    }
  }
  return ret;
}

std::vector<CLogCategoryActive> ListActiveLogCategories() {
  std::vector<CLogCategoryActive> ret;
  for (unsigned int i = 0; i < ARRAYLEN(LogCategories); i++) {
    if (LogCategories[i].flag != BCLog::NONE &&
        LogCategories[i].flag != BCLog::ALL) {
      CLogCategoryActive catActive;
      catActive.category = LogCategories[i].category;
      catActive.active = LogAcceptCategory(LogCategories[i].flag);
      ret.push_back(catActive);
    }
  }
  return ret;
}

static std::string LogTimestampStr(const std::string &str,
                                   std::atomic_bool *fStartedNewLine) {
  std::string strStamped;

  if (!fLogTimestamps)
    return str;

  if (*fStartedNewLine) {
    int64_t nTimeMicros = GetTimeMicros();
    strStamped = DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nTimeMicros / 1000000);
    if (fLogTimeMicros)
      strStamped += strprintf(".%06d", nTimeMicros % 1000000);
    int64_t mocktime = GetMockTime();
    if (mocktime) {
      strStamped +=
          " (mocktime: " + DateTimeStrFormat("%Y-%m-%d %H:%M:%S", mocktime) +
          ")";
    }
    strStamped += ' ' + str;
  } else
    strStamped = str;

  if (!str.empty() && str[str.size() - 1] == '\n')
    *fStartedNewLine = true;
  else
    *fStartedNewLine = false;

  return strStamped;
}

int LogPrintStr(const std::string &str) {
  int ret = 0;

  static std::atomic_bool fStartedNewLine(true);

  std::string strTimestamped = LogTimestampStr(str, &fStartedNewLine);

  if (fPrintToConsole) {
    ret = fwrite(strTimestamped.data(), 1, strTimestamped.size(), stdout);
    fflush(stdout);
  } else if (fPrintToDebugLog) {
    boost::call_once(&DebugPrintInit, debugPrintInitFlag);
    boost::mutex::scoped_lock scoped_lock(*mutexDebugLog);

    if (fileout == nullptr) {
      assert(vMsgsBeforeOpenLog);
      ret = strTimestamped.length();
      vMsgsBeforeOpenLog->push_back(strTimestamped);
    } else {
      if (fReopenDebugLog) {
        fReopenDebugLog = false;
        fs::path pathDebug = GetDebugLogPath();
        if (fsbridge::freopen(pathDebug, "a", fileout) != nullptr)
          setbuf(fileout, nullptr);
      }

      ret = FileWriteStr(strTimestamped, fileout);
    }
  }
  return ret;
}

static std::map<std::string, std::unique_ptr<boost::interprocess::file_lock>>
    dir_locks;

static std::mutex cs_dir_locks;

bool LockDirectory(const fs::path &directory, const std::string lockfile_name,
                   bool probe_only) {
  std::lock_guard<std::mutex> ulock(cs_dir_locks);
  fs::path pathLockFile = directory / lockfile_name;

  if (dir_locks.count(pathLockFile.string())) {
    return true;
  }

  FILE *file = fsbridge::fopen(pathLockFile, "a");
  if (file)
    fclose(file);

  try {
    auto lock = MakeUnique<boost::interprocess::file_lock>(
        pathLockFile.string().c_str());
    if (!lock->try_lock()) {
      return false;
    }
    if (!probe_only) {
      dir_locks.emplace(pathLockFile.string(), std::move(lock));
    }
  } catch (const boost::interprocess::interprocess_exception &e) {
    return error("Error while attempting to lock directory %s: %s",
                 directory.string(), e.what());
  }
  return true;
}

void ReleaseDirectoryLocks() {
  std::lock_guard<std::mutex> ulock(cs_dir_locks);
  dir_locks.clear();
}

static bool InterpretBool(const std::string &strValue) {
  if (strValue.empty())
    return true;
  return (atoi(strValue) != 0);
}

static void InterpretNegativeSetting(std::string &strKey,
                                     std::string &strValue) {
  if (strKey.length() > 3 && strKey[0] == '-' && strKey[1] == 'n' &&
      strKey[2] == 'o') {
    strKey = "-" + strKey.substr(3);
    strValue = InterpretBool(strValue) ? "0" : "1";
  }
}

void ArgsManager::ParseParameters(int argc, const char *const argv[]) {
  LOCK(cs_args);
  mapArgs.clear();
  mapMultiArgs.clear();

  for (int i = 1; i < argc; i++) {
    std::string str(argv[i]);
    std::string strValue;
    size_t is_index = str.find('=');
    if (is_index != std::string::npos) {
      strValue = str.substr(is_index + 1);
      str = str.substr(0, is_index);
    }
#ifdef WIN32
    boost::to_lower(str);
    if (boost::algorithm::starts_with(str, "/"))
      str = "-" + str.substr(1);
#endif

    if (str[0] != '-')
      break;

    if (str.length() > 1 && str[1] == '-')
      str = str.substr(1);
    InterpretNegativeSetting(str, strValue);

    mapArgs[str] = strValue;
    mapMultiArgs[str].push_back(strValue);
  }
}

std::vector<std::string> ArgsManager::GetArgs(const std::string &strArg) const {
  LOCK(cs_args);
  auto it = mapMultiArgs.find(strArg);
  if (it != mapMultiArgs.end())
    return it->second;
  return {};
}

bool ArgsManager::IsArgSet(const std::string &strArg) const {
  LOCK(cs_args);
  return mapArgs.count(strArg);
}

std::string ArgsManager::GetArg(const std::string &strArg,
                                const std::string &strDefault) const {
  LOCK(cs_args);
  auto it = mapArgs.find(strArg);
  if (it != mapArgs.end())
    return it->second;
  return strDefault;
}

int64_t ArgsManager::GetArg(const std::string &strArg, int64_t nDefault) const {
  LOCK(cs_args);
  auto it = mapArgs.find(strArg);
  if (it != mapArgs.end())
    return atoi64(it->second);
  return nDefault;
}

bool ArgsManager::GetBoolArg(const std::string &strArg, bool fDefault) const {
  LOCK(cs_args);
  auto it = mapArgs.find(strArg);
  if (it != mapArgs.end())
    return InterpretBool(it->second);
  return fDefault;
}

bool ArgsManager::SoftSetArg(const std::string &strArg,
                             const std::string &strValue) {
  LOCK(cs_args);
  if (IsArgSet(strArg))
    return false;
  ForceSetArg(strArg, strValue);
  return true;
}

bool ArgsManager::SoftSetBoolArg(const std::string &strArg, bool fValue) {
  if (fValue)
    return SoftSetArg(strArg, std::string("1"));
  else
    return SoftSetArg(strArg, std::string("0"));
}

void ArgsManager::ForceSetArg(const std::string &strArg,
                              const std::string &strValue) {
  LOCK(cs_args);
  mapArgs[strArg] = strValue;
  mapMultiArgs[strArg] = {strValue};
}

static const int screenWidth = 79;
static const int optIndent = 2;
static const int msgIndent = 7;

std::string HelpMessageGroup(const std::string &message) {
  return std::string(message) + std::string("\n\n");
}

std::string HelpMessageOpt(const std::string &option,
                           const std::string &message) {
  return std::string(optIndent, ' ') + std::string(option) + std::string("\n") +
         std::string(msgIndent, ' ') +
         FormatParagraph(message, screenWidth - msgIndent, msgIndent) +
         std::string("\n\n");
}

static std::string FormatException(const std::exception *pex,
                                   const char *pszThread) {
#ifdef WIN32
  char pszModule[MAX_PATH] = "";
  GetModuleFileNameA(nullptr, pszModule, sizeof(pszModule));
#else
  const char *pszModule = "litecoincash";
#endif
  if (pex)
    return strprintf("EXCEPTION: %s       \n%s       \n%s in %s       \n",
                     typeid(*pex).name(), pex->what(), pszModule, pszThread);
  else
    return strprintf("UNKNOWN EXCEPTION       \n%s in %s       \n", pszModule,
                     pszThread);
}

void PrintExceptionContinue(const std::exception *pex, const char *pszThread) {
  std::string message = FormatException(pex, pszThread);
  LogPrintf("\n\n************************\n%s\n", message);
  fprintf(stderr, "\n\n************************\n%s\n", message.c_str());
}

fs::path GetDefaultDataDir() {
#ifdef WIN32

  return GetSpecialFolderPath(CSIDL_APPDATA) / "LitecoinCash";
#else
  fs::path pathRet;
  char *pszHome = getenv("HOME");
  if (pszHome == nullptr || strlen(pszHome) == 0)
    pathRet = fs::path("/");
  else
    pathRet = fs::path(pszHome);
#ifdef MAC_OSX

  return pathRet / "Library/Application Support/LitecoinCash";
#else

  return pathRet / ".litecoincash";
#endif
#endif
}

static fs::path pathCached;
static fs::path pathCachedNetSpecific;
static CCriticalSection csPathCached;

const fs::path &GetDataDir(bool fNetSpecific) {
  LOCK(csPathCached);

  fs::path &path = fNetSpecific ? pathCachedNetSpecific : pathCached;

  if (!path.empty())
    return path;

  if (gArgs.IsArgSet("-datadir")) {
    path = fs::system_complete(gArgs.GetArg("-datadir", ""));
    if (!fs::is_directory(path)) {
      path = "";
      return path;
    }
  } else {
    path = GetDefaultDataDir();
  }
  if (fNetSpecific)
    path /= BaseParams().DataDir();

  if (fs::create_directories(path)) {
    fs::create_directories(path / "wallets");
  }

  return path;
}

void ClearDatadirCache() {
  LOCK(csPathCached);

  pathCached = fs::path();
  pathCachedNetSpecific = fs::path();
}

fs::path GetConfigFile(const std::string &confPath) {
  fs::path pathConfigFile(confPath);
  if (!pathConfigFile.is_absolute())
    pathConfigFile = GetDataDir(false) / pathConfigFile;

  return pathConfigFile;
}

void ArgsManager::ReadConfigFile(const std::string &confPath) {
  fs::ifstream streamConfig(GetConfigFile(confPath));
  if (!streamConfig.good())
    return;

  {
    LOCK(cs_args);
    std::set<std::string> setOptions;
    setOptions.insert("*");

    for (boost::program_options::detail::config_file_iterator
             it(streamConfig, setOptions),
         end;
         it != end; ++it) {
      std::string strKey = std::string("-") + it->string_key;
      std::string strValue = it->value[0];
      InterpretNegativeSetting(strKey, strValue);
      if (mapArgs.count(strKey) == 0)
        mapArgs[strKey] = strValue;
      mapMultiArgs[strKey].push_back(strValue);
    }
  }

  ClearDatadirCache();
  if (!fs::is_directory(GetDataDir(false))) {
    throw std::runtime_error(
        strprintf("specified data directory \"%s\" does not exist.",
                  gArgs.GetArg("-datadir", "").c_str()));
  }
}

#ifndef WIN32
fs::path GetPidFile() {
  fs::path pathPidFile(gArgs.GetArg("-pid", BITCOIN_PID_FILENAME));
  if (!pathPidFile.is_absolute())
    pathPidFile = GetDataDir() / pathPidFile;
  return pathPidFile;
}

void CreatePidFile(const fs::path &path, pid_t pid) {
  FILE *file = fsbridge::fopen(path, "w");
  if (file) {
    fprintf(file, "%d\n", pid);
    fclose(file);
  }
}
#endif

bool RenameOver(fs::path src, fs::path dest) {
#ifdef WIN32
  return MoveFileExA(src.string().c_str(), dest.string().c_str(),
                     MOVEFILE_REPLACE_EXISTING) != 0;
#else
  int rc = std::rename(src.string().c_str(), dest.string().c_str());
  return (rc == 0);
#endif
}

bool TryCreateDirectories(const fs::path &p) {
  try {
    return fs::create_directories(p);
  } catch (const fs::filesystem_error &) {
    if (!fs::exists(p) || !fs::is_directory(p))
      throw;
  }

  return false;
}

void FileCommit(FILE *file) {
  fflush(file);

#ifdef WIN32
  HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
  FlushFileBuffers(hFile);
#else
#if defined(__linux__) || defined(__NetBSD__)
  fdatasync(fileno(file));
#elif defined(__APPLE__) && defined(F_FULLFSYNC)
  fcntl(fileno(file), F_FULLFSYNC, 0);
#else
  fsync(fileno(file));
#endif
#endif
}

bool TruncateFile(FILE *file, unsigned int length) {
#if defined(WIN32)
  return _chsize(_fileno(file), length) == 0;
#else
  return ftruncate(fileno(file), length) == 0;
#endif
}

int RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
  return 2048;
#else
  struct rlimit limitFD;
  if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
    if (limitFD.rlim_cur < (rlim_t)nMinFD) {
      limitFD.rlim_cur = nMinFD;
      if (limitFD.rlim_cur > limitFD.rlim_max)
        limitFD.rlim_cur = limitFD.rlim_max;
      setrlimit(RLIMIT_NOFILE, &limitFD);
      getrlimit(RLIMIT_NOFILE, &limitFD);
    }
    return limitFD.rlim_cur;
  }
  return nMinFD;

#endif
}

void AllocateFileRange(FILE *file, unsigned int offset, unsigned int length) {
#if defined(WIN32)

  HANDLE hFile = (HANDLE)_get_osfhandle(_fileno(file));
  LARGE_INTEGER nFileSize;
  int64_t nEndPos = (int64_t)offset + length;
  nFileSize.u.LowPart = nEndPos & 0xFFFFFFFF;
  nFileSize.u.HighPart = nEndPos >> 32;
  SetFilePointerEx(hFile, nFileSize, 0, FILE_BEGIN);
  SetEndOfFile(hFile);
#elif defined(MAC_OSX)

  fstore_t fst;
  fst.fst_flags = F_ALLOCATECONTIG;
  fst.fst_posmode = F_PEOFPOSMODE;
  fst.fst_offset = 0;
  fst.fst_length = (off_t)offset + length;
  fst.fst_bytesalloc = 0;
  if (fcntl(fileno(file), F_PREALLOCATE, &fst) == -1) {
    fst.fst_flags = F_ALLOCATEALL;
    fcntl(fileno(file), F_PREALLOCATE, &fst);
  }
  ftruncate(fileno(file), fst.fst_length);
#elif defined(__linux__)

  off_t nEndPos = (off_t)offset + length;
  posix_fallocate(fileno(file), 0, nEndPos);
#else

  static const char buf[65536] = {};
  fseek(file, offset, SEEK_SET);
  while (length > 0) {
    unsigned int now = 65536;
    if (length < now)
      now = length;
    fwrite(buf, 1, now, file);

    length -= now;
  }
#endif
}

void ShrinkDebugFile() {
  constexpr size_t RECENT_DEBUG_HISTORY_SIZE = 10 * 1000000;

  fs::path pathLog = GetDebugLogPath();
  FILE *file = fsbridge::fopen(pathLog, "r");

  if (file && fs::file_size(pathLog) > 11 * (RECENT_DEBUG_HISTORY_SIZE / 10)) {
    std::vector<char> vch(RECENT_DEBUG_HISTORY_SIZE, 0);
    fseek(file, -((long)vch.size()), SEEK_END);
    int nBytes = fread(vch.data(), 1, vch.size(), file);
    fclose(file);

    file = fsbridge::fopen(pathLog, "w");
    if (file) {
      fwrite(vch.data(), 1, nBytes, file);
      fclose(file);
    }
  } else if (file != nullptr)
    fclose(file);
}

#ifdef WIN32
fs::path GetSpecialFolderPath(int nFolder, bool fCreate) {
  char pszPath[MAX_PATH] = "";

  if (SHGetSpecialFolderPathA(nullptr, pszPath, nFolder, fCreate)) {
    return fs::path(pszPath);
  }

  LogPrintf(
      "SHGetSpecialFolderPathA() failed, could not obtain requested path.\n");
  return fs::path("");
}
#endif

void runCommand(const std::string &strCommand) {
  if (strCommand.empty())
    return;
  int nErr = ::system(strCommand.c_str());
  if (nErr)
    LogPrintf("runCommand error: system(%s) returned %d\n", strCommand, nErr);
}

void RenameThread(const char *name) {
#if defined(PR_SET_NAME)

  ::prctl(PR_SET_NAME, name, 0, 0, 0);
#elif (defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__DragonFly__))
  pthread_set_name_np(pthread_self(), name);

#elif defined(MAC_OSX)
  pthread_setname_np(name);
#else

  (void)name;
#endif
}

void SetupEnvironment() {
#ifdef HAVE_MALLOPT_ARENA_MAX

  if (sizeof(void *) == 4) {
    mallopt(M_ARENA_MAX, 1);
  }
#endif

#if !defined(WIN32) && !defined(MAC_OSX) && !defined(__FreeBSD__) &&           \
    !defined(__OpenBSD__)
  try {
    std::locale("");

  } catch (const std::runtime_error &) {
    setenv("LC_ALL", "C", 1);
  }
#endif

  std::locale loc = fs::path::imbue(std::locale::classic());
  fs::path::imbue(loc);
}

bool SetupNetworking() {
#ifdef WIN32

  WSADATA wsadata;
  int ret = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (ret != NO_ERROR || LOBYTE(wsadata.wVersion) != 2 ||
      HIBYTE(wsadata.wVersion) != 2)
    return false;
#endif
  return true;
}

int GetNumVirtualCores() { return boost::thread::hardware_concurrency(); }

int GetNumCores() {
#if BOOST_VERSION >= 105600
  return boost::thread::physical_concurrency();
#else

  return boost::thread::hardware_concurrency();
#endif
}

std::string CopyrightHolders(const std::string &strPrefix) {
  std::string strFirstPrefix = strPrefix;
  strFirstPrefix.replace(strFirstPrefix.find("2011-"), sizeof("2011-") - 1,
                         "2018-");
  std::string strCopyrightHolders =
      strFirstPrefix +
      strprintf(_(COPYRIGHT_HOLDERS), _(COPYRIGHT_HOLDERS_SUBSTITUTION));

  if (strprintf(COPYRIGHT_HOLDERS, COPYRIGHT_HOLDERS_SUBSTITUTION)
          .find("Litecoin Core") == std::string::npos)
    strCopyrightHolders += "\n" + strPrefix + "The Litecoin Core developers";

  if (strprintf(COPYRIGHT_HOLDERS, COPYRIGHT_HOLDERS_SUBSTITUTION)
          .find("Bitcoin Core") == std::string::npos) {
    std::string strYear = strPrefix;
    strYear.replace(strYear.find("2011"), sizeof("2011") - 1, "2009");
    strCopyrightHolders += "\n" + strYear + "The Bitcoin Core developers";
  }
  return strCopyrightHolders;
}

int64_t GetStartupTime() { return nStartupTime; }
