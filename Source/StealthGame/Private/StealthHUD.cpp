#include "StealthHUD.h"
#include "View/Input/PawnCommandClient.h"
#include "StealthCharacter.h"
#include "StealthGameState.h"
#include "StealthObjective.h"
#include "StealthPlayerController.h"
#include "StealthPlayerState.h"
#include "StealthRules.h"
#include "StealthSettings.h"
#include "Server/StealthCompositionRoot.h"
#include "View/StealthRosterTeamView.h"
#include "View/StealthRoundPublicState.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor AlliedTurquoise(0.20f, 0.85f, 0.78f);
	const FLinearColor DangerCochineal(0.85f, 0.12f, 0.22f);
	const FLinearColor Neutral(0.92f, 0.89f, 0.82f);
	const FLinearColor Muted(0.65f, 0.62f, 0.56f);

	FString TeamName(uint8 Team)
	{
		const TArray<FText>& Names = UStealthSettings::Get()->TeamNames;
		return Names.IsValidIndex(Team) ? Names[Team].ToString() : TEXT("Unassigned");
	}
}

void AStealthHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetOwningPlayerController());
	const AStealthPlayerState* PS = PC ? PC->GetPlayerState<AStealthPlayerState>() : nullptr;
	const AStealthGameState* GS = GetWorld()->GetGameState<AStealthGameState>();
	const AStealthCharacter* Pawn = PC ? Cast<AStealthCharacter>(PC->GetPawn()) : nullptr;
	if (!PC || !PS || !GS)
	{
		return;
	}
	const double ServerNow = GS->GetServerWorldTimeSeconds();
	PC->PruneExpired(GetWorld()->GetTimeSeconds());

	DrawTeammateMarkers(PS);
	DrawObjectiveMarkers(PS, GS);
	DrawPings(PC);
	DrawStatusPanel(PS, GS, Pawn, ServerNow);
	DrawRoundBanner(GS, ServerNow);
	DrawMessages(PC);
	DrawInteractBar(Pawn);
	DrawControls();
	DrawProductionLine(PC, GS);
}

void AStealthHUD::DrawProductionLine(AStealthPlayerController* PC, const AStealthGameState* GS)
{
#if !UE_BUILD_SHIPPING
	FString Line;
	if (GetWorld()->GetNetMode() != NM_Client)
	{
		if (const UStealthCompositionRoot* Root = UStealthCompositionRoot::Get(GetWorld()))
		{
			Line = Root->DescribeProductionState(GetWorld());
		}
	}
	else
	{
		const FRoundPublicState& State = GS->GetRoundPublicState();
		const URosterTeamViewComponent* View = PC->GetTeamView();
		const int32 Mates = View ? View->GetView().Teammates.Num() : 0;
		const int32 Team = View ? View->GetView().Owner.Team : 255;
		Line = FString::Printf(TEXT("[prod] Round %s  idx %u  changes %u (replicated) | own team view: team %d, %d teammate(s)"),
			LexRoundPhase(State.Phase), State.RoundIndex, State.PhaseChangeCount, Team == 255 ? -1 : Team, Mates);
	}
	if (!Line.IsEmpty())
	{
		DrawShadowedText(Line, 24.f, Canvas->ClipY - 64.f, FLinearColor(1.f, 0.85f, 0.25f), 0.9f);
	}
	// The production command path (story iif-094, D53): what the owner's builder sends this frame.
	if (const UPawnCommandClient* Client = PC ? PC->GetCommandClient() : nullptr)
	{
		const FPawnCommandClientCore& Core = Client->GetCore();
		const FString Cmd = FString::Printf(TEXT("[cmd] move %.2f %s  pace %.2f  | epoch %u  last seq %u  | ctx %s%s | swallowed ends %d"),
			Client->GetLastSentMove().Size(), Client->GetGait() == EGait::Run ? TEXT("Run") : TEXT("Walk"), Client->GetWalkSlowerPace(),
			Core.GetOwnerPossessionEpoch(), Core.GetLastSeq(), Client->IsInformantContext() ? TEXT("Informant") : TEXT("Active"),
			Client->IsMenuMode() ? TEXT(" (menu)") : TEXT(""), Core.GetSwallowedCount());
		DrawShadowedText(Cmd, 24.f, Canvas->ClipY - 92.f, FLinearColor(0.55f, 0.9f, 1.f), 0.9f);
	}
#endif
}

bool AStealthHUD::ProjectToScreen(const FVector& World, FVector2D& OutScreen) const
{
	const FVector Projected = Canvas->Project(World, false);
	if (Projected.Z <= 0.f)
	{
		return false;
	}
	OutScreen = FVector2D(Projected.X, Projected.Y);
	return true;
}

