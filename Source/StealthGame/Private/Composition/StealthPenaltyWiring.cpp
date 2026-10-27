#include "StealthPenaltyWiring.h"

#include "GameFramework/Controller.h"
#include "Server/Core/StealthPawnFactory.h"
#include "StealthCharacter.h"
#include "StealthGameMode.h"

FStealthPenaltyPawnGateway::FStealthPenaltyPawnGateway(AStealthGameMode* InGameMode)
	: GameMode(InGameMode)
{
}

void FStealthPenaltyPawnGateway::ReleaseCard(APawn* Pawn)
{
	// STAND-IN (Character Pool & Appearance not built): no card ledger exists and the pawn keeps its look.
}

void FStealthPenaltyPawnGateway::RequestHandOverToAi(APawn* Pawn, uint32 ExpectedRoundEpoch, double PossessNotBeforeServerTime)
{
	if (Pawn == nullptr)
	{
		return;
	}
	AController* Former = Pawn->GetController();
	if (UStealthPawnFactory* Factory = UStealthPawnFactory::Get(Pawn->GetWorld()))
	{
		Factory->RequestHandOver(Pawn, EStealthPawnControlKind::Ai, PossessNotBeforeServerTime);
	}
	if (AStealthGameMode* Mode = GameMode.Get())
	{
		Mode->HandleProductionExile(Cast<AStealthCharacter>(Pawn), Former);
	}
}
