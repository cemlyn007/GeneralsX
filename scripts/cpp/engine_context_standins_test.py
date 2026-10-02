#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 02/10/2026 Tests for engine_context_standins.py's uses() (review fix for PLAN-023
# Phase 8, stage RR2a-1: the sizeof/& gate missed a cast before unary `&`, `&&` before unary `&`,
# `std::addressof`, and a stand-in passed bare through a variadic logger). cemlyn007 02/10/2026: a
# multi-word cast (`(unsigned char*)&name`) and the variadic check's false positives on a converted
# argument (`(int)name`, `static_cast<int>(name)`, `name == 32`, `f(name)`) were both review-fix gaps too.
# MainTest covers main() end to end: a stand-in whose class's members are defined in a differently named
# file (the MapObject/WorldHeightMap.cpp gap, found by content now, not by file name), a violation in an
# `--extra-dir` tree (a consumer's own C++ outside Core/Generals/GeneralsMD), and the own-file detection's
# own gaps found by a later re-review (127-gx-w2-r3-16/17/18): a bare call statement and a
# `return C::member(...)` call must not make a file C's own (they are not definitions), while
# `T *C::member()`/`T* C::member()` and a return type on the line above must.
#
# Usage: python3 engine_context_standins_test.py

import contextlib
import io
import os
import shutil
import sys
import tempfile
import unittest

import engine_context_standins as ecs


