#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "StealthHUD.generated.h"

class AStealthCharacter;
class AStealthGameState;
class AStealthPlayerController;
class AStealthPlayerState;

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

protected:
	void DrawStatusPanel(const AStealthPlayerState* PS, const AStealthGameState* GS, const AStealthCharacter* Pawn, double ServerNow);
	void DrawTeammateMarkers(const AStealthPlayerState* PS);
	void DrawObjectiveMarkers(const AStealthPlayerState* PS, const AStealthGameState* GS);
	void DrawPings(AStealthPlayerController* PC);
	void DrawMessages(AStealthPlayerController* PC);
	void DrawRoundBanner(const AStealthGameState* GS, double ServerNow);
	void DrawInteractBar(const AStealthCharacter* Pawn);
	void DrawControls();

	/** Projects a world location to the screen; false if behind the camera. */
	bool ProjectToScreen(const FVector& World, FVector2D& OutScreen) const;

	void DrawShadowedText(const FString& Text, float X, float Y, const FLinearColor& Color, float Scale = 1.f, bool bCentered = false);
};
