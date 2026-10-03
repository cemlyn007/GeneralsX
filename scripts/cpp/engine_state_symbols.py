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
import bisect
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
    # An empty `names` would still send c++filt a lone newline and get one empty line back (a stripped
    # library's `nm` yields no rows at all): short-circuit so the mismatched-line-count guard below is never
    # tripped by this, and `read_library` genuinely returns no symbols for cmd_check's empty-library guard to
    # catch.
    if not names:
        return []
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
    (newlines kept, so offsets and line numbers are unchanged), and every line-continuation backslash
    (a `\\` immediately before a newline, as a multi-line `#define` body uses) blanked to a space too, so a
    function signature or body split across a macro's continuation lines reads the same as one that is not:
    what the definition search looks at."""
    return re.sub(r"\\(\r?\n)", r" \1", LEXEME_RE.sub(_blank, text))


def bare(part):
    """The identifier of a scope part: `f(int) const` -> `f`, `A<int>` -> `A`, `{lambda()#1}` -> ''."""
    m = re.match(r"\s*(?:\w+\s+)*?(~?[A-Za-z_]\w*|operator\S+)", part)
    return m.group(1) if m else ""


def skip_balanced(text, open_idx, open_char, close_char):
    """The index just past the bracket matching `text[open_idx]` (which must be `open_char`), depth-counted
    over `text` as-is (comments and literals already blanked by `code_only`, so a stray bracket in either
    never miscounts)."""
    depth, i = 1, open_idx + 1
    n = len(text)
    while depth and i < n:
        c = text[i]
        depth += (c == open_char) - (c == close_char)
        i += 1
    return i


# What can follow a function's closing `)` in its own definition (never in a call or a declaration): an
# optional `const`/`override`/`final`/`noexcept(...)`, then, for a constructor, a `: base(), member(x)`
# initialiser list (no brace or semicolon inside it, so it cannot itself hide a body), then the body's `{`.
# A plain declaration or a call both end in `;` instead and never match.
DEF_BODY_RE = re.compile(
    r"\s*(?:(?:const|override|final)\b\s*|noexcept\s*(?:\([^(){};]*\))?\s*)*(?::[^{};]*)?\{"
)

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


#  What kind of block a `{` opens, read from the (`code_only`) text immediately before it: a function body
# (its own definition, never a call: the same `)` [const/override/final/noexcept] [: initialiser list] `{`
# shape as DEF_BODY_RE, but anchored at the end of the lookbehind instead of after a known `)` offset), a
# namespace block (named or anonymous), a class/struct/union body, or anything else (if/for/while/switch, a
# lambda, an initialiser list, ...).
BRACE_FUNC_RE = re.compile(r"\)\s*(?:(?:const|override|final)\b\s*|noexcept\s*(?:\([^(){}]*\))?\s*)*(?::[^{};]*)?$")
BRACE_NAMESPACE_RE = re.compile(r"\bnamespace\s+[\w:]*\s*$")
BRACE_CLASS_RE = re.compile(r"\b(?:class|struct|union)\b[^;{}]*$")
# How far back to look for the shape before a `{`: a type/initialiser list can run long, but this is only
# ever used on code already known to be short (a function signature, `namespace X`, `class Foo : public Bar`).
BRACE_LOOKBACK = 400

# A preprocessor conditional: `#if`/`#ifdef`/`#ifndef` opens one, `#elif`/`#else` starts a sibling branch of
# the SAME one, and `#endif` closes it. Braces are counted without evaluating which branch the build takes
# (PartitionManager.cpp's `#ifndef ... { ... #else ... { ... #endif`, each branch opening its own `{` closed
# by one shared `}` after `#endif`), so every branch after the first must restart from the brace stack seen
# at the conditional's own `#if`, or two never-taken branches that each open a `{` leave the rest of the
# file one scope too deep.
PP_IF_RE = re.compile(r"^[ \t]*#\s*(?:if|ifdef|ifndef)\b", re.M)
PP_ELSE_RE = re.compile(r"^[ \t]*#\s*(?:elif|else)\b", re.M)
PP_ENDIF_RE = re.compile(r"^[ \t]*#\s*endif\b", re.M)


def _scope_events(text):
    """Every `{`/`}` in `text`, as two parallel lists: positions (strictly increasing, each one past the
    brace) and the full scope-kind stack (innermost last) in force from that position until the next event.
    Built once per file, bisected on the positions to find the stack enclosing any candidate site's offset."""
    positions, stacks = [0], [()]
    stack = []
    pp_events = sorted(
        [(m.start(), "if") for m in PP_IF_RE.finditer(text)]
        + [(m.start(), "else") for m in PP_ELSE_RE.finditer(text)]
        + [(m.start(), "endif") for m in PP_ENDIF_RE.finditer(text)]
    )
    pp_idx = 0
    pp_stack = []
    for i, c in enumerate(text):
        while pp_idx < len(pp_events) and pp_events[pp_idx][0] == i:
            kind = pp_events[pp_idx][1]
            if kind == "if":
                pp_stack.append(list(stack))
            elif kind == "else" and pp_stack:
                stack = list(pp_stack[-1])
                # `stack` just changed with no `{`/`}` of its own: record it as a scope event too, or
                # `scope_at` would bisect to the stack the LAST brace left (inside the branch this #else/
                # #elif just abandoned) for everything up to the next actual brace.
                positions.append(i)
                stacks.append(tuple(stack))
            elif kind == "endif" and pp_stack:
                pp_stack.pop()
            pp_idx += 1
        if c == "{":
            tail = text[max(0, i - BRACE_LOOKBACK) : i]
            if BRACE_NAMESPACE_RE.search(tail):
                kind = "namespace"
            elif BRACE_FUNC_RE.search(tail):
                kind = "function"
            elif BRACE_CLASS_RE.search(tail):
                kind = "class"
            else:
                kind = "other"
            stack.append(kind)
            positions.append(i + 1)
            stacks.append(tuple(stack))
        elif c == "}":
            if stack:
                stack.pop()
            positions.append(i + 1)
            stacks.append(tuple(stack))
    return positions, stacks


