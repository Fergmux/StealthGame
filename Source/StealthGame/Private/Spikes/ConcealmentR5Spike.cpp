// THROWAWAY SPIKE CODE: Concealment story con-021, spike R5 (production/epics/concealment-bushes/
// story-021-spike-r5-hidden-primitive-leaks.md; ADR-0017 section 6, Verification Required R5). Report:
// production/qa/spikes/2026-10-28_R5.md.
//
// Run in a rendering PIE session (one player, standalone) on L_Prototype:  Stealth.Spike.R5 [SettleFrames]
// A subject (the game's configured character class, with a groom from the MetaHuman Kellan content attached to the head
// and an engine-template Niagara system attached to the mesh) stands on open ground 7 m ahead of the local player's own
// camera (the player's pawn frozen and hidden, so the view holds still), framing it and the ground its shadow falls on. For each matrix cell (sun low / noon / shade under a roof occluder, x VSM on / off;
// Lumen software tracing - the project has r.RayTracing=False, so the hardware-RT cells cannot run in this build) the
// spike captures four states: EmptyA (subject 200 m away), Visible, Hidden (the production rule shape: SetVisibility(false,
// true) on the mesh, the three gating flags left at their class defaults, no bHidden), EmptyB. Each state is six screenshots
// after SettleFrames frames (default 120: VSM pages and Lumen's temporal accumulation converge). Per hidden state the spike
// also records whether each primitive still has a render proxy (SceneProxy) - the engine-level answer the pixels confirm.
// Then it profiles hide/show: game-thread cost of the SetVisibility pair and the render-thread frame time with a toggle
// every frame versus none. Output: Saved/Harness/R5/ (PNGs + r5.jsonl); tools/analysis/r5_diff.py computes the noise
// floor (the 99th percentile of |EmptyA - EmptyB|) and the leak (|Hidden - EmptyA| above that floor).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderTimer.h"
#include "StealthCharacter.h"
#include "StealthSettings.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogStealthR5, Log, All);

namespace StealthR5Spike
{
	struct FStep
	{
		int32 WaitFrames = 0;
		TFunction<void()> Do;
	};

