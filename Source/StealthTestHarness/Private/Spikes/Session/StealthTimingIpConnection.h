#pragma once

// THROWAWAY spike measurement: see StealthSpikeMeasure.h.

#include "CoreMinimal.h"
#include "IpConnection.h"
#include "StealthTimingIpConnection.generated.h"

/** UIpConnection that time-stamps every flushed and every received packet id (harness processes only). */
UCLASS(transient, config = Engine)
class UStealthTimingIpConnection : public UIpConnection
{
	GENERATED_BODY()

public:
	UStealthTimingIpConnection(const FObjectInitializer& ObjectInitializer);

	virtual void FlushNet(bool bIgnoreSimulation = false) override;
	virtual void ReceivedPacket(FBitReader& Reader, bool bIsReinjectedPacket = false, bool bDispatchPacket = true) override;
};
