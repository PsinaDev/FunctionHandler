// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "Templates/SubclassOf.h"

#include "K2Node_FunctionHandlerBase.generated.h"

UCLASS(Abstract)
class UK2Node_FunctionHandlerBase : public UK2Node
{
	GENERATED_BODY()

public:

	virtual void PinConnectionListChanged(UEdGraphPin* Pin) override;
	virtual void PostReconstructNode() override;
	virtual void PostPasteNode() override;
	virtual void DestroyNode() override;
	virtual bool IsNodePure() const override { return false; }
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetMenuCategory() const override;

protected:

	static const FName PN_Handler;

	UFunction* TryResolveHandlerFunction();
	void RefreshFromHandler();

	static bool IsInputParameter(const FProperty* Prop);

	UPROPERTY()
	TSubclassOf<UObject> CachedTargetClass;

	UPROPERTY()
	FName CachedFunctionName;

private:

	void BindBlueprintCompileDelegate();
	void UnbindBlueprintCompileDelegate();
	void OnBlueprintCompiled(UBlueprint* Blueprint);

	TWeakObjectPtr<UBlueprint> BoundBlueprint;
	bool bIsRefreshing = false;
};
