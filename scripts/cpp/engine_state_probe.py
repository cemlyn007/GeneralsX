#!/usr/bin/env python3
# GeneralsX @feature cemlyn007 30/09/2026 A runtime write probe of libgeneralsx.so's static state (PLAN-023 Phase 8, stage RR1)
#
# engine_state_symbols.py classifies every writable symbol of the library by reading the code; this checks the
# classification at run time. Inside a process that has the library loaded (an embedding host, such as
# rlgenerals' Python bindings), it snapshots the bytes of every .data and .bss symbol, lets the host run what it
# wants to watch (a rendering engine's steps, say), snapshots again and reports every symbol whose bytes changed,
# with its class from the checked-in list (docs/WORKDIR/planning/PLAN-023_STATE_CLASSIFICATION.tsv).
#
# A written symbol is judged by name, never by its class alone:
#
#   worklist     classified render-per-engine or render-scratch: state known to change as an
#                engine draws, which PLAN-023 Phase 8 moves into the engine (the row's phase says when);
#   allowed      named in the explicit allowlist (ALLOWLIST below, or the caller's), each with its reason: a
#                process-wide counter, pool or cache that is written on purpose and safe to share;
#   unexpected   anything else: a symbol the list calls constant, render-const, render-process, debug-only,
#                process-global or render-only (UI) that the engine wrote all the same. Either the list is
#                wrong (classify it again) or the write is a bug.
#
# It cannot see a static that is written and restored within the watched span (the same bytes at both
# snapshots), nor thread-local state (.tbss/.tdata, not probed): ThreadSanitizer covers those.
#
# Usage, from the host process (Python):
#
#   import engine_state_probe
#   probe = engine_state_probe.Probe(root="GeneralsX")      # finds the loaded libgeneralsx.so
#   snapshots = [("start", probe.snapshot())]
#   ...  # boot an engine
#   snapshots.append(("boot", probe.snapshot()))
#   ...  # step it
#   snapshots.append(("steps", probe.snapshot()))
#   report = probe.report(snapshots)                          # each span: what changed since the last one
#   print(report.text()); assert not report.unexpected
#
# Take the first snapshot after the process's first boot (a priming engine): what that boot writes once for the
# process is by design, and would otherwise all need listing.
#
# Needs binutils (nm, c++filt) and Linux (/proc/self/maps).

import collections
import ctypes
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import engine_state_symbols as ess  # noqa: E402

LIBRARY = "libgeneralsx.so"
PROBED_SECTIONS = {".data", ".bss"}
WORKLIST_CLASSES = {"render-per-engine", "render-scratch"}
GUARD = "guard variable for "

# Written on purpose, and safe to share between engines: each by exact key (the TSV's symbol column), with its
# reason. A class is never enough: a process-global symbol that a render step writes is listed here once
# someone has checked that the write is by design.
ALLOWLIST = {
    # Written once per process, by the first render boot (the probe's first snapshot follows a headless
    # priming boot, so the render engine's is the process's first device).
    "D3D8Lib": "the D3D8 library, loaded per process under a mutex and never freed",
    "Direct3DCreate8Ptr": "the D3D8 library's Direct3DCreate8, looked up once per process with it",
    "DX8Wrapper::Init(void*, bool)::s_d3d8LibMutex": "the mutex that guards the load of the D3D8 library",
    "DX8Wrapper_FinalReleaseHook": "the host's final-release check, set by its first render boot",
    "RTS3DScene::updateFixedLightEnvironments(RenderInfoClass&)::id": "a constant vector, built at first use under the static-init guard",
    "W3DVolumetricShadow::Update()::originCompareVector": "a constant vector, built at first use under the static-init guard",
    "Drawable::drawBombed(IRegion2D const*)::key_StickyBombUpdate": "a NameKey cache (PLAN-023 Decision 2), set at first use (a draw path) under the static-init guard",
    "FiringTracker::getModuleNameKey() const::nk": "a NameKey cache (PLAN-023 Decision 2), set at first use under the static-init guard",
    "ObjectWeaponStatusHelper::getModuleNameKey() const::nk": "a NameKey cache (PLAN-023 Decision 2), set at first use under the static-init guard",
    "TempWeaponBonusHelper::getModuleNameKey() const::nk": "a NameKey cache (PLAN-023 Decision 2), set at first use under the static-init guard",
    # Process-wide by design, written by every engine.
    "WideStringClass::m_TempStrings": "WWLib string temp-buffer pool, under its own mutex",
    "StringClass::m_TempStrings": "WWLib string temp-buffer pool, under its own mutex",
    "AssetStatusClass::Instance": "the process's missing-asset report, under its lock",
    "AutoPoolClass<PolyRenderTaskClass, 256>::Allocator()::allocator": "WWLib's object pool for the mesh renderer's tasks, process-wide like malloc (made at its first use)",
    "DX8Wrapper_IsWindowed": "the process's assert switch, a std::atomic every boot's command line writes",
    "LookupTableMgrClass::Tables": "WWMath's lookup tables, made by the first counted WWMath::Init and freed by the last Shutdown",
    "WWMathInitCount": "the process-wide WWMath::Init/Shutdown count, under its mutex",
    "theMemoryManagerUsers": "the refcounted memory manager's user count, under its mutex",
    "s_totalOpen": "a std::atomic open-file counter for a debug assert",
    "std::__detail::__waiter_pool_base::_S_for(void const*)::__w": "libstdc++'s wait pool for std::atomic::wait, process-wide by design",
    "fcrandbuf": "fontconfig's random state (third-party; its cache file names): whether it is safe with renderers on several threads is ThreadSanitizer's to find (RR4)",
    "statebuf": "fontconfig's random state (third-party; its cache file names): whether it is safe with renderers on several threads is ThreadSanitizer's to find (RR4)",
}


