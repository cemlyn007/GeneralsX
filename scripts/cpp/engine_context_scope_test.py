#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 03/10/2026 Tests the per-thread invariants an rts::Scope sets and restores
# (PLAN-023 Phase 5b, item 3), by calling the library's exported enterEngineThreadInvariants and
# leaveEngineThreadInvariants through ctypes, on the main thread and on another thread.
#
# The host thread is given a non-engine floating-point mode (round upward, 53-bit x87 precision, and SSE round
# upward with flush-to-zero and denormals-are-zero set) and a locale whose LC_NUMERIC is not "C". Between enter
# and leave the thread must be in the engine's mode (round to nearest, 24-bit x87 precision, no SSE flush or
# denormals-are-zero, "." as the radix) with the host's LC_CTYPE (UTF-8) kept, and afterwards the host's mode and
# locale must be back.
#
# Usage:
#   engine_context_scope_test.py [--library LIB.so]
#
# LIB.so is the RTS_ENGINE_CONTEXT=ON library, by default the one rlgenerals' Bazel builds
# (bazel-bin/rlgenerals/generalsx/generalsx_foreign_cc/lib/libgeneralsx.so, found upwards from this script).
# The test is skipped (exit 0) on a library without the context, and where the host has no suitable locale.

import argparse
import ctypes
import ctypes.util
import os
import platform
import sys
import threading
import unittest

LC_CTYPE_MASK = 1 << 0
LC_NUMERIC_MASK = 1 << 1
LC_CTYPE = 0
LC_NUMERIC = 1
FE_TONEAREST = 0
FE_UPWARD = 0x800
MXCSR_OFFSET = 28  # in glibc's x86-64 fenv_t
MXCSR_MODE = 0xE040  # the SSE rounding bits, flush-to-zero and denormals-are-zero
MXCSR_HOST = 0x4000 | 0x8000 | 0x0040  # round upward, flush-to-zero, denormals-are-zero

ENTER = "_ZN3rts27enterEngineThreadInvariantsERNS_16ThreadInvariantsE"
LEAVE = "_ZN3rts27leaveEngineThreadInvariantsERKNS_16ThreadInvariantsE"
LIBRARY = os.path.join("bazel-bin", "rlgenerals", "generalsx", "generalsx_foreign_cc", "lib", "libgeneralsx.so")

libc = ctypes.CDLL(None)
libm = ctypes.CDLL(ctypes.util.find_library("m") or "libm.so.6")
libc.setlocale.restype = ctypes.c_char_p
libc.strtod.restype = ctypes.c_double
libc.strtod.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
libc.mbstowcs.restype = ctypes.c_ssize_t
libc.mbstowcs.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
libc.uselocale.restype = ctypes.c_void_p
libc.uselocale.argtypes = [ctypes.c_void_p]

# glibc x86-64's fenv_t: the x87 control word is its first 16 bits, the SSE control/status register its last 32.
X87 = platform.machine() in ("x86_64", "AMD64") and sys.platform.startswith("linux")


def control_word():
    env = ctypes.create_string_buffer(32)
    libm.fegetenv(env)
    return int.from_bytes(env.raw[0:2], "little")


def mxcsr_mode():
    env = ctypes.create_string_buffer(32)
    libm.fegetenv(env)
    return int.from_bytes(env.raw[MXCSR_OFFSET:MXCSR_OFFSET + 4], "little") & MXCSR_MODE


def set_fenv_fields(control_word, mxcsr_mode_bits):
    env = ctypes.create_string_buffer(32)
    libm.fegetenv(env)
    raw = bytearray(env.raw)
    raw[0:2] = control_word.to_bytes(2, "little")
    mxcsr = int.from_bytes(raw[MXCSR_OFFSET:MXCSR_OFFSET + 4], "little")
    raw[MXCSR_OFFSET:MXCSR_OFFSET + 4] = ((mxcsr & ~MXCSR_MODE) | mxcsr_mode_bits).to_bytes(4, "little")
    ctypes.memmove(env, bytes(raw), 32)
    libm.fesetenv(env)


def radix_is_dot():
    return libc.strtod(b"0.5", None) == 0.5


def utf8_multibyte_works():
    return libc.mbstowcs(None, "café".encode("utf-8"), 0) == 4


class ScopeInvariants(unittest.TestCase):
    library = None
    enter = None
    leave = None

    @classmethod
    def setUpClass(cls):
        if not sys.platform.startswith("linux") or not X87:
            raise unittest.SkipTest("needs x86-64 Linux (glibc fenv_t layout)")
        path = os.environ.get("GENERALSX_LIBRARY") or find_library_path()
        if path is None or not os.path.exists(path):
            raise unittest.SkipTest("no libgeneralsx.so (pass --library)")
        cls.library = ctypes.CDLL(path)
        try:
            cls.enter = cls.library[ENTER]
            cls.leave = cls.library[LEAVE]
        except AttributeError:
            raise unittest.SkipTest("the library is built without RTS_ENGINE_CONTEXT")
        # The host's locale, set before the first entry (a thread's engine locale is made from the locale it has then): UTF-8
        # characters and a comma radix.
        if libc.setlocale(LC_CTYPE, b"C.utf8") is None or libc.setlocale(LC_NUMERIC, b"en_DK.utf8") is None:
            raise unittest.SkipTest("the host has no C.utf8 and en_DK.utf8 locales")
        if radix_is_dot() or not utf8_multibyte_works():
            raise unittest.SkipTest("the host locales do not differ from the engine's as expected")
        cls.enter.argtypes = [ctypes.c_void_p]
        cls.leave.argtypes = [ctypes.c_void_p]

    def host_mode(self):
        libm.fesetround(FE_UPWARD)
        set_fenv_fields((control_word() & ~0x0300) | 0x0200, MXCSR_HOST)

    def check_host_mode(self, where):
        self.assertEqual(libm.fegetround(), FE_UPWARD, f"{where}: rounding")
        self.assertEqual(control_word() & 0x0F00, 0x0200 | 0x0800, f"{where}: x87 precision and rounding")
        self.assertEqual(mxcsr_mode(), MXCSR_HOST, f"{where}: SSE rounding, flush-to-zero and denormals-are-zero")
        self.assertFalse(radix_is_dot(), f"{where}: LC_NUMERIC")
        self.assertTrue(utf8_multibyte_works(), f"{where}: LC_CTYPE")

    def enter_leave(self):
        self.host_mode()
        saved = ctypes.create_string_buffer(64)
        self.check_host_mode("before")
        self.enter(saved)
        try:
            self.assertEqual(libm.fegetround(), FE_TONEAREST, "engine rounding")
            self.assertEqual(control_word() & 0x0F00, 0, "engine x87 precision (24 bits) and rounding")
            self.assertEqual(mxcsr_mode(), 0, "engine SSE rounding, flush-to-zero and denormals-are-zero")
            self.assertTrue(radix_is_dot(), "engine radix")
            self.assertEqual(libc.strtod(b"0,5", None), 0.0, "engine radix is not a comma")
            self.assertTrue(utf8_multibyte_works(), "the engine keeps the host's LC_CTYPE")
        finally:
            self.leave(saved)
        self.check_host_mode("after")

    def test_main_thread(self):
        self.enter_leave()

    def test_other_thread(self):
        errors = []

        def run():
            try:
                self.enter_leave()
            except BaseException as e:  # reported on the test's thread
                errors.append(e)

        for _ in range(2):  # a fresh thread each time: nothing is cached against a thread
            t = threading.Thread(target=run)
            t.start()
            t.join()
        if errors:
            raise errors[0]

    def test_host_state_survives_a_different_thread_entering(self):
        # Entered on this thread and left here, with another thread entering and leaving in between.
        self.host_mode()
        saved = ctypes.create_string_buffer(64)
        errors = []

        def run():
            try:
                self.enter_leave()
            except BaseException as e:  # reported on the test's thread
                errors.append(e)

        self.enter(saved)
        try:
            other = threading.Thread(target=run)
            other.start()
            other.join()
            self.assertEqual(libm.fegetround(), FE_TONEAREST, "this thread still in the engine's mode")
        finally:
            self.leave(saved)
        if errors:
            raise errors[0]
        self.check_host_mode("after")


def find_library_path():
    here = os.path.dirname(os.path.abspath(__file__))
    while True:
        candidate = os.path.join(here, LIBRARY)
        if os.path.exists(candidate):
            return candidate
        parent = os.path.dirname(here)
        if parent == here:
            return None
        here = parent


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--library")
    args, rest = parser.parse_known_args()
    if args.library:
        os.environ["GENERALSX_LIBRARY"] = args.library
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)
