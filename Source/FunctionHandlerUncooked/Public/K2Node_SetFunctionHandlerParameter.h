// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"

#include "K2Node_SetFunctionHandlerParameter.generated.h"

UCLASS()
class UK2Node_SetFunctionHandlerParameter : public UK2Node
{
	GENERATED_BODY()

public:

#pragma region UK2Node

	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;
	virtual bool IsNodePure() const override { return false; }
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FLinearColor GetNodeTitleColor() const override;

#pragma endregion UK2Node

	void CollectParameterNames(TArray<TSharedPtr<FName>>& OutNames) const;

	static const FName PN_Handler;
	static const FName PN_ParameterName;
	static const FName PN_Value;

private:

	UFunction* TryResolveHandlerFunction() const;
	void RefreshValuePinType();
	FName GetSelectedParameterName() const;
	void RefreshFromHandler();
	void BindBlueprintCompileDelegate();
	void UnbindBlueprintCompileDelegate();
	void OnBlueprintCompiled(UBlueprint* Blueprint);

	mutable TSubclassOf<UObject> CachedTargetClass;
	mutable FName CachedFunctionName;

	TWeakObjectPtr<UBlueprint> BoundBlueprint;
	bool bIsRefreshing = false;
};
