using UnrealBuildTool;

public class RiorsEdge : ModuleRules
{
    public RiorsEdge(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "ApplicationCore", "EnhancedInput",
            "GameplayAbilities", "GameplayTags", "GameplayTasks", "Slate", "SlateCore",
            // The travel loading screen (Game/BreakerGameInstance.cpp): the
            // engine's own draw-while-loading mechanism, nothing more.
            "MoviePlayer"
        });
        if (Target.bBuildEditor)
        {
            // UI/BreakerFontTools.cpp: the role-font builder registers the
            // fonts it saves with the asset registry. Editor-only, like the
            // command itself.
            PrivateDependencyModuleNames.Add("AssetRegistry");
        }
    }
}
