#!/usr/bin/env python3
# Copyright (c) 2026 Litecoin Cash Core developers
# Distributed under the MIT software license.
"""Test entry point: delegates to run_sanity_tests.sh."""
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
runner = os.path.join(SCRIPT_DIR, 'run_sanity_tests.sh')
sys.exit(subprocess.call(['bash', runner]))
