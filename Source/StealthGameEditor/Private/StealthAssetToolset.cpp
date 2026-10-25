#include "StealthAssetToolset.h"

#include "AssetExportTask.h"
#include "Exporters/Exporter.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"

void UStealthAssetToolset::ExportAssetToFile(UObject* Asset, const FString& FilePath)
{
	if (!Asset)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Asset must not be null."));
		return;
	}
	if (FilePath.IsEmpty() || FPaths::GetExtension(FilePath).IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("FilePath must be a file path with an extension."));
		return;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);

	UAssetExportTask* Task = NewObject<UAssetExportTask>();
	Task->Object = Asset;
	Task->Filename = FilePath;
	Task->bSelected = false;
	Task->bReplaceIdentical = true;
	Task->bPrompt = false;
	Task->bAutomated = true;
	Task->bUseFileArchive = false;
	Task->bWriteEmptyFiles = false;

	if (!UExporter::RunAssetExportTask(Task) || !IFileManager::Get().FileExists(*FilePath))
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("Export of '%s' to '%s' failed: no exporter for this asset and extension, or the write failed."),
			*Asset->GetPathName(), *FilePath));
	}
}
