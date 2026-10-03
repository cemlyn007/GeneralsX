#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 02/10/2026 Tests for engine_state_symbols.py's classification logic (PLAN-023)
#
# Table-driven coverage for the regex/branch defects a code review of this script found: rule:const and
# rule:namekey over-matching reassignable pointers/containers (SAFE_FOR_NEW must never pass those), the
# owner-definition search taking a qualified call for a real definition, find_all dropping sites in a
# different branch, resolve_sources picking an unresolved candidate for a count-1 name, and the new-defining-
# file policy in `compare`. Run with `python3 -m unittest scripts/cpp/engine_state_symbols_test.py` (or
# pytest); no bazel target needed, same as the script itself.

import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import engine_state_symbols as m  # noqa: E402


def make_symbol(key, decls, count=1, source="x.cpp:1", sections=None):
    sym = m.Symbol(key)
    sym.decls = decls if len(decls) > 1 else []
    sym.decl = decls[0] if decls else ""
    sym.count = count
    sym.source = source
    sym.sections = sections or set()
    sym.parsed = m.Parsed(key)
    return sym


class RuleConstObjectTest(unittest.TestCase):
    """rule:const is SAFE_FOR_NEW: it must never pass a reassignable pointer or reference."""

    def assert_const(self, decl, expect, msg=None):
        got = m.rule_const_object(make_symbol("s", [decl])) is not None
        self.assertEqual(got, expect, msg or decl)

    def test_plain_const_is_safe(self):
        self.assert_const("static const int x = 5;", True)

    def test_const_pointer_to_single_word_type_is_not_safe(self):
        # A pointer-to-const (the pointer itself is reassignable) is never a const *object*.
        self.assert_const("static const unsigned int* p", False)

    def test_const_pointer_to_qualified_type_is_not_safe(self):
        self.assert_const("static const Foo::Bar* p", False)

    def test_const_pointer_to_template_type_is_not_safe(self):
        self.assert_const("static const std::vector<int>* p", False)

    def test_const_pointer_to_struct_is_not_safe(self):
        self.assert_const("static const struct Foo* p", False)

    def test_const_pointer_to_char_is_not_safe(self):
        # `const char*` is a mutable pointer variable, whatever the pointee; not `* const`.
        self.assert_const('static const char* p = "x";', False)

    def test_reseatable_pointer_with_literal_initializer_is_not_safe(self):
        self.assert_const("static Foo* p = nullptr;", False)

    def test_const_pointer_with_literal_initializer_is_safe(self):
        self.assert_const("static Foo* const p = &kDefault;", True)

    def test_const_pointer_with_lookup_initializer_is_not_safe(self):
        # The per-engine pointer-cache shape (ActiveBody, WaveGuideUpdate, ...): never written again, but the
        # one-time lookup differs per engine.
        self.assert_const('static ThingTemplate* const tmpl = TheThingFactory->findTemplate("X");', False)

    def test_const_pointer_with_call_initializer_is_not_safe(self):
        self.assert_const("static Foo* const p = makeDefault();", False)

    def test_const_reference_is_never_safe(self):
        self.assert_const("static const T& r = *TheX->find(1);", False)

    def test_by_value_const_initialized_through_a_global_pointer_is_not_safe(self):
        # A pointer need not be involved: a by-value const that reads TheGlobalData (or any other global
        # pointer) at static-init time captures whichever engine's INI/map data ran first.
        self.assert_const("static const Real r = TheGlobalData->m_maxCameraHeight;", False)

    def test_by_value_const_initialized_through_a_global_pointer_string_is_not_safe(self):
        self.assert_const("static const AsciiString s = TheGlobalData->m_mapName;", False)

    def test_by_value_const_initialized_from_an_allow_listed_constructor_is_safe(self):
        # Only an allow-listed pure value constructor passes; a `->` read is never involved here.
        self.assert_const("static const Int c = GameMakeColor(255, 255, 255, 255);", True)

    def test_by_value_const_initialized_from_its_own_declared_type_is_safe(self):
        # `Type var = Type();`: a plain default-constructed value of its own declared type, never a read of
        # anything (the s_emptyWaypoints shape: `static const WaypointMap s_emptyWaypoints = WaypointMap();`).
        self.assert_const("static const WaypointMap s_emptyWaypoints = WaypointMap();", True)

    def test_by_value_const_initialized_from_an_rng_call_is_not_safe(self):
        # GameLogicRandomValue reads/advances the per-engine logic RNG: not a pure value constructor, even
        # though its own arguments are literals, so a literal-argument check alone would miss it.
        self.assert_const("static const Int pick = GameLogicRandomValue(0, 3);", False)

    def test_by_value_const_initialized_from_a_non_allow_listed_getter_is_not_safe(self):
        self.assert_const("static const Real h = getMaxCameraHeight();", False)

    def test_by_value_const_initialized_from_an_indexed_method_call_is_not_safe(self):
        self.assert_const("static const Int n = ThePlayerList[0].getPlayerCount();", False)

    def test_by_value_const_direct_initialized_from_an_rng_call_is_not_safe(self):
        # Direct-initialisation (`name(expr)`, no `=`) must be read the same as `name = expr`: splitting
        # only on `=` leaves the whole call after the declared name instead of in the checked initialiser.
        self.assert_const("static const Int pick(GameLogicRandomValue(0, 3));", False)

    def test_const_pointer_direct_initialized_from_a_lookup_is_not_safe(self):
        self.assert_const('static ThingTemplate* const tmpl(TheThingFactory->findTemplate("X"));', False)

    def test_by_value_const_brace_initialized_through_a_global_pointer_is_not_safe(self):
        # Brace-initialisation (`name{expr}`) must be read the same as `name = expr`.
        self.assert_const("static const Real r{TheGlobalData->m_maxCameraHeight};", False)

    def test_by_value_const_brace_initialized_from_an_rng_call_is_not_safe(self):
        self.assert_const("static const Int pick{GameLogicRandomValue(0, 3)};", False)

    def test_by_value_const_direct_initialized_with_literal_is_safe(self):
        self.assert_const("static const Int pick(3);", True)

    def test_by_value_const_brace_initialized_with_literal_is_safe(self):
        self.assert_const("static const Int pick{3};", True)

    def test_by_value_const_with_initializer_on_a_later_source_line_is_not_safe(self):
        # The recorded declaration is one source line; when the initialiser starts on the next one, the
        # line this rule sees ends at a bare `=` and must not be read as having no (so trivially safe)
        # initialiser at all.
        self.assert_const("static const Int pick =", False)

    def test_by_value_const_with_unclosed_direct_initializer_is_not_safe(self):
        self.assert_const("static const Int pick(GameLogicRandomValue(0, 3)", False)

    def test_by_value_const_with_unclosed_brace_initializer_is_not_safe(self):
        self.assert_const("static const Real r{TheGlobalData->m_maxCameraHeight", False)

    def test_by_value_const_with_no_initializer_is_still_safe(self):
        # A default-constructed const (no `=`, `(` or `{` at all) has no initialiser to distrust.
        self.assert_const("static const WaypointMap s_emptyWaypoints;", True)

    def test_every_tu_must_qualify(self):
        sym = make_symbol(
            "s",
            ["static const int x = 1;", "static const Foo::Bar* p"],
        )
        self.assertIsNone(m.rule_const_object(sym))

    def test_array_with_a_call_bound_reading_a_global_pointer_is_not_safe(self):
        # The bound's own `(...)` (ARRAY_SIZE(x)) must never be mistaken for the real `= {...}` initialiser
        # that follows it: the scan for the initialiser boundary must skip past `[...]` entirely.
        self.assert_const(
            "static const Real s_heights[ARRAY_SIZE(s_names)] = { TheGlobalData->m_maxCameraHeight, 1.0f };",
            False,
        )

    def test_array_with_a_sizeof_bound_initialized_from_an_rng_call_is_not_safe(self):
        self.assert_const("static const Int s_picks[sizeof(Int)] = { GameLogicRandomValue(0, 3) };", False)

    def test_array_with_a_call_bound_and_literal_initializer_is_safe(self):
        self.assert_const("static const Real s_literals[ARRAY_SIZE(s_names)] = { 1.0f, 2.0f };", True)