class SourceIndex:
    def __init__(self, root, wanted):
        self.root = root
        self.files = {}
        self._scope_cache = {}
        self._is_class_cache = {}
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

    def is_class_name(self, cls):
        """Whether `cls` is declared as a class/struct ANYWHERE in the scanned tree (as opposed to a
        namespace): every out-of-line `Cls::method(...)` implies a `class`/`struct Cls { ... };` somewhere
        to compile against, even when that declaration lives in a different file (a header) from the
        in-line method the inline branch is restricted to Cls's own body against."""
        cached = self._is_class_cache.get(cls)
        if cached is None:
            cached = self._is_class_cache[cls] = any(
                True for rel in self.candidates(cls) for _ in self.class_bodies(rel, cls)
            )
        return cached

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
        full = self._statement_text(self.files[rel], offset, line)
        return f"{rel}:{self.line_of(rel, offset)}", re.sub(r"\s+", " ", full.strip()), line[:1] in (" ", "\t")

    @staticmethod
    def _statement_text(text, offset, line):
        """`line` (the declaration's own source line, as `lines_with` sliced it) extended to cover a
        multi-line initialiser: a direct- or brace-initialiser whose opening `(`/`{` does not close on this
        line (an array of struct literals, `static const T arr[] =\\n{ ... };`), or a bare `name =` whose
        value starts on the next one. `line` alone already settles the common case (ends in `;` with its
        own brackets balanced) without scanning the file at all. Otherwise scanned forward over `text`
        (already `code_only`, so a `;` inside a blanked comment or string literal is never read as the
        statement's own) for the top-level `;`, bounded so a malformed file can never hang this."""
        depth = 0
        for c in line:
            depth += (c in "({[") - (c in ")}]")
        if depth <= 0 and line.rstrip().endswith(";"):
            return line
        limit = min(len(text), offset + 4000)
        depth, i = 0, offset
        while i < limit:
            c = text[i]
            if c in "({[":
                depth += 1
            elif c in ")}]":
                depth -= 1
            elif c == ";" and depth <= 0:
                return text[offset : i + 1]
            i += 1
        return text[offset:limit]

    def scope_at(self, rel, pos):
        """The stack of enclosing brace kinds (innermost last) at `pos` in `rel`: `"function"` in it means
        `pos` is inside some function body (a local, whether or not it names the function we are looking
        for: that function is matched by name separately), `"namespace"`/`"class"` a namespace or class
        body with no enclosing function."""
        cached = self._scope_cache.get(rel)
        if cached is None:
            cached = self._scope_cache[rel] = _scope_events(self.files[rel])
        positions, stacks = cached
        return stacks[bisect.bisect_right(positions, pos) - 1]

    def find(self, parsed):
        """(file:line, declaration text, whether the line is indented) of the definition, or ('?', '', False)."""
        for _branch, source, decl, indented in self.definitions(parsed):
            return source, decl, indented
        return "?", "", False

    def find_all(self, parsed):
        """Every definition site (file:line, declaration text), one per file, the best-ranked one `find` would
        take for that file: the TUs of a name that several translation units each define (a file static, a
        function-local static of a same-named function). `definitions` already yields best first within and
        across its branches (owner before inline before macro; qualified before in-class; `plain` sites
        sorted by rank), so the first site seen for a file is its best one; stopping at the first branch
        change would instead drop every other branch's sites outright (a namespace member defined at file
        scope in one TU and indented inside a `namespace { ... }` block in another have different `plain`
        ranks, so only one of the two TUs would ever be recorded)."""
        sites, files = [], set()
        for _branch, source, decl, _indented in self.definitions(parsed):
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
            # Defined out of line, then (inline in the class body) anywhere after the function's name. Each
            # match of the name followed by `(` can be a call rather than this function's own definition (a
            # qualified call to it, or, with no class, any same-named inline function); only a match whose
            # parameter list is followed by a function body (optionally `const`/`override`/`noexcept` or a
            # constructor's initialiser list, then `{`, not `;`) is a definition, and only a static inside
            # that body belongs to this function.
            found_body = False
            for branch, owner_re in (("owner", owner), ("inline", re.compile(rf"\b{func}\s*\("))):
                for rel in self.candidates(parsed.var, parsed.func, parsed.cls):
                    text = self.files[rel]
                    # The inline branch's bare `func(` regex carries no class qualifier of its own (it
                    # exists for a method defined inline, unqualified, inside its own class body), so, when
                    # `parsed.cls` genuinely names a class (declared with `class`/`struct` somewhere, even in
                    # a different file than this one: every out-of-line `Cls::func(...)` implies one exists
                    # to compile against), it is only ever this class's own definition when the match sits
                    # inside THIS class's own body here. Unrestricted, an out-of-line definition (qualified,
                    # `Other::func(...)`) or any other class's own inline definition of a same-named method,
                    # sharing this file, would be wrongly attributed to this class (W3DAssetManager::
                    # Create_Render_Obj's own `static int warning_count` taken for WW3DAssetManager::
                    # Create_Render_Obj's; or Derived's own inline method stealing Base::func()'s static
                    # purely because Derived mentions `Base::func(` inside its own body). When `parsed.cls`
                    # is not a class at all (a namespace nesting the function instead), there is no body to
                    # restrict to, and the bare regex is simply this namespace-scoped function's own name.
                    class_spans = (
                        list(self.class_bodies(rel, parsed.cls))
                        if branch == "inline" and parsed.cls and self.is_class_name(parsed.cls)
                        else None
                    )
                    if class_spans is not None and not class_spans:
                        continue
                    site = None
                    for fm in owner_re.finditer(text):
                        if class_spans is not None and not any(s <= fm.start() < e for s, e in class_spans):
                            continue
                        close = skip_balanced(text, fm.end() - 1, "(", ")")
                        head = DEF_BODY_RE.match(text, close)
                        if not head:
                            continue
                        body_end = skip_balanced(text, head.end() - 1, "{", "}")
                        for ls, line, a, _b in self.lines_with(rel, var_re, head.end()):
                            if ls >= body_end:
                                break
                            if re.search(r"\bstatic\b", line[:a]):
                                site = self.where_is(rel, ls, line)
                                break
                        if site:
                            break
                    if site:
                        yield (branch, *site)
                        found_body = True
            # Made by a macro (MAKE_STANDARD_MODULE_MACRO's getModuleNameKey, ...): the class's header. Only
            # a fallback: a static found inside a real function body above (owner or inline) is this
            # function's actual definition, and the macro-expanded class line is not a second one.
            if parsed.cls and not found_body:
                cls_re = re.compile(rf"^[ \t]*(?:class|struct)\s+(?:\w+\s+)?{re.escape(parsed.cls)}\b[^;]*$", re.M)
                for rel in self.candidates(parsed.cls):
                    m = cls_re.search(self.files[rel])
                    if m:
                        yield "macro", f"{rel}:{self.line_of(rel, m.start())}", "(defined by a macro in the class)", False
                        break
        if parsed.scope in ("class", "guard") and parsed.cls and not parsed.func:
            qual = re.compile(rf"\b{re.escape(parsed.cls)}\s*::\s*{re.escape(parsed.var)}\b")
            found_qualified = False
            for rel in self.candidates(parsed.var, parsed.cls):
                if not rel.endswith((".cpp", ".c", ".cc")):
                    continue
                for ls, line, a, b in self.lines_with(rel, qual):
                    if is_def_prefix(line[:a]) and DEF_SUFFIX_RE.match(line[b:]):
                        yield ("qualified", *self.where_is(rel, ls, line))
                        found_qualified = True
                        break
            # A static data member defined in the class body (inline or constexpr): only inside that class's
            # own body, so that another class's member of the same name is never taken for it. Only a
            # fallback: when some TU has the qualified out-of-line definition, the in-class line is its
            # declaration, not a second definition site.
            if not found_qualified:
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
                    scope = self.scope_at(rel, ls + a)
                    # A class body's own member (static or not, however it is written) is the qualified/
                    # in-class branches' job, not this one: never take it for a namespace- or file-scope
                    # name just because an unrelated class happens to declare a same-named member.
                    if "class" in scope:
                        continue
                    # A match enclosed by a function body is a local of some unrelated function (ours is
                    # matched by the function branch above, never reaching here), never this namespace- or
                    # class-scope name, column 0 included: a plain text search for the bare identifier
                    # cannot otherwise tell a local `parent` in nlohmann::json's code from this symbol's own
                    # `parent`, or an unindented local (StackDump.cpp's `HANDLE thread = ...;`) from a file
                    # static. `_scope_events` evaluates `#if`/`#else`/`#endif` branches so this stays
                    # accurate even when a preprocessor conditional's un-taken branch opens its own `{`.
                    if "function" in scope:
                        continue
                    indented = line[:1] in (" ", "\t")
                    # Indented: a local-looking variable unless it is `static` or genuinely inside a
                    # namespace block (a class body's own static is the in-class branch's job, not this one,
                    # and is already excluded above).
                    if indented and not re.search(r"\bstatic\b", line[:a]) and "namespace" not in scope:
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
        else:
            # Several instances (TU-local statics of one name): record every definition site, so that the
            # list shows each TU the one classification covers and a new TU's static is a visible change.
            # Also run this for a single-instance name: `index.find` can only guess which of several
            # same-named TU definitions is the one the linker actually kept (it has no symbol-to-TU mapping,
            # only candidates() sorted .cpp-first then alphabetically), so a count of 1 with more than one
            # candidate site is recorded as ambiguous the same way, rather than silently picking one (the
            # linker may have dropped it as unreferenced, as it did Diplomacy's unused ControlBarPopupDescription
            # siblings of `theLayout`/`theWindow`).
            sites = index.find_all(p)
            if len(sites) > 1:
                source = SITE_SEP.join(site for site, _decl in sites)
                sym.decls = [re.sub(r"\s+", " ", d) for _site, d in sites]
        sym.source = source
        sym.decl = re.sub(r"\s+", " ", decl)
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


