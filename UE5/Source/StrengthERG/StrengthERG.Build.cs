// StrengthERG.Build.cs
// UE5 module build file — converted from Unity C# project.

using UnrealBuildTool;

public class StrengthERG : ModuleRules
{
    public StrengthERG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "UMG",
            "Slate",
            "SlateCore",
            "Sockets",          // TCP socket for ErgBridge communication
            "Networking",       // FSocket wrappers
            "EnhancedInput"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "OnlineSubsystem"
        });

        // Allow cross-module includes
        PublicIncludePaths.AddRange(new string[]
        {
            "StrengthERG/Public",
            "StrengthERG/Game/Public"
        });
    }
}
