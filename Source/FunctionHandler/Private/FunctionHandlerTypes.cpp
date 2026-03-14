// Copyright PsinaDev. All Rights Reserved.

#include "FunctionHandlerTypes.h"
#include "UObject/Class.h"

UFunction* FFunctionHandler::ResolveFunction(const UObject* Target) const
{
	if (!Target || FunctionName.IsNone())
	{
		return nullptr;
	}

	return Target->FindFunction(FunctionName);
}

UFunction* FFunctionHandler::ResolveFunctionFromClass() const
{
	const UClass* Class = TargetClass.Get();
	if (!Class || FunctionName.IsNone())
	{
		return nullptr;
	}

	return Class->FindFunctionByName(FunctionName);
}
