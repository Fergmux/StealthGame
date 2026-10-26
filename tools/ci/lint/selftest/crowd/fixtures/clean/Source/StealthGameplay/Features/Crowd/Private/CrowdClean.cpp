// MoveToLocation, AddMovementInput and Attack( in a comment are not code; neither is "SetFocus(" in a string.
#include "Server/CrowdSink.h"
void A(FCrowdCommandSink& Sink) { Sink.Move(FVector2D(0.0, 1.0), EGait::Walk); const TCHAR* Text = TEXT("C->Attack(x)"); }
void B(AAIController* C) { C->OnPossess(nullptr); RemoveToken(); }
struct FNeighbour { FVector VelocityCmPerSec; }; void C(FNeighbour& N) { N.VelocityCmPerSec = FVector::ZeroVector; }
