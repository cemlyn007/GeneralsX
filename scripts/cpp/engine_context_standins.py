#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 30/09/2026 The sizeof/& gate for EngineContext stand-ins (PLAN-023 Phase 8, stage RR2a-1)
#
# With RTS_ENGINE_CONTEXT, a class's static data member can become a stand-in for a per-engine field:
# `static constexpr rts::ContextField<T, &rts::EngineContext::field> name{};` or its indirect variant,
# `rts::IndirectContextField<...>` (Core/GameEngine/Include/Common/EngineContext.h). A stand-in is an empty
# object that converts to the field, so reads, assignments and comparisons compile unchanged, and so do
# `sizeof(name)` and `&name`, which then give the stand-in's size and address, not the field's. This fails on
# either use of any stand-in, so a name used that way is caught at review time (it needs a reference-returning
# macro instead, as DX8Wrapper's names have: WW3D2/w3drenderstate_names.h).
#
# Where it looks, for a stand-in `name` of class `C` declared in `c.h`: `C::name` in every source file, and
# `name` unqualified in `c.h`, in every file that defines one of `C`'s members, and in every file that
# defines a member of a class `D` deriving (directly or transitively) from `C` (ordinary unqualified name
# lookup finds `C`'s static member inside `D`'s own member functions too; `D`'s base clause is found by
# content across every source file, since `D`'s own header need not mention the stand-in's name, e.g.
# W3DAssetManager.h does not mention WW3DAssetManager::TheInstance). A member-defining file is found by
# content, not by file name (a class's members are not always defined in a file named after the class,
# e.g. MapObject's are in WorldHeightMap.cpp): a `C::member(` (or `D::member(`) whose parameter list's
# matching close paren is followed (after optional whitespace and any of `const`/`noexcept`/`override`/
# `final`) by `{`, or, only when `member` is a constructor (names the same class), an initialiser's `:`,
# not `;`. That excludes a declaration, a bare call statement (`C::member();`) and a call inside a return/
# case/ternary/other statement (`return C::member();`, `case C::member():`, `cond ? C::member() : x`),
# none of which are `C`'s own code; `*`/`&` may attach to the return type or straight to `C`'s name either
# way (`T *C::member()`, `T* C::member()`), and the return type may sit on the line above. `sizeof(name[0])`, `&name[i]`,
# `&name->x` and `&name.x` are allowed: they reach the field itself. `std::addressof(name)` is the same
# hazard as unary `&` and is caught too. A unary `&` is also recognised after a C-style pointer cast
# (`(void*)&name`, `(unsigned char*)&name`, `(struct Foo*)&name`) and after `&&` (`a && &name`), not only
# after the fixed set of operators it is otherwise unambiguous after. `name` passed bare as a whole
# argument to a direct variadic function (the printf family, `DebugLog`, `DebugLogRaw`, `DebugCrash`,
# `WWDebug_Printf`, `WWDebug_Printf_Warning`, `WWDebug_Printf_Error`) or to a `Format`/`format` call
# (`StringClass::Format`, `WideStringClass::Format`, `AsciiString::format`, `UnicodeString::format`,
# `Debug::Format`: every `Format`/`format` declared in the tree is a printf-style variadic, matched on the
# method name alone so a call through any instance is caught) is flagged too: it passes the empty
# stand-in object instead of the field's value. So is `name` passed as a whole element of the
# doubled-parens message list that every logging macro built on Debug.h's shape takes (`WWDEBUG_SAY`,
# `WWRELEASE_SAY`, `DEBUG_LOG`, `SNAPSHOT_SAY`, `SHATTER_DEBUG_SAY`, ... and any later one, however it is
# named): a comma inside the list survives the macro's own argument split only because the list is
# itself one extra, whole, parenthesised argument, e.g. `WWDEBUG_SAY(("bits %d", BitDepth))`,
# `DEBUG_ASSERTLOG(c, ("bits %d", BitDepth))`. That shape — a parenthesised argument, itself a complete
# top-level argument, led by a string or char literal — is recognised generically, not by an enumerated
# macro name, so a logger this gate has never heard of is still caught. A two-argument macro's
# condition/level argument (`DEBUG_ASSERTLOG`'s and `DEBUG_ASSERTCRASH`'s `c`, `DEBUG_LOG_LEVEL`'s and
# `DEBUG_LOG_LEVEL_RAW`'s `l`) is never flagged: it is not itself a parenthesised list, so it reaches the
# field through the stand-in's own conversion operator (`!(c)`, `l & DebugLevelMask`), not through `...`.
# Comments and string literals are ignored.
#
# Usage: engine_context_standins.py [--root GeneralsX] [--extra-dir DIR ...] [--list]
#   exit 0 when no stand-in is used with sizeof, &, std::addressof or a variadic logger, 1 (with each use
#   listed) otherwise; --list prints the stand-ins found. Stand-ins are always declared under --root; a
#   use is looked for there and in every --extra-dir too (a consumer's own C++, outside the
#   Core/Generals/GeneralsMD layout, that includes GeneralsX's headers declaring them, e.g.
#   rlgenerals/launcher), so a consumer using a stand-in with sizeof or & is caught as well.
import argparse
import os
import re
import sys

