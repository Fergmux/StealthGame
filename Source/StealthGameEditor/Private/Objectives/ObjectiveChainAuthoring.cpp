// Objectives Chain story oc-002: the MVP chain asset, authored in the editor from code (never a hand-edited .uasset) so it can
// be rebuilt after any schema change.
//   Stealth.Objectives.BuildCacaoChain   creates or refreshes /Game/StealthGame/Objectives/DA_ObjectiveChain_Cacao from the
//                                        OC-16 fixture (View/Testing/ObjectiveCacaoChainFixture.h) and saves exactly that
//                                        package (never another asset; production/never-resave-assets.md).
// Summary line: "STEALTH OBJECTIVES CHAIN: PASS|FAIL <file>".

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "View/ObjectiveChainData.h"
#include "View/Testing/ObjectiveCacaoChainFixture.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthObjectivesAssets, Log, All);

namespace StealthObjectivesAuthoring
{
	void BuildCacaoChain()
	{
		const FString PackageName = FString::Printf(TEXT("%s/%s"), StealthObjectivesCacaoFixture::AssetFolder, StealthObjectivesCacaoFixture::AssetName);
		UPackage* Package = FPackageName::DoesPackageExist(PackageName) ? LoadPackage(nullptr, *PackageName, LOAD_None) : CreatePackage(*PackageName);
		if (Package == nullptr)
		{
			UE_LOGF(LogStealthObjectivesAssets, Error, "STEALTH OBJECTIVES CHAIN: FAIL could not create %ls", *PackageName);
			return;
		}
		UObjectiveChainData* Chain = FindObject<UObjectiveChainData>(Package, StealthObjectivesCacaoFixture::AssetName);
		if (Chain == nullptr)
		{
			Chain = NewObject<UObjectiveChainData>(Package, StealthObjectivesCacaoFixture::AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Chain);
		}
		Chain->ChainSerial = StealthObjectivesCacaoFixture::ChainSerial;
		Chain->Definitions.Reset();
		for (const StealthObjectivesCacaoFixture::FRow& Row : StealthObjectivesCacaoFixture::Rows)
		{
			Chain->Definitions.Add(StealthObjectivesCacaoFixture::MakeDef(Row));
		}
		Package->MarkPackageDirty();

		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.Error = GError;
		const bool bSaved = UPackage::SavePackage(Package, nullptr, *File, Args);
		UE_LOGF(LogStealthObjectivesAssets, Display, "STEALTH OBJECTIVES CHAIN: %ls %ls (%d rows, serial %d)", bSaved ? TEXT("PASS") : TEXT("FAIL"), *File,
			Chain->Definitions.Num(), static_cast<int32>(Chain->ChainSerial));
	}

	FAutoConsoleCommand BuildCommand(
		TEXT("Stealth.Objectives.BuildCacaoChain"),
		TEXT("Objectives story oc-002: creates or refreshes /Game/StealthGame/Objectives/DA_ObjectiveChain_Cacao from the OC-16 fixture and saves exactly that package."),
		FConsoleCommandDelegate::CreateStatic(&BuildCacaoChain));
}
