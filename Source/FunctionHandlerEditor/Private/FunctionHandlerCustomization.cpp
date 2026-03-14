// Copyright PsinaDev. All Rights Reserved.

#include "FunctionHandlerCustomization.h"
#include "FunctionHandlerTypes.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailsView.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "PropertyEditorModule.h"
#include "SSearchableComboBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FunctionHandlerCustomization"

#pragma region IPropertyTypeCustomization

TSharedRef<IPropertyTypeCustomization> FFunctionHandlerCustomization::MakeInstance()
{
	return MakeShared<FFunctionHandlerCustomization>();
}

void FFunctionHandlerCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		];
}

void FFunctionHandlerCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructHandle = PropertyHandle;
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	ParameterBuffer.Reset();
	ParameterDetailsView.Reset();

	TargetClassHandle = PropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FFunctionHandler, TargetClass));
	FunctionNameHandle = PropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FFunctionHandler, FunctionName));
	ParameterValuesHandle = PropertyHandle->GetChildHandle(
		GET_MEMBER_NAME_CHECKED(FFunctionHandler, ParameterValues));

	ChildBuilder.AddProperty(TargetClassHandle.ToSharedRef());

	TargetClassHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateLambda([WeakUtils = TWeakPtr<IPropertyUtilities>(PropertyUtilities),
			WeakStruct = TWeakPtr<IPropertyHandle>(StructHandle),
			WeakFuncName = TWeakPtr<IPropertyHandle>(FunctionNameHandle),
			WeakParams = TWeakPtr<IPropertyHandle>(ParameterValuesHandle)]()
		{
			TSharedPtr<IPropertyHandle> FuncNamePin = WeakFuncName.Pin();
			TSharedPtr<IPropertyHandle> ParamsPin = WeakParams.Pin();
			TSharedPtr<IPropertyHandle> StructPin = WeakStruct.Pin();

			if (StructPin.IsValid())
			{
				TArray<void*> RawData;
				StructPin->AccessRawData(RawData);
				if (RawData.Num() == 1 && RawData[0])
				{
					FFunctionHandler* Handler = static_cast<FFunctionHandler*>(RawData[0]);

					if (FuncNamePin.IsValid())
					{
						FuncNamePin->NotifyPreChange();
					}
					if (ParamsPin.IsValid())
					{
						ParamsPin->NotifyPreChange();
					}

					Handler->FunctionName = NAME_None;
					Handler->ParameterValues.Empty();

					if (FuncNamePin.IsValid())
					{
						FuncNamePin->NotifyPostChange(EPropertyChangeType::ValueSet);
					}
					if (ParamsPin.IsValid())
					{
						ParamsPin->NotifyPostChange(EPropertyChangeType::ValueSet);
					}
				}
			}

			if (TSharedPtr<IPropertyUtilities> Pinned = WeakUtils.Pin())
			{
				Pinned->RequestForceRefresh();
			}
		}));

	FFunctionHandler* Handler = nullptr;
	const UClass* ResolvedClass = nullptr;
	{
		TArray<void*> RawData;
		StructHandle->AccessRawData(RawData);
		if (RawData.Num() == 1 && RawData[0])
		{
			Handler = static_cast<FFunctionHandler*>(RawData[0]);
			ResolvedClass = Handler->TargetClass;
		}
	}

	if (!ResolvedClass)
	{
		return;
	}

	CachedFunctionNames.Reset();
	CollectFunctionNames(ResolvedClass, CachedFunctionNames);

	TSharedPtr<FString> CurrentSelection;
	if (Handler && !Handler->FunctionName.IsNone())
	{
		const FString CurrentName = Handler->FunctionName.ToString();
		for (const TSharedPtr<FString>& Entry : CachedFunctionNames)
		{
			if (Entry.IsValid() && *Entry == CurrentName)
			{
				CurrentSelection = Entry;
				break;
			}
		}
	}

	TWeakPtr<IPropertyUtilities> WeakUtils = PropertyUtilities;
	TWeakPtr<IPropertyHandle> WeakStruct = StructHandle;
	TWeakPtr<IPropertyHandle> WeakFuncName = FunctionNameHandle;
	TWeakPtr<IPropertyHandle> WeakParams = ParameterValuesHandle;

	ChildBuilder.AddCustomRow(LOCTEXT("FunctionNameRow", "Function"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("FunctionNameLabel", "Function"))
			.Font(IPropertyTypeCustomizationUtils::GetRegularFont())
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			SNew(SSearchableComboBox)
			.OptionsSource(&CachedFunctionNames)
			.InitiallySelectedItem(CurrentSelection)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
			{
				return SNew(STextBlock)
					.Text(Item.IsValid() ? FText::FromString(*Item) : FText::GetEmpty());
			})
			.OnSelectionChanged_Lambda([WeakStruct, WeakFuncName, WeakParams, WeakUtils]
				(TSharedPtr<FString> Selected, ESelectInfo::Type)
			{
				TSharedPtr<IPropertyHandle> StructPin = WeakStruct.Pin();
				TSharedPtr<IPropertyHandle> FuncNamePin = WeakFuncName.Pin();
				TSharedPtr<IPropertyHandle> ParamsPin = WeakParams.Pin();
				if (!StructPin.IsValid())
				{
					return;
				}

				TArray<void*> RawData;
				StructPin->AccessRawData(RawData);
				if (RawData.Num() != 1 || !RawData[0])
				{
					return;
				}

				FFunctionHandler* HandlerInner = static_cast<FFunctionHandler*>(RawData[0]);
				const FName NewName = (Selected.IsValid() && *Selected != TEXT("None"))
					? FName(**Selected)
					: NAME_None;

				if (FuncNamePin.IsValid())
				{
					FuncNamePin->NotifyPreChange();
				}
				if (ParamsPin.IsValid())
				{
					ParamsPin->NotifyPreChange();
				}

				HandlerInner->FunctionName = NewName;
				HandlerInner->ParameterValues.Empty();

				if (HandlerInner->TargetClass && !NewName.IsNone())
				{
					if (UFunction* Func = HandlerInner->TargetClass->FindFunctionByName(NewName))
					{
						FFunctionHandlerCustomization::ExportParameterDefaults(Func, HandlerInner);
					}
				}

				if (FuncNamePin.IsValid())
				{
					FuncNamePin->NotifyPostChange(EPropertyChangeType::ValueSet);
				}
				if (ParamsPin.IsValid())
				{
					ParamsPin->NotifyPostChange(EPropertyChangeType::ValueSet);
				}

				if (TSharedPtr<IPropertyUtilities> Pinned = WeakUtils.Pin())
				{
					Pinned->RequestForceRefresh();
				}
			})
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([WeakStruct]() -> FText
				{
					TSharedPtr<IPropertyHandle> Pin = WeakStruct.Pin();
					if (!Pin.IsValid())
					{
						return FText::GetEmpty();
					}
					TArray<void*> RawData;
					Pin->AccessRawData(RawData);
					if (RawData.Num() == 1 && RawData[0])
					{
						const FFunctionHandler* H = static_cast<const FFunctionHandler*>(RawData[0]);
						if (!H->FunctionName.IsNone())
						{
							return FText::FromName(H->FunctionName);
						}
					}
					return LOCTEXT("NoFunction", "None");
				})
			]
		];

	if (!Handler || Handler->FunctionName.IsNone() || !ResolvedClass)
	{
		return;
	}

	UFunction* Function = ResolvedClass->FindFunctionByName(Handler->FunctionName);
	if (!Function || !HasInputParameters(Function))
	{
		return;
	}

	// Build FStructOnScope from UFunction and import stored parameter values.
	ParameterBuffer = MakeShared<FStructOnScope>(Function);
	ImportMapToBuffer(Function, Handler->ParameterValues, ParameterBuffer->GetStructMemory());

	// Create a full IStructureDetailsView — this picks up all registered IPropertyTypeCustomizations.
	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bForceHiddenPropertyVisibility = true;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bShowPropertyMatrixButton = false;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowScrollBar = false;

	FStructureDetailsViewArgs StructArgs;

	// Create without data first so the visibility filter is active before the property tree is built.
	ParameterDetailsView = PropertyEditorModule.CreateStructureDetailView(
		DetailsArgs, StructArgs, nullptr);

	ParameterDetailsView->SetCustomName(LOCTEXT("ParametersGroup", "Parameters"));

	// Filter: only show input parameters (hide return value and pure out).
	ParameterDetailsView->GetDetailsView()->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropAndParent) -> bool
		{
			const FProperty* Prop = &PropAndParent.Property;
			if (!Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				// Child of a struct parameter — always visible.
				return true;
			}
			return IsInputParameter(Prop);
		}));

	// Now set the data — triggers a rebuild with the filter already active.
	ParameterDetailsView->SetStructureData(ParameterBuffer);

	// Sync buffer back to TMap on every property edit.
	TSharedPtr<FStructOnScope> BufferCapture = ParameterBuffer;
	TWeakObjectPtr<UFunction> WeakFunction = Function;

	ParameterDetailsView->GetOnFinishedChangingPropertiesDelegate().AddLambda(
		[WeakStruct, WeakParams, BufferCapture, WeakFunction]
		(const FPropertyChangedEvent&)
	{
		UFunction* Func = WeakFunction.Get();
		if (!Func || !BufferCapture.IsValid() || !BufferCapture->IsValid())
		{
			return;
		}

		TSharedPtr<IPropertyHandle> StructPin = WeakStruct.Pin();
		TSharedPtr<IPropertyHandle> ParamsPin = WeakParams.Pin();
		if (!StructPin.IsValid())
		{
			return;
		}

		TArray<void*> RawData;
		StructPin->AccessRawData(RawData);
		if (RawData.Num() != 1 || !RawData[0])
		{
			return;
		}

		FFunctionHandler* H = static_cast<FFunctionHandler*>(RawData[0]);

		if (ParamsPin.IsValid())
		{
			ParamsPin->NotifyPreChange();
		}

		ExportBufferToMap(Func, BufferCapture->GetStructMemory(), H->ParameterValues);

		if (ParamsPin.IsValid())
		{
			ParamsPin->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	});

	TSharedPtr<SWidget> DetailsWidget = ParameterDetailsView->GetWidget();
	if (DetailsWidget.IsValid())
	{
		ChildBuilder.AddCustomRow(LOCTEXT("ParametersRow", "Parameters"))
			.WholeRowContent()
			[
				DetailsWidget.ToSharedRef()
			];
	}
}

