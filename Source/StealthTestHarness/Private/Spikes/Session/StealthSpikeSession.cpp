// THROWAWAY spike code (session-travel stories 002-004). See StealthSpikeSession.h.
#include "Spikes/Session/StealthSpikeSession.h"

#include "Scenarios/StealthHarnessRun.h"
#include "Session/StealthConnectionEventHook.h"
#include "Spikes/Session/StealthSpikeMeasure.h"
#include "StealthGameState.h"
#include "StealthSettings.h"
#include "StealthTestHarnessModule.h"

#include "Engine/GameInstance.h"
#include "GameFramework/LocalMessage.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Crc.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

namespace StealthSpikeSessionPrivate
{
	static void Write(const TCHAR* Type, const FString& Fields)
	{
		if (StealthHarness::FRun* Run = StealthHarness::FRun::Get())
		{
			Run->Write(Type, Fields);
		}
	}

	template <typename StoreType>
	static StoreType* Store(const UObject* WorldContext)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<StoreType>() : nullptr;
	}

	static FString KeyPrefix(const FString& Key)
	{
		return Key.Left(8); // never more than a 4-byte prefix of a key in records (ADR-0005 section 3)
	}
}

bool& StealthSpikeSession::ClientExpectsReconnect()
{
	static bool bExpects = false;
	return bExpects;
}

// ---- Team view ------------------------------------------------------------------------------------

void FStealthSpikeTeamView::Seal()
{
	FString Text = FString::Printf(TEXT("%d|%d|%d|"), ParticipantId, (int32)Team, RoundEpoch);
	for (const int32 Id : TeammateIds)
	{
		Text += FString::Printf(TEXT("%d,"), Id);
	}
	Hash = FCrc::StrCrc32(*Text);
}

// ---- Stores ---------------------------------------------------------------------------------------

bool UStealthSpikeStoreBase::ShouldCreateSubsystem(UObject* Outer) const
{
	return FCString::Strifind(FCommandLine::Get(), TEXT("-StealthHarnessScenario=")) != nullptr;
}

uint32 UStealthSpikeStoreBase::ContentHash() const
{
	return FCrc::StrCrc32(*DescribeContent());
}

FString UStealthSpikeSessionStore::DescribeContent() const
{
	TArray<FString> Keys;
	Entries.GetKeys(Keys);
	Keys.Sort();
	FString Out = FString::Printf(TEXT("nonce=%s;supersedes=%d;"), *MatchNonce.ToString(), SupersedeCount);
	for (const FString& Key : Keys)
	{
		const FEntry& Entry = Entries[Key];
		Out += FString::Printf(TEXT("%s:%d:%d:%d;"), *StealthSpikeSessionPrivate::KeyPrefix(Key), Entry.ParticipantId, (int32)Entry.State, Entry.Generation);
	}
	return Out;
}

void UStealthSpikeSessionStore::BeginMatchIfNeeded()
{
	if (!MatchNonce.IsValid())
	{
		MatchNonce = FGuid::NewGuid();
	}
}

FString UStealthSpikeRosterStore::DescribeContent() const
{
	TArray<FString> Keys;
	Records.GetKeys(Keys);
	Keys.Sort();
	FString Out = FString::Printf(TEXT("match=%s;epoch=%d;seq=%d;next=%d;"), *MatchId.ToString(), RoundEpoch, EventSeq, NextId);
	for (const FString& Key : Keys)
	{
		const FRecord& Record = Records[Key];
		Out += FString::Printf(TEXT("%d:%s:%d;"), Record.Id, *Record.KeyPrefix, (int32)Record.Team);
	}
	return Out;
}

UStealthSpikeRosterStore::FRecord& UStealthSpikeRosterStore::FindOrRegister(const FString& Key, const FString& Name, uint8 Team)
{
	if (!MatchId.IsValid())
	{
		MatchId = FGuid::NewGuid();
	}
	if (FRecord* Existing = Records.Find(Key))
	{
		return *Existing;
	}
	FRecord& Record = Records.Add(Key);
	Record.Id = NextId++;
	Record.KeyPrefix = StealthSpikeSessionPrivate::KeyPrefix(Key);
	Record.Name = Name;
	Record.Team = Team;
	++EventSeq;
	return Record;
}

