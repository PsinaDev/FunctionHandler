// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_ExecuteFunctionHandler.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_MakeFunctionHandler.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_ExecuteFunctionHandler"

const FName UK2Node_ExecuteFunctionHandler::PN_Target(TEXT("Target"));
const FName UK2Node_ExecuteFunctionHandler::PN_Handler(TEXT("Handler"));
const FName UK2Node_ExecuteFunctionHandler::PN_Success(TEXT("Success"));

#pragma region UK2Node

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

			// Chain exec: previous → GetterNode → next.
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

	// Wire final exec out to our Then pin.
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

FText UK2Node_ExecuteFunctionHandler::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "FunctionHandler");
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

void UK2Node_ExecuteFunctionHandler::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	if (Pin && Pin->PinName == PN_Handler)
	{
		RefreshFromHandler();
	}
}

void UK2Node_ExecuteFunctionHandler::PinDefaultValueChanged(UEdGraphPin* Pin)
{
	Super::PinDefaultValueChanged(Pin);
	RefreshFromHandler();
}

void UK2Node_ExecuteFunctionHandler::PostReconstructNode()
{
	Super::PostReconstructNode();
	BindBlueprintCompileDelegate();
}

void UK2Node_ExecuteFunctionHandler::PostPasteNode()
{
	Super::PostPasteNode();
	RefreshFromHandler();
}

void UK2Node_ExecuteFunctionHandler::DestroyNode()
{
	UnbindBlueprintCompileDelegate();
	Super::DestroyNode();
}

FSlateIcon UK2Node_ExecuteFunctionHandler::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.8f, 0.3f, 0.05f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");
}

FLinearColor UK2Node_ExecuteFunctionHandler::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.3f, 0.05f);
}

#pragma endregion UK2Node

#pragma region Helpers

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

UFunction* UK2Node_ExecuteFunctionHandler::TryResolveHandlerFunction()
{
	// Try to read from linked Handler variable.
	const UEdGraphPin* HandlerPin = FindPin(PN_Handler);
	if (HandlerPin && HandlerPin->LinkedTo.Num() == 1)
	{
		const UEdGraphPin* SourcePin = HandlerPin->LinkedTo[0];
		if (SourcePin)
		{
			// Resolve from MakeFunctionHandler node.
			const UK2Node_MakeFunctionHandler* MakeNode = Cast<UK2Node_MakeFunctionHandler>(SourcePin->GetOwningNode());
			if (MakeNode)
			{
				if (UFunction* Func = MakeNode->GetTargetFunction())
				{
					CachedTargetClass = MakeNode->TargetClass;
					CachedFunctionName = Func->GetFName();
				}
			}

			// Resolve from variable getter via CDO.
			const UK2Node_VariableGet* VarGetNode = Cast<UK2Node_VariableGet>(SourcePin->GetOwningNode());
			if (VarGetNode)
			{
				const UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(this);
				UClass* GenClass = BP ? (BP->GeneratedClass ? BP->GeneratedClass : BP->SkeletonGeneratedClass) : nullptr;
				if (GenClass)
				{
					const FStructProperty* StructProp = CastField<FStructProperty>(
						GenClass->FindPropertyByName(VarGetNode->GetVarName()));

					if (StructProp && StructProp->Struct == FFunctionHandler::StaticStruct())
					{
						const UObject* CDO = GenClass->GetDefaultObject(false);
						if (CDO)
						{
							const FFunctionHandler* Handler = StructProp->ContainerPtrToValuePtr<FFunctionHandler>(CDO);
							if (Handler && Handler->IsValid() && Handler->TargetClass)
							{
								CachedTargetClass = Handler->TargetClass;
								CachedFunctionName = Handler->FunctionName;
							}
						}
					}
				}
			}

			// Fallback: resolve from intermediate CallFunction node wrapping InternalMakeFunctionHandler.
			// This handles the case where MakeFunctionHandler was already expanded before us.
			if (CachedFunctionName.IsNone())
			{
				const UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(SourcePin->GetOwningNode());
				if (CallNode)
				{
					static const FName InternalMakeName = GET_FUNCTION_NAME_CHECKED(UFunctionHandlerLibrary, InternalMakeFunctionHandler);
					if (CallNode->GetFunctionName() == InternalMakeName)
					{
						const UEdGraphPin* ClassPin = CallNode->FindPin(TEXT("TargetClass"));
						const UEdGraphPin* FuncNamePin = CallNode->FindPin(TEXT("FunctionName"));
						if (ClassPin && FuncNamePin)
						{
							UClass* ResolvedClass = Cast<UClass>(ClassPin->DefaultObject);
							FName ResolvedFuncName = FName(*FuncNamePin->DefaultValue);
							if (ResolvedClass && !ResolvedFuncName.IsNone())
							{
								CachedTargetClass = ResolvedClass;
								CachedFunctionName = ResolvedFuncName;
							}
						}
					}
				}
			}
		}
	}

	// Fallback to cached values (during reconstruction when pins aren't linked yet).
	if (CachedTargetClass && !CachedFunctionName.IsNone())
	{
		return CachedTargetClass->FindFunctionByName(CachedFunctionName);
	}

	return nullptr;
}

void UK2Node_ExecuteFunctionHandler::RefreshFromHandler()
{
	if (bIsRefreshing)
	{
		return;
	}

	bIsRefreshing = true;

	const FName OldFunctionName = CachedFunctionName;
	const TSubclassOf<UObject> OldTargetClass = CachedTargetClass;

	// Clear cache to force a fresh read.
	CachedTargetClass = nullptr;
	CachedFunctionName = NAME_None;

	TryResolveHandlerFunction();

	if (CachedFunctionName != OldFunctionName || CachedTargetClass != OldTargetClass)
	{
		ReconstructNode();
	}

	bIsRefreshing = false;
}

void UK2Node_ExecuteFunctionHandler::BindBlueprintCompileDelegate()
{
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(this);
	if (!BP || BoundBlueprint == BP)
	{
		return;
	}

	UnbindBlueprintCompileDelegate();

	BP->OnCompiled().AddUObject(this, &UK2Node_ExecuteFunctionHandler::OnBlueprintCompiled);
	BoundBlueprint = BP;
}

void UK2Node_ExecuteFunctionHandler::UnbindBlueprintCompileDelegate()
{
	if (UBlueprint* BP = BoundBlueprint.Get())
	{
		BP->OnCompiled().RemoveAll(this);
		BoundBlueprint = nullptr;
	}
}

void UK2Node_ExecuteFunctionHandler::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	RefreshFromHandler();
}

#pragma endregion Helpers

#undef LOCTEXT_NAMESPACE