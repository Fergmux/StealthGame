#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StealthHUD.generated.h"

class AStealthCharacter;
class AStealthGameState;
class AStealthPlayerController;
class AStealthPlayerState;
class SWidget;

/**
 * Prototype HUD drawn on the canvas (a UMG version is a later UI story).
 * Shows only the local player's own and team information. It never marks, names or outlines
 * an enemy (Pillar 1): teammate markers are computed on this client from this client's team.
 * Colour rule from the art direction: turquoise = allied UI only, cochineal red = danger/finality.
 */
UCLASS()
class STEALTHGAME_API AStealthHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	/** Owner: adds the Penalty HUD overlay (story nkp-018: tally, returning countdown, exile card) to this player's viewport. */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The owner HUD's element list for PC's current state (story asn-024 AC3 / story asn-017): one entry per element
	 * DrawHUD would draw (name plus its shown value), computed from the same state without a canvas, so tests compare the
	 * widget list of two states (an aimed-at bush and an unaimed baseline must be identical: no element depends on aim).
	 * World-anchored elements are listed by state, not by screen projection.
	 */
	static void CollectOwnerElements(const AStealthPlayerController& PC, TArray<FString>& OutElements);

protected:
	void DrawStatusPanel(const AStealthPlayerState* PS, const AStealthGameState* GS, const AStealthCharacter* Pawn, double ServerNow);
	void DrawTeammateMarkers(const AStealthPlayerState* PS);
	void DrawPings(AStealthPlayerController* PC);
	void DrawMessages(AStealthPlayerController* PC);
	/** The Result slot (HUD E07 / E08 stand-in): the owner's plate on the F3 timeline from its local contact frame. */
	void DrawResultPlate(const AStealthPlayerController* PC, double ServerNow);
	void DrawRoundBanner(const AStealthPlayerController* PC, const AStealthGameState* GS, double ServerNow);
	void DrawInteractBar(const AStealthCharacter* Pawn);
	void DrawControls();

	/**
	 * Development only (decision D53): one line showing the production Round phase and Roster counts so the
	 * integration is visible. Host: the server stores (full counts, a development view); client: the replicated
	 * FRoundPublicState and this player's own C-TeamView only (no opponent count reaches a client).
	 */
	void DrawProductionLine(AStealthPlayerController* PC, const AStealthGameState* GS);

	/** Projects a world location to the screen; false if behind the camera. */
	bool ProjectToScreen(const FVector& World, FVector2D& OutScreen) const;

	void DrawShadowedText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f, bool bCentered = false);

	/** The minimal production Penalty overlay (StealthPresentation MakePenaltyHudOverlay), while it is in the viewport. */
	TSharedPtr<SWidget> PenaltyOverlay;
};
