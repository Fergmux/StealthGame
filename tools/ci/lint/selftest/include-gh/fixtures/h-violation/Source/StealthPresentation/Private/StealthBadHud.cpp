#include "CoreMinimal.h"
void BadHud(UWorld* World)
{
	for (TActorIterator<APawn> It(World); It; ++It) {}
	for (auto Actor : TActorRange<AActor>(World)) {}
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Out);
	UGameplayStatics::GetAllActorsWithInterface(World, UInterface::StaticClass(), Out);
	for (auto It = World->GetPlayerControllerIterator(); It; ++It) {}
	World->GetGameMode();
	World->GetAuthGameMode();
	Pawn->GetPlayerState<APlayerState>();
	Pawn->GetController();
	Pawn->GetController(); // LOCAL-PAWN-OK:
	Pawn->GetController(); // LOCAL-PAWN-OK: local HUD owner pawn only
	Pawn->GetPlayerState(); /* LOCAL-PAWN-OK: the owning client's own state */
	World->GetGameMode(); // LOCAL-PAWN-OK: annotation does not cover other APIs
	// LOCAL-PAWN-OK:
}
