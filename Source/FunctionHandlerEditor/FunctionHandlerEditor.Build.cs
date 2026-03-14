using UnrealBuildTool;

public class FunctionHandlerEditor : ModuleRules
{
	public FunctionHandlerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"FunctionHandler",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"PropertyEditor",
			"InputCore",
			"ToolWidgets"
		});
	}
}