# The declarator itself (before `=`) must be exactly a const FieldParse array: `FieldParse` merely
# appearing in the type (a mutable pointer, a container or map keyed/valued by it, a non-const array) is
# not provably a built-once table.
FIELDPARSE_DECL_RE = re.compile(r"^(?:static\s+)?const\s+FieldParse\s+\w+\s*\[[^\]]*\]\s*$")


def rule_field_parse(sym):
    """INI FieldParse table: built once, never written. By declared type, not by the variable's name alone
    (a variable merely named dataFieldParse/myFieldParse/commonFieldParse is not provably one), and not by
    `FieldParse` merely appearing in the type somewhere (a mutable pointer or container is not a table)."""
    for d in declarations(sym):
        decl = d.split("=")[0].rstrip().rstrip(";").rstrip()
        if not FIELDPARSE_DECL_RE.match(decl):
            return None
    return CONST, "", "INI FieldParse table: built once, never written"


CONST_DECL_RE = re.compile(r"^(?:static\s+)?(?:inline\s+)?(?:const\s+static|const)\b")
# `T* const name`, the only pointer declarator that cannot be reseated; a trailing array bound is allowed
# (`T* const table[]`), but anything after the name (another declarator, a function parameter list) is not.
CONST_PTR_DECL_RE = re.compile(r"\*\s*const\s+\w+\s*(?:\[[^\]]*\]\s*)*$")
# A `->` read through a global pointer (`TheGlobalData->m_mapName`, `TheWriterSomething->field`): per-engine
# or INI/map-dependent state captured by the first engine to run, whether the static itself is a pointer or
# a by-value object. This is only the arrow form; a by-value initialiser's other ways of reading the same
# kind of state (`(*TheGlobalData).m_mapName`, `TheGlobalData[0].m_mapName`, `*TheGlobalData`, a subscript,
# `++`/`--`, an assignment) are caught by `_by_value_init_is_safe`'s allow-list instead, not by this regex.
GLOBAL_LOOKUP_RE = re.compile(r"->")
# Pure value constructors used in this codebase to build a by-value const from literals alone: no lookup, no
# engine-state read, no counter or RNG draw. Nothing outside this list (and the declared type's own name) is
# accepted as a call initialiser.
SAFE_VALUE_CONSTRUCTOR_NAMES = {
    "GameMakeColor",
    "DEG_TO_RAD",
    "DEG_TO_RADF",
    "RAD_TO_DEG",
    "RAD_TO_DEGF",
    "Vector2",
    "Vector3",
    "Vector4",
    "Coord2D",
    "Coord3D",
    "ICoord2D",
    "ICoord3D",
    "Region2D",
    "Region3D",
    "RGBColor",
    "RGBAColorReal",
    "RGBAColorInt",
}
# Storage/qualifier keywords that are never the declared type or the variable name, skipped when reading
# a declaration's last two identifiers off (the variable name, then its type).
DECL_KEYWORDS = {"static", "const", "inline"}
# Characters after which a `*` is a prefix (dereference) rather than multiplication: the start of the
# initialiser, an opening bracket, a comma (the next constructor argument), or another operator. A `*`
# after an identifier, a literal or a closing bracket is always multiplication.
_STAR_PREFIX_CHARS = set("([{,=&|!<>+-*/%^~:")
# A subscript on anything (`theGameLogicSeed[0]`, `TheGlobalData[0]`): none of this codebase's safe,
# literal-only initialisers index into an array, so any `name[` is rejected outright.
SUBSCRIPT_RE = re.compile(r"\w\s*\[")
# A member access (`.field`) on an identifier (which, unlike a numeric literal, always starts with a letter
# or underscore, whatever digits follow) or a parenthesised/subscripted expression (`(*TheGlobalData).field`,
# `arr[0].field`): never a pure value. Matching the token itself, rather than a single preceding character,
# rules out a floating-point literal's own `.` (`0.f`, `1.0f`) without also missing a member access on an
# identifier that happens to end in a digit (`s_t2.field`): a digit alone, with no letter/underscore before
# it, can only be a number literal's own digits, never an identifier.
MEMBER_ACCESS_RE = re.compile(r"(?:[A-Za-z_]\w*|[)\]])\s*\.\s*[A-Za-z_]")
# `++`/`--`: a counter, never a pure value.
INC_DEC_RE = re.compile(r"\+\+|--")
# A bare assignment inside the initialiser (not `==`, `!=`, `<=`, `>=`): never a pure value.
ASSIGN_RE = re.compile(r"(?<![=!<>])=(?!=)")
# A numeric literal: optionally signed, decimal or hex, with an optional fractional/exponent part and a
# trailing type suffix (`f`, `u`, `l`, `ll`, in any case/combination).
_NUMBER_RE = r"[+-]?(?:0[xX][0-9a-fA-F]+|\d+\.?\d*(?:[eE][+-]?\d+)?)[uUlLfF]*"
LITERAL_RE = re.compile(rf"^(?:{_NUMBER_RE}|\"(?:[^\"\\]|\\.)*\"|'(?:[^'\\]|\\.)*')$")
# An ALL_CAPS identifier or enumerator, optionally `::`-qualified (`NAMEKEY_INVALID`, `Foo::BAR`, the scope
# itself in whatever case the class/namespace uses): a named constant, never a per-engine read. The caller
# still checks that the final (unqualified) segment is upper case.
ALL_CAPS_RE = re.compile(r"^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$")
# A C-style or functional cast (`(Int)expr`, `static_cast<Int>(expr)`) ahead of a value this rule still
# checks underneath: the cast itself reads nothing, so only the expression it casts need be a pure value.
_CAST_RE = re.compile(r"^\(\s*(?:const\s+)?[A-Za-z_][\w:]*\s*(?:[*&]\s*)?\)\s*(?P<rest>\S.*)$", re.DOTALL)
_STATIC_CAST_RE = re.compile(
    r"^(?:static|const|reinterpret)_cast\s*<[^<>]*>\s*\((?P<rest>.*)\)$", re.DOTALL
)
# A function (pointer) name, bare or `&`-taken, possibly scoped (`isConditionTrue`, `&foo`,
# `DeliverPayloadStateMachine::isOffMap`): accepted only as a direct argument of the declared type's own
# constructor (a callback field, such as StateConditionInfo's), never as a value on its own, and never when
# it ends in an ALL_CAPS segment (that is an enumerator, already accepted above as a plain value).
FUNC_PTR_ARG_RE = re.compile(r"^&?[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$")



