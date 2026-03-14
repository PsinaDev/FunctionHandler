// Copyright PsinaDev. All Rights Reserved.

#include "SGraphPinFunctionName.h"
#include "K2Node_MakeFunctionHandler.h"
#include "SSearchableComboBox.h"
#include "Widgets/Text/STextBlock.h"

void SGraphPinFunctionName::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
}

TSharedRef<SWidget> SGraphPinFunctionName::GetDefaultValueWidget()
{
	RebuildOptions();

	return SNew(SSearchableComboBox)
		.OptionsSource(&FunctionOptions)
		.InitiallySelectedItem(CurrentSelection)
		.OnSelectionChanged(this, &SGraphPinFunctionName::OnSelectionChanged)
		.OnGenerateWidget(this, &SGraphPinFunctionName::OnGenerateWidget)
		.Content()
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				if (CurrentSelection.IsValid() && *CurrentSelection != TEXT("None"))
				{
					return FText::FromString(*CurrentSelection);
				}
				return FText::FromString(TEXT("None"));
			})
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		];
}

void SGraphPinFunctionName::OnSelectionChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	CurrentSelection = NewValue;

	if (GraphPinObj)
	{
		const FString NewDefault = (NewValue.IsValid() && *NewValue != TEXT("None"))
			? *NewValue
			: FString();

		if (GraphPinObj->DefaultValue != NewDefault)
		{
			GraphPinObj->Modify();
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewDefault);
		}
	}
}

TSharedRef<SWidget> SGraphPinFunctionName::OnGenerateWidget(TSharedPtr<FString> Item)
{
	return SNew(STextBlock)
		.Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty())
		.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"));
}

void SGraphPinFunctionName::RebuildOptions()
{
	FunctionOptions.Reset();
	CurrentSelection = nullptr;

	if (!GraphPinObj)
	{
		return;
	}

	UK2Node_MakeFunctionHandler* OwningNode =
		Cast<UK2Node_MakeFunctionHandler>(GraphPinObj->GetOwningNode());

	if (OwningNode)
	{
		OwningNode->CollectFunctionNames(FunctionOptions);
	}

	if (FunctionOptions.Num() == 0)
	{
		FunctionOptions.Add(MakeShared<FString>(TEXT("None")));
	}

	const FString CurrentValue = GraphPinObj->DefaultValue.IsEmpty()
		? TEXT("None")
		: GraphPinObj->DefaultValue;

	for (const TSharedPtr<FString>& Entry : FunctionOptions)
	{
		if (Entry.IsValid() && *Entry == CurrentValue)
		{
			CurrentSelection = Entry;
			break;
		}
	}

	if (!CurrentSelection.IsValid())
	{
		CurrentSelection = FunctionOptions[0];
	}
}
