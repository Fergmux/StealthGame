#pragma once

// Story production/epics/session-travel/story-001-harness-connection-event-hook.md (TR-session-006,
// ADR-0005 section 5, GDD Open Question 2): the test hook that forces connection events through the
// real net driver, never by editing Session state:
//   (a) Drop      a chosen client's connection (client quit, server-side close, or a silent outage);
//   (b) Rejoin    a client reconnects with an IdentityKey login option (a known key = a rejoin);
//   (c) Supersede a second connection with the key of a live participant;
//   (d) Kick      the host kicks a chosen participant (AGameSession::KickPlayer, the engine's path).
// Works for single-process PIE (T2, CQTest) and for the multi-process harness (T3): a client is named by
// its world-context handle, which survives the map loads a rejoin causes, and every observation comes
// from the engine's own login/logout events and connection objects.
//
// Test-only module code (StealthTestHarness never ships, ADR-0007). Until story 026 lands, the
// IdentityKey is only a login option (`?IdentityKey=`) that this hook writes and reads back from
// UNetConnection::RequestURL; no game code parses it yet.

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtr.h"

class AController;
class AGameModeBase;
class APlayerController;
class UNetConnection;
class UWorld;

/** How Drop takes a client off the server. */
enum class EStealthDropMode : uint8
{
	/** The client leaves: the engine's `disconnect` path (close bunch sent, client travels to its default map). */
	ClientQuit,
	/** The server closes the client's UNetConnection (the shape of a Session-initiated close). */
	ServerClose,
	/** The client goes silent: 100 % packet loss both ways on its net driver, connection left open (a Hard-timeout case). */
	Silent,
};

/** One server-side login or logout seen by the hook. */
struct FStealthConnectionObservation
{
	enum class EKind : uint8 { Login, Logout };

	EKind Kind = EKind::Login;
	/** The `IdentityKey` login option of the connection ("" when the connection carried none). */
	FString IdentityKey;
	/** The `Name` login option of the connection. */
	FString Name;
	/** The connection that logged in or out (weak: it is destroyed after a logout). */
	TWeakObjectPtr<UNetConnection> Connection;
	/** GFrameCounter at the event. */
	uint64 Frame = 0;
};

/**
 * Forces and observes connection events. Construct it with the server world (listen host); it listens to
 * FGameModeEvents for as long as it lives. Client-side calls take a world-context handle so they keep
 * working after the client's world is replaced by a travel.
 */
class FStealthConnectionEventHook
{
public:
	/** Starts observing logins and logouts on ServerWorld (may be null on a client-only process). */
	explicit FStealthConnectionEventHook(UWorld* ServerWorld);
	~FStealthConnectionEventHook();

	FStealthConnectionEventHook(const FStealthConnectionEventHook&) = delete;
	FStealthConnectionEventHook& operator=(const FStealthConnectionEventHook&) = delete;

	/** Deterministic 128-bit test key (32 hex digits) for a harness slot; distinct slots never collide. */
	static FString IdentityKeyForSlot(int32 Slot);

	/** The `IdentityKey` option of the URL Connection logged in with, or "" (server side). */
	static FString IdentityKeyOf(const UNetConnection* Connection);

	/** The `Name` option of the URL Connection logged in with, or "" (server side). */
	static FString LoginNameOf(const UNetConnection* Connection);

	/** World-context handle of World (NAME_None when World has no context). */
	static FName ContextOf(const UWorld* World);

	/** The current world of the context Handle, or null. */
	static UWorld* WorldOf(FName Handle);

	/** True when the client world of Handle has an open server connection. */
	static bool IsClientConnected(FName Handle);

	/**
	 * (a) Takes the client of Handle off the server in Mode. ClientQuit and Silent act on the client side;
	 * ServerClose needs this hook's server world. Returns false (OutError filled) when the client is not connected.
	 */
	bool Drop(FName ClientHandle, EStealthDropMode Mode, FString* OutError = nullptr);

	/** Ends a Silent drop: restores the client's packet settings. */
	bool ResumeFromSilent(FName ClientHandle, FString* OutError = nullptr);

	/**
	 * (b) The client of Handle (re)connects to the server it last joined (or Address, when given, "host:port")
	 * with `?Name=<Name>?IdentityKey=<IdentityKey>`. A live connection is closed first by the engine's browse.
	 * Returns false when no server address is known.
	 */
	static bool Rejoin(FName ClientHandle, const FString& IdentityKey, const FString& Name, const FString& Address = FString(), FString* OutError = nullptr);

	/**
	 * (c) Opens a second connection that carries the key of a live participant: the client of Handle rejoins
	 * with IdentityKey. Refuses (false) unless the server currently has a live connection with that key and it
	 * is not the client's own connection, so the call always produces a supersede shape.
	 */
	bool Supersede(FName ClientHandle, const FString& IdentityKey, const FString& Name, FString* OutError = nullptr);

	/** (d) The host kicks the participant whose live connection carries IdentityKey (AGameSession::KickPlayer). */
	bool Kick(const FString& IdentityKey, const FText& Reason, FString* OutError = nullptr);

	/** (d) The host kicks the participant on Connection. */
	bool KickConnection(UNetConnection* Connection, const FText& Reason, FString* OutError = nullptr);

	/** Server: open client connections whose login carried IdentityKey. */
	TArray<UNetConnection*> LiveConnectionsWithKey(const FString& IdentityKey) const;

	/** Server: the server-side connection of the client of Handle (matched by the client's local port), or null. */
	UNetConnection* ServerConnectionForClient(FName ClientHandle) const;

	/** Every login and logout seen since construction, in order. */
	const TArray<FStealthConnectionObservation>& GetObservations() const { return Observations; }

	/** Number of observations of Kind with IdentityKey. */
	int32 CountObservations(FStealthConnectionObservation::EKind Kind, const FString& IdentityKey) const;

private:
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);
	bool IsServerWorld(const AGameModeBase* GameMode) const;

	TWeakObjectPtr<UWorld> ServerWorld;
	FDelegateHandle PostLoginHandle;
	FDelegateHandle LogoutHandle;
	TArray<FStealthConnectionObservation> Observations;

	/** What a controller logged in with, kept until its logout (the connection is gone by then). */
	struct FLoginRecord
	{
		FString IdentityKey;
		FString Name;
		TWeakObjectPtr<UNetConnection> Connection;
	};
	TMap<TObjectKey<AController>, FLoginRecord> LoginsByController;
};
