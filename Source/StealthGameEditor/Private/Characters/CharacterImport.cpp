// Character import onto the project skeleton (CPA stories 002 and 003, AC-3; art bible 8.6 / 8.7 step 11).
// Editor console commands (paths without spaces; FBX paths are relative to the project folder):
//   Stealth.Characters.Import <DestFolder> <AssetName> <BaseFbx> [<Lod1Fbx> <Lod2Fbx> ...] [-NoMaterials]
//       imports BaseFbx through Interchange (the project's DefaultFBXOBJAssetsPipeline, duplicated and set to:
//       skeletal mesh only, skeleton = SK_UEFN_Mannequin, no animation, no physics asset, materials and textures
//       imported unless -NoMaterials), imports each LOD file into LOD 1..n, prints the report below, and saves ONLY the packages under
//       DestFolder by explicit path. The skeleton package is never saved: the report states whether it is dirty.
//   Stealth.Characters.Report <MeshObjectPath>
//       prints the report: bones, skeleton, per-bone reference-pose deltas against the skeleton, imported bounds
//       (height, sole Z), the forward axis (component-space foot_l -> ball_l), LOD count and triangles per LOD.
//   Stealth.Characters.CharacterMeshRotation <BlueprintClassPath>
//       logs the relative rotation of a character Blueprint's mesh component (how mesh space maps to actor space).
//   Stealth.Characters.Open <MeshObjectPath>
//       opens the skeletal mesh editor on the asset (for the retained viewport screenshot).
// Report lines start with "STEALTH CHARACTER" so a run log can be grepped.

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/Skeleton.h"
#include "Editor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeManager.h"
#include "InterchangeSourceData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshModel.h"
#include "SkeletalMeshEditorSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthCharacterImport, Log, All);

namespace StealthCharacterImport
{
	/** The project skeleton every pool model is imported onto (art bible 1.3.2; a sample asset: never saved). */
	const TCHAR* const SkeletonPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin");
	/** The engine's default Interchange pipeline for FBX/OBJ assets, duplicated per import. */
	const TCHAR* const DefaultPipelinePath = TEXT("/Interchange/Pipelines/DefaultFBXOBJAssetsPipeline.DefaultFBXOBJAssetsPipeline");