def _declared_type_name(decl):
    """The declared type's own bare name (`WaypointMap` from `static const WaypointMap s_emptyWaypoints`):
    the declaration's last identifier is the variable being declared, so the one before it is its type."""
    tokens = [t for t in re.findall(r"[A-Za-z_]\w*", decl) if t not in DECL_KEYWORDS]
    return tokens[-2] if len(tokens) >= 2 else ""


def _split_top_level_commas(text):
    """`text` split on its top-level commas (inside no bracket of its own): the arguments of a call, or the
    elements of a brace list."""
    parts, depth, start = [], 0, 0
    for i, c in enumerate(text):
        if c in "({[":
            depth += 1
        elif c in ")}]":
            depth -= 1
        elif c == "," and depth == 0:
            parts.append(text[start:i])
            start = i + 1
    parts.append(text[start:])
    return [p.strip() for p in parts if p.strip()]


def _fully_parenthesized(expr):
    """True when `expr`'s leading `(` closes only at its very last character, so the whole expression is one
    parenthesised group (`(3)`, `(a + b)`), not a call whose own `(...)` is followed by more text."""
    depth = 0
    for i, c in enumerate(expr):
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i == len(expr) - 1
    return False


def _has_unsafe_operator(expr):
    """True when `expr` contains a dereference, a subscript, a member access, `++`/`--` or a bare
    assignment: none of this rule's pure value shapes (a literal, a named constant, a cast or an
    allow-listed/own-type constructor call) ever needs any of these."""
    if SUBSCRIPT_RE.search(expr) or MEMBER_ACCESS_RE.search(expr) or INC_DEC_RE.search(expr) or ASSIGN_RE.search(expr):
        return True
    prev = None
    for c in expr:
        if c == "*" and (prev is None or prev in _STAR_PREFIX_CHARS):
            return True
        if not c.isspace():
            prev = c
    return False


