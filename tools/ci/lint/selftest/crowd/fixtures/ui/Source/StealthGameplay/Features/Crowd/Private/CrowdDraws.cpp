#include "DrawDebugHelpers.h"
#include "Blueprint/UserWidget.h"
void A(UWorld* W) { DrawDebugSphere(W, FVector::ZeroVector, 50.f, 8, FColor::Red); }
void B() { GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Red, TEXT("crowd")); }
