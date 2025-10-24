// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_GUICONSTANTS_H
#define BITCOIN_QT_GUICONSTANTS_H

static const int MODEL_UPDATE_DELAY = 250;

static const int MAX_PASSPHRASE_SIZE = 1024;

static const int STATUSBAR_ICONSIZE = 16;

static const bool DEFAULT_SPLASHSCREEN = true;

#define STYLE_INVALID "background:#FF8080"

#define COLOR_UNCONFIRMED QColor(128, 128, 128)

#define COLOR_NEGATIVE QColor(255, 0, 0)

#define COLOR_BAREADDRESS QColor(140, 140, 140)

#define COLOR_TX_STATUS_OPENUNTILDATE QColor(64, 64, 255)

#define COLOR_TX_STATUS_OFFLINE QColor(192, 192, 192)

#define COLOR_TX_STATUS_DANGER QColor(200, 100, 100)

#define COLOR_BLACK QColor(0, 0, 0)

static const int TOOLTIP_WRAP_THRESHOLD = 80;

static const int MAX_URI_LENGTH = 255;

#define QR_IMAGE_SIZE 300

#define SPINNER_FRAMES 36

#define QAPP_ORG_NAME "Litecoin Cash"
#define QAPP_ORG_DOMAIN "litecoinca.sh"
#define QAPP_APP_NAME_DEFAULT "LitecoinCash-Qt"
#define QAPP_APP_NAME_TESTNET "LitecoinCash-Qt-testnet"

#endif