def _is_safe_value_expr(expr, type_name):
    """True when `expr` (a whole by-value initialiser, or one brace-list element / call argument of one)
    is provably a pure value: a literal, an ALL_CAPS named constant or enumerator, `true`/`false`/`nullptr`,
    a cast of another pure value, a call to an allow-listed pure value constructor or to the declared type's
    own name (each of its arguments checked the same way, except a lower-case-led function name, bare,
    `&`-taken or scoped, is accepted as a direct argument of the type's own constructor: a function-pointer
    field, such as StateConditionInfo's callback members, not a read), or a `{...}` list of pure values."""
    expr = expr.strip()
    if not expr:
        return True
    if _has_unsafe_operator(expr):
        return False
    if LITERAL_RE.match(expr) or expr in ("true", "false", "nullptr", "NULL"):
        return True
    if ALL_CAPS_RE.match(expr) and expr.rsplit("::", 1)[-1].isupper():
        return True
    m = _STATIC_CAST_RE.match(expr)
    if m:
        return _is_safe_value_expr(m.group("rest"), type_name)
    m = _CAST_RE.match(expr)
    if m and m.group("rest")[:1] != "(":
        # `(name)(args)` is a parenthesised callee (a call through a function pointer, or a macro that
        # expands to one), not a C-style cast: a real cast's operand is never itself a call whose own
        # `(...)` could be mistaken for the cast's parentheses. Falling through here (rather than
        # recursing into "(args)" as the cast's operand) leaves this expression to the checks below, all
        # of which require a leading identifier or `{`/`(` that is not this cast-looking prefix, so it is
        # rejected rather than read as a cast of a safe-looking argument list.
        return _is_safe_value_expr(m.group("rest"), type_name)
    if expr[0] == "{" and expr[-1] == "}":
        return all(_is_safe_value_expr(e, type_name) for e in _split_top_level_commas(expr[1:-1]))
    if expr[0] == "(" and _fully_parenthesized(expr):
        # Direct-initialisation's own `(...)` (`pick(3)`'s initialiser is recorded as `(3)`, `Matrix3D::
        # Identity`'s is `(1.0, 0.0, ..., 0.0)`) or a plain grouping: neither is a call (a call starts with
        # the callee's name, not `(`), so unwrap one layer. Several top-level, comma-separated arguments
        # are the declared type's own constructor args (direct-init has no other way to pass more than
        # one); a single one is just a parenthesised expression and is checked the same way either way.
        parts = _split_top_level_commas(expr[1:-1])
        if len(parts) > 1:
            return all(_is_safe_value_expr(p, type_name) for p in parts)
        return _is_safe_value_expr(expr[1:-1], type_name)
    call = re.match(r"^([A-Za-z_]\w*)\s*\((.*)\)$", expr, re.DOTALL)
    if call and (call.group(1) in SAFE_VALUE_CONSTRUCTOR_NAMES or call.group(1) == type_name):
        args = _split_top_level_commas(call.group(2))
        return all(
            _is_safe_value_expr(a, type_name)
            or (
                call.group(1) == type_name
                and FUNC_PTR_ARG_RE.match(a)
                and a.lstrip("&").rsplit("::", 1)[-1][:1].islower()
            )
            for a in args
        )
    return False


