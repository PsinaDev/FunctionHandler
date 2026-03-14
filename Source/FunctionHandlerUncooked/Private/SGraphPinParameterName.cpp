// Copyright PsinaDev. All Rights Reserved.

#include "SGraphPinParameterName.h"
#include "K2Node_SetFunctionHandlerParameter.h"

#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"

void SGraphPinParameterName::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
}

TSharedRef<SWidget> SGraphPinParameterName::GetDefaultValueWidget()
{
	RebuildOptions();

	return SNew(SComboBox<TSharedPtr<FName>>)
		.OptionsSource(&ParameterOptions)
		.InitiallySelectedItem(CurrentSelection)
		.OnSelectionChanged(this, &SGraphPinParameterName::OnSelectionChanged)
		.OnGenerateWidget(this, &SGraphPinParameterName::OnGenerateWidget)
		.Content()
		[
			SNew(STextBlock)
			.Text_Lambda([this]() -> FText
			{
				if (CurrentSelection.IsValid() && !CurrentSelection->IsNone())
				{
					return FText::FromName(*CurrentSelection);
				}
				return FText::FromString(TEXT("None"));
			})
			.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		];
}

void SGraphPinParameterName::OnSelectionChanged(TSharedPtr<FName> NewValue, ESelectInfo::Type SelectInfo)
{
	CurrentSelection = NewValue;

	if (GraphPinObj)
	{
		const FString NewDefault = (NewValue.IsValid() && !NewValue->IsNone())
			? NewValue->ToString()
			: FString();

		if (GraphPinObj->DefaultValue != NewDefault)
		{
			GraphPinObj->Modify();
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, NewDefault);
		}
	}
}

TSharedRef<SWidget> SGraphPinParameterName::OnGenerateWidget(TSharedPtr<FName> Item)
{
	FText DisplayText = (Item.IsValid() && !Item->IsNone())
		? FText::FromName(*Item)
		: FText::FromString(TEXT("None"));

	return SNew(STextBlock)
		.Text(DisplayText)
		.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"));
}

void SGraphPinParameterName::RebuildOptions()
{
	ParameterOptions.Reset();
	CurrentSelection = nullptr;

	if (!GraphPinObj)
	{
		return;
	}

	UK2Node_SetFunctionHandlerParameter* OwningNode =
		Cast<UK2Node_SetFunctionHandlerParameter>(GraphPinObj->GetOwningNode());

	if (OwningNode)
	{
		OwningNode->CollectParameterNames(ParameterOptions);
	}

	if (ParameterOptions.Num() == 0)
	{
		ParameterOptions.Add(MakeShared<FName>(NAME_None));
	}

	const FName CurrentValue = GraphPinObj->DefaultValue.IsEmpty()
		? NAME_None
		: FName(*GraphPinObj->DefaultValue);

	for (const TSharedPtr<FName>& Entry : ParameterOptions)
	{
		if (Entry.IsValid() && *Entry == CurrentValue)
		{
			CurrentSelection = Entry;
			break;
		}
	}

	if (!CurrentSelection.IsValid())
	{
		CurrentSelection = ParameterOptions[0];
	}
}
