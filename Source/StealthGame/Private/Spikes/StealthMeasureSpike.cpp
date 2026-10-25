// THROWAWAY SPIKE CODE (2026-10-25): art-bible OI-1 measurement helper. Compiled out of Shipping.
// Console: stealth.Spike.MeasureMesh /Game/Path/SK_A [/Game/Path/SK_B ...]
// Logs, per skeletal mesh: skeleton, bone count, imported (reference-pose) bounds and every bone's
// component-space reference-pose position, so stature and chain lengths can be computed offline.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "HAL/IConsoleManager.h"
#include "ReferenceSkeleton.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthMeasure, Log, All);

namespace StealthSpikeMeasure
{
	static void Measure(const FString& Path)
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Path);
		if (!Mesh)
		{
			UE_LOG(LogStealthMeasure, Warning, TEXT("[SpikeMeasure] cannot load %s"), *Path);
			return;
		}
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		const FBox Bounds = Mesh->GetImportedBounds().GetBox();
		UE_LOG(LogStealthMeasure, Display, TEXT("[SpikeMeasure] mesh=%s skeleton=%s rawBones=%d meshBones=%d importedMin=%s importedMax=%s heightZ=%.2f"),
			*Mesh->GetPathName(), *GetPathNameSafe(Mesh->GetSkeleton()), Ref.GetRawBoneNum(), Ref.GetNum(),
			*Bounds.Min.ToCompactString(), *Bounds.Max.ToCompactString(), Bounds.Max.Z - Bounds.Min.Z);
		if (const USkeleton* Skeleton = Mesh->GetSkeleton())
		{
			UE_LOG(LogStealthMeasure, Display, TEXT("[SpikeMeasure] skeleton=%s skeletonBones=%d"), *Skeleton->GetPathName(), Skeleton->GetReferenceSkeleton().GetNum());
		}
		const TArray<FTransform>& Local = Ref.GetRefBonePose();
		TArray<FTransform> Component;
		Component.SetNum(Local.Num());
		for (int32 Index = 0; Index < Local.Num(); ++Index)
		{
			const int32 Parent = Ref.GetParentIndex(Index);
			Component[Index] = Parent == INDEX_NONE ? Local[Index] : Local[Index] * Component[Parent];
			const FVector P = Component[Index].GetLocation();
			UE_LOG(LogStealthMeasure, Display, TEXT("[SpikeMeasureBone] mesh=%s i=%d name=%s parent=%s cs=%.3f,%.3f,%.3f"),
				*Mesh->GetName(), Index, *Ref.GetBoneName(Index).ToString(),
				Parent == INDEX_NONE ? TEXT("-") : *Ref.GetBoneName(Parent).ToString(), P.X, P.Y, P.Z);
		}
	}

	static FAutoConsoleCommand CmdMeasure(TEXT("stealth.Spike.MeasureMesh"),
		TEXT("SPIKE: log bone count, reference-pose bounds and component-space bone positions of skeletal meshes."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			for (const FString& Path : Args)
			{
				Measure(Path);
			}
		}));
}

#endif // !UE_BUILD_SHIPPING
