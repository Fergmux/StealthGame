#include "StealthTestHarnessModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogStealthHarness);

/** Module object for StealthTestHarness. Registers nothing at startup (ADR-0007 section 4). */
class FStealthTestHarnessModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FStealthTestHarnessModule, StealthTestHarness);
