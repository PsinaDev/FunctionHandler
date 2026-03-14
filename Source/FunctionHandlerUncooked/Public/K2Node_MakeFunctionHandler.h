// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"

#include "K2Node_MakeFunctionHandler.generated.h"

UCLASS()
class UK2Node_MakeFunctionHandler : public UK2Node
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "FunctionHandler")
	TSubclassOf<UObject> TargetClass;

#pragma region UK2Node

	virtual void AllocateDefaultPins() override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	virtual void GetMenuActions(class FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual bool IsNodePure() const override { return false; }
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FLinearColor GetNodeTitleColor() const override;

#pragma endregion UK2Node

	void CollectFunctionNames(TArray<TSharedPtr<FString>>& OutNames) const;
	UFunction* GetTargetFunction() const;

	static const FName PN_FunctionName;
	static const FName PN_Output;

private:

	FName GetSelectedFunctionName() const;
	void CreateInputPinsForFunction(UFunction* Function);

	static bool IsInputParameter(const FProperty* Prop);
};
