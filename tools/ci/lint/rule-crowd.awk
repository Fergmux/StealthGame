# Crowd rules (ADR-0016 section 2 and Implementation Guidelines, ADR-0001 section 2; stories cnb-005, cnb-008, cnb-013),
# run through cpplex.awk over every source file under Source/StealthGameplay/Features/Crowd/. Code only (comments and
# string literal content are not code). Rule ids:
#   crowd-api      a pawn reached other than through the sinks' UPawnCommandComponent entry points: MoveTo* (MoveToLocation,
#                  MoveToActor, ...), AITask_MoveTo, RequestDirectMove, RequestPathMove, AddMovementInput,
#                  SetActorRotation, LaunchCharacter, a CMC Velocity write, SetFocus / SetFocalPoint / FaceRotation,
#                  UAIPerceptionComponent, Possess / UnPossess, an Attack call, and SetControlRotation outside the
#                  crowd controller's single UpdateControlRotation override (StealthCrowdController.h/.cpp)
#   crowd-identity a name of a player/team/kind query: IsPlayer, IsPlayerController, GetPlayerState, GetTeam*, VictimKind,
#                  FSiteAssignment (the static half of AC-CNB-01's "0 calls"; the site-assignment sentinel is the runtime half)
#   crowd-include  an include of a Roster, Objectives-assignment or Visibility Policy fact header, or the engine
#                  perception header (identity blindness, CNB-R2, AC-CNB-01)
function crowd_find(path, ln, rule, msg) { lint_find(path, ln, rule, msg) }
function rule_file(path,   rel, ln, code, raw, rest, m, inc, ctrl) {
  rel = path
  if (index(rel, root "/") == 1) rel = substr(rel, length(root) + 2)
  ctrl = (rel ~ /StealthCrowdController\.(cpp|h)$/)
  for (ln = 1; ln <= lex_n; ln++) {
    code = lex_text_line(CODE, ln)
    raw = lex_raw[ln]
    if (match(raw, /^[ \t]*#[ \t]*include[ \t]*["<][^">]*[">]/)) {
      inc = substr(raw, RSTART, RLENGTH); sub(/^[ \t]*#[ \t]*include[ \t]*["<]/, "", inc); sub(/[">]$/, "", inc)
      if (inc ~ /(^|\/)Server\/Roster\// || inc ~ /StealthRoster(Store|Core|Deps|CallTypes|TeamView)\.h$/ || inc ~ /Roster(Gateway)?\.h$/)
        crowd_find(path, ln, "crowd-include", "includes the Roster header " inc " (identity blindness, CNB-R2)")
      if (inc ~ /Objective/ || inc ~ /SiteAssignment/)
        crowd_find(path, ln, "crowd-include", "includes the Objectives header " inc " (identity blindness, CNB-R2)")
      if (inc ~ /Visibility/ || inc ~ /PolicyFact/ || inc ~ /StealthPolicy/)
        crowd_find(path, ln, "crowd-include", "includes the Visibility Policy header " inc " (identity blindness, CNB-R2)")
      if (inc ~ /AIPerception/)
        crowd_find(path, ln, "crowd-include", "includes the engine perception header " inc " (ADR-0016 section 5)")
      continue
    }
    rest = code
    while (match(rest, /(^|[^A-Za-z0-9_])(MoveTo[A-Za-z0-9_]*|AITask_MoveTo|RequestDirectMove|RequestPathMove|AddMovementInput|SetActorRotation|LaunchCharacter|SetFocus|SetFocalPoint|FaceRotation|UAIPerceptionComponent|UnPossess|Possess)[ \t]*[(<:]/)) {
      m = substr(rest, RSTART, RLENGTH); sub(/^[^A-Za-z_]/, "", m); sub(/[ \t]*[(<:]$/, "", m)
      crowd_find(path, ln, "crowd-api", "uses " m " (crowd code reaches a pawn only through the sinks, ADR-0016 section 2)")
      rest = substr(rest, RSTART + RLENGTH)
    }
    rest = code
    while (match(rest, /(^|[^A-Za-z0-9_])(IsPlayer|IsPlayerController|IsLocalPlayerController|GetPlayerState|GetTeam|GetTeamId|GetGenericTeamId|GetTeamAttitudeTowards|VictimKind|FSiteAssignment|SiteAssignment)([^A-Za-z0-9_]|$)/)) {
      m = substr(rest, RSTART, RLENGTH); gsub(/[^A-Za-z0-9_]/, "", m)
      crowd_find(path, ln, "crowd-identity", "names " m " (no crowd code reads player, binding, team, victim kind or site assignment; CNB-R2, AC-CNB-01)")
      rest = substr(rest, RSTART + RLENGTH)
    }
    rest = code
    while (match(rest, /(->|\.)[ \t]*Attack[ \t]*\(/)) {
      crowd_find(path, ln, "crowd-api", "calls Attack (the NPC sink has no Attack, AC-CNB-11)")
      rest = substr(rest, RSTART + RLENGTH)
    }
    rest = code
    while (match(rest, /(->|\.)[ \t]*Velocity[ \t]*(=[^=]|\+=|-=)/)) {
      crowd_find(path, ln, "crowd-api", "writes a movement Velocity (ADR-0016 section 2)")
      rest = substr(rest, RSTART + RLENGTH)
    }
    rest = code
    if (!ctrl) {
      while (match(rest, /(^|[^A-Za-z0-9_])SetControlRotation[ \t]*\(/)) {
        crowd_find(path, ln, "crowd-api", "calls SetControlRotation outside the crowd controller's UpdateControlRotation override")
        rest = substr(rest, RSTART + RLENGTH)
      }
    }
  }
}
