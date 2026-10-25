// Story session-travel 001: connection-event test hook. See StealthConnectionEventHook.h.
#include "Session/StealthConnectionEventHook.h"

#include "StealthTestHarnessModule.h"

#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "IPAddress.h"

namespace StealthConnectionEventHookPrivate
{
	/** Option value of Key in a login URL ("" when absent). FURL parses the `?Key=Value` options the engine stored. */
	static FString UrlOption(const FString& RequestUrl, const TCHAR* Key)
	{
		if (RequestUrl.IsEmpty())
		{
			return FString();
		}
		const FURL Url(nullptr, *RequestUrl, TRAVEL_Absolute);
		return FString(Url.GetOption(*FString::Printf(TEXT("%s="), Key), TEXT("")));
	}

	static UNetDriver* ClientDriver(FName Handle)
	{
		const UWorld* World = FStealthConnectionEventHook::WorldOf(Handle);
		UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		return Driver && Driver->ServerConnection ? Driver : nullptr;
	}

	static void SetError(FString* OutError, const FString& Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		UE_LOGF(LogStealthHarness, Warning, "Connection hook: %ls", *Message);
	}
}

FStealthConnectionEventHook::FStealthConnectionEventHook(UWorld* InServerWorld)
	: ServerWorld(InServerWorld)
{
	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddRaw(this, &FStealthConnectionEventHook::HandlePostLogin);
	LogoutHandle = FGameModeEvents::OnGameModeLogoutEvent().AddRaw(this, &FStealthConnectionEventHook::HandleLogout);
	// Connections that logged in before the hook existed: remember them so their logout is observed too.
	if (const UNetDriver* Driver = InServerWorld ? InServerWorld->GetNetDriver() : nullptr)
	{
		for (UNetConnection* Connection : Driver->ClientConnections)
		{
			if (Connection && Connection->PlayerController)
			{
				LoginsByController.Add(Connection->PlayerController, FLoginRecord{ IdentityKeyOf(Connection), LoginNameOf(Connection), Connection });
			}
		}
	}
}

FStealthConnectionEventHook::~FStealthConnectionEventHook()
{
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	FGameModeEvents::OnGameModeLogoutEvent().Remove(LogoutHandle);
}

FString FStealthConnectionEventHook::IdentityKeyForSlot(int32 Slot)
{
	// The slot as 8 hex digits, then "Stel" in hex and zeros: deterministic, 128 bits, distinct per slot (also in
	// the 4-byte prefix records may show), and easy to produce outside the engine (run-mp.ps1 -IdentityKeys).
	return FString::Printf(TEXT("%08x5374656c0000000000000000"), Slot);
}

FString FStealthConnectionEventHook::IdentityKeyOf(const UNetConnection* Connection)
{
	return Connection ? StealthConnectionEventHookPrivate::UrlOption(Connection->RequestURL, TEXT("IdentityKey")) : FString();
}

FString FStealthConnectionEventHook::LoginNameOf(const UNetConnection* Connection)
{
	return Connection ? StealthConnectionEventHookPrivate::UrlOption(Connection->RequestURL, TEXT("Name")) : FString();
}

FName FStealthConnectionEventHook::ContextOf(const UWorld* World)
{
	const FWorldContext* Context = (GEngine && World) ? GEngine->GetWorldContextFromWorld(World) : nullptr;
	return Context ? Context->ContextHandle : NAME_None;
}

UWorld* FStealthConnectionEventHook::WorldOf(FName Handle)
{
	const FWorldContext* Context = (GEngine && Handle != NAME_None) ? GEngine->GetWorldContextFromHandle(Handle) : nullptr;
	return Context ? Context->World() : nullptr;
}

bool FStealthConnectionEventHook::IsClientConnected(FName Handle)
{
	const UNetDriver* Driver = StealthConnectionEventHookPrivate::ClientDriver(Handle);
	const UWorld* World = WorldOf(Handle);
	return Driver && World && World->GetNetMode() == NM_Client && Driver->ServerConnection->GetConnectionState() == USOCK_Open;
}

