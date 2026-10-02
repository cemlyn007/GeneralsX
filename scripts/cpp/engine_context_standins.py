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
# `name` unqualified in `c.h` and in every file that defines one of `C`'s members: a `C::member(` found by
# content, not by file name (a class's members are not always defined in a file named after the class, e.g.
# MapObject's are in WorldHeightMap.cpp), whose parameter list's matching close paren is followed (after
# optional whitespace and any of `const`/`noexcept`/`override`/`final`) by `{` or a constructor
# initialiser's `:`, not `;`. That excludes a declaration, a bare call statement (`C::member();`) and a
# call inside a return/case/other statement (`return C::member();`), none of which are `C`'s own code;
# `*`/`&` may attach to the return type or straight to `C`'s name either way (`T *C::member()`,
# `T* C::member()`), and the return type may sit on the line above. `sizeof(name[0])`, `&name[i]`,
# `&name->x` and `&name.x` are allowed: they reach the field itself. `std::addressof(name)` is the same
# hazard as unary `&` and is caught too. A unary `&` is also recognised after a C-style pointer cast
# (`(void*)&name`, `(unsigned char*)&name`, `(struct Foo*)&name`) and after `&&` (`a && &name`), not only
# after the fixed set of operators it is otherwise unambiguous after. `name` passed bare as a whole
# argument to a known variadic logger (the printf family, `WWDEBUG_SAY`, `WWDEBUG_WARNING`, `WWDEBUG_ERROR`,
# `WWRELEASE_SAY`, `DEBUG_LOG`, `DEBUG_LOG_RAW`, `DEBUG_LOG_LEVEL`, `DEBUG_LOG_LEVEL_RAW`, `DEBUG_ASSERTLOG`,
# `DEBUG_CRASH`, `DEBUG_ASSERTCRASH`, `CRCDEBUG_LOG`) or to a `Format`/`format` call (`StringClass::Format`,
# `WideStringClass::Format`, `AsciiString::format`, `UnicodeString::format`, `Debug::Format`: every
# `Format`/`format` declared in the tree is a printf-style variadic, matched on the method name alone so a
# call through any instance is caught) is flagged too: it passes the empty stand-in object instead of the
# field's value. A two-macro-argument logger's condition/level argument (`DEBUG_ASSERTLOG`'s and
# `DEBUG_ASSERTCRASH`'s `c`, `DEBUG_LOG_LEVEL`'s and `DEBUG_LOG_LEVEL_RAW`'s `l`) is never flagged: it
# reaches the field through the stand-in's own conversion operator (`!(c)`, `l & DebugLevelMask`), not
# through `...`. Comments and string literals are ignored.
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
# A bare stand-in passed to one of these: a variadic call takes its argument's value by its declared type
# (`...`), with no user-defined conversion, so an empty stand-in object is passed, not the field's value.
VARIADIC_FUNCS = (
    "printf",
    "fprintf",
    "sprintf",
    "snprintf",
    "WWDEBUG_SAY",
    "WWDEBUG_WARNING",
    "WWDEBUG_ERROR",
    "WWRELEASE_SAY",
    "DEBUG_LOG",
    "DEBUG_LOG_RAW",
    "DEBUG_LOG_LEVEL",
    "DEBUG_LOG_LEVEL_RAW",
    "DEBUG_ASSERTLOG",
    "DEBUG_CRASH",
    "DEBUG_ASSERTCRASH",
    "CRCDEBUG_LOG",
    "Format",
    "format",
)
VARIADIC_CALL = re.compile(r"\b(?:" + "|".join(VARIADIC_FUNCS) + r")\s*\(")
# For each of these, the message argument (named by its 0-based index among the macro's own arguments,
# split on top-level commas) is itself written as a parenthesised list, e.g. `WWDEBUG_SAY(("fmt", x))`
# or `DEBUG_ASSERTLOG(c, ("fmt", x))` (Core/GameEngine/Include/Common/Debug.h) — a doubled-parens idiom
# that lets a comma inside survive the macro's own argument split. A macro not listed here (the printf
# family) takes its arguments directly, with no such wrapping, and every one of them is checked as is.
# Any other macro argument (e.g. DEBUG_ASSERTLOG's/DEBUG_ASSERTCRASH's condition, DEBUG_LOG_LEVEL's
# level) is never the message list and is left unchecked: it reaches the field through the stand-in's
# own conversion operator, not through `...`.
MESSAGE_ARG_INDEX = {
    "WWDEBUG_SAY": 0,
    "WWDEBUG_WARNING": 0,
    "WWDEBUG_ERROR": 0,
    "WWRELEASE_SAY": 0,
    "DEBUG_LOG": 0,
    "DEBUG_LOG_RAW": 0,
    "DEBUG_CRASH": 0,
    "CRCDEBUG_LOG": 0,
    "DEBUG_LOG_LEVEL": 1,
    "DEBUG_LOG_LEVEL_RAW": 1,
    "DEBUG_ASSERTLOG": 1,
    "DEBUG_ASSERTCRASH": 1,
}


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


