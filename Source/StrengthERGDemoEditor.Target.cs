using UnrealBuildTool;
using System.Collections.Generic;

public class StrengthERGDemoEditorTarget : TargetRules
{
	public StrengthERGDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V3;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_3;
		ExtraModuleNames.Add("StrengthERGDemo");
	}
}