#pragma endregion IPropertyTypeCustomization

#pragma region Helpers

void FFunctionHandlerCustomization::CollectFunctionNames(
	const UClass* Class,
	TArray<TSharedPtr<FString>>& OutNames) const
{
	if (!Class)
	{
		OutNames.Add(MakeShared<FString>(TEXT("None")));
		return;
	}

	TArray<TSharedPtr<FString>> FuncNames;
	for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		UFunction* Func = *It;

		if (Func->HasAnyFunctionFlags(FUNC_Delegate | FUNC_MulticastDelegate))
		{
			continue;
		}

		if (Func->GetName().EndsWith(TEXT("_Implementation")))
		{
			continue;
		}

		FuncNames.Add(MakeShared<FString>(Func->GetFName().ToString()));
	}

	FuncNames.Sort([](const TSharedPtr<FString>& A, const TSharedPtr<FString>& B)
	{
		return *A < *B;
	});

	OutNames.Add(MakeShared<FString>(TEXT("None")));
	OutNames.Append(FuncNames);
}

bool FFunctionHandlerCustomization::IsInputParameter(const FProperty* Prop)
{
	if (!Prop || !Prop->HasAnyPropertyFlags(CPF_Parm))
	{
		return false;
	}

	if (Prop->HasAnyPropertyFlags(CPF_ReturnParm))
	{
		return false;
	}

	if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
	{
		return false;
	}

	return true;
}

