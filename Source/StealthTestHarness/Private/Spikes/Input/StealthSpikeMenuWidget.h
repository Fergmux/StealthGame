#pragma once

// THROWAWAY SPIKE (story iif-011, ADR-0011 V5 / ADR-0021 UI-V2): a minimal Common UI activatable widget whose desired
// input config is Menu, standing in for a real menu pushed through UStealthMenuStack (which does not exist yet). The
// spike activates it to make the action router switch its active input mode to Menu. Harness-only, never shipped.

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"

#include "StealthSpikeMenuWidget.generated.h"

UCLASS(NotBlueprintable, NotBlueprintType)
class UStealthSpikeMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override
	{
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	}
};
