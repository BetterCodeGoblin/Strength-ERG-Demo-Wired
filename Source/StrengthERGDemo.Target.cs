using UnrealBuildTool;
using System.Collections.Generic;

public class StrengthERGDemoTarget : TargetRules
{
	public StrengthERGDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V3;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5Latest;
		ExtraModuleNames.Add("StrengthERGDemo");
	}
}
