#pragma once

#include "ToolsetRegistry/ToolsetDefinition.h"

#include "StealthPlayToolset.generated.h"

/** One running Play-In-Editor instance. */
USTRUCT(BlueprintType)
struct FStealthPlayInstance
{
	GENERATED_BODY()

	/** PIE instance index (0 = listen server or standalone host). */
	UPROPERTY()
	int32 Index = 0;

	/** "Standalone", "ListenServer", "DedicatedServer" or "Client". */
	UPROPERTY()
	FString NetMode;

	/** Name of the instance's world. */
	UPROPERTY()
	FString WorldName;
};

/**
 * Drives running Play-In-Editor sessions from outside the game: lists PIE instances and runs
 * console commands (including game Exec commands such as StealthBot) as a given instance's local player.
 */
UCLASS(BlueprintType, MinimalAPI)
class UStealthPlayToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Lists the running PIE instances, ordered by index.
	 * @return Empty when no play session is running.
	 */
	UFUNCTION(meta = (AICallable), Category = "StealthPlayToolset")
	static TArray<FStealthPlayInstance> ListPlayInstances();

	/**
	 * Runs a console command as the first local player of a PIE instance, as if typed into that
	 * instance's console.
	 * @param InstanceIndex PIE instance index from ListPlayInstances.
	 * @param Command The full console command line.
	 */
	UFUNCTION(meta = (AICallable), Category = "StealthPlayToolset")
	static void RunPlayConsoleCommand(int32 InstanceIndex, const FString& Command);
};
