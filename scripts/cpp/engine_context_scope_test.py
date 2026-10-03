#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 03/10/2026 Tests the per-thread invariants an rts::Scope sets and restores
# (PLAN-023 Phase 5b, item 3), by calling the library's exported enterEngineThreadInvariants and
# leaveEngineThreadInvariants through ctypes, on the main thread and on another thread.
#
# The host thread is given a non-engine floating-point mode (round upward, 53-bit x87 precision, and SSE round
# upward with flush-to-zero and denormals-are-zero set) and a UTF-8 LC_CTYPE (C.utf8). Between enter and leave the
# thread must be in the engine's mode (round to nearest, 24-bit x87 precision, no SSE flush or denormals-are-zero,
# "." as the radix) with the host's LC_CTYPE kept, and afterwards the host's mode and locale must be back. The
# locale is checked by identity (the thread is off the global locale inside a Scope and back on it afterwards),
# so that needs no particular host locale. Where the host has a locale with a comma radix (en_DK, de_DE or
# fr_FR), the host's LC_NUMERIC is that locale too and the radix is checked; only the radix test skips without one.
#
# Usage:
#   engine_context_scope_test.py [--library LIB.so]
#
# LIB.so is the RTS_ENGINE_CONTEXT=ON library, by default the one rlgenerals' Bazel builds
# (bazel-bin/rlgenerals/generalsx/generalsx_foreign_cc/lib/libgeneralsx.so, found upwards from this script).
# The tests are skipped (exit 0) on a library without the context, and where the host has no C.utf8 locale,
# unless GENERALSX_REQUIRE_SCOPE_TESTS is set: then each of those skips is a failure (for a run that must exercise
# them). The radix test's skip for a host with no comma-radix locale is not covered by it.

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

PTHREAD_KEYS_MAX = 1024
LC_GLOBAL_LOCALE = (1 << 64) - 1  # (locale_t)-1, what uselocale(NULL) returns on a thread that has not set one


def skip(reason):
    # A skip is a failure when the run is required to exercise the tests.
    if os.environ.get("GENERALSX_REQUIRE_SCOPE_TESTS"):
        raise AssertionError("required, but would be skipped: " + reason)
    raise unittest.SkipTest(reason)


libc = ctypes.CDLL(None)
libc.pthread_getspecific.restype = ctypes.c_void_p
libc.pthread_getspecific.argtypes = [ctypes.c_uint]
libm = ctypes.CDLL(ctypes.util.find_library("m") or "libm.so.6")
libc.setlocale.restype = ctypes.c_char_p
libc.strtod.restype = ctypes.c_double
libc.strtod.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
libc.mbstowcs.restype = ctypes.c_ssize_t
libc.mbstowcs.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
libc.uselocale.restype = ctypes.c_void_p
libc.newlocale.restype = ctypes.c_void_p
libc.newlocale.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p]
libc.freelocale.argtypes = [ctypes.c_void_p]
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


def keys_holding(value):
    # The pthread keys whose value on this thread is `value`.
    return [k for k in range(PTHREAD_KEYS_MAX) if libc.pthread_getspecific(k) == value]


