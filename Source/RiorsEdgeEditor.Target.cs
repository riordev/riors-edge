using UnrealBuildTool;

public class RiorsEdgeEditorTarget : TargetRules
{
    public RiorsEdgeEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RiorsEdge");
    }
}
