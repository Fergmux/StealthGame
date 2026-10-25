// Scenario "S1": IVP spike S1 multi-process raw-bunch capture (story
// production/epics/information-visibility-policy/story-001-spike-s1-multiprocess-raw-bunch-capture.md).
//
// Listen host + 3 clients (C1, C2, C3). The server starts a round, places C1's and C2's pawns and two
// crowd pawns in front of C3 (the non-owning observer), then runs takeovers (player possesses a crowd
// pawn, the former body is handed to that pawn's AI controller), a kill and a second takeover. C1 and
// C2 feed real movement, look (pitch) and Enhanced Input walk/sprint injections so RemoteViewPitch16 and
// the sample's CharacterInputState are exercised. Server "truth" records (pawn NetGUID -> kind, owner)
// let the analyser (tools/ci/mp/analyze_s1.py) split every captured bunch into owner / non-owner and
// player / NPC. Run with -StealthNetCapture (capture) and -StealthHarnessTrims=1 (ADR-0001 4.1) or 17
// (negative control: PlayerState back at COND_None).
#include "Scenarios/StealthHarnessRun.h"

#include "StealthCharacter.h"
#include "StealthGameMode.h"
#include "StealthNpcController.h"
#include "StealthPlayerState.h"
#include "StealthTestHarnessModule.h"

#include "EngineUtils.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "InputAction.h"

namespace StealthHarness
{
	namespace S1
	{
		/** Seconds after the crowd exists at which each step runs (server). */
		constexpr double PlaceAt = 1.0;
		constexpr double TakeOverC1At = 6.0;
		constexpr double TakeOverC2At = 12.0;
		constexpr double ReplaceAt = 18.0;
		constexpr double KillAt = 24.0;
		constexpr double TakeOverC1AgainAt = 30.0;
		constexpr double EndAt = 40.0;
		constexpr double TruthPeriod = 0.5;
		constexpr int32 CrowdNeeded = 4;
		constexpr double CrowdWaitCeiling = 30.0;
	}

	/** Server truth for every AStealthCharacter: NetGUID, kind (player name or "npc"), team, dying. */
	static void WriteTruth(FRun& Run, UWorld* World, const TCHAR* Tag)
	{
		FString Pawns;
		for (TActorIterator<AStealthCharacter> It(World); It; ++It)
		{
			const AStealthCharacter* Pawn = *It;
			const AController* Controller = Pawn->GetController();
			const APlayerController* PC = Cast<APlayerController>(Controller);
			const AStealthPlayerState* PS = PC ? PC->GetPlayerState<AStealthPlayerState>() : nullptr;
			Pawns += FString::Printf(TEXT("%s{\"guid\":%s,\"name\":%s,\"kind\":%s,\"ctrl\":%s,\"team\":%d,\"dying\":%d,\"loc\":%s}"),
				Pawns.IsEmpty() ? TEXT("") : TEXT(","), *FRun::Q(FRun::Guid(World, Pawn)), *FRun::Q(Pawn->GetName()),
				*FRun::Q(PC ? FRun::PlayerName(PC) : FString(TEXT("npc"))), *FRun::Q(GetNameSafe(Controller)), PS ? (int32)PS->Team : -1,
				(int32)Pawn->IsDying(), *FRun::Q(Pawn->GetActorLocation().ToCompactString()));
		}
		Run.Write(TEXT("truthS1"), FString::Printf(TEXT("\"tag\":%s,%s,\"pawns\":[%s]"), *FRun::Q(Tag), *FRun::ServerTimeFields(World), *Pawns));
	}

