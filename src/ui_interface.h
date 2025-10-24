// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UI_INTERFACE_H
#define BITCOIN_UI_INTERFACE_H

#include <stdint.h>
#include <string>

#include <boost/signals2/last_value.hpp>
#include <boost/signals2/signal.hpp>

class CWallet;
class CBlockIndex;

enum ChangeType { CT_NEW, CT_UPDATED, CT_DELETED };

class CClientUIInterface {
public:
  enum MessageBoxFlags {
    ICON_INFORMATION = 0,
    ICON_WARNING = (1U << 0),
    ICON_ERROR = (1U << 1),

    ICON_MASK = (ICON_INFORMATION | ICON_WARNING | ICON_ERROR),

    BTN_OK = 0x00000400U,

    BTN_YES = 0x00004000U,

    BTN_NO = 0x00010000U,

    BTN_ABORT = 0x00040000U,

    BTN_RETRY = 0x00080000U,

    BTN_IGNORE = 0x00100000U,

    BTN_CLOSE = 0x00200000U,

    BTN_CANCEL = 0x00400000U,

    BTN_DISCARD = 0x00800000U,

    BTN_HELP = 0x01000000U,

    BTN_APPLY = 0x02000000U,

    BTN_RESET = 0x04000000U,

    BTN_MASK = (BTN_OK | BTN_YES | BTN_NO | BTN_ABORT | BTN_RETRY | BTN_IGNORE |
                BTN_CLOSE | BTN_CANCEL | BTN_DISCARD | BTN_HELP | BTN_APPLY |
                BTN_RESET),

    MODAL = 0x10000000U,

    SECURE = 0x40000000U,

    MSG_INFORMATION = ICON_INFORMATION,
    MSG_WARNING = (ICON_WARNING | BTN_OK | MODAL),
    MSG_ERROR = (ICON_ERROR | BTN_OK | MODAL)
  };

  boost::signals2::signal<bool(const std::string &message,
                               const std::string &caption, unsigned int style),
                          boost::signals2::last_value<bool>>
      ThreadSafeMessageBox;

  boost::signals2::signal<bool(const std::string &message,
                               const std::string &noninteractive_message,
                               const std::string &caption, unsigned int style),
                          boost::signals2::last_value<bool>>
      ThreadSafeQuestion;

  boost::signals2::signal<void(const std::string &message)> InitMessage;

  boost::signals2::signal<void(int newNumConnections)>
      NotifyNumConnectionsChanged;

  boost::signals2::signal<void(bool networkActive)> NotifyNetworkActiveChanged;

  boost::signals2::signal<void()> NotifyAlertChanged;

  boost::signals2::signal<void(CWallet *wallet)> LoadWallet;

  boost::signals2::signal<void(const std::string &title, int nProgress,
                               bool resume_possible)>
      ShowProgress;

  boost::signals2::signal<void(bool, const CBlockIndex *)> NotifyBlockTip;

  boost::signals2::signal<void(bool, const CBlockIndex *)> NotifyHeaderTip;

  boost::signals2::signal<void(void)> BannedListChanged;
};

void InitWarning(const std::string &str);

bool InitError(const std::string &str);

std::string AmountHighWarn(const std::string &optname);

std::string AmountErrMsg(const char *const optname,
                         const std::string &strValue);

extern CClientUIInterface uiInterface;

#endif
