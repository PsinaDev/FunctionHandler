// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"

#include "K2Node_GetHandlerParameters.generated.h"

UCLASS()
class UK2Node_GetHandlerParameters : public UK2Node
{
	GENERATED_BODY()

public:

	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;
	virtual bool IsNodePure() const override { return false; }
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	static const FName PN_Handler;

private:

	void CreateOutputPinsForFunction(UFunction* Function);
	UFunction* TryResolveHandlerFunction();
	void RefreshFromHandler();
	void BindBlueprintCompileDelegate();
	void UnbindBlueprintCompileDelegate();
	void OnBlueprintCompiled(UBlueprint* Blueprint);

	static bool IsInputParameter(const FProperty* Prop);

	UPROPERTY()
	TSubclassOf<UObject> CachedTargetClass;

	UPROPERTY()
	FName CachedFunctionName;

	TWeakObjectPtr<UBlueprint> BoundBlueprint;
	bool bIsRefreshing = false;
};
