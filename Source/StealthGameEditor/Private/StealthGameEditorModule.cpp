#include "Modules/ModuleManager.h"
#include "StealthAssetToolset.h"
#include "StealthPlayToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

/** Editor-only module: registers the project's MCP toolsets. */
class FStealthGameEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolsetRegistry::RegisterToolsetClass(UStealthPlayToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UStealthAssetToolset::StaticClass());
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UToolsetRegistry::UnregisterToolsetClass(UStealthPlayToolset::StaticClass());
			UToolsetRegistry::UnregisterToolsetClass(UStealthAssetToolset::StaticClass());
		}
	}
};

IMPLEMENT_MODULE(FStealthGameEditorModule, StealthGameEditor);
