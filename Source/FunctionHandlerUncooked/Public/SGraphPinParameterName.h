// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SGraphPin.h"

class SGraphPinParameterName : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SGraphPinParameterName) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);

protected:

	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

private:

	void OnSelectionChanged(TSharedPtr<FName> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> OnGenerateWidget(TSharedPtr<FName> Item);

	void RebuildOptions();

	TArray<TSharedPtr<FName>> ParameterOptions;
	TSharedPtr<FName> CurrentSelection;
};
