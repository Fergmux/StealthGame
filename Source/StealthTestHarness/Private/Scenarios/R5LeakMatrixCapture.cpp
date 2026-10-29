// Concealment story con-034 (production/epics/concealment-bushes/story-034-r5-leak-matrix.md; GDD AC-CON-23, VA-4; ADR-0017
// section 6, ADR-0022): the R5 leak matrix against the PRODUCTION hide. A capture scenario, not a gate test: it needs a
// rendering viewport, so it runs in the editor's PIE (standalone, 1 player) through the console:
//   Stealth.R5Matrix [SettleFrames=60]
// Captures and the jsonl log go to Saved/Harness/R5Matrix; the analysis is tools/analysis/r5_matrix.py (observations only).
//
// The subject is an unbound Shared Pawn (AStealthPawn, an NPC: a non-ally of the local viewer) with a groom on its head and
// an active Niagara system attached under the mesh (the spike's unverified attachments). It hides through the production
// path only: Hide(Enter) at a bush through the command component -> Round's queue -> UConcealmentService -> the commit;
// the local machine's UConcealmentComponent latch applies the hide (ApplyHidden) at Start + BushEnterSwapSec. Nothing here
// calls SetVisibility on the subject.
//
// Per cell (sun low / noon / shade under a roof slab x VSM on / off; Lumen software tracing; the hardware-RT cells are
// N/A by configuration: r.RayTracing=False, ADR-0022), with the viewer 7 m from the bush:
//   EmptyA (subject 200 m away), Visible (subject standing at the bush's +X entry), Hidden (subject concealed in the bush),
//   EmptyB: six captures each after SettleFrames (the temporal mean); plus every frame from the Hide(Enter) press to 0.25 s
//   past the swap and from the Hide(Exit) press to 0.15 s past the crossing (single frames around each swap: one-frame
//   flashes that temporal means hide), each logged with the subject's pose start and this machine's server time.
// Then the LOD rows (noon, VSM on): the concealed subject's mesh forced to every LOD in turn, the render-proxy report and
// three captures each. Then the magenta rows: the subject's materials overridden with a saturated magenta (the occlusion
// proxy: any drawn pixel of the body is detectable) at viewer distances 5, 15 and 25 m, every frame of one full enter and
// exit; frames whose server time lies in [swap, crossing) must hold 0 magenta pixels.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && WITH_STEALTH_TEST_SEAMS

#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Server/ConcealmentService.h"
#include "StealthCharacter.h"
#include "StealthSettings.h"
#include "Server/Core/StealthPawnCommandComponent.h"
#include "UnrealClient.h"
#include "View/BushRuntimeSubsystem.h"
#include "View/ConcealmentComponent.h"
#include "View/Core/StealthPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthR5Matrix, Log, All);

namespace StealthR5Matrix
{
	struct FStep
	{
		int32 WaitFrames = 0;
		/** Returns false to stay on this step another frame (a wait-until). */
		TFunction<bool()> Do;
	};

