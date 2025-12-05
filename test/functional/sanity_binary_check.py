#!/usr/bin/env python3
# Copyright (c) 2024-2025 The Litecoin Cash Core developers
# Distributed under the MIT software license.
"""
Sanity test: Verify compiled binaries exist and respond to --version/--help.
This test does NOT require chain startup or RPC.
"""

import os
import sys
import subprocess

# Find the src directory relative to this script
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
SRC_DIR = os.path.join(PROJECT_ROOT, 'src')

BINARIES = [
    ('litecoincashd', SRC_DIR),
    ('litecoincash-cli', SRC_DIR),
    ('litecoincash-tx', SRC_DIR),
    ('litecoincash-qt', os.path.join(SRC_DIR, 'qt')),
]

def test_binary_exists(name, directory):
    """Test that a binary exists and is executable."""
    path = os.path.join(directory, name)
    if not os.path.exists(path):
        print(f"FAIL: {name} does not exist at {path}")
        return False
    if not os.access(path, os.X_OK):
        print(f"FAIL: {name} exists but is not executable")
        return False
    print(f"PASS: {name} exists and is executable")
    return True

def test_version_output(name, directory):
    """Test that binary responds to --version."""
    path = os.path.join(directory, name)
    if not os.path.exists(path):
        print(f"SKIP: {name} --version (binary not found)")
        return True  # Don't fail if binary doesn't exist (covered by exists test)
    
    try:
        result = subprocess.run(
            [path, '--version'],
            capture_output=True,
            text=True,
            timeout=10
        )
        # Some tools (like -tx) may return non-zero but still print version
        output = result.stdout + result.stderr
        if 'version' in output.lower() or 'LitecoinCash' in output:
            version_line = output.split('\n')[0]
            print(f"PASS: {name} --version: {version_line}")
            return True
        else:
            print(f"FAIL: {name} --version no version info in output")
            return False
    except subprocess.TimeoutExpired:
        print(f"FAIL: {name} --version timed out")
        return False
    except Exception as e:
        print(f"FAIL: {name} --version error: {e}")
        return False

def test_help_output(name, directory):
    """Test that binary responds to --help."""
    path = os.path.join(directory, name)
    if not os.path.exists(path):
        print(f"SKIP: {name} --help (binary not found)")
        return True
    
    try:
        result = subprocess.run(
            [path, '--help'],
            capture_output=True,
            text=True,
            timeout=10
        )
        # --help typically returns 0 and prints usage
        if len(result.stdout) > 100:  # Should have substantial help text
            print(f"PASS: {name} --help returned {len(result.stdout)} bytes")
            return True
        else:
            print(f"FAIL: {name} --help output too short ({len(result.stdout)} bytes)")
            return False
    except subprocess.TimeoutExpired:
        print(f"FAIL: {name} --help timed out")
        return False
    except Exception as e:
        print(f"FAIL: {name} --help error: {e}")
        return False

def main():
    print("=" * 60)
    print("SANITY TEST: Binary Existence and Basic Response")
    print("=" * 60)
    
    all_passed = True
    
    # Test 1: Binary existence
    print("\n--- Testing binary existence ---")
    for name, directory in BINARIES:
        if not test_binary_exists(name, directory):
            all_passed = False
    
    # Test 2: Version output (only for daemon and CLI, tx doesn't support --version)
    print("\n--- Testing --version output ---")
    for name, directory in BINARIES[:2]:  # Only daemon and CLI
        if not test_version_output(name, directory):
            all_passed = False
    
    # Test 3: Help output
    print("\n--- Testing --help output ---")
    for name, directory in BINARIES[:3]:  # Skip Qt
        if not test_help_output(name, directory):
            all_passed = False
    
    print("\n" + "=" * 60)
    if all_passed:
        print("RESULT: All sanity tests PASSED")
        return 0
    else:
        print("RESULT: Some sanity tests FAILED")
        return 1

if __name__ == '__main__':
    sys.exit(main())
