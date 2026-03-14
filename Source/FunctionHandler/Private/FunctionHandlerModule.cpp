// Copyright PsinaDev. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FFunctionHandlerModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FFunctionHandlerModule, FunctionHandler)