void AStealthHUD::DrawShadowedText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale, bool bCentered)
{
	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	float DrawX = X;
	if (bCentered)
	{
		float Width = 0.f, Height = 0.f;
		GetTextSize(Text, Width, Height, Font, Scale);
		DrawX -= Width * 0.5f;
	}
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, 0.8f), DrawX + 1.5f, Y + 1.5f, Font, Scale);
	DrawText(Text, Color, DrawX, Y, Font, Scale);
}

void AStealthHUD::DrawStatusPanel(const AStealthPlayerState* PS, const AStealthGameState* GS, const AStealthCharacter* Pawn, double ServerNow)
{
	const UStealthSettings* Settings = UStealthSettings::Get();
	float Y = 24.f;
	const float X = 24.f;
	const float Line = 22.f;

	DrawShadowedText(FString::Printf(TEXT("%s"), *TeamName(PS->Team)), X, Y, AlliedTurquoise, 1.3f);
	Y += Line * 1.4f;

	if (PS->IsInformant())
	{
		DrawShadowedText(TEXT("INFORMANT: watch, ping sightings [T]. No attacks, tasks or parkour."), X, Y, DangerCochineal);
	}
	else
	{
		DrawShadowedText(TEXT("ACTIVE"), X, Y, Neutral);
	}
	Y += Line;

	if (PS->IsActivePlayer())
	{
		FString Defence;
		FLinearColor DefenceColor = Neutral;
		if (Pawn && Pawn->IsPreparingDefence())
		{
			Defence = TEXT("Defence: PREPARED");
			DefenceColor = AlliedTurquoise;
		}
		else
		{
			const float Remaining = FStealthRules::DefenceCooldownRemaining(ServerNow, PS->LastDefenceActivationTime, Settings->DefenceCooldown);
			if (Remaining > 0.f)
			{
				Defence = FString::Printf(TEXT("Defence: recovering %.0fs"), Remaining);
				DefenceColor = Muted;
			}
			else if (Pawn && (Pawn->IsRunning() || Pawn->IsInTraversal()))
			{
				Defence = TEXT("Defence: unavailable while running or climbing");
				DefenceColor = Muted;
			}
			else
			{
				Defence = TEXT("Defence: READY [hold Q]");
			}
		}
		DrawShadowedText(Defence, X, Y, DefenceColor);
		Y += Line;

		DrawShadowedText(FString::Printf(TEXT("Civilians killed: %d/%d"), PS->NpcKills, Settings->NpcKillLimit), X, Y, PS->NpcKills > 0 ? DangerCochineal : Muted);
		Y += Line;

		if (Pawn && Pawn->IsConcealed())
		{
			DrawShadowedText(TEXT("Hidden from enemies"), X, Y, AlliedTurquoise);
			Y += Line;
		}
	}

	// Team progress (own team in full, enemy stage only — never their location).
	if (PS->Team < STEALTH_NUM_TEAMS)
	{
		const int32 Stage = GS->GetTeamStage(PS->Team);
		const int32 EnemyStage = GS->GetTeamStage(PS->Team == 0 ? 1 : 0);
		FString Current = TEXT("-");
		for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
		{
			if (!It->IsCompleted() && FStealthRules::CanCompleteObjective(PS->Team, Stage, It->OwningTeam, It->Stage))
			{
				Current = It->DisplayName.ToString();
				break;
			}
		}
		Y += Line * 0.5f;
		DrawShadowedText(FString::Printf(TEXT("Stage %d/%d: %s  [hold E]"), Stage, STEALTH_FINAL_STAGE, *Current), X, Y, Neutral);
		Y += Line;
		DrawShadowedText(FString::Printf(TEXT("Enemy progress: stage %d/%d"), EnemyStage, STEALTH_FINAL_STAGE), X, Y, Muted);
	}
}

void AStealthHUD::DrawTeammateMarkers(const AStealthPlayerState* PS)
{
	if (PS->Team >= STEALTH_NUM_TEAMS)
	{
		return;
	}
	const APawn* Own = GetOwningPawn();
	for (TActorIterator<AStealthCharacter> It(GetWorld()); It; ++It)
	{
		const AStealthCharacter* Character = *It;
		if (Character == Own || Character->IsDying() || Character->IsNpc() || Character->GetTeam() != PS->Team)
		{
			continue;
		}
		FVector2D Screen;
		if (!ProjectToScreen(Character->GetActorLocation() + FVector(0.f, 0.f, 120.f), Screen))
		{
			continue;
		}
		const FLinearColor Color = Character->IsInformant() ? Muted : AlliedTurquoise;
		// Small downward chevron.
		const float S = 8.f;
		DrawLine(Screen.X - S, Screen.Y - S, Screen.X, Screen.Y, Color, 2.f);
		DrawLine(Screen.X + S, Screen.Y - S, Screen.X, Screen.Y, Color, 2.f);
		const AStealthPlayerState* MatePS = Character->GetStealthPlayerState();
		const FString Label = MatePS ? MatePS->GetPlayerName() + (Character->IsInformant() ? TEXT(" (informant)") : TEXT("")) : FString();
		DrawShadowedText(Label, Screen.X, Screen.Y - S - 20.f, Color, 0.8f, true);
	}
}

