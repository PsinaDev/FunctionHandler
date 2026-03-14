// Copyright PsinaDev. All Rights Reserved.

#include "FunctionHandlerPinFactory.h"
#include "K2Node_SetFunctionHandlerParameter.h"
#include "K2Node_MakeFunctionHandler.h"
#include "SGraphPinParameterName.h"
#include "SGraphPinFunctionName.h"

TSharedPtr<SGraphPin> FFunctionHandlerPinFactory::CreatePin(UEdGraphPin* Pin) const
{
	if (!Pin)
	{
		return nullptr;
	}

	if (Pin->PinName == UK2Node_SetFunctionHandlerParameter::PN_ParameterName)
	{
		if (Cast<UK2Node_SetFunctionHandlerParameter>(Pin->GetOwningNode()))
		{
			return SNew(SGraphPinParameterName, Pin);
		}
	}

	if (Pin->PinName == UK2Node_MakeFunctionHandler::PN_FunctionName)
	{
		if (Cast<UK2Node_MakeFunctionHandler>(Pin->GetOwningNode()))
		{
			return SNew(SGraphPinFunctionName, Pin);
		}
	}

	return nullptr;
}
