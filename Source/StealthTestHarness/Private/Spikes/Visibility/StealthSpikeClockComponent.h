#pragma once

// THROWAWAY spike code for the IVP S10 re-run (story production/epics/information-visibility-policy/
// story-056-s10-fix-implementation-and-rerun.md; ADR-0003 amendment "S10 resolution 2026-10-25" S10-R6).
//
// A harness implementation of ADR-0002 section 9's client server-time estimator, exactly as the ADR states it:
// the owning client sends ServerClockPing(T1) (unreliable) every 0.5 s for the first 5 s after begin play, then
// every 2 s; the server answers ClientClockPong(T1, T2, T3) (unreliable, owner only) with T2 stamped in the RPC body
// and T3 in the server tick (TG_PostUpdateWork, nearest the flush), both on the precise server clock TimeSeconds + (FPlatformTime::Seconds() - frame start);
// the server drops pings above 4 per second. The client keeps the last 16 samples (NTP four-timestamp form) and uses
// the offset of the minimum-delay sample; the estimate is FPlatformTime::Seconds() + SlewedOffset, slewed at
// <= 50 ms per second, stepped only on first sync or when the error exceeds 250 ms; synced after 5 samples.
// The production estimator is round-match-framework story 091 (Foundation Public/View EstimatedServerNow()).
// Records: "clkPong" (client, per sample), "clk" (client, every 0.5 s), "srvClk" (server frame-start pairs, written
// by the S10 re-run scenario).

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StealthSpikeClockComponent.generated.h"

/** One four-timestamp sample. */
struct FStealthSpikeClockSample
{
	double Delay = 0.0;
	double Offset = 0.0;
};

/** Process-wide client estimator state (one local player per harness client process). */
struct FStealthSpikeClockEstimator
{
	/** Sliding window of the last 16 samples. */
	TArray<FStealthSpikeClockSample> Samples;
	/** Samples received in total. */
	int32 SampleCount = 0;
	/** Offset added to FPlatformTime::Seconds() (server time - client QPC), slewed. */
	double SlewedOffset = 0.0;
	/** Last FPlatformTime at which the slew was advanced. */
	double LastSlewAt = -1.0;
	bool bSynced = false;
	/** Times the estimate was stepped (first sync, error > 250 ms). */
	int32 Steps = 0;

	/** The minimum-delay sample's offset (the slew target); 0 without samples. */
	double TargetOffset() const;

	/** Adds a sample (T1..T4) and advances the slew. */
	void AddSample(double T1, double T2, double T3, double T4);

	/** Advances the slew towards the target at <= 50 ms per second. Idempotent within one instant. */
	void Advance(double PlatformNow);

	/** EstimatedServerNow at PlatformNow (advances the slew first). */
	double EstimatedServerNow(double PlatformNow);

	static FStealthSpikeClockEstimator& Get();
};

/** Spike: the ADR-0002 section 9 clock exchange on a PlayerController (added by the S10 re-run scenario on the server). */
UCLASS()
class UStealthSpikeClockComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStealthSpikeClockComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Precise server clock (ADR-0002 section 9, E8): TimeSeconds + (FPlatformTime::Seconds() - frame start), evaluated
	 * through an anchor taken after the frame's time update, so a stamp inside TickDispatch (an RPC body, which runs
	 * before UWorld advances TimeSeconds) is not one frame early.
	 */
	static double ServerNowPrecise(const UWorld* World);

	/** Server: records the frame's anchor (call from a tick, after the world's time update). */
	static void NoteServerFrame(const UWorld* World);

	UFUNCTION(Server, Unreliable)
	void ServerClockPing(double T1);

	UFUNCTION(Client, Unreliable)
	void ClientClockPong(double T1, double T2, double T3);

private:
	/** Server: pongs to send in the next tick (T1, T2). */
	TArray<TPair<double, double>> PendingPongs;
	/** Server: ping receipt times inside the last second (rate limit). */
	TArray<double> RecentPings;
	/** Client: when the controller began play (ping cadence) and when the last ping went out. */
	double ClientBeganAt = -1.0;
	double LastPingAt = -100.0;
	double LastRecordAt = -100.0;
	int32 DroppedPings = 0;
};
