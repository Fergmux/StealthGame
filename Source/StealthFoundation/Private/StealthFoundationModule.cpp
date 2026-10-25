#include "StealthFoundationModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogStealthFoundation);

/**
 * Module object for StealthFoundation. StartupModule stays empty: implementations are
 * registered by the composition root, never from module startup (ADR-0007 section 4).
 */
class FStealthFoundationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FStealthFoundationModule, StealthFoundation);
