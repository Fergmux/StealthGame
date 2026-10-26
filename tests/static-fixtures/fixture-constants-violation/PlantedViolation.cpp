// PLANTED VIOLATION (story rmf-005 negative control). Data, never compiled: it sits outside Source/, so UBT does
// not build it and outside both scanned globs, so StealthGame.Round.FixtureConstants does not scan it except as
// its negative control, which must report each category below. Do not "fix" this file.

const TCHAR* const InlinedHeaderField = TEXT("TuningHash");      // a RoundLogHeaderFields name inlined
const TCHAR* const InlinedCode = TEXT("PreparingWatchdog");       // a fixture code string inlined
const uint64 InlinedSecretWord = 0x7b2e4f9a1c6d3e85ull;           // a RoundTestSeeds secret word inlined (lower case)
