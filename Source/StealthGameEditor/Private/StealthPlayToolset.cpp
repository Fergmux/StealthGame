#include "StealthPlayToolset.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UnrealEngine.h"

namespace
{
	FString NetModeName(ENetMode Mode)
	{
		switch (Mode)
		{
		case NM_Standalone: return TEXT("Standalone");
		case NM_ListenServer: return TEXT("ListenServer");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_Client: return TEXT("Client");
		default: return TEXT("Unknown");
		}
	}

	UWorld* FindPlayWorld(int32 InstanceIndex)
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.PIEInstance == InstanceIndex)
			{
				return Context.World();
			}
		}
		return nullptr;
	}
}

TArray<FStealthPlayInstance> UStealthPlayToolset::ListPlayInstances()
{
	TArray<FStealthPlayInstance> Result;
	if (!GEngine)
	{
		return Result;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType != EWorldType::PIE || !World)
		{
			continue;
		}
		FStealthPlayInstance& Instance = Result.AddDefaulted_GetRef();
		Instance.Index = Context.PIEInstance;
		Instance.NetMode = NetModeName(World->GetNetMode());
		Instance.WorldName = World->GetName();
	}
	Result.Sort([](const FStealthPlayInstance& A, const FStealthPlayInstance& B) { return A.Index < B.Index; });
	return Result;
}

void UStealthPlayToolset::RunPlayConsoleCommand(int32 InstanceIndex, const FString& Command)
{
	if (Command.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Command must not be empty."));
		return;
	}
	UWorld* World = FindPlayWorld(InstanceIndex);
	if (!World)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("No running PIE instance with index %d."), InstanceIndex));
		return;
	}
	APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		UKismetSystemLibrary::RaiseScriptError(
			FString::Printf(TEXT("PIE instance %d has no local player controller."), InstanceIndex));
		return;
	}
	// Run in that instance's PIE context, as its own console would.
	FTemporaryPlayInEditorIDOverride PlayInEditorScope(InstanceIndex);
	PlayerController->ConsoleCommand(Command, true);
}
