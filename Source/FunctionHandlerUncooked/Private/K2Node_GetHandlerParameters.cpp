// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_GetHandlerParameters.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_GetHandlerParameters"

void UK2Node_GetHandlerParameters::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	UEdGraphPin* HandlerPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FFunctionHandler::StaticStruct(), PN_Handler);
	HandlerPin->PinType.bIsReference = true;
	HandlerPin->PinType.bIsConst = true;

	if (UFunction* Function = TryResolveHandlerFunction())
	{
		CreateOutputPinsForFunction(Function);
	}

	Super::AllocateDefaultPins();
}

void UK2Node_GetHandlerParameters::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UFunction* GetParamFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalGetParameter));

	UFunction* TargetFunction = TryResolveHandlerFunction();

	if (!GetParamFunc)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingGetFunc", "@@: Missing InternalGetParameter.").ToString(), this);
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

			UEdGraphPin* MyOutputPin = FindPin(Prop->GetFName());
			if (!MyOutputPin || MyOutputPin->LinkedTo.Num() == 0)
			{
				continue;
			}

			UK2Node_CallFunction* GetNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			GetNode->SetFromFunction(GetParamFunc);
			GetNode->AllocateDefaultPins();

			if (!FirstExecIn)
			{
				FirstExecIn = GetNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute);
			}
			else
			{
				LastExecOut->MakeLinkTo(GetNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
			}
			LastExecOut = GetNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

			CompilerContext.CopyPinLinksToIntermediate(*HandlerPin, *GetNode->FindPinChecked(TEXT("Handler")));
			GetNode->FindPinChecked(TEXT("ParameterName"))->DefaultValue = Prop->GetFName().ToString();

			UEdGraphPin* OutValuePin = GetNode->FindPinChecked(TEXT("OutValue"));
			OutValuePin->PinType = MyOutputPin->PinType;
			OutValuePin->PinType.bIsReference = false;
			OutValuePin->PinType.bIsConst = false;

			CompilerContext.MovePinLinksToIntermediate(*MyOutputPin, *OutValuePin);
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

FText UK2Node_GetHandlerParameters::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedFunctionName.IsNone())
	{
		return LOCTEXT("NodeTitle", "Get Handler Parameters");
	}
	return FText::Format(
		LOCTEXT("NodeTitleWithFunc", "Get Handler Parameters: {0}"),
		FText::FromName(CachedFunctionName));
}

FText UK2Node_GetHandlerParameters::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Read all stored parameter values from a Function Handler as typed outputs.");
}

void UK2Node_GetHandlerParameters::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(NodeClass);
		ActionRegistrar.AddBlueprintAction(NodeClass, Spawner);
	}
}

void UK2Node_GetHandlerParameters::ReallocatePinsDuringReconstruction(
	TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();
	RestoreSplitPins(OldPins);
}

FSlateIcon UK2Node_GetHandlerParameters::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.8f, 0.3f, 0.05f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.BreakStruct_16x");
}

void UK2Node_GetHandlerParameters::CreateOutputPinsForFunction(UFunction* Function)
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
		PinType.bIsReference = false;
		PinType.bIsConst = false;
		CreatePin(EGPD_Output, PinType, Prop->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE
