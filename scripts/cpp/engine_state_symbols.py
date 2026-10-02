#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 28/09/2026 Classifies the writable state of libgeneralsx.so (PLAN-023 Phase 1)
#
# PLAN-023's Phase 1 deliverable: every writable (.data/.bss/.tbss, function-local statics and their guard
# variables included) and every STB_GNU_UNIQUE symbol of the RTS_ENGINE_CONTEXT=ON Zero Hour library,
# classified as
#
#   per-engine      must move into the engine (PLAN-023 Phase 2, 3 or 4; the note says how)
#   process-global  one per process on purpose (the note says why)
#   constant        a table or value that never changes after static initialisation / first use
#   debug-only      only written by debug, logging or profiling code
#   render-only     only reached with a render device or a user interface (menus, the shell, the W3D
#                   renderer); fine while only the first engine in a process renders
#   unreviewed      not classified yet: nobody has looked, so nothing is claimed
#
# The per-engine entries are the work list for Phases 2-4, and Phase 7 turns the same list into the CI gate.
#
# How a symbol is classified: HAND (below) first, an exact name or a regular expression with a class, a
# phase and a note, written by someone who read the code; then RULES, name/declaration/file patterns that
# are sound for a whole family (NameKeyType caches are process-global by PLAN-023 Decision 2, FieldParse
# tables are constant, third-party library state is process-global, ...); guard variables follow their
# static. Anything neither matches is `unreviewed`. The checked-in list is
# docs/WORKDIR/planning/PLAN-023_STATE_CLASSIFICATION.tsv, which `snapshot` writes.
#
# Usage (from the GeneralsX root, or with --root):
#   engine_state_symbols.py snapshot LIB.so   rewrite the TSV from the library's symbols
#   engine_state_symbols.py check LIB.so      exit 1 if the library has a symbol the TSV lacks, unless a
#                                             rule marked safe for new symbols classifies it (SAFE_FOR_NEW:
#                                             the structural family rules, and the guard variable of a listed
#                                             static); a hand entry, `re:` and `file:` ones included, never
#                                             passes a new symbol, since its pattern was written for the
#                                             symbols someone read, not for whatever a sync adds that
#                                             happens to match. Also exit 1 on a listed symbol whose class or
#                                             phase changed (to `unreviewed` included) or a listed name with
#                                             a new instance (more TUs define it); with --strict also if the
#                                             TSV is stale in any way (a new symbol a safe rule classifies, a
#                                             vanished symbol, a moved definition)
#   engine_state_symbols.py report [LIB.so]   print the per-engine work list grouped by phase, the counts
#                                             per class and the unreviewed symbols (from the TSV, or from
#                                             the library when given)
#
# The library: the classification is of the ON Zero Hour library that rlgenerals builds (its Bazel
# //rlgenerals/generalsx:generalsx_foreign_cc target with --config=engine_context); from an rlgenerals
# checkout,
#
#   bazel build --config=engine_context //rlgenerals/generalsx:generalsx_foreign_cc
#   GeneralsX/scripts/cpp/engine_state_symbols.py --root GeneralsX check --strict \
#       bazel-bin/rlgenerals/generalsx/generalsx_foreign_cc/lib/libgeneralsx.so
#
# A CMake build of this repository with -DRTS_ENGINE_CONTEXT=ON gives the same set up to build flags
# (rlgenerals builds with DEEP_CRC_TO_MEMORY=0, for one); run the check without --strict there. Third-party
# symbols (the vcpkg static libraries linked into the .so: openal-soft, curl, OpenSSL, ...) are recognised
# by the static archives in the build's vcpkg_installed/<triplet>/lib, found next to the library or given
# with --vcpkg-lib. The library needs its symbol table (not stripped); DWARF is not needed: the source file
# is found by searching the sources for the definition.
#
# Needs binutils (nm, c++filt).

import argparse
import collections
import glob
import os
import re
import subprocess
import sys

TSV = "docs/WORKDIR/planning/PLAN-023_STATE_CLASSIFICATION.tsv"

SCAN_ROOTS = ["Core", "GeneralsMD/Code", "Generals/Code/CompatLib", "Dependencies"]
SKIP_DIRS = {"Core/Tools", "GeneralsMD/Code/Tools"}
SOURCE_EXTENSIONS = (".cpp", ".c", ".cc", ".h", ".hpp", ".inl")

CLASSES = ("per-engine", "process-global", "constant", "debug-only", "render-only", "unreviewed")
COLUMNS = ("symbol", "scope", "binding", "section", "count", "bytes", "source", "class", "phase", "note", "by")

WRITABLE_SECTIONS = {".bss", ".data", ".tbss", ".tdata"}


# --------------------------------------------------------------------------------------------------------
# Reading the library


class Symbol:
    """One classification key: every instance of a demangled name (TUs can each have a file static of the
    same name, e.g. the menus' `buttonOkID`)."""

    def __init__(self, key):
        self.key = key
        self.count = 0
        self.bytes = 0
        self.sections = set()
        self.bindings = set()
        self.scope = ""
        self.source = "?"
        self.decl = ""
        self.decls = []
        self.library = ""
        self.cls = ""
        self.phase = ""
        self.note = ""
        self.by = ""

    @property
    def section(self):
        return ",".join(sorted(self.sections))

    @property
    def binding(self):
        return ",".join(sorted(self.bindings))


