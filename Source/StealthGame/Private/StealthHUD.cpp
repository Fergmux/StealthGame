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
#include "View/DefenceOwnerComponent.h"
#include "View/OwnResultMatcher.h"
#include "View/OwnResultPlateSettings.h"
#include "View/PenaltyHudViewModel.h"
#include "View/EncounterPresentation.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
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

void AStealthHUD::BeginPlay()
{
	Super::BeginPlay();
	// Story nkp-018: the Penalty elements are the minimal production Slate overlay (StealthPresentation), fed every frame from
	// this owner's controller; the HUD epic restyles them. No viewport (headless -nullrhi): nothing is added.
	AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetOwningPlayerController());
	ULocalPlayer* Local = PC != nullptr ? PC->GetLocalPlayer() : nullptr;
	UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr;
	if (Viewport == nullptr || PenaltyOverlay.IsValid())
	{
		return;
	}
	TWeakObjectPtr<AStealthPlayerController> WeakPC(PC);
	PenaltyOverlay = MakePenaltyHudOverlay([WeakPC]()
	{
		const AStealthPlayerController* OwnerPC = WeakPC.Get();
		return OwnerPC != nullptr ? OwnerPC->BuildPenaltyHudFrame(UEncounterPresentationSubsystem::EstimatedServerNow(OwnerPC->GetWorld())) : FPenaltyHudFrame();
	});
	Viewport->AddViewportWidgetForPlayer(Local, PenaltyOverlay.ToSharedRef(), 10);
}

void AStealthHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PenaltyOverlay.IsValid())
	{
		const AStealthPlayerController* PC = Cast<AStealthPlayerController>(GetOwningPlayerController());
		ULocalPlayer* Local = PC != nullptr ? PC->GetLocalPlayer() : nullptr;
		if (UGameViewportClient* Viewport = Local != nullptr ? Local->ViewportClient.Get() : nullptr)
		{
			Viewport->RemoveViewportWidgetForPlayer(Local, PenaltyOverlay.ToSharedRef());
		}
		PenaltyOverlay.Reset();
	}
	Super::EndPlay(EndPlayReason);
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
	DrawRoundBanner(PC, GS, ServerNow);
	DrawMessages(PC);
	DrawResultPlate(PC, ServerNow);
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
	float Y = 24.f;
	const float X = 24.f;
	const float Line = 22.f;
	// Story asn-017 / ASN-R23: the presented role (OnRoleChangePresented), never the Roster state's arrival.
	const AStealthPlayerController* PresentingController = Cast<AStealthPlayerController>(GetOwningPlayerController());
	const bool bInformant = PresentingController != nullptr ? PresentingController->IsPresentedInformant() : PS->IsInformant();

	DrawShadowedText(FString::Printf(TEXT("%s"), *TeamName(PS->Team)), X, Y, AlliedTurquoise, 1.3f);
	Y += Line * 1.4f;

	if (bInformant)
	{
		DrawShadowedText(TEXT("INFORMANT: watch, ping sightings [T]. No attacks, tasks or parkour."), X, Y, DangerCochineal);
	}
	else
	{
		DrawShadowedText(TEXT("ACTIVE"), X, Y, Neutral);
	}
	Y += Line;

	if (!bInformant)
	{
		FString Defence;
		FLinearColor DefenceColor = Neutral;
		// Story asn-016: the cooldown is the owner-only UDefenceOwnerComponent on this controller (the pawn's defence state
		// never replicates). Story asn-024 (UI-ASN-3): the brace square is the controller's sampled FOwnBraceIndicator, so a
		// reversal removes it at the contact, never at the Counter write's or the FDefenceResult's arrival.
		const AStealthPlayerController* OwnerController = Cast<AStealthPlayerController>(GetOwningPlayerController());
		const UDefenceOwnerComponent* DefenceOwner = OwnerController ? OwnerController->GetDefenceOwner() : nullptr;
		if (OwnerController && OwnerController->IsBraceSquareShown())
		{
			Defence = TEXT("Defence: PREPARED");
			DefenceColor = AlliedTurquoise;
		}
		else
		{
			const float Remaining = DefenceOwner ? static_cast<float>(DefenceOwner->GetCooldownRemaining(ServerNow)) : 0.f;
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

		// Story nkp-018: the "Wrong targets" tally (UI-NKP-1) is painted by the Penalty overlay (squares, numeral, triangle),
		// below this panel, Active and frozen alike; the true strike count stays on the server.

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
	const AStealthPlayerController* PresentingController = Cast<AStealthPlayerController>(GetOwningPlayerController());
	const bool bInformant = PresentingController != nullptr ? PresentingController->IsPresentedInformant() : !PS->IsActivePlayer();
	// The markers go with the round-end presentation, released by the controller (never at the Phase's arrival, HUD F10).
	const bool bRoundOn = GS->RoundPhase == EStealthRoundPhase::InProgress
		|| (GS->RoundPhase == EStealthRoundPhase::RoundOver && PresentingController != nullptr && !PresentingController->IsRoundEndPresented());
	if (PS->Team >= STEALTH_NUM_TEAMS || bInformant || !bRoundOn)
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

void AStealthHUD::DrawRoundBanner(const AStealthPlayerController* PC, const AStealthGameState* GS, double ServerNow)
{
	// Story asn-024 (HUD F10 stand-in): the round end is released by the controller's hold buffer, never before this
	// owner's own plate or own fall.
	if (GS->RoundPhase == EStealthRoundPhase::RoundOver && (PC == nullptr || !PC->IsRoundEndPresented()))
	{
		return;
	}
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

void AStealthHUD::DrawResultPlate(const AStealthPlayerController* PC, double ServerNow)
{
	const FOwnResultPlateView& Plate = PC->GetResultPlate();
	if (!Plate.bActive)
	{
		return;
	}
	const FOwnResultPlateSample Sample = FOwnResultPlateTimeline::Sample(UOwnResultPlateSettings::Get()->TimelineParams(), ServerNow - Plate.ShowServerTime);
	if (Sample.Opacity <= 0.f)
	{
		return;
	}
	// Z4 Result slot (design/ux/hud.md): one 300 x 64 plate at the lower centre, the same size for every outcome (HUD-R3), a
	// 40 x 40 glyph slot that is always reserved and the text in Bone. Only the text and the glyph differ.
	const float Scale = Canvas->ClipY / 1080.f;
	const float W = 300.f * Scale, H = 64.f * Scale, G = 40.f * Scale;
	const float X = Canvas->ClipX * 0.5f - W * 0.5f;
	float Y = 724.f * Scale;
	if (Plate.PreemptServerTime >= 0.0)
	{
		// HUD-R9 (story nkp-018): the removal card pushes the plate off the Result slot over HudPreemptExitSec in four steps at
		// full opacity (a push-off, never a cross-fade: no frame shows the plate and the card both semi-transparent).
		const double Exit = FMath::Max(0.01, static_cast<double>(UOwnResultPlateSettings::Get()->HudPreemptExitSec));
		const double Step = FMath::Floor(4.0 * FMath::Clamp((ServerNow - Plate.PreemptServerTime) / Exit, 0.0, 1.0)) / 4.0;
		if (Step >= 1.0)
		{
			return;
		}
		Y += static_cast<float>(Step) * (Canvas->ClipY - Y);
	}
	const float A = Sample.Opacity;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f * A), X, Y, W, H);
	const float GX = X + 12.f * Scale;
	const float GY = Y + (H - G) * 0.5f;
	switch (Plate.Plate)
	{
	case EOwnResultPlate::TargetEliminated:
	case EOwnResultPlate::StrikeReversed:
		// The Bone stepped-square stamp with one bar.
		DrawRect(FLinearColor(Neutral.R, Neutral.G, Neutral.B, A), GX + 6.f * Scale, GY + 6.f * Scale, G - 12.f * Scale, G - 12.f * Scale);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, A), GX + 6.f * Scale, GY + G * 0.5f - 2.f * Scale, G - 12.f * Scale, 4.f * Scale);
		break;
	case EOwnResultPlate::WrongTarget:
	case EOwnResultPlate::Reversed:
	{
		// The Cochineal upward triangle.
		const FLinearColor C(DangerCochineal.R, DangerCochineal.G, DangerCochineal.B, A);
		DrawLine(GX + G * 0.5f, GY + 4.f * Scale, GX + 4.f * Scale, GY + G - 4.f * Scale, C, 3.f);
		DrawLine(GX + 4.f * Scale, GY + G - 4.f * Scale, GX + G - 4.f * Scale, GY + G - 4.f * Scale, C, 3.f);
		DrawLine(GX + G - 4.f * Scale, GY + G - 4.f * Scale, GX + G * 0.5f, GY + 4.f * Scale, C, 3.f);
		break;
	}
	default:
		// NoContact: the slot stays empty (a miss is not finality; no Cochineal).
		break;
	}
	const float TextCentre = X + G + 12.f * Scale + (W - G - 12.f * Scale) * 0.5f;
	DrawShadowedText(Plate.Text.ToString(), TextCentre, Y + H * 0.5f - 12.f * Scale, FLinearColor(Neutral.R, Neutral.G, Neutral.B, A), 1.2f * Scale, true);
}

