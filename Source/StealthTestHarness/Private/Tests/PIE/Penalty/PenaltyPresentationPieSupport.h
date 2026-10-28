#pragma once

// Shared helpers of the NPC-Kill Penalty owner-presentation PIE tests (stories nkp-016, nkp-017, nkp-018, nkp-021): the
// exile driver (L NPC kills by one player through the production strike path), the owner machine's view and fade sample,
// and the owner presentation log of a server controller's machine.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Camera/PlayerCameraManager.h"
#include "Server/StealthServerTuning.h"
#include "Server/StealthPenaltyService.h"
#include "Tests/PIE/Assassination/OwnPresentationPieSupport.h"
#include "Tests/PIE/Penalty/PenaltyPieSupport.h"
#include "View/EncounterPresentation.h"
#include "View/OwnResultPlateSettings.h"
#include "View/PenaltyHudViewModel.h"

namespace StealthPenaltyPresentationPie
{
	/** Past AttackAttemptLockoutSec (1.75 s) and the contact: the next strike of the same attacker is legal. */
	inline constexpr double StrikeStepSec = 2.4;

	/** The latched L of the schema defaults (the PIE rounds run on the schema defaults). */
	inline int32 Limit()
	{
		return FPenaltyParams::FromTuning(FStealthServerTuning()).NpcKillStrikeLimit;
	}

	/** The pair player whose machine is a client (the host has no wire: lateness and drops need a remote owner). */
	inline AStealthPlayerController* ClientPairPlayer(AStealthPlayerController* const Pair[2])
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const AStealthPlayerController* Machine = StealthAssassinationPie::OwnerMachineController(Pair[Index]);
			if (Pair[Index] != nullptr && Machine != nullptr && Machine->GetWorld() != nullptr && Machine->GetWorld()->GetNetMode() == NM_Client)
			{
				return Pair[Index];
			}
		}
		return nullptr;
	}

	/** One NPC kill by Striker: an unbound Shared Pawn spawned 150 cm in front of it and struck (the production path). */
	inline AStealthPawn* NpcKill(AStealthPawn* Striker, uint32& Seq)
	{
		AStealthPawn* Npc = StealthAssassinationPie::SpawnUnboundPawn(StealthAssassinationPie::ServerWorld(), Striker, 150.0);
		StealthAssassinationPie::StrikeAt(Striker, Npc, Seq);
		return Npc;
	}

	/** The owner machine's view target and fade (the camera manager of the machine's local controller). */
	struct FViewSample
	{
		const AActor* ViewTarget = nullptr;
		float FadeAmount = 0.f;
		bool bFading = false;
		const APawn* Possessed = nullptr;
	};

	inline FViewSample SampleView(const AStealthPlayerController* Machine)
	{
		FViewSample Out;
		if (Machine == nullptr)
		{
			return Out;
		}
		Out.Possessed = Machine->GetPawn();
		if (const APlayerCameraManager* Camera = Machine->PlayerCameraManager)
		{
			Out.ViewTarget = Camera->GetViewTarget();
			Out.bFading = Camera->bEnableFading;
			Out.FadeAmount = Camera->FadeAmount;
		}
		return Out;
	}

	/** A frame is black when the camera fade covers it (the owner's screen shows nothing of the world). */
	inline bool IsBlack(const FViewSample& Sample)
	{
		return Sample.bFading && Sample.FadeAmount >= 0.99f;
	}

	/** Machine's owner presentation records of Event from index From (a GetOwnPresentationLog().Num() snapshot). */
	inline TArray<FOwnPresentationRecord> RecordsOf(const AStealthPlayerController* Machine, EOwnPresentationEvent Event, int32 From = 0)
	{
		TArray<FOwnPresentationRecord> Out;
		if (Machine != nullptr)
		{
			const TArray<FOwnPresentationRecord>& Log = Machine->GetOwnPresentationLog();
			for (int32 Index = FMath::Max(0, From); Index < Log.Num(); ++Index)
			{
				if (Log[Index].Event == Event)
				{
					Out.Add(Log[Index]);
				}
			}
		}
		return Out;
	}

	/** EstimatedServerNow() on Machine. */
	inline double MachineNow(const AStealthPlayerController* Machine)
	{
		return Machine != nullptr ? UEncounterPresentationSubsystem::EstimatedServerNow(Machine->GetWorld()) : 0.0;
	}

	/** The machine's longest frame so far plus slack (the +- 1 frame bound), from the machine's own delta time. */
	struct FMachineFrame
	{
		double Longest = 0.0;
		void Sample(const AStealthPlayerController* Machine)
		{
			if (Machine != nullptr && Machine->GetWorld() != nullptr)
			{
				Longest = FMath::Max(Longest, static_cast<double>(Machine->GetWorld()->GetDeltaSeconds()));
			}
		}
		double Frame() const { return FMath::Max(1.0 / 60.0, Longest) + 0.003; }
	};
}

#endif
