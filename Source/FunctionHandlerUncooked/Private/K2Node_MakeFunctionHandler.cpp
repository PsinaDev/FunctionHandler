// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_MakeFunctionHandler.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_MakeFunctionHandler"

const FName UK2Node_MakeFunctionHandler::PN_FunctionName(TEXT("Function"));
const FName UK2Node_MakeFunctionHandler::PN_Output(TEXT("Handler"));

#pragma region UK2Node

void UK2Node_MakeFunctionHandler::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	// Function name pin — rendered as dropdown by SGraphPinFunctionName.
	UEdGraphPin* FuncPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, PN_FunctionName);
	FuncPin->bNotConnectable = true;
	FuncPin->bDefaultValueIsReadOnly = false;

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, FFunctionHandler::StaticStruct(), PN_Output);

	if (UFunction* Function = GetTargetFunction())
	{
		CreateInputPinsForFunction(Function);
	}

	Super::AllocateDefaultPins();
}

void UK2Node_MakeFunctionHandler::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const FName SelectedFunc = GetSelectedFunctionName();

	UFunction* MakeFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalMakeFunctionHandler));

	UFunction* SetParamFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalSetParameter));

	if (!MakeFunc || !SetParamFunc)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingFuncs", "@@: Missing internal library functions.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* MakeNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	MakeNode->SetFromFunction(MakeFunc);
	MakeNode->AllocateDefaultPins();

	UEdGraphPin* ClassPin = MakeNode->FindPinChecked(TEXT("TargetClass"));
	if (TargetClass)
	{
		ClassPin->DefaultObject = TargetClass.Get();
	}

	MakeNode->FindPinChecked(TEXT("FunctionName"))->DefaultValue = SelectedFunc.ToString();

	UEdGraphPin* HandlerRefPin = MakeNode->FindPinChecked(TEXT("OutHandler"));

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(UEdGraphSchema_K2::PN_Execute),
		*MakeNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(PN_Output),
		*HandlerRefPin);

	UEdGraphPin* LastExecOut = MakeNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

	UFunction* TargetFunction = GetTargetFunction();
	if (TargetFunction)
	{
		for (TFieldIterator<FProperty> It(TargetFunction); It; ++It)
		{
			FProperty* Prop = *It;
			if (!IsInputParameter(Prop))
			{
				continue;
			}

			UEdGraphPin* MyParamPin = FindPin(Prop->GetFName());
			if (!MyParamPin)
			{
				continue;
			}
			if (!MyParamPin->HasAnyConnections() && MyParamPin->DefaultValue.IsEmpty() && !MyParamPin->DefaultObject)
			{
				continue;
			}

			UK2Node_CallFunction* SetNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			SetNode->SetFromFunction(SetParamFunc);
			SetNode->AllocateDefaultPins();

			LastExecOut->MakeLinkTo(SetNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
			LastExecOut = SetNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

			SetNode->FindPinChecked(TEXT("Handler"))->MakeLinkTo(HandlerRefPin);
			SetNode->FindPinChecked(TEXT("ParameterName"))->DefaultValue = Prop->GetFName().ToString();

			UEdGraphPin* SetValuePin = SetNode->FindPinChecked(TEXT("Value"));
			SetValuePin->PinType = MyParamPin->PinType;
			CompilerContext.MovePinLinksToIntermediate(*MyParamPin, *SetValuePin);
		}
	}

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(UEdGraphSchema_K2::PN_Then),
		*LastExecOut);

	BreakAllNodeLinks();
}

FText UK2Node_MakeFunctionHandler::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FName SelectedFunc = GetSelectedFunctionName();
	if (SelectedFunc.IsNone())
	{
		return LOCTEXT("NodeTitle", "Make Function Handler");
	}
	return FText::Format(LOCTEXT("NodeTitleWithFunc", "Make Handler: {0}"), FText::FromName(SelectedFunc));
}

FText UK2Node_MakeFunctionHandler::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Create a Function Handler with typed parameter inputs.");
}

FText UK2Node_MakeFunctionHandler::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "FunctionHandler");
}

void UK2Node_MakeFunctionHandler::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(NodeClass);
		ActionRegistrar.AddBlueprintAction(NodeClass, Spawner);
	}
}

void UK2Node_MakeFunctionHandler::ReallocatePinsDuringReconstruction(
	TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

	for (UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin && OldPin->PinName == PN_FunctionName)
		{
			if (UEdGraphPin* NewPin = FindPin(PN_FunctionName))
			{
				NewPin->DefaultValue = OldPin->DefaultValue;
			}
			break;
		}
	}

	// Re-resolve function to create parameter pins.
	if (UFunction* Function = GetTargetFunction())
	{
		CreateInputPinsForFunction(Function);
	}

	RestoreSplitPins(OldPins);
}

void UK2Node_MakeFunctionHandler::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UK2Node_MakeFunctionHandler, TargetClass))
	{
		// Clear function name when class changes.
		if (UEdGraphPin* FuncPin = FindPin(PN_FunctionName))
		{
			FuncPin->GetSchema()->TrySetDefaultValue(*FuncPin, FString());
		}
		ReconstructNode();
	}
}

void UK2Node_MakeFunctionHandler::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	if (Pin && Pin->PinName == PN_FunctionName)
	{
		ReconstructNode();
	}
}

FSlateIcon UK2Node_MakeFunctionHandler::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.8f, 0.3f, 0.05f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.MakeStruct_16x");
}

FLinearColor UK2Node_MakeFunctionHandler::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.3f, 0.05f);
}

#pragma endregion UK2Node

#pragma region Helpers

void UK2Node_MakeFunctionHandler::CollectFunctionNames(TArray<TSharedPtr<FString>>& OutNames) const
{
	OutNames.Add(MakeShared<FString>(TEXT("None")));

	const UClass* Class = TargetClass.Get();
	if (!Class)
	{
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

	OutNames.Append(FuncNames);
}

FName UK2Node_MakeFunctionHandler::GetSelectedFunctionName() const
{
	const UEdGraphPin* FuncPin = FindPin(PN_FunctionName);
	if (FuncPin && !FuncPin->DefaultValue.IsEmpty())
	{
		return FName(*FuncPin->DefaultValue);
	}
	return NAME_None;
}

UFunction* UK2Node_MakeFunctionHandler::GetTargetFunction() const
{
	const UClass* Class = TargetClass.Get();
	const FName SelectedFunc = GetSelectedFunctionName();
	if (!Class || SelectedFunc.IsNone())
	{
		return nullptr;
	}
	return Class->FindFunctionByName(SelectedFunc);
}

void UK2Node_MakeFunctionHandler::CreateInputPinsForFunction(UFunction* Function)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!IsInputParameter(Prop))
		{
			continue;
		}

		// Don't create duplicates.
		if (FindPin(Prop->GetFName()))
		{
			continue;
		}

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(Prop, PinType);
		CreatePin(EGPD_Input, PinType, Prop->GetFName());
	}
}

bool UK2Node_MakeFunctionHandler::IsInputParameter(const FProperty* Prop)
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

#pragma endregion Helpers

#undef LOCTEXT_NAMESPACE