class UsesTest(unittest.TestCase):
    def assert_flagged(self, text, name="BitDepth", qualifier=""):
        bad = ecs.uses(text, name, qualifier)
        self.assertTrue(bad, f"expected {text!r} to be flagged, got {bad!r}")

    def assert_clean(self, text, name="BitDepth", qualifier=""):
        bad = ecs.uses(text, name, qualifier)
        self.assertFalse(bad, f"expected {text!r} not to be flagged, got {bad!r}")

    # Unary `&`, the fixed set of operators the original gate already covered.
    def test_plain_unary_ampersand(self):
        self.assert_flagged("&BitDepth")
        self.assert_flagged("x = &BitDepth")

    def test_binary_ampersand_is_not_flagged(self):
        self.assert_clean("x & BitDepth")
        self.assert_clean("BitDepth & mask")

    def test_reaching_the_field_is_not_flagged(self):
        self.assert_clean("&BitDepth.x")
        self.assert_clean("&BitDepth[0]")
        self.assert_clean("&BitDepth->x")

    # A C-style pointer cast before unary `&`: the gap this review found.
    def test_cast_before_ampersand(self):
        self.assert_flagged("(void*)&BitDepth")
        self.assert_flagged("(char*)&Textures", name="Textures")
        self.assert_flagged("(IDirect3DBaseTexture8**)&Textures", name="Textures")

    def test_cast_without_pointer_is_not_flagged(self):
        # Indistinguishable from `(expr)&mask`, a binary `&` of a parenthesised expression.
        self.assert_clean("(int)&BitDepth")

    # A multi-word C-style pointer cast before unary `&`: the re-review's gap (the usual
    # memset/memcmp/byte-view idioms, which the single-identifier CAST_BEFORE missed entirely).
    def test_multiword_cast_before_ampersand(self):
        self.assert_flagged("(unsigned char*)&BitDepth")
        self.assert_flagged("(const unsigned char *)&BitDepth")
        self.assert_flagged("(struct Foo*)&BitDepth")
        self.assert_flagged("(void const*)&BitDepth")
        self.assert_flagged("(long long*)&BitDepth")

    # `&&` before unary `&`: also missed before this review.
    def test_double_ampersand_then_unary_ampersand(self):
        self.assert_flagged("a && &BitDepth")
        self.assert_flagged("a&& &BitDepth")

    def test_double_ampersand_token_is_not_flagged(self):
        # `a &&BitDepth` is always logical-and of `a` and `BitDepth`, never unary `&` of `&BitDepth`.
        self.assert_clean("a &&BitDepth")
        self.assert_clean("a && BitDepth")

    # std::addressof: the same hazard as unary `&`, not covered at all before this review.
    def test_std_addressof(self):
        self.assert_flagged("std::addressof(BitDepth)")
        self.assert_flagged("std::addressof( BitDepth )")

    def test_std_addressof_reaching_the_field_is_not_flagged(self):
        self.assert_clean("std::addressof(BitDepth.x)")

    # A stand-in passed bare through a variadic logger: not covered at all before this review.
    def test_variadic_logger(self):
        self.assert_flagged('WWDEBUG_SAY(("bits %d", BitDepth))')
        self.assert_flagged('DEBUG_LOG(("bits %d", BitDepth))')
        self.assert_flagged('fprintf(stderr, "%d", BitDepth)')

    def test_variadic_logger_reaching_the_field_is_not_flagged(self):
        self.assert_clean('WWDEBUG_SAY(("bits %d", BitDepth.x))')

    # A converted stand-in passed to a variadic logger is correct code, not the hazard the gate exists
    # for: it must be flagged only when the name is itself a whole top-level argument (the re-review's
    # false-positive gap).
    def test_variadic_logger_converted_argument_is_not_flagged(self):
        self.assert_clean('WWDEBUG_SAY(("%d", (int)BitDepth))')
        self.assert_clean('DEBUG_LOG(("%d", static_cast<int>(BitDepth)))')
        self.assert_clean('DEBUG_LOG(("%d", BitDepth == 32))')
        self.assert_clean('printf("%d", foo(BitDepth))')

    def test_unrelated_call_is_not_flagged(self):
        self.assert_clean("some_other_function(BitDepth)")

    # DEBUG_ASSERTLOG(c, m) is two macro arguments (Debug.h), not one doubled-paren argument: its
    # message (the second argument) must still be unwrapped and checked, and its condition (the first
    # argument) must never be flagged, since it reaches the field through the stand-in's own conversion
    # operator (`!(c)`), not through `...`. The 530ad166b regression this review found.
    def test_debug_assertlog_message_is_checked(self):
        self.assert_flagged('DEBUG_ASSERTLOG(x, ("bits %d", BitDepth))')

    def test_debug_assertlog_condition_is_not_flagged(self):
        self.assert_clean('DEBUG_ASSERTLOG(BitDepth, ("bits"))')

    def test_debug_assertlog_converted_message_argument_is_not_flagged(self):
        self.assert_clean('DEBUG_ASSERTLOG(x, ("%d", (int)BitDepth))')

    # The same two-macro-argument shape applies to DEBUG_ASSERTCRASH(c, m) and DEBUG_LOG_LEVEL(l, m)
    # (Debug.h): printf-style loggers the gate left uncovered before this review.
    def test_debug_assertcrash_message_is_checked(self):
        self.assert_flagged('DEBUG_ASSERTCRASH(x, ("bits %d", BitDepth))')

    def test_debug_assertcrash_condition_is_not_flagged(self):
        self.assert_clean('DEBUG_ASSERTCRASH(BitDepth, ("bits"))')

    def test_debug_log_level_message_is_checked(self):
        self.assert_flagged('DEBUG_LOG_LEVEL(LEVEL_DEBUG, ("bits %d", BitDepth))')

    def test_debug_log_level_level_is_not_flagged(self):
        self.assert_clean('DEBUG_LOG_LEVEL(BitDepth, ("bits"))')

    # DEBUG_CRASH(m) and WWDEBUG_WARNING(x) are single-doubled-paren-argument loggers too (Debug.h,
    # wwdebug.h), the same shape as WWDEBUG_SAY/DEBUG_LOG.
    def test_debug_crash_message_is_checked(self):
        self.assert_flagged('DEBUG_CRASH(("bits %d", BitDepth))')

    def test_wwdebug_warning_message_is_checked(self):
        self.assert_flagged('WWDEBUG_WARNING(("bits %d", BitDepth))')

    def test_qualified_name(self):
        self.assert_flagged("(void*)&DX8Wrapper::BitDepth", qualifier="DX8Wrapper")
        self.assert_clean("DX8Wrapper::BitDepth & mask", qualifier="DX8Wrapper")


