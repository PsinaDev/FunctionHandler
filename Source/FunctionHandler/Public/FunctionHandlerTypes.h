// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "FunctionHandlerTypes.generated.h"

/**
 * Serializable handle to a UFunction with stored parameter values.
 * Does not hold a reference to any UObject — resolves the function by name on the target at call time.
 * Parameter values are stored as exported text (FProperty::ExportText / ImportText).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Function Handler"))
struct FUNCTIONHANDLER_API FFunctionHandler
{
	GENERATED_BODY()

#pragma region Config

	/** Class used to enumerate available functions in the editor. Not required at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FunctionHandler")
	TSubclassOf<UObject> TargetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FunctionHandler")
	FName FunctionName;

	/** Parameter values serialized as text. Key = parameter name, Value = exported property text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FunctionHandler")
	TMap<FName, FString> ParameterValues;

#pragma endregion Config

#pragma region Query

	bool IsValid() const { return !FunctionName.IsNone(); }

	/** Resolve UFunction on the given object. Returns nullptr if not found or object is null. */
	UFunction* ResolveFunction(const UObject* Target) const;

	/** Resolve UFunction using TargetClass (no object instance needed). */
	UFunction* ResolveFunctionFromClass() const;

#pragma endregion Query
};