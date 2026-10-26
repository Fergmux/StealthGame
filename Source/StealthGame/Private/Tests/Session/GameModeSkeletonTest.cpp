// Story st-023 (production/epics/session-travel/story-023-game-mode-skeleton-agamemodebase.md): the game mode, session
// and PlayerState classes have ADR-0005 section 1's shape (TR-session-007), checked by reflection and statically:
//   AStealthGameMode derives AGameModeBase and never AGameMode; bUseSeamlessTravel true, bStartPlayersAsSpectators false;
//   its GameSessionClass is AStealthGameSession, whose ApproveLogin always returns success; HandleStartingNewPlayer is a
//   no-op (empty body) and PlayerCanRestart returns false; AStealthPlayerState::ShouldBroadCastWelcomeMessage is false
//   in every phase (entering and exiting); DispatchPostLogin is not overridden (login uses PostLogin).
// Lives in the composition-root module (StealthGame), which owns these classes; the story's file list names a
// Foundation path that cannot include them (ADR-0007).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/GameMode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StealthGameMode.h"
#include "Server/StealthGameSession.h"
#include "StealthPlayerState.h"

namespace GameModeSkeletonTestData
{
	bool LoadSource(const TCHAR* Relative, FString& Out)
	{
		return FFileHelper::LoadFileToString(Out, *FPaths::Combine(FPaths::ProjectDir(), Relative));
	}

	/** The body of the first function definition named Signature in Text (between its braces), or empty. */
	FString FunctionBody(const FString& Text, const TCHAR* Signature)
	{
		const int32 At = Text.Find(Signature, ESearchCase::CaseSensitive);
		if (At == INDEX_NONE)
		{
			return FString();
		}
		const int32 Open = Text.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
		int32 Depth = 0;
		for (int32 Index = Open; Index != INDEX_NONE && Index < Text.Len(); ++Index)
		{
			Depth += Text[Index] == TEXT('{') ? 1 : (Text[Index] == TEXT('}') ? -1 : 0);
			if (Depth == 0)
			{
				return Text.Mid(Open + 1, Index - Open - 1);
			}
		}
		return FString();
	}

	/** Body text without line comments and whitespace (an empty function body becomes empty). */
	FString StripCode(const FString& Body)
	{
		TArray<FString> Lines;
		Body.ParseIntoArrayLines(Lines);
		FString Out;
		for (FString Line : Lines)
		{
			const int32 Comment = Line.Find(TEXT("//"));
			if (Comment != INDEX_NONE)
			{
				Line.LeftInline(Comment);
			}
			Out += Line.TrimStartAndEnd();
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionGameModeSkeletonTest, "StealthGame.Session.GameModeSkeleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionGameModeSkeletonTest::RunTest(const FString& Parameters)
{
	using namespace GameModeSkeletonTestData;

	// Reflection on the class defaults.
	const UClass* ModeClass = AStealthGameMode::StaticClass();
	TestTrue(TEXT("AStealthGameMode derives AGameModeBase"), ModeClass->IsChildOf(AGameModeBase::StaticClass()));
	TestFalse(TEXT("AStealthGameMode never derives AGameMode"), ModeClass->IsChildOf(AGameMode::StaticClass()));
	AStealthGameMode* Mode = GetMutableDefault<AStealthGameMode>();
	TestTrue(TEXT("bUseSeamlessTravel is true"), Mode->bUseSeamlessTravel);
	const FBoolProperty* Spectators = FindFProperty<FBoolProperty>(AGameModeBase::StaticClass(), TEXT("bStartPlayersAsSpectators"));
	if (TestNotNull(TEXT("bStartPlayersAsSpectators is reflected"), Spectators))
	{
		TestFalse(TEXT("bStartPlayersAsSpectators is false"), Spectators->GetPropertyValue_InContainer(Mode));
	}
	TestTrue(TEXT("GameSessionClass is AStealthGameSession"), Mode->GameSessionClass == AStealthGameSession::StaticClass());
	TestFalse(TEXT("PlayerCanRestart returns false"), Mode->PlayerCanRestart_Implementation(nullptr));

	AStealthGameSession* Session = GetMutableDefault<AStealthGameSession>();
	TestTrue(TEXT("ApproveLogin returns success (empty) for plain options"), Session->ApproveLogin(TEXT("?Name=A")).IsEmpty());
	TestTrue(TEXT("ApproveLogin returns success for a spectator request too"), Session->ApproveLogin(TEXT("?SpectatorOnly=1?Name=B")).IsEmpty());

	AStealthPlayerState* State = GetMutableDefault<AStealthPlayerState>();
	TestFalse(TEXT("ShouldBroadCastWelcomeMessage is false when entering"), State->ShouldBroadCastWelcomeMessage(false));
	TestFalse(TEXT("ShouldBroadCastWelcomeMessage is false when exiting"), State->ShouldBroadCastWelcomeMessage(true));

	// Static checks on the sources.
	FString ModeHeader, ModeSource;
	if (TestTrue(TEXT("StealthGameMode.h is readable"), LoadSource(TEXT("Source/StealthGame/Public/StealthGameMode.h"), ModeHeader))
		&& TestTrue(TEXT("StealthGameMode.cpp is readable"), LoadSource(TEXT("Source/StealthGame/Private/StealthGameMode.cpp"), ModeSource)))
	{
		TestFalse(TEXT("DispatchPostLogin is not overridden"), ModeHeader.Contains(TEXT("DispatchPostLogin")) || ModeSource.Contains(TEXT("::DispatchPostLogin")));
		TestTrue(TEXT("Login uses PostLogin"), ModeHeader.Contains(TEXT("virtual void PostLogin(")));
		const FString Body = FunctionBody(ModeSource, TEXT("AStealthGameMode::HandleStartingNewPlayer_Implementation"));
		TestTrue(TEXT("HandleStartingNewPlayer_Implementation is defined"), ModeSource.Contains(TEXT("AStealthGameMode::HandleStartingNewPlayer_Implementation")));
		TestTrue(TEXT("HandleStartingNewPlayer_Implementation is a no-op (empty body)"), StripCode(Body).IsEmpty());
		TestFalse(TEXT("No AGameMode base anywhere in the game mode header"), ModeHeader.Contains(TEXT("public AGameMode\n")) || ModeHeader.Contains(TEXT("public AGameMode ")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
