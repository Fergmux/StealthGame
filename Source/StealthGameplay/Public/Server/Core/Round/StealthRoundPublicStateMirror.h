#pragma once

#include "StealthServerOnlyGuard.h"

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/WeakObjectPtr.h"
#include "View/StealthRoundPublicState.h"

#include "StealthRoundPublicStateMirror.generated.h"

class FRoundPhaseCore;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class URoundPublicStateSink : public UInterface
{
	GENERATED_BODY()
};

/**
 * Where Round's public state is written (GDD round-match-framework R20; ADR-0002 section 8.10; story rmf-029): the
 * composition root's GameState implements it (one replicated FRoundPublicState, one RepNotify, unconditional). The
 * GameState is recreated at every travel, so the composition root registers the new one in InitGameState and Round
 * re-seeds it from the game-instance store before its first net update.
 */
class STEALTHGAMEPLAY_API IRoundPublicStateSink
{
	GENERATED_BODY()

public:
	/**
	 * Server: replaces the replicated struct, forces a net update and runs the client handler explicitly (a server
	 * receives no RepNotify).
	 */
	virtual void WriteRoundPublicState(const FRoundPublicState& State) = 0;
};

/**
 * FRoundPublicStateMirror: copies FRoundPhaseCore's public view into the registered sink (story rmf-029). Holds the
 * sink weakly; a dead or missing sink drops the write (the next InitGameState re-seeds the whole struct).
 */
class STEALTHGAMEPLAY_API FRoundPublicStateMirror
{
public:
	/** Registers the current GameState's sink and writes the whole struct to it at once (the re-seed). */
	void RegisterSink(IRoundPublicStateSink* Sink, UObject* Owner, const FRoundPhaseCore& Core);

	/** Writes Core's public state to the sink when it differs from the last write. Returns true when written. */
	bool Publish(const FRoundPhaseCore& Core);

	/** The last struct written (the host's view). */
	const FRoundPublicState& GetLastWritten() const { return LastWritten; }

private:
	IRoundPublicStateSink* Sink = nullptr;
	TWeakObjectPtr<UObject> SinkOwner;
	FRoundPublicState LastWritten;
	bool bHasWritten = false;
};