const UStealthSpikeRosterStore::FRecord* UStealthSpikeRosterStore::Find(const FString& Key) const
{
	return Records.Find(Key);
}

FStealthSpikeTeamView UStealthSpikeRosterStore::BuildTeamView(int32 Id) const
{
	FStealthSpikeTeamView View;
	const FRecord* Self = nullptr;
	for (const TPair<FString, FRecord>& Pair : Records)
	{
		if (Pair.Value.Id == Id)
		{
			Self = &Pair.Value;
		}
	}
	if (!Self)
	{
		return View;
	}
	View.ParticipantId = Id;
	View.Team = Self->Team;
	View.RoundEpoch = RoundEpoch;
	for (const TPair<FString, FRecord>& Pair : Records)
	{
		if (Pair.Value.Id != Id && Pair.Value.Team == Self->Team)
		{
			View.TeammateIds.Add(Pair.Value.Id);
		}
	}
	View.TeammateIds.Sort();
	View.Seal();
	return View;
}

FString UStealthSpikeRoundStore::DescribeContent() const
{
	return FString::Printf(TEXT("rounds=%d;score=%d-%d;"), RoundsPlayed, Score[0], Score[1]);
}

FString UStealthSpikeTuningStore::DescribeContent() const
{
	TArray<FString> Keys;
	Latched.GetKeys(Keys);
	Keys.Sort();
	FString Out = FString::Printf(TEXT("preset=%s;"), *PresetId);
	for (const FString& Key : Keys)
	{
		Out += FString::Printf(TEXT("%s=%.3f;"), *Key, Latched[Key]);
	}
	return Out;
}

// ---- PlayerState -----------------------------------------------------------------------------------

void AStealthSpikePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AStealthSpikePlayerState, TeamView, COND_OwnerOnly);
}

void AStealthSpikePlayerState::OnDeactivated()
{
	if (bRetainWhenDeactivated)
	{
		// ADR-0005 section 1: mid-round the PlayerState stays active, in the player array, unchanged.
		StealthSpikeSessionPrivate::Write(TEXT("psRetained"), FString::Printf(TEXT("\"ps\":%s,\"guid\":%s,%s"),
			*StealthHarness::FRun::Q(GetName()), *StealthHarness::FRun::Q(StealthHarness::FRun::Guid(GetWorld(), this)), *StealthHarness::FRun::ServerTimeFields(GetWorld())));
		return;
	}
	Super::OnDeactivated();
}

void AStealthSpikePlayerState::OnRep_TeamView()
{
	StealthSpikeSessionPrivate::Write(TEXT("teamView"), FString::Printf(TEXT("\"id\":%d,\"team\":%d,\"epoch\":%d,\"hash\":%u,\"mates\":%d"),
		TeamView.ParticipantId, (int32)TeamView.Team, TeamView.RoundEpoch, TeamView.Hash, TeamView.TeammateIds.Num()));
}

// ---- PlayerController --------------------------------------------------------------------------------

void AStealthSpikePlayerController::InitPlayerState()
{
	if (GetNetMode() != NM_Client)
	{
		if (AStealthSpikeSessionGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthSpikeSessionGameMode>())
		{
			if (APlayerState* Retained = GameMode->TakePendingReclaim().Get())
			{
				// ADR-0005 section 5: adopt the retained PlayerState; no PlayerState is spawned, the array is unchanged.
				SetPlayerState(Retained);
				Retained->SetOwner(this);
				Retained->OnReactivated();
				if (AStealthSpikePlayerState* SpikeState = Cast<AStealthSpikePlayerState>(Retained))
				{
					SpikeState->bRetainWhenDeactivated = false;
				}
				bAdoptedRetained = true;
				StealthSpikeSessionPrivate::Write(TEXT("psAdopted"), FString::Printf(TEXT("\"ps\":%s,\"guid\":%s,\"pc\":%s,%s"),
					*StealthHarness::FRun::Q(Retained->GetName()), *StealthHarness::FRun::Q(StealthHarness::FRun::Guid(GetWorld(), Retained)),
					*StealthHarness::FRun::Q(GetName()), *StealthHarness::FRun::ServerTimeFields(GetWorld())));
				return;
			}
		}
	}
	Super::InitPlayerState();
}

