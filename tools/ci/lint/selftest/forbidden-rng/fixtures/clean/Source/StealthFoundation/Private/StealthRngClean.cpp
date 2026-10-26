// Clean control (story st-012): names in comments and strings are not code: FMath::Rand FRandomStream std::rand <random>
#include "View/Random/StealthIntStream.h"
const TCHAR* Text = TEXT("FMath::FRand() and FRandomStream are forbidden");
int32 MyRandomStreamCount = 0;      // a longer identifier containing FRandomStream is not FRandomStream
int32 Operand(FIntStream& S) { return static_cast<int32>(S.NextU32()); }
struct FRandomStreamLike {};        // not the engine type
