// Story iif-010 (production/epics/input-interaction-framework/story-010-spike-v3-netguid-server-resolution.md):
// ADR-0011 verification item V3. A THROWAWAY probe harness, not a gate test (named under StealthSpike., outside the
// per-commit ^StealthGame. filter); run on demand:
//   UnrealEditor-Cmd.exe StealthGame.uproject -ExecCmds="Automation RunTests StealthSpike.Input.PIE.SpikeV3; Quit" -unattended -nullrhi
// Listen server + 1 client on the fixture map (legacy replication, net.Iris.UseIrisReplication=0). The server resolves,
// through FNetGUIDCache::GetObjectFromNetGUID(Guid, bIgnoreMustBeMapped = false) (the function IngestCommand's
// ResolvePawnRefGuid uses), five GUID cases for the test connection:
//   (a) the GUID the server assigned to a pawn relevant to the connection;
//   (b) the GUID of a pawn culled for the connection (IsNetRelevantFor false through the IVP S9 spike hook until
//       ADR-0003's cull exists; the channel closes, the GUID stays in the server's cache);
//   (c) a never-assigned GUID;  (d) the stale GUID of a pawn destroyed earlier;
//   (e) GUIDs with a bit pattern the server never assigns (the default GUID, ObjectId 1; and an unassigned static GUID).
// Two answers are recorded per case: the raw engine call, and ResolvePawnRefGuid (the ingress resolver, which after the
// first run refuses the default GUID and torn-down actors). The verdict is the ingress resolver's.
// Pass rule: (a) and (b) resolve to the pawn; (c), (d), (e) resolve to null with no crash and no log error above
// Warning. Results go to Saved/Spikes/V3/v3-results.json and V3RESULT log lines; the report is in production/qa/spikes/.

#include "CoreMinimal.h"
#include "Tests/PIE/StealthPIENetworkComponent.h"

#if WITH_DEV_AUTOMATION_TESTS && defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "CQTest.h"
#include "Engine/ActorChannel.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Paths.h"
#include "Server/Core/CommandIngress.h"
#include "Server/Spikes/StealthSpikeHooks.h"
#include "StealthGameMode.h"
#include "View/Core/StealthPawn.h"

namespace SpikeV3
{
	const TCHAR* const FixtureMap = TEXT("/Game/StealthGame/Maps/L_Prototype");
	constexpr uint64 SettleFrames = 90;

	/** Counts log lines above Warning while alive. */
	class FErrorCounter : public FOutputDevice
	{
	public:
		FErrorCounter() { GLog->AddOutputDevice(this); }
		virtual ~FErrorCounter() override { GLog->RemoveOutputDevice(this); }
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if (Verbosity <= ELogVerbosity::Error)
			{
				++Errors;
			}
		}
		int32 Errors = 0;
	};

	struct FCase
	{
		FString Name;
		uint64 Guid = 0;
		FString Resolved;
		/** The raw engine call's answer (FNetGUIDCache::GetObjectFromNetGUID, no project guard). */
		FString Raw;
		bool bExpectPawn = false;
		bool bPass = false;
	};
}

