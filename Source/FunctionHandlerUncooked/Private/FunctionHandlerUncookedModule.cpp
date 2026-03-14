// Copyright PsinaDev. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "EdGraphUtilities.h"
#include "FunctionHandlerPinFactory.h"

class FFunctionHandlerUncookedModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		PinFactory = MakeShared<FFunctionHandlerPinFactory>();
		FEdGraphUtilities::RegisterVisualPinFactory(PinFactory);
	}

	virtual void ShutdownModule() override
	{
		if (PinFactory.IsValid())
		{
			FEdGraphUtilities::UnregisterVisualPinFactory(PinFactory);
			PinFactory.Reset();
		}
	}

private:
	TSharedPtr<FFunctionHandlerPinFactory> PinFactory;
};

IMPLEMENT_MODULE(FFunctionHandlerUncookedModule, FunctionHandlerUncooked)