def _by_value_init_is_safe(init, type_name):
    """True when the whole by-value initialiser is a provably pure value (see `_is_safe_value_expr`):
    literals, named constants, casts and allow-listed/own-type constructor calls, built from each other and
    from nothing else. A dereference, a subscript, a member access, `.`/`[...]` on anything but a brace
    list, `++`/`--`, an assignment, or a call that is not on the allow-list (a one-time lookup, a getter
    backed by engine globals, an RNG draw such as GameLogicRandomValue advancing the per-engine logic RNG,
    even with literal arguments) is rejected."""
    return _is_safe_value_expr(init, type_name)


# The address of a named value (`&kDefault`), optionally scoped: the only way this codebase's safe pointer
# initialisers take an address, as opposed to copying another pointer.
POINTER_ADDR_RE = re.compile(r"^&[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$")


def _pointer_init_is_safe(init):
    """True when a `T* const` pointer's by-value initialiser is provably never a per-engine read: an
    allow-list, not a deny-list of only `->` and a call (which a bare copy of a per-engine global pointer,
    `static ThingFactory* const s_factory = TheThingFactory;`, matches neither of, and so would pass).
    Only nullptr/NULL, a literal (a null pointer's own `0`, or a string literal for a `const char* const`),
    the address of a named value, and a cast of one of these are safe; a bare identifier (a copy of another
    pointer, per-engine or not), a subscript and any call are rejected, the same per-engine pointer-cache
    defect as a reassignable pointer's own `TheX->find(...)` shape."""
    init = init.strip()
    if init[:1] == "(" and init[-1:] == ")" and _fully_parenthesized(init):
        return _pointer_init_is_safe(init[1:-1])
    m = _CAST_RE.match(init)
    if m and m.group("rest")[:1] != "(":
        return _pointer_init_is_safe(m.group("rest"))
    m = _STATIC_CAST_RE.match(init)
    if m:
        return _pointer_init_is_safe(m.group("rest"))
    if init in ("nullptr", "NULL"):
        return True
    if LITERAL_RE.match(init):
        return True
    return bool(POINTER_ADDR_RE.match(init))


