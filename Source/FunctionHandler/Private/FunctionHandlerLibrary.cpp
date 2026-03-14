// Copyright PsinaDev. All Rights Reserved.

#include "FunctionHandlerLibrary.h"
#include "FunctionCallResult.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"

#pragma region ExecuteFunctionByHandler

bool UFunctionHandlerLibrary::ExecuteFunctionByHandler(
	UObject* Target,
	const FFunctionHandler& Handler)
{
	if (!IsValid(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("FunctionHandler: Target is null or pending kill."));
		return false;
	}

	UFunction* Function = Handler.ResolveFunction(Target);
	if (!Function)
	{
		UE_LOG(LogTemp, Warning, TEXT("FunctionHandler: Function '%s' not found on %s."),
			*Handler.FunctionName.ToString(), *Target->GetPathName());
		return false;
	}

	if (Function->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate))
	{
		UE_LOG(LogTemp, Warning, TEXT("FunctionHandler: Cannot invoke delegate signature '%s'."),
			*Function->GetName());
		return false;
	}

	const int32 ParmsSize = Function->ParmsSize;
	uint8* ParamBuffer = nullptr;

	if (ParmsSize > 0)
	{
		ParamBuffer = static_cast<uint8*>(FMemory::Malloc(ParmsSize, Function->GetMinAlignment()));
		Function->InitializeStruct(ParamBuffer);
	}

	bool bImportOk = true;
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Prop = *It;

		if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}
		if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			continue;
		}

		const FString* StoredValue = Handler.ParameterValues.Find(Prop->GetFName());
		if (!StoredValue)
		{
			continue;
		}

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ParamBuffer);
		const TCHAR* Result = Prop->ImportText_Direct(**StoredValue, ValuePtr, Target, PPF_None);
		if (!Result)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("FunctionHandler: Failed to import value for parameter '%s' in function '%s'. Text: \"%s\""),
				*Prop->GetName(), *Function->GetName(), **StoredValue);
			bImportOk = false;
		}
	}

	if (!bImportOk)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("FunctionHandler: Some parameters failed to import for '%s'. Proceeding with defaults for those."),
			*Function->GetName());
	}

	Target->ProcessEvent(Function, ParamBuffer);

	if (ParamBuffer)
	{
		Function->DestroyStruct(ParamBuffer);
		FMemory::Free(ParamBuffer);
	}

	return true;
}

#pragma endregion ExecuteFunctionByHandler

#pragma region InternalK2Node

UFunctionCallResult* UFunctionHandlerLibrary::InternalExecuteWithResult(
	UObject* Target,
	const FFunctionHandler& Handler,
	bool& bSuccess)
{
	bSuccess = false;

	if (!IsValid(Target))
	{
		return nullptr;
	}

	UFunction* Function = Handler.ResolveFunction(Target);
	if (!Function)
	{
		return nullptr;
	}

	const int32 ParmsSize = Function->ParmsSize;
	if (ParmsSize <= 0)
	{
		Target->ProcessEvent(Function, nullptr);
		bSuccess = true;
		return nullptr;
	}

	TSharedPtr<FStructOnScope> Buffer = MakeShared<FStructOnScope>(Function);
	uint8* ParamBuffer = Buffer->GetStructMemory();

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Parm) || Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}
		if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			continue;
		}

		const FString* StoredValue = Handler.ParameterValues.Find(Prop->GetFName());
		if (!StoredValue || StoredValue->IsEmpty())
		{
			continue;
		}

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ParamBuffer);
		Prop->ImportText_Direct(**StoredValue, ValuePtr, Target, PPF_None);
	}

	Target->ProcessEvent(Function, ParamBuffer);
	bSuccess = true;

	UFunctionCallResult* Result = NewObject<UFunctionCallResult>();
	Result->ResultData = Buffer;
	Result->CachedFunction = Function;

	return Result;
}

DEFINE_FUNCTION(UFunctionHandlerLibrary::execGetResultByName)
{
	P_GET_OBJECT(UFunctionCallResult, Result);
	P_GET_PROPERTY(FNameProperty, ParameterName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* OutValuePtr = Stack.MostRecentPropertyAddress;
	FProperty* OutValueProp = Stack.MostRecentProperty;

	P_FINISH;

	if (!OutValuePtr || !OutValueProp || !Result || !Result->IsResultValid())
	{
		return;
	}

	UFunction* Func = Result->CachedFunction.Get();

	FProperty* SrcProp = Func->FindPropertyByName(ParameterName);
	if (!SrcProp && ParameterName == TEXT("ReturnValue"))
	{
		for (TFieldIterator<FProperty> It(Func); It; ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				SrcProp = *It;
				break;
			}
		}
	}

	if (!SrcProp || SrcProp->GetSize() != OutValueProp->GetSize())
	{
		return;
	}

	const void* SrcPtr = SrcProp->ContainerPtrToValuePtr<void>(Result->GetBuffer());
	SrcProp->CopySingleValue(OutValuePtr, SrcPtr);
}

DEFINE_FUNCTION(UFunctionHandlerLibrary::execInternalSetParameter)
{
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	FFunctionHandler* Handler = reinterpret_cast<FFunctionHandler*>(Stack.MostRecentPropertyAddress);

	P_GET_PROPERTY(FNameProperty, ParameterName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* ValuePtr = Stack.MostRecentPropertyAddress;
	FProperty* ValueProp = Stack.MostRecentProperty;

	P_FINISH;

	if (!Handler || !ValuePtr || !ValueProp || ParameterName.IsNone())
	{
		return;
	}

	FString ExportedValue;
	ValueProp->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
	Handler->ParameterValues.Add(ParameterName, MoveTemp(ExportedValue));
}

DEFINE_FUNCTION(UFunctionHandlerLibrary::execInternalGetParameter)
{
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	const FFunctionHandler* Handler = reinterpret_cast<const FFunctionHandler*>(Stack.MostRecentPropertyAddress);

	P_GET_PROPERTY(FNameProperty, ParameterName);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentProperty = nullptr;
	Stack.StepCompiledIn<FProperty>(nullptr);
	void* OutValuePtr = Stack.MostRecentPropertyAddress;
	FProperty* OutValueProp = Stack.MostRecentProperty;

	P_FINISH;

	if (!Handler || !OutValuePtr || !OutValueProp || ParameterName.IsNone())
	{
		return;
	}

	const FString* StoredValue = Handler->ParameterValues.Find(ParameterName);
	if (!StoredValue || StoredValue->IsEmpty())
	{
		return;
	}

	OutValueProp->ImportText_Direct(**StoredValue, OutValuePtr, nullptr, PPF_None);
}

void UFunctionHandlerLibrary::InternalMakeFunctionHandler(
	TSubclassOf<UObject> TargetClass,
	FName FunctionName,
	FFunctionHandler& OutHandler)
{
	OutHandler.TargetClass = TargetClass;
	OutHandler.FunctionName = FunctionName;
	OutHandler.ParameterValues.Empty();
}

#pragma endregion InternalK2Node

#pragma region SetParameterViaProperty

void UFunctionHandlerLibrary::SetParameterViaProperty(
	FFunctionHandler& Handler,
	FName ParameterName,
	const void* ValuePtr,
	FProperty* ExpectedProp)
{
	if (!ExpectedProp || !ValuePtr)
	{
		return;
	}

	FString ExportedValue;
	ExpectedProp->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);
	Handler.ParameterValues.Add(ParameterName, MoveTemp(ExportedValue));
}

#pragma endregion SetParameterViaProperty
