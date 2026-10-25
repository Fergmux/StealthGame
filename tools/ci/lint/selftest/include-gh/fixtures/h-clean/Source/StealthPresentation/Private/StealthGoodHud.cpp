#include "CoreMinimal.h"
// TActorIterator and GetAllActorsOfClass named in a comment are not code.
/* GetGameMode(), GetAuthGameMode(), Pawn->GetController() in a block comment */
void GoodHud(APawn* LocalPawn)
{
	const TCHAR* Text = TEXT("GetPlayerState GetController TActorRange in a string");
	LocalPawn->GetController(); // LOCAL-PAWN-OK: the HUD's own owner pawn
	LocalPawn->GetPlayerState<APlayerState>(); /* LOCAL-PAWN-OK: local pawn state for the nameplate */
	int32 GetControllerCount = 0;
}