def _is_balanced(text):
    """True when every `(`/`[`/`{` in `text` closes before the text ends, and nothing closes early. A copy
    (`= expr`) initialiser that fails this was cut off mid-expression (the recorded text ran out before the
    statement did), not a complete expression that merely contains brackets."""
    depth = 0
    for c in text:
        if c in "({[":
            depth += 1
        elif c in ")}]":
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


def _split_initializer(decl):
    """(declarator, initialiser text) for a recorded declaration line, however it initialises: copy
    (`= expr`), direct (`name(expr)`) or brace (`name{expr}`). The initialiser is `""` when the declarator
    has none at all (a default-constructed `static const Foo foo;`), and `None` when the recorded line
    (one source line) does not hold it: it ends with a bare `=`, or with an unclosed `(`/`{`/`[`, because the
    initialiser starts on the next source line, or because the scan could not find the declarator/initialiser
    boundary at all. The caller must treat `None` as not provably safe, never as an empty, trivially-safe
    one.

    The scan for the first `=`, `(` or `{` skips over balanced `[...]` groups (an array bound's own
    parentheses, `[ARRAY_SIZE(x)]` or `[sizeof(T)]`), so it lands on the declarator/initialiser boundary
    after the declared name and its array bounds, never on a `(` that is part of the bound itself. `<`/`>`
    are never treated as brackets: inside an array bound they are almost always operators (`[1 << 4]`,
    `[N < 2 ? 1 : 2]`, `[N >> 1]`), not a template argument list, and misreading them as brackets either
    swallows the real `=` (leaving the depth above zero for the rest of the declaration, so the scan runs
    off the end) or drops the depth back to zero mid-bound (handing the boundary to a later `(` that is
    still part of the bound). The same reason keeps the direct/brace branch from trusting the first matched
    group it finds: the declared type's own template arguments can hold a `(`/`{` too (`std::function<Int()>`,
    `Foo<sizeof(INT)>`), and two comma-joined declarators share one recorded line (`x{1}, y = expr`). Once
    that group's closing bracket is found, anything other than the end of the declaration after it means
    the group was not the initialiser after all, so the boundary (and the real initialiser) is still
    unknown."""
    i, n, depth = 0, len(decl), 0
    while i < n:
        c = decl[i]
        if c == "[":
            depth += 1
        elif c == "]":
            depth = max(0, depth - 1)
        elif depth == 0 and c in "=({":
            break
        i += 1
    if i == n:
        # Either a plain declaration with no initialiser at all (depth stayed 0 throughout: a safe, empty
        # initialiser), or the scan ran off the end still inside an unbalanced `[...]` (a malformed or
        # unrecognised declarator): fail closed rather than assume "no initialiser".
        return decl, (None if depth else "")
    declarator = decl[:i].rstrip()
    if decl[i] == "=":
        init = decl[i + 1 :].strip()
        if init and not _is_balanced(init):
            # The recorded text ran out mid-expression (the statement is longer than the caller's scan
            # window): treat this the same as an unclosed `(`/`{` initialiser below, never as a complete,
            # literal-looking one.
            return declarator, None
        return declarator, (init or None)
    open_c, close_c = decl[i], (")" if decl[i] == "(" else "}")
    depth, j = 0, i
    while j < n:
        if decl[j] == open_c:
            depth += 1
        elif decl[j] == close_c:
            depth -= 1
            if depth == 0:
                if decl[j + 1 :].strip():
                    return declarator, None
                return declarator, decl[i : j + 1]
        j += 1
    return declarator, None


