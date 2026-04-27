using UnrealBuildTool;
using System.Collections.Generic;

public class StrengthERGEditorTarget : TargetRules
{
    public StrengthERGEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("StrengthERG");
    }
}
