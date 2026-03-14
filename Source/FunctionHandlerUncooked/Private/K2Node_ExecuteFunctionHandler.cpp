// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_ExecuteFunctionHandler.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_ExecuteFunctionHandler"

const FName UK2Node_ExecuteFunctionHandler::PN_Target(TEXT("Target"));
const FName UK2Node_ExecuteFunctionHandler::PN_Success(TEXT("Success"));

void UK2Node_ExecuteFunctionHandler::AllocateDefaultPins()
{
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);

	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UObject::StaticClass(), PN_Target);

	UEdGraphPin* HandlerPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, FFunctionHandler::StaticStruct(), PN_Handler);
	HandlerPin->PinType.bIsReference = true;
	HandlerPin->PinType.bIsConst = true;

	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, PN_Success);

	if (UFunction* Function = TryResolveHandlerFunction())
	{
		CreateOutputPinsForFunction(Function);
	}

	Super::AllocateDefaultPins();
}

void UK2Node_ExecuteFunctionHandler::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	UFunction* InternalExecFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalExecuteWithResult));

	UFunction* GetResultFunc = UFunctionHandlerLibrary::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, GetResultByName));

	UFunction* TargetFunction = TryResolveHandlerFunction();

	if (!InternalExecFunc || !GetResultFunc)
	{
		CompilerContext.MessageLog.Error(
			*LOCTEXT("MissingInternalFunc", "@@: Missing internal library functions.").ToString(), this);
		BreakAllNodeLinks();
		return;
	}

	UK2Node_CallFunction* ExecNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	ExecNode->SetFromFunction(InternalExecFunc);
	ExecNode->AllocateDefaultPins();

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(UEdGraphSchema_K2::PN_Execute),
		*ExecNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(PN_Target),
		*ExecNode->FindPinChecked(TEXT("Target")));
	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(PN_Handler),
		*ExecNode->FindPinChecked(TEXT("Handler")));

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(PN_Success),
		*ExecNode->FindPinChecked(TEXT("bSuccess")));

	UEdGraphPin* ResultPin = ExecNode->FindPinChecked(UEdGraphSchema_K2::PN_ReturnValue);
	UEdGraphPin* LastExecOut = ExecNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

	if (TargetFunction)
	{
		for (TFieldIterator<FProperty> It(TargetFunction); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Parm))
			{
				continue;
			}

			const bool bIsReturn = Prop->HasAnyPropertyFlags(CPF_ReturnParm);
			const bool bIsOut = Prop->HasAnyPropertyFlags(CPF_OutParm)
				&& !Prop->HasAnyPropertyFlags(CPF_ReferenceParm);

			if (!bIsReturn && !bIsOut)
			{
				continue;
			}

			const FName OutputPinName = bIsReturn ? UEdGraphSchema_K2::PN_ReturnValue : Prop->GetFName();
			UEdGraphPin* MyOutputPin = FindPin(OutputPinName);
			if (!MyOutputPin || MyOutputPin->LinkedTo.Num() == 0)
			{
				continue;
			}

			UK2Node_CallFunction* GetterNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			GetterNode->SetFromFunction(GetResultFunc);
			GetterNode->AllocateDefaultPins();

			LastExecOut->MakeLinkTo(GetterNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
			LastExecOut = GetterNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);

			UEdGraphPin* GetterResultInput = GetterNode->FindPinChecked(TEXT("Result"));
			GetterResultInput->MakeLinkTo(ResultPin);

			UEdGraphPin* ParamNamePin = GetterNode->FindPinChecked(TEXT("ParameterName"));
			ParamNamePin->DefaultValue = Prop->GetFName().ToString();

			UEdGraphPin* OutValuePin = GetterNode->FindPinChecked(TEXT("OutValue"));
			OutValuePin->PinType = MyOutputPin->PinType;
			OutValuePin->PinType.bIsReference = false;
			OutValuePin->PinType.bIsConst = false;

			CompilerContext.MovePinLinksToIntermediate(*MyOutputPin, *OutValuePin);
		}
	}

	CompilerContext.MovePinLinksToIntermediate(
		*FindPinChecked(UEdGraphSchema_K2::PN_Then),
		*LastExecOut);

	BreakAllNodeLinks();
}

FText UK2Node_ExecuteFunctionHandler::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (CachedFunctionName.IsNone())
	{
		return LOCTEXT("NodeTitle", "Execute Function Handler");
	}
	return FText::Format(
		LOCTEXT("NodeTitleWithFunc", "Execute Handler: {0}"),
		FText::FromName(CachedFunctionName));
}

FText UK2Node_ExecuteFunctionHandler::GetTooltipText() const
{
	return LOCTEXT("Tooltip", "Execute a function on the target using stored parameters from the handler, with typed result outputs.");
}

void UK2Node_ExecuteFunctionHandler::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* NodeClass = GetClass();
	if (ActionRegistrar.IsOpenForRegistration(NodeClass))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(NodeClass);
		ActionRegistrar.AddBlueprintAction(NodeClass, Spawner);
	}
}

void UK2Node_ExecuteFunctionHandler::ReallocatePinsDuringReconstruction(
	TArray<UEdGraphPin*>& OldPins)
{
	AllocateDefaultPins();
	RestoreSplitPins(OldPins);
}

void UK2Node_ExecuteFunctionHandler::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	RefreshFromHandler();
}

void UK2Node_ExecuteFunctionHandler::CreateOutputPinsForFunction(UFunction* Function)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		const bool bIsReturn = Prop->HasAnyPropertyFlags(CPF_ReturnParm);
		const bool bIsOut = Prop->HasAnyPropertyFlags(CPF_OutParm)
			&& !Prop->HasAnyPropertyFlags(CPF_ReferenceParm);

		if (!bIsReturn && !bIsOut)
		{
			continue;
		}

		FEdGraphPinType PinType;
		Schema->ConvertPropertyToPinType(Prop, PinType);
		PinType.bIsReference = false;
		PinType.bIsConst = false;

		const FName PinName = bIsReturn ? UEdGraphSchema_K2::PN_ReturnValue : Prop->GetFName();
		CreatePin(EGPD_Output, PinType, PinName);
	}
}

#undef LOCTEXT_NAMESPACE
