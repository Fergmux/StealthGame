#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"

#include "StealthAssetToolset.generated.h"

/**
 * Moves project assets out to files for external tools (e.g. Blender): exports an asset to disk
 * in the format implied by the file extension, such as a skeletal mesh to FBX.
 */
UCLASS(BlueprintType, MinimalAPI)
class UStealthAssetToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Exports an asset to a file, choosing the exporter from the file extension (.fbx, .obj, .png...).
	 * Overwrites an existing file.
	 * @param Asset The asset to export.
	 * @param FilePath Absolute destination path including extension.
	 */
	UFUNCTION(meta = (AICallable), Category = "StealthAssetToolset")
	static void ExportAssetToFile(UObject* Asset, const FString& FilePath);
};
