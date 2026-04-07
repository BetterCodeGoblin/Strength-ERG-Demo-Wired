using UnrealBuildTool;

public class StrengthERGDemo : ModuleRules
{
	public StrengthERGDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"Networking",
			"Sockets",
			"UMG",
			"SlateCore",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"OnlineSubsystem"
		});

		// Networking support for TCP socket to ErgBridge
		bEnableExceptions = true;
	}
}
