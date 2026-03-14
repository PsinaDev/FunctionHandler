// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"

class SGraphPinFunctionName : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinFunctionName) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:

	void OnSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FString> Item);
	void RebuildOptions();

	TArray<TSharedPtr<FString>> FunctionOptions;
	TSharedPtr<FString> CurrentSelection;
};