class ScopeInvariants(unittest.TestCase):
    library = None
    enter = None
    leave = None
    comma = None

    @classmethod
    def setUpClass(cls):
        if not sys.platform.startswith("linux") or not X87:
            skip("needs x86-64 Linux (glibc fenv_t layout)")
        path = os.environ.get("GENERALSX_LIBRARY") or find_library_path()
        if path is None or not os.path.exists(path):
            skip("no libgeneralsx.so (pass --library)")
        cls.library = ctypes.CDLL(path)
        try:
            cls.enter = cls.library[ENTER]
            cls.leave = cls.library[LEAVE]
        except AttributeError:
            skip("the library is built without RTS_ENGINE_CONTEXT")
        # The host's locale, set before the first entry (a thread's engine locale is made from the locale it has
        # then): UTF-8 characters, and a comma radix where the host has a locale for one.
        if libc.setlocale(LC_CTYPE, b"C.utf8") is None or not utf8_multibyte_works():
            skip("the host has no C.utf8 locale")
        cls.comma = None
        for name in (b"en_DK.utf8", b"de_DE.utf8", b"fr_FR.utf8"):
            if libc.setlocale(LC_NUMERIC, name) is not None and not radix_is_dot():
                cls.comma = name
                break
            libc.setlocale(LC_NUMERIC, b"C")
        cls.enter.argtypes = [ctypes.c_void_p]
        cls.leave.argtypes = [ctypes.c_void_p]

    def host_mode(self):
        libm.fesetround(FE_UPWARD)
        set_fenv_fields((control_word() & ~0x0300) | 0x0200, MXCSR_HOST)

    def check_host_mode(self, where):
        self.assertEqual(libm.fegetround(), FE_UPWARD, f"{where}: rounding")
        self.assertEqual(control_word() & 0x0F00, 0x0200 | 0x0800, f"{where}: x87 precision and rounding")
        self.assertEqual(mxcsr_mode(), MXCSR_HOST, f"{where}: SSE rounding, flush-to-zero and denormals-are-zero")
        if self.comma:
            self.assertFalse(radix_is_dot(), f"{where}: LC_NUMERIC")
        self.assertTrue(utf8_multibyte_works(), f"{where}: LC_CTYPE")

    def ascii_locale(self):
        # A freeable locale of a thread's own with an ASCII LC_CTYPE (the host's is UTF-8), and a comma radix where
        # the host has one. (glibc returns its static C locale only when every category asked for is "C".)
        locale = libc.newlocale(LC_NUMERIC_MASK, self.comma or b"C.utf8", None)
        self.assertTrue(locale)
        return locale

    def enter_leave(self):
        self.host_mode()
        saved = ctypes.create_string_buffer(64)
        self.check_host_mode("before")
        host_locale = libc.uselocale(None)
        self.enter(saved)
        try:
            self.assertNotEqual(libc.uselocale(None), host_locale, "the thread is on the engine's locale")
            self.assertEqual(libm.fegetround(), FE_TONEAREST, "engine rounding")
            self.assertEqual(control_word() & 0x0F00, 0, "engine x87 precision (24 bits) and rounding")
            self.assertEqual(mxcsr_mode(), 0, "engine SSE rounding, flush-to-zero and denormals-are-zero")
            self.assertTrue(radix_is_dot(), "engine radix")
            self.assertEqual(libc.strtod(b"0,5", None), 0.0, "engine radix is not a comma")
            self.assertTrue(utf8_multibyte_works(), "the engine keeps the host's LC_CTYPE")
        finally:
            self.leave(saved)
        self.assertEqual(libc.uselocale(None), host_locale, "the host's locale is back")
        self.check_host_mode("after")

    def test_main_thread(self):
        self.enter_leave()

    def test_the_radix_is_switched_to_a_dot_and_restored(self):
        if not self.comma:
            raise unittest.SkipTest("the host has no locale with a comma radix (en_DK, de_DE or fr_FR)")
        self.assertFalse(radix_is_dot(), "the host's radix")
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

        host_locale = libc.uselocale(None)
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
        self.assertEqual(libc.uselocale(None), host_locale, "the host's locale is back")
        self.check_host_mode("after")

    def test_a_change_made_inside_a_nested_scope_is_undone(self):
        # The inner Scope finds the thread already in the engine's mode, so it has nothing to load on entry, but
        # it must still undo a change made inside it (another engine's code leaving its own mode behind).
        self.host_mode()
        outer = ctypes.create_string_buffer(64)
        inner = ctypes.create_string_buffer(64)
        host_locale = libc.uselocale(None)
        self.enter(outer)
        try:
            self.assertEqual(mxcsr_mode(), 0, "engine SSE mode")
            self.enter(inner)
            try:
                libm.fesetround(FE_UPWARD)
                set_fenv_fields(control_word() | 0x0300, MXCSR_HOST)
            finally:
                self.leave(inner)
            self.assertNotEqual(libc.uselocale(None), host_locale, "still on the engine's locale after the nested Scope")
            self.assertEqual(libm.fegetround(), FE_TONEAREST, "x87 rounding after the nested Scope")
            self.assertEqual(control_word() & 0x0F00, 0, "x87 precision and rounding after the nested Scope")
            self.assertEqual(mxcsr_mode(), 0, "SSE mode after the nested Scope")
        finally:
            self.leave(outer)
        self.assertEqual(libc.uselocale(None), host_locale, "the host's locale is back")
        self.check_host_mode("after")

    def run_on_a_thread(self, body):
        errors = []

        def run():
            try:
                body()
            except BaseException as e:  # reported on the test's thread
                errors.append(e)

        t = threading.Thread(target=run)
        t.start()
        t.join()
        if errors:
            raise errors[0]

    def test_a_locale_of_the_threads_own_taken_after_the_first_entry(self):
        # The thread's cached engine locale is still in use by the outer Scope when the thread switches to a
        # locale of its own and a nested Scope enters: that one gets a locale for itself alone, made from the
        # thread's own (an ASCII LC_CTYPE here, the cached one's is UTF-8) and the outer's is left alone.
        def body():
            outer = ctypes.create_string_buffer(64)
            inner = ctypes.create_string_buffer(64)
            self.enter(outer)
            own = self.ascii_locale()
            previous = libc.uselocale(own)
            try:
                self.assertTrue(keys_holding(previous), "the outer Scope's engine locale is cached in a key")
                if self.comma:
                    self.assertFalse(radix_is_dot(), "the thread's own locale has a comma radix")
                self.assertFalse(utf8_multibyte_works(), "the thread's own locale has an ASCII LC_CTYPE")
                self.enter(inner)
                try:
                    self.assertTrue(radix_is_dot(), "engine radix over the thread's own locale")
                    self.assertFalse(utf8_multibyte_works(), "LC_CTYPE of the thread's own locale, not the cached one")
                finally:
                    self.leave(inner)
                self.assertEqual(libc.uselocale(None), own, "the thread's own locale back")
                if self.comma:
                    self.assertFalse(radix_is_dot(), "the thread's own locale back")
                self.assertTrue(keys_holding(previous), "the outer Scope's locale is still the cached one, unfreed")
            finally:
                libc.uselocale(previous)
                self.assertEqual(libc.uselocale(None), previous, "the engine's locale before the outer Scope leaves")
                self.leave(outer)
            self.assertEqual(libc.uselocale(None), LC_GLOBAL_LOCALE, "the thread's locale is back to the global one")
            libc.freelocale(own)
            if self.comma:
                self.assertFalse(radix_is_dot(), "the thread's locale back")

        self.run_on_a_thread(body)

    def test_a_reused_host_locale_address_does_not_keep_a_stale_engine_locale(self):
        # The engine locale is not keyed on the address of a host locale that may since have been freed: a locale
        # with an ASCII LC_CTYPE made at the address of a freed UTF-8 one must give an engine locale that is ASCII.
        reused = []

        def body():
            saved = ctypes.create_string_buffer(64)
            for _ in range(20):
                utf8 = libc.newlocale(LC_CTYPE_MASK, b"C.utf8", None)
                previous = libc.uselocale(utf8)
                self.enter(saved)
                try:
                    self.assertTrue(utf8_multibyte_works(), "UTF-8 own locale")
                finally:
                    self.leave(saved)
                libc.uselocale(previous)
                libc.freelocale(utf8)
                ascii_locale = self.ascii_locale()
                previous = libc.uselocale(ascii_locale)
                if ascii_locale == utf8:
                    reused.append(True)
                self.enter(saved)
                try:
                    self.assertFalse(utf8_multibyte_works(), "ASCII own locale (address reused: %s)" % (ascii_locale == utf8))
                    self.assertTrue(radix_is_dot())
                finally:
                    self.leave(saved)
                libc.uselocale(previous)
                libc.freelocale(ascii_locale)

        self.run_on_a_thread(body)
        if not reused:
            skip("the allocator did not reuse a locale address")

    def test_entering_makes_the_pthread_key_that_frees_the_locale(self):
        # The engine locale is held in a pthread key whose destructor is in the library, made only once the
        # library is pinned (never unloaded). Without the key, the locale would be made and freed per Scope.
        def body():
            saved = ctypes.create_string_buffer(64)
            self.enter(saved)
            try:
                self.assertTrue(keys_holding(libc.uselocale(None)), "no pthread key holds the engine locale")
            finally:
                self.leave(saved)

        self.run_on_a_thread(body)

    def test_a_thread_exit_frees_its_cached_locale(self):
        # Each thread that enters caches a locale freed by a pthread key's destructor at its exit: many short
        # threads must not grow the heap by one locale each.
        class Mallinfo2(ctypes.Structure):
            _fields_ = [(n, ctypes.c_size_t) for n in (
                "arena", "ordblks", "smblks", "hblks", "hblkhd", "usmblks", "fsmblks", "uordblks", "fordblks", "keepcost")]

        if not hasattr(libc, "mallinfo2"):
            skip("no mallinfo2")
        libc.mallinfo2.restype = Mallinfo2

        def run():
            saved = ctypes.create_string_buffer(64)
            self.enter(saved)
            self.leave(saved)

        def burst(count):
            for _ in range(count):
                t = threading.Thread(target=run)
                t.start()
                t.join()

        burst(50)  # warm-up: the key and the allocator's arenas
        before = libc.mallinfo2().uordblks
        burst(1000)
        grown = libc.mallinfo2().uordblks - before
        self.assertLess(grown, 100 * 1024, f"the heap grew by {grown} bytes over 1000 threads")


