#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 02/10/2026 Tests for engine_context_standins.py (PLAN-023 Phase 8, stage RR2a-1).
# UsesTest covers uses(): a unary `&` after a C-style pointer cast (including a multi-word one such as
# `(unsigned char*)&name`) or after `&&`, `std::addressof`, and a stand-in passed bare to a direct variadic
# function (the printf family, DebugLog, WWDebug_Printf, ...), to Format/format (matched by method name
# alone), or as an element of a doubled-parens message list under any macro name (the list is recognised by
# its shape, a parenthesised, string-literal-led argument, not by an enumerated name). A converted argument
# (`(int)name`, `static_cast<int>(name)`, `name == 32`, `f(name)`) and a two-argument macro's condition or
# level argument are not flagged.
# MainTest covers main() end to end: a stand-in whose class's members are defined in a differently named
# file (found by content, not by file name), a violation in an `--extra-dir` tree (a consumer's own C++
# outside Core/Generals/GeneralsMD), a stand-in scoped to the game it is declared in, and the own-file
# detection's edge cases: a bare call statement, a `return C::member(...)` call, a ternary's
# `cond ? C::member() : x` and a case label's `case C::member():` must not make a file C's own (none of them
# are definitions, and `:` is a definition's own shape only for a constructor's initialiser list), while
# `T *C::member()`/`T* C::member()`, a return type on the line above, a constructor's initialiser-list `:`
# and a trailing `const` must, and so must a file that defines only a member of a class deriving (directly
# or transitively) from a stand-in's class.
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

    # A C-style pointer cast before unary `&`.
    def test_cast_before_ampersand(self):
        self.assert_flagged("(void*)&BitDepth")
        self.assert_flagged("(char*)&Textures", name="Textures")
        self.assert_flagged("(IDirect3DBaseTexture8**)&Textures", name="Textures")

    def test_cast_without_pointer_is_not_flagged(self):
        # Indistinguishable from `(expr)&mask`, a binary `&` of a parenthesised expression.
        self.assert_clean("(int)&BitDepth")

    # A multi-word C-style pointer cast before unary `&` (the usual
    # memset/memcmp/byte-view idioms, which the single-identifier cast form misses).
    def test_multiword_cast_before_ampersand(self):
        self.assert_flagged("(unsigned char*)&BitDepth")
        self.assert_flagged("(const unsigned char *)&BitDepth")
        self.assert_flagged("(struct Foo*)&BitDepth")
        self.assert_flagged("(void const*)&BitDepth")
        self.assert_flagged("(long long*)&BitDepth")

    # `&&` before unary `&`.
    def test_double_ampersand_then_unary_ampersand(self):
        self.assert_flagged("a && &BitDepth")
        self.assert_flagged("a&& &BitDepth")

    def test_double_ampersand_token_is_not_flagged(self):
        # `a &&BitDepth` is always logical-and of `a` and `BitDepth`, never unary `&` of `&BitDepth`.
        self.assert_clean("a &&BitDepth")
        self.assert_clean("a && BitDepth")

    # std::addressof: the same hazard as unary `&`.
    def test_std_addressof(self):
        self.assert_flagged("std::addressof(BitDepth)")
        self.assert_flagged("std::addressof( BitDepth )")

    def test_std_addressof_reaching_the_field_is_not_flagged(self):
        self.assert_clean("std::addressof(BitDepth.x)")

    # A stand-in passed bare through a variadic logger.
    def test_variadic_logger(self):
        self.assert_flagged('WWDEBUG_SAY(("bits %d", BitDepth))')
        self.assert_flagged('DEBUG_LOG(("bits %d", BitDepth))')
        self.assert_flagged('fprintf(stderr, "%d", BitDepth)')

    def test_variadic_logger_reaching_the_field_is_not_flagged(self):
        self.assert_clean('WWDEBUG_SAY(("bits %d", BitDepth.x))')

    # A converted stand-in passed to a variadic logger is correct code, not the hazard the gate exists
    # for: it must be flagged only when the name is itself a whole top-level argument.
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
    # operator (`!(c)`), not through `...`.
    def test_debug_assertlog_message_is_checked(self):
        self.assert_flagged('DEBUG_ASSERTLOG(x, ("bits %d", BitDepth))')

    def test_debug_assertlog_condition_is_not_flagged(self):
        self.assert_clean('DEBUG_ASSERTLOG(BitDepth, ("bits"))')

    def test_debug_assertlog_converted_message_argument_is_not_flagged(self):
        self.assert_clean('DEBUG_ASSERTLOG(x, ("%d", (int)BitDepth))')

    # The same two-macro-argument shape applies to DEBUG_ASSERTCRASH(c, m) and DEBUG_LOG_LEVEL(l, m)
    # (Debug.h): printf-style loggers too.
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

    # DEBUG_LOG_RAW/DEBUG_LOG_LEVEL_RAW (same doubled-parens shape as DEBUG_LOG/DEBUG_LOG_LEVEL),
    # WWRELEASE_SAY, WWDEBUG_ERROR and CRCDEBUG_LOG, and Format/format (StringClass::Format,
    # AsciiString::format/UnicodeString::format): WWRELEASE_SAY is not the only logger live in release
    # builds (wwdebug.h also defines WWRELEASE_WARNING and WWRELEASE_ERROR, unconditionally, right beside
    # it), it just happens to be the one this group of tests originally covered.
    def test_debug_log_raw_message_is_checked(self):
        self.assert_flagged('DEBUG_LOG_RAW(("bits %d", BitDepth))')

    def test_debug_log_level_raw_message_is_checked(self):
        self.assert_flagged('DEBUG_LOG_LEVEL_RAW(LEVEL_DEBUG, ("bits %d", BitDepth))')

    def test_debug_log_level_raw_level_is_not_flagged(self):
        self.assert_clean('DEBUG_LOG_LEVEL_RAW(BitDepth, ("bits"))')

    def test_wwrelease_say_message_is_checked(self):
        self.assert_flagged('WWRELEASE_SAY(("bits %d", BitDepth))')

    def test_wwdebug_error_message_is_checked(self):
        self.assert_flagged('WWDEBUG_ERROR(("bits %d", BitDepth))')

    def test_crcdebug_log_message_is_checked(self):
        self.assert_flagged('CRCDEBUG_LOG(("bits %d", BitDepth))')

    def test_format_method_call_is_checked(self):
        self.assert_flagged('name.Format("bits %d", BitDepth)')
        self.assert_flagged('str.format("bits %d", BitDepth)')

    def test_format_method_call_reaching_the_field_is_not_flagged(self):
        self.assert_clean('name.Format("bits %d", BitDepth.x)')

    # The doubled-parens message list is now found generically, by its own shape (a parenthesised,
    # string-literal-led argument list that is itself a complete top-level argument), not by an
    # enumerated macro name: a logger the gate has never heard of is still caught. SNAPSHOT_SAY
    # (Core/Libraries/Source/WWVegas/WW3D2/ww3d.h), WWRELEASE_WARNING/WWRELEASE_ERROR (wwdebug.h, right
    # beside WWRELEASE_SAY) and SHATTER_DEBUG_SAY/SLOTLIST_DEBUG_LOG (shattersystem.cpp,
    # WOLGameSetupMenu.cpp) all share WWDEBUG_SAY's doubled-parens shape and are among the loggers this
    # closed (an enumerated macro-name list would have missed all of these).
    def test_snapshot_say_message_is_checked(self):
        self.assert_flagged('SNAPSHOT_SAY(("bits %d", BitDepth))')

    def test_wwrelease_warning_message_is_checked(self):
        self.assert_flagged('WWRELEASE_WARNING(("bits %d", BitDepth))')

    def test_wwrelease_error_message_is_checked(self):
        self.assert_flagged('WWRELEASE_ERROR(("bits %d", BitDepth))')

    def test_shatter_debug_say_message_is_checked(self):
        self.assert_flagged('SHATTER_DEBUG_SAY(("bits %d", BitDepth))')

    def test_slotlist_debug_log_message_is_checked(self):
        self.assert_flagged('SLOTLIST_DEBUG_LOG(("bits %d", BitDepth))')

    # A macro the gate has no name for at all: the generic, shape-based detection does not need one.
    def test_unenumerated_doubled_parens_macro_is_checked(self):
        self.assert_flagged('SOME_FUTURE_LOGGER(("bits %d", BitDepth))')

    # The direct variadic functions behind the macros above (and the C-library printf-family functions
    # they are sometimes called through directly, with no doubled-parens wrapping at all).
    def test_debuglog_function_is_checked(self):
        self.assert_flagged('DebugLog("bits %d", BitDepth)')

    def test_debuglograw_function_is_checked(self):
        self.assert_flagged('DebugLogRaw("bits %d", BitDepth)')

    def test_debugcrash_function_is_checked(self):
        self.assert_flagged('DebugCrash("bits %d", BitDepth)')

    def test_wwdebug_printf_function_is_checked(self):
        self.assert_flagged('WWDebug_Printf("bits %d", BitDepth)')
        self.assert_flagged('WWDebug_Printf_Warning("bits %d", BitDepth)')
        self.assert_flagged('WWDebug_Printf_Error("bits %d", BitDepth)')

    def test_snprintf_function_is_checked(self):
        self.assert_flagged('_snprintf(buf, 10, "%d", BitDepth)')

    def test_swprintf_function_is_checked(self):
        self.assert_flagged('swprintf(buf, L"%d", BitDepth)')

    # An ordinary parenthesised sub-expression that happens to be a whole argument, and happens to start
    # with a string literal, is not this idiom unless the call turns out to also carry the stand-in as one
    # of that list's own comma-separated items: a plain redundant-paren argument must stay clean.
    def test_redundant_parens_around_unrelated_argument_is_not_flagged(self):
        self.assert_clean('foo(("just a literal"), c)')
        self.assert_clean('foo((a + b), c)')

    # DEBUG_ASSERTLOG's/DEBUG_ASSERTCRASH's condition and DEBUG_LOG_LEVEL's/DEBUG_LOG_LEVEL_RAW's level
    # are each the call's own first, unwrapped argument, not a doubled-parens list, so the generic
    # detector must not reach into them even when the stand-in sits there bare.
    def test_unwrapped_macro_argument_is_not_flagged_generically(self):
        self.assert_clean('DEBUG_ASSERTLOG(BitDepth, ("bits %d", 1))')
        self.assert_clean('DEBUG_LOG_LEVEL(BitDepth, ("bits %d", 1))')


