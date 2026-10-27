#pragma once

// Shared helpers of the Assassination PIE tests of stories asn-015, asn-016 and asn-019 (listen server + 2 clients on
// L_Prototype): the machines' worlds, server time, the round's phase, the three players and their machines, pawn
// placement clear of the prototype bushes, and a server-side command submitted with a player's OWN origin (its mailbox,
// reply sink and control epoch, as stage-1 ingress opens it for that client), so owner replies reach that connection.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST && WITH_STEALTH_TEST_SEAMS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "Server/StealthCompositionRoot.h"
#include "StealthBush.h"
#include "StealthGameState.h"
#include "StealthPlayerController.h"
#include "StealthPlayerState.h"
#include "View/Core/StealthPawn.h"

namespace StealthAssassinationPie
{
	inline const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	inline const TCHAR* const NetworkTimeoutCeilingSeconds = TEXT("300");

	/** Every PIE world, the server first, then the clients by PIE instance. */
	inline TArray<UWorld*> Worlds()
	{
		TArray<const FWorldContext*> Contexts;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				Contexts.Add(&Context);
			}
		}
		Contexts.Sort([](const FWorldContext& A, const FWorldContext& B)
		{
			const bool bServerA = A.World()->GetNetMode() != NM_Client;
			const bool bServerB = B.World()->GetNetMode() != NM_Client;
			return bServerA != bServerB ? bServerA : A.PIEInstance < B.PIEInstance;
		});
		TArray<UWorld*> Out;
		for (const FWorldContext* Context : Contexts)
		{
			Out.Add(Context->World());
		}
		return Out;
	}

	inline UWorld* ServerWorld()
	{
		const TArray<UWorld*> All = Worlds();
		return All.Num() > 0 && All[0]->GetNetMode() == NM_ListenServer ? All[0] : nullptr;
	}

	inline double ServerNow(const UWorld* World)
	{
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		return State != nullptr ? State->GetServerWorldTimeSeconds() : 0.0;
	}

	inline bool RoundActive()
	{
		const AStealthGameState* State = ServerWorld() != nullptr ? ServerWorld()->GetGameState<AStealthGameState>() : nullptr;
		return State != nullptr && State->GetRoundPublicState().Phase == ERoundPhase::Active;
	}

	/** The server's three player controllers with pawns (empty until all three are up). */
	inline TArray<AStealthPlayerController*> ServerPlayers()
	{
		TArray<AStealthPlayerController*> Out;
		if (UWorld* Server = ServerWorld())
		{
			for (FConstPlayerControllerIterator It = Server->GetPlayerControllerIterator(); It; ++It)
			{
				AStealthPlayerController* Controller = Cast<AStealthPlayerController>(It->Get());
				if (Controller != nullptr && Controller->GetPawn() != nullptr && Controller->GetPlayerState<AStealthPlayerState>() != nullptr)
				{
					Out.Add(Controller);
				}
			}
		}
		return Out;
	}

	inline uint8 TeamOf(const AStealthPlayerController* Controller)
	{
		const AStealthPlayerState* State = Controller != nullptr ? Controller->GetPlayerState<AStealthPlayerState>() : nullptr;
		return State != nullptr ? State->Team : 255;
	}

	/**
	 * The 2v1 split of three players: Solo is alone on its team, Pair[0] / Pair[1] share the other. False when the
	 * teams are not 2v1.
	 */
	inline bool SplitTeams(AStealthPlayerController*& OutSolo, AStealthPlayerController* OutPair[2])
	{
		const TArray<AStealthPlayerController*> Players = ServerPlayers();
		if (Players.Num() != 3)
		{
			return false;
		}
		for (AStealthPlayerController* Candidate : Players)
		{
			int32 Mates = 0;
			for (AStealthPlayerController* Other : Players)
			{
				Mates += Other != Candidate && TeamOf(Other) == TeamOf(Candidate) ? 1 : 0;
			}
			if (Mates == 0)
			{
				OutSolo = Candidate;
				int32 Index = 0;
				for (AStealthPlayerController* Other : Players)
				{
					if (Other != Candidate)
					{
						OutPair[Index++] = Other;
					}
				}
				return Index == 2 && TeamOf(OutPair[0]) == TeamOf(OutPair[1]);
			}
		}
		return false;
	}

	/** The local controller of the machine that owns ServerController (the server world for the host). */
	inline AStealthPlayerController* OwnerMachineController(const AStealthPlayerController* ServerController)
	{
		if (ServerController == nullptr || ServerController->PlayerState == nullptr)
		{
			return nullptr;
		}
		if (ServerController->IsLocalController())
		{
			return const_cast<AStealthPlayerController*>(ServerController);
		}
		const int32 Id = ServerController->PlayerState->GetPlayerId();
		for (UWorld* World : Worlds())
		{
			AStealthPlayerController* Local = World->GetNetMode() == NM_Client ? Cast<AStealthPlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (Local != nullptr && Local->PlayerState != nullptr && Local->PlayerState->GetPlayerId() == Id)
			{
				return Local;
			}
		}
		return nullptr;
	}

	/** Teleports Pawn to a spot clear of every prototype bush (rule 2 would turn the strike into a bush strike). */
	inline void MoveClearOfBushes(APawn* Pawn)
	{
		UWorld* Server = ServerWorld();
		const FVector Origin = Pawn->GetActorLocation();
		for (int32 Ring = 0; Ring < 8; ++Ring)
		{
			for (int32 Step = 0; Step < 8; ++Step)
			{
				const FVector Candidate = Origin + FRotator(0.0, Step * 45.0, 0.0).RotateVector(FVector(Ring * 400.0, 0.0, 0.0));
				bool bClear = true;
				for (TActorIterator<AStealthBush> It(Server); It && bClear; ++It)
				{
					bClear = FVector::Dist2D(It->GetActorLocation(), Candidate) > 900.0;
				}
				FHitResult Floor;
				if (bClear && Server->LineTraceSingleByChannel(Floor, Candidate + FVector(0.0, 0.0, 200.0), Candidate - FVector(0.0, 0.0, 800.0), ECC_Visibility))
				{
					Pawn->TeleportTo(Candidate, FRotator(0.0, Pawn->GetActorRotation().Yaw, 0.0));
					return;
				}
			}
		}
	}

	/**
	 * Places Pawn DistanceCm in front of Anchor, facing it; a player's control rotation follows (its owning client owns the
	 * rotation, as the StealthBot driver does), so the facing survives the next moves.
	 */
	inline void PlaceFacing(APawn* Pawn, const APawn* Anchor, double DistanceCm)
	{
		const FVector Forward = Anchor->GetActorForwardVector().GetSafeNormal2D();
		const FRotator Facing(0.0, (-Forward).Rotation().Yaw, 0.0);
		Pawn->TeleportTo(Anchor->GetActorLocation() + Forward * DistanceCm, Facing);
		if (AController* Controller = Pawn->GetController())
		{
			Controller->SetControlRotation(Facing);
			if (APlayerController* Player = Cast<APlayerController>(Controller))
			{
				Player->ClientSetRotation(Facing, true);
			}
		}
	}

	/** Runs Submit on Pawn's command component inside its controller's own origin (as ingress does for that client). */
	inline void AsOwner(AStealthPawn* Pawn, uint32& SeqCounter, TFunctionRef<void(UPawnCommandComponent&)> Submit)
	{
		UPawnCommandComponent* Command = Pawn != nullptr ? Pawn->GetPawnCommand() : nullptr;
		AStealthPlayerController* Controller = Pawn != nullptr ? Cast<AStealthPlayerController>(Pawn->GetController()) : nullptr;
		if (Command == nullptr)
		{
			return;
		}
		if (Controller == nullptr)
		{
			Submit(*Command);
			return;
		}
		const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(Pawn->GetWorld());
		const FParticipantId Participant = Root != nullptr ? Root->GetParticipant(Controller) : FParticipantId();
		FCommandOrigin Origin;
		Origin.Mailbox = Participant.IsValid() ? FMailboxId::ForParticipant(Participant) : FMailboxId::ServerLocal();
		Origin.Sink = Controller->GetReplySink();
		FClientCommandSeq Seq;
		Seq.PossessionEpoch = Controller->GetServerPossessionEpoch();
		Seq.Seq = ++SeqCounter;
		FScopedCommandOrigin Scope(*Command, Origin, Seq, Controller->GetPossessedControlEpoch());
		Submit(*Command);
	}

	/** An Attack by Attacker at Target (an aimed PawnRef strike), with the attacker's own origin. */
	inline void StrikeAt(AStealthPawn* Attacker, AActor* Target, uint32& SeqCounter)
	{
		FAttackPayload Payload;
		Payload.WorldPoint = Target->GetActorLocation();
		Payload.PawnRef = Cast<APawn>(Target);
		AsOwner(Attacker, SeqCounter, [&Payload](UPawnCommandComponent& Command) { Command.Attack(Payload); });
	}

	/** Spawns an unbound Shared Pawn (an NPC identity for the Roster) DistanceCm in front of Anchor. */
	inline AStealthPawn* SpawnUnboundPawn(UWorld* World, const APawn* Anchor, double DistanceCm)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Forward = Anchor->GetActorForwardVector().GetSafeNormal2D();
		return World->SpawnActor<AStealthPawn>(AStealthPawn::StaticClass(), Anchor->GetActorLocation() + Forward * DistanceCm, (-Forward).Rotation(), Params);
	}
}

#endif
