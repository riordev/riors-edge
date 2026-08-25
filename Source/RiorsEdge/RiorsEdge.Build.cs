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
    }
}
