// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_SetFunctionHandlerParameter.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_SetFunctionHandlerParameter"

const FName UK2Node_SetFunctionHandlerParameter::PN_ParameterName(TEXT("Parameter"));
const FName UK2Node_SetFunctionHandlerParameter::PN_Value(TEXT("Value"));

void UK2Node_SetFunctionHandlerParameter::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	UEdGraphPin* HandlerPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FFunctionHandler::StaticStruct(), PN_Handler);
	HandlerPin->PinType.bIsReference = true;

	UEdGraphPin* ParamPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, PN_ParameterName);
	ParamPin->bNotConnectable = true;
	ParamPin->bDefaultValueIsReadOnly = false;

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, PN_Value);
	RefreshValuePinType();

	Super::AllocateDefaultPins();
}

void UK2Node_SetFunctionHandlerParameter::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const FName SelectedParam = GetSelectedParameterName();
	if (SelectedParam.IsNone())
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("NoParamSelected", "@@: No parameter selected.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UFunction* SetFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalSetParameter));

	if (!SetFunc)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingSetFunc", "@@: Missing InternalSetParameter.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* CallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallNode->SetFromFunction(SetFunc);
	CallNode->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(UEdGraphSchema_K2::PN_Execute),
		*CallNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(UEdGraphSchema_K2::PN_Then),
		*CallNode->FindPinChecked(UEdGraphSchema_K2::PN_Then));
	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(PN_Handler),
		*CallNode->FindPinChecked(TEXT("Handler")));

	CallNode->FindPinChecked(TEXT("ParameterName"))->DefaultValue = SelectedParam.ToString();

	UEdGraphPin* IntermediateValuePin = CallNode->FindPinChecked(TEXT("Value"));
	UEdGraphPin* MyValuePin = FindPinChecked(PN_Value);
	IntermediateValuePin->PinType = MyValuePin->PinType;
	CompilerContext.MovePinLinksToIntermediate(*MyValuePin, *IntermediateValuePin);

	BreakAllNodeLinks();
}

FText UK2Node_SetFunctionHandlerParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FName Param = GetSelectedParameterName();
	if (Param.IsNone())
	{
		return LOCTEXT("NodeTitle", "Set Handler Parameter");
	}
	return FText::Format(LOCTEXT("NodeTitleWithParam", "Set Handler: {0}"), FText::FromName(Param));
}

FText UK2Node_SetFunctionHandlerParameter::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Set a typed parameter value on a Function Handler.");
}

void UK2Node_SetFunctionHandlerParameter::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(NodeClass);
		ActionRegistrar.AddBlueprintAction(NodeClass, Spawner);
	}
}

void UK2Node_SetFunctionHandlerParameter::ReallocatePinsDuringReconstruction(
	TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();

	for (UEdGraphPin* OldPin : OldPins)
	{
		if (OldPin && OldPin->PinName == PN_ParameterName)
		{
			if (UEdGraphPin* NewPin = FindPin(PN_ParameterName))
			{
				NewPin->DefaultValue = OldPin->DefaultValue;
			}
			break;
		}
	}

	RefreshValuePinType();
	RestoreSplitPins(OldPins);
}

void UK2Node_SetFunctionHandlerParameter::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	if (Pin && Pin->PinName == PN_ParameterName)
	{
		ReconstructNode();
	}
}

void UK2Node_SetFunctionHandlerParameter::CollectParameterNames(TArray<TSharedPtr<FName>>& OutNames) const
{
	OutNames.Add(MakeShared<FName>(NAME_None));

	UFunction* Function = const_cast<UK2Node_SetFunctionHandlerParameter*>(this)->TryResolveHandlerFunction();
	if (!Function)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Parm) || Prop->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}
		if (Prop->HasAnyPropertyFlags(CPF_OutParm) && !Prop->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			continue;
		}
		OutNames.Add(MakeShared<FName>(Prop->GetFName()));
	}
}

FName UK2Node_SetFunctionHandlerParameter::GetSelectedParameterName() const
{
	const UEdGraphPin* ParamPin = FindPin(PN_ParameterName);
	if (ParamPin && !ParamPin->DefaultValue.IsEmpty())
	{
		return FName(*ParamPin->DefaultValue);
	}
	return NAME_None;
}

void UK2Node_SetFunctionHandlerParameter::RefreshValuePinType()
{
	UEdGraphPin* ValuePin = FindPin(PN_Value);
	if (!ValuePin)
	{
		return;
	}

	const FName SelectedParam = GetSelectedParameterName();
	if (SelectedParam.IsNone())
	{
		ValuePin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		ValuePin->PinType.PinSubCategoryObject = nullptr;
		return;
	}

	UFunction* Function = TryResolveHandlerFunction();
	if (!Function)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (Prop->GetFName() == SelectedParam && Prop->HasAnyPropertyFlags(CPF_Parm))
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			FEdGraphPinType NewType;
			Schema->ConvertPropertyToPinType(Prop, NewType);
			ValuePin->PinType = NewType;
			return;
		}
	}
}

#undef LOCTEXT_NAMESPACE
