#pragma once

// Shared T2 support for StealthTestHarness CQTest network tests (ADR-0008 section 1; story tdc-019).
// Editor-only (CQTest's PIE network tests need WITH_EDITOR), so it lives in the harness, not under
// StealthFoundation/Public/Server/Testing/ (Foundation cannot depend on CQTest).

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Components/PIENetworkComponent.h"
#endif

#if defined(ENABLE_PIE_NETWORK_TEST) && ENABLE_PIE_NETWORK_TEST

#include "FileHelpers.h"
#include "Misc/PackageName.h"

/**
 * FPIENetworkComponent that starts PIE on a fixture map instead of a new empty map.
 *
 * ADR-0008 T2 tests must load their fixture map, but the stock component always runs
 * "Stop PIE -> Create New Map -> Start PIE -> ...". Construct this component (its stock chain is
 * disabled), build it with FNetworkComponentBuilder as usual, then call StartOnFixtureMap from
 * BEFORE_EACH: it queues the same public steps with "load fixture map" in place of "create new
 * map", plus the stock teardown. Every wait is a condition; the wall-clock ceiling is
 * TestFramework.CQTest.CommandTimeout.Network.
 */
template <typename NetworkDataType = FBasePIENetworkComponentState>
class TStealthPIENetworkComponent : public FPIENetworkComponent<NetworkDataType>
{
public:
	using Super = FPIENetworkComponent<NetworkDataType>;

	/** Same arguments as FPIENetworkComponent; the stock start chain is never queued. */
	TStealthPIENetworkComponent(FAutomationTestBase* InTestRunner, FTestCommandBuilder& InCommandBuilder)
		: Super(InTestRunner, InCommandBuilder, /*IsInitializing: suppress the stock chain*/ true)
	{
	}

	/**
	 * Queues: stop PIE, load MapPackage (a long package name such as /Game/Maps/L_Fixture), start PIE,
	 * wait for the worlds, apply packet settings, match connections, wait for every client to be
	 * ready; restores the editor state on teardown. Call after FNetworkComponentBuilder::Build.
	 */
	TStealthPIENetworkComponent& StartOnFixtureMap(const FString& MapPackage)
	{
		this->CommandBuilder->OnTearDown(TEXT("Restore Editor State"), [this]() { this->RestoreState(); });
		Super::Do(TEXT("Stop PIE"), [this]() { this->StopPie(); })
			.Then(TEXT("Load fixture map"), [MapPackage]()
			{
				const FString File = FPackageName::LongPackageNameToFilename(MapPackage, FPackageName::GetMapPackageExtension());
				FEditorFileUtils::LoadMap(File, /*bLoadAsTemplate*/ false, /*bShowProgress*/ false);
			})
			.Then(TEXT("Start PIE"), [this]() { this->StartPie(); })
			.Until(TEXT("Set Worlds"), [this]() { return this->SetWorlds(); })
			.Then(TEXT("Setup Packet Settings"), [this]() { this->SetPacketSettings(); })
			.Then(TEXT("Connect Clients to Server"), [this]() { this->ConnectClientsToServer(); })
			.Until(TEXT("Await Clients Ready"), [this]() { return this->AwaitClientsReady(); });
		return *this;
	}
};

#endif // ENABLE_PIE_NETWORK_TEST