	struct FRun
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<ACharacter> Subject;
		TWeakObjectPtr<UDirectionalLightComponent> Sun;
		TWeakObjectPtr<AStaticMeshActor> Roof;
		FRotator SunOriginal = FRotator::ZeroRotator;
		EComponentMobility::Type SunMobility = EComponentMobility::Movable;
		FVector Spot = FVector::ZeroVector;
		FString Dir;
		FString Lines;
		TArray<FStep> Steps;
		int32 Index = 0;
		int32 Wait = 0;
		FTSTicker::FDelegateHandle Handle;
		// Profile accumulators.
		double ToggleGameMs = 0.0;
		int32 ToggleCount = 0;
		TArray<double> RenderMsStatic;
		TArray<double> RenderMsToggling;
		TArray<double> GameMsStatic;
		TArray<double> GameMsToggling;
	};

	static TUniquePtr<FRun> Active;

	void Log(const FString& Json)
	{
		Active->Lines += Json + TEXT("\n");
		UE_LOG(LogStealthR5, Display, TEXT("R5 %s"), *Json);
	}

	void SetCVar(const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByConsole);
		}
	}

	void SetHidden(bool bHidden)
	{
		ACharacter* Subject = Active->Subject.Get();
		if (Subject == nullptr)
		{
			return;
		}
		// The production rule shape (UConcealmentComponent::ApplyHidden): the mesh with propagation to every attachment.
		Subject->GetMesh()->SetVisibility(!bHidden, true);
	}

	void Place(bool bAtSpot)
	{
		ACharacter* Subject = Active->Subject.Get();
		if (Subject == nullptr)
		{
			return;
		}
		const double HalfHeight = Subject->GetSimpleCollisionHalfHeight();
		const FVector At = bAtSpot ? Active->Spot + FVector(0.0, 0.0, HalfHeight + 2.0) : Active->Spot + FVector(20000.0, 20000.0, 5000.0);
		Subject->SetActorLocationAndRotation(At, FRotator(0.0, -135.0, 0.0), false, nullptr, ETeleportType::TeleportPhysics);
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

	void AddCapture(const FString& Cell, const FString& State, int32 SettleFrames)
	{
		TArray<FStep>& Steps = Active->Steps;
		Steps.Add({ SettleFrames, [Cell, State]()
		{
			Log(FString::Printf(TEXT("{\"event\":\"state\",\"cell\":\"%s\",\"state\":\"%s\",\"primitives\":[%s]}"), *Cell, *State, *ProxyReport()));
		} });
		for (int32 Shot = 0; Shot < 6; ++Shot)
		{
			Steps.Add({ 4, [Cell, State, Shot]()
			{
				const FString File = Active->Dir / FString::Printf(TEXT("%s_%s_%d.png"), *Cell, *State, Shot);
				FScreenshotRequest::RequestScreenshot(File, false, false);
				Log(FString::Printf(TEXT("{\"event\":\"shot\",\"cell\":\"%s\",\"state\":\"%s\",\"file\":\"%s\"}"), *Cell, *State, *FPaths::GetCleanFilename(File)));
			} });
		}
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
				Roof->SetActorHiddenInGame(!bRoof); // the occluder, not the subject
				Roof->GetStaticMeshComponent()->SetVisibility(bRoof);
			}
			Place(false);
			SetHidden(false);
			Log(FString::Printf(TEXT("{\"event\":\"cell\",\"cell\":\"%s\",\"sunPitch\":%.1f,\"roof\":%d,\"vsm\":%d,\"lumen\":\"software\"}"), *Cell, SunPitch, bRoof ? 1 : 0, Vsm));
		} });
		AddCapture(Cell, TEXT("EmptyA"), SettleFrames);
		Active->Steps.Add({ 1, []() { Place(true); SetHidden(false); } });
		AddCapture(Cell, TEXT("Visible"), SettleFrames);
		Active->Steps.Add({ 1, []() { SetHidden(true); } });
		AddCapture(Cell, TEXT("Hidden"), SettleFrames);
		Active->Steps.Add({ 1, []() { SetHidden(false); Place(false); } });
		AddCapture(Cell, TEXT("EmptyB"), SettleFrames);
	}

	void AddProfile()
	{
		Active->Steps.Add({ 30, []() { Place(true); SetHidden(false); } });
		for (int32 Frame = 0; Frame < 120; ++Frame)
		{
			Active->Steps.Add({ 1, []()
			{
				Active->RenderMsStatic.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
				Active->GameMsStatic.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
			} });
		}
		for (int32 Frame = 0; Frame < 120; ++Frame)
		{
			Active->Steps.Add({ 1, [Frame]()
			{
				Active->RenderMsToggling.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
				Active->GameMsToggling.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
				const double Start = FPlatformTime::Seconds();
				SetHidden(Frame % 2 == 0);
				Active->ToggleGameMs += (FPlatformTime::Seconds() - Start) * 1000.0;
				++Active->ToggleCount;
			} });
		}
		Active->Steps.Add({ 2, []()
		{
			auto Mean = [](const TArray<double>& V) { double S = 0.0; for (double X : V) { S += X; } return V.Num() > 0 ? S / V.Num() : 0.0; };
			Log(FString::Printf(TEXT("{\"event\":\"profile\",\"toggles\":%d,\"setVisibilityGameMsMean\":%.4f,\"renderMsStatic\":%.3f,\"renderMsToggling\":%.3f,\"gameMsStatic\":%.3f,\"gameMsToggling\":%.3f}"),
				Active->ToggleCount, Active->ToggleCount > 0 ? Active->ToggleGameMs / Active->ToggleCount : 0.0, Mean(Active->RenderMsStatic), Mean(Active->RenderMsToggling),
				Mean(Active->GameMsStatic), Mean(Active->GameMsToggling)));
			SetHidden(false);
		} });
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
			if (UDirectionalLightComponent* Sun = Active->Sun.Get())
			{
				Sun->SetWorldRotation(Active->SunOriginal);
				Sun->SetMobility(Active->SunMobility);
			}
			SetCVar(TEXT("r.Shadow.Virtual.Enable"), 1);
			Log(TEXT("{\"event\":\"done\"}"));
			FFileHelper::SaveStringToFile(Active->Lines, *(Active->Dir / TEXT("r5.jsonl")));
			UE_LOG(LogStealthR5, Display, TEXT("R5 DONE %s"), *Active->Dir);
			Active.Reset();
			return false;
		}
		FStep& Step = Active->Steps[Active->Index++];
		if (Step.Do)
		{
			Step.Do();
		}
		Active->Wait = Active->Index < Active->Steps.Num() ? Active->Steps[Active->Index].WaitFrames : 0;
		return true;
	}

	void Start(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Active.IsValid())
		{
			UE_LOG(LogStealthR5, Warning, TEXT("R5: no world, or a run is in progress"));
			return;
		}
		const int32 SettleFrames = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 120;
		// "profile": the hide/show profile only, into Harness/R5_profile (D56: re-run under the min-spec PROXY affinity).
		const bool bProfileOnly = Args.Num() > 1 && Args[1].Equals(TEXT("profile"), ESearchCase::IgnoreCase);
		Active = MakeUnique<FRun>();
		Active->World = World;
		Active->Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / (bProfileOnly ? TEXT("Harness/R5_profile") : TEXT("Harness/R5")));
		IFileManager::Get().DeleteDirectory(*Active->Dir, false, true);
		IFileManager::Get().MakeDirectory(*Active->Dir, true);

		// The view: the local player's own camera (the sample's camera system owns the view target, so the spike does not
		// replace it). The player's pawn is frozen and hidden (it is not the subject), so the camera holds still. The spot:
		// open ground 7 m ahead of the camera and 1.5 m to its right (ground traced).
		APlayerController* Viewer = World->GetFirstPlayerController();
		if (Viewer == nullptr || Viewer->PlayerCameraManager == nullptr)
		{
			UE_LOG(LogStealthR5, Error, TEXT("R5: no local player camera"));
			Active.Reset();
			return;
		}
		if (ACharacter* Own = Cast<ACharacter>(Viewer->GetPawn()))
		{
			Own->GetCharacterMovement()->DisableMovement();
			Own->GetMesh()->SetVisibility(false, true);
		}
		const FVector CamLoc = Viewer->PlayerCameraManager->GetCameraLocation();
		const FRotator CamRot = Viewer->PlayerCameraManager->GetCameraRotation();
		const FVector Forward = FRotator(0.0, CamRot.Yaw, 0.0).Vector();
		const FVector Right = FRotator(0.0, CamRot.Yaw + 90.0, 0.0).Vector();
		Active->Spot = CamLoc + Forward * 700.0 + Right * 150.0;
		FHitResult Ground;
		if (World->LineTraceSingleByChannel(Ground, Active->Spot + FVector(0.0, 0.0, 500.0), Active->Spot - FVector(0.0, 0.0, 2000.0), ECC_Visibility))
		{
			Active->Spot = Ground.ImpactPoint;
		}

		// The subject: the game's character class, a groom on the head and a Niagara system on the mesh.
		UClass* SubjectClass = UStealthSettings::Get()->CharacterClass.LoadSynchronous();
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* Subject = World->SpawnActor<ACharacter>(SubjectClass != nullptr ? SubjectClass : ACharacter::StaticClass(), Active->Spot, FRotator::ZeroRotator, Spawn);
		Active->Subject = Subject;
		if (Subject == nullptr)
		{
			UE_LOG(LogStealthR5, Error, TEXT("R5: the subject did not spawn"));
			Active.Reset();
			return;
		}
		Subject->GetCharacterMovement()->DisableMovement();
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

		// The roof occluder for the shade cell: a 6 x 6 m slab 4 m above the spot.
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		AStaticMeshActor* Roof = World->SpawnActor<AStaticMeshActor>(Active->Spot + FVector(0.0, 0.0, 400.0), FRotator::ZeroRotator, Spawn);
		if (Roof != nullptr && Cube != nullptr)
		{
			Roof->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Roof->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Roof->SetActorScale3D(FVector(6.0, 6.0, 0.2));
		}
		Active->Roof = Roof;
		// The sun: the first directional light component in the level (a sky Blueprint carries it on L_Prototype); made
		// Movable for the session so the cells can turn it, restored at the end.
		for (TActorIterator<AActor> It(World); It && !Active->Sun.IsValid(); ++It)
		{
			if (UDirectionalLightComponent* Light = It->FindComponentByClass<UDirectionalLightComponent>())
			{
				Active->Sun = Light;
				Active->SunOriginal = Light->GetComponentRotation();
				Active->SunMobility = Light->Mobility;
				Light->SetMobility(EComponentMobility::Movable);
				UE_LOG(LogStealthR5, Display, TEXT("R5 sun: %s on %s (mobility %d)"), *Light->GetName(), *It->GetName(), static_cast<int32>(Active->SunMobility));
			}
		}
		Log(FString::Printf(TEXT("{\"event\":\"setup\",\"spot\":\"%s\",\"subject\":\"%s\",\"sun\":%d,\"createRenderStateForHiddenWithCollision\":%d,\"rayTracing\":%d,\"settleFrames\":%d}"),
			*Active->Spot.ToString(), *Subject->GetClass()->GetName(), Active->Sun.IsValid() ? 1 : 0, World->bCreateRenderStateForHiddenComponentsWithCollsion ? 1 : 0,
			IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing")) ? IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"))->GetInt() : -1, SettleFrames));

		if (!bProfileOnly)
		{
			AddCell(TEXT("LowSun_VSM1"), -10.0, false, 1, SettleFrames);
			AddCell(TEXT("LowSun_VSM0"), -10.0, false, 0, SettleFrames);
			AddCell(TEXT("Noon_VSM1"), -80.0, false, 1, SettleFrames);
			AddCell(TEXT("Noon_VSM0"), -80.0, false, 0, SettleFrames);
			AddCell(TEXT("Shade_VSM1"), -55.0, true, 1, SettleFrames);
			AddCell(TEXT("Shade_VSM0"), -55.0, true, 0, SettleFrames);
		}
		AddProfile();
		Active->Wait = Active->Steps.Num() > 0 ? Active->Steps[0].WaitFrames : 0;
		Active->Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdR5(TEXT("Stealth.Spike.R5"),
		TEXT("SPIKE R5 (Concealment story con-021): hidden-primitive leak captures and the hide/show profile. Args: [SettleFrames=120] [profile]."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
}

#endif
