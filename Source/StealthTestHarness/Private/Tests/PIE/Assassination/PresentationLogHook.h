#pragma once

// Story asn-017 (production/epics/assassination-defence-reversal/story-017-no-warning-and-no-observer-cue.md): the
// harness's per-machine presentation log (ADR-0008; WITH_STEALTH_TEST_SEAMS builds only, never Shipping). On one PIE
// machine it records, stamped with that machine's EstimatedServerNow():
//   Presentation.<Event>   UEncounterPresentationSubsystem rows (clips, warp, collision, removal, role change, grade) of the
//                          pawns the namer names (the victim's own pawn: its montage and pose presentation);
//   Hud.<Event>            the local controller's owner presentation log (plates, tally, cards, brace square, HUD lines,
//                          round end): the HUD view-model;
//   Widget.+ / Widget.-    every change of the HUD element list (AStealthHUD::CollectOwnerElements), sampled per frame;
//   Camera.Grade / .Role   UEncounterViewSubsystem calls (the camera parameter set and the own-role clock);
//   OwnPawn.Montage/.Crouch the own pawn's active montage and crouch pose (AnimBP inputs), sampled per frame.
// There is no owner audio bus and no haptics in the build yet: nothing to record for them (stated, not asserted away).
// FReplicatedPropertyWatcher is the client-receipt side of the send capture: every replicated property of one actor copy
// (and its replicated components), hashed per frame, with the receipt time of each change.

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_STEALTH_TEST_SEAMS

class AActor;
class AStealthPawn;

namespace StealthPresentationLog
{
	/** One presentation change on one machine. */
	struct FEntry
	{
		FString Source;
		FString Key;
		double ServerNow = 0.0;
	};

	/** Names a pawn of the recording machine for the comparison; an empty name leaves its rows out. */
	using FPawnNamer = TFunction<FString(const AStealthPawn* /*the pawn on the recording machine*/)>;

	/** The recorder of one machine (index into StealthAssassinationPie::Worlds()). */
	class FPresentationLogHook
	{
	public:
		/** Starts recording on Machine from its server time now. */
		void Begin(int32 InMachine, FPawnNamer InNamer);

		/** One frame (call every test frame while recording): the element-list diffs and the own-pawn pose changes. */
		void Sample();

		/** Stops the recording at the machine's server time now (Collect returns nothing later). */
		void End();

		/** Every entry from Begin to End (or now), by time. */
		TArray<FEntry> Collect() const;

		int32 GetMachine() const { return Machine; }
		double GetBeginServerTime() const { return BeginServerTime; }

	private:
		int32 Machine = INDEX_NONE;
		FPawnNamer Namer;
		double BeginServerTime = 0.0;
		double EndServerTime = TNumericLimits<double>::Max();
		bool bStarted = false;
		TArray<FString> LastElements;
		FString LastMontage;
		bool bLastCrouched = false;
		TArray<FEntry> Sampled;
	};

	/**
	 * The entries of Struck whose time relative to StruckOrigin lies in [From, To) and that Control lacks: no Control entry
	 * with the same Source and Key within Tolerance of the same time relative to ControlOrigin.
	 */
	TArray<FEntry> Unmatched(const TArray<FEntry>& Struck, double StruckOrigin, const TArray<FEntry>& Control, double ControlOrigin, double From, double To, double Tolerance);

	/** Formats entries for the test log. */
	FString Describe(const TArray<FEntry>& Entries, double Origin);

	/** One received change of a replicated property. */
	struct FPropertyChange
	{
		/** "<Owner class>.<Property>" (a component's properties under its class). */
		FString Property;
		double ServerNow = 0.0;
	};

	/** The client-receipt half of the send capture for one actor copy on one machine. */
	class FReplicatedPropertyWatcher
	{
	public:
		/** Starts watching Copy on Machine (its values now are the baseline). */
		void Begin(AActor* Copy, int32 InMachine);

		/** One frame: records every property whose value changed since the last frame. */
		void Sample();

		const TArray<FPropertyChange>& GetChanges() const { return Changes; }

		/** ReplicatedMovement and the character movement's replicated support fields (the "movement and CMC acks" set). */
		static bool IsMovementProperty(const FString& Property);

	private:
		void Snapshot(TMap<FString, FString>& Out) const;

		TWeakObjectPtr<AActor> Actor;
		int32 Machine = INDEX_NONE;
		TMap<FString, FString> Last;
		TArray<FPropertyChange> Changes;
	};
}

#endif