def _looks_like_definition(text, close_idx):
    """Whether what follows the parameter list's close paren at close_idx is a definition's own shape:
    `{` or a constructor initialiser's `:`, optionally after whitespace and any of `const`/`noexcept`/
    `override`/`final` (in any order), rather than a declaration's or a call statement's `;`."""
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
    return i < n and text[i] in "{:"


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
        depth, i, end = 0, open_paren, None
        while i < len(text):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    end = i
                    break
            i += 1
        if end is None:
            continue
        inner = text[open_paren + 1 : end]
        func = cm.group(0).split("(", 1)[0].strip()
        base = open_paren + 1

        def split_top_level_commas(s):
            """[(start, end), ...] spans of s's top-level (depth-0) comma-separated pieces."""
            depth3, piece_start, spans = 0, 0, []
            for idx, ch in enumerate(s):
                if ch in "([{":
                    depth3 += 1
                elif ch in ")]}":
                    depth3 -= 1
                elif ch == "," and depth3 == 0:
                    spans.append((piece_start, idx))
                    piece_start = idx + 1
            spans.append((piece_start, len(s)))
            return spans

        macro_args = split_top_level_commas(inner)
        msg_index = MESSAGE_ARG_INDEX.get(func)
        if msg_index is None:
            # Not a doubled-parens macro (the printf family): every top-level argument is checked as is.
            check_spans = [(base + s0, base + s1) for s0, s1 in macro_args]
        elif msg_index < len(macro_args):
            # Only the message argument can hold a logged stand-in; the condition/level argument
            # (DEBUG_ASSERTLOG's/DEBUG_ASSERTCRASH's `c`, DEBUG_LOG_LEVEL's `l`) reaches the field through
            # the stand-in's own conversion operator and is never checked. The message argument is
            # itself written as a parenthesised list (doubled parens, so a variadic comma inside it does
            # not split the macro's own argument list): unwrap that one level before splitting on commas,
            # so the format string and each logged value are checked individually.
            m_start, m_end = macro_args[msg_index]
            msg_base = base + m_start
            msg_text = inner[m_start:m_end]
            lead = len(msg_text) - len(msg_text.lstrip())
            stripped_msg = msg_text.strip()
            if stripped_msg.startswith("(") and stripped_msg.endswith(")"):
                d, closes_at = 0, None
                for idx, ch in enumerate(stripped_msg):
                    if ch == "(":
                        d += 1
                    elif ch == ")":
                        d -= 1
                        if d == 0:
                            closes_at = idx
                            break
                if closes_at == len(stripped_msg) - 1:
                    msg_base += lead + 1
                    msg_text = stripped_msg[1:-1]
            check_spans = [(msg_base + s0, msg_base + s1) for s0, s1 in split_top_level_commas(msg_text)]
        else:
            check_spans = []  # fewer macro arguments than expected: malformed call, nothing to check
        # Flag a name only when it IS a whole top-level argument (not converted by a cast, a comparison,
        # or a call that takes it and returns something else): check each argument's stripped text
        # against the bare/qualified name, not just whether the name occurs inside it.
        for a_start, a_end in check_spans:
            arg_text = text[a_start:a_end]
            stripped = arg_text.strip()
            if stripped and re.fullmatch(ident, stripped):
                arg_lead = len(arg_text) - len(arg_text.lstrip())
                pos = a_start + arg_lead
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
    # or a constructor initialiser's `:`, not `;`. That is a definition's own shape; it is not a
    # declaration (`;`), a bare call statement (`C::member();`) or a call inside some other statement
    # (`return C::member();`, `case C::member():`), none of which make this `C`'s own file. The return
    # type, if any, is not inspected at all (and so needs no prefix pattern of its own): it may sit on
    # the same line, with `*`/`&` attached to either word (`T *C::member()`, `T* C::member()`), or on the
    # line above.
    owners = {owner for owner, _, _ in standins if owner}
    defines_member = {}
    if owners:
        member_def = re.compile(
            r"(?<![\w:])(?P<cls>" + "|".join(re.escape(o) for o in sorted(owners)) + r")"
            r"\s*::\s*(?P<member>~?[A-Za-z_]\w*)\s*\(",
        )
        for path, text in files.items():
            classes = set()
            for m in member_def.finditer(text):
                close = _matching_close_paren(text, m.end() - 1)
                if close is not None and _looks_like_definition(text, close):
                    classes.add(m.group("cls"))
            if classes:
                defines_member[path] = classes

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
