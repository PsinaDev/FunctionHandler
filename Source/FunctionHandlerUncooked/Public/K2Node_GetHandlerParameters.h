// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node_FunctionHandlerBase.h"

#include "K2Node_GetHandlerParameters.generated.h"

UCLASS()
class UK2Node_GetHandlerParameters : public UK2Node_FunctionHandlerBase
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

private:

	void CreateOutputPinsForFunction(UFunction* Function);
};
