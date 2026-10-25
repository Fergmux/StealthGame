#include "StealthPresentationModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogStealthPresentation);

/** Module object for StealthPresentation. Registers nothing at startup (ADR-0007 section 4). */
class FStealthPresentationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FStealthPresentationModule, StealthPresentation);

#if defined(WITH_STEALTH_TEST_SEAMS) && WITH_STEALTH_TEST_SEAMS
bool StealthPresentationSeesGuardDefine()
{
#if defined(STEALTH_PRESENTATION_MODULE) && STEALTH_PRESENTATION_MODULE
	return true;
#else
	return false;
#endif
}
#endif