	struct FRun
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AStealthPawn> Subject;
		TWeakObjectPtr<APlayerController> Viewer;
		TWeakObjectPtr<UDirectionalLightComponent> Sun;
		TWeakObjectPtr<AStaticMeshActor> Roof;
		FRotator SunOriginal = FRotator::ZeroRotator;
		EComponentMobility::Type SunMobility = EComponentMobility::Movable;
		FBushRuntime Bush;
		/** The direction from the bush to the viewer (the subject enters and leaves on that side, in view). */
		FVector ViewDir = FVector(1.0, 0.0, 0.0);
		TOptional<FConcealmentParams> PreviousParams;
		TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;
		FString Dir;
		FString Lines;
		TArray<FStep> Steps;
		int32 Index = 0;
		int32 Wait = 0;
		int32 Frame = 0;
		FTSTicker::FDelegateHandle Handle;
	};

	static TUniquePtr<FRun> Active;

	void Log(const FString& Json)
	{
		Active->Lines += Json + TEXT("\n");
		UE_LOG(LogStealthR5Matrix, Display, TEXT("R5M %s"), *Json);
	}

	double Now()
	{
		const UWorld* World = Active->World.Get();
		const AGameStateBase* State = World != nullptr ? World->GetGameState() : nullptr;
		return State != nullptr ? State->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0);
	}

	void SetCVar(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByConsole);
		}
	}

	/** The subject teleported to Where (ground point) facing Yaw. */
	void PlaceSubject(const FVector& Where, double Yaw)
	{
		AStealthPawn* Subject = Active->Subject.Get();
		if (Subject != nullptr)
		{
			Subject->GetCharacterMovement()->StopMovementImmediately();
			Subject->TeleportTo(Where + FVector(0.0, 0.0, Subject->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0), FRotator(0.0, Yaw, 0.0), false, true);
		}
	}

	void PlaceSubjectFar() { PlaceSubject(Active->Bush.Anchor + FVector(20000.0, 20000.0, 0.0), 0.0); }
	/** The bush side facing the viewer (the subject's entry and exit side, so the camera sees both). */
	uint8 ViewSide() { return ConcealmentGeometry::SideFromYaw(Active->ViewDir.Rotation().Yaw); }

	void PlaceSubjectAtEntry()
	{
		const FVector Dir = ConcealmentGeometry::SideNormal(ViewSide());
		PlaceSubject(Active->Bush.Anchor + Dir * 140.0, (-Dir).Rotation().Yaw);
	}

	/**
	 * The viewer's pawn DistanceCm from the bush, facing it (its camera frames the bush): the first of eight directions (from
	 * +X, turning 45 degrees) with a clear line of sight from the eye to the bush anchor, so no building fills the frame.
	 */
	void PlaceViewer(double DistanceCm)
	{
		APlayerController* Viewer = Active->Viewer.Get();
		ACharacter* Own = Viewer != nullptr ? Cast<ACharacter>(Viewer->GetPawn()) : nullptr;
		UWorld* World = Active->World.Get();
		if (Own == nullptr || World == nullptr)
		{
			return;
		}
		const double HalfHeight = Own->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		FVector At = Active->Bush.Anchor + FVector(DistanceCm, 250.0, HalfHeight + 2.0);
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const FVector Dir = FRotator(0.0, Step * 45.0, 0.0).Vector();
			const FVector Candidate = Active->Bush.Anchor + Dir * DistanceCm + FVector(0.0, 0.0, HalfHeight + 2.0);
			FCollisionQueryParams Query(TEXT("R5MatrixViewer"), false);
			Query.AddIgnoredActor(Own);
			if (AStealthPawn* Subject = Active->Subject.Get())
			{
				Query.AddIgnoredActor(Subject);
			}
			FHitResult Hit;
			const FVector Eye = Candidate + FVector(0.0, 0.0, 150.0);
			const FVector Target = Active->Bush.Anchor + FVector(0.0, 0.0, 250.0); // over the bush top: buildings, not the bush
			if (!World->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Query))
			{
				At = Candidate;
				Active->ViewDir = Dir;
				break;
			}
		}
		const FRotator Facing = (Active->Bush.Anchor - At).GetSafeNormal2D().Rotation();
		Own->GetCharacterMovement()->StopMovementImmediately();
		Own->TeleportTo(At, FRotator(0.0, Facing.Yaw, 0.0), false, true);
		Viewer->SetControlRotation(FRotator(-10.0, Facing.Yaw, 0.0));
		Log(FString::Printf(TEXT("{\"event\":\"viewer\",\"distanceCm\":%.0f,\"at\":\"%s\"}"), DistanceCm, *At.ToString()));
	}

	FString ProxyReport()
	{
		FString Out;
		TInlineComponentArray<UPrimitiveComponent*> Primitives(Active->Subject.Get());
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive == Active->Subject->GetCapsuleComponent())
			{
				continue;
			}
			Out += FString::Printf(TEXT("%s{\"name\":\"%s\",\"class\":\"%s\",\"visible\":%d,\"proxy\":%d,\"castHiddenShadow\":%d,\"indirectWhileHidden\":%d,\"rtFarField\":%d}"),
				Out.IsEmpty() ? TEXT("") : TEXT(","), *Primitive->GetName(), *Primitive->GetClass()->GetName(), Primitive->IsVisible() ? 1 : 0,
				Primitive->SceneProxy != nullptr ? 1 : 0, Primitive->bCastHiddenShadow ? 1 : 0, Primitive->bAffectIndirectLightingWhileHidden ? 1 : 0,
				Primitive->bRayTracingFarField ? 1 : 0);
		}
		return Out;
	}

	FString PoseJson()
	{
		const AStealthPawn* Subject = Active->Subject.Get();
		const FActionState Action = Subject != nullptr ? Subject->GetActionState() : FActionState();
		const UConcealmentComponent* Conceal = UConcealmentComponent::Find(Subject);
		return FString::Printf(TEXT("\"pose\":%d,\"start\":%.4f,\"now\":%.4f,\"latch\":%d"), static_cast<int32>(Action.Pose), Action.StartServerTime, Now(),
			Conceal != nullptr && Conceal->IsPresentationHidden() ? 1 : 0);
	}

	void Shot(const FString& Name)
	{
		const FString File = Active->Dir / (Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(File, false, false);
		Log(FString::Printf(TEXT("{\"event\":\"shot\",\"file\":\"%s\",%s}"), *FPaths::GetCleanFilename(File), *PoseJson()));
	}

	void AddCapture(const FString& Prefix, int32 SettleFrames, int32 Count)
	{
		Active->Steps.Add({ SettleFrames, [Prefix]()
		{
			Log(FString::Printf(TEXT("{\"event\":\"state\",\"name\":\"%s\",%s,\"primitives\":[%s]}"), *Prefix, *PoseJson(), *ProxyReport()));
			return true;
		} });
		for (int32 N = 0; N < Count; ++N)
		{
			Active->Steps.Add({ 4, [Prefix, N]() { Shot(FString::Printf(TEXT("%s_%d"), *Prefix, N)); return true; } });
		}
	}

	/** Hide(Enter) through the production path, a shot every frame until Start + swap + 0.25 s, then wait for the End. */
	void AddEnter(const FString& Prefix)
	{
		Active->Steps.Add({ 1, []()
		{
			PlaceSubjectAtEntry();
			return true;
		} });
		Active->Steps.Add({ 20, [Prefix]()
		{
			AStealthPawn* Subject = Active->Subject.Get();
			if (UPawnCommandComponent* Command = Subject != nullptr ? Subject->GetPawnCommand() : nullptr)
			{
				Command->Hide(FHideCommand::Enter(Active->Bush.Id));
			}
			Active->Frame = 0;
			Log(FString::Printf(TEXT("{\"event\":\"enter\",\"name\":\"%s\"}"), *Prefix));
			return true;
		} });
		Active->Steps.Add({ 0, [Prefix]()
		{
			const AStealthPawn* Subject = Active->Subject.Get();
			const FConcealmentTuning_Public Tuning;
			Shot(FString::Printf(TEXT("%s_f%03d"), *Prefix, Active->Frame++));
			const FActionState Action = Subject != nullptr ? Subject->GetActionState() : FActionState();
			return Active->Frame > 240 || (Action.Pose == EActionPose::BushEnter && Now() >= Action.StartServerTime + Tuning.BushEnterSwapSec + 0.25);
		} });
		Active->Steps.Add({ 0, []()
		{
			const UConcealmentService* Service = UConcealmentService::Get(Active->World.Get());
			const AStealthPawn* Subject = Active->Subject.Get();
			return Service == nullptr || Subject == nullptr || (Service->GetState(Subject) == EConcealState::Concealed && Subject->GetActionState().Pose == EActionPose::None);
		} });
	}

	/** Hide(Exit) through the production path, a shot every frame until the crossing + 0.15 s, then wait for Outside. */
	void AddExit(const FString& Prefix)
	{
		Active->Steps.Add({ 1, [Prefix]()
		{
			AStealthPawn* Subject = Active->Subject.Get();
			if (UPawnCommandComponent* Command = Subject != nullptr ? Subject->GetPawnCommand() : nullptr)
			{
				Command->Hide(FHideCommand::Exit(ViewSide()));
			}
			Active->Frame = 0;
			Log(FString::Printf(TEXT("{\"event\":\"exit\",\"name\":\"%s\"}"), *Prefix));
			return true;
		} });
		Active->Steps.Add({ 0, [Prefix]()
		{
			const AStealthPawn* Subject = Active->Subject.Get();
			const FConcealmentTuning_Public Tuning;
			Shot(FString::Printf(TEXT("%s_f%03d"), *Prefix, Active->Frame++));
			const FActionState Action = Subject != nullptr ? Subject->GetActionState() : FActionState();
			return Active->Frame > 240 || (Action.Pose == EActionPose::BushExit && Now() >= Action.StartServerTime + Tuning.BushExitCrossSec + 0.15);
		} });
		Active->Steps.Add({ 0, []()
		{
			const UConcealmentService* Service = UConcealmentService::Get(Active->World.Get());
			const AStealthPawn* Subject = Active->Subject.Get();
			return Service == nullptr || Subject == nullptr || Service->GetState(Subject) == EConcealState::Outside;
		} });
	}

	void AddCell(const FString& Cell, double SunPitch, bool bRoof, int32 Vsm, int32 SettleFrames)
	{
		Active->Steps.Add({ 2, [Cell, SunPitch, bRoof, Vsm]()
		{
			SetCVar(TEXT("r.Shadow.Virtual.Enable"), Vsm);
			if (UDirectionalLightComponent* Sun = Active->Sun.Get())
			{
				Sun->SetWorldRotation(FRotator(SunPitch, Active->SunOriginal.Yaw, 0.0));
			}
			if (AStaticMeshActor* Roof = Active->Roof.Get())
			{
				Roof->SetActorHiddenInGame(!bRoof);
				Roof->GetStaticMeshComponent()->SetVisibility(bRoof);
			}
			PlaceViewer(700.0);
			PlaceSubjectFar();
			Log(FString::Printf(TEXT("{\"event\":\"cell\",\"cell\":\"%s\",\"sunPitch\":%.1f,\"roof\":%d,\"vsm\":%d,\"lumen\":\"software\",\"hwrt\":\"N/A-by-config\"}"), *Cell, SunPitch, bRoof ? 1 : 0, Vsm));
			return true;
		} });
		AddCapture(Cell + TEXT("_EmptyA"), SettleFrames * 15, 6); // the sky light and Lumen GI re-converge after the sun moves (seconds)
		Active->Steps.Add({ 1, []() { PlaceSubjectAtEntry(); return true; } });
		AddCapture(Cell + TEXT("_Visible"), SettleFrames, 6);
		AddEnter(Cell + TEXT("_Enter"));
		AddCapture(Cell + TEXT("_Hidden"), SettleFrames, 6);
		AddExit(Cell + TEXT("_Exit"));
		Active->Steps.Add({ 1, []() { PlaceSubjectFar(); return true; } });
		AddCapture(Cell + TEXT("_EmptyB"), SettleFrames, 6);
	}

	void AddLodRows(int32 SettleFrames)
	{
		Active->Steps.Add({ 2, []()
		{
			SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
			if (UDirectionalLightComponent* Sun = Active->Sun.Get())
			{
				Sun->SetWorldRotation(FRotator(-80.0, Active->SunOriginal.Yaw, 0.0));
			}
			if (AStaticMeshActor* Roof = Active->Roof.Get())
			{
				Roof->SetActorHiddenInGame(true);
				Roof->GetStaticMeshComponent()->SetVisibility(false);
			}
			PlaceViewer(700.0);
			return true;
		} });
		AddEnter(TEXT("Lod_Enter"));
		const AStealthPawn* Subject = Active->Subject.Get();
		const int32 Lods = Subject != nullptr && Subject->GetMesh() != nullptr ? FMath::Max(Subject->GetMesh()->GetNumLODs(), 1) : 1;
		for (int32 Lod = 0; Lod < Lods; ++Lod)
		{
			Active->Steps.Add({ 1, [Lod]()
			{
				if (AStealthPawn* S = Active->Subject.Get())
				{
					S->GetMesh()->SetForcedLOD(Lod + 1);
				}
				Log(FString::Printf(TEXT("{\"event\":\"lod\",\"lod\":%d}"), Lod));
				return true;
			} });
			AddCapture(FString::Printf(TEXT("Lod%d_Hidden"), Lod), SettleFrames / 2, 3);
		}
		Active->Steps.Add({ 1, []()
		{
			if (AStealthPawn* S = Active->Subject.Get())
			{
				S->GetMesh()->SetForcedLOD(0);
			}
			return true;
		} });
		AddExit(TEXT("Lod_Exit"));
		Active->Steps.Add({ 1, []() { PlaceSubjectFar(); return true; } });
		AddCapture(TEXT("Lod_Empty"), SettleFrames / 2, 3);
	}

	void SetMagenta(bool bOn)
	{
		AStealthPawn* Subject = Active->Subject.Get();
		USkeletalMeshComponent* Mesh = Subject != nullptr ? Subject->GetMesh() : nullptr;
		if (Mesh == nullptr)
		{
			return;
		}
		if (bOn)
		{
			// The occlusion proxy: the subject's own (skinned-mesh-ready) materials, tinted pure magenta through the appearance
			// colour parameter the character material exposes (an engine material without skeletal-mesh usage would render the
			// default material instead). Any drawn body pixel then reads as saturated magenta-red with no green.
			const UStealthSettings* Settings = UStealthSettings::Get();
			UMaterialInterface* PoolMaterial = Settings->AppearanceMaterial.LoadSynchronous();
			Active->OriginalMaterials.Reset();
			for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				Active->OriginalMaterials.Add(Mesh->GetMaterial(Slot));
				UMaterialInstanceDynamic* Dynamic = PoolMaterial != nullptr ? Mesh->CreateDynamicMaterialInstance(Slot, PoolMaterial) : Mesh->CreateAndSetMaterialInstanceDynamic(Slot);
				if (Dynamic == nullptr)
				{
					continue;
				}
				for (const FName& Parameter : Settings->AppearanceColorParameters)
				{
					Dynamic->SetVectorParameterValue(Parameter, FLinearColor(1.0f, 0.0f, 1.0f));
				}
			}
		}
		else
		{
			for (int32 Slot = 0; Slot < Active->OriginalMaterials.Num(); ++Slot)
			{
				Mesh->SetMaterial(Slot, Active->OriginalMaterials[Slot]);
			}
		}
	}

	void AddMagentaRows(int32 SettleFrames)
	{
		Active->Steps.Add({ 1, []() { SetMagenta(true); return true; } });
		for (const int32 Metres : { 5, 15, 25 })
		{
			const FString Prefix = FString::Printf(TEXT("Magenta%02dm"), Metres);
			Active->Steps.Add({ 1, [Metres, Prefix]()
			{
				PlaceViewer(Metres * 100.0);
				PlaceSubjectAtEntry();
				Log(FString::Printf(TEXT("{\"event\":\"magenta\",\"name\":\"%s\",\"distanceM\":%d}"), *Prefix, Metres));
				return true;
			} });
			AddCapture(Prefix + TEXT("_Visible"), SettleFrames / 2, 2);
			AddEnter(Prefix + TEXT("_Enter"));
			AddCapture(Prefix + TEXT("_Hidden"), SettleFrames / 2, 3);
			AddExit(Prefix + TEXT("_Exit"));
		}
		Active->Steps.Add({ 1, []() { SetMagenta(false); PlaceSubjectFar(); return true; } });
	}

	void Finish()
	{
		if (UDirectionalLightComponent* Sun = Active->Sun.Get())
		{
			Sun->SetWorldRotation(Active->SunOriginal);
			Sun->SetMobility(Active->SunMobility);
		}
		if (Active->PreviousParams.IsSet())
		{
			if (UConcealmentService* Service = UConcealmentService::Get(Active->World.Get()); Service != nullptr && Service->GetCore() != nullptr)
			{
				Service->GetCore()->SetParamsForTest(Active->PreviousParams.GetValue());
			}
		}
		SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
		SetCVar(TEXT("r.EyeAdaptationQuality"), 2);
		Log(TEXT("{\"event\":\"done\"}"));
		FFileHelper::SaveStringToFile(Active->Lines, *(Active->Dir / TEXT("r5matrix.jsonl")));
		UE_LOG(LogStealthR5Matrix, Display, TEXT("R5M DONE %s"), *Active->Dir);
	}

	bool Tick(float)
	{
		if (!Active.IsValid() || !Active->World.IsValid())
		{
			Active.Reset();
			return false;
		}
		if (Active->Wait > 0)
		{
			--Active->Wait;
			return true;
		}
		if (Active->Index >= Active->Steps.Num())
		{
			Finish();
			Active.Reset();
			return false;
		}
		FStep& Step = Active->Steps[Active->Index];
		if (Step.Do && !Step.Do())
		{
			return true; // the same step again next frame
		}
		++Active->Index;
		Active->Wait = Active->Index < Active->Steps.Num() ? Active->Steps[Active->Index].WaitFrames : 0;
		return true;
	}

	void Start(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Active.IsValid() || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogStealthR5Matrix, Warning, TEXT("R5M: needs a standalone or server PIE world with no run in progress"));
			return;
		}
		const int32 SettleFrames = Args.Num() > 0 ? FMath::Max(FCString::Atoi(*Args[0]), 8) : 60;
		Active = MakeUnique<FRun>();
		Active->World = World;
		Active->Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Harness/R5Matrix"));
		IFileManager::Get().DeleteDirectory(*Active->Dir, false, true);
		IFileManager::Get().MakeDirectory(*Active->Dir, true);
		UConcealmentService* Service = UConcealmentService::Get(World);
		const UBushRuntimeSubsystem* Bushes = UBushRuntimeSubsystem::Get(World);
		APlayerController* Viewer = World->GetFirstPlayerController();
		if (Service == nullptr || Service->GetCore() == nullptr || Bushes == nullptr || Bushes->GetBushes().IsEmpty() || Viewer == nullptr)
		{
			UE_LOG(LogStealthR5Matrix, Error, TEXT("R5M: Concealment is not bound, no bush, or no local player (start PIE on L_Prototype first)"));
			Active.Reset();
			return;
		}
		Active->Viewer = Viewer;
		Active->Bush = Bushes->GetBushes()[0];
		// Back-to-back hides: the re-entry cooldown lifted for the session (test seam), restored at the end.
		Active->PreviousParams = Service->GetCore()->GetParams();
		FConcealmentParams Lifted = Active->PreviousParams.GetValue();
		Lifted.Server.BushReenterCooldownSec = 0.0f;
		Service->GetCore()->SetParamsForTest(Lifted);

		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// The game's character class (the production pawn with the shipped mesh; an AStealthPawn subclass), as an unbound NPC.
		UClass* SubjectClass = UStealthSettings::Get()->CharacterClass.LoadSynchronous();
		AStealthPawn* Subject = World->SpawnActor<AStealthPawn>(SubjectClass != nullptr ? SubjectClass : AStealthPawn::StaticClass(), Active->Bush.Anchor + FVector(20000.0, 20000.0, 200.0), FRotator::ZeroRotator, Spawn);
		if (Subject == nullptr)
		{
			UE_LOG(LogStealthR5Matrix, Error, TEXT("R5M: the subject did not spawn"));
			Active.Reset();
			return;
		}
		Subject->GetCharacterMovement()->bRunPhysicsWithNoController = true; // an NPC identity without the crowd's controller
		Active->Subject = Subject;
		if (UClass* GroomClass = LoadObject<UClass>(nullptr, TEXT("/Script/HairStrandsCore.GroomComponent")))
		{
			UPrimitiveComponent* Groom = NewObject<UPrimitiveComponent>(Subject, GroomClass, TEXT("R5Groom"));
			if (FObjectProperty* Property = FindFProperty<FObjectProperty>(GroomClass, TEXT("GroomAsset")))
			{
				Property->SetObjectPropertyValue_InContainer(Groom, LoadObject<UObject>(nullptr, TEXT("/Game/MetaHumans/Kellan/MaleHair/Hair/Hair_S_360Waves.Hair_S_360Waves")));
			}
			Groom->SetupAttachment(Subject->GetMesh(), TEXT("head"));
			Groom->RegisterComponent();
		}
		if (UClass* NiagaraClass = LoadObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraComponent")))
		{
			UPrimitiveComponent* Niagara = NewObject<UPrimitiveComponent>(Subject, NiagaraClass, TEXT("R5Niagara"));
			if (FObjectProperty* Property = FindFProperty<FObjectProperty>(NiagaraClass, TEXT("Asset")))
			{
				Property->SetObjectPropertyValue_InContainer(Niagara, LoadObject<UObject>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight")));
			}
			Niagara->SetupAttachment(Subject->GetMesh());
			Niagara->SetRelativeLocation(FVector(0.0, 0.0, 120.0));
			Niagara->RegisterComponent();
			Niagara->Activate(true);
		}
		if (ACharacter* Own = Cast<ACharacter>(Viewer->GetPawn()))
		{
			Own->GetMesh()->SetVisibility(false, true); // the viewer is not the subject; its body would fill the frame
		}
		// Every other pawn (the crowd wandering through the frame) is hidden for the session: the empty-vs-empty noise floor
		// must measure the renderer, not people walking past (the spike set NpcCount 0 for the same reason).
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			if (*It != Subject && *It != Viewer->GetPawn())
			{
				It->SetActorHiddenInGame(true);
			}
		}
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Roof = World->SpawnActor<AStaticMeshActor>(Active->Bush.Anchor + FVector(0.0, 0.0, 450.0), FRotator::ZeroRotator, Spawn);
		if (Roof != nullptr && Cube != nullptr)
		{
			Roof->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Roof->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Roof->SetActorScale3D(FVector(6.0, 6.0, 0.2));
		}
		Active->Roof = Roof;
		for (TActorIterator<AActor> It(World); It && !Active->Sun.IsValid(); ++It)
		{
			if (UDirectionalLightComponent* Light = It->FindComponentByClass<UDirectionalLightComponent>())
			{
				Active->Sun = Light;
				Active->SunOriginal = Light->GetComponentRotation();
				Active->SunMobility = Light->Mobility;
				Light->SetMobility(EComponentMobility::Movable);
			}
		}
		// A fixed exposure for the session: auto exposure adapting between captures is not the subject's doing.
		SetCVar(TEXT("r.EyeAdaptationQuality"), 0);
		const IConsoleVariable* RayTracing = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
		Log(FString::Printf(TEXT("{\"event\":\"setup\",\"bush\":%d,\"anchor\":\"%s\",\"subject\":\"%s\",\"lods\":%d,\"sun\":%d,\"createRenderStateForHiddenWithCollision\":%d,\"rayTracing\":%d,\"settleFrames\":%d}"),
			Active->Bush.Id, *Active->Bush.Anchor.ToString(), *Subject->GetClass()->GetName(), Subject->GetMesh()->GetNumLODs(), Active->Sun.IsValid() ? 1 : 0,
			World->bCreateRenderStateForHiddenComponentsWithCollsion ? 1 : 0, RayTracing != nullptr ? RayTracing->GetInt() : -1, SettleFrames));

		AddCell(TEXT("LowSun_VSM1"), -10.0, false, 1, SettleFrames);
		AddCell(TEXT("LowSun_VSM0"), -10.0, false, 0, SettleFrames);
		AddCell(TEXT("Noon_VSM1"), -80.0, false, 1, SettleFrames);
		AddCell(TEXT("Noon_VSM0"), -80.0, false, 0, SettleFrames);
		AddCell(TEXT("Shade_VSM1"), -55.0, true, 1, SettleFrames);
		AddCell(TEXT("Shade_VSM0"), -55.0, true, 0, SettleFrames);
		AddLodRows(SettleFrames);
		AddMagentaRows(SettleFrames);
		Active->Wait = Active->Steps.Num() > 0 ? Active->Steps[0].WaitFrames : 0;
		Active->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdR5Matrix(TEXT("Stealth.R5Matrix"),
		TEXT("Concealment story con-034: the R5 leak matrix against the production hide (cells, swap frames, every LOD, magenta proxy at 5/15/25 m). Args: [SettleFrames=60]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
}

#endif
