// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_SetHandlerParameters.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_MakeFunctionHandler.h"
#include "KismetCompiler.h"
#include "Kismet2/BlueprintEditorUtils.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "K2Node_SetHandlerParameters"

const FName UK2Node_SetHandlerParameters::PN_Handler(TEXT("Handler"));

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

FText UK2Node_SetHandlerParameters::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "FunctionHandler");
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

void UK2Node_SetHandlerParameters::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	if (Pin && Pin->PinName == PN_Handler)
	{
		RefreshFromHandler();
	}
}

void UK2Node_SetHandlerParameters::PostReconstructNode()
{
	Super::PostReconstructNode();
	BindBlueprintCompileDelegate();
}

void UK2Node_SetHandlerParameters::PostPasteNode()
{
	Super::PostPasteNode();
	RefreshFromHandler();
}

void UK2Node_SetHandlerParameters::DestroyNode()
{
	UnbindBlueprintCompileDelegate();
	Super::DestroyNode();
}

FSlateIcon UK2Node_SetHandlerParameters::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.8f, 0.3f, 0.05f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");
}

FLinearColor UK2Node_SetHandlerParameters::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.3f, 0.05f);
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

UFunction* UK2Node_SetHandlerParameters::TryResolveHandlerFunction()
{
	const UEdGraphPin* HandlerPin = FindPin(PN_Handler);
	if (HandlerPin && HandlerPin->LinkedTo.Num() == 1)
	{
		const UEdGraphPin* SourcePin = HandlerPin->LinkedTo[0];
		if (SourcePin)
		{
			const UK2Node_MakeFunctionHandler* MakeNode = Cast<UK2Node_MakeFunctionHandler>(SourcePin->GetOwningNode());
			if (MakeNode)
			{
				if (UFunction* Func = MakeNode->GetTargetFunction())
				{
					CachedTargetClass = MakeNode->TargetClass;
					CachedFunctionName = Func->GetFName();
				}
			}

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

	if (CachedTargetClass && !CachedFunctionName.IsNone())
	{
		return CachedTargetClass->FindFunctionByName(CachedFunctionName);
	}

	return nullptr;
}

void UK2Node_SetHandlerParameters::RefreshFromHandler()
{
	if (bIsRefreshing)
	{
		return;
	}
	bIsRefreshing = true;

	const FName OldFunctionName = CachedFunctionName;
	const TSubclassOf<UObject> OldTargetClass = CachedTargetClass;

	CachedTargetClass = nullptr;
	CachedFunctionName = NAME_None;

	TryResolveHandlerFunction();

	if (CachedFunctionName != OldFunctionName || CachedTargetClass != OldTargetClass)
	{
		ReconstructNode();
	}

	bIsRefreshing = false;
}

void UK2Node_SetHandlerParameters::BindBlueprintCompileDelegate()
{
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(this);
	if (!BP || BoundBlueprint == BP)
	{
		return;
	}
	UnbindBlueprintCompileDelegate();
	BP->OnCompiled().AddUObject(this, &UK2Node_SetHandlerParameters::OnBlueprintCompiled);
	BoundBlueprint = BP;
}

void UK2Node_SetHandlerParameters::UnbindBlueprintCompileDelegate()
{
	if (UBlueprint* BP = BoundBlueprint.Get())
	{
		BP->OnCompiled().RemoveAll(this);
		BoundBlueprint = nullptr;
	}
}

void UK2Node_SetHandlerParameters::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	RefreshFromHandler();
}

bool UK2Node_SetHandlerParameters::IsInputParameter(const FProperty* Prop)
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

#undef LOCTEXT_NAMESPACE
