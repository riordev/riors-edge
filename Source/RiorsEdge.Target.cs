using UnrealBuildTool;

public class RiorsEdgeTarget : TargetRules
{
    public RiorsEdgeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("RiorsEdge");
    }
}
