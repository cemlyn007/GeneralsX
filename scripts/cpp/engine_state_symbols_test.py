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

    def test_reseatable_pointer_with_literal_initialiser_is_not_safe(self):
        self.assert_const("static Foo* p = nullptr;", False)

    def test_const_pointer_with_literal_initialiser_is_safe(self):
        self.assert_const("static Foo* const p = &kDefault;", True)

    def test_const_pointer_with_lookup_initialiser_is_not_safe(self):
        # The per-engine pointer-cache shape (ActiveBody, WaveGuideUpdate, ...): never written again, but the
        # one-time lookup differs per engine.
        self.assert_const('static ThingTemplate* const tmpl = TheThingFactory->findTemplate("X");', False)

    def test_const_pointer_with_call_initialiser_is_not_safe(self):
        self.assert_const("static Foo* const p = makeDefault();", False)

    def test_const_reference_is_never_safe(self):
        self.assert_const("static const T& r = *TheX->find(1);", False)

    def test_every_tu_must_qualify(self):
        sym = make_symbol(
            "s",
            ["static const int x = 1;", "static const Foo::Bar* p"],
        )
        self.assertIsNone(m.rule_const_object(sym))


class RuleNamekeyTest(unittest.TestCase):
    """rule:namekey is SAFE_FOR_NEW: the variable itself must be a NameKeyType/StaticNameKey cache, not a
    container or struct that merely mentions one."""

    def assert_namekey(self, decl, expect, msg=None):
        got = m.rule_namekey(make_symbol("s", [decl])) is not None
        self.assertEqual(got, expect, msg or decl)

    def test_namekey_from_macro_is_safe(self):
        self.assert_namekey('static NameKeyType s_key = NAMEKEY("Foo");', True)

    def test_const_namekey_with_no_initialiser_is_safe(self):
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
        args = mock.Mock(root=".", lib="lib.so", vcpkg_lib=None, strict=False)
        with mock.patch.object(m, "load_library", return_value={}):
            with self.assertRaises(SystemExit) as ctx:
                m.cmd_check(args)
        self.assertNotEqual(ctx.exception.code, 0)

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