class RuleNamekeyTest(unittest.TestCase):
    """rule:namekey is SAFE_FOR_NEW: the variable itself must be a NameKeyType/StaticNameKey cache, not a
    container or struct that merely mentions one."""

    def assert_namekey(self, decl, expect, msg=None):
        got = m.rule_namekey(make_symbol("s", [decl])) is not None
        self.assertEqual(got, expect, msg or decl)

    def test_namekey_from_macro_is_safe(self):
        self.assert_namekey('static NameKeyType s_key = NAMEKEY("Foo");', True)

    def test_const_namekey_with_no_initializer_is_safe(self):
        self.assert_namekey("static const NameKeyType s_key", True)

    def test_namekey_from_generator_call_is_safe(self):
        self.assert_namekey('static NameKeyType s_key = TheNameKeyGenerator->nameToKey("Foo");', True)

    def test_mutable_namekey_reassigned_to_a_constant_is_not_safe(self):
        self.assert_namekey("static NameKeyType s_lastSelected = NAMEKEY_INVALID;", False)

    def test_container_of_namekeys_is_not_safe(self):
        self.assert_namekey("static std::map<NameKeyType, Object*> s_byKey;", False)

    def test_struct_mentioning_namekey_is_not_safe(self):
        self.assert_namekey("static StructWithANameKeyType s_thing;", False)