SOURCE_DIRS = ("Core", "Generals", "GeneralsMD")
SOURCE_EXTENSIONS = (".h", ".hpp", ".inl", ".cpp", ".cc")

STANDIN = re.compile(
    r"static\s+constexpr\s+(?:::)?rts::(?:Indirect)?ContextField\s*<(?P<args>.*?)>\s*(?P<name>\w+)\s*\{\s*\}\s*;",
    re.S,
)
CLASS = re.compile(r"^\s*(?:class|struct)\s+(?:\w+\s+)*?(?P<name>\w+)\s*(?::[^;{]*)?\{", re.M)
# `class D : public C, private E { ... }` / `struct D : C { ... }`: a derived class's direct base list,
# scanned across every source file (a derived class's own header may never mention a stand-in's name,
# e.g. W3DAssetManager.h does not mention WW3DAssetManager::TheInstance).
BASE_CLAUSE = re.compile(r"^[ \t]*(?:class|struct)\s+(?:\w+\s+)*?(?P<name>\w+)\s*:\s*(?P<bases>[^;{]*?)\{", re.M)
BASE_ACCESS = re.compile(r"\b(?:public|private|protected|virtual)\b", re.I)


def _direct_bases(bases_text):
    """The (unqualified, template-stripped) names in a base-clause's comma-separated list."""
    out = []
    for part in bases_text.split(","):
        part = BASE_ACCESS.sub("", part).strip()
        part = part.split("<", 1)[0].strip()  # drop template arguments, e.g. `Foo<T>` -> `Foo`
        if part:
            out.append(part.rsplit("::", 1)[-1])
    return out
