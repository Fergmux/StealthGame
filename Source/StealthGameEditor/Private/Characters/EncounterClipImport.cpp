// Assassination stories asn-018 / asn-020 (placeholder encounter clips): imports the D54 spike clips (SCL story 056,
// assets/animations/spike-d54/, retargeted onto SK_UEFN_Mannequin by tools/asset-pipeline/retarget_motion_to_mannequin.py)
// as animation sequences and wraps each in a DefaultSlot montage, so the encounter presentation has a strike lunge, a
// victim fall and a counter to play until the D54 engine half delivers the authored clips. Editor console command:
//   Stealth.Animations.ImportEncounterClips [<DestFolder>]
//       DestFolder defaults to /Game/StealthGame/Animations/Assassination. Interchange (a transient copy of the engine's
//       DefaultFBXOBJAssetsPipeline): animations only, skeleton SK_UEFN_Mannequin, no mesh, materials or physics asset.
//       The strike sequence enables root motion (the lunge is a root-motion clip warped to the local "StrikeContact"
//       target); the fall and counter do not. Montages: DefaultSlot, blend in 0.10 s; the fall montage never auto blends
//       out (the body holds its final pose through the linger). Saves ONLY the packages under DestFolder by explicit path;
//       the skeleton package is never saved (the log says whether it is dirty).
// Report lines start with "STEALTH CLIPS" so a run log can be grepped.

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Factories/AnimMontageFactory.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "InterchangeGenericAnimationPipeline.h"
#include "InterchangeGenericAssetsPipeline.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangeGenericMaterialPipeline.h"
#include "InterchangeGenericMeshPipeline.h"
#include "InterchangeGenericTexturePipeline.h"
#include "InterchangeManager.h"
#include "InterchangeSourceData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthClipImport, Log, All);

namespace StealthEncounterClipImport
{
	const TCHAR* const SkeletonPath = TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin.SK_UEFN_Mannequin");
	const TCHAR* const DefaultPipelinePath = TEXT("/Interchange/Pipelines/DefaultFBXOBJAssetsPipeline.DefaultFBXOBJAssetsPipeline");
	const TCHAR* const DefaultFolder = TEXT("/Game/StealthGame/Animations/Assassination");

	/** One placeholder clip: the source FBX (relative to the project), the sequence and montage names, root motion, auto blend out. */
	struct FClip
	{
		const TCHAR* Fbx;
		const TCHAR* Sequence;
		const TCHAR* Montage;
		bool bRootMotion;
		bool bAutoBlendOut;
	};

	const FClip Clips[] = {
		{ TEXT("assets/animations/spike-d54/A_D54_Strike_T2MSwiftFast.fbx"), TEXT("A_ASN_Strike_D54Placeholder"), TEXT("AM_ASN_Strike_D54Placeholder"), true, true },
		{ TEXT("assets/animations/spike-d54/A_D54_Fall_LibShotFallForward.fbx"), TEXT("A_ASN_DeathFall_D54Placeholder"), TEXT("AM_ASN_DeathFall_D54Placeholder"), false, false },
		{ TEXT("assets/animations/spike-d54/A_D54_Brace_LibBlock1.fbx"), TEXT("A_ASN_Counter_D54Placeholder"), TEXT("AM_ASN_Counter_D54Placeholder"), false, true },
	};

