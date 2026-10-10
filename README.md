Litecoin Cash Core integration/staging tree
===========================================

https://litecoinca.sh

What is Litecoin Cash?
----------------------

Litecoin Cash (LCC) is a fork of Litecoin supporting SHA256 Proof of Work and
Hive Mining. Litecoin Cash Core is the full node software that makes up the
backbone of the LCC network.

License
-------

Litecoin Cash Core is released under the terms of the MIT license. See
[COPYING](COPYING) for more information or visit
https://opensource.org/licenses/MIT.

Building
--------

### macOS (Apple Silicon / M1,M2,M3,M4,M5,M6)

A build script is provided for macOS on Apple Silicon:

```bash
./build-osx-qt-m1.sh
```

This script handles the full build pipeline: LevelDB, autotools, configure,
compile, and packaging into a `.dmg`.

**Prerequisites (via Homebrew):**

```bash
brew install autoconf automake libtool pkg-config
brew install berkeley-db@4 boost@1.85 openssl@3 qt@5
```

The build produces:
- `Litecoin-Cash-Qt.app` — the GUI wallet application
- `Litecoin-Cash-Core.dmg` — disk image for distribution
- `src/litecoincashd` — headless daemon
- `src/litecoincash-cli` — RPC command-line client
- `src/litecoincash-tx` — transaction utility

### Linux / Other Unix

```bash
./autogen.sh
./configure --with-wallet --enable-qt
make
```

Data Directory
--------------

| Platform  | Path                                              |
|-----------|---------------------------------------------------|
| macOS     | `~/Library/Application Support/Litecoin-Cash/`    |
| Linux     | `~/.litecoincash/`                                |
| Windows   | `%APPDATA%\Litecoin-Cash\`                        |

The configuration file is `litecoincash.conf` inside the data directory.
Use `-datadir=<path>` to override the default location.

Development
-----------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable.
[Tags](https://github.com/litecoincash-project/litecoincash/tags) are created
regularly to indicate new official, stable release versions of Litecoin Cash
Core.

The contribution workflow is described in
[CONTRIBUTING.md](CONTRIBUTING.md).

Developer discussion takes place on the
[mailing list](https://groups.google.com/forum/#!forum/litecoincash-dev).

Testing
-------

### Sanity Tests

Quick tests that verify basic functionality without requiring chain sync:

```bash
./test/functional/run_sanity_tests.sh
```

These checks run against a private regtest datadir in a temp directory
(no network: `-connect=0 -listen=0`) and cover binary existence,
`--version` and `--help` output, configuration parsing, data directory
creation, and a clean stop/restart cycle:

- `sanity_binary_check.py` — binaries exist and respond to `--version`/`--help`
- `sanity_config_check.py` — conflicting network options rejected, config parsed
- `sanity_datadir_check.py` — data directory, `debug.log` and pid file creation
- `sanity_restart_check.py` — daemon starts, serves RPC, exits 0 on `stop`,
  then restarts against the same datadir with the tip preserved and no
  assertion / `Corrupted block database` / `Error initializing block database`
  markers in `debug.log` (benign first-boot `DeserializeFileDB` lines for
  `peers.dat`/`banlist.dat` are ignored). Set `LITECOINCASHD_PATH` and
  `LITECOINCASH_CLI_PATH` to test installed binaries instead of `src/`.

`test/functional/test_runner.py` delegates to `run_sanity_tests.sh`.

### Unit Tests

Unit tests can be compiled and run (if not disabled in configure) with:

```bash
make check
```

Further details in [src/test/README.md](src/test/README.md).

### Integration Tests

Python-based regression and integration tests live under
[test/](test/). Run them with:

```bash
test/functional/test_runner.py
```

Translations
------------

Translation corrections or expansions are welcomed as GitHub pull requests.

Cleaning Up
-----------

To remove build artifacts and log files:

```bash
./cleanup.sh
```