bool FStealthConnectionEventHook::Drop(FName ClientHandle, EStealthDropMode Mode, FString* OutError)
{
	using namespace StealthConnectionEventHookPrivate;
	UWorld* ClientWorld = WorldOf(ClientHandle);
	if (!IsClientConnected(ClientHandle))
	{
		SetError(OutError, FString::Printf(TEXT("Drop: client %s is not connected"), *ClientHandle.ToString()));
		return false;
	}
	switch (Mode)
	{
	case EStealthDropMode::ClientQuit:
		// The engine's own leave path: HandleDisconnect -> SetClientTravel("?closed"); the net driver is shut down
		// (close bunch sent) when the client loads its default map.
		GEngine->HandleDisconnect(ClientWorld, ClientWorld->GetNetDriver());
		return true;
	case EStealthDropMode::ServerClose:
	{
		UNetConnection* Connection = ServerConnectionForClient(ClientHandle);
		if (!Connection)
		{
			SetError(OutError, TEXT("Drop(ServerClose): no server-side connection matches the client"));
			return false;
		}
		Connection->Close();
		return true;
	}
	case EStealthDropMode::Silent:
	{
#if DO_ENABLE_NET_TEST
		UNetDriver* Driver = ClientWorld->GetNetDriver();
		FPacketSimulationSettings Settings = Driver->PacketSimulationSettings;
		Settings.PktLoss = 100;
		Settings.PktIncomingLoss = 100;
		Driver->SetPacketSimulationSettings(Settings);
		return true;
#else
		SetError(OutError, TEXT("Drop(Silent) needs DO_ENABLE_NET_TEST (non-Shipping)"));
		return false;
#endif
	}
	}
	return false;
}

bool FStealthConnectionEventHook::ResumeFromSilent(FName ClientHandle, FString* OutError)
{
#if DO_ENABLE_NET_TEST
	UNetDriver* Driver = StealthConnectionEventHookPrivate::ClientDriver(ClientHandle);
	if (!Driver)
	{
		StealthConnectionEventHookPrivate::SetError(OutError, TEXT("ResumeFromSilent: client has no server connection"));
		return false;
	}
	FPacketSimulationSettings Settings = Driver->PacketSimulationSettings;
	Settings.PktLoss = 0;
	Settings.PktIncomingLoss = 0;
	Driver->SetPacketSimulationSettings(Settings);
	return true;
#else
	StealthConnectionEventHookPrivate::SetError(OutError, TEXT("ResumeFromSilent needs DO_ENABLE_NET_TEST"));
	return false;
#endif
}

bool FStealthConnectionEventHook::Rejoin(FName ClientHandle, const FString& IdentityKey, const FString& Name, const FString& Address, FString* OutError)
{
	using namespace StealthConnectionEventHookPrivate;
	FWorldContext* Context = GEngine ? GEngine->GetWorldContextFromHandle(ClientHandle) : nullptr;
	if (!Context || !Context->World())
	{
		SetError(OutError, FString::Printf(TEXT("Rejoin: no world context %s"), *ClientHandle.ToString()));
		return false;
	}
	FString Target = Address;
	if (Target.IsEmpty())
	{
		// The address of the last server this context joined; it survives the ?closed travel of a drop.
		if (!Context->LastRemoteURL.Valid || Context->LastRemoteURL.Host.IsEmpty())
		{
			SetError(OutError, TEXT("Rejoin: the client never joined a server and no address was given"));
			return false;
		}
		Target = FString::Printf(TEXT("%s:%d"), *Context->LastRemoteURL.Host, Context->LastRemoteURL.Port);
	}
	FString Url = Target;
	if (!Name.IsEmpty())
	{
		Url += FString::Printf(TEXT("?Name=%s"), *Name);
	}
	if (!IdentityKey.IsEmpty())
	{
		Url += FString::Printf(TEXT("?IdentityKey=%s"), *IdentityKey);
	}
	// Browse (next engine tick) shuts the current world's net driver down, if any, and starts a pending net game.
	GEngine->SetClientTravel(Context->World(), *Url, TRAVEL_Absolute);
	UE_LOGF(LogStealthHarness, Display, "Connection hook: rejoin %ls -> %ls (key %.8ls...)", *ClientHandle.ToString(), *Target, *IdentityKey);
	return true;
}

bool FStealthConnectionEventHook::Supersede(FName ClientHandle, const FString& IdentityKey, const FString& Name, FString* OutError)
{
	using namespace StealthConnectionEventHookPrivate;
	const TArray<UNetConnection*> Live = LiveConnectionsWithKey(IdentityKey);
	if (Live.Num() == 0)
	{
		SetError(OutError, TEXT("Supersede: no live connection carries the key, so this would be a plain join"));
		return false;
	}
	const UNetConnection* Own = ServerConnectionForClient(ClientHandle);
	if (Live.Num() == 1 && Live[0] == Own)
	{
		SetError(OutError, TEXT("Supersede: the only live connection with the key is the client's own"));
		return false;
	}
	return Rejoin(ClientHandle, IdentityKey, Name, FString(), OutError);
}

bool FStealthConnectionEventHook::Kick(const FString& IdentityKey, const FText& Reason, FString* OutError)
{
	const TArray<UNetConnection*> Live = LiveConnectionsWithKey(IdentityKey);
	if (Live.Num() != 1)
	{
		StealthConnectionEventHookPrivate::SetError(OutError, FString::Printf(TEXT("Kick: %d live connections carry the key (need exactly 1)"), Live.Num()));
		return false;
	}
	return KickConnection(Live[0], Reason, OutError);
}

bool FStealthConnectionEventHook::KickConnection(UNetConnection* Connection, const FText& Reason, FString* OutError)
{
	UWorld* World = ServerWorld.Get();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	APlayerController* PC = Connection ? Connection->PlayerController : nullptr;
	if (!GameMode || !GameMode->GameSession || !PC)
	{
		StealthConnectionEventHookPrivate::SetError(OutError, TEXT("Kick: needs the server game mode, its game session and the connection's controller"));
		return false;
	}
	// The engine kick: destroys the pawn and the controller; destroying a remote controller closes its connection.
	if (!GameMode->GameSession->KickPlayer(PC, Reason))
	{
		StealthConnectionEventHookPrivate::SetError(OutError, TEXT("Kick: AGameSession::KickPlayer refused"));
		return false;
	}
	return true;
}

TArray<UNetConnection*> FStealthConnectionEventHook::LiveConnectionsWithKey(const FString& IdentityKey) const
{
	TArray<UNetConnection*> Out;
	const UWorld* World = ServerWorld.Get();
	const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
	if (!Driver || IdentityKey.IsEmpty())
	{
		return Out;
	}
	for (UNetConnection* Connection : Driver->ClientConnections)
	{
		if (Connection && Connection->GetConnectionState() == USOCK_Open && IdentityKeyOf(Connection) == IdentityKey)
		{
			Out.Add(Connection);
		}
	}
	return Out;
}

UNetConnection* FStealthConnectionEventHook::ServerConnectionForClient(FName ClientHandle) const
{
	const UWorld* World = ServerWorld.Get();
	const UNetDriver* ServerDriver = World ? World->GetNetDriver() : nullptr;
	UNetDriver* Driver = StealthConnectionEventHookPrivate::ClientDriver(ClientHandle);
	const TSharedPtr<const FInternetAddr> Local = Driver ? Driver->GetLocalAddr() : nullptr;
	if (!ServerDriver || !Local.IsValid())
	{
		return nullptr;
	}
	const int32 Port = Local->GetPort();
	for (UNetConnection* Connection : ServerDriver->ClientConnections)
	{
		const TSharedPtr<const FInternetAddr> Remote = Connection ? Connection->GetRemoteAddr() : nullptr;
		if (Remote.IsValid() && Remote->GetPort() == Port)
		{
			return Connection;
		}
	}
	return nullptr;
}

int32 FStealthConnectionEventHook::CountObservations(FStealthConnectionObservation::EKind Kind, const FString& IdentityKey) const
{
	int32 Count = 0;
	for (const FStealthConnectionObservation& Observation : Observations)
	{
		Count += (Observation.Kind == Kind && Observation.IdentityKey == IdentityKey) ? 1 : 0;
	}
	return Count;
}

bool FStealthConnectionEventHook::IsServerWorld(const AGameModeBase* GameMode) const
{
	return GameMode && ServerWorld.IsValid() && GameMode->GetWorld() == ServerWorld.Get();
}

void FStealthConnectionEventHook::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!IsServerWorld(GameMode) || !NewPlayer)
	{
		return;
	}
	UNetConnection* Connection = NewPlayer->GetNetConnection();
	if (!Connection)
	{
		return; // the listen host's own player has no connection (ADR-0005 E2)
	}
	FStealthConnectionObservation& Observation = Observations.AddDefaulted_GetRef();
	Observation.Kind = FStealthConnectionObservation::EKind::Login;
	Observation.IdentityKey = IdentityKeyOf(Connection);
	Observation.Name = LoginNameOf(Connection);
	Observation.Connection = Connection;
	Observation.Frame = GFrameCounter;
	LoginsByController.Add(NewPlayer, FLoginRecord{ Observation.IdentityKey, Observation.Name, Connection });
}

void FStealthConnectionEventHook::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!IsServerWorld(GameMode))
	{
		return;
	}
	// By Logout the controller's connection is already null (ADR-0005 E20) and the engine's
	// GNetConnectionBeingCleanedUp is not exported: the login record of this controller names the connection.
	const FLoginRecord* Record = LoginsByController.Find(Exiting);
	if (!Record)
	{
		return; // the listen host's own player, or a controller that logged in before the hook existed
	}
	UNetConnection* Connection = Record->Connection.Get();
	const FString Key = Record->IdentityKey;
	const FString LoginName = Record->Name;
	LoginsByController.Remove(Exiting);
	FStealthConnectionObservation& Observation = Observations.AddDefaulted_GetRef();
	Observation.Kind = FStealthConnectionObservation::EKind::Logout;
	Observation.IdentityKey = Key;
	Observation.Name = LoginName;
	Observation.Connection = Connection;
	Observation.Frame = GFrameCounter;
}