# `name.0`, `name.lto_priv.0`, `name.constprop.1`: compiler numbering of TU-local objects, not stable.
SUFFIX_RE = re.compile(r"(\.(lto_priv|constprop|isra|part|cold|\d+))+$")


def run(cmd, input_text=None):
    result = subprocess.run(cmd, input=input_text, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        sys.exit(f"{' '.join(cmd[:2])} failed: {result.stderr.strip()}")
    return result.stdout


def demangle(names):
    out = run(["c++filt"], "\n".join(names) + "\n").splitlines()
    if len(out) != len(names):
        sys.exit("c++filt returned a different number of lines")
    return out


def nm_sysv(path):
    """(name, type letter, section, size) of every defined symbol, from `nm -f sysv` (no demangling: a
    demangled name can contain `|`)."""
    rows = []
    for line in run(["nm", "-f", "sysv", path]).splitlines():
        fields = [f.strip() for f in line.split("|")]
        if len(fields) < 7 or not fields[0] or fields[2] in ("U", "w", "v"):
            continue
        size = int(fields[4], 16) if fields[4] else 0
        rows.append((fields[0], fields[2], fields[6], size))
    return rows


def is_state(letter, section):
    return section in WRITABLE_SECTIONS or letter == "u"


def binding_of(letter):
    if letter == "u":
        return "unique"
    return "global" if letter.isupper() else "local"


def read_library(path):
    rows = [r for r in nm_sysv(path) if is_state(r[1], r[2])]
    names = demangle([r[0] for r in rows])
    symbols = {}
    for (_mangled, letter, section, size), name in zip(rows, names):
        key = SUFFIX_RE.sub("", name)
        sym = symbols.get(key)
        if sym is None:
            sym = symbols[key] = Symbol(key)
        sym.count += 1
        sym.bytes += size
        sym.sections.add(section)
        sym.bindings.add(binding_of(letter))
    return symbols


def find_vcpkg_lib(lib_path):
    """The vcpkg_installed/<triplet>/lib of the build that made `lib_path`, searched upwards from it."""
    d = os.path.dirname(os.path.realpath(lib_path))
    for base in (os.path.dirname(os.path.abspath(lib_path)), d):
        cur = base
        for _ in range(6):
            hits = glob.glob(os.path.join(cur, "vcpkg_installed", "*", "lib"))
            hits = [h for h in hits if glob.glob(os.path.join(h, "*.a"))]
            if hits:
                return sorted(hits)[0]
            cur = os.path.dirname(cur)
    return None


def read_third_party(vcpkg_lib):
    """Every data/bss symbol (demangled, suffix-stripped) that a vcpkg static archive defines -> archive."""
    owners = {}
    if not vcpkg_lib:
        return owners
    mangled = []
    libs = []
    for archive in sorted(glob.glob(os.path.join(vcpkg_lib, "*.a"))):
        lib = os.path.basename(archive)[3:-2] if os.path.basename(archive).startswith("lib") else archive
        out = subprocess.run(["nm", "-f", "sysv", archive], capture_output=True, text=True, check=False).stdout
        for line in out.splitlines():
            fields = [f.strip() for f in line.split("|")]
            if len(fields) < 7 or not fields[0] or fields[2] in ("U", "w", "v"):
                continue
            if fields[6] in WRITABLE_SECTIONS or fields[2] == "u" or fields[6].startswith((".bss", ".data", ".tbss")):
                mangled.append(fields[0])
                libs.append(lib)
    for name, lib in zip(demangle(mangled) if mangled else [], libs):
        owners.setdefault(SUFFIX_RE.sub("", name), lib)
    return owners


# --------------------------------------------------------------------------------------------------------
# Finding the definition in the sources


def split_scopes(name):
    """`A<x::y>::f(int, B::C) const::v` -> ['A<x::y>', 'f(int, B::C) const', 'v']: splits on `::` outside
    brackets."""
    parts, depth, cur, i = [], 0, [], 0
    while i < len(name):
        c = name[i]
        if c in "<({[":
            depth += 1
        elif c in ">)}]":
            depth -= 1
        if depth == 0 and name.startswith("::", i):
            parts.append("".join(cur))
            cur = []
            i += 2
            continue
        cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts


# A comment or a string/character literal, whichever starts first: `/*` inside a `//` comment or a string
# opens nothing, and `//` inside a string or a block comment is no comment. An unterminated block comment
# runs to the end of the file. (Raw strings and `'` digit separators are not recognised; a separator can only
# mis-blank the rest of its own line.)
LEXEME_RE = re.compile(r"//[^\n]*|/\*.*?(?:\*/|\Z)|\"(?:\\.|[^\"\\\n])*\"|'(?:\\.|[^'\\\n])*'", re.S)


def _blank(m):
    text = m.group(0)
    if text[0] in "\"'":
        return text[0] + re.sub(r"[^\n]", " ", text[1:-1]) + text[-1] if len(text) > 1 else text
    return re.sub(r"[^\n]", " ", text)


def code_only(text):
    """`text` with every comment and the contents of every string and character literal replaced by spaces
    (newlines kept, so offsets and line numbers are unchanged): what the definition search looks at."""
    return LEXEME_RE.sub(_blank, text)


def bare(part):
    """The identifier of a scope part: `f(int) const` -> `f`, `A<int>` -> `A`, `{lambda()#1}` -> ''."""
    m = re.match(r"\s*(?:\w+\s+)*?(~?[A-Za-z_]\w*|operator\S+)", part)
    return m.group(1) if m else ""


class Parsed:
    def __init__(self, key):
        self.guarded = None
        name = key
        if name.startswith("guard variable for "):
            self.guarded = name[len("guard variable for ") :]
            name = self.guarded
        # An anonymous namespace is no scope for finding the definition.
        parts = [p for p in split_scopes(name) if p != "(anonymous namespace)"]
        self.var = bare(parts[-1]) or parts[-1]
        func_index = max((i for i, p in enumerate(parts[:-1]) if "(" in p), default=None)
        if func_index is not None:
            self.scope = "function"
            self.func = bare(parts[func_index])
            self.cls = bare(parts[func_index - 1]) if func_index > 0 else ""
        elif len(parts) > 1:
            self.scope = "class"
            self.func = ""
            self.cls = bare(parts[-2])
        else:
            self.scope = "namespace"
            self.func = ""
            self.cls = ""
        if self.guarded:
            self.scope = "guard"


class SourceIndex:
    def __init__(self, root, wanted):
        self.root = root
        self.files = {}
        self.where = collections.defaultdict(set)
        for scan_root in SCAN_ROOTS:
            for dirpath, dirnames, filenames in os.walk(os.path.join(root, scan_root)):
                rel_dir = os.path.relpath(dirpath, root)
                dirnames[:] = [d for d in dirnames if os.path.join(rel_dir, d) not in SKIP_DIRS]
                for fn in filenames:
                    if not fn.endswith(SOURCE_EXTENSIONS):
                        continue
                    rel = os.path.join(rel_dir, fn)
                    with open(os.path.join(root, rel), encoding="latin-1") as f:
                        text = f.read()
                    # Searched with comments and literals blanked out (offsets and lines unchanged).
                    self.files[rel] = code_only(text)
                    for word in set(re.findall(r"[A-Za-z_]\w*", text)) & wanted:
                        self.where[word].add(rel)

    def candidates(self, *words):
        sets = [self.where.get(w, set()) for w in words if w]
        if not sets:
            return []
        found = set.intersection(*sets)
        # Definitions live in source files; headers only for inline functions and class-body statics.
        return sorted(found, key=lambda p: (not p.endswith((".cpp", ".c", ".cc")), p))

    def class_bodies(self, rel, cls):
        """(start, end) offsets of the body of every definition of class `cls` in `rel`, braces matched (outside
        comments and literals, which `code_only` has blanked)."""
        text = self.files[rel]
        head = re.compile(rf"\b(?:class|struct)\s+(?:\w+\s+)*?{re.escape(cls)}\b[^;{{}}()]*\{{")
        for m in head.finditer(text):
            depth, i = 1, m.end()
            while depth and i < len(text):
                c = text[i]
                depth += (c == "{") - (c == "}")
                i += 1
            yield m.end(), i

    def line_of(self, rel, pos):
        return self.files[rel].count("\n", 0, pos) + 1

    def lines_with(self, rel, word_re, start=0):
        """(line start offset, line text, match start and end within the line) of every match of `word_re` in
        code: the text is `code_only`, so a match in a comment or a literal is never seen, and the line comes
        back with its comments and literals blanked too."""
        text = self.files[rel]
        for m in word_re.finditer(text, start):
            ls = text.rfind("\n", 0, m.start()) + 1
            le = text.find("\n", m.end())
            line = text[ls : le if le >= 0 else len(text)]
            yield ls, line, m.start() - ls, m.end() - ls

    def where_is(self, rel, offset, line):
        return f"{rel}:{self.line_of(rel, offset)}", re.sub(r"\s+", " ", line.strip()), line[:1] in (" ", "\t")

    def find(self, parsed):
        """(file:line, declaration text, whether the line is indented) of the definition, or ('?', '', False)."""
        for _branch, source, decl, indented in self.definitions(parsed):
            return source, decl, indented
        return "?", "", False

    def find_all(self, parsed):
        """Every definition site (file:line, declaration text), one per file, found the way `find` finds the first: the TUs of
        a name that several translation units each define (a file static, a function-local static of a
        same-named function)."""
        sites, files, first_branch = [], set(), None
        for branch, source, decl, _indented in self.definitions(parsed):
            if first_branch is None:
                first_branch = branch
            if branch != first_branch:
                break
            rel = source.split(":")[0]
            if rel not in files:
                files.add(rel)
                sites.append((source, decl))
        return sites

    def definitions(self, parsed):
        """(branch, file:line, declaration text, indented) of each candidate definition, best first."""
        var_re = re.compile(rf"\b{re.escape(parsed.var)}\b")
        if parsed.scope in ("function", "guard") and parsed.func:
            func = re.escape(parsed.func)
            owner = re.compile(
                rf"\b{re.escape(parsed.cls)}\s*(?:<[^;{{}}]*?>)?\s*::\s*{func}\s*\("
                if parsed.cls
                else rf"\b{func}\s*\("
            )
            # Defined out of line, then (inline in the class body) anywhere after the function's name.
            for branch, owner_re in (("owner", owner), ("inline", re.compile(rf"\b{func}\s*\("))):
                for rel in self.candidates(parsed.var, parsed.func, parsed.cls):
                    fm = owner_re.search(self.files[rel])
                    if not fm:
                        continue
                    for ls, line, a, _b in self.lines_with(rel, var_re, fm.start()):
                        if re.search(r"\bstatic\b", line[:a]):
                            yield (branch, *self.where_is(rel, ls, line))
                            break
            # Made by a macro (MAKE_STANDARD_MODULE_MACRO's getModuleNameKey, ...): the class's header.
            if parsed.cls:
                cls_re = re.compile(rf"^[ \t]*(?:class|struct)\s+(?:\w+\s+)?{re.escape(parsed.cls)}\b[^;]*$", re.M)
                for rel in self.candidates(parsed.cls):
                    m = cls_re.search(self.files[rel])
                    if m:
                        yield "macro", f"{rel}:{self.line_of(rel, m.start())}", "(defined by a macro in the class)", False
                        break
        if parsed.scope in ("class", "guard") and parsed.cls and not parsed.func:
            qual = re.compile(rf"\b{re.escape(parsed.cls)}\s*::\s*{re.escape(parsed.var)}\b")
            for rel in self.candidates(parsed.var, parsed.cls):
                if not rel.endswith((".cpp", ".c", ".cc")):
                    continue
                for ls, line, a, b in self.lines_with(rel, qual):
                    if is_def_prefix(line[:a]) and DEF_SUFFIX_RE.match(line[b:]):
                        yield ("qualified", *self.where_is(rel, ls, line))
                        break
            # A static data member defined in the class body (inline or constexpr): only inside that class's
            # own body, so that another class's member of the same name is never taken for it.
            for rel in self.candidates(parsed.var, parsed.cls):
                for body_start, body_end in self.class_bodies(rel, parsed.cls):
                    site = None
                    for ls, line, a, _b in self.lines_with(rel, var_re, body_start):
                        if ls + a >= body_end:
                            break
                        prefix = line[:a]
                        if re.search(r"\bstatic\b", prefix) and "(" not in prefix:
                            site = self.where_is(rel, ls, line)
                            break
                    if site:
                        yield ("in-class", *site)
                        break
        if parsed.scope in ("namespace", "class", "guard") and not parsed.func:
            # Every file's first definition, ranked: a file-scope definition in a source file first.
            # A qualified name found by neither branch above is a namespace member: only a definition after
            # that namespace opens counts (not another scope's variable of the same name).
            ns_re = re.compile(rf"^\s*namespace\s+(?:\w+\s*::\s*)*{re.escape(parsed.cls)}\b", re.M) if parsed.cls else None
            found = []
            for rel in self.candidates(parsed.var):
                is_source = rel.endswith((".cpp", ".c", ".cc"))
                for ls, line, a, b in self.lines_with(rel, var_re):
                    if not (is_def_prefix(line[:a]) and DEF_SUFFIX_RE.match(line[b:])):
                        continue
                    if ns_re and not ns_re.search(self.files[rel], 0, ls):
                        continue
                    indented = line[:1] in (" ", "\t")
                    # Indented: a local variable unless it is a static or inside a namespace block.
                    if indented and not re.search(r"\bstatic\b", line[:a]):
                        text = self.files[rel]
                        if not re.search(r"^\s*namespace\b", text[:ls], re.M):
                            continue
                    found.append(((indented, not is_source), ls, line, rel))
                    break
            found.sort(key=lambda f: f[0])  # stable: candidates order within a rank
            for rank, ls, line, rel in found:
                yield (f"plain{rank}", *self.where_is(rel, ls, line))


# What comes before the name in a definition: a type (and qualifiers), no keyword that makes it a
# statement; and after it: array bounds, then an initialiser or the end of the declarator.
DEF_PREFIX_RE = re.compile(
    r"(?!\s*(?:return|extern|delete|if|else|case|goto|throw|using|typedef|friend|#|//|/\*|\*)\b)"
    r"(?:\s*(?:[A-Za-z_][\w:]*(?:\s*<[^;()]*>)?[\s*&]+)+"  # a type
    r"(?:[*&]*\s*\w+\s*(?:\[[^\]]*\]\s*)*(?:=[^,;]*)?,\s*)*"  # earlier declarators on the same line
    r"|\s*\}\s*"  # the instance after a struct body
    r"|[A-Z_][A-Z_0-9]*\(.*\)\s*)"  # a declaring macro (DECLARE_DEFINITION_FACTORY(...) name;)
)
DEF_SUFFIX_RE = re.compile(r"\s*(?:\[[^\]]*\]\s*)*(?:=|;|\{|\(|,|$)")


def is_def_prefix(prefix):
    return DEF_PREFIX_RE.fullmatch(prefix) is not None


SITE_SEP = ";"


def source_files(source):
    """The files of a source column (`a.cpp:1;b.cpp:2` -> {'a.cpp', 'b.cpp'}), without line numbers."""
    return {site.split(":")[0] if not site.startswith("third-party:") else site for site in source.split(SITE_SEP)}


def resolve_sources(root, symbols, third_party):
    parsed = {k: Parsed(k) for k in symbols}
    wanted = set()
    for p in parsed.values():
        wanted.update(w for w in (p.var, p.func, p.cls) if w)
    index = SourceIndex(root, wanted)
    for key, sym in symbols.items():
        p = parsed[key]
        sym.scope = p.scope
        target = p.guarded or key
        source, decl, indented = index.find(p)
        library = third_party.get(target) or third_party.get(key)
        # A vcpkg archive defines the name too: it is the library's unless the sources define it at file
        # scope in a source file (a short C name such as `types` can be both, in different objects).
        at_file_scope = source.split(":")[0].endswith((".cpp", ".c", ".cc")) and not indented
        if library and (source == "?" or not at_file_scope or p.scope == "class" or sym.count == 1 and p.scope != "namespace"):
            sym.library = library
            source = f"third-party:{library}"
        elif sym.count > 1:
            # Several instances (TU-local statics of one name): record every definition site, so that the
            # list shows each TU the one classification covers and a new TU's static is a visible change.
            sites = index.find_all(p)
            if len(sites) > 1:
                source = SITE_SEP.join(site for site, _decl in sites)
                sym.decls = [re.sub(r"\s+", " ", d)[:160] for _site, d in sites]
        sym.source = source
        sym.decl = re.sub(r"\s+", " ", decl)[:160]
        sym.parsed = p
    return index


# --------------------------------------------------------------------------------------------------------
# Classification

PER, GLOBAL, CONST, DEBUG, RENDER, UNREVIEWED = CLASSES


class Hand:
    """A hand classification from engine_state_hand.py: (class, phase, matcher, note). The matcher is the
    exact key, `re:` + a regular expression (fullmatch on the key) or `file:` + a regular expression
    (searched for in every definition site of the source column; all of them must match). A `file:` match
    is a blanket one, recorded as `by` = `file`. `check` fails on a new symbol any hand entry classifies
    (see SAFE_FOR_NEW), so every static a sync adds is looked at once (and recorded by `snapshot`)."""

    def __init__(self, entry):
        self.cls, phase, matcher, self.note = entry
        self.phase = str(phase) if phase else ""
        if self.cls not in CLASSES or (self.cls == PER) != bool(self.phase) or not self.note:
            sys.exit(f"engine_state_hand.py: bad entry {entry!r}")
        self.name = self.regex = self.file = None
        if matcher.startswith("re:"):
            self.regex = re.compile(matcher[3:])
        elif matcher.startswith("file:"):
            self.file = re.compile(matcher[5:])
        else:
            self.name = matcher
        self.used = 0

    def matches(self, sym):
        if self.name is not None:
            return sym.key == self.name
        if self.regex is not None:
            return bool(self.regex.fullmatch(sym.key))
        return all(self.file.search(site) for site in sym.source.split(SITE_SEP))


sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from engine_state_hand import HAND as HAND_ENTRIES  # noqa: E402

HAND = [Hand(e) for e in HAND_ENTRIES]


# The rules: (name, function(sym) -> (class, phase, note) or None). Sound for the whole family they match.

TOOLCHAIN = {"__dso_handle", "completed", "__TMC_END__", "_GLOBAL_OFFSET_TABLE_", "_DYNAMIC", "__bss_start", "_edata", "_end"}


def rule_toolchain(sym):
    if sym.key in TOOLCHAIN or sym.key.startswith("DW.ref."):
        return GLOBAL, "", "compiler/linker runtime object (crtstuff, DWARF personality references)"
    return None


def rule_libstdcxx(sym):
    if sym.key.startswith(("std::", "__gnu_cxx::", "__cxxabiv1::", "guard variable for std::")):
        return GLOBAL, "", "libstdc++ template static instantiated into the library (library state, not engine state)"
    return None


def rule_third_party(sym):
    if sym.library:
        return GLOBAL, "", f"state of the statically linked third-party library `{sym.library}` (vcpkg); not engine state"
    return None


def rule_read_only(sym):
    if sym.sections <= {".rodata", ".data.rel.ro"}:
        return CONST, "", "in a read-only (or RELRO) section: nothing can write it after relocation"
    return None


def declarations(sym):
    """The declaration of every definition site: a rule that reads the declaration must hold for all of
    the TUs one key covers, not only the first."""
    return sym.decls or [sym.decl]


# The variable itself must be a NameKeyType/StaticNameKey (optionally const), not merely mention one
# somewhere in its type: a container or struct keyed by NameKeyType is not a cache of one. Unless const, it
# must also be initialised from the shared generator, so it is never reassigned to a different key later.
NAMEKEY_DECL_RE = re.compile(r"^(?:static\s+)?(const\s+)?(?:NameKeyType|StaticNameKey)\s+\w+\s*(?:\[[^\]]*\])?\s*$")
NAMEKEY_INIT_RE = re.compile(r"^\s*(?:NAMEKEY\s*\(|TheNameKeyGenerator\s*->\s*nameToKey\b)")


def rule_namekey(sym):
    p = sym.parsed
    if sym.key.startswith("TheKey_") or (p.var == "nk" and p.func == "getModuleNameKey"):
        return GLOBAL, "", "cached NameKeyType: process-wide by PLAN-023 Decision 2 (shared immortal generator)"
    for d in declarations(sym):
        decl, sep, init = d.partition("=")
        m = NAMEKEY_DECL_RE.match(decl.rstrip().rstrip(";").rstrip())
        if not m:
            return None
        if not m.group(1) and not (sep and NAMEKEY_INIT_RE.search(init)):
            return None
    return GLOBAL, "", "cached NameKeyType (a name or window ID key): process-wide by PLAN-023 Decision 2"


def rule_field_parse(sym):
    p = sym.parsed
    if p.var in ("dataFieldParse", "myFieldParse", "commonFieldParse") or all(
        re.search(r"\bFieldParse\b", d.split("=")[0]) for d in declarations(sym)
    ):
        return CONST, "", "INI FieldParse table: built once, never written"
    return None


CONST_DECL_RE = re.compile(r"^(?:static\s+)?(?:inline\s+)?(?:const\s+static|const)\b")
# `T* const name`, the only pointer declarator that cannot be reseated; a trailing array bound is allowed
# (`T* const table[]`), but anything after the name (another declarator, a function parameter list) is not.
CONST_PTR_DECL_RE = re.compile(r"\*\s*const\s+\w+\s*(?:\[[^\]]*\]\s*)*$")
# A one-time lookup (`TheX->find(...)`, `someFunc(...)`) that a per-engine cache (ActiveBody's templates,
# WaveGuideUpdate's particles, ...) would also use to fill its pointer on first call: not a constant
# expression, even though the pointer slot itself is never written again.
LOOKUP_INIT_RE = re.compile(r"->|\w+\s*\(")


def rule_const_object(sym):
    """A const object (dynamically initialised, so it lands in .data/.bss): never written after its
    initialisation. A reference is never safe here (it can alias per-engine state at any type). A pointer is
    safe only when the pointer itself cannot be reseated (`T* const name`, not `const T*`, whatever the
    pointee type) and its initialiser is not a lookup: a per-engine pointer cache has exactly the `T* const
    name = TheX->find(...)` shape."""
    for d in declarations(sym):
        decl, _, init = d.partition("=")
        decl = decl.rstrip().rstrip(";").rstrip()
        if "&" in decl:
            return None
        if "*" in decl:
            if not CONST_PTR_DECL_RE.search(decl) or LOOKUP_INIT_RE.search(init):
                return None
        elif not CONST_DECL_RE.match(decl):
            return None
    return CONST, "", "const object: initialised once, never written"


RULES = [
    ("toolchain", rule_toolchain),
    ("libstdc++", rule_libstdcxx),
    ("third-party", rule_third_party),
    ("read-only", rule_read_only),
    ("namekey", rule_namekey),
    ("fieldparse", rule_field_parse),
    ("const", rule_const_object),
]

# The `by` values that may classify a symbol the TSV lacks without failing a non-strict `check`: rules that
# decide from what the symbol structurally is (its section, its declared type, its defining library, a
# compiler or macro naming scheme), so they hold for any new member of the family. Hand entries are never
# here, however they match: a `re:` or `file:` pattern names the symbols its author read, and a new symbol
# that happens to match it (a new `static Int key_count` under `re:.*::key_\w+`) would otherwise inherit a
# classification nobody gave it. A guard variable is safe when its static is listed (see `new_symbol_error`).
# A rule added here must be as sound for an unseen symbol as these.
SAFE_FOR_NEW = {f"rule:{name}" for name, _rule in RULES}


def classify(sym, symbols):
    p = sym.parsed
    if p.guarded:
        target = symbols.get(p.guarded)
        if target is not None:
            if not target.cls:
                classify(target, symbols)
            sym.cls, sym.phase = target.cls, target.phase
            sym.note = f"guard variable of that static: {target.note}" if target.note else "guard variable of that static"
            sym.by = "guard"
            return
    for hand in HAND:
        if hand.matches(sym):
            sym.cls, sym.phase, sym.note = hand.cls, hand.phase, hand.note
            sym.by = "file" if hand.file is not None else "hand"
            hand.used += 1
            return
    for name, rule in RULES:
        result = rule(sym)
        if result:
            sym.cls, sym.phase, sym.note = result
            sym.by = f"rule:{name}"
            return
    sym.cls, sym.phase, sym.note, sym.by = UNREVIEWED, "", "", "none"


# --------------------------------------------------------------------------------------------------------
# The checked-in list

TSV_HEADER = (
    "# PLAN-023 Phase 1: the writable and STB_GNU_UNIQUE symbols of the RTS_ENGINE_CONTEXT=ON Zero Hour\n"
    "# libgeneralsx.so, classified. GENERATED by scripts/cpp/engine_state_symbols.py snapshot: edit the hand\n"
    "# list (scripts/cpp/engine_state_hand.py) or the rules, not this file. Columns: symbol (demangled, compiler\n"
    "# `.N` suffixes stripped); scope (namespace, class, function = function-local static, guard = its guard\n"
    "# variable); binding (local, global, unique = STB_GNU_UNIQUE); section; count (instances: TUs can each\n"
    "# have one); bytes; source (path:line of the definition, found by searching the sources, every TU's site\n"
    "# joined by `;` when several TUs define the name, `third-party:<lib>` for a vcpkg library, `?` if not\n"
    "# found); class; phase (per-engine only); note; by (hand, file = a hand `file:` pattern, rule:<name>,\n"
    "# guard, none).\n"
)


def load_library(root, lib, vcpkg_lib):
    if not os.path.isfile(lib):
        sys.exit(f"{lib}: no such file")
    symbols = read_library(lib)
    vcpkg_lib = vcpkg_lib or find_vcpkg_lib(lib)
    if not vcpkg_lib:
        print(
            "warning: no vcpkg_installed/<triplet>/lib found next to the library (give --vcpkg-lib): "
            "third-party symbols will not be recognised",
            file=sys.stderr,
        )
    resolve_sources(root, symbols, read_third_party(vcpkg_lib))
    for sym in symbols.values():
        classify(sym, symbols)
    return symbols


def row_of(sym):
    return {
        "symbol": sym.key,
        "scope": sym.scope,
        "binding": sym.binding,
        "section": sym.section,
        "count": str(sym.count),
        "bytes": str(sym.bytes),
        "source": sym.source,
        "class": sym.cls,
        "phase": sym.phase,
        "note": sym.note,
        "by": sym.by,
    }


def clean(value):
    return value.replace("\t", " ").replace("\n", " ")


def write_tsv(path, symbols):
    rows = sorted((row_of(s) for s in symbols.values()), key=lambda r: (r["source"], r["symbol"]))
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(TSV_HEADER)
        f.write("\t".join(COLUMNS) + "\n")
        for row in rows:
            f.write("\t".join(clean(row[c]) for c in COLUMNS) + "\n")
    return rows


def read_tsv(path):
    if not os.path.isfile(path):
        sys.exit(f"{path}: no such file (run `snapshot` first)")
    rows = {}
    with open(path, encoding="utf-8") as f:
        header = None
        for line in f:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if header is None:
                header = fields
                continue
            row = dict(zip(header, fields))
            rows[row["symbol"]] = row
    return rows


# --------------------------------------------------------------------------------------------------------
# Modes


def summary(rows):
    counts = collections.Counter(r["class"] for r in rows)
    return ", ".join(f"{c} {counts.get(c, 0)}" for c in CLASSES)


def cmd_snapshot(args):
    symbols = load_library(args.root, args.lib, args.vcpkg_lib)
    rows = write_tsv(os.path.join(args.root, TSV), symbols)
    print(f"{TSV}: {len(rows)} symbols ({summary(rows)})")
    unused = [h for h in HAND if not h.used]
    for h in unused:
        print(f"warning: hand entry matches nothing: {h.name or (h.regex or h.file).pattern}", file=sys.stderr)
    return 0


def compare(key, sym, row):
    """(errors, stale) for a symbol the list records. Errors in every mode: a class or phase that changed
    (including to `unreviewed`: a hand entry deleted, or a hand regex or `file:` pattern that stopped
    matching after an upstream rename or move), a blanket `file:` match replacing a reviewed one, and a new
    instance of an already listed name (a count or a set of defining files that grew: a new TU's static of
    the same name would otherwise inherit the existing classification unseen). Stale only: fewer instances,
    and definitions that moved."""
    errors, stale = [], []
    if row["class"] != sym.cls or row["phase"] != sym.phase:
        errors.append(f"class changed: {key}: {row['class']} {row['phase']} -> {sym.cls} {sym.phase} (by {sym.by})")
    elif sym.by == "file" and row["by"] != "file":
        errors.append(f"now classified only by a file pattern (was {row['by']}): {key} ({sym.source})")
    try:
        recorded_count = int(row["count"])
    except (KeyError, ValueError):
        recorded_count = 0
    new_files = source_files(sym.source) - source_files(row.get("source", ""))
    if sym.count > recorded_count or (recorded_count > 1 or sym.count > 1) and new_files and row.get("source") != "?":
        errors.append(
            f"new instance of a listed name: {key}: {recorded_count} -> {sym.count} instances"
            + (f", new definition in {', '.join(sorted(new_files))}" if new_files else "")
            + " (the listed classification would cover it unseen)"
        )
    elif sym.count < recorded_count:
        stale.append(f"fewer instances: {key}: {recorded_count} -> {sym.count}")
    elif row.get("source") != sym.source:
        stale.append(f"definition moved: {key}: {row.get('source')} -> {sym.source}")
    return errors, stale


def new_symbol_error(sym, recorded):
    """Why `check` fails on `sym`, a symbol the TSV lacks, or None when a safe rule classifies it."""
    if sym.cls == UNREVIEWED:
        return f"new symbol, not classified: {sym.key} ({sym.source})"
    if sym.by in SAFE_FOR_NEW:
        return None
    if sym.by == "guard":
        if sym.parsed.guarded in recorded:
            return None
        return f"new symbol, the guard variable of a new static (review the static): {sym.key} ({sym.source})"
    how = "a blanket file pattern" if sym.by == "file" else "a hand entry"
    return f"new symbol, classified {sym.cls} only by {how} written before it existed (review it): {sym.key} ({sym.source})"


def cmd_check(args):
    symbols = load_library(args.root, args.lib, args.vcpkg_lib)
    recorded = read_tsv(os.path.join(args.root, TSV))
    errors, stale = [], []
    for key, sym in sorted(symbols.items()):
        row = recorded.get(key)
        if row is None:
            error = new_symbol_error(sym, recorded)
            if error:
                errors.append(error)
            else:
                stale.append(f"new symbol, classified {sym.cls} by {sym.by}: {key} ({sym.source})")
        else:
            e, st = compare(key, sym, row)
            errors += e
            stale += st
    for key in sorted(set(recorded) - set(symbols)):
        stale.append(f"gone from the library: {key}")
    for line in errors:
        print(f"error: {line}")
    for line in stale:
        print(f"{'error' if args.strict else 'warning'}: {line}")
    if errors or (stale and args.strict):
        print(
            f"\n{TSV} is out of date: classify the new symbols in scripts/cpp/engine_state_hand.py (or a rule), "
            "then run `engine_state_symbols.py snapshot <lib>` and review the diff"
        )
        return 1
    unreviewed = sum(1 for s in symbols.values() if s.cls == UNREVIEWED)
    print(f"ok: {len(symbols)} symbols, none new but what a safe rule classifies ({unreviewed} still unreviewed in the list)")
    return 0


def cmd_report(args):
    if args.lib:
        rows = [row_of(s) for s in load_library(args.root, args.lib, args.vcpkg_lib).values()]
    else:
        rows = list(read_tsv(os.path.join(args.root, TSV)).values())
    rows = [r for r in rows if r["by"] != "guard"] if not args.guards else rows
    print(f"{len(rows)} symbols{'' if args.guards else ' (guard variables left out)'}: {summary(rows)}\n")
    per = [r for r in rows if r["class"] == PER]
    for phase in sorted({r["phase"] for r in per}):
        group = sorted((r for r in per if r["phase"] == phase), key=lambda r: (r["source"], r["symbol"]))
        print(f"== Phase {phase}: {len(group)} per-engine")
        for r in group:
            print(f"  {r['symbol']}  [{r['source']}]\n      {r['note']}")
        print()
    rest = sorted((r for r in rows if r["class"] == UNREVIEWED), key=lambda r: (r["source"], r["symbol"]))
    print(f"== Unreviewed: {len(rest)}")
    for r in rest:
        print(f"  {r['symbol']}  [{r['source']}]")
    if args.by_class:
        for cls in (GLOBAL, CONST, DEBUG, RENDER):
            group = sorted((r for r in rows if r["class"] == cls), key=lambda r: (r["source"], r["symbol"]))
            print(f"\n== {cls}: {len(group)}")
            for r in group:
                print(f"  {r['symbol']}  [{r['source']}]  {r['note']}")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__ or "PLAN-023 state classification")
    parser.add_argument("--root", default=".", help="the GeneralsX checkout (default: the current directory)")
    parser.add_argument("--vcpkg-lib", help="the build's vcpkg_installed/<triplet>/lib (default: searched for)")
    sub = parser.add_subparsers(dest="mode", required=True)
    p = sub.add_parser("snapshot", help="rewrite the TSV from the library")
    p.add_argument("lib")
    p = sub.add_parser("check", help="fail on a new symbol no safe rule classifies, a changed class or a new instance")
    p.add_argument("lib")
    p.add_argument("--strict", action="store_true", help="also fail if the TSV is stale in any way")
    p = sub.add_parser("report", help="print the per-engine work list by phase and the unreviewed symbols")
    p.add_argument("lib", nargs="?")
    p.add_argument("--guards", action="store_true", help="include guard variables")
    p.add_argument("--by-class", action="store_true", help="also list every other class")
    args = parser.parse_args()
    if not os.path.isfile(os.path.join(args.root, "scripts/cpp/engine_state_symbols.py")):
        sys.exit(f"{args.root}: not a GeneralsX checkout (give --root)")
    return {"snapshot": cmd_snapshot, "check": cmd_check, "report": cmd_report}[args.mode](args)


if __name__ == "__main__":
    sys.exit(main())
