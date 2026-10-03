#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 03/10/2026 Builds and runs engine_context_unit_test.cpp (PLAN-023)
#
# Compiles Common/EngineContext.cpp and the test with g++ against a stub PreRTS.h, under AddressSanitizer,
# and runs it. Run with `python3 -m unittest scripts/cpp/engine_context_unit_test.py` (or pytest); no bazel
# target needed, same as engine_state_symbols_test.py. It is skipped where g++ is missing.

import os
import shutil
import subprocess
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
INCLUDE = os.path.join(ROOT, "Core", "GameEngine", "Include")
SOURCE = os.path.join(ROOT, "Core", "GameEngine", "Source", "Common", "EngineContext.cpp")

STUB_PRERTS = """#pragma once
#include <cassert>
#define DEBUG_ASSERTCRASH(condition, message) assert(condition)
#define DEBUG_CRASH(message) assert(false)
"""


@unittest.skipIf(shutil.which("g++") is None, "g++ not found")
class EngineContextUnitTest(unittest.TestCase):
    def test_per_engine_static(self):
        with tempfile.TemporaryDirectory() as work:
            with open(os.path.join(work, "PreRTS.h"), "w") as stub:
                stub.write(STUB_PRERTS)
            binary = os.path.join(work, "engine_context_unit_test")
            subprocess.run(
                [
                    "g++", "-std=c++20", "-g", "-fsanitize=address,undefined", "-DRTS_ENGINE_CONTEXT=1",
                    "-I" + work, "-I" + INCLUDE, "-I" + os.path.join(INCLUDE, "Common"),
                    os.path.join(HERE, "engine_context_unit_test.cpp"), SOURCE, "-o", binary,
                ],
                check=True,
            )
            result = subprocess.run([binary], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("engine_context_unit_test: ok", result.stdout)


if __name__ == "__main__":
    unittest.main()
