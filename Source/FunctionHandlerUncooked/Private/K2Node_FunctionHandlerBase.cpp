// Copyright PsinaDev. All Rights Reserved.

#include "K2Node_FunctionHandlerBase.h"
#include "FunctionHandlerLibrary.h"
#include "FunctionHandlerTypes.h"
#include "K2Node_MakeFunctionHandler.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Containers/Ticker.h"

#define LOCTEXT_NAMESPACE "K2Node_FunctionHandlerBase"

const FName UK2Node_FunctionHandlerBase::PN_Handler(TEXT("Handler"));

void UK2Node_FunctionHandlerBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);
	if (Pin && Pin->PinName == PN_Handler)
	{
		RefreshFromHandler();
	}
}

void UK2Node_FunctionHandlerBase::PostReconstructNode()
{
	Super::PostReconstructNode();
	BindBlueprintCompileDelegate();
}

void UK2Node_FunctionHandlerBase::PostPasteNode()
{
	Super::PostPasteNode();
	RefreshFromHandler();
}

void UK2Node_FunctionHandlerBase::DestroyNode()
{
	UnbindBlueprintCompileDelegate();
	Super::DestroyNode();
}

FSlateIcon UK2Node_FunctionHandlerBase::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = FLinearColor(0.8f, 0.3f, 0.05f);
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "Kismet.AllClasses.FunctionIcon");
}

FLinearColor UK2Node_FunctionHandlerBase::GetNodeTitleColor() const
{
	return FLinearColor(0.8f, 0.3f, 0.05f);
}

FText UK2Node_FunctionHandlerBase::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "FunctionHandler");
}

UFunction* UK2Node_FunctionHandlerBase::TryResolveHandlerFunction()
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

void UK2Node_FunctionHandlerBase::RefreshFromHandler()
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
		TWeakObjectPtr<UK2Node_FunctionHandlerBase> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float) -> bool
			{
				if (UK2Node_FunctionHandlerBase* Node = WeakThis.Get())
				{
					Node->ReconstructNode();
				}
				return false;
			}));
	}

	bIsRefreshing = false;
}

bool UK2Node_FunctionHandlerBase::IsInputParameter(const FProperty* Prop)
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

void UK2Node_FunctionHandlerBase::BindBlueprintCompileDelegate()
{
	UBlueprint* BP = FBlueprintEditorUtils::FindBlueprintForNode(this);
	if (!BP || BoundBlueprint == BP)
	{
		return;
	}
	UnbindBlueprintCompileDelegate();
	BP->OnCompiled().AddUObject(this, &UK2Node_FunctionHandlerBase::OnBlueprintCompiled);
	BoundBlueprint = BP;
}

void UK2Node_FunctionHandlerBase::UnbindBlueprintCompileDelegate()
{
	if (UBlueprint* BP = BoundBlueprint.Get())
	{
		BP->OnCompiled().RemoveAll(this);
		BoundBlueprint = nullptr;
	}
}

void UK2Node_FunctionHandlerBase::OnBlueprintCompiled(UBlueprint* Blueprint)
{
	RefreshFromHandler();
}

#undef LOCTEXT_NAMESPACE