class StripTest(unittest.TestCase):
    def test_comments_and_literals_are_blanked_keeping_length_and_lines(self):
        text = 'a // x\nb /* y\nz */ c "s\\"q" \'d\' e\n'
        out = ecs.strip_comments_and_strings(text)
        self.assertEqual(len(out), len(text))
        self.assertEqual(out.count("\n"), text.count("\n"))
        for blanked in ("x", "y", "z", "s", "q", "d"):
            self.assertNotIn(blanked, out.replace("a", "").replace("b", ""))

    def test_backslash_newline_inside_a_string_keeps_the_newline(self):
        text = '"ab\\\ncd" e\n'
        out = ecs.strip_comments_and_strings(text)
        self.assertEqual(out.count("\n"), text.count("\n"))
        self.assertTrue(out.rstrip().endswith("e"))

    def test_unterminated_literal_ends_at_the_line_end(self):
        out = ecs.strip_comments_and_strings("#error don't\nint x;\n")
        self.assertEqual(out.splitlines()[1], "int x;")


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
        # Unrelated.cpp carries a REAL unqualified use (not just a stripped comment), so a gate that
        # treats every file as "own" fails this test.
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
        # A file that only CALLS a member (no definition) does not become that class's own file: a
        # gate that drops the prefix/definition-shape guard fails this test.
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
        # definition: it must not make the file C's own (dx8wrapper.cpp/ww3d.cpp-shaped).
        # dx8wrapper.cpp carries a REAL flaggable use of the local `FrameCount` macro (a file with
        # nothing to flag either way cannot show whether it was treated as WW3D's own).
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
            'void DX8Wrapper::Log() { WWDEBUG_SAY(("dx8 frame %lu", FrameCount)); }\n'
            "unsigned DX8Wrapper::Other()\n"
            "{\n"
            "    return WW3D::Get_Frame_Count();\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_ternary_call_is_not_own_file(self):
        # `cond ? C::member() : x` has a `:` after the call's close paren, but it is a ternary, not a
        # constructor initialiser: it must not make the file C's own.
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
            'void DX8Wrapper::Log() { WWDEBUG_SAY(("dx8 frame %lu", FrameCount)); }\n'
            "unsigned DX8Wrapper::Other(bool b)\n"
            "{\n"
            "    return b ? WW3D::Get_Frame_Count() : 0;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_case_label_call_is_not_own_file(self):
        # `case C::member():` also has a `:` after the call's close paren, and is also not a definition.
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
            'void DX8Wrapper::Log() { WWDEBUG_SAY(("dx8 frame %lu", FrameCount)); }\n'
            "void DX8Wrapper::Other(int b)\n"
            "{\n"
            "    switch (b) {\n"
            "    case WW3D::Get_Frame_Count():\n"
            "        break;\n"
            "    }\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_constructor_initialiser_is_own_file(self):
        # `C::C() : m(0) { ... }`: the `:` after the constructor's close paren IS a definition's own
        # shape (its member name equals the class name), unlike a ternary's or a case label's `:`.
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
            "MapObject::MapObject() : m_next(0)\n"
            "{\n"
            "    MapObject** link = &TheMapObjectListPtr;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("WorldHeightMap.cpp", out)

    def test_const_method_is_own_file(self):
        # `C::member() const { ... }`: a trailing `const` before the `{` is still a definition's own
        # shape.
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
            "int MapObject::get() const\n"
            "{\n"
            "    MapObject** link = &TheMapObjectListPtr;\n"
            "    return 0;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("WorldHeightMap.cpp", out)

    def test_derived_class_member_file_is_own_file(self):
        # A file that defines only a member of a class D deriving from C can still name C's static
        # member unqualified (ordinary unqualified lookup finds an inherited member): it must be C's own
        # file too (W3DAssetManager/WW3DAssetManager-shaped). D's own header
        # (W3DAssetManager.h) never mentions the stand-in's name, so it is found by its base clause alone.
        self.write(
            "Core/WWAssetManager.h",
            "class WWAssetManager {\n"
            "public:\n"
            "    static constexpr rts::ContextField<WWAssetManager*, &rts::EngineContext::assetManager> "
            "TheInstance{};\n"
            "};\n",
        )
        self.write(
            "Core/W3DAssetManager.h",
            '#include "WWAssetManager.h"\n'
            "class W3DAssetManager : public WWAssetManager {\n"
            "public:\n"
            "    void Load();\n"
            "};\n",
        )
        self.write(
            "Core/W3DAssetManager.cpp",
            '#include "W3DAssetManager.h"\n'
            "void W3DAssetManager::Load()\n"
            "{\n"
            "    WWAssetManager** link = &TheInstance;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("W3DAssetManager.cpp", out)
        self.assertIn("TheInstance", out)

    def test_unrelated_class_member_file_is_not_own_file(self):
        # A class that does NOT derive from C must not be treated as C's own, even if it shares no name
        # with C: the derived-class lookup must not over-match.
        self.write(
            "Core/WWAssetManager.h",
            "class WWAssetManager {\n"
            "public:\n"
            "    static constexpr rts::ContextField<WWAssetManager*, &rts::EngineContext::assetManager> "
            "TheInstance{};\n"
            "};\n",
        )
        self.write(
            "Core/Unrelated.h",
            "class Unrelated {\n"
            "public:\n"
            "    void Load();\n"
            "};\n",
        )
        self.write(
            "Core/Unrelated.cpp",
            '#include "Unrelated.h"\n#include "WWAssetManager.h"\n'
            "void Unrelated::Load()\n"
            "{\n"
            "    WWAssetManager** link = &TheInstance;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_pointer_attached_to_class_name_is_own_file(self):
        # `T *C::member()`/`T* C::member()`: the `*`/`&` may attach to either word.
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
        # `C::member(` line itself.
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

    def test_other_game_tree_with_same_class_name_is_not_flagged(self):
        # A bare class name is not unique across the two games' own trees: GeneralsMD's
        # ScriptList has a stand-in for m_curId, but Generals has its own, unrelated ScriptList with a
        # real static of the same name. Correct Generals code using its own static must stay clean.
        self.write(
            "GeneralsMD/Scripts.h",
            "class ScriptList {\n"
            "public:\n"
            "    static constexpr rts::ContextField<int, &rts::EngineContext::scriptListCurId> m_curId{};\n"
            "};\n",
        )
        self.write(
            "Generals/Scripts.h",
            "class ScriptList {\n"
            "public:\n"
            "    static int m_curId;\n"
            "    void reset();\n"
            "};\n",
        )
        self.write(
            "Generals/Scripts.cpp",
            '#include "Scripts.h"\n'
            "void ScriptList::reset()\n"
            "{\n"
            "    Int* p = &ScriptList::m_curId;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 0, out)

    def test_other_game_tree_stand_in_is_still_checked(self):
        # The same scenario, but the violation is in GeneralsMD's own tree (the stand-in's own game):
        # scoping must not swallow a real violation, only the cross-game false one.
        self.write(
            "GeneralsMD/Scripts.h",
            "class ScriptList {\n"
            "public:\n"
            "    static constexpr rts::ContextField<int, &rts::EngineContext::scriptListCurId> m_curId{};\n"
            "};\n",
        )
        self.write(
            "Generals/Scripts.h",
            "class ScriptList {\n"
            "public:\n"
            "    static int m_curId;\n"
            "};\n",
        )
        self.write(
            "GeneralsMD/user.cpp",
            '#include "Scripts.h"\n' "Int* p = &ScriptList::m_curId;\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("GeneralsMD", out)
        self.assertIn("m_curId", out)

    def test_qualified_use_through_derived_class_name_is_checked(self):
        # `&D::name` names the same inherited stand-in as `&C::name`, so the qualified
        # scan must run for D's name too, not only C's.
        self.write(
            "Core/WWAssetManager.h",
            "class WWAssetManager {\n"
            "public:\n"
            "    static constexpr rts::ContextField<WWAssetManager*, &rts::EngineContext::assetManager> "
            "TheInstance{};\n"
            "};\n",
        )
        self.write(
            "Core/W3DAssetManager.h",
            '#include "WWAssetManager.h"\n'
            "class W3DAssetManager : public WWAssetManager {\n"
            "public:\n"
            "    void Load();\n"
            "};\n",
        )
        self.write(
            "Core/user.cpp",
            '#include "W3DAssetManager.h"\n'
            "WWAssetManager** p = &W3DAssetManager::TheInstance;\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("user.cpp", out)
        self.assertIn("TheInstance", out)

    def test_final_owner_class_is_not_skipped(self):
        # `class MapObject final { ... }`: CLASS's lazy prefix must not treat `final` as the class name.
        self.write(
            "Core/MapObject.h",
            "class MapObject final {\n"
            "public:\n"
            "    static constexpr rts::ContextField<MapObject*, &rts::EngineContext::mapObjectList> "
            "TheMapObjectListPtr{};\n"
            "};\n",
        )
        self.write(
            "Core/user.cpp",
            '#include "MapObject.h"\n' "auto *p = &MapObject::TheMapObjectListPtr;\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("TheMapObjectListPtr", out)
        self.assertNotIn("final::TheMapObjectListPtr", out)

    def test_final_derived_class_is_not_skipped(self):
        # `class W3DAssetManager final : public WW3DAssetManager { ... }`: BASE_CLAUSE's lazy prefix has
        # the same `final`-as-name bug, which drops the derived class out of the closure entirely
        # 
        self.write(
            "Core/WWAssetManager.h",
            "class WWAssetManager {\n"
            "public:\n"
            "    static constexpr rts::ContextField<WWAssetManager*, &rts::EngineContext::assetManager> "
            "TheInstance{};\n"
            "};\n",
        )
        self.write(
            "Core/W3DAssetManager.h",
            '#include "WWAssetManager.h"\n'
            "class W3DAssetManager final : public WWAssetManager {\n"
            "public:\n"
            "    void Track() { WWAssetManager** link = &TheInstance; }\n"
            "};\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("TheInstance", out)

    def test_derived_class_inline_body_is_own_file(self):
        # A derived class's own class body is the base's own file too: an inline member body written directly inside D's class declaration has no `D::` qualifier at
        # all. D's own header (here ViewerAssetMgr.h-shaped) is
        # found by its base clause, same as the out-of-class case, and must be scanned too.
        self.write(
            "Core/WWAssetManager.h",
            "class WWAssetManager {\n"
            "public:\n"
            "    static constexpr rts::ContextField<WWAssetManager*, &rts::EngineContext::assetManager> "
            "TheInstance{};\n"
            "};\n",
        )
        self.write(
            "Core/W3DAssetManager.h",
            '#include "WWAssetManager.h"\n'
            "class W3DAssetManager : public WWAssetManager {\n"
            "public:\n"
            "    void Track() { WWAssetManager** link = &TheInstance; }\n"
            "};\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("W3DAssetManager.h", out)
        self.assertIn("TheInstance", out)

    def test_transitive_derived_class_member_file_is_own_file(self):
        # E : D : C (D itself has no stand-in of its own): the closure must follow more than one level
        # (a direct-only closure would pass every other test here but this one).
        self.write(
            "Core/WWAssetManager.h",
            "class WWAssetManager {\n"
            "public:\n"
            "    static constexpr rts::ContextField<WWAssetManager*, &rts::EngineContext::assetManager> "
            "TheInstance{};\n"
            "};\n",
        )
        self.write(
            "Core/W3DAssetManager.h",
            '#include "WWAssetManager.h"\n'
            "class W3DAssetManager : public WWAssetManager {\n"
            "public:\n"
            "    void Load();\n"
            "};\n",
        )
        self.write(
            "Core/DX8AssetManager.h",
            '#include "W3DAssetManager.h"\n'
            "class DX8AssetManager : public W3DAssetManager {\n"
            "public:\n"
            "    void Reload();\n"
            "};\n",
        )
        self.write(
            "Core/DX8AssetManager.cpp",
            '#include "DX8AssetManager.h"\n'
            "void DX8AssetManager::Reload()\n"
            "{\n"
            "    WWAssetManager** link = &TheInstance;\n"
            "}\n",
        )
        code, out = self.run_main(["--root", self.root])
        self.assertEqual(code, 1, out)
        self.assertIn("DX8AssetManager.cpp", out)
        self.assertIn("TheInstance", out)

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