bool FFunctionHandlerCustomization::HasInputParameters(UFunction* Function)
{
	if (!Function)
	{
		return false;
	}

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		if (IsInputParameter(*It))
		{
			return true;
		}
	}

	return false;
}

void FFunctionHandlerCustomization::ImportMapToBuffer(
	UFunction* Function,
	const TMap<FName, FString>& Map,
	uint8* Buffer)
{
	if (!Function || !Buffer)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!IsInputParameter(Prop))
		{
			continue;
		}

		const FString* StoredValue = Map.Find(Prop->GetFName());
		if (!StoredValue || StoredValue->IsEmpty())
		{
			continue;
		}

		void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Buffer);
		Prop->ImportText_Direct(**StoredValue, ValuePtr, nullptr, PPF_None);
	}
}

void FFunctionHandlerCustomization::ExportBufferToMap(
	UFunction* Function,
	const uint8* Buffer,
	TMap<FName, FString>& OutMap)
{
	if (!Function || !Buffer)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!IsInputParameter(Prop))
		{
			continue;
		}

		FString ExportedValue;
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Buffer);
		Prop->ExportTextItem_Direct(ExportedValue, ValuePtr, nullptr, nullptr, PPF_None);

		OutMap.Add(Prop->GetFName(), MoveTemp(ExportedValue));
	}
}

void FFunctionHandlerCustomization::ExportParameterDefaults(
	UFunction* Function,
	FFunctionHandler* Handler)
{
	if (!Function || !Handler)
	{
		return;
	}

	const int32 ParmsSize = Function->ParmsSize;
	if (ParmsSize <= 0)
	{
		return;
	}

	uint8* DefaultBuffer = static_cast<uint8*>(FMemory::Malloc(ParmsSize, Function->GetMinAlignment()));
	Function->InitializeStruct(DefaultBuffer);

	// Apply CPP_Default_ metadata where available.
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!IsInputParameter(Prop))
		{
			continue;
		}

		const FString MetaKey = FString::Printf(TEXT("CPP_Default_%s"), *Prop->GetName());
		const FString* DefaultMeta = Function->FindMetaData(*MetaKey);
		if (DefaultMeta && !DefaultMeta->IsEmpty())
		{
			void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(DefaultBuffer);
			Prop->ImportText_Direct(**DefaultMeta, ValuePtr, nullptr, PPF_None);
		}
	}

	ExportBufferToMap(Function, DefaultBuffer, Handler->ParameterValues);

	Function->DestroyStruct(DefaultBuffer);
	FMemory::Free(DefaultBuffer);
}

#pragma endregion Helpers

#undef LOCTEXT_NAMESPACE