void AStealthSpikePlayerController::PawnLeavingGame()
{
	AStealthSpikeSessionGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AStealthSpikeSessionGameMode>() : nullptr;
	if (GameMode && GameMode->IsRoundPhase() && GetPawn())
	{
		// Keep the body (ADR-0010 hand-over shape; the spike just leaves it unpossessed).
		if (UStealthSpikeSessionStore* Session = StealthSpikeSessionPrivate::Store<UStealthSpikeSessionStore>(this))
		{
			const FString Key = GameMode->KeyOfPlayerState(PlayerState); // the connection is already gone here (E20)
			if (!Key.IsEmpty())
			{
				Session->RetainedPawns.Add(Key, GetPawn());
			}
		}
		UnPossess();
		return;
	}
	Super::PawnLeavingGame();
}

void AStealthSpikePlayerController::UpdatePing(float InPing)
{
	Super::UpdatePing(InPing);
	StealthHarness::FMeasure::AddRtt(IsLocalController() ? FString(TEXT("server")) : StealthHarness::FRun::PlayerName(this), InPing);
}

void AStealthSpikePlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
	StealthSpikeSessionPrivate::Write(TEXT("clientTravel"), FString::Printf(TEXT("\"seamless\":%d,\"travelType\":%d"), (int32)bIsSeamlessTravel, (int32)TravelType));
	if (!bIsSeamlessTravel)
	{
		StealthSpikeSession::ClientExpectsReconnect() = true;
	}
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void AStealthSpikePlayerController::ClientReceiveLocalizedMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1,
	APlayerState* RelatedPlayerState_2, UObject* OptionalObject)
{
	StealthSpikeSessionPrivate::Write(TEXT("localizedMessage"), FString::Printf(TEXT("\"cls\":%s,\"switch\":%d,\"ps1\":%s"),
		*StealthHarness::FRun::Q(GetNameSafe(Message.Get())), Switch, *StealthHarness::FRun::Q(GetNameSafe(RelatedPlayerState_1))));
	Super::ClientReceiveLocalizedMessage_Implementation(Message, Switch, RelatedPlayerState_1, RelatedPlayerState_2, OptionalObject);
}

void AStealthSpikePlayerController::ServerSpikeAck_Implementation(int32 Epoch, uint32 Hash)
{
	if (AStealthSpikeSessionGameMode* GameMode = GetWorld()->GetAuthGameMode<AStealthSpikeSessionGameMode>())
	{
		GameMode->ReceiveAck(this, Epoch, Hash);
	}
}

// ---- Game mode ----------------------------------------------------------------------------------------

AStealthSpikeSessionGameMode::AStealthSpikeSessionGameMode()
{
	PlayerStateClass = AStealthSpikePlayerState::StaticClass();
	PlayerControllerClass = AStealthSpikePlayerController::StaticClass();
	bUseSeamlessTravel = true; // ADR-0005 section 1
}

FString AStealthSpikeSessionGameMode::KeyOf(const APlayerController* Controller)
{
	if (!Controller)
	{
		return FString();
	}
	if (Controller->IsLocalController() && Controller->GetNetMode() != NM_Client)
	{
		return TEXT("host"); // FSessionPeerKey::Host: the listen host's player has no connection (ADR-0005 E2)
	}
	// RequestURL is the whole login URL (map and options), so parse it as a URL, not as an options string.
	return FStealthConnectionEventHook::IdentityKeyOf(Controller->GetNetConnection());
}

bool AStealthSpikeSessionGameMode::IsRoundPhase() const
{
	const AStealthGameState* GS = GetGameState<AStealthGameState>();
	return GS && GS->RoundPhase == EStealthRoundPhase::InProgress;
}

FString AStealthSpikeSessionGameMode::KeyOfPlayerState(const APlayerState* PlayerState) const
{
	const AStealthSpikePlayerState* PS = Cast<AStealthSpikePlayerState>(PlayerState);
	const UStealthSpikeSessionStore* Session = StealthSpikeSessionPrivate::Store<UStealthSpikeSessionStore>(this);
	if (!PS || !Session || PS->TeamView.ParticipantId < 0)
	{
		return FString();
	}
	for (const TPair<FString, UStealthSpikeSessionStore::FEntry>& Pair : Session->Entries)
	{
		if (Pair.Value.ParticipantId == PS->TeamView.ParticipantId)
		{
			return Pair.Key;
		}
	}
	return FString();
}