	FString AbsoluteSource(const FString& Relative)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Relative));
	}

	/** Component-space reference pose of a reference skeleton. */
	TArray<FTransform> ComponentSpace(const FReferenceSkeleton& Ref)
	{
		const TArray<FTransform>& Local = Ref.GetRawRefBonePose();
		TArray<FTransform> Out;
		Out.SetNum(Local.Num());
		for (int32 Bone = 0; Bone < Local.Num(); ++Bone)
		{
			const int32 Parent = Ref.GetParentIndex(Bone);
			Out[Bone] = Parent == INDEX_NONE ? Local[Bone] : Local[Bone] * Out[Parent];
		}
		return Out;
	}

	/** Logs the measurement report of a skeletal mesh against SK_UEFN_Mannequin. */
	void Report(USkeletalMesh* Mesh)
	{
		const USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
		const FReferenceSkeleton& MeshRef = Mesh->GetRefSkeleton();
		UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER %ls: bones %d, skeleton %ls",
			*Mesh->GetPathName(), MeshRef.GetRawBoneNum(), Mesh->GetSkeleton() ? *Mesh->GetSkeleton()->GetPathName() : TEXT("<none>"));

		if (Skeleton)
		{
			const FReferenceSkeleton& SkelRef = Skeleton->GetReferenceSkeleton();
			const TArray<FTransform> MeshCS = ComponentSpace(MeshRef);
			const TArray<FTransform> SkelCS = ComponentSpace(SkelRef);
			double MaxPos = 0.0, MaxRotDeg = 0.0;
			FName MaxPosBone, MaxRotBone;
			int32 Missing = 0;
			for (int32 Bone = 0; Bone < MeshRef.GetRawBoneNum(); ++Bone)
			{
				const int32 SkelBone = SkelRef.FindBoneIndex(MeshRef.GetBoneName(Bone));
				if (SkelBone == INDEX_NONE)
				{
					++Missing;
					UE_LOGF(LogStealthCharacterImport, Warning, "STEALTH CHARACTER bone %ls is not in SK_UEFN_Mannequin", *MeshRef.GetBoneName(Bone).ToString());
					continue;
				}
				const double Pos = FVector::Dist(MeshCS[Bone].GetLocation(), SkelCS[SkelBone].GetLocation());
				const double Rot = FMath::RadiansToDegrees(MeshCS[Bone].GetRotation().AngularDistance(SkelCS[SkelBone].GetRotation()));
				if (Pos > MaxPos) { MaxPos = Pos; MaxPosBone = MeshRef.GetBoneName(Bone); }
				if (Rot > MaxRotDeg) { MaxRotDeg = Rot; MaxRotBone = MeshRef.GetBoneName(Bone); }
			}
			UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER ref pose vs SK_UEFN_Mannequin (%d bones): bones not in skeleton %d; max position delta %.4f cm (%ls); max rotation delta %.4f deg (%ls)",
				SkelRef.GetRawBoneNum(), Missing, MaxPos, *MaxPosBone.ToString(), MaxRotDeg, *MaxRotBone.ToString());
			UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER skeleton package dirty: %ls", Skeleton->GetPackage()->IsDirty() ? TEXT("YES") : TEXT("no"));

			const int32 Foot = MeshRef.FindBoneIndex(TEXT("foot_l"));
			const int32 Ball = MeshRef.FindBoneIndex(TEXT("ball_l"));
			if (Foot != INDEX_NONE && Ball != INDEX_NONE)
			{
				FVector Forward = MeshCS[Ball].GetLocation() - MeshCS[Foot].GetLocation();
				Forward.Z = 0.0;
				Forward.Normalize();
				UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER forward (foot_l -> ball_l, mesh space): (%.3f, %.3f, %.3f)", Forward.X, Forward.Y, Forward.Z);
			}
		}

		const FBox Box = Mesh->GetImportedBounds().GetBox();
		UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER imported bounds: min (%.2f, %.2f, %.2f) max (%.2f, %.2f, %.2f); height %.2f cm; sole Z %.2f",
			Box.Min.X, Box.Min.Y, Box.Min.Z, Box.Max.X, Box.Max.Y, Box.Max.Z, Box.Max.Z - Box.Min.Z, Box.Min.Z);

		if (const FSkeletalMeshModel* Model = Mesh->GetImportedModel())
		{
			for (int32 Lod = 0; Lod < Model->LODModels.Num(); ++Lod)
			{
				int32 Triangles = 0;
				for (const FSkelMeshSection& Section : Model->LODModels[Lod].Sections)
				{
					Triangles += Section.NumTriangles;
				}
				UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER LOD%d: %d triangles, %d vertices, %d section(s)",
					Lod, Triangles, Model->LODModels[Lod].NumVertices, Model->LODModels[Lod].Sections.Num());
			}
		}
		UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER materials: %d", Mesh->GetMaterials().Num());
	}

	/** Saves every package under DestFolder (and nothing else) by explicit path. */
	void SaveFolder(const FString& DestFolder)
	{
		IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*DestFolder), Assets, /*bRecursive*/ true);
		TSet<UPackage*> Packages;
		for (const FAssetData& Asset : Assets)
		{
			if (UObject* Object = Asset.GetAsset())
			{
				Packages.Add(Object->GetPackage());
			}
		}
		for (UPackage* Package : Packages)
		{
			if (!Package->GetName().StartsWith(DestFolder + TEXT("/")))
			{
				continue;
			}
			const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs Args;
			Args.TopLevelFlags = RF_Public | RF_Standalone;
			Args.Error = GError;
			const bool bSaved = UPackage::SavePackage(Package, nullptr, *File, Args);
			UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER %ls %ls", bSaved ? TEXT("saved") : TEXT("FAILED to save"), *File);
		}
	}

	void Import(const TArray<FString>& InArgs)
	{
		TArray<FString> Args = InArgs;
		const bool bImportMaterials = Args.RemoveAll([](const FString& Arg) { return Arg.Equals(TEXT("-NoMaterials"), ESearchCase::IgnoreCase); }) == 0;
		if (Args.Num() < 3)
		{
			UE_LOGF(LogStealthCharacterImport, Error, "usage: Stealth.Characters.Import <DestFolder> <AssetName> <BaseFbx> [<Lod1Fbx> ...]");
			return;
		}
		const FString DestFolder = Args[0];
		const FString AssetName = Args[1];
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
		const UInterchangeGenericAssetsPipeline* Default = LoadObject<UInterchangeGenericAssetsPipeline>(nullptr, DefaultPipelinePath);
		if (!Skeleton || !Default)
		{
			UE_LOGF(LogStealthCharacterImport, Error, "STEALTH CHARACTER import: skeleton or default pipeline not found");
			return;
		}
		const double Start = FPlatformTime::Seconds();

		UInterchangeGenericAssetsPipeline* Pipeline = DuplicateObject<UInterchangeGenericAssetsPipeline>(Default, GetTransientPackage());
		Pipeline->CommonMeshesProperties->ForceAllMeshAsType = EInterchangeForceMeshType::IFMT_SkeletalMesh;
		Pipeline->CommonMeshesProperties->bImportLods = true;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->Skeleton = Skeleton;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = false;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bAddCurveMetadataToSkeleton = false;
		Pipeline->MeshPipeline->bImportStaticMeshes = false;
		Pipeline->MeshPipeline->bImportSkeletalMeshes = true;
		Pipeline->MeshPipeline->bImportMorphTargets = false;
		Pipeline->MeshPipeline->bCreatePhysicsAsset = false;
		Pipeline->AnimationPipeline->bImportAnimations = false;
		Pipeline->MaterialPipeline->bImportMaterials = bImportMaterials;

		FImportAssetParameters Params;
		Params.bIsAutomated = true;
		Params.bReplaceExisting = true;
		Params.DestinationName = AssetName;
		Params.OverridePipelines.Add(FSoftObjectPath(Pipeline));

		UInterchangeSourceData* Source = UInterchangeManager::CreateSourceData(AbsoluteSource(Args[2]));
		TArray<UObject*> Imported;
		const bool bOk = UInterchangeManager::GetInterchangeManager().ImportAsset(DestFolder, Source, Params, Imported);
		USkeletalMesh* Mesh = nullptr;
		for (UObject* Object : Imported)
		{
			UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER imported %ls (%ls)", *GetPathNameSafe(Object), Object ? *Object->GetClass()->GetName() : TEXT("null"));
			Mesh = Mesh ? Mesh : Cast<USkeletalMesh>(Object);
		}
		if (!bOk || !Mesh)
		{
			UE_LOGF(LogStealthCharacterImport, Error, "STEALTH CHARACTER import of %ls failed (ok=%d, skeletal mesh %ls)", *Args[2], bOk ? 1 : 0, Mesh ? TEXT("found") : TEXT("missing"));
			return;
		}

		for (int32 Lod = 1; Lod < Args.Num() - 2; ++Lod)
		{
			const int32 Result = USkeletalMeshEditorSubsystem::ImportLOD(Mesh, Lod, AbsoluteSource(Args[2 + Lod]));
			UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER LOD%d from %ls: %ls", Lod, *Args[2 + Lod], Result == Lod ? TEXT("imported") : TEXT("FAILED"));
		}

		Report(Mesh);
		SaveFolder(DestFolder);
		UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER import wall time %.1f s", FPlatformTime::Seconds() - Start);
	}

	void ReportCommand(const TArray<FString>& Args)
	{
		USkeletalMesh* Mesh = Args.Num() > 0 ? LoadObject<USkeletalMesh>(nullptr, *Args[0]) : nullptr;
		if (!Mesh)
		{
			UE_LOGF(LogStealthCharacterImport, Error, "usage: Stealth.Characters.Report <MeshObjectPath> (mesh not found)");
			return;
		}
		Report(Mesh);
	}

	void CharacterMeshRotationCommand(const TArray<FString>& Args)
	{
		const UClass* Class = Args.Num() > 0 ? LoadObject<UClass>(nullptr, *Args[0]) : nullptr;
		const ACharacter* Character = Class ? Cast<ACharacter>(Class->GetDefaultObject()) : nullptr;
		if (!Character || !Character->GetMesh())
		{
			UE_LOGF(LogStealthCharacterImport, Error, "usage: Stealth.Characters.CharacterMeshRotation <BlueprintClassPath> (no character class)");
			return;
		}
		const FRotator Rotation = Character->GetMesh()->GetRelativeRotation();
		const FVector MeshForward = Rotation.RotateVector(FVector::YAxisVector);
		UE_LOGF(LogStealthCharacterImport, Display, "STEALTH CHARACTER %ls mesh relative rotation: pitch %.1f yaw %.1f roll %.1f; mesh-space +Y maps to actor (%.3f, %.3f, %.3f)",
			*Class->GetPathName(), Rotation.Pitch, Rotation.Yaw, Rotation.Roll, MeshForward.X, MeshForward.Y, MeshForward.Z);
	}

	void OpenCommand(const TArray<FString>& Args)
	{
		USkeletalMesh* Mesh = Args.Num() > 0 ? LoadObject<USkeletalMesh>(nullptr, *Args[0]) : nullptr;
		if (Mesh && GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Mesh);
		}
	}

	FAutoConsoleCommand ImportCommand(TEXT("Stealth.Characters.Import"),
		TEXT("Imports a character FBX (and LOD FBXs) onto SK_UEFN_Mannequin and saves only the destination folder."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Import));
	FAutoConsoleCommand ReportCmd(TEXT("Stealth.Characters.Report"),
		TEXT("Logs bones, reference-pose deltas against SK_UEFN_Mannequin, bounds, forward axis and LODs of a skeletal mesh."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ReportCommand));
	FAutoConsoleCommand MeshRotationCmd(TEXT("Stealth.Characters.CharacterMeshRotation"),
		TEXT("Logs the mesh component's relative rotation of a character Blueprint class."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&CharacterMeshRotationCommand));
	FAutoConsoleCommand OpenCmd(TEXT("Stealth.Characters.Open"),
		TEXT("Opens the skeletal mesh editor on a mesh."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&OpenCommand));
}
