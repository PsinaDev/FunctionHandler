// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_FunctionHandlerBase.h"

#include "K2Node_SetHandlerParameters.generated.h"

UCLASS()
class UK2Node_SetHandlerParameters : public UK2Node_FunctionHandlerBase
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;

private:

	void CreateInputPinsForFunction(UFunction* Function);
};