TWeakObjectPtr<APlayerState> AStealthSpikeSessionGameMode::TakePendingReclaim()
{
	TWeakObjectPtr<APlayerState> Out = PendingReclaim;
	PendingReclaim.Reset();
	return Out;
}

APlayerController* AStealthSpikeSessionGameMode::Login(UPlayer* NewPlayer, ENetRole InRemoteRole, const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	using namespace StealthSpikeSessionPrivate;
	const FString Key = UGameplayStatics::ParseOption(Options, TEXT("IdentityKey"));
	UStealthSpikeSessionStore* Session = Store<UStealthSpikeSessionStore>(this);
	PendingReclaim.Reset();
	PendingPawn.Reset();
	if (Session && !Key.IsEmpty())
	{
		// Supersede (rule 15): a live controller already holds the key. Drop its mapping first, then close it.
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Old = It->Get();
			UNetConnection* OldConnection = Old ? Old->GetNetConnection() : nullptr;
			if (!OldConnection || OldConnection->GetConnectionState() != USOCK_Open || KeyOf(Old) != Key)
			{
				continue;
			}
			++Session->SupersedeCount;
			PendingReclaim = Old->PlayerState;
			PendingPawn = Old->GetPawn();
			if (Old->GetPawn())
			{
				Old->UnPossess();
			}
			if (Old->PlayerState)
			{
				Old->PlayerState->SetOwner(nullptr);
			}
			Old->SetPlayerState(nullptr); // the old controller's destruction then neither logs out nor deactivates the PlayerState
			OldConnection->Close();
			Write(TEXT("supersede"), FString::Printf(TEXT("\"key\":%s,%s"), *StealthHarness::FRun::Q(KeyPrefix(Key)), *StealthHarness::FRun::ServerTimeFields(GetWorld())));
			break;
		}
		if (!PendingReclaim.IsValid())
		{
			if (const TWeakObjectPtr<APlayerState>* Retained = Session->RetainedPlayerStates.Find(Key); Retained && Retained->IsValid())
			{
				PendingReclaim = *Retained;
				if (const TWeakObjectPtr<APawn>* Pawn = Session->RetainedPawns.Find(Key))
				{
					PendingPawn = *Pawn;
				}
			}
		}
		Session->RetainedPlayerStates.Remove(Key);
		Session->RetainedPawns.Remove(Key);
	}
	APlayerController* Controller = Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
	PendingReclaim.Reset(); // consumed by InitPlayerState, or dropped on failure
	return Controller;
}

FString AStealthSpikeSessionGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	const AStealthSpikePlayerController* Spike = Cast<AStealthSpikePlayerController>(NewPlayerController);
	if (Spike && Spike->bAdoptedRetained)
	{
		return FString(); // ADR-0005 section 1: a reclaimed PlayerState is never re-registered or renamed
	}
	return Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
}

void AStealthSpikeSessionGameMode::PostLogin(APlayerController* NewPlayer)
{
	const AStealthSpikePlayerController* Spike = Cast<AStealthSpikePlayerController>(NewPlayer);
	if (Spike && Spike->bAdoptedRetained)
	{
		// Skip the prototype's team assignment: the record's team is restored by the rebind.
		AGameModeBase::PostLogin(NewPlayer);
		return;
	}
	Super::PostLogin(NewPlayer);
}

void AStealthSpikeSessionGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	using namespace StealthSpikeSessionPrivate;
	UStealthSpikeSessionStore* Session = Store<UStealthSpikeSessionStore>(this);
	UStealthSpikeRosterStore* Roster = Store<UStealthSpikeRosterStore>(this);
	AStealthSpikePlayerController* Spike = Cast<AStealthSpikePlayerController>(NewPlayer);
	const FString Key = KeyOf(NewPlayer);
	if (Session && Roster && !Key.IsEmpty())
	{
		Session->BeginMatchIfNeeded();
		if (UStealthSpikeTuningStore* Tuning = Store<UStealthSpikeTuningStore>(this); Tuning && Tuning->Latched.Num() == 0)
		{
			const UStealthSettings* Settings = UStealthSettings::Get();
			Tuning->Latched.Add(TEXT("RoundStartDelay"), Settings->RoundStartDelay);
			Tuning->Latched.Add(TEXT("DefenceDuration"), Settings->DefenceDuration);
			Tuning->Latched.Add(TEXT("AssassinationRange"), Settings->AssassinationRange);
		}
		const AStealthPlayerState* PS = NewPlayer->GetPlayerState<AStealthPlayerState>();
		const bool bKnown = Roster->Find(Key) != nullptr;
		UStealthSpikeRosterStore::FRecord& Record = Roster->FindOrRegister(Key, PS ? PS->GetPlayerName() : FString(), PS ? PS->Team : 255);
		UStealthSpikeSessionStore::FEntry& Entry = Session->Entries.FindOrAdd(Key);
		Entry.ParticipantId = Record.Id;
		if (Entry.State != UStealthSpikeSessionStore::EConnection::Connected)
		{
			Entry.State = UStealthSpikeSessionStore::EConnection::Connected;
			++Entry.Generation;
		}
		RebindView(NewPlayer);
		Write(TEXT("bind"), FString::Printf(TEXT("\"key\":%s,\"id\":%d,\"known\":%d,\"adopted\":%d,\"pc\":%s,\"ps\":%s,\"psGuid\":%s,%s"),
			*StealthHarness::FRun::Q(KeyPrefix(Key)), Record.Id, (int32)bKnown, (int32)(Spike && Spike->bAdoptedRetained), *StealthHarness::FRun::Q(NewPlayer->GetName()),
			*StealthHarness::FRun::Q(GetNameSafe(NewPlayer->PlayerState)), *StealthHarness::FRun::Q(StealthHarness::FRun::Guid(GetWorld(), NewPlayer->PlayerState)),
			*StealthHarness::FRun::ServerTimeFields(GetWorld())));
	}
	if (Spike && Spike->bAdoptedRetained && PendingPawn.IsValid() && !PendingPawn->GetController())
	{
		NewPlayer->Possess(PendingPawn.Get()); // the participant's own body, never a new pawn
		PendingPawn.Reset();
		return;
	}
	PendingPawn.Reset();
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void AStealthSpikeSessionGameMode::Logout(AController* Exiting)
{
	using namespace StealthSpikeSessionPrivate;
	APlayerController* Controller = Cast<APlayerController>(Exiting);
	UStealthSpikeSessionStore* Session = Store<UStealthSpikeSessionStore>(this);
	// The connection is already gone here (E20): find the key through the PlayerState's record binding.
	const FString Key = Controller ? KeyOfPlayerState(Controller->PlayerState) : FString();
	if (GetWorld()->bIsTearingDown || (Session && Session->bHardTravelInProgress))
	{
		// A hard travel unloads the world: the controllers go with it, the participants do not leave (rule 11.9).
		Write(TEXT("logoutAtTeardown"), FString::Printf(TEXT("\"key\":%s"), *StealthHarness::FRun::Q(KeyPrefix(Key))));
	}
	else if (Session && !Key.IsEmpty())
	{
		UStealthSpikeSessionStore::FEntry& Entry = Session->Entries[Key];
		Entry.State = UStealthSpikeSessionStore::EConnection::Disconnected;
		++Entry.Generation;
		AStealthSpikePlayerState* PS = Cast<AStealthSpikePlayerState>(Controller->PlayerState);
		if (PS && IsRoundPhase())
		{
			PS->bRetainWhenDeactivated = true;
			Session->RetainedPlayerStates.Add(Key, PS);
		}
		Write(TEXT("logout"), FString::Printf(TEXT("\"key\":%s,\"retained\":%d,%s"), *StealthHarness::FRun::Q(KeyPrefix(Key)),
			(int32)(PS && PS->bRetainWhenDeactivated), *StealthHarness::FRun::ServerTimeFields(GetWorld())));
	}
	Super::Logout(Exiting);
}

void AStealthSpikeSessionGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
	Super::HandleSeamlessTravelPlayer(C); // new controller and PlayerState (E5); HandleStartingNewPlayer re-binds
	StealthSpikeSessionPrivate::Write(TEXT("seamlessArrival"), FString::Printf(TEXT("\"pc\":%s,\"key\":%s,%s"),
		*StealthHarness::FRun::Q(GetNameSafe(C)), *StealthHarness::FRun::Q(StealthSpikeSessionPrivate::KeyPrefix(KeyOf(Cast<APlayerController>(C)))),
		*StealthHarness::FRun::ServerTimeFields(GetWorld())));
}

void AStealthSpikeSessionGameMode::PostSeamlessTravel()
{
	// T_srvload ends here: the server's own map is loaded (rule 20); the host is re-bound inside Super (E19).
	StealthSpikeSessionPrivate::Write(TEXT("postSeamlessTravel"), FString::Printf(TEXT("\"map\":%s,%s"),
		*StealthHarness::FRun::Q(GetWorld()->GetOutermost()->GetName()), *StealthHarness::FRun::ServerTimeFields(GetWorld())));
	Super::PostSeamlessTravel();
	// E21: PlayerStates carried without a controller (disconnected participants) are in the new array.
	if (GameState)
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (PS && !PS->GetOwningController())
			{
				StealthSpikeSessionPrivate::Write(TEXT("carriedPlayerState"), FString::Printf(TEXT("\"ps\":%s,\"guid\":%s,\"fromPrevious\":%d"),
					*StealthHarness::FRun::Q(PS->GetName()), *StealthHarness::FRun::Q(StealthHarness::FRun::Guid(GetWorld(), PS)), (int32)PS->IsFromPreviousLevel()));
			}
		}
	}
}

void AStealthSpikeSessionGameMode::RebindView(APlayerController* Controller)
{
	using namespace StealthSpikeSessionPrivate;
	UStealthSpikeRosterStore* Roster = Store<UStealthSpikeRosterStore>(this);
	UStealthSpikeSessionStore* Session = Store<UStealthSpikeSessionStore>(this);
	AStealthSpikePlayerState* PS = Controller ? Controller->GetPlayerState<AStealthSpikePlayerState>() : nullptr;
	const FString Key = KeyOf(Controller);
	const UStealthSpikeRosterStore::FRecord* Record = Roster ? Roster->Find(Key) : nullptr;
	if (!PS || !Record || !Session)
	{
		return;
	}
	PS->Team = Record->Team; // view field written from the record (Roster R4)
	PS->TeamView = Roster->BuildTeamView(Record->Id);
	PS->ForceNetUpdate();
	if (Key == TEXT("host"))
	{
		ReceiveAck(Controller, PS->TeamView.RoundEpoch, PS->TeamView.Hash); // the host acknowledges locally, no RPC
	}
}

void AStealthSpikeSessionGameMode::ReceiveAck(APlayerController* Controller, int32 Epoch, uint32 Hash)
{
	using namespace StealthSpikeSessionPrivate;
	UStealthSpikeRosterStore* Roster = Store<UStealthSpikeRosterStore>(this);
	UStealthSpikeSessionStore* Session = Store<UStealthSpikeSessionStore>(this);
	const FString Key = KeyOf(Controller); // the participant comes from the receiving connection, never a parameter
	const UStealthSpikeRosterStore::FRecord* Record = Roster ? Roster->Find(Key) : nullptr;
	if (!Record || !Session)
	{
		return;
	}
	const uint32 Expected = Roster->BuildTeamView(Record->Id).Hash;
	const bool bCurrent = Epoch == Roster->RoundEpoch;
	if (bCurrent)
	{
		Session->Acks.Add(Record->Id, Hash);
	}
	Write(TEXT("ack"), FString::Printf(TEXT("\"key\":%s,\"id\":%d,\"epoch\":%d,\"serverEpoch\":%d,\"hash\":%u,\"expected\":%u,\"match\":%d,%s"),
		*StealthHarness::FRun::Q(KeyPrefix(Key)), Record->Id, Epoch, Roster->RoundEpoch, Hash, Expected, (int32)(bCurrent && Hash == Expected),
		*StealthHarness::FRun::ServerTimeFields(GetWorld())));
}
