#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 30/09/2026 The sizeof/& gate for EngineContext stand-ins (PLAN-023 Phase 8, stage RR2a-1)
#
# With RTS_ENGINE_CONTEXT, a class's static data member can become a stand-in for a per-engine field:
# `static constexpr rts::ContextField<T, &rts::EngineContext::field> name{};`, its indirect variant,
# `rts::IndirectContextField<...>`, or its accessor variant, `rts::AccessorContextField<...>` (RR2a-2: WW3D's names;
# Core/GameEngine/Include/Common/EngineContext.h). A stand-in is an empty
# object that converts to the field, so reads, assignments and comparisons compile unchanged, and so do
# `sizeof(name)` and `&name`, which then give the stand-in's size and address, not the field's. This fails on
# either use of any stand-in, so a name used that way is caught at review time (it needs a reference-returning
# macro instead, as DX8Wrapper's names have: WW3D2/w3drenderstate_names.h).
#
# Where it looks, for a stand-in `name` of class `C` declared in `c.h`: `C::name` in every source file, and
# `name` unqualified in `c.h` and in the files named `c.cpp`/`c.inl` (any directory, any case), where `C`'s
# members are defined. `sizeof(name[0])`, `&name[i]`, `&name->x` and `&name.x` are allowed: they reach the
# field itself. Comments and string literals are ignored.
#
# Usage: engine_context_standins.py [--root GeneralsX] [--list]
#   exit 0 when no stand-in is used with sizeof or &, 1 (with each use listed) otherwise; --list prints the
#   stand-ins found.
import argparse
import os
import re
import sys

SOURCE_DIRS = ("Core", "Generals", "GeneralsMD")
SOURCE_EXTENSIONS = (".h", ".hpp", ".inl", ".cpp", ".cc")

STANDIN = re.compile(
    r"static\s+constexpr\s+(?:::)?rts::(?:Indirect|Accessor)?ContextField\s*<(?P<args>.*?)>\s*(?P<name>\w+)\s*\{\s*\}\s*;",
    re.S,
)
CLASS = re.compile(r"^\s*(?:class|struct)\s+(?:\w+\s+)*?(?P<name>\w+)\s*(?::[^;{]*)?\{", re.M)
# After the name: something that reaches the field itself, not the stand-in.
REACHES_FIELD = re.compile(r"\s*(?:\[|\.|->|\()")
# Before a unary `&`: an operator, an opening bracket, a separator or `return`.
UNARY_BEFORE = re.compile(r"(?:^|[=(,{};?:!<>+\-*/%|^~\[]|\breturn)\s*$")


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


def source_files(root):
    for top in SOURCE_DIRS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(root, top)):
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
    """(line, what) of each `sizeof` or unary `&` of the stand-in in the stripped text."""
    ident = (re.escape(qualifier) + r"\s*::\s*" if qualifier else r"(?<![\w:.>])") + re.escape(name) + r"\b"
    bad = []
    for m in re.finditer(r"\bsizeof\s*(?:\(\s*)?" + ident, text):
        if not REACHES_FIELD.match(text, m.end()):
            bad.append((text.count("\n", 0, m.start()) + 1, "sizeof"))
    for m in re.finditer(r"&\s*" + ident, text):
        before = text[max(0, m.start() - 40):m.start()]
        if before.endswith("&") or not UNARY_BEFORE.search(before):
            continue  # `&&` or a binary `&`
        if not REACHES_FIELD.match(text, m.end()):
            bad.append((text.count("\n", 0, m.start()) + 1, "&"))
    return bad


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0] if __doc__ else None)
    parser.add_argument("--root", default=".", help="the GeneralsX checkout (default: the current directory)")
    parser.add_argument("--list", action="store_true", help="print the stand-ins found")
    args = parser.parse_args()

    raw = {}
    for path in source_files(args.root):
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
            f"{len(violations)} use(s) of an EngineContext stand-in with sizeof or &: they give the stand-in's "
            "size or address, not the field's; use a reference-returning macro for that name instead "
            "(see EngineContext.h, ContextField)",
            file=sys.stderr,
        )
        return 1
    print(f"ok: {len(standins)} stand-ins, none used with sizeof or &")
    return 0


if __name__ == "__main__":
    sys.exit(main())