class Probe:
    """Snapshots of the .data/.bss symbols of the library loaded in this process."""

    def __init__(self, root=".", lib_path=None, allowlist=None):
        self.root = root
        self.lib_path, self.base = self._find_library(lib_path)
        self.allowlist = dict(ALLOWLIST)
        if allowlist:
            self.allowlist.update(allowlist)
        self.symbols = self._read_symbols(self.lib_path)
        self.rows = ess.read_tsv(os.path.join(root, ess.TSV))

    @staticmethod
    def _find_library(lib_path):
        """The loaded library's path and load base (its first mapping, file offset 0)."""
        want = os.path.realpath(lib_path) if lib_path else None
        with open("/proc/self/maps") as f:
            for line in f:
                fields = line.split()
                if len(fields) < 6:
                    continue
                path = fields[5]
                if (want and os.path.realpath(path) == want) or (not want and os.path.basename(path) == LIBRARY):
                    if int(fields[2], 16) == 0:
                        return path, int(fields[0].split("-")[0], 16)
        raise RuntimeError(f"{lib_path or LIBRARY} is not loaded in this process")

    @staticmethod
    def _read_symbols(path):
        """key -> [(address offset, size)] for every sized .data/.bss symbol."""
        rows = []
        for line in ess.run(["nm", "-f", "sysv", path]).splitlines():
            fields = [f.strip() for f in line.split("|")]
            if len(fields) < 7 or not fields[0] or fields[2] in ("U", "w", "v"):
                continue
            if fields[6] not in PROBED_SECTIONS or not fields[1] or not fields[4]:
                continue
            size = int(fields[4], 16)
            if size:
                rows.append((fields[0], int(fields[1], 16), size))
        names = ess.demangle([r[0] for r in rows])
        symbols = collections.defaultdict(list)
        for (_mangled, value, size), name in zip(rows, names):
            symbols[ess.SUFFIX_RE.sub("", name)].append((value, size))
        return dict(symbols)

    def snapshot(self):
        """key -> the bytes of every instance, now."""
        return {
            key: b"".join(ctypes.string_at(self.base + value, size) for value, size in spans)
            for key, spans in self.symbols.items()
        }

    @staticmethod
    def diff(before, after):
        """The keys whose bytes changed between two snapshots."""
        return sorted(k for k, v in after.items() if before.get(k) != v)

    def report(self, snapshots):
        """The report over a sequence of (span name, snapshot) pairs: each span is from the snapshot before
        it to its own, and a symbol is listed with every span that changed it."""
        spans = collections.defaultdict(list)
        for (_, before), (name, after) in zip(snapshots, snapshots[1:]):
            for key in self.diff(before, after):
                spans[key].append(name)
        return Report(self, spans)


class Report:
    """What a probe found written, judged by name (see the top of the file)."""

    def __init__(self, probe, spans):
        self.rows = []
        for key in sorted(spans):
            row = probe.rows.get(key)
            cls = row["class"] if row else "(not in the list)"
            phase = row.get("phase", "") if row else ""
            verdict, why = self._judge(probe, key, cls, phase)
            # A guard variable is written once, when its static is first initialised: it is judged as its
            # static is (by the static's class and name, never by the guard's own).
            if key.startswith(GUARD) and verdict == "unexpected":
                static = key[len(GUARD) :]
                static_row = probe.rows.get(static)
                static_verdict, static_why = self._judge(
                    probe,
                    static,
                    static_row["class"] if static_row else "(not in the list)",
                    static_row.get("phase", "") if static_row else "",
                )
                if static_verdict != "unexpected":
                    verdict, why = static_verdict, f"the guard of {static}" + (f": {static_why}" if static_why else "")
            self.rows.append((key, cls, phase, verdict, ",".join(spans[key]), why))
        self.unexpected = [r[0] for r in self.rows if r[3] == "unexpected"]

    @staticmethod
    def _judge(probe, key, cls, phase=""):
        if cls in WORKLIST_CLASSES:
            return "worklist", ""
        # A render-const row with a phase says so itself: every render boot still rewrites it (with the same
        # value) until that stage builds it once.
        if cls == "render-const" and phase:
            return "worklist", f"rewritten at every render boot until {phase}"
        if key in probe.allowlist:
            return "allowed", probe.allowlist[key]
        return "unexpected", ""

    def written(self, key):
        """The spans that changed `key` ("" if none)."""
        return next((r[4] for r in self.rows if r[0] == key), "")

    def text(self):
        out = []
        counts = collections.Counter(r[3] for r in self.rows)
        out.append(
            f"{len(self.rows)} symbols written: "
            + ", ".join(f"{v} {counts.get(v, 0)}" for v in ("worklist", "allowed", "unexpected"))
        )
        by_class = collections.Counter(r[1] for r in self.rows)
        out.append("by class: " + ", ".join(f"{c} {n}" for c, n in sorted(by_class.items())))
        for verdict in ("unexpected", "allowed", "worklist"):
            group = [r for r in self.rows if r[3] == verdict]
            if not group:
                continue
            out.append(f"\n== {verdict}: {len(group)}")
            for key, cls, phase, _v, spans, why in group:
                out.append(
                    f"  {key}  [{cls}{' ' + phase if phase else ''}] written in {spans}"
                    + (f"  ({why})" if why else "")
                )
        return "\n".join(out)
