#include "StealthFoundationModule.h"
#include "Modules/ModuleManager.h"
#include "Server/Testing/StealthTestSeams.h"

DEFINE_LOG_CATEGORY(LogStealthFoundation);

/**
 * Module object for StealthFoundation. Startup registers no implementation: those are
 * registered by the composition root, never from module startup (ADR-0007 section 4). The one
 * exception is the test-seam sentinel, which is not an implementation (ADR-0008, story tdc-006).
 */
class FStealthFoundationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
#if WITH_STEALTH_TEST_SEAMS
		StealthRegisterTestSeamSentinel();
#endif
	}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FStealthFoundationModule, StealthFoundation);
