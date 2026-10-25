#include "StealthTestHarnessModule.h"
#include "Modules/ModuleManager.h"
#include "Capture/StealthNetCapture.h"
#include "Scenarios/StealthHarnessRun.h"

DEFINE_LOG_CATEGORY(LogStealthHarness);

/**
 * Module object for StealthTestHarness. Registers nothing at startup (ADR-0007 section 4) unless the
 * process was launched as a T3 harness process (`-StealthHarnessScenario=<Name>`, ADR-0008 section 1).
 */
class FStealthTestHarnessModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		StealthHarness::Capture::InstallIfRequested();
		StealthHarness::FRun::StartIfRequested();
	}
	virtual void ShutdownModule() override { StealthHarness::FRun::Shutdown(); }
};

IMPLEMENT_MODULE(FStealthTestHarnessModule, StealthTestHarness);
