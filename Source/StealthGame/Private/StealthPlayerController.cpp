#include "StealthPlayerController.h"
#include "StealthGame.h"
#include "StealthGameMode.h"
#include "StealthSettings.h"

#include "StealthBush.h"

#include "Camera/CameraActor.h"
#include "EngineUtils.h"

namespace
{
	constexpr int32 MaxHudMessages = 5;
	/** Short confirmations were missed in playtests; keep every message up at least this long. */
	constexpr float MinHudMessageSeconds = 6.f;
}

void AStealthPlayerController::ClientStealthMessage_Implementation(const FText& Text, bool bDanger, float Duration)
{
	FStealthHudMessage& Message = Messages.AddDefaulted_GetRef();
	Message.Text = Text;
	Message.bDanger = bDanger;
	Message.ExpireTime = GetWorld()->GetTimeSeconds() + FMath::Max(Duration, MinHudMessageSeconds);
	if (Messages.Num() > MaxHudMessages)
	{
		Messages.RemoveAt(0);
	}
	UE_LOG(LogStealth, Log, TEXT("[HUD] %s (netmode %d) message: %s"), *GetName(), (int32)GetNetMode(), *Text.ToString());
}

void AStealthPlayerController::ClientReceivePing_Implementation(FVector_NetQuantize Location, const FString& SenderName)
{
	FStealthPing& Ping = Pings.AddDefaulted_GetRef();
	Ping.Location = Location;
	Ping.SenderName = SenderName;
	Ping.ExpireTime = GetWorld()->GetTimeSeconds() + UStealthSettings::Get()->PingLifetime;
}

void AStealthPlayerController::PruneExpired(double Now)
{
	Messages.RemoveAll([Now](const FStealthHudMessage& M) { return M.ExpireTime <= Now; });
	Pings.RemoveAll([Now](const FStealthPing& P) { return P.ExpireTime <= Now; });
}

void AStealthPlayerController::StealthStartRound()
{
	ServerStartRound();
}

void AStealthPlayerController::StealthEndRound()
{
	ServerEndRound();
}

void AStealthPlayerController::StealthSwitchTeam()
{
	ServerSwitchTeam();
}

void AStealthPlayerController::ServerStartRound_Implementation()
{
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->StartRound();
	}
}

void AStealthPlayerController::ServerEndRound_Implementation()
{
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->AbortRound();
	}
}

void AStealthPlayerController::ServerSwitchTeam_Implementation()
{
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->SwitchTeam(this);
	}
}

void AStealthPlayerController::StealthBot(int32 PlayerIndex, const FString& Action)
{
#if !UE_BUILD_SHIPPING
	ServerStealthBot(PlayerIndex, Action);
#endif
}

void AStealthPlayerController::ServerStealthBot_Implementation(int32 PlayerIndex, const FString& Action)
{
#if !UE_BUILD_SHIPPING
	if (AStealthGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthGameMode>())
	{
		GameMode->DebugBotAction(PlayerIndex, Action);
	}
#endif
}

void AStealthPlayerController::StealthDebugCam(int32 Mode)
{
#if !UE_BUILD_SHIPPING
	if (Mode == 0)
	{
		if (GetPawn())
		{
			SetViewTarget(GetPawn());
		}
		return;
	}
	if (!DebugCamera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		DebugCamera = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(-4200.f, 0.f, 3200.f), FRotator(-40.f, 0.f, 0.f), Params);
	}
	DebugCamera->SetActorLocationAndRotation(FVector(-4200.f, 0.f, 3200.f), FRotator(-40.f, 0.f, 0.f));

	// Mode 2: frame the bush nearest to this player's pawn, seen from the pawn's side.
	if (Mode == 2 && GetPawn())
	{
		const FVector From = GetPawn()->GetActorLocation();
		const AStealthBush* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AStealthBush> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), From);
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				Nearest = *It;
			}
		}
		if (Nearest)
		{
			const FVector Target = Nearest->GetActorLocation();
			const FVector Eye = Target + (From - Target).GetSafeNormal2D() * 450.f + FVector(0.f, 0.f, 120.f);
			DebugCamera->SetActorLocationAndRotation(Eye, (Target - Eye).Rotation());
		}
	}
	SetViewTarget(DebugCamera);
#endif
}
