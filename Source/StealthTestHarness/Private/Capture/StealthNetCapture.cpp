// Raw-bunch capture (ADR-0008 section 6). See StealthNetCapture.h.
#include "Capture/StealthNetCapture.h"

#include "Scenarios/StealthHarnessRun.h"
#include "StealthTestHarnessModule.h"

#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Net/DataBunch.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StealthNetCapture)

namespace StealthHarness::Capture
{
	/** One replicated object (actor or subobject) inside a bunch: its class path (the engine's "Owner=") and parent indices in order. */
	struct FObjectEntry
	{
		FString Owner;
		TArray<int32> Parents;
	};

	/** State for one bunch being received or written. */
	struct FBlock
	{
		bool bActive = false;
		FString ActorName;
		int32 bNetInitial = -1;
		int32 bNetOwner = -1;
		TArray<FObjectEntry> Objects;
		TArray<FString> Rpcs;

		void Reset()
		{
			bActive = false;
			ActorName.Reset();
			bNetInitial = -1;
			bNetOwner = -1;
			Objects.Reset();
			Rpcs.Reset();
		}
	};

	/** Last non-property send (an RPC or a close), waiting for the engine's "Sent RPC" line. */
	struct FLastSend
	{
		bool bValid = false;
		FString Conn;
		FString Actor;
	};

	static bool GInstalled = false;
	static FBlock GReceive;   // client: inside UStealthCaptureActorChannel::ReceivedBunch
	static FBlock GPendingTx; // server: since the last "Replicate <actor>" line
	static FLastSend GLastSend;
	static const FName NAME_LogRepProperties(TEXT("LogRepProperties"));
	static const FName NAME_LogNetTraffic(TEXT("LogNetTraffic"));
	static const FName NAME_LogRepTraffic(TEXT("LogRepTraffic"));

	static int32 ParseIntAfter(const TCHAR* Line, const TCHAR* Key)
	{
		const TCHAR* Found = FCString::Strstr(Line, Key);
		return Found ? FCString::Atoi(Found + FCString::Strlen(Key)) : INDEX_NONE;
	}

	static void AddParent(FBlock& Block, int32 Parent)
	{
		if (Parent == INDEX_NONE)
		{
			return;
		}
		if (Block.Objects.Num() == 0)
		{
			Block.Objects.AddDefaulted();
		}
		TArray<int32>& Parents = Block.Objects.Last().Parents;
		if (Parents.Num() == 0 || Parents.Last() != Parent)
		{
			Parents.Add(Parent); // struct members and array elements repeat their parent: keep one entry per run
		}
	}