	/**
	 * The D54 FBX clips carry their bone translations in metres under a centimetre root (the Blender armature's unit scale
	 * is not applied to the bone keys by Interchange): the pelvis sits 0.9 cm above the root instead of about 90 cm, and
	 * every pose collapses onto the floor. Detected from the pelvis against the skeleton's reference pose and fixed by
	 * scaling every non-root bone translation key by 100 (the root track, the root motion, is already in centimetres).
	 */
	void FixTranslationUnits(UAnimSequence* Sequence, const USkeleton* Skeleton)
	{
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		const FReferenceSkeleton& Ref = Skeleton->GetReferenceSkeleton();
		const int32 Pelvis = Ref.FindBoneIndex(TEXT("pelvis"));
		if (Model == nullptr || Pelvis == INDEX_NONE)
		{
			return;
		}
		const double Keyed = Model->GetBoneTrackTransform(TEXT("pelvis"), FFrameNumber(0)).GetLocation().Size();
		const double Reference = Ref.GetRefBonePose()[Pelvis].GetLocation().Size();
		const double Ratio = Keyed > UE_KINDA_SMALL_NUMBER ? Reference / Keyed : 0.0;
		UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS %ls pelvis keyed %.3f vs reference %.3f (ratio %.1f)", *Sequence->GetName(), Keyed, Reference, Ratio);
		if (Ratio < 50.0 || Ratio > 200.0)
		{
			return;
		}
		TArray<FName> Names;
		Model->GetBoneTrackNames(Names);
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.OpenBracket(FText::FromString(TEXT("Scale D54 bone translations to centimetres")), false);
		for (const FName& Name : Names)
		{
			if (Name == FName(TEXT("root")))
			{
				continue;
			}
			TArray<FTransform> Keys;
			Model->GetBoneTrackTransforms(Name, Keys);
			TArray<FVector> Positions;
			TArray<FQuat> Rotations;
			TArray<FVector> Scales;
			for (const FTransform& Key : Keys)
			{
				Positions.Add(Key.GetLocation() * 100.0);
				Rotations.Add(Key.GetRotation());
				Scales.Add(Key.GetScale3D());
			}
			Controller.SetBoneTrackKeys(Name, Positions, Rotations, Scales, false);
		}
		Controller.CloseBracket(false);
		UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS %ls: %d non-root bone tracks scaled x100; pelvis now %.1f cm", *Sequence->GetName(), Names.Num() - 1,
			Model->GetBoneTrackTransform(TEXT("pelvis"), FFrameNumber(0)).GetLocation().Size());
	}

	UAnimSequence* ImportSequence(const FClip& Clip, const FString& DestFolder, USkeleton* Skeleton, const UInterchangeGenericAssetsPipeline* Default)
	{
		UInterchangeGenericAssetsPipeline* Pipeline = DuplicateObject<UInterchangeGenericAssetsPipeline>(Default, GetTransientPackage());
		Pipeline->CommonMeshesProperties->ForceAllMeshAsType = EInterchangeForceMeshType::IFMT_SkeletalMesh;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->Skeleton = Skeleton;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bImportOnlyAnimations = true;
		Pipeline->CommonSkeletalMeshesAndAnimationsProperties->bAddCurveMetadataToSkeleton = false;
		Pipeline->MeshPipeline->bImportStaticMeshes = false;
		Pipeline->MeshPipeline->bImportSkeletalMeshes = false;
		Pipeline->MeshPipeline->bImportMorphTargets = false;
		Pipeline->MeshPipeline->bCreatePhysicsAsset = false;
		Pipeline->AnimationPipeline->bImportAnimations = true;
		Pipeline->AnimationPipeline->bImportBoneTracks = true;
		Pipeline->MaterialPipeline->bImportMaterials = false;
		if (Pipeline->MaterialPipeline->TexturePipeline) { Pipeline->MaterialPipeline->TexturePipeline->bImportTextures = false; }

		FImportAssetParameters Params;
		Params.bIsAutomated = true;
		Params.bReplaceExisting = true;
		Params.DestinationName = Clip.Sequence;
		Params.OverridePipelines.Add(FSoftObjectPath(Pipeline));

		const FString Source = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Clip.Fbx));
		TArray<UObject*> Imported;
		const bool bOk = UInterchangeManager::GetInterchangeManager().ImportAsset(DestFolder, UInterchangeManager::CreateSourceData(Source), Params, Imported);
		UAnimSequence* Sequence = nullptr;
		for (UObject* Object : Imported)
		{
			UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS imported %ls (%ls)", *GetPathNameSafe(Object), Object ? *Object->GetClass()->GetName() : TEXT("null"));
			Sequence = Sequence ? Sequence : Cast<UAnimSequence>(Object);
		}
		if (!bOk || Sequence == nullptr)
		{
			UE_LOGF(LogStealthClipImport, Error, "STEALTH CLIPS import of %ls failed (ok=%d, sequence %ls)", Clip.Fbx, bOk ? 1 : 0, Sequence ? TEXT("found") : TEXT("missing"));
			return nullptr;
		}
		Sequence->Modify();
		FixTranslationUnits(Sequence, Skeleton);
		Sequence->bEnableRootMotion = Clip.bRootMotion;
		Sequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
		Sequence->bForceRootLock = !Clip.bRootMotion;
		Sequence->MarkPackageDirty();
		UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS sequence %ls: length %.3f s, %d keys, root motion %d, skeleton %ls",
			*Sequence->GetPathName(), Sequence->GetPlayLength(), Sequence->GetNumberOfSampledKeys(), Clip.bRootMotion ? 1 : 0,
			Sequence->GetSkeleton() ? *Sequence->GetSkeleton()->GetName() : TEXT("<none>"));
		return Sequence;
	}

	UAnimMontage* MakeMontage(const FClip& Clip, const FString& DestFolder, UAnimSequence* Sequence)
	{
		const FString ObjectPath = DestFolder / FString(Clip.Montage) + TEXT(".") + Clip.Montage;
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Montage == nullptr)
		{
			UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
			Factory->SourceAnimation = Sequence;
			Factory->TargetSkeleton = Sequence->GetSkeleton();
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			Montage = Cast<UAnimMontage>(AssetTools.CreateAsset(Clip.Montage, DestFolder, UAnimMontage::StaticClass(), Factory));
		}
		if (Montage == nullptr)
		{
			UE_LOGF(LogStealthClipImport, Error, "STEALTH CLIPS montage %ls could not be created", Clip.Montage);
			return nullptr;
		}
		Montage->Modify();
		Montage->BlendIn.SetBlendTime(0.10f);
		Montage->BlendOut.SetBlendTime(0.20f);
		Montage->bEnableAutoBlendOut = Clip.bAutoBlendOut;
		Montage->MarkPackageDirty();
		const FName Slot = Montage->SlotAnimTracks.Num() > 0 ? Montage->SlotAnimTracks[0].SlotName : NAME_None;
		UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS montage %ls: length %.3f s, slot %ls, auto blend out %d, root motion %d",
			*Montage->GetPathName(), Montage->GetPlayLength(), *Slot.ToString(), Montage->bEnableAutoBlendOut ? 1 : 0, Montage->HasRootMotion() ? 1 : 0);
		return Montage;
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
			UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS %ls %ls", bSaved ? TEXT("saved") : TEXT("FAILED to save"), *File);
		}
	}

	void Import(const TArray<FString>& Args)
	{
		const FString DestFolder = Args.Num() > 0 ? Args[0] : FString(DefaultFolder);
		USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
		const UInterchangeGenericAssetsPipeline* Default = LoadObject<UInterchangeGenericAssetsPipeline>(nullptr, DefaultPipelinePath);
		if (Skeleton == nullptr || Default == nullptr)
		{
			UE_LOGF(LogStealthClipImport, Error, "STEALTH CLIPS skeleton or default pipeline not found");
			return;
		}
		for (const FClip& Clip : Clips)
		{
			if (UAnimSequence* Sequence = ImportSequence(Clip, DestFolder, Skeleton, Default))
			{
				MakeMontage(Clip, DestFolder, Sequence);
			}
		}
		SaveFolder(DestFolder);
		UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS skeleton package dirty: %ls (never saved)", Skeleton->GetPackage()->IsDirty() ? TEXT("YES") : TEXT("no"));
	}

	/** Logs the root and pelvis tracks of a sequence at a few frames (diagnostics for the root lock of a clip). */
	void DumpRoot(const TArray<FString>& Args)
	{
		const UAnimSequence* Sequence = Args.Num() > 0 ? LoadObject<UAnimSequence>(nullptr, *Args[0]) : nullptr;
		const IAnimationDataModel* Model = Sequence != nullptr ? Sequence->GetDataModel() : nullptr;
		if (Model == nullptr)
		{
			UE_LOGF(LogStealthClipImport, Error, "usage: Stealth.Animations.DumpRoot <SequenceObjectPath>");
			return;
		}
		const int32 Frames = Model->GetNumberOfKeys();
		for (const TCHAR* Bone : { TEXT("root"), TEXT("pelvis") })
		{
			for (int32 Step = 0; Step <= 4; ++Step)
			{
				const FFrameNumber Frame((Frames - 1) * Step / 4);
				const FTransform T = Model->GetBoneTrackTransform(FName(Bone), Frame);
				const FRotator R = T.Rotator();
				UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS %ls %ls frame %d: loc (%.1f, %.1f, %.1f) rot (p %.1f y %.1f r %.1f)", *Sequence->GetName(), Bone,
					Frame.Value, T.GetLocation().X, T.GetLocation().Y, T.GetLocation().Z, R.Pitch, R.Yaw, R.Roll);
			}
		}
		UE_LOGF(LogStealthClipImport, Display, "STEALTH CLIPS %ls root motion %d, root lock %d, force root lock %d", *Sequence->GetName(), Sequence->bEnableRootMotion ? 1 : 0,
			static_cast<int32>(Sequence->RootMotionRootLock.GetValue()), Sequence->bForceRootLock ? 1 : 0);
	}

	FAutoConsoleCommand DumpRootCommand(TEXT("Stealth.Animations.DumpRoot"),
		TEXT("Logs the root and pelvis tracks of an animation sequence at five frames."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpRoot));

	FAutoConsoleCommand ImportCommand(TEXT("Stealth.Animations.ImportEncounterClips"),
		TEXT("Imports the D54 placeholder strike, fall and counter clips onto SK_UEFN_Mannequin as montages and saves only the destination folder."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&Import));
}