void AStealthHUD::DrawObjectiveMarkers(const AStealthPlayerState* PS, const AStealthGameState* GS)
{
	if (PS->Team >= STEALTH_NUM_TEAMS || !PS->IsActivePlayer() || GS->RoundPhase != EStealthRoundPhase::InProgress)
	{
		return;
	}
	const int32 Stage = GS->GetTeamStage(PS->Team);
	for (TActorIterator<AStealthObjective> It(GetWorld()); It; ++It)
	{
		// The final site is only revealed to a team once its preliminaries are done (anti-camping, Pillar 4).
		if (It->IsCompleted() || !FStealthRules::CanCompleteObjective(PS->Team, Stage, It->OwningTeam, It->Stage))
		{
			continue;
		}
		FVector2D Screen;
		if (ProjectToScreen(It->GetActorLocation() + FVector(0.f, 0.f, 150.f), Screen))
		{
			DrawRect(AlliedTurquoise, Screen.X - 5.f, Screen.Y - 5.f, 10.f, 10.f);
			DrawShadowedText(It->DisplayName.ToString(), Screen.X, Screen.Y - 28.f, AlliedTurquoise, 0.85f, true);
		}
	}
}

void AStealthHUD::DrawPings(AStealthPlayerController* PC)
{
	for (const FStealthPing& Ping : PC->GetPings())
	{
		FVector2D Screen;
		if (ProjectToScreen(Ping.Location + FVector(0.f, 0.f, 30.f), Screen))
		{
			const float R = 12.f;
			DrawLine(Screen.X - R, Screen.Y, Screen.X, Screen.Y - R, AlliedTurquoise, 2.f);
			DrawLine(Screen.X, Screen.Y - R, Screen.X + R, Screen.Y, AlliedTurquoise, 2.f);
			DrawLine(Screen.X + R, Screen.Y, Screen.X, Screen.Y + R, AlliedTurquoise, 2.f);
			DrawLine(Screen.X, Screen.Y + R, Screen.X - R, Screen.Y, AlliedTurquoise, 2.f);
			DrawShadowedText(Ping.SenderName, Screen.X, Screen.Y + R + 4.f, AlliedTurquoise, 0.75f, true);
		}
	}
}

void AStealthHUD::DrawMessages(AStealthPlayerController* PC)
{
	float Y = Canvas->ClipY * 0.42f;
	for (const FStealthHudMessage& Message : PC->GetMessages())
	{
		DrawShadowedText(Message.Text.ToString(), Canvas->ClipX * 0.5f, Y, Message.bDanger ? DangerCochineal : Neutral, 1.1f, true);
		Y += 28.f;
	}
}

void AStealthHUD::DrawRoundBanner(const AStealthGameState* GS, double ServerNow)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float Remaining = FMath::Max(0.f, static_cast<float>(GS->PhaseEndServerTime - ServerNow));
	switch (GS->RoundPhase)
	{
	case EStealthRoundPhase::WaitingToStart:
		DrawShadowedText(GS->PhaseEndServerTime > 0.0 ? FString::Printf(TEXT("Round starts in %.0f"), Remaining) : TEXT("Waiting for players"), CX, 24.f, Neutral, 1.3f, true);
		break;
	case EStealthRoundPhase::RoundOver:
		DrawShadowedText(FString::Printf(TEXT("%s WIN (%s)"), *TeamName(GS->WinningTeam).ToUpper(),
			GS->WinReason == EStealthWinReason::FinalObjective ? TEXT("final objective") : TEXT("elimination")), CX, 24.f, DangerCochineal, 1.6f, true);
		DrawShadowedText(FString::Printf(TEXT("Next round in %.0f"), Remaining), CX, 60.f, Neutral, 1.f, true);
		break;
	default:
		break;
	}
}

void AStealthHUD::DrawInteractBar(const AStealthCharacter* Pawn)
{
	const float Progress = Pawn ? Pawn->GetLocalInteractProgress() : -1.f;
	if (Progress < 0.f)
	{
		return;
	}
	const float W = 260.f, H = 10.f;
	const float X = Canvas->ClipX * 0.5f - W * 0.5f;
	const float Y = Canvas->ClipY * 0.62f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X - 2.f, Y - 2.f, W + 4.f, H + 4.f);
	DrawRect(AlliedTurquoise, X, Y, W * Progress, H);
}

void AStealthHUD::DrawControls()
{
	// The IIF R15 default bindings (IMC_Common / IMC_Active, story iif-015).
	DrawShadowedText(TEXT("WASD move (walk)  Ctrl/Alt walk slower  Shift run  Space climb  LMB assassinate  hold Q defend  hold E task  F hide  G ping  V camera  X shoulder  C crouch"),
		24.f, Canvas->ClipY - 34.f, Muted, 0.8f);
}