	/** Collects the engine's replication log points into the open block (game thread only). */
	class FCollectorDevice final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (!IsInGameThread() || V == nullptr)
			{
				return;
			}
			if (Category == NAME_LogRepProperties)
			{
				if (FCString::Strstr(V, TEXT("ReceiveProperties: Owner=")) && GReceive.bActive)
				{
					GReceive.Objects.AddDefaulted_GetRef().Owner = FCString::Strstr(V, TEXT("Owner=")) + 6;
				}
				else if (FCString::Strstr(V, TEXT("ReceiveProperties_r: Parent=")) && GReceive.bActive)
				{
					AddParent(GReceive, ParseIntAfter(V, TEXT("Parent=")));
				}
				else if (FCString::Strstr(V, TEXT("SendProperties: Owner=")) && GPendingTx.bActive)
				{
					FString Owner = FCString::Strstr(V, TEXT("Owner=")) + 6;
					int32 Comma = INDEX_NONE;
					if (Owner.FindChar(TEXT(','), Comma))
					{
						Owner.LeftInline(Comma);
					}
					GPendingTx.Objects.AddDefaulted_GetRef().Owner = MoveTemp(Owner);
				}
				else if (FCString::Strstr(V, TEXT("SendProperties_r: Parent=")) && GPendingTx.bActive)
				{
					AddParent(GPendingTx, ParseIntAfter(V, TEXT("Parent=")));
				}
				return;
			}
			if (Category == NAME_LogNetTraffic)
			{
				// "Replicate %s, bNetInitial: %d, bNetOwner: %d" (UActorChannel::ReplicateActor, DataChannel.cpp)
				if (FCString::Strncmp(V, TEXT("Replicate "), 10) == 0)
				{
					GPendingTx.Reset();
					GPendingTx.bActive = true;
					FString Rest = V + 10;
					int32 Comma = INDEX_NONE;
					if (Rest.FindChar(TEXT(','), Comma))
					{
						GPendingTx.ActorName = Rest.Left(Comma);
					}
					GPendingTx.bNetInitial = ParseIntAfter(V, TEXT("bNetInitial: "));
					GPendingTx.bNetOwner = ParseIntAfter(V, TEXT("bNetOwner: "));
				}
				else if (const TCHAR* Sent = FCString::Strstr(V, TEXT("Sent RPC: ")))
				{
					FRun* Run = FRun::Get();
					if (Run && GLastSend.bValid)
					{
						Run->Write(TEXT("txrpc"), FString::Printf(TEXT("\"conn\":%s,\"actor\":%s,\"fn\":%s"),
							*FRun::Q(GLastSend.Conn), *FRun::Q(GLastSend.Actor), *FRun::Q(FString(Sent + 10).TrimStartAndEnd())));
					}
					GLastSend.bValid = false;
				}
				return;
			}
			if (Category == NAME_LogRepTraffic && GReceive.bActive)
			{
				if (const TCHAR* Received = FCString::Strstr(V, TEXT("Received RPC: ")))
				{
					GReceive.Rpcs.Add(FString(Received + 14).TrimStartAndEnd());
				}
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }
		virtual bool CanBeUsedOnMultipleThreads() const override { return true; }
	};

	static TUniquePtr<FCollectorDevice> GDevice;

	static FString ObjectsJson(const FBlock& Block)
	{
		FString Out = TEXT("[");
		for (int32 Index = 0; Index < Block.Objects.Num(); ++Index)
		{
			const FObjectEntry& Entry = Block.Objects[Index];
			FString Parents;
			for (int32 P = 0; P < Entry.Parents.Num(); ++P)
			{
				Parents += FString::Printf(TEXT("%s%d"), P ? TEXT(",") : TEXT(""), Entry.Parents[P]);
			}
			Out += FString::Printf(TEXT("%s{\"o\":%s,\"p\":[%s]}"), Index ? TEXT(",") : TEXT(""), *FRun::Q(Entry.Owner), *Parents);
		}
		return Out + TEXT("]");
	}

	static void DumpOwnerLayouts(FRun& Run, const FBlock& Block)
	{
		for (const FObjectEntry& Entry : Block.Objects)
		{
			if (!Entry.Owner.IsEmpty())
			{
				// The engine logs the replicated object's class path (FRepLayout owner), not the instance path.
				if (const UObject* Object = StaticFindObject(UObject::StaticClass(), nullptr, *Entry.Owner))
				{
					const UClass* AsClass = Cast<UClass>(Object);
					Run.DumpLayoutOnce(AsClass ? AsClass : Object->GetClass());
				}
			}
		}
	}

	void BeginReceive()
	{
		GReceive.Reset();
		GReceive.bActive = GInstalled;
	}

	void EndReceive(UStealthCaptureActorChannel* Channel, int64 Bits, bool bOpen, bool bClose, bool bReliable)
	{
		FRun* Run = FRun::Get();
		if (Run && GReceive.bActive)
		{
			const AActor* Actor = Channel->Actor;
			DumpOwnerLayouts(*Run, GReceive);
			FString Rpcs;
			for (int32 Index = 0; Index < GReceive.Rpcs.Num(); ++Index)
			{
				Rpcs += FString::Printf(TEXT("%s%s"), Index ? TEXT(",") : TEXT(""), *FRun::Q(GReceive.Rpcs[Index]));
			}
			Run->Write(TEXT("rx"), FString::Printf(
				TEXT("\"ch\":%d,\"actor\":%s,\"cls\":%s,\"guid\":%s,\"bits\":%lld,\"open\":%d,\"close\":%d,\"rel\":%d,\"objs\":%s,\"rpc\":[%s]"),
				Channel->ChIndex, *FRun::Q(GetNameSafe(Actor)), *FRun::Q(Actor ? Actor->GetClass()->GetName() : FString()),
				*FRun::Q(Channel->ActorNetGUID.ToString()), Bits, (int32)bOpen, (int32)bClose, (int32)bReliable, *ObjectsJson(GReceive), *Rpcs));
		}
		GReceive.Reset();
	}

	void OnSend(UStealthCaptureActorChannel* Channel, const FOutBunch* Bunch)
	{
		FRun* Run = FRun::Get();
		if (!Run || !GInstalled || Bunch == nullptr)
		{
			return;
		}
		const AActor* Actor = Channel->Actor;
		const FString Conn = FRun::ConnectionName(Channel->Connection);
		const FString ActorName = GetNameSafe(Actor);
		if (GPendingTx.bActive && GPendingTx.ActorName == ActorName)
		{
			DumpOwnerLayouts(*Run, GPendingTx);
			Run->Write(TEXT("tx"), FString::Printf(
				TEXT("\"conn\":%s,\"ch\":%d,\"actor\":%s,\"cls\":%s,\"guid\":%s,\"bits\":%lld,\"open\":%d,\"close\":%d,\"rel\":%d,\"init\":%d,\"own\":%d,\"objs\":%s"),
				*FRun::Q(Conn), Channel->ChIndex, *FRun::Q(ActorName), *FRun::Q(Actor ? Actor->GetClass()->GetName() : FString()),
				*FRun::Q(Channel->ActorNetGUID.ToString()), Bunch->GetNumBits(), (int32)Bunch->bOpen, (int32)Bunch->bClose, (int32)Bunch->bReliable,
				GPendingTx.bNetInitial, GPendingTx.bNetOwner, *ObjectsJson(GPendingTx)));
			GPendingTx.Reset();
			GLastSend.bValid = false;
			return;
		}
		// An RPC or a close bunch: the engine logs "Sent RPC" right after SendBunch returns.
		GLastSend.bValid = true;
		GLastSend.Conn = Conn;
		GLastSend.Actor = ActorName;
		if (Bunch->bClose)
		{
			Run->Write(TEXT("txclose"), FString::Printf(TEXT("\"conn\":%s,\"ch\":%d,\"actor\":%s,\"guid\":%s,\"bits\":%lld"),
				*FRun::Q(Conn), Channel->ChIndex, *FRun::Q(ActorName), *FRun::Q(Channel->ActorNetGUID.ToString()), Bunch->GetNumBits()));
		}
	}

	void Install()
	{
		if (GInstalled)
		{
			return;
		}
		const FName CaptureClassName(*UStealthCaptureActorChannel::StaticClass()->GetPathName());
		FArrayProperty* DefinitionsProperty = FindFProperty<FArrayProperty>(UNetDriver::StaticClass(), TEXT("ChannelDefinitions"));
		int32 Patched = 0;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			if (!It->IsChildOf(UNetDriver::StaticClass()) || It->HasAnyClassFlags(CLASS_Abstract) || DefinitionsProperty == nullptr)
			{
				continue;
			}
			UNetDriver* Default = It->GetDefaultObject<UNetDriver>();
			TArray<FChannelDefinition>* Definitions = DefinitionsProperty->ContainerPtrToValuePtr<TArray<FChannelDefinition>>(Default);
			for (FChannelDefinition& Definition : *Definitions)
			{
				if (Definition.ChannelName == NAME_Actor)
				{
					Definition.ClassName = CaptureClassName;
					Definition.ChannelClass = UStealthCaptureActorChannel::StaticClass();
					++Patched;
				}
			}
		}
		GDevice = MakeUnique<FCollectorDevice>();
		GLog->AddOutputDevice(GDevice.Get());
		GInstalled = true;
		UE_LOGF(LogStealthHarness, Display, "Net capture installed: Actor channel -> %ls on %d net driver class defaults.", *CaptureClassName.ToString(), Patched);
		if (FRun* Run = FRun::Get())
		{
			Run->Write(TEXT("capture"), FString::Printf(TEXT("\"channelClass\":%s,\"patchedDefaults\":%d"), *FRun::Q(CaptureClassName.ToString()), Patched));
		}
	}

	void Uninstall()
	{
		if (GDevice.IsValid())
		{
			GLog->RemoveOutputDevice(GDevice.Get());
			GDevice.Reset();
		}
		GInstalled = false;
	}

	bool IsInstalled()
	{
		return GInstalled;
	}

	/**
	 * Only a process launched with -StealthNetCapture installs the capture: after engine init (every net
	 * driver class is loaded, no driver exists yet) and it is removed before exit. Nothing else registers.
	 */
	static struct FAutoInstall
	{
		FAutoInstall()
		{
			if (FParse::Param(FCommandLine::Get(), TEXT("StealthNetCapture")))
			{
				FCoreDelegates::GetOnPostEngineInit().AddStatic(&Install);
				FCoreDelegates::OnPreExit.AddStatic(&Uninstall);
			}
		}
	} GAutoInstall;
}

UStealthCaptureActorChannel::UStealthCaptureActorChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UStealthCaptureActorChannel::ReceivedBunch(FInBunch& Bunch)
{
	const int64 Bits = Bunch.GetNumBits();
	const bool bOpen = Bunch.bOpen;
	const bool bClose = Bunch.bClose;
	const bool bReliable = Bunch.bReliable;
	StealthHarness::Capture::BeginReceive();
	Super::ReceivedBunch(Bunch);
	StealthHarness::Capture::EndReceive(this, Bits, bOpen, bClose, bReliable);
}

FPacketIdRange UStealthCaptureActorChannel::SendBunch(FOutBunch* Bunch, bool Merge)
{
	StealthHarness::Capture::OnSend(this, Bunch);
	return Super::SendBunch(Bunch, Merge);
}
