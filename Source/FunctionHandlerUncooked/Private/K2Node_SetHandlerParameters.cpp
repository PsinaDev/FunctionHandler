// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_SetHandlerParameters.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_SetHandlerParameters"

void UK2Node_SetHandlerParameters::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	UEdGraphPin* HandlerPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FFunctionHandler::StaticStruct(), PN_Handler);
	HandlerPin->PinType.bIsReference = true;

	if (UFunction* Function = TryResolveHandlerFunction())
	{
		CreateInputPinsForFunction(Function);
	}

	Super::AllocateDefaultPins();
}

void UK2Node_SetHandlerParameters::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UFunction* SetParamFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalSetParameter));

	UFunction* TargetFunction = TryResolveHandlerFunction();

	if (!SetParamFunc)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingSetFunc", "@@: Missing InternalSetParameter.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UEdGraphPin* HandlerPin = FindPinChecked(PN_Handler);
	UEdGraphPin* FirstExecIn = nullptr;
	UEdGraphPin* LastExecOut = nullptr;

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

			if (!FirstExecIn)
			{
				FirstExecIn = SetNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute);
			}
			else
			{
				LastExecOut->MakeLinkTo(SetNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
			}
			LastExecOut = SetNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

			CompilerContext.CopyPinLinksToIntermediate(*HandlerPin, *SetNode->FindPinChecked(TEXT("Handler")));
			SetNode->FindPinChecked(TEXT("ParameterName"))->DefaultValue = Prop->GetFName().ToString();

			UEdGraphPin* SetValuePin = SetNode->FindPinChecked(TEXT("Value"));
			SetValuePin->PinType = MyParamPin->PinType;
			CompilerContext.MovePinLinksToIntermediate(*MyParamPin, *SetValuePin);
		}
	}

	if (FirstExecIn && LastExecOut)
	{
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Execute), *FirstExecIn);
		CompilerContext.MovePinLinksToIntermediate(*FindPinChecked(UEdGraphSchema_K2::PN_Then), *LastExecOut);
	}
	else
	{
		CompilerContext.MovePinLinksToIntermediate(
			*FindPinChecked(UEdGraphSchema_K2::PN_Then),
			*FindPinChecked(UEdGraphSchema_K2::PN_Execute));
	}

	BreakAllNodeLinks();
}

FText UK2Node_SetHandlerParameters::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedFunctionName.IsNone())
	{
		return LOCTEXT("NodeTitle", "Set Handler Parameters");
	}
	return FText::Format(
		LOCTEXT("NodeTitleWithFunc", "Set Handler Parameters: {0}"),
		FText::FromName(CachedFunctionName));
}

FText UK2Node_SetHandlerParameters::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Set all parameter values on a Function Handler at once with typed inputs.");
}

void UK2Node_SetHandlerParameters::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(NodeClass);
		ActionRegistrar.AddBlueprintAction(NodeClass, Spawner);
	}
}

void UK2Node_SetHandlerParameters::ReallocatePinsDuringReconstruction(
	TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();
	RestoreSplitPins(OldPins);
}

void UK2Node_SetHandlerParameters::CreateInputPinsForFunction(UFunction* Function)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!IsInputParameter(Prop))
		{
			continue;
		}
		if (FindPin(Prop->GetFName()))
		{
			continue;
		}

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(Prop, PinType);
		CreatePin(EGPD_Input, PinType, Prop->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