class RuleFieldParseTest(unittest.TestCase):
    """rule:fieldparse decides by declared type, not by the variable's name alone."""

    def test_fieldparse_table_is_safe(self):
        sym = make_symbol("s", ["static const FieldParse myFieldParse[] = {};"])
        self.assertIsNotNone(m.rule_field_parse(sym))

    def test_same_named_non_fieldparse_variable_is_not_safe(self):
        sym = make_symbol("s", ["static int myFieldParse = 0;"])
        self.assertIsNone(m.rule_field_parse(sym))

    def test_mutable_fieldparse_pointer_is_not_safe(self):
        # `FieldParse` appears in the type, but the declarator is a reassignable pointer, not a const array.
        sym = make_symbol("s", ["static FieldParse* s_cursor = nullptr;"])
        self.assertIsNone(m.rule_field_parse(sym))

    def test_map_keyed_by_fieldparse_pointer_is_not_safe(self):
        sym = make_symbol("s", ["static std::map<AsciiString, const FieldParse*> s_byName;"])
        self.assertIsNone(m.rule_field_parse(sym))

    def test_non_const_fieldparse_array_is_not_safe(self):
        sym = make_symbol("s", ["static FieldParse s_scratch[16];"])
        self.assertIsNone(m.rule_field_parse(sym))


class CodeOnlyTest(unittest.TestCase):
    def test_comment_and_string_contents_are_blanked_not_removed(self):
        text = 'int x = 1; // static const Foo* p\nconst char* s = "static const Bar* q";\n'
        blanked = m.code_only(text)
        self.assertNotIn("Foo", blanked)
        self.assertNotIn("Bar", blanked)
        # Newlines and non-comment/string code are preserved, so line numbers do not shift.
        self.assertEqual(blanked.count("\n"), text.count("\n"))
        self.assertIn("int x = 1;", blanked)


