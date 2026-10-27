// Stories asn-018 / asn-020 / asn-021: the encounter presentation settings. See View/EncounterPresentationSettings.h.

#include "View/EncounterPresentationSettings.h"

#include "Animation/AnimMontage.h"

namespace EncounterPresentationSettingsLocal
{
	const TCHAR* const StrikePlaceholder = TEXT("/Game/StealthGame/Animations/Assassination/AM_ASN_Strike_D54Placeholder.AM_ASN_Strike_D54Placeholder");
	const TCHAR* const CounterPlaceholder = TEXT("/Game/StealthGame/Animations/Assassination/AM_ASN_Counter_D54Placeholder.AM_ASN_Counter_D54Placeholder");
	const TCHAR* const FallPlaceholder = TEXT("/Game/StealthGame/Animations/Assassination/AM_ASN_DeathFall_D54Placeholder.AM_ASN_DeathFall_D54Placeholder");
}

UEncounterPresentationSettings::UEncounterPresentationSettings()
{
	using namespace EncounterPresentationSettingsLocal;
	CategoryName = TEXT("Game");
	// PLACEHOLDERS (see the class comment): one D54 strike montage for all three entry variants.
	StrikeFMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(StrikePlaceholder));
	StrikeL60Montage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(StrikePlaceholder));
	StrikeR60Montage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(StrikePlaceholder));
	CounterMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(CounterPlaceholder));
	DeathFallMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(FallPlaceholder));
}

const UEncounterPresentationSettings* UEncounterPresentationSettings::Get()
{
	return GetDefault<UEncounterPresentationSettings>();
}
