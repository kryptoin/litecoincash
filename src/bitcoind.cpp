// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <chainparams.h>
#include <clientversion.h>
#include <compat.h>
#include <fs.h>
#include <httprpc.h>
#include <httpserver.h>
#include <init.h>
#include <noui.h>
#include <rpc/server.h>
#include <util.h>
#include <utilstrencodings.h>

#include <boost/thread.hpp>

#include <stdio.h>

void WaitForShutdown() {
  bool fShutdown = ShutdownRequested();

  while (!fShutdown) {
    MilliSleep(200);
    fShutdown = ShutdownRequested();
  }
  Interrupt();
}

bool AppInit(int argc, char *argv[]) {
  bool fRet = false;

  gArgs.ParseParameters(argc, argv);

  if (gArgs.IsArgSet("-?") || gArgs.IsArgSet("-h") || gArgs.IsArgSet("-help") ||
      gArgs.IsArgSet("-version")) {
    std::string strUsage = strprintf(_("%s Daemon"), _(PACKAGE_NAME)) + " " +
                           _("version") + " " + FormatFullVersion() + "\n";

    if (gArgs.IsArgSet("-version")) {
      strUsage += FormatParagraph(LicenseInfo());
    } else {
      strUsage += "\n" + _("Usage:") + "\n" +
                  "  litecoincashd [options]                     " +
                  strprintf(_("Start %s Daemon"), _(PACKAGE_NAME)) + "\n";

      strUsage += "\n" + HelpMessage(HMM_BITCOIND);
    }

    fprintf(stdout, "%s", strUsage.c_str());
    return true;
  }

  try {
    if (!fs::is_directory(GetDataDir(false))) {
      fprintf(stderr,
              "Error: Specified data directory \"%s\" does not exist.\n",
              gArgs.GetArg("-datadir", "").c_str());
      return false;
    }
    try {
      gArgs.ReadConfigFile(gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME));
    } catch (const std::exception &e) {
      fprintf(stderr, "Error reading configuration file: %s\n", e.what());
      return false;
    }

    try {
      SelectParams(ChainNameFromCommandLine());
    } catch (const std::exception &e) {
      fprintf(stderr, "Error: %s\n", e.what());
      return false;
    }

    for (int i = 1; i < argc; i++) {
      if (!IsSwitchChar(argv[i][0])) {
        fprintf(stderr,
                "Error: Command line contains unexpected token '%s', see "
                "litecoincashd -h for a list of options.\n",
                argv[i]);
        return false;
      }
    }

    gArgs.SoftSetBoolArg("-server", true);

    InitLogging();
    InitParameterInteraction();
    if (!AppInitBasicSetup()) {
      return false;
    }
    if (!AppInitParameterInteraction()) {
      return false;
    }
    if (!AppInitSanityChecks()) {
      return false;
    }
    if (gArgs.GetBoolArg("-daemon", false)) {
#if HAVE_DECL_DAEMON
      fprintf(stdout, "LitecoinCash server starting\n");

      if (daemon(1, 0)) {
        fprintf(stderr, "Error: daemon() failed: %s\n", strerror(errno));
        return false;
      }
#else
      fprintf(stderr,
              "Error: -daemon is not supported on this operating system\n");
      return false;
#endif
    }

    if (!AppInitLockDataDirectory()) {
      return false;
    }
    fRet = AppInitMain();
  } catch (const std::exception &e) {
    PrintExceptionContinue(&e, "AppInit()");
  } catch (...) {
    PrintExceptionContinue(nullptr, "AppInit()");
  }

  if (!fRet) {
    Interrupt();
  } else {
    WaitForShutdown();
  }
  Shutdown();

  return fRet;
}

int main(int argc, char *argv[]) {
  SetupEnvironment();

  noui_connect();

  return (AppInit(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE);
}
