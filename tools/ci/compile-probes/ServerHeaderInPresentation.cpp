// EXPECT-ERROR: "Server-only header included from StealthPresentation"
// TARGET-MODULE: StealthPresentation
// CONTROL-MODULE: StealthGame
//
// Compile-fail probe (ADR-0008 section 7, ADR-0007 section 3; story tdc-005).
// A StealthPresentation file that includes a Public/Server/ header must fail to compile through
// StealthServerOnlyGuard.h with the #error text above. Positive control: the same file compiled
// from StealthGame (no CONTROL-FILE header, so the control body is this file) must compile.
//
// Header contract (read by tools/ci/compile-probes/run-probes.ps1):
//   EXPECT-ERROR   required  substring the failing compile output must contain
//   TARGET-MODULE  required  module whose Private/CompileProbes/ receives the probe
//   CONTROL-MODULE optional  module for the positive control (default StealthGame)
//   CONTROL-FILE   optional  control body, relative to tools/ci/compile-probes/ (default: this file)
// Never part of a normal build: the runner copies it into a disposable worktree only.

#include "Server/StealthGuardProbeTarget.h"

static_assert(StealthGuardProbe::ServerOnlyMarker == 1, "probe target header was reached");
