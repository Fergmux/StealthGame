#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "StealthGameSession.generated.h"

/**
 * The game session (ADR-0005 section 1 hook table; story st-023). Admission is decided by Session's gates in
 * AStealthGameMode::PreLogin (rule 7, stories 028-038), so the engine's session approval never refuses a login:
 * ApproveLogin always returns success (an empty error string).
 */
UCLASS()
class STEALTHGAME_API AStealthGameSession : public AGameSession
{
	GENERATED_BODY()

public:
	/** Always success: the admission gates live in PreLogin (ADR-0005 section 3). */
	virtual FString ApproveLogin(const FString& Options) override;
};
