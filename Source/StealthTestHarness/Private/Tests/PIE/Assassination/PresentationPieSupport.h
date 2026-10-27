#pragma once

// Shared helpers of the Assassination presentation PIE tests (stories asn-018, asn-020, asn-021): a pawn's copy on another
// machine, each machine's presentation and view subsystems, and the one-time fixture settings of the 3-player session.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Engine/LocalPlayer.h"
#include "Tests/PIE/Assassination/AssassinationPieSupport.h"
#include "View/EncounterPresentation.h"
#include "View/EncounterViewSubsystem.h"

namespace StealthPresentationPie
{
	/** The copy of ServerPawn in World (the nearest Shared Pawn within 60 cm; ServerPawn itself on the server). */
	inline AStealthPawn* CopyOf(const AStealthPawn* ServerPawn, UWorld* World)
	{
		if (ServerPawn == nullptr || World == nullptr)
		{
			return nullptr;
		}
		if (ServerPawn->GetWorld() == World)
		{
			return const_cast<AStealthPawn*>(ServerPawn);
		}
		AStealthPawn* Best = nullptr;
		double BestDistance = 60.0;
		for (TActorIterator<AStealthPawn> It(World); It; ++It)
		{
			const double Distance = FVector::Dist(It->GetActorLocation(), ServerPawn->GetActorLocation());
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = *It;
			}
		}
		return Best;
	}

	/** The machine index (into StealthAssassinationPie::Worlds()) whose local player controls ServerController. */
	inline int32 MachineOf(const AStealthPlayerController* ServerController)
	{
		const AStealthPlayerController* Local = StealthAssassinationPie::OwnerMachineController(ServerController);
		const TArray<UWorld*> All = StealthAssassinationPie::Worlds();
		return Local != nullptr ? All.IndexOfByKey(Local->GetWorld()) : INDEX_NONE;
	}

	inline UEncounterPresentationSubsystem* PresentationOf(UWorld* World)
	{
		return UEncounterPresentationSubsystem::Get(World);
	}

	/** The local player's view subsystem of a machine (its first local player controller). */
	inline UEncounterViewSubsystem* ViewOf(UWorld* World)
	{
		APlayerController* Local = World != nullptr ? World->GetFirstPlayerController() : nullptr;
		ULocalPlayer* Player = Local != nullptr ? Local->GetLocalPlayer() : nullptr;
		return Player != nullptr ? Player->GetSubsystem<UEncounterViewSubsystem>() : nullptr;
	}

	/** The first presentation log row of Pawn's copy on World for Event and Clip (null when none). */
	inline const FEncounterPresentationLogRow* FirstRow(UWorld* World, const AStealthPawn* Copy, EEncounterPresentationEvent Event, EEncounterClip Clip = EEncounterClip::None)
	{
		const UEncounterPresentationSubsystem* Presentation = PresentationOf(World);
		if (Presentation == nullptr)
		{
			return nullptr;
		}
		for (const FEncounterPresentationLogRow& Row : Presentation->GetLog())
		{
			if (Row.PawnKey == Copy && Row.Event == Event && (Clip == EEncounterClip::None || Row.Clip == Clip))
			{
				return &Row;
			}
		}
		return nullptr;
	}
}

#endif