void AStealthHUD::CollectOwnerElements(const AStealthPlayerController& PC, TArray<FString>& Out)
{
	Out.Reset();
	UWorld* World = PC.GetWorld();
	const AStealthPlayerState* PS = PC.GetPlayerState<AStealthPlayerState>();
	const AStealthGameState* GS = World != nullptr ? World->GetGameState<AStealthGameState>() : nullptr;
	const AStealthCharacter* Pawn = Cast<AStealthCharacter>(PC.GetPawn());
	if (PS == nullptr || GS == nullptr)
	{
		return;
	}
	const double ServerNow = GS->GetServerWorldTimeSeconds();
	// The status panel.
	Out.Add(FString::Printf(TEXT("Team:%d"), PS->Team));
	const bool bInformant = PC.IsPresentedInformant();
	Out.Add(bInformant ? TEXT("Role:Informant") : TEXT("Role:Active"));
	if (!bInformant)
	{
		const UDefenceOwnerComponent* DefenceOwner = PC.GetDefenceOwner();
		const double Remaining = DefenceOwner != nullptr ? DefenceOwner->GetCooldownRemaining(ServerNow) : 0.0;
		const TCHAR* Defence = PC.IsBraceSquareShown() ? TEXT("Defence:Prepared")
			: (Remaining > 0.0 ? TEXT("Defence:Recovering")
				: (Pawn && (Pawn->IsRunning() || Pawn->IsInTraversal()) ? TEXT("Defence:Unavailable") : TEXT("Defence:Ready")));
		Out.Add(Defence);
		if (Pawn && Pawn->IsConcealed())
		{
			Out.Add(TEXT("Concealed"));
		}
	}
	if (PS->Team < STEALTH_NUM_TEAMS)
	{
		Out.Add(FString::Printf(TEXT("Stage:%d"), GS->GetTeamStage(PS->Team)));
		Out.Add(FString::Printf(TEXT("EnemyStage:%d"), GS->GetTeamStage(PS->Team == 0 ? 1 : 0)));
		// Teammate markers, by state (DrawTeammateMarkers also skips the off-screen ones).
		for (TActorIterator<AStealthCharacter> It(World); It; ++It)
		{
			const AStealthCharacter* Character = *It;
			if (Character != Pawn && !Character->IsDying() && !Character->IsNpc() && Character->GetTeam() == PS->Team)
			{
				const AStealthPlayerState* MatePS = Character->GetStealthPlayerState();
				Out.Add(FString::Printf(TEXT("TeammateMarker:%s%s"), MatePS ? *MatePS->GetPlayerName() : TEXT("?"), Character->IsInformant() ? TEXT(":informant") : TEXT("")));
			}
		}
		const bool bRoundOn = GS->RoundPhase == EStealthRoundPhase::InProgress || (GS->RoundPhase == EStealthRoundPhase::RoundOver && !PC.IsRoundEndPresented());
		if (!bInformant && bRoundOn)
		{
			const int32 Stage = GS->GetTeamStage(PS->Team);
			for (TActorIterator<AStealthObjective> It(World); It; ++It)
			{
				if (!It->IsCompleted() && FStealthRules::CanCompleteObjective(PS->Team, Stage, It->OwningTeam, It->Stage))
				{
					Out.Add(FString::Printf(TEXT("ObjectiveMarker:%s"), *It->DisplayName.ToString()));
				}
			}
		}
	}
	for (const FStealthPing& Ping : PC.GetPings())
	{
		Out.Add(FString::Printf(TEXT("Ping:%s"), *Ping.SenderName));
	}
	if (GS->RoundPhase == EStealthRoundPhase::WaitingToStart || (GS->RoundPhase == EStealthRoundPhase::RoundOver && PC.IsRoundEndPresented()))
	{
		Out.Add(FString::Printf(TEXT("RoundBanner:%d"), static_cast<int32>(GS->RoundPhase)));
	}
	const double WorldNow = World->GetTimeSeconds();
	for (const FStealthHudMessage& Message : PC.GetMessages())
	{
		if (Message.ExpireTime > WorldNow)
		{
			Out.Add(FString::Printf(TEXT("Message:%s"), *Message.Text.ToString()));
		}
	}
	const FOwnResultPlateView& Plate = PC.GetResultPlate();
	if (Plate.bActive && FOwnResultPlateTimeline::Sample(UOwnResultPlateSettings::Get()->TimelineParams(), ServerNow - Plate.ShowServerTime).Opacity > 0.f)
	{
		Out.Add(FString::Printf(TEXT("ResultPlate:%s"), *Plate.Text.ToString()));
	}
	if (Pawn && Pawn->GetLocalInteractProgress() >= 0.f)
	{
		Out.Add(TEXT("InteractBar"));
	}
	// Story nkp-018: the Penalty overlay's elements (the tally Active and frozen, the exile card, the returning countdown).
	const FPenaltyHudFrame Penalty = PC.BuildPenaltyHudFrame(UEncounterPresentationSubsystem::EstimatedServerNow(World));
	Out.Add(FString::Printf(TEXT("Tally:%d%s"), Penalty.Tally.Tally, Penalty.Tally.bFrozen ? TEXT(":frozen") : (Penalty.Tally.bTriangle ? TEXT(":triangle") : TEXT(""))));
	if (Penalty.Card.Phase != EPenaltyCardPhase::Hidden)
	{
		Out.Add(FString::Printf(TEXT("ExileCard:%d"), static_cast<int32>(Penalty.Card.Phase)));
	}
	if (Penalty.Countdown.bVisible)
	{
		Out.Add(FString::Printf(TEXT("Countdown:%s"), *Penalty.Countdown.Text.ToString()));
	}
	Out.Add(TEXT("Controls"));
}
