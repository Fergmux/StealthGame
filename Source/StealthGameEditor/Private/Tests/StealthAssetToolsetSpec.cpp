#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "StealthAssetToolset.h"
#include "ToolsetRegistry/ToolCallExceptionHandler.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FStealthAssetToolsetSpec, "StealthGame.EditorTools.AssetToolset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	TUniquePtr<UE::ToolsetRegistry::FToolCallExceptionHandler> ExceptionHandler;

END_DEFINE_SPEC(FStealthAssetToolsetSpec)

void FStealthAssetToolsetSpec::Define()
{
	BeforeEach([this]() { ExceptionHandler = MakeUnique<UE::ToolsetRegistry::FToolCallExceptionHandler>(); });
	AfterEach([this]() { ExceptionHandler.Reset(); });

	Describe("ExportAssetToFile", [this]()
	{
		It("exports an engine static mesh to FBX", [this]()
		{
			UObject* Cube = LoadObject<UObject>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			const FString Path = FPaths::ProjectSavedDir() / TEXT("Automation/StealthAssetToolset/Cube.fbx");
			IFileManager::Get().Delete(*Path);
			ExceptionHandler->CaptureErrorsIn([Cube, Path]() { UStealthAssetToolset::ExportAssetToFile(Cube, Path); });
			TestEqual(TEXT("No error"), ExceptionHandler->GetException(), FString());
			TestTrue(TEXT("File written"), IFileManager::Get().FileExists(*Path));
		});

		It("raises when the asset is null", [this]()
		{
			ExceptionHandler->CaptureErrorsIn([]() { UStealthAssetToolset::ExportAssetToFile(nullptr, TEXT("C:/Temp/x.fbx")); });
			TestTrue(TEXT("Error mentions null asset"), ExceptionHandler->GetException().Contains(TEXT("Asset must not be null")));
		});

		It("raises when the path has no extension", [this]()
		{
			UObject* Cube = LoadObject<UObject>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			ExceptionHandler->CaptureErrorsIn([Cube]() { UStealthAssetToolset::ExportAssetToFile(Cube, TEXT("C:/Temp/noextension")); });
			TestTrue(TEXT("Error mentions extension"), ExceptionHandler->GetException().Contains(TEXT("extension")));
		});
	});
}

#endif
