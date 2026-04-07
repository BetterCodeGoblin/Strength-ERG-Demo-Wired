using UnrealBuildTool;
using System.Collections.Generic;

public class StrengthERGDemoTarget : TargetRules
{
	public StrengthERGDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_6;
		CppStandard = CppStandardVersion.Cpp20;
		ExtraModuleNames.Add("StrengthERGDemo");
	}
}
