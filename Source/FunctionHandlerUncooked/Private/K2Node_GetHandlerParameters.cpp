// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_GetHandlerParameters.h"
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

#define LOCTEXT_NAMESPACE "K2Node_GetHandlerParameters"

const FName UK2Node_GetHandlerParameters::PN_Handler(TEXT("Handler"));

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

FText UK2Node_GetHandlerParameters::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "FunctionHandler");
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

void UK2Node_GetHandlerParameters::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	if (Pin && Pin->PinName == PN_Handler)
	{
		RefreshFromHandler();
	}
}

void UK2Node_GetHandlerParameters::PostReconstructNode()
{
	Super::PostReconstructNode();
	BindBlueprintCompileDelegate();
}

void UK2Node_GetHandlerParameters::PostPasteNode()
{
	Super::PostPasteNode();
	RefreshFromHandler();
}

void UK2Node_GetHandlerParameters::DestroyNode()
{
	UnbindBlueprintCompileDelegate();
	Super::DestroyNode();
}

FSlateIcon UK2Node_GetHandlerParameters::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.8f, 0.3f, 0.05f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.BreakStruct_16x");
}

FLinearColor UK2Node_GetHandlerParameters::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.3f, 0.05f);
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

UFunction* UK2Node_GetHandlerParameters::TryResolveHandlerFunction()
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

void UK2Node_GetHandlerParameters::RefreshFromHandler()
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

void UK2Node_GetHandlerParameters::BindBlueprintCompileDelegate()
{
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(this);
	if (!BP || BoundBlueprint == BP)
	{
		return;
	}
	UnbindBlueprintCompileDelegate();
	BP->OnCompiled().AddUObject(this, &UK2Node_GetHandlerParameters::OnBlueprintCompiled);
	BoundBlueprint = BP;
}

void UK2Node_GetHandlerParameters::UnbindBlueprintCompileDelegate()
{
	if (UBlueprint* BP = BoundBlueprint.Get())
	{
		BP->OnCompiled().RemoveAll(this);
		BoundBlueprint = nullptr;
	}
}

void UK2Node_GetHandlerParameters::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	RefreshFromHandler();
}

bool UK2Node_GetHandlerParameters::IsInputParameter(const FProperty* Prop)
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