class MainTest(unittest.TestCase):
    """End-to-end tests of main(), against a temporary GeneralsX-shaped tree."""

    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="ecs_main_test_")
        self.addCleanup(shutil.rmtree, self.root, ignore_errors=True)
        for d in ("Core", "Generals", "GeneralsMD"):
            os.makedirs(os.path.join(self.root, d), exist_ok=True)

    def write(self, relpath, content):
        path = os.path.join(self.root, relpath)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(content)
        return path

    def run_main(self, argv):
        old_argv = sys.argv
        sys.argv = ["engine_context_standins.py"] + argv
        out = io.StringIO()
        try:
            with contextlib.redirect_stdout(out):
                code = ecs.main()
        finally:
            sys.argv = old_argv
        return code, out.getvalue()

    def test_unqualified_use_in_differently_named_member_file(self):
        # The MapObject/WorldHeightMap.cpp gap: a stand-in's class's members are defined in a file
        # that is not named after the class, so unqualified-use scanning must find it by content.
        self.write(
            "Core/MapObject.h",
            "class MapObject {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        self.write(
            "Core/WorldHeightMap.cpp",
            '#include "MapObject.h"\n'
            "void MapObject::fastAssignAllUniqueIDs()\n"
            "{\n"
            "    MapObject** link = &TheMapObjectListPtr;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("WorldHeightMap.cpp", out)
        self.assertIn("TheMapObjectListPtr", out)

    def test_unqualified_use_outside_own_files_is_not_flagged(self):
        # The same class/stand-in, but the unqualified use sits in a file that neither declares the
        # stand-in nor defines any of the class's members: not flagged (it would not compile either).
        # Unrelated.cpp carries a REAL unqualified use (not just a stripped comment, the gap the
        # re-review found: a mutation that treats every file as "own" must fail this test).
        self.write(
            "Core/MapObject.h",
            "class MapObject {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        self.write(
            "Core/WorldHeightMap.cpp",
            '#include "MapObject.h"\nvoid MapObject::fastAssignAllUniqueIDs() {}\n',
        )
        self.write(
            "Core/Unrelated.cpp",
            "MapObject** p = &TheMapObjectListPtr;\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_bare_call_statement_is_not_own_file(self):
        # A file that only CALLS a member (no definition) does not become that class's own file: the
        # mutation the re-review found (dropping the prefix/definition-shape guard) must fail this test.
        self.write(
            "Core/MapObject.h",
            "class MapObject {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        self.write(
            "Core/Caller.cpp",
            '#include "MapObject.h"\n'
            "void f() {\n"
            "    MapObject::fastAssignAllUniqueIDs();\n"
            "    MapObject** p = &TheMapObjectListPtr;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_return_statement_call_is_not_own_file(self):
        # `return C::member(...)` has the non-empty prefix `return`, but it is a call, not a
        # definition: it must not make the file C's own (127-gx-w2-r3-16's dx8wrapper.cpp/ww3d.cpp gap).
        self.write(
            "Core/WW3D.h",
            "class WW3D {\n"
            "public:\n"
            "    static constexpr rts::ContextField<unsigned long, &rts::EngineContext::frame> "
            "FrameCount{};\n"
            "};\n",
        )
        self.write(
            "Core/dx8wrapper.cpp",
            '#include "WW3D.h"\n'
            "#define FrameCount (12345)\n"
            "unsigned DX8Wrapper::Other()\n"
            "{\n"
            "    return WW3D::Get_Frame_Count();\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_pointer_attached_to_class_name_is_own_file(self):
        # `T *C::member()`/`T* C::member()`: the `*`/`&` may attach to either word
        # (127-gx-w2-r3-17's WorldHeightMap.cpp/ww3d.cpp gap).
        self.write(
            "Core/MapObject.h",
            "class MapObject {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        self.write(
            "Core/WorldHeightMap.cpp",
            '#include "MapObject.h"\n'
            "MapObject *MapObject::duplicate()\n"
            "{\n"
            "    MapObject** link = &TheMapObjectListPtr;\n"
            "    return link ? *link : 0;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("WorldHeightMap.cpp", out)

    def test_multiline_return_type_is_own_file(self):
        # A return type on the line above (the classic Westwood style) leaves an empty prefix on the
        # `C::member(` line itself (127-gx-w2-r3-17's gap).
        self.write(
            "Core/MapObject.h",
            "class MapObject {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        self.write(
            "Core/WorldHeightMap.cpp",
            '#include "MapObject.h"\n'
            "Bool\n"
            "MapObject::validate()\n"
            "{\n"
            "    MapObject** link = &TheMapObjectListPtr;\n"
            "    return true;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("WorldHeightMap.cpp", out)

    def test_extra_dir_violation_is_reported(self):
        # A consumer's own C++ outside the Core/Generals/GeneralsMD layout, found through --extra-dir.
        self.write(
            "Core/MapObject.h",
            "class MapObject {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        extra_dir = tempfile.mkdtemp(prefix="ecs_extra_dir_test_")
        self.addCleanup(shutil.rmtree, extra_dir, ignore_errors=True)
        with open(os.path.join(extra_dir, "launcher.cpp"), "w", encoding="utf-8") as f:
            f.write('#include "MapObject.h"\n' "auto *p = &MapObject::TheMapObjectListPtr;\n")
        code, out = self.run_main(["--root", self.root, "--extra-dir", extra_dir])
        self.assertEqual(code, 1, out)
        self.assertIn("launcher.cpp", out)
        self.assertIn("TheMapObjectListPtr", out)


if __name__ == "__main__":
    sys.exit(unittest.main())
