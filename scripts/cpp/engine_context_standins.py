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
# `name` unqualified in `c.h` and in the files named `c.cpp`/`c.inl` (any directory, any case), where `C`'s
# members are defined. `sizeof(name[0])`, `&name[i]`, `&name->x` and `&name.x` are allowed: they reach the
# field itself. `std::addressof(name)` is the same hazard as unary `&` and is caught too. A unary `&` is also
# recognised after a C-style pointer cast (`(void*)&name`, `(char*)&name`) and after `&&` (`a && &name`), not
# only after the fixed set of operators it is otherwise unambiguous after. `name` passed bare to a known
# variadic logger (the printf family, `WWDEBUG_SAY`, `DEBUG_LOG`, `DEBUG_ASSERTLOG`) is flagged too: it passes
# the empty stand-in object instead of the field's value. Comments and string literals are ignored.
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
VARIADIC_FUNCS = ("printf", "fprintf", "sprintf", "snprintf", "WWDEBUG_SAY", "DEBUG_LOG", "DEBUG_ASSERTLOG")
VARIADIC_CALL = re.compile(r"\b(?:" + "|".join(VARIADIC_FUNCS) + r")\s*\(")
# These take one macro argument that is itself written as a parenthesised list (`WWDEBUG_SAY(("fmt", x))`),
# a doubled-parens idiom that lets a comma inside survive the macro's own single-argument expansion.
DOUBLE_PAREN_FUNCS = ("WWDEBUG_SAY", "DEBUG_LOG", "DEBUG_ASSERTLOG")


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
        if func in DOUBLE_PAREN_FUNCS:
            # `WWDEBUG_SAY(("fmt", args...))`/`DEBUG_LOG(...)`/`DEBUG_ASSERTLOG(...)` take one macro
            # argument that is itself a parenthesised list (doubled parens, so a variadic comma inside
            # does not split the macro's own argument list). Unwrap that one level before splitting on
            # commas, so the format string and each logged value are checked individually.
            lead = len(inner) - len(inner.lstrip())
            trail = len(inner) - len(inner.rstrip())
            stripped_inner = inner.strip()
            if stripped_inner.startswith("(") and stripped_inner.endswith(")"):
                d, closes_at = 0, None
                for idx, ch in enumerate(stripped_inner):
                    if ch == "(":
                        d += 1
                    elif ch == ")":
                        d -= 1
                        if d == 0:
                            closes_at = idx
                            break
                if closes_at == len(stripped_inner) - 1:
                    base += lead + 1
                    inner = stripped_inner[1:-1]
        # Flag a name only when it IS a whole top-level argument (not converted by a cast, a comparison,
        # or a call that takes it and returns something else): split on top-level commas and check each
        # argument's stripped text against the bare/qualified name, not just whether the name occurs
        # inside it.
        depth2, arg_start, bounds = 0, 0, []
        for idx, ch in enumerate(inner):
            if ch in "([{":
                depth2 += 1
            elif ch in ")]}":
                depth2 -= 1
            elif ch == "," and depth2 == 0:
                bounds.append((arg_start, idx))
                arg_start = idx + 1
        bounds.append((arg_start, len(inner)))
        for a_start, a_end in bounds:
            arg_text = inner[a_start:a_end]
            stripped = arg_text.strip()
            if stripped and re.fullmatch(ident, stripped):
                arg_lead = len(arg_text) - len(arg_text.lstrip())
                pos = base + a_start + arg_lead
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

    by_stem = {}
    for path in raw:
        by_stem.setdefault(os.path.splitext(os.path.basename(path))[0].lower(), []).append(path)

    violations = []
    for owner, name, header in standins:
        stem = os.path.splitext(os.path.basename(header))[0].lower()
        own = {header} | {p for p in by_stem.get(stem, []) if not p.endswith((".h", ".hpp"))}
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
