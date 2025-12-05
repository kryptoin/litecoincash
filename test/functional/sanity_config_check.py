#!/usr/bin/env python3
# Copyright (c) 2024-2025 The Litecoin Cash Core developers
# Distributed under the MIT software license.
"""
Sanity test: Verify configuration parsing without full node startup.
This test does NOT require chain startup or RPC.
"""

import os
import sys
import subprocess
import tempfile
import shutil

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
SRC_DIR = os.path.join(PROJECT_ROOT, 'src')
DAEMON_PATH = os.path.join(SRC_DIR, 'litecoincashd')

def test_invalid_option_rejected():
    """Test that invalid command line options are rejected."""
    if not os.path.exists(DAEMON_PATH):
        print("SKIP: Invalid option test (daemon not found)")
        return True
    
    try:
        result = subprocess.run(
            [DAEMON_PATH, '--totallyinvalidoption12345'],
            capture_output=True,
            text=True,
            timeout=10
        )
        # Should fail with non-zero exit code
        if result.returncode != 0:
            print("PASS: Invalid option correctly rejected")
            return True
        else:
            print("FAIL: Invalid option was not rejected")
            return False
    except Exception as e:
        print(f"FAIL: Error testing invalid option: {e}")
        return False

def test_conflicting_options():
    """Test that conflicting options are detected."""
    if not os.path.exists(DAEMON_PATH):
        print("SKIP: Conflicting options test (daemon not found)")
        return True
    
    # Create a temp directory for this test
    temp_dir = tempfile.mkdtemp(prefix='lcc_test_')
    
    try:
        # -regtest and -testnet together should conflict
        result = subprocess.run(
            [DAEMON_PATH, '-regtest', '-testnet', f'-datadir={temp_dir}'],
            capture_output=True,
            text=True,
            timeout=10
        )
        # Should fail
        if result.returncode != 0:
            print("PASS: Conflicting network options correctly rejected")
            return True
        else:
            print("FAIL: Conflicting network options were not rejected")
            return False
    except Exception as e:
        print(f"FAIL: Error testing conflicting options: {e}")
        return False
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_printtoconsole():
    """Test that -printtoconsole works without full startup."""
    if not os.path.exists(DAEMON_PATH):
        print("SKIP: printtoconsole test (daemon not found)")
        return True
    
    temp_dir = tempfile.mkdtemp(prefix='lcc_test_')
    
    try:
        # Use -? to just print help and exit (shouldn't start full node)
        result = subprocess.run(
            [DAEMON_PATH, '-?', '-printtoconsole', f'-datadir={temp_dir}'],
            capture_output=True,
            text=True,
            timeout=10
        )
        # -? should exit immediately with help text
        if len(result.stdout) > 0 or len(result.stderr) > 0:
            print("PASS: -printtoconsole works with -?")
            return True
        else:
            print("FAIL: No output with -printtoconsole -?")
            return False
    except Exception as e:
        print(f"FAIL: Error testing printtoconsole: {e}")
        return False
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def test_config_file_parsing():
    """Test that a valid config file is parsed."""
    if not os.path.exists(DAEMON_PATH):
        print("SKIP: Config file test (daemon not found)")
        return True
    
    temp_dir = tempfile.mkdtemp(prefix='lcc_test_')
    config_path = os.path.join(temp_dir, 'litecoincash.conf')
    
    try:
        # Write a simple config file
        with open(config_path, 'w') as f:
            f.write("# Test config\n")
            f.write("regtest=1\n")
            f.write("server=0\n")
        
        # Try to get help with this config (should parse config without error)
        result = subprocess.run(
            [DAEMON_PATH, '-?', f'-datadir={temp_dir}', f'-conf={config_path}'],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        # Should succeed and print help
        if result.returncode == 0 or len(result.stdout) > 100:
            print("PASS: Config file parsed successfully")
            return True
        else:
            print(f"FAIL: Config parsing issue: {result.stderr}")
            return False
    except Exception as e:
        print(f"FAIL: Error testing config file: {e}")
        return False
    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

def main():
    print("=" * 60)
    print("SANITY TEST: Configuration Parsing")
    print("=" * 60)
    
    all_passed = True
    
    print("\n--- Testing invalid option rejection ---")
    if not test_invalid_option_rejected():
        all_passed = False
    
    print("\n--- Testing conflicting options ---")
    if not test_conflicting_options():
        all_passed = False
    
    print("\n--- Testing -printtoconsole ---")
    if not test_printtoconsole():
        all_passed = False
    
    print("\n--- Testing config file parsing ---")
    if not test_config_file_parsing():
        all_passed = False
    
    print("\n" + "=" * 60)
    if all_passed:
        print("RESULT: All config tests PASSED")
        return 0
    else:
        print("RESULT: Some config tests FAILED")
        return 1

if __name__ == '__main__':
    sys.exit(main())