class SourceIndexDefinitionTest(unittest.TestCase):
    """Exercises SourceIndex.find/find_all against small synthetic source trees, the way `find` finds the
    real definition for a demangled symbol key."""

    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = self._tmp.name
        os.makedirs(os.path.join(self.root, "Core"))
        self._old_scan_roots = m.SCAN_ROOTS
        m.SCAN_ROOTS = ["Core"]

    def tearDown(self):
        m.SCAN_ROOTS = self._old_scan_roots
        self._tmp.cleanup()

    def write(self, rel, text):
        path = os.path.join(self.root, "Core", rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w") as f:
            f.write(text)

    def index(self, wanted):
        return m.SourceIndex(self.root, set(wanted))

    def test_owner_match_skips_a_qualified_call_site(self):
        # A derived override's body calls the base method by its qualified name; that call site is not the
        # base method's own definition and must not be mistaken for it.
        self.write(
            "derived.cpp",
            "int Derived::func(const char* name) {\n"
            "    return Base::Create_Render_Obj(name);\n"
            "}\n",
        )
        self.write(
            "assetmgr.cpp",
            "Foo* Base::Create_Render_Obj(char const* name) {\n"
            "    static int warning_count = 0;\n"
            "    return nullptr;\n"
            "}\n",
        )
        idx = self.index({"warning_count", "Create_Render_Obj", "Base"})
        p = m.Parsed("Base::Create_Render_Obj(char const*)::warning_count")
        source, decl, _indented = idx.find(p)
        self.assertEqual(source, "Core/assetmgr.cpp:2")
        self.assertIn("warning_count", decl)

    def test_inline_fallback_skips_a_call_before_the_real_definition(self):
        # No class (the inline-fallback branch): a call to the free function earlier in the same file must
        # not be mistaken for its definition.
        self.write(
            "helper.cpp",
            "void doSomething() {\n"
            "    helper();\n"
            "}\n"
            "\n"
            "int helper() {\n"
            "    static int counter = 7;\n"
            "    return counter;\n"
            "}\n",
        )
        idx = self.index({"helper", "counter"})
        p = m.Parsed("helper()::counter")
        source, decl, _indented = idx.find(p)
        self.assertEqual(source, "Core/helper.cpp:6")
        self.assertIn("= 7", decl)

    def test_find_all_collects_sites_of_different_plain_ranks(self):
        # One TU defines the namespace member at file scope, another defines it indented inside the
        # namespace block: different `plain` ranks, both must be recorded.
        self.write("gui.cpp", "static int lastUpdate = 0;\n")
        self.write("logic.cpp", "namespace Foo {\n    static int lastUpdate = 0;\n}\n")
        idx = self.index({"lastUpdate"})
        p = m.Parsed("lastUpdate")
        sites = idx.find_all(p)
        files = {site.split(":")[0] for site, _decl in sites}
        self.assertEqual(files, {"Core/gui.cpp", "Core/logic.cpp"})

    def test_resolve_sources_marks_a_count_1_name_with_multiple_candidates_ambiguous(self):
        # Two TUs define the same-named static, but only one is ever referenced (so the library has one
        # instance): resolve_sources must not silently pick a candidate, it must record the ambiguity.
        self.write("ControlBarPopupDescription.cpp", "static GameWindow* theWindow = NULL;\n")
        self.write("Diplomacy.cpp", "static GameWindow* theWindow = NULL;\n")
        sym = m.Symbol("theWindow")
        sym.count = 1
        symbols = {"theWindow": sym}
        m.resolve_sources(self.root, symbols, {})
        sites = sym.source.split(m.SITE_SEP)
        self.assertEqual(len(sites), 2)
        self.assertEqual(len(sym.decls), 2)

    def test_resolve_sources_count_1_single_candidate_is_unchanged(self):
        self.write("only.cpp", "static int s_once = 0;\n")
        sym = m.Symbol("s_once")
        sym.count = 1
        symbols = {"s_once": sym}
        m.resolve_sources(self.root, symbols, {})
        self.assertEqual(sym.source, "Core/only.cpp:1")
        self.assertEqual(sym.decls, [])

    def test_class_static_with_header_declaration_is_not_ambiguous(self):
        # A class static declared in the header and defined out-of-line in one .cpp is one definition, not
        # two: the in-class declaration must not count as a second site once the qualified one is found.
        self.write("dx8wrapper.h", "class DX8Wrapper {\n    static int BitDepth;\n    void Foo();\n};\n")
        self.write(
            "dx8wrapper.cpp",
            "int DX8Wrapper::BitDepth = 32;\n"
            "void DX8Wrapper::Foo() {\n"
            "    static const int table = 7;\n"
            "}\n",
        )
        sym = m.Symbol("DX8Wrapper::BitDepth")
        sym.count = 1
        symbols = {"DX8Wrapper::BitDepth": sym}
        m.resolve_sources(self.root, symbols, {})
        self.assertEqual(sym.source, "Core/dx8wrapper.cpp:1")
        self.assertEqual(sym.decls, [])

    def test_member_function_static_does_not_gain_header_class_line(self):
        # The member function has a real out-of-line body with the static inside it: the class-line macro
        # fallback must not also be recorded as a second site.
        self.write("dx8wrapper.h", "class DX8Wrapper {\n    void Foo();\n};\n")
        self.write(
            "dx8wrapper.cpp",
            "void DX8Wrapper::Foo() {\n    static const int table = 7;\n}\n",
        )
        sym = m.Symbol("DX8Wrapper::Foo()::table")
        sym.count = 1
        symbols = {"DX8Wrapper::Foo()::table": sym}
        m.resolve_sources(self.root, symbols, {})
        self.assertEqual(sym.source, "Core/dx8wrapper.cpp:2")
        self.assertEqual(sym.decls, [])

    def test_macro_defined_function_body_allows_backslash_continuation(self):
        # DEFINE_AUTO_POOL-style macro: the function's `)` and `{` are split across continuation lines.
        self.write(
            "mempool.h",
            "class AutoPoolClass {\n"
            "public:\n"
            "    static Pool* Allocator() \\\n"
            "    { \\\n"
            "        static Pool* const allocator = new Pool(); \\\n"
            "        return allocator; \\\n"
            "    }\n"
            "};\n",
        )
        p = m.Parsed("AutoPoolClass::Allocator()::allocator")
        idx = self.index({"allocator", "Allocator", "AutoPoolClass"})
        source, decl, _indented = idx.find(p)
        self.assertEqual(source, "Core/mempool.h:5")
        self.assertIn("allocator", decl)
        self.assertNotIn("defined by a macro", decl)

    def test_plain_scope_skips_an_unrelated_local_in_another_function(self):
        # A local (non-static) variable of the same bare name inside some unrelated function must never be
        # taken for this namespace-scope symbol's own definition (the nlohmann::json `parent` collision).
        self.write(
            "json.hpp",
            "namespace nlohmann {\n"
            "void foo() {\n"
            "    basic_json& parent = result.at(ptr);\n"
            "}\n"
            "}\n",
        )
        p = m.Parsed("parent")
        idx = self.index({"parent"})
        source, _decl, _indented = idx.find(p)
        self.assertEqual(source, "?")

    def test_inline_branch_does_not_cross_into_an_unrelated_classs_method(self):
        # A derived class calls the base class's method by its qualified name from its OWN same-named
        # method's body; the inline branch's bare (unqualified) regex must not wander into that unrelated
        # method's body and steal its static for the base class's symbol (the WW3DAssetManager /
        # W3DAssetManager::Create_Render_Obj collision, both sharing one file through such a call). Both
        # definitions are kept in ONE file here so that `find_all`'s one-site-per-file dedup cannot hide a
        # false match recorded for the same file as the real one; see the next two tests for the dedup-proof
        # cross-file cases.
        self.write(
            "mgr.cpp",
            "class Derived {\n"
            "    Foo* Create_Render_Obj(const char* name) {\n"
            "        if (!Find(name)) {\n"
            "            return Base::Create_Render_Obj(name);\n"
            "        }\n"
            "        static int warning_count = 0;\n"
            "        return nullptr;\n"
            "    }\n"
            "};\n"
            "Foo* Base::Create_Render_Obj(char const* name) {\n"
            "    static int warning_count = 1;\n"
            "    return nullptr;\n"
            "}\n",
        )
        p = m.Parsed("Base::Create_Render_Obj(char const*)::warning_count")
        idx = self.index({"warning_count", "Create_Render_Obj", "Base", "Derived"})
        sites = idx.find_all(p)
        self.assertEqual(len(sites), 1)
        self.assertEqual(sites[0][0], "Core/mgr.cpp:11")

    def test_inline_branch_does_not_steal_an_unrelated_classs_static_from_another_file(self):
        # The same collision as above, but with Base's real out-of-line definition and Derived's unrelated
        # inline method in SEPARATE files (Base declared in its own header, as every out-of-line `Base::`
        # definition implies, so `is_class_name` can tell Base is a real class): find_all's one-site-per-
        # file dedup cannot mask a false match here, so this is the case that actually proves the inline
        # branch is restricted to its own class's body.
        self.write("base.h", "class Base {\npublic:\n    Foo* Create_Render_Obj(const char* name);\n};\n")
        self.write(
            "base.cpp",
            "Foo* Base::Create_Render_Obj(char const* name) {\n"
            "    static int warning_count = 1;\n"
            "    return nullptr;\n"
            "}\n",
        )
        # Derived inherits from Base (so "Base" is a candidate word of this file too, the way Derived's own
        # call to Base::Create_Render_Obj was in the single-file test above) and overrides the same method
        # inline, unqualified, with its own unrelated static of the same name.
        self.write(
            "derived.h",
            "class Derived : public Base {\n"
            "    Foo* Create_Render_Obj(const char* name) {\n"
            "        static int warning_count = 0;\n"
            "        return nullptr;\n"
            "    }\n"
            "};\n",
        )
        p = m.Parsed("Base::Create_Render_Obj(char const*)::warning_count")
        idx = self.index({"warning_count", "Create_Render_Obj", "Base", "Derived"})
        sites = idx.find_all(p)
        self.assertEqual(len(sites), 1)
        self.assertEqual(sites[0][0], "Core/base.cpp:2")

    def test_inline_branch_finds_each_siblings_own_static_in_a_shared_file(self):
        # The actual FXList.cpp shape: several unrelated classes, each with its own inline, unqualified
        # `parse` method and its own same-named static, all in ONE file, with no out-of-line definition at
        # all (so the owner branch never matches and there is nothing for find_all's per-file dedup to hide
        # a false match behind). Unrestricted, the inline branch would always return the FIRST class's own
        # site for every other class's symbol.
        self.write(
            "nuggets.cpp",
            "class FirstNugget {\n"
            "    static void parse() {\n"
            "        static const FieldParse myFieldParse[] = {};\n"
            "    }\n"
            "};\n"
            "class SecondNugget {\n"
            "    static void parse() {\n"
            "        static const FieldParse myFieldParse[] = {};\n"
            "    }\n"
            "};\n",
        )
        p = m.Parsed("SecondNugget::parse()::myFieldParse")
        idx = self.index({"myFieldParse", "parse", "FirstNugget", "SecondNugget"})
        source, _decl, _indented = idx.find(p)
        self.assertEqual(source, "Core/nuggets.cpp:8")

    def test_inline_branch_ignores_another_classs_qualified_out_of_line_definition(self):
        # A file that holds only some OTHER class's qualified out-of-line definition, but still mentions
        # Cls (a member, so `candidates(var, func, cls)` makes it a real candidate: without that, the
        # file is never looked at by either branch and this test would pass no matter what the class-body
        # restriction did). The inline branch's bare `func(` regex must not take it for Cls::func()'s own
        # definition just because no qualifier differs from Cls. Cls is declared (as any real `Cls::`
        # reference would require) but never defines `func` anywhere, inline or out of line, so its own
        # (empty) class body correctly rules the match out; Other's site must never be the answer (the
        # class-level macro fallback, a separate feature, may still answer from Cls's own declaration line
        # instead).
        self.write("cls.h", "class Cls {\npublic:\n    void unrelated();\n};\n")
        self.write(
            "other.cpp",
            "Foo* Other::func(char const* name) {\n"
            "    static int v = 9;\n"
            "    Cls* owner = nullptr;\n"
            "    return nullptr;\n"
            "}\n",
        )
        p = m.Parsed("Cls::func(char const*)::v")
        idx = self.index({"v", "func", "Cls", "Other"})
        self.assertIn("Core/other.cpp", idx.candidates("v", "func", "Cls"))
        source, _decl, _indented = idx.find(p)
        self.assertNotEqual(source, "Core/other.cpp:2")

    def test_inline_branch_finds_a_static_in_a_function_nested_in_a_namespace(self):
        # `cls` here is "ns", a namespace, not a class: is_class_name("ns") is False (no `class`/`struct
        # ns` exists anywhere), so the inline branch's class-body restriction must not apply at all. Without
        # that guard, `class_bodies(rel, "ns")` finds no body either (there is no such class), so
        # `class_spans` would come back `[]` (restricted to nothing) rather than `None` (unrestricted), and
        # the inline branch would skip every candidate, including this file's own real definition.
        self.write(
            "util.cpp",
            "namespace ns {\n"
            "inline int f() {\n"
            "    static int v = 0;\n"
            "    return v;\n"
            "}\n"
            "}\n",
        )
        p = m.Parsed("ns::f()::v")
        idx = self.index({"v", "f", "ns"})
        self.assertFalse(idx.is_class_name("ns"))
        source, decl, _indented = idx.find(p)
        self.assertEqual(source, "Core/util.cpp:3")
        self.assertIn("v", decl)

    def test_plain_scope_keeps_a_column_0_static_after_an_unbalanced_preprocessor_brace(self):
        # `_scope_events` counts braces without evaluating preprocessor branches, so an `#ifndef ... #else
        # ... #endif` pair that each open a `{` (PartitionManager.cpp's shape) leaves one bogus "function"
        # scope open for the rest of the file. A column-0 file-scope definition after it is never actually a
        # local and must still be found.
        self.write(
            "partition.cpp",
            "int g() {\n"
            "    return 1;\n"
            "}\n"
            "#ifndef DISABLE_X\n"
            "void h() {\n"
            "#else\n"
            "void h() {\n"
            "#endif\n"
            "    doThing();\n"
            "}\n"
            "static Real ringSpacing = 5.0f;\n",
        )
        p = m.Parsed("ringSpacing")
        idx = self.index({"ringSpacing"})
        source, _decl, indented = idx.find(p)
        self.assertEqual(source, "Core/partition.cpp:11")
        self.assertFalse(indented)

    def test_scope_at_sees_the_stack_an_else_restores_before_the_next_brace(self):
        # `_scope_events` resets `stack` at `#else`/`#elif` to the snapshot taken at the matching `#if`, but
        # (before this fix) only recorded that as a scope event at the next `{`/`}`: `scope_at` would bisect
        # to the stack the LAST brace left (the #if branch's own, still "inside a function") for every
        # offset between the `#else` and that next brace. `s_q` sits in exactly that window, genuinely at
        # file scope in the branch actually taken, and must still be found as a file static, not skipped as
        # an unrelated function's local.
        self.write(
            "cond.cpp",
            "#ifdef A\n"
            "void h() {\n"
            "  x();\n"
            "#else\n"
            "static int s_q = 1;\n"
            "void h() {\n"
            "#endif\n"
            "}\n",
        )
        p = m.Parsed("s_q")
        idx = self.index({"s_q"})
        sites = idx.find_all(p)
        self.assertEqual(sites, [("Core/cond.cpp:5", "static int s_q = 1;")])

    def test_plain_scope_skips_another_classs_static_member_declaration(self):
        # A file static elsewhere and an unrelated class's own static member of the same bare name must
        # never be merged into one ambiguous name: a class body's own member is the qualified/in-class
        # branches' job, not the plain branch's.
        self.write("a.cpp", "static const int s_count = 0;\n")
        self.write("b.h", "class Foo {\n    static int s_count;\n};\n")
        p = m.Parsed("s_count")
        idx = self.index({"s_count", "Foo"})
        sites = idx.find_all(p)
        self.assertEqual(len(sites), 1)
        self.assertEqual(sites[0][0], "Core/a.cpp:1")

    def test_plain_scope_keeps_a_real_namespace_block_static(self):
        # The genuine case this filtering must not break: a namespace-scope static indented inside a
        # `namespace { ... }` block, with no enclosing function.
        self.write("logic.cpp", "namespace Foo {\n    static int lastUpdate = 0;\n}\n")
        p = m.Parsed("lastUpdate")
        idx = self.index({"lastUpdate"})
        source, _decl, indented = idx.find(p)
        self.assertEqual(source, "Core/logic.cpp:2")
        self.assertTrue(indented)

    def test_find_reads_a_brace_initializer_that_opens_on_a_later_line(self):
        # `lines_with` slices one source line; a `static const T arr[] =` whose `{ ... }` opens on the
        # next line (the common StateConditionInfo/Matrix3D table shape) must still hand back the whole
        # initialiser, not just the declarator the first line holds, or a rule reading it (rule:const) sees
        # an empty one and cannot check what it actually contains.
        self.write(
            "table.cpp",
            "void f() {\n"
            "    static const int table[] =\n"
            "    {\n"
            "        1, 2, 3\n"
            "    };\n"
            "}\n",
        )
        p = m.Parsed("f()::table")
        idx = self.index({"table", "f"})
        _source, decl, _indented = idx.find(p)
        self.assertIn("{ 1, 2, 3 }", decl)
        self.assertTrue(decl.rstrip().endswith(";"))

    def test_rule_const_reads_a_global_pointer_read_past_the_old_160_char_cap(self):
        # End to end through resolve_sources and classify: a multi-line initialiser with a `->` read well
        # past the 160th character must still make rule:const refuse it, not stop at a truncated copy of
        # the declaration that never reaches the unsafe read.
        padding = ", ".join(f"{i}.0f" for i in range(1, 29))  # pushes the `->` read past character 160
        self.write(
            "longtable.cpp",
            "static const Real s_longtable[] =\n"
            "{\n"
            f"    {padding},\n"
            "    TheGlobalData->m_maxCameraHeight,\n"
            "};\n",
        )
        sym = m.Symbol("s_longtable")
        sym.count = 1
        symbols = {"s_longtable": sym}
        m.resolve_sources(self.root, symbols, {})
        self.assertGreater(len(sym.decl), 160)
        self.assertIn("->", sym.decl)
        m.classify(sym, symbols)
        self.assertNotEqual(sym.by, "rule:const")

    def test_find_does_not_extend_a_statement_that_already_ends_on_its_own_line(self):
        # The common, already-correct case must not be touched: a declaration complete on one line keeps
        # exactly that line (nothing from the next statement bleeds in).
        self.write("plain.cpp", "static const int x = 5;\nstatic const int y = 6;\n")
        p = m.Parsed("x")
        idx = self.index({"x"})
        _source, decl, _indented = idx.find(p)
        self.assertEqual(decl, "static const int x = 5;")


class CompareTest(unittest.TestCase):
    """compare() drives `check`'s error/warning split for a symbol the TSV already lists."""

    def row(self, **overrides):
        base = {"class": "render-only", "phase": "", "by": "hand", "count": "1", "source": "mesh.cpp:10"}
        base.update(overrides)
        return base

    def sym(self, count, source, cls="render-only", phase="", by="hand"):
        s = m.Symbol("k")
        s.count, s.source, s.cls, s.phase, s.by = count, source, cls, phase, by
        return s

    def test_count_1_new_defining_file_is_an_error(self):
        errors, stale = m.compare("temp_apt", self.sym(1, "logic.cpp:20"), self.row())
        self.assertTrue(any("new instance" in e for e in errors))
        self.assertEqual(stale, [])

    def test_count_1_moved_within_same_file_is_only_stale(self):
        errors, stale = m.compare("temp_apt", self.sym(1, "mesh.cpp:20"), self.row())
        self.assertEqual(errors, [])
        self.assertTrue(any("definition moved" in s for s in stale))

    def test_higher_count_is_always_an_error(self):
        errors, _stale = m.compare("k", self.sym(2, "mesh.cpp:10"), self.row(count="1"))
        self.assertTrue(any("new instance" in e for e in errors))

    def test_fewer_instances_is_stale(self):
        errors, stale = m.compare("k", self.sym(1, "mesh.cpp:10"), self.row(count="2"))
        self.assertEqual(errors, [])
        self.assertTrue(any("fewer instances" in s for s in stale))

    def test_class_change_is_an_error(self):
        errors, _stale = m.compare("k", self.sym(1, "mesh.cpp:10", cls="per-engine", phase="4"), self.row())
        self.assertTrue(any("class changed" in e for e in errors))

    def test_lost_definition_is_stale_not_a_new_instance(self):
        # The search can no longer find a listed definition (moved inside a macro, outside SCAN_ROOTS, ...):
        # `?` must not be read as a brand-new defining file.
        errors, stale = m.compare("k", self.sym(1, "?"), self.row())
        self.assertEqual(errors, [])
        self.assertTrue(any("definition moved" in s for s in stale))


class DemangleTest(unittest.TestCase):
    def test_empty_input_returns_empty_list_without_calling_c_filt(self):
        # A stripped library's `nm` yields no state symbols at all: demangle([]) must short-circuit rather
        # than sending c++filt a lone newline and tripping its own line-count mismatch guard.
        self.assertEqual(m.demangle([]), [])


class HandMatchesTest(unittest.TestCase):
    def test_exact_name(self):
        h = m.Hand((m.GLOBAL, "", "TheVersion", "note"))
        self.assertTrue(h.matches(make_symbol("TheVersion", [])))
        self.assertFalse(h.matches(make_symbol("TheOther", [])))

    def test_regex_is_fullmatch(self):
        h = m.Hand((m.GLOBAL, "", "re:s_\\w+", "note"))
        self.assertTrue(h.matches(make_symbol("s_foo", [])))
        self.assertFalse(h.matches(make_symbol("x_s_foo", [])))

    def test_file_pattern_requires_every_site(self):
        h = m.Hand((m.RENDER, "", "file:/GUI/", "note"))
        self.assertTrue(h.matches(make_symbol("k", [], source="a/GUI/x.cpp:1")))
        self.assertFalse(h.matches(make_symbol("k", [], source="a/GUI/x.cpp:1;b/Logic/y.cpp:2")))


class NewSymbolErrorTest(unittest.TestCase):
    def test_unreviewed_symbol_is_an_error(self):
        sym = make_symbol("k", [])
        sym.cls, sym.by = m.UNREVIEWED, "none"
        self.assertIsNotNone(m.new_symbol_error(sym, {}))

    def test_safe_rule_passes(self):
        sym = make_symbol("k", [])
        sym.cls, sym.by = m.CONST, "rule:const"
        self.assertIsNone(m.new_symbol_error(sym, {}))

    def test_hand_entry_never_passes_a_new_symbol(self):
        sym = make_symbol("k", [])
        sym.cls, sym.by = m.GLOBAL, "hand"
        self.assertIsNotNone(m.new_symbol_error(sym, {}))

    def test_file_pattern_never_passes_a_new_symbol(self):
        sym = make_symbol("k", [])
        sym.cls, sym.by = m.RENDER, "file"
        self.assertIsNotNone(m.new_symbol_error(sym, {}))

    def test_guard_of_a_listed_static_passes(self):
        sym = make_symbol("guard variable for TheThing", [])
        sym.cls, sym.by = m.GLOBAL, "guard"
        self.assertIsNone(m.new_symbol_error(sym, {"TheThing"}))

    def test_guard_of_an_unlisted_static_is_an_error(self):
        sym = make_symbol("guard variable for TheNewThing", [])
        sym.cls, sym.by = m.GLOBAL, "guard"
        self.assertIsNotNone(m.new_symbol_error(sym, {"TheThing"}))


class CmdCheckEmptyLibraryTest(unittest.TestCase):
    """A stripped or wrong-file library must fail `check` outright, not pass having checked nothing."""

    def test_no_symbols_exits_nonzero(self):
        # `read_tsv` is deliberately left unmocked (and never called): the empty-library guard must fire
        # from `load_library`'s result alone, before `read_tsv` runs, so a missing or unreadable TSV can
        # never make this test pass for the wrong reason (the bug this test is named for).
        args = mock.Mock(root=".", lib="lib.so", vcpkg_lib=None, strict=False)
        with mock.patch.object(m, "load_library", return_value={}):
            with mock.patch.object(m, "read_tsv", side_effect=AssertionError("read_tsv must not be called")):
                with self.assertRaises(SystemExit) as ctx:
                    m.cmd_check(args)
        self.assertNotEqual(ctx.exception.code, 0)
        self.assertIn("no writable/unique symbols found", str(ctx.exception.code))

    def test_mostly_vanished_symbols_exits_nonzero(self):
        args = mock.Mock(root=".", lib="lib.so", vcpkg_lib=None, strict=False)
        one_symbol = {"k": make_symbol("k", [], source="x.cpp:1")}
        with mock.patch.object(m, "load_library", return_value=one_symbol):
            with mock.patch.object(m, "read_tsv", return_value={str(i): {} for i in range(10)}):
                with self.assertRaises(SystemExit) as ctx:
                    m.cmd_check(args)
        self.assertNotEqual(ctx.exception.code, 0)


if __name__ == "__main__":
    unittest.main()