	static int32 CountCrowd(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AStealthCharacter> It(World); It; ++It)
		{
			Count += (Cast<AStealthNpcController>(It->GetController()) && !It->IsDying()) ? 1 : 0;
		}
		return Count;
	}

	/** Player Name possesses the nearest crowd pawn; its former body goes to that pawn's AI controller. */
	static void TakeOver(FRun& Run, UWorld* World, const FString& Name)
	{
		APlayerController* PC = FRun::FindPlayer(World, Name);
		APawn* Old = PC ? PC->GetPawn() : nullptr;
		AStealthCharacter* Target = nullptr;
		double Best = TNumericLimits<double>::Max();
		for (TActorIterator<AStealthCharacter> It(World); Old && It; ++It)
		{
			if (*It != Old && !It->IsDying() && Cast<AStealthNpcController>(It->GetController()))
			{
				const double Distance = FVector::Dist(It->GetActorLocation(), Old->GetActorLocation());
				if (Distance < Best)
				{
					Best = Distance;
					Target = *It;
				}
			}
		}
		if (!PC || !Old || !Target)
		{
			Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"takeoverFailed\",\"player\":%s"), *FRun::Q(Name)));
			return;
		}
		AController* NpcController = Target->GetController();
		NpcController->UnPossess();
		PC->UnPossess();
		PC->Possess(Target);
		NpcController->Possess(Old);
		Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"takeover\",\"player\":%s,\"newBody\":%s,\"formerBody\":%s,%s"),
			*FRun::Q(Name), *FRun::Q(FRun::Guid(World, Target)), *FRun::Q(FRun::Guid(World, Old)), *FRun::ServerTimeFields(World)));
	}

	/** Places C1's and C2's pawns and the two nearest crowd pawns in front of C3's pawn. */
	static void PlaceInViewOfObserver(FRun& Run, UWorld* World)
	{
		const APlayerController* ObserverPC = FRun::FindPlayer(World, TEXT("C3"));
		const APawn* Observer = ObserverPC ? ObserverPC->GetPawn() : nullptr;
		if (!Observer)
		{
			Run.Write(TEXT("event"), TEXT("\"ev\":\"placeFailed\""));
			return;
		}
		const FVector Origin = Observer->GetActorLocation();
		const FVector Forward = Observer->GetActorForwardVector().GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
		const FRotator FaceObserver = (-Forward).Rotation();
		int32 Placed = 0;
		for (const TCHAR* Name : { TEXT("C1"), TEXT("C2") })
		{
			if (const APlayerController* PC = FRun::FindPlayer(World, Name); PC && PC->GetPawn())
			{
				const double Side = Placed++ == 0 ? 150.0 : -150.0;
				PC->GetPawn()->TeleportTo(Origin + Forward * 400.0 + Right * Side, FaceObserver);
			}
		}
		TArray<AStealthCharacter*> Npcs;
		for (TActorIterator<AStealthCharacter> It(World); It; ++It)
		{
			if (Cast<AStealthNpcController>(It->GetController()) && !It->IsDying())
			{
				Npcs.Add(*It);
			}
		}
		Npcs.Sort([&Origin](const AStealthCharacter& A, const AStealthCharacter& B)
		{
			return FVector::DistSquared(A.GetActorLocation(), Origin) < FVector::DistSquared(B.GetActorLocation(), Origin);
		});
		for (int32 Index = 0; Index < FMath::Min(2, Npcs.Num()); ++Index)
		{
			Npcs[Index]->TeleportTo(Origin + Forward * 600.0 + Right * (Index == 0 ? 150.0 : -150.0), FaceObserver);
		}
		Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"placed\",\"players\":%d,\"npcs\":%d,%s"), Placed, FMath::Min(2, Npcs.Num()), *FRun::ServerTimeFields(World)));
	}

	static int32 PlayerIndex(UWorld* World, const FString& Name)
	{
		int32 Index = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It, ++Index)
		{
			if (FRun::PlayerName(It->Get()) == Name)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	class FS1Scenario final : public IScenario
	{
	public:
		virtual void OnAllJoined(FRun& Run, UWorld* World) override
		{
			if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
			{
				GameMode->StartRound();
				Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"startRound\",%s"), *FRun::ServerTimeFields(World)));
			}
			RoundStartedAt = World->GetTimeSeconds();
		}

		virtual bool TickServer(FRun& Run, UWorld* World, double) override
		{
			const double Now = World->GetTimeSeconds();
			if (CrowdAt < 0.0)
			{
				if (CountCrowd(World) >= S1::CrowdNeeded || Now - RoundStartedAt > S1::CrowdWaitCeiling)
				{
					CrowdAt = Now;
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"crowd\",\"npcs\":%d,%s"), CountCrowd(World), *FRun::ServerTimeFields(World)));
				}
				return false;
			}
			const double T = Now - CrowdAt;
			if (Now - LastTruth >= S1::TruthPeriod)
			{
				WriteTruth(Run, World, TEXT("periodic"));
				LastTruth = Now;
			}
			auto Step = [&](int32 Index, double At, TFunctionRef<void()> Action)
			{
				if (NextStep == Index && T >= At)
				{
					Action();
					WriteTruth(Run, World, TEXT("step"));
					++NextStep;
				}
			};
			Step(0, S1::PlaceAt, [&]() { PlaceInViewOfObserver(Run, World); });
			Step(1, S1::TakeOverC1At, [&]() { TakeOver(Run, World, TEXT("C1")); });
			Step(2, S1::TakeOverC2At, [&]() { TakeOver(Run, World, TEXT("C2")); });
			Step(3, S1::ReplaceAt, [&]() { PlaceInViewOfObserver(Run, World); });
			Step(4, S1::KillAt, [&]()
			{
				if (AStealthGameMode* GameMode = World->GetAuthGameMode<AStealthGameMode>())
				{
					GameMode->DebugBotAction(PlayerIndex(World, TEXT("C1")), TEXT("strike_npc"));
					Run.Write(TEXT("event"), FString::Printf(TEXT("\"ev\":\"kill\",\"attacker\":\"C1\",%s"), *FRun::ServerTimeFields(World)));
				}
			});
			Step(5, S1::TakeOverC1AgainAt, [&]() { TakeOver(Run, World, TEXT("C1")); });
			return T >= S1::EndAt;
		}

		virtual void TickClient(FRun& Run, UWorld* World, double ClientSeconds) override
		{
			// C1 and C2 act like players; C3 is the passive non-owning observer.
			if (Run.GetClientIndex() != 1 && Run.GetClientIndex() != 2)
			{
				return;
			}
			APlayerController* PC = World->GetFirstPlayerController();
			APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (!Pawn)
			{
				return;
			}
			const double Cycle = FMath::Fmod(ClientSeconds, 6.0);
			if (Cycle < 2.0)
			{
				Pawn->AddMovementInput(Pawn->GetActorForwardVector(), 1.f);
				PC->AddYawInput(0.4f);
			}
			// Look up and down like a mouse would (RemoteViewPitch16 follows the control rotation pitch).
			PC->AddPitchInput(FMath::Sin(ClientSeconds * 1.3) * 0.6f);
			InjectOnce(Run, PC, Cycle, 2.5, TEXT("/Game/Input/IA_Walk.IA_Walk"), bWalkSent);
			InjectOnce(Run, PC, Cycle, 4.0, TEXT("/Game/Input/IA_Sprint.IA_Sprint"), bSprintSent);
			if (Cycle < 1.0)
			{
				bWalkSent = false;
				bSprintSent = false;
			}
		}

	private:
		/** Injects one press of the action through Enhanced Input once per cycle, at At seconds into it. */
		static void InjectOnce(FRun& Run, APlayerController* PC, double Cycle, double At, const TCHAR* ActionPath, bool& bSent)
		{
			if (bSent || Cycle < At)
			{
				return;
			}
			bSent = true;
			const UInputAction* Action = LoadObject<UInputAction>(nullptr, ActionPath);
			UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer() ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
			if (Action && Input)
			{
				Input->InjectInputForAction(Action, FInputActionValue(true), {}, {});
			}
			Run.Write(TEXT("input"), FString::Printf(TEXT("\"action\":%s,\"ok\":%d"), *FRun::Q(ActionPath), (int32)(Action && Input)));
		}

		double RoundStartedAt = 0.0;
		double CrowdAt = -1.0;
		double LastTruth = -1.0;
		int32 NextStep = 0;
		bool bWalkSent = false;
		bool bSprintSent = false;
	};

	static TUniquePtr<IScenario> MakeS1Scenario()
	{
		return MakeUnique<FS1Scenario>();
	}

	static FScenarioRegistrar GRegisterS1(TEXT("S1"), &MakeS1Scenario);
}
