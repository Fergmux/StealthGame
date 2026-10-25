#include "StealthGameplayModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogStealthGameplay);

/** Module object for StealthGameplay. Registers nothing at startup (ADR-0007 section 4). */
class FStealthGameplayModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FStealthGameplayModule, StealthGameplay);
