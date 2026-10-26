#pragma once

// The two RPC spies of the pawn command interface tests (story iif-095, ADR-0011 Validation Criteria):
//   FReplyTransportSpy   an IReplyTransport that records each ClientCommandReplies batch and whether it was sent inside
//                        a queue slot (the caller supplies the "in slot" probe), for story iif-004 and AC 47.
//   FOutgoingRpcSpy      records every RPC a world's NetDriver sends (UNetDriver::SendRPCDel, non-Shipping): on a client
//                        world that is every client-to-server RPC, so a test can assert "zero RPCs" (AC 29) or "no RPC
//                        other than the ServerMove family" (AC 76). It observes only: the RPC is never blocked or altered.
// Harness-only (the module is excluded from Shipping and Test).

#include "CoreMinimal.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Server/Core/ReplySink.h"

/** Records every reply batch; CallsInsideSlot counts batches sent while the probe said a slot was open. */
class FReplyTransportSpy : public IReplyTransport
{
public:
	struct FCall
	{
		FReplySinkId Sink;
		TArray<FCommandReply> Replies;
		bool bInsideSlot = false;
	};

	explicit FReplyTransportSpy(TFunction<bool()> InInSlotProbe = TFunction<bool()>()) : InSlotProbe(MoveTemp(InInSlotProbe)) {}

	virtual void SendCommandReplies(FReplySinkId Sink, const TArray<FCommandReply>& Replies) override
	{
		const bool bInside = InSlotProbe ? InSlotProbe() : false;
		Calls.Add({ Sink, Replies, bInside });
		CallsInsideSlot += bInside ? 1 : 0;
	}

	TArray<FCall> Calls;
	int32 CallsInsideSlot = 0;

private:
	TFunction<bool()> InSlotProbe;
};

/** Records the RPCs a world's NetDriver sends while the spy is alive (restores any previous binding on destruction). */
class FOutgoingRpcSpy
{
public:
	struct FRecord
	{
		FName Function;
		FName ActorClass;
		int32 ParmsSize = 0;
	};

	explicit FOutgoingRpcSpy(UWorld* World)
	{
		Driver = World ? World->GetNetDriver() : nullptr;
		if (UNetDriver* D = Driver.Get())
		{
			Previous = D->SendRPCDel;
			D->SendRPCDel.BindRaw(this, &FOutgoingRpcSpy::OnSend);
		}
	}

	~FOutgoingRpcSpy()
	{
		if (UNetDriver* D = Driver.Get())
		{
			D->SendRPCDel = Previous;
		}
	}

	FOutgoingRpcSpy(const FOutgoingRpcSpy&) = delete;
	FOutgoingRpcSpy& operator=(const FOutgoingRpcSpy&) = delete;

	/** True when the spy is attached to a NetDriver. */
	bool IsAttached() const { return Driver.IsValid(); }

	/** Records whose function name is not in Allowed (e.g. the ServerMove family). */
	TArray<FRecord> Excluding(TFunctionRef<bool(FName)> Allowed) const
	{
		return Records.FilterByPredicate([&Allowed](const FRecord& R) { return !Allowed(R.Function); });
	}

	/** True for the CMC move RPC family (ServerMove, ServerMovePacked, ServerMoveNoBase, ...). */
	static bool IsServerMoveFamily(FName Function)
	{
		return Function.ToString().StartsWith(TEXT("ServerMove"));
	}

	/**
	 * The engine's own periodic client-to-server traffic of a possessed player: the CMC move family and
	 * APlayerController::ServerUpdateCamera (client-side camera updates, which ADR-0015 turns off with
	 * bUseClientSideCameraUpdates = false when the camera epic lands). Neither is a gameplay RPC.
	 */
	static bool IsEngineBackground(FName Function)
	{
		return IsServerMoveFamily(Function) || Function == TEXT("ServerUpdateCamera");
	}

	/** The function names of Records, joined (test messages). */
	static FString Describe(const TArray<FRecord>& In)
	{
		return FString::JoinBy(In, TEXT(", "), [](const FRecord& R) { return R.Function.ToString(); });
	}

	TArray<FRecord> Records;

private:
	void OnSend(AActor* Actor, UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack, UObject* SubObject, bool& bBlockSendRPC)
	{
		Records.Add({ Function ? Function->GetFName() : NAME_None, Actor ? Actor->GetClass()->GetFName() : NAME_None, Function ? static_cast<int32>(Function->ParmsSize) : 0 });
		// Observation only: never block.
		if (Previous.IsBound())
		{
			Previous.Execute(Actor, Function, Parameters, OutParms, Stack, SubObject, bBlockSendRPC);
		}
	}

	TWeakObjectPtr<UNetDriver> Driver;
	FOnSendRPC Previous;
};
