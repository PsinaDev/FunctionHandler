using UnrealBuildTool;

public class FunctionHandlerUncooked : ModuleRules
{
	public FunctionHandlerUncooked(ReadOnlyTargetRules Target) : base(Target)
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
			"BlueprintGraph",
			"KismetCompiler",
			"GraphEditor",
			"ToolMenus",
			"ToolWidgets",
			"InputCore",
		});
	}
}