def rule_const_object(sym):
    """A const object (dynamically initialised, so it lands in .data/.bss): never written after its
    initialisation. A reference is never safe here (it can alias per-engine state at any type). A pointer is
    safe only when the pointer itself cannot be reseated (`T* const name`, not `const T*`, whatever the
    pointee type) and its initialiser is provably built from nothing but nullptr/NULL, a literal, the
    address of a named value, and casts of these (`_pointer_init_is_safe`'s allow-list): a per-engine
    pointer cache can fill the slot from either a one-time lookup (`T* const name = TheX->find(...)`) or a
    bare copy of another per-engine global pointer (`T* const name = TheGlobalData`), and both are
    rejected. A by-value const must have its whole initialiser provably built from pure
    values and nothing else (`_by_value_init_is_safe`'s allow-list: literals, named constants, casts and
    allow-listed/own-type constructor calls): any way of reading or changing per-engine state is rejected,
    whether a `->` read (`static const Real r = TheGlobalData->m_maxCameraHeight;`), a dereference or
    subscript (`*TheGlobalData`, `theGameLogicSeed[0]`), a member access on either (`(*TheGlobalData).m`,
    `TheGlobalData[0].m`), a call that is not on the allow-list (`static const Int pick =
    GameLogicRandomValue(0, 3);` advances the per-engine logic RNG despite its literal arguments), or
    `++`/`--`/an assignment. Every one of these captures whichever engine ran first, or diverges per engine,
    the same first-engine-wins defect as the pointer case. This holds whichever syntax initialises it (copy,
    direct `name(expr)` or brace `name{expr}`); a declaration whose recorded line ends before its initialiser
    does (a continuation onto the next source line, or a statement longer than the caller's scan window, so
    the text we have simply stops without reaching the statement's own `;`) is never assumed safe."""
    for d in declarations(sym):
        if not d.rstrip().endswith(";"):
            return None
        d = d.rstrip().rstrip(";").rstrip()
        decl, init = _split_initializer(d)
        if init is None:
            return None
        if "&" in decl:
            return None
        if GLOBAL_LOOKUP_RE.search(init):
            return None
        if "*" in decl:
            if not CONST_PTR_DECL_RE.search(decl) or not _pointer_init_is_safe(init):
                return None
        else:
            if not CONST_DECL_RE.match(decl):
                return None
            if not _by_value_init_is_safe(init, _declared_type_name(decl)):
                return None
    return CONST, "", "const object: initialized once, never written"


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
    "# found; a count-1 name can still list several `;`-joined candidate sites when the search cannot tell\n"
    "# which one build configuration actually links, for example two platform files that are never both\n"
    "# built); class; phase (per-engine only); note; by (hand, file = a hand `file:` pattern, rule:<name>,\n"
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
            "third-party symbols will not be recognized",
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


def compare(key, sym, row, strict=False):
    """(errors, stale) for a symbol the list records. Errors in every mode: a class or phase that changed
    (including to `unreviewed`: a hand entry deleted, or a hand regex or `file:` pattern that stopped
    matching after an upstream rename or move), a blanket `file:` match replacing a reviewed one, and a new
    instance of an already listed name (a count or a set of defining files that grew: a new TU's static of
    the same name would otherwise inherit the existing classification unseen). Stale only: fewer instances,
    definitions that moved, and, with `strict`, any other column (scope, binding, section, bytes, note, by)
    that no longer matches what `row_of(sym)` would write today: those never fail a non-strict `check`
    (the row's classification is still correct), but they do mean the checked-in TSV is not what `snapshot`
    writes, which `--strict` promises to catch."""
    errors, stale = [], []
    if row["class"] != sym.cls or row["phase"] != sym.phase:
        errors.append(f"class changed: {key}: {row['class']} {row['phase']} -> {sym.cls} {sym.phase} (by {sym.by})")
    elif sym.by == "file" and row["by"] != "file":
        errors.append(f"now classified only by a file pattern (was {row['by']}): {key} ({sym.source})")
    try:
        recorded_count = int(row["count"])
    except (KeyError, ValueError):
        recorded_count = 0
    # `?` (no definition found) is never a "new" file: a listed definition the search can no longer find
    # (moved into a macro, outside SCAN_ROOTS, ...) must fall through to the "definition moved" staleness
    # below, not read as a brand-new defining file that needs review.
    new_files = source_files(sym.source) - source_files(row.get("source", "")) - {"?"}
    if sym.count > recorded_count or new_files and row.get("source") != "?":
        errors.append(
            f"new instance of a listed name: {key}: {recorded_count} -> {sym.count} instances"
            + (f", new definition in {', '.join(sorted(new_files))}" if new_files else "")
            + " (the listed classification would cover it unseen)"
        )
    elif sym.count < recorded_count:
        stale.append(f"fewer instances: {key}: {recorded_count} -> {sym.count}")
    elif row.get("source") != sym.source:
        stale.append(f"definition moved: {key}: {row.get('source')} -> {sym.source}")
    if strict:
        current = row_of(sym)
        for col in ("scope", "binding", "section", "bytes", "note", "by"):
            if row.get(col, "") != current[col]:
                stale.append(f"{col} changed: {key}: {row.get(col, '')!r} -> {current[col]!r}")
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
    # A stripped library, or the wrong file, gives `nm` no symbols at all (GNU nm prints "no symbols" and
    # still exits 0, and `demangle([])` now returns `[]` rather than tripping its own line-count guard):
    # `read_library` then returns no symbols, and every recorded row would land in the harmless-looking
    # "gone from the library" warning, so a non-strict `check` would pass having checked nothing. Fail
    # outright instead. Checked before `read_tsv` (not after): a missing or unreadable TSV must never stand
    # in for this check by exiting non-zero first for an unrelated reason.
    if not symbols:
        sys.exit(f"{args.lib}: no writable/unique symbols found (stripped library, or the wrong file?)")
    recorded = read_tsv(os.path.join(args.root, TSV))
    if recorded and len(symbols) < len(recorded) // 2:
        sys.exit(
            f"{args.lib}: only {len(symbols)} of {len(recorded)} recorded symbols found in the library "
            "(stripped, partial, or the wrong build?)"
        )
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
            e, st = compare(key, sym, row, args.strict)
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