# After the name: something that reaches the field itself, not the stand-in.
REACHES_FIELD = re.compile(r"\s*(?:\[|\.|->|\()")
# Before a unary `&`: an operator, an opening bracket, a separator or `return`.
UNARY_BEFORE = re.compile(r"(?:^|[=(,{};?:!<>+\-*/%|^~\[]|\breturn)\s*$")
# A C-style pointer cast immediately before: `(void*)`, `(char*)`, `(Foo::Bar**)`, `(unsigned char*)`,
# `(const unsigned char *)`, `(struct Foo*)`, `(void const*)`, ... One or more type words (cv/sign/size/
# elaborated-type keywords, or a plain/`::`-qualified identifier), in any order, then one or more `*`/`&`.
# A cast with no `*` (`(int)&name`) is indistinguishable from `(expr)&mask` (binary `&`), so it is not
# matched here.
_CAST_WORD = r"(?:const|volatile|unsigned|signed|long|short|struct|class|enum|[A-Za-z_]\w*(?:\s*::\s*[A-Za-z_]\w*)*)"
CAST_BEFORE = re.compile(r"\(\s*" + _CAST_WORD + r"(?:\s+" + _CAST_WORD + r")*(?:\s*[*&])+\s*\)\s*$")
# `&&` (logical-and) immediately before, with or without a space: the following `&` is always unary.
DOUBLE_AMP_BEFORE = re.compile(r"&&\s*$")
# A bare stand-in passed to one of these: a direct variadic call takes its argument's value by its
# declared type (`...`), with no user-defined conversion, so an empty stand-in object is passed, not the
# field's value. These take their format string and values directly as ordinary comma-separated
# arguments, with no doubled-parens wrapping (contrast the macros `_doubled_paren_message_lists` finds
# generically, by shape, below).
VARIADIC_FUNCS = (
    "printf",
    "fprintf",
    "sprintf",
    "snprintf",
    "vsnprintf",
    "_snprintf",
    "swprintf",
    "vswprintf",
    "DebugLog",
    "DebugLogRaw",
    "DebugCrash",
    "WWDebug_Printf",
    "WWDebug_Printf_Warning",
    "WWDebug_Printf_Error",
    "Format",
    "format",
)
VARIADIC_CALL = re.compile(r"\b(?:" + "|".join(VARIADIC_FUNCS) + r")\s*\(")
# A parenthesised argument that is itself a complete doubled-parens message list: an opening `(`
# immediately after the call's own `(` (`IDENT((...))`), or after a `,` (`IDENT(c, (...))`), either way
# with any amount of whitespace in between.
_DOUBLED_PAREN_OPEN = re.compile(r"(?P<prev>[(,])\s*(?P<open>\()")


