# Litecoin Cash Core v0.16.4.0 (macOS Apple Silicon)

GUI wallet release for Apple Silicon Macs (`Litecoin-Cash-Qt.app`,
bundled as `Litecoin-Cash-Core.dmg`).

## Download

- `Litecoin-Cash-Core.dmg` (attached below)
- SHA256: `72b6be26bde4b0560243a3455c61a620f1f142eacfc644b11084e48119308752`

## What's changed

Node stability and startup diagnostics:

- No longer aborts on peer headers that descend from a previously failed
  block; they are rejected as invalid instead of crashing the node
  (a crash of this kind previously left the block index and chainstate
  out of sync).
- Auxiliary (Rialto) databases are now closed cleanly on shutdown.
- The two "Error initializing block database" failure modes now log
  distinctly (`genesis` vs `chaintip`, with the offending block hash),
  so `debug.log` identifies which recovery applies.
- New restart smoke test (`test/functional/sanity_restart_check.py`):
  clean stop/restart cycle with tip-preservation check.

## Upgrade notes

- Back up `wallet.dat` (and `litecoincash.conf`) before upgrading.
  Data directory (default):
  `~/Library/Application Support/Litecoin-Cash/`
- If the node previously failed to start with
  "Error initializing block database", do **not** delete anything:
  free disk space first, then run once with `-reindex-chainstate`
  (rebuilds from local block files, no redownload). Only use `-reindex`
  if that fails.
- The `.dmg` is distributed as a release asset only; binaries are not
  committed to git.
