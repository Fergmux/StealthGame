// Seeded violations for the forbidden-RNG rule (story st-012): every line 3-11 must be reported.
#include <random>
int32 A() { return FMath::Rand(); }
float B() { return FMath::FRand(); }
int32 C() { return FMath::RandRange(0, 9); }
void D() { FMath::RandInit(7); FMath::SRandInit(7); }
FRandomStream Stream(42);
int E() { return std::rand(); }
std::mt19937 Engine;
std::uniform_int_distribution<int> Dist(0, 3);
FVector F() { return FMath::VRand(); }
