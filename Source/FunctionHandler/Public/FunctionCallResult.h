// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/StructOnScope.h"

#include "FunctionCallResult.generated.h"

UCLASS(Transient)
class FUNCTIONHANDLER_API UFunctionCallResult : public UObject
{
	GENERATED_BODY()

public:

	TSharedPtr<FStructOnScope> ResultData;
	TWeakObjectPtr<UFunction> CachedFunction;

	bool IsResultValid() const
	{
		return CachedFunction.IsValid()
			&& ResultData.IsValid()
			&& ResultData->IsValid();
	}

	uint8* GetBuffer() const
	{
		return ResultData.IsValid() ? ResultData->GetStructMemory() : nullptr;
	}
};
