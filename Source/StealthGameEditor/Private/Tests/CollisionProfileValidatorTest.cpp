// Camera story cs-007 (production/epics/camera-system/story-007-viewblock-channel-collision-profiles.md; ADR-0015 section 4.1):
//   StealthGame.Camera.CollisionProfiles (AC1, AC2; the Config/Data smoke check): Config/DefaultEngine.ini declares exactly one
//     +DefaultChannelResponses named ViewBlock, a trace channel defaulting to Block, on the channel StealthCollision::ViewBlock
//     names; every row of StealthCollision::ProfileTable exists as a collision profile with the expected ViewBlock and Camera
//     responses, and the rows marked explicit write both in the profile's own CustomResponses.
//   StealthGame.Camera.CollisionProfileValidator (AC3): a body whose ViewBlock or Camera response differs from its profile's
//     expectation (an override makes the preset Custom: a Custom Pawn body is held to the Pawn row, any other Custom body
//     that blocks ViewBlock must block Camera) fails StealthCollisionAudit::CheckBody naming the channel and the response, and the editor validator marks a
//     static mesh asset carrying it Invalid (an ungoverned profile and a matching body pass; nothing is saved).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataValidationModule.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Validators/StealthCollisionProfileValidator.h"
#include "View/Core/StealthCollisionChannels.h"

namespace CameraCollisionProfilesTestData
{
	FString ReadIni()
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("DefaultEngine.ini")));
		return Text;
	}

	/** The +Profiles line of Name, or empty. */
	FString ProfileLine(const FString& Ini, const TCHAR* Name)
	{
		TArray<FString> Lines;
		Ini.ParseIntoArrayLines(Lines);
		const FString Key = FString::Printf(TEXT("+Profiles=(Name=\"%s\","), Name);
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(Key))
			{
				return Line;
			}
		}
		return FString();
	}

	const TCHAR* ResponseToken(ECollisionResponse Response)
	{
		return Response == ECR_Ignore ? TEXT("ECR_Ignore") : (Response == ECR_Overlap ? TEXT("ECR_Overlap") : TEXT("ECR_Block"));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraCollisionProfilesTest, "StealthGame.Camera.CollisionProfiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraCollisionProfilesTest::RunTest(const FString& Parameters)
{
	using namespace CameraCollisionProfilesTestData;
	const FString Ini = ReadIni();
	if (!TestFalse(TEXT("Config/DefaultEngine.ini is readable"), Ini.IsEmpty()))
	{
		return false;
	}
	// AC1: exactly one ViewBlock channel, trace type, default Block, on the constant's channel.
	TArray<FString> Lines;
	Ini.ParseIntoArrayLines(Lines);
	TArray<FString> ViewBlockLines;
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(TEXT("+DefaultChannelResponses=")) && Line.Contains(TEXT("Name=\"ViewBlock\"")))
		{
			ViewBlockLines.Add(Line);
		}
	}
	TestEqual(TEXT("exactly one +DefaultChannelResponses named ViewBlock"), ViewBlockLines.Num(), 1);
	if (ViewBlockLines.Num() == 1)
	{
		const FString& Line = ViewBlockLines[0];
		TestTrue(TEXT("ViewBlock: bTraceType=True"), Line.Contains(TEXT("bTraceType=True")));
		TestTrue(TEXT("ViewBlock: DefaultResponse=ECR_Block"), Line.Contains(TEXT("DefaultResponse=ECR_Block")));
		const FString ChannelName = StaticEnum<ECollisionChannel>()->GetNameStringByValue(static_cast<int64>(StealthCollision::ViewBlock));
		TestTrue(*FString::Printf(TEXT("the project constant StealthCollision::ViewBlock (%s) is the ini's channel"), *ChannelName), Line.Contains(FString::Printf(TEXT("Channel=%s,"), *ChannelName)));
	}

	// AC2: the table.
	for (const StealthCollision::FProfileExpectation& Row : StealthCollision::ProfileTable)
	{
		FCollisionResponseTemplate Template;
		if (!TestTrue(*FString::Printf(TEXT("profile %s (%s) exists"), Row.Profile, Row.Role), UCollisionProfile::Get()->GetProfileTemplate(FName(Row.Profile), Template)))
		{
			continue;
		}
		TestEqual(*FString::Printf(TEXT("%s: ViewBlock response"), Row.Profile), static_cast<int32>(Template.ResponseToChannels.GetResponse(StealthCollision::ViewBlock)), static_cast<int32>(Row.ViewBlock));
		TestEqual(*FString::Printf(TEXT("%s: Camera response"), Row.Profile), static_cast<int32>(Template.ResponseToChannels.GetResponse(ECC_Camera)), static_cast<int32>(Row.Camera));
		if (Row.bExplicit)
		{
			const FString Line = ProfileLine(Ini, Row.Profile);
			TestTrue(*FString::Printf(TEXT("%s: ViewBlock set explicitly (%s)"), Row.Profile, ResponseToken(Row.ViewBlock)),
				Line.Contains(FString::Printf(TEXT("(Channel=\"ViewBlock\",Response=%s)"), ResponseToken(Row.ViewBlock))));
			TestTrue(*FString::Printf(TEXT("%s: Camera set explicitly (%s)"), Row.Profile, ResponseToken(Row.Camera)),
				Line.Contains(FString::Printf(TEXT("(Channel=\"Camera\",Response=%s)"), ResponseToken(Row.Camera))));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCameraCollisionProfileValidatorTest, "StealthGame.Camera.CollisionProfileValidator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCameraCollisionProfileValidatorTest::RunTest(const FString& Parameters)
{
	// The pure check.
	FBodyInstance Matching;
	Matching.SetCollisionProfileName(TEXT("Pawn"));
	TestEqual(TEXT("a Pawn body as its profile prescribes: no mismatch"), StealthCollisionAudit::CheckBody(Matching).Num(), 0);
	FBodyInstance Drifted;
	Drifted.SetCollisionProfileName(TEXT("Pawn"));
	Drifted.SetResponseToChannel(StealthCollision::ViewBlock, ECR_Block);
	const TArray<FString> Mismatches = StealthCollisionAudit::CheckBody(Drifted);
	TestTrue(TEXT("a Pawn body that Blocks ViewBlock fails, naming the channel and the response"),
		Mismatches.Num() == 1 && Mismatches[0].Contains(TEXT("ViewBlock: Block")) && Mismatches[0].Contains(TEXT("'Pawn'")));
	TestEqual(TEXT("an overridden response turns the preset Custom (as the editor does)"), Drifted.GetCollisionProfileName(), UCollisionProfile::CustomCollisionProfileName);
	FBodyInstance Ungoverned;
	Ungoverned.SetCollisionProfileName(TEXT("OverlapAll"));
	TestEqual(TEXT("a profile outside the table is not governed"), StealthCollisionAudit::CheckBody(Ungoverned).Num(), 0);
	FBodyInstance CustomGlass;
	CustomGlass.SetCollisionProfileName(TEXT("BlockAll"));
	CustomGlass.SetResponseToChannel(StealthCollision::ViewBlock, ECR_Ignore);
	TestEqual(TEXT("a Custom non-pawn body seen through but blocking the boom passes"), StealthCollisionAudit::CheckBody(CustomGlass).Num(), 0);
	FBodyInstance CustomLeak;
	CustomLeak.SetCollisionProfileName(TEXT("BlockAll"));
	CustomLeak.SetResponseToChannel(ECC_Camera, ECR_Ignore);
	TestEqual(TEXT("a Custom sight blocker the boom passes through fails"), StealthCollisionAudit::CheckBody(CustomLeak).Num(), 1);

	// The validator on a transient static mesh (a kit asset): Camera drifted from StealthKit's Block.
	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("CameraValidatorKitMesh"));
	Mesh->CreateBodySetup();
	UBodySetup* Setup = Mesh->GetBodySetup();
	if (!TestNotNull(TEXT("the transient kit mesh has a body setup"), Setup))
	{
		return false;
	}
	Setup->DefaultInstance.SetCollisionProfileName(TEXT("StealthKit"));
	UStealthCollisionProfileValidator* Validator = NewObject<UStealthCollisionProfileValidator>();
	FDataValidationContext Context;
	const FAssetData AssetData(Mesh);
	Validator->ResetValidationState();
	TestEqual(TEXT("a kit mesh matching StealthKit is Valid"), static_cast<int32>(Validator->ValidateLoadedAsset(AssetData, Mesh, Context)), static_cast<int32>(EDataValidationResult::Valid));
	Setup->DefaultInstance.SetResponseToChannel(ECC_Camera, ECR_Ignore);
	FDataValidationContext Failing;
	Validator->ResetValidationState();
	TestEqual(TEXT("a kit mesh ignoring the Camera channel is Invalid"), static_cast<int32>(Validator->ValidateLoadedAsset(AssetData, Mesh, Failing)), static_cast<int32>(EDataValidationResult::Invalid));
	Validator->ExtractValidationState(Failing);
	bool bNamed = false;
	for (const FDataValidationContext::FIssue& Issue : Failing.GetIssues())
	{
		const FString Text = Issue.TokenizedMessage.IsValid() ? Issue.TokenizedMessage->ToText().ToString() : Issue.Message.ToString();
		bNamed |= Text.Contains(TEXT("CameraValidatorKitMesh")) && Text.Contains(TEXT("Camera: Ignore"));
	}
	TestTrue(TEXT("the failure names the asset and the response"), bNamed);
	return true;
}

#endif
