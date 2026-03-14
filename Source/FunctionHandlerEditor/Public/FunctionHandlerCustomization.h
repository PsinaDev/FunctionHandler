// Copyright PsinaDev. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IStructureDetailsView.h"
#include "UObject/StructOnScope.h"

class IPropertyHandle;
class IPropertyUtilities;
class IDetailGroup;

class FFunctionHandlerCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

#pragma region IPropertyTypeCustomization

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

#pragma endregion IPropertyTypeCustomization

private:

#pragma region Helpers

	void CollectFunctionNames(const UClass* Class, TArray<TSharedPtr<FString>>& OutNames) const;

	static bool IsInputParameter(const FProperty* Prop);
	static bool HasInputParameters(UFunction* Function);

	static void ImportMapToBuffer(UFunction* Function, const TMap<FName, FString>& Map, uint8* Buffer);

	static void ExportBufferToMap(UFunction* Function, const uint8* Buffer, TMap<FName, FString>& OutMap);

	static void ExportParameterDefaults(UFunction* Function, struct FFunctionHandler* Handler);

#pragma endregion Helpers

#pragma region CachedState

	TSharedPtr<IPropertyHandle> StructHandle;
	TSharedPtr<IPropertyHandle> TargetClassHandle;
	TSharedPtr<IPropertyHandle> FunctionNameHandle;
	TSharedPtr<IPropertyHandle> ParameterValuesHandle;
	TSharedPtr<IPropertyUtilities> PropertyUtilities;

	TSharedPtr<FStructOnScope> ParameterBuffer;
	TSharedPtr<IStructureDetailsView> ParameterDetailsView;

	TArray<TSharedPtr<FString>> CachedFunctionNames;

#pragma endregion CachedState
};