SET_ASSET_ROOT = "_ZN18StdLocalFileSystem16setAssetRootPathERK11AsciiString"


class AssetRootIsWriteOnce(unittest.TestCase):
    def run_calls(self, *roots):
        # A fresh process (the root is process-wide) calls StdLocalFileSystem::setAssetRootPath for each root in
        # turn (a member function that never uses `this`); "" is an AsciiString with no data, as a boot that resolved
        # no root passes. The AsciiStrings are made by the library's own constructor, so the test does not depend on
        # their layout. Returns the child's stderr, once it has finished the calls and exited cleanly.
        path = os.environ.get("GENERALSX_LIBRARY") or find_library_path()
        if path is None or not os.path.exists(path):
            skip("no libgeneralsx.so (pass --library)")
        code = (
            "import ctypes, sys\n"
            "lib = ctypes.CDLL(sys.argv[1])\n"
            "try:\n"
            "    lib[sys.argv[3]]\n"  # no entry function: the library is built without the context
            "    f = lib[sys.argv[2]]\n"
            "except AttributeError:\n"
            "    sys.exit(77)\n"
            "f.argtypes = [ctypes.c_void_p, ctypes.c_void_p]\n"
            "make = lib['_ZN11AsciiStringC1EPKc']\n"
            "make.argtypes = [ctypes.c_void_p, ctypes.c_char_p]\n"
            "lib['_Z17initMemoryManagerv']()\n"  # the engine's operator new needs it
            "for root in sys.argv[4:]:\n"
            "    ascii_string = ctypes.c_void_p()\n"
            "    make(ctypes.byref(ascii_string), root.encode())\n"
            "    f(None, ctypes.byref(ascii_string))\n"  # a member function that does not use `this`
            "print('done')\n"
        )
        import subprocess
        result = subprocess.run([sys.executable, "-c", code, os.path.realpath(path), SET_ASSET_ROOT, ENTER, *roots],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        if result.returncode == 77:
            skip("the library has no engine context or no StdLocalFileSystem::setAssetRootPath")
        self.assertEqual(result.returncode, 0, "the child failed: " + result.stderr)
        self.assertIn("done", result.stdout, "the child did not finish its calls: " + result.stderr)
        return result.stderr

    def test_a_root_after_a_first_boot_that_resolved_none_is_refused(self):
        # The usual first boot finds its BIGs through the current directory and resolves no root. A later boot
        # that does (an environment variable set in between) must not be the first write of the shared path.
        self.assertIn("setAssetRootPath - the asset fallback path is process-wide", self.run_calls("", "/some/install"))

    def test_the_same_root_again_is_not_refused(self):
        self.assertNotIn("refused", self.run_calls("/some/install", "/some/install"))

    def test_another_root_after_a_first_one_is_refused(self):
        self.assertIn("refused", self.run_calls("/some/install", "/another/install"))


class BootPassesTheRootUnconditionally(unittest.TestCase):
    def test_the_big_file_system_settles_the_root_even_when_empty(self):
        # StdBIGFileSystem::init is the call site that makes the usual first boot (BIGs found through the current
        # directory, no root resolved) settle the process-wide root. It cannot be run without a game install, so
        # this checks the source: under RTS_ENGINE_CONTEXT the call must not sit behind an isNotEmpty() guard.
        source = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "Core", "GameEngineDevice",
                              "Source", "StdDevice", "Common", "StdBIGFileSystem.cpp")
        with open(source) as f:
            text = f.read()
        branch = text.split("#if RTS_ENGINE_CONTEXT", 1)[1].split("#else", 1)[0]
        self.assertIn("setAssetRootPath(primaryAssetsDirectory)", branch)
        self.assertNotIn("isNotEmpty", branch)


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