def strip_comments_and_strings(text):
    """The text with comments and string/char literals blanked (newlines kept, so lines still count)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c in "\"'":
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(c + " " * (j - i - 2) + (c if j - i >= 2 else ""))
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def source_files(root, extra_dirs=()):
    """Every source file under --root's SOURCE_DIRS, plus every one under each of extra_dirs directly
    (no Core/Generals/GeneralsMD layout expected there: a consumer's own tree, e.g. rlgenerals/launcher)."""
    for top in SOURCE_DIRS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
            for f in filenames:
                if f.endswith(SOURCE_EXTENSIONS):
                    yield os.path.join(dirpath, f)
    for extra in extra_dirs:
        for dirpath, dirnames, filenames in os.walk(extra):
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
            for f in filenames:
                if f.endswith(SOURCE_EXTENSIONS):
                    yield os.path.join(dirpath, f)


def find_standins(files):
    """(class, name, header path) of every stand-in declaration."""
    found = []
    for path, text in files.items():
        if "ContextField" not in text:
            continue
        for m in STANDIN.finditer(text):
            classes = [c for c in CLASS.finditer(text, 0, m.start())]
            owner = classes[-1].group("name") if classes else ""
            found.append((owner, m.group("name"), path))
    return found


_DEF_TRAILING_KEYWORDS = ("const", "noexcept", "override", "final")


def _matching_close_paren(text, open_idx):
    """The index of the `)` matching the `(` at open_idx, or None if text ends first."""
    depth, i, n = 0, open_idx, len(text)
    while i < n:
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return None


def _looks_like_definition(text, close_idx, cls, member):
    """Whether what follows the parameter list's close paren at close_idx is a definition's own shape:
    `{`, or (only for a constructor, where `member` names the same class as `cls`) a constructor
    initialiser's `:`, optionally after whitespace and any of `const`/`noexcept`/`override`/`final` (in
    any order), rather than a declaration's or a call statement's `;`. A non-constructor's `:` is never a
    definition's own shape: it is a ternary's (`cond ? C::f() : x`) or a case label's (`case C::f():`),
    both of which are calls inside some other statement, not `C`'s own code."""
    i, n = close_idx + 1, len(text)
    while True:
        while i < n and text[i] in " \t\r\n":
            i += 1
        for kw in _DEF_TRAILING_KEYWORDS:
            if text[i : i + len(kw)] == kw and not (
                text[i + len(kw) : i + len(kw) + 1].isalnum() or text[i + len(kw) : i + len(kw) + 1] == "_"
            ):
                i += len(kw)
                break
        else:
            break
    if i >= n:
        return False
    if text[i] == "{":
        return True
    return text[i] == ":" and member == cls


def _split_top_level_commas(s):
    """[(start, end), ...] spans of s's top-level (depth-0) comma-separated pieces."""
    depth, piece_start, spans = 0, 0, []
    for idx, ch in enumerate(s):
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            spans.append((piece_start, idx))
            piece_start = idx + 1
    spans.append((piece_start, len(s)))
    return spans


def _enclosing_open_paren(text, pos):
    """The index of the `(` that owns the top-level argument list containing position pos, i.e. the
    nearest unmatched `(` scanning backward from pos."""
    depth, i = 0, pos - 1
    while i >= 0:
        c = text[i]
        if c == ")":
            depth += 1
        elif c == "(":
            if depth == 0:
                return i
            depth -= 1
        i -= 1
    return None


def _call_name_before(text, open_idx):
    """The identifier (or keyword) immediately before text[open_idx] (that call's own open paren),
    skipping whitespace, or a generic placeholder when there is none to find."""
    i = open_idx
    while i > 0 and text[i - 1] in " \t\r\n":
        i -= 1
    j = i
    while j > 0 and (text[j - 1].isalnum() or text[j - 1] == "_"):
        j -= 1
    return text[j:i] or "a logger"


def _doubled_paren_message_lists(text):
    """(func_name, content_base_index, content, item_spans) for every argument, anywhere in text, that
    is itself a complete parenthesised list led by a string or char literal: the doubled-parens idiom
    every logging macro built on Debug.h's shape takes (WWDEBUG_SAY, WWRELEASE_SAY, DEBUG_LOG,
    SNAPSHOT_SAY, SHATTER_DEBUG_SAY, ... and any later one), recognised by that shape alone, not by an
    enumerated macro name, so a logger this gate has never heard of is still caught."""
    out = []
    for m in _DOUBLED_PAREN_OPEN.finditer(text):
        open_idx = m.start("open")
        close_idx = _matching_close_paren(text, open_idx)
        if close_idx is None:
            continue
        j = close_idx + 1
        while j < len(text) and text[j] in " \t\r\n":
            j += 1
        if j < len(text) and text[j] not in ")," :
            continue  # not a complete, standalone argument of whatever call or list encloses it
        content = text[open_idx + 1 : close_idx]
        items = _split_top_level_commas(content)
        first = content[items[0][0] : items[0][1]].strip()
        if not first or first[0] not in "\"'":
            continue  # not format-string-led: an ordinary parenthesised sub-expression, not this idiom
        if m.group("prev") == "(":
            enclosing_open = m.start("prev")
        else:
            enclosing_open = _enclosing_open_paren(text, m.start("prev"))
        func = _call_name_before(text, enclosing_open) if enclosing_open is not None else "a logger"
        out.append((func, open_idx + 1, content, items))
    return out


def uses(text, name, qualifier):
    """(line, what) of each `sizeof`, unary `&`, `std::addressof` or bare variadic-argument use of the
    stand-in in the stripped text."""
    ident = (re.escape(qualifier) + r"\s*::\s*" if qualifier else r"(?<![\w:.>])") + re.escape(name) + r"\b"
    bad = []
    for m in re.finditer(r"\bsizeof\s*(?:\(\s*)?" + ident, text):
        if not REACHES_FIELD.match(text, m.end()):
            bad.append((text.count("\n", 0, m.start()) + 1, "sizeof"))
    for m in re.finditer(r"&\s*" + ident, text):
        before = text[max(0, m.start() - 40):m.start()]
        if DOUBLE_AMP_BEFORE.search(before):
            pass  # `&&` then this unary `&`: always address-of, never a binary `&` of `&&`'s result
        elif before.endswith("&"):
            continue  # the no-space `&&name` token: always logical-and, never unary
        elif CAST_BEFORE.search(before):
            pass  # a C-style pointer cast, e.g. `(void*)&name`
        elif not UNARY_BEFORE.search(before):
            continue  # a binary `&`
        if not REACHES_FIELD.match(text, m.end()):
            bad.append((text.count("\n", 0, m.start()) + 1, "&"))
    for m in re.finditer(r"\bstd::addressof\s*\(\s*" + ident, text):
        if not REACHES_FIELD.match(text, m.end()):
            bad.append((text.count("\n", 0, m.start()) + 1, "std::addressof"))
    for cm in VARIADIC_CALL.finditer(text):
        open_paren = cm.end() - 1
        close_paren = _matching_close_paren(text, open_paren)
        if close_paren is None:
            continue
        inner = text[open_paren + 1 : close_paren]
        func = cm.group(0).split("(", 1)[0].strip()
        base = open_paren + 1
        # A direct variadic call (not a doubled-parens macro): every top-level argument is checked as is.
        for a0, a1 in _split_top_level_commas(inner):
            arg_text = text[base + a0 : base + a1]
            stripped = arg_text.strip()
            if stripped and re.fullmatch(ident, stripped):
                arg_lead = len(arg_text) - len(arg_text.lstrip())
                pos = base + a0 + arg_lead
                bad.append((text.count("\n", 0, pos) + 1, f"passed by value to {func}"))
    # Every doubled-parens message list, under any macro name: the format string and each logged value
    # are its top-level comma-separated items, found generically by the list's own shape (a parenthesised
    # argument, itself a complete top-level argument, led by a string or char literal), not by matching a
    # macro name. DEBUG_ASSERTLOG's/DEBUG_ASSERTCRASH's condition and DEBUG_LOG_LEVEL's/
    # DEBUG_LOG_LEVEL_RAW's level sit in a different, unwrapped argument and are never reached by this.
    for func, content_base, content, items in _doubled_paren_message_lists(text):
        for s0, s1 in items:
            arg_text = content[s0:s1]
            stripped = arg_text.strip()
            # Flag a name only when it IS a whole top-level item (not converted by a cast, a comparison,
            # or a call that takes it and returns something else).
            if stripped and re.fullmatch(ident, stripped):
                arg_lead = len(arg_text) - len(arg_text.lstrip())
                pos = content_base + s0 + arg_lead
                bad.append((text.count("\n", 0, pos) + 1, f"passed by value to {func}"))
    return bad


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0] if __doc__ else None)
    parser.add_argument("--root", default=".", help="the GeneralsX checkout (default: the current directory)")
    parser.add_argument(
        "--extra-dir",
        action="append",
        default=[],
        metavar="DIR",
        help="additional directory (relative to the current directory, or absolute) to scan directly for "
        "uses, e.g. a consumer's own C++ that includes GeneralsX's headers; repeatable",
    )
    parser.add_argument("--list", action="store_true", help="print the stand-ins found")
    args = parser.parse_args()

    raw = {}
    for path in source_files(args.root, args.extra_dir):
        with open(path, encoding="utf-8", errors="replace") as f:
            raw[path] = f.read()
    standins = find_standins({p: strip_comments_and_strings(t) for p, t in raw.items() if "ContextField" in t})
    # Only the files that mention a stand-in's name need a closer look.
    names = re.compile(r"\b(?:" + "|".join(sorted({re.escape(n) for _, n, _ in standins})) + r")\b") if standins else None
    files = {p: strip_comments_and_strings(t) for p, t in raw.items() if names is not None and names.search(t)}
    if not standins:
        print("error: no stand-in found; is --root a GeneralsX checkout?", file=sys.stderr)
        return 2
    if args.list:
        for owner, name, path in standins:
            print(f"{owner}::{name}  [{os.path.relpath(path, args.root)}]")

    # Files that define at least one member of each class with a stand-in, found by content (a class's
    # members are not always defined in a file named after the class, e.g. MapObject's are in
    # WorldHeightMap.cpp): a `C::member(` whose parameter list's matching close paren is followed by `{`
    # or, only for a constructor, an initialiser's `:`, not `;`. That is a definition's own shape; it is
    # not a declaration (`;`), a bare call statement (`C::member();`) or a call inside some other
    # statement (`return C::member();`, `case C::member():`, `cond ? C::member() : x`), none of which
    # make this `C`'s own file. The return type, if any, is not inspected at all (and so needs no prefix
    # pattern of its own): it may sit on the same line, with `*`/`&` attached to either word
    # (`T *C::member()`, `T* C::member()`), or on the line above. A class `D` deriving (directly or
    # transitively) from `C` can also name `C`'s static member unqualified inside `D`'s own member
    # functions (ordinary unqualified name lookup finds an inherited member), so a file that defines only
    # `D`'s members is `C`'s own file too (base classes found by content, across every source file, since
    # a derived class's own header, e.g. W3DAssetManager.h, need not mention the stand-in's name). That
    # includes an inline member body written directly in `D`'s own class body (`class D : public C { void
    # f() { ... TheInstance ... } };`), which has no `D::` qualifier for member_def to match at all: the
    # file holding the base clause that makes `D` derive from `C` (or from another such `D`) is `D`'s own
    # file too, whether or not it also holds an out-of-class `D::member(...) {` definition.
    owners = {owner for owner, _, _ in standins if owner}
    defines_member = {}
    if owners:
        children_of = {}
        base_clause_file = {}
        for path, raw_text in raw.items():
            stripped = strip_comments_and_strings(raw_text)
            for m in BASE_CLAUSE.finditer(stripped):
                for base in _direct_bases(m.group("bases")):
                    children_of.setdefault(base, set()).add(m.group("name"))
                base_clause_file.setdefault(m.group("name"), set()).add(path)
        # class_owners[D] = every stand-in owner that D is, or (transitively) derives from.
        class_owners = {}
        for owner in owners:
            stack, seen = [owner], {owner}
            while stack:
                cur = stack.pop()
                class_owners.setdefault(cur, set()).add(owner)
                for child in children_of.get(cur, ()):
                    if child not in seen:
                        seen.add(child)
                        stack.append(child)
        for cls, classes in class_owners.items():
            for path in base_clause_file.get(cls, ()):
                defines_member.setdefault(path, set()).update(classes)
        member_def = re.compile(
            r"(?<![\w:])(?P<cls>" + "|".join(re.escape(c) for c in sorted(class_owners)) + r")"
            r"\s*::\s*(?P<member>~?[A-Za-z_]\w*)\s*\(",
        )
        for path, text in files.items():
            classes = set()
            for m in member_def.finditer(text):
                close = _matching_close_paren(text, m.end() - 1)
                if close is not None and _looks_like_definition(text, close, m.group("cls"), m.group("member")):
                    classes.update(class_owners.get(m.group("cls"), ()))
            if classes:
                defines_member.setdefault(path, set()).update(classes)

    violations = []
    for owner, name, header in standins:
        own = {header} | {p for p, classes in defines_member.items() if owner in classes}
        for path, text in files.items():
            found = uses(text, name, owner) if owner else []
            if path in own:
                found += uses(text, name, "")
            for line, what in sorted(set(found)):
                violations.append(f"{os.path.relpath(path, args.root)}:{line}: {what} of the stand-in {owner}::{name}")

    for v in violations:
        print(v)
    if violations:
        print(
            f"{len(violations)} use(s) of an EngineContext stand-in that give its own size, address or bytes "
            "instead of the field's (sizeof, &, std::addressof, or passed bare to a variadic logger); use a "
            "reference-returning macro for that name instead (see EngineContext.h, ContextField)",
            file=sys.stderr,
        )
        return 1
    print(f"ok: {len(standins)} stand-ins, none used with sizeof, &, std::addressof or a variadic logger")
    return 0


if __name__ == "__main__":
    sys.exit(main())
