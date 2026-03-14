// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/UnrealType.h"
#include "FunctionHandlerTypes.h"
#include "FunctionCallResult.h"

#include "FunctionHandlerLibrary.generated.h"

UCLASS()
class FUNCTIONHANDLER_API UFunctionHandlerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "FunctionHandler",
		meta = (DisplayName = "Execute Function by Handler", ReturnDisplayName = "Success"))
	static bool ExecuteFunctionByHandler(
		UObject* Target,
		UPARAM(ref) const FFunctionHandler& Handler);

#pragma region InternalK2Node

	UFUNCTION(BlueprintCallable, Category = "FunctionHandler",
		meta = (BlueprintInternalUseOnly = "true"))
	static class UFunctionCallResult* InternalExecuteWithResult(
		UObject* Target,
		UPARAM(ref) const FFunctionHandler& Handler,
		bool& bSuccess);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "FunctionHandler",
		meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutValue"))
	static void GetResultByName(
		class UFunctionCallResult* Result,
		FName ParameterName,
		int32& OutValue);

	DECLARE_FUNCTION(execGetResultByName);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "FunctionHandler",
		meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value"))
	static void InternalSetParameter(
		UPARAM(ref) FFunctionHandler& Handler,
		FName ParameterName,
		int32 Value);

	DECLARE_FUNCTION(execInternalSetParameter);

	UFUNCTION(BlueprintCallable, CustomThunk, Category = "FunctionHandler",
		meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "OutValue"))
	static void InternalGetParameter(
		const FFunctionHandler& Handler,
		FName ParameterName,
		int32& OutValue);

	DECLARE_FUNCTION(execInternalGetParameter);

	UFUNCTION(BlueprintCallable, Category = "FunctionHandler",
		meta = (BlueprintInternalUseOnly = "true"))
	static void InternalMakeFunctionHandler(
		TSubclassOf<UObject> TargetClass,
		FName FunctionName,
		FFunctionHandler& OutHandler);

#pragma endregion InternalK2Node

	/** Generic typed setter for C++ usage. Resolves FProperty from TargetClass and exports via ExportTextItem_Direct. */
	template<typename T>
	static void SetParameter(FFunctionHandler& Handler, FName ParameterName, const T& Value);

private:

	static void SetParameterViaProperty(
		FFunctionHandler& Handler,
		FName ParameterName,
		const void* ValuePtr,
		FProperty* ExpectedProp);
};

template<typename T>
void UFunctionHandlerLibrary::SetParameter(FFunctionHandler& Handler, FName ParameterName, const T& Value)
{
	UFunction* Function = Handler.ResolveFunctionFromClass();
	if (!Function)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		if (It->GetFName() == ParameterName && It->HasAnyPropertyFlags(CPF_Parm))
		{
			SetParameterViaProperty(Handler, ParameterName, &Value, *It);
			return;
		}
	}
}