NETWORK_TEST_CLASS(SpikeV3NetGuidServerResolution, "StealthSpike.Input.PIE")
{
	TStealthPIENetworkComponent<FBasePIENetworkComponentState> Network{ TestRunner, TestCommandBuilder };

	TWeakObjectPtr<AStealthPawn> Relevant;
	TWeakObjectPtr<AStealthPawn> Culled;
	TWeakObjectPtr<AStealthPawn> Doomed;
	FNetworkGUID RelevantGuid;
	FNetworkGUID CulledGuid;
	FNetworkGUID DoomedGuid;
	bool bCulledChannelClosed = false;
	uint64 Mark = 0;
	TArray<SpikeV3::FCase> Cases;
	TUniquePtr<SpikeV3::FErrorCounter> Errors;

	BEFORE_EACH()
	{
		FNetworkComponentBuilder<FBasePIENetworkComponentState>()
			.AsListenServer()
			.WithClients(1)
			.WithGameMode(AStealthGameMode::StaticClass())
			.Build(Network);
		Network.StartOnFixtureMap(SpikeV3::FixtureMap);
	}

	AFTER_EACH()
	{
		StealthSpikeHooks::SetRelevancyOverride(StealthSpikeHooks::FRelevancyOverride());
	}

	UNetConnection* ClientConnection(UWorld* ServerWorld) const
	{
		const UNetDriver* Driver = ServerWorld->GetNetDriver();
		return Driver && Driver->ClientConnections.Num() > 0 ? Driver->ClientConnections[0] : nullptr;
	}

	TEST_METHOD(FiveGuidCasesResolveAsRecorded)
	{
		using namespace SpikeV3;
		Network
			.UntilServer(TEXT("pawns replicated to the client"), [this](FBasePIENetworkComponentState& Server)
			{
				UNetConnection* Connection = ClientConnection(Server.World);
				if (!Connection)
				{
					return false;
				}
				// Three crowd pawns with an open channel to the test connection.
				TArray<AStealthPawn*> Open;
				for (TActorIterator<AStealthPawn> It(Server.World); It && Open.Num() < 3; ++It)
				{
					if (It->GetController() && !It->GetController()->IsA<APlayerController>() && Connection->FindActorChannelRef(*It))
					{
						Open.Add(*It);
					}
				}
				if (Open.Num() < 3)
				{
					return false;
				}
				Relevant = Open[0];
				Culled = Open[1];
				Doomed = Open[2];
				const TSharedPtr<FNetGUIDCache>& Cache = Server.World->GetNetDriver()->GetNetGuidCache();
				RelevantGuid = Cache->GetNetGUID(Relevant.Get());
				CulledGuid = Cache->GetNetGUID(Culled.Get());
				DoomedGuid = Cache->GetNetGUID(Doomed.Get());
				return RelevantGuid.IsValid() && CulledGuid.IsValid() && DoomedGuid.IsValid();
			})
			.ThenServer(TEXT("cull one pawn for the connection; destroy another"), [this](FBasePIENetworkComponentState& Server)
			{
				TWeakObjectPtr<AStealthPawn> Target = Culled;
				StealthSpikeHooks::SetRelevancyOverride([Target](const AActor* Subject, const AActor*, const AActor*) -> TOptional<bool>
				{
					return Subject == Target.Get() ? TOptional<bool>(false) : TOptional<bool>();
				});
				Doomed->Destroy();
				Mark = GFrameCounter;
			})
			.UntilServer(TEXT("the culled pawn's channel closes"), [this](FBasePIENetworkComponentState& Server)
			{
				UNetConnection* Connection = ClientConnection(Server.World);
				bCulledChannelClosed = Connection && Culled.IsValid() && Connection->FindActorChannelRef(Culled.Get()) == nullptr;
				return bCulledChannelClosed || GFrameCounter > Mark + 900;
			})
			.UntilServer(TEXT("settle"), [this](FBasePIENetworkComponentState&) { return GFrameCounter >= Mark + SettleFrames; })
			.ThenServer(TEXT("resolve the five cases"), [this](FBasePIENetworkComponentState& Server)
			{
				Errors = MakeUnique<FErrorCounter>();
				FNetworkGUID Never = FNetworkGUID::CreateFromIndex(0x7FFFFFF0ull, false);
				FNetworkGUID StaticUnassigned = FNetworkGUID::CreateFromIndex(0x7FFFFFF1ull, true);
				struct FIn { const TCHAR* Name; FNetworkGUID Guid; const AStealthPawn* Expect; };
				const FIn In[] = {
					{ TEXT("a relevant pawn"), RelevantGuid, Relevant.Get() },
					{ TEXT("b culled pawn (channel closed)"), CulledGuid, Culled.Get() },
					{ TEXT("c never-assigned GUID"), Never, nullptr },
					{ TEXT("d stale GUID of a destroyed pawn"), DoomedGuid, nullptr },
					{ TEXT("e1 default GUID (ObjectId 1)"), FNetworkGUID::GetDefault(), nullptr },
					{ TEXT("e2 unassigned static GUID"), StaticUnassigned, nullptr },
				};
				for (const FIn& Case : In)
				{
					const APawn* Got = ResolvePawnRefGuid(Server.World, Case.Guid);
					FCase Out;
					if (Case.Guid.IsDefault())
					{
						// Not called raw: the first run (2026-10-26) showed the engine ensures on the default GUID
						// ("Ensure condition failed: !NetGUID.IsDefault()", PackageMapClient.cpp:3745) and returns null.
						Out.Raw = TEXT("ensure + null (first run)");
					}
					else
					{
						const UObject* RawObject = Server.World->GetNetDriver()->GetNetGuidCache()->GetObjectFromNetGUID(Case.Guid, false);
						const AActor* RawActor = Cast<AActor>(RawObject);
						Out.Raw = RawObject ? FString::Printf(TEXT("%s%s"), *RawObject->GetName(), RawActor && RawActor->IsActorBeingDestroyed() ? TEXT(" (being destroyed)") : TEXT("")) : FString(TEXT("null"));
					}
					Out.Name = Case.Name;
					Out.Guid = Case.Guid.ObjectId;
					Out.Resolved = Got ? Got->GetName() : FString(TEXT("null"));
					Out.bExpectPawn = Case.Expect != nullptr;
					Out.bPass = Case.Expect ? Got == Case.Expect : Got == nullptr;
					Cases.Add(Out);
				}
			})
			.ThenServer(TEXT("report"), [this](FBasePIENetworkComponentState& Server)
			{
				const int32 ErrorLines = Errors ? Errors->Errors : -1;
				Errors.Reset();
				bool bAll = ErrorLines == 0;
				FString Json = FString::Printf(TEXT("{\n  \"engine\": \"%s\",\n  \"function\": \"FNetGUIDCache::GetObjectFromNetGUID(Guid, bIgnoreMustBeMapped=false)\",\n  \"culling\": \"IsNetRelevantFor false via StealthSpikeHooks (IVP S9 hook)\",\n  \"culledChannelClosed\": %s,\n  \"errorLinesAboveWarning\": %d,\n  \"cases\": [\n"),
					*FEngineVersion::Current().ToString(), bCulledChannelClosed ? TEXT("true") : TEXT("false"), ErrorLines);
				for (int32 Index = 0; Index < Cases.Num(); ++Index)
				{
					const FCase& C = Cases[Index];
					bAll &= C.bPass;
					Json += FString::Printf(TEXT("    {\"case\": \"%s\", \"guid\": %llu, \"raw\": \"%s\", \"resolved\": \"%s\", \"expect\": \"%s\", \"pass\": %s}%s\n"), *C.Name, C.Guid, *C.Raw, *C.Resolved,
						C.bExpectPawn ? TEXT("pawn") : TEXT("null"), C.bPass ? TEXT("true") : TEXT("false"), Index + 1 < Cases.Num() ? TEXT(",") : TEXT(""));
					TestRunner->AddInfo(FString::Printf(TEXT("V3RESULT %s | guid %llu | raw %s | resolved %s | %s"), *C.Name, C.Guid, *C.Raw, *C.Resolved, C.bPass ? TEXT("PASS") : TEXT("FAIL")));
				}
				Json += FString::Printf(TEXT("  ],\n  \"overall\": \"%s\"\n}\n"), bAll ? TEXT("PASS") : TEXT("FAIL"));
				FFileHelper::SaveStringToFile(Json, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Spikes/V3/v3-results.json")));
				TestRunner->AddInfo(FString::Printf(TEXT("V3 overall %s (culled channel closed: %d, error lines: %d)"), bAll ? TEXT("PASS") : TEXT("FAIL"), bCulledChannelClosed ? 1 : 0, ErrorLines));
				ASSERT_THAT(IsTrue(Cases.Num() == 6, TEXT("the harness resolved every case")));
			});
	}
};

#endif
