#!/usr/bin/env python3
# Copyright (c) 2026 Litecoin Cash Core developers
# Distributed under the MIT software license.
"""
Sanity test: clean stop/restart cycle on a private regtest datadir.

Starts litecoincashd, checks RPC, stops it cleanly via `stop`, then
restarts against the SAME datadir with no flags and verifies the tip
is preserved and debug.log shows no load errors.

This guards the restart/load path (unclean-shutdown handling, block
index + chainstate consistency on reload). It does NOT mine, sync,
or need the network: -connect=0 -listen=0.
"""

import os
import sys
import subprocess
import tempfile
import shutil
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
SRC_DIR = os.path.join(PROJECT_ROOT, 'src')
# Allow testing installed/moved binaries, e.g.
# LITECOINCASHD_PATH=/opt/crypto/lcc/lcc16-deep-pro/litecoincashd
DAEMON_PATH = os.environ.get('LITECOINCASHD_PATH',
                             os.path.join(SRC_DIR, 'litecoincashd'))
CLI_PATH = os.environ.get('LITECOINCASH_CLI_PATH',
                          os.path.join(SRC_DIR, 'litecoincash-cli'))

RPC_USER = 'lcc_smoke'
RPC_PASSWORD = 'lcc_smoke_pw_12345'
RPC_PORT = 19557
START_TIMEOUT = 90
STOP_TIMEOUT = 60

# Markers that must NOT appear in debug.log after clean runs.
# Note: bare "ERROR:" is deliberately excluded -- a fresh datadir
# benignly logs "DeserializeFileDB: Failed to open file" for the
# not-yet-created peers.dat/banlist.dat on first boot.
BAD_LOG_MARKERS = [
    'Assertion failed',
    'Corrupted block database',
    'Aborted block database rebuild',
    'Error initializing block database',
    'Error opening block database',
    'Error loading block database',
]


class Node:
    def __init__(self, data_dir):
        self.data_dir = data_dir
        self.proc = None

    def base_args(self):
        return [
            DAEMON_PATH,
            '-regtest',
            '-datadir=%s' % self.data_dir,
            '-connect=0',
            '-listen=0',
            '-server=1',
            '-daemon=0',
            '-rpcuser=%s' % RPC_USER,
            '-rpcpassword=%s' % RPC_PASSWORD,
            '-rpcport=%d' % RPC_PORT,
            '-rpcbind=127.0.0.1',
        ]

    def cli_args(self, *cmd):
        return [
            CLI_PATH,
            '-regtest',
            '-datadir=%s' % self.data_dir,
            '-rpcuser=%s' % RPC_USER,
            '-rpcpassword=%s' % RPC_PASSWORD,
            '-rpcport=%d' % RPC_PORT,
        ] + list(cmd)

    def start(self):
        self.proc = subprocess.Popen(
            self.base_args(),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )

    def rpc(self, *cmd, timeout=10):
        try:
            result = subprocess.run(
                self.cli_args(*cmd),
                capture_output=True,
                text=True,
                timeout=timeout,
            )
        except Exception as e:
            return None, 'cli error: %s' % e
        return result.returncode, (result.stdout + result.stderr).strip()

    def wait_for_rpc(self, timeout=START_TIMEOUT):
        deadline = time.time() + timeout
        last_out = ''
        while time.time() < deadline:
            if self.proc.poll() is not None:
                out = self.drain()
                return False, 'daemon exited early (rc=%d): %s' % (
                    self.proc.returncode, out[-2000:])
            rc, last_out = self.rpc('getblockcount')
            if rc == 0:
                return True, last_out
            time.sleep(1)
        return False, 'RPC not ready in %ds: %s' % (timeout, last_out[-500:])

    def stop(self, timeout=STOP_TIMEOUT):
        rc, out = self.rpc('stop')
        if rc != 0:
            return False, 'stop RPC failed: %s' % out[-500:]
        try:
            self.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            return False, 'daemon did not exit within %ds of stop' % timeout
        if self.proc.returncode != 0:
            return False, 'daemon exit code %d (expected 0). Output tail: %s' % (
                self.proc.returncode, self.drain()[-2000:])
        return True, ''

    def drain(self):
        try:
            out, _ = self.proc.communicate(timeout=5)
            return out.decode('utf-8', 'replace') if isinstance(out, bytes) else (out or '')
        except Exception:
            return ''

    def terminate(self):
        if self.proc and self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.proc.kill()


def find_debug_log(data_dir):
    for candidate in (os.path.join(data_dir, 'regtest', 'debug.log'),
                      os.path.join(data_dir, 'debug.log')):
        if os.path.isfile(candidate):
            return candidate
    return None


def check_debug_log(data_dir):
    path = find_debug_log(data_dir)
    if not path:
        print('INFO: debug.log not found (node may have exited too fast)')
        return True
    with open(path, 'r', errors='replace') as f:
        content = f.read()
    bad = [m for m in BAD_LOG_MARKERS if m in content]
    if bad:
        print('FAIL: debug.log contains failure markers: %s' % ', '.join(bad))
        for line in content.splitlines():
            if any(m in line for m in bad):
                print('  | ' + line[:300])
        return False
    print('PASS: debug.log clean (%d bytes, no failure markers)' % len(content))
    return True


def main():
    print('=' * 60)
    print('SANITY TEST: Clean Stop/Restart Cycle (regtest)')
    print('=' * 60)

    for path in (DAEMON_PATH, CLI_PATH):
        if not os.path.exists(path):
            print('SKIP: binary not found: %s' % path)
            return 0

    temp_dir = tempfile.mkdtemp(prefix='lcc_restart_')
    data_dir = os.path.join(temp_dir, 'data')
    os.makedirs(data_dir, exist_ok=True)
    node = Node(data_dir)
    ok = True

    try:
        # --- First boot ---
        print('\n--- First boot ---')
        node.start()
        ready, info = node.wait_for_rpc()
        if not ready:
            print('FAIL: first boot: %s' % info)
            return 1
        print('PASS: daemon started, getblockcount=%s' % info)

        rc, tip1 = node.rpc('getbestblockhash')
        if rc != 0:
            print('FAIL: getbestblockhash: %s' % tip1)
            return 1
        print('PASS: tip before stop: %s' % tip1)

        # --- Clean stop ---
        print('\n--- Clean stop via `stop` RPC ---')
        stopped, info = node.stop()
        if not stopped:
            print('FAIL: %s' % info)
            return 1
        print('PASS: daemon exited 0 on clean stop')

        # --- Restart against the SAME datadir, no extra flags ---
        print('\n--- Restart (same datadir, no flags) ---')
        node.start()
        ready, info = node.wait_for_rpc()
        if not ready:
            print('FAIL: restart: %s' % info)
            return 1
        print('PASS: daemon reloaded existing datadir, getblockcount=%s' % info)

        rc, tip2 = node.rpc('getbestblockhash')
        if rc != 0:
            print('FAIL: getbestblockhash after restart: %s' % tip2)
            return 1
        if tip1 != tip2:
            print('FAIL: tip changed across restart: %s -> %s' % (tip1, tip2))
            return 1
        print('PASS: tip preserved across restart: %s' % tip2)

        print('\n--- Final clean stop ---')
        stopped, info = node.stop()
        if not stopped:
            print('FAIL: %s' % info)
            return 1
        print('PASS: daemon exited 0 on second clean stop')

        print('\n--- debug.log check ---')
        if not check_debug_log(data_dir):
            return 1
    finally:
        node.terminate()
        shutil.rmtree(temp_dir, ignore_errors=True)

    print('\n' + '=' * 60)
    print('RESULT: Restart smoke test PASSED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
