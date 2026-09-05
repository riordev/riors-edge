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
            // The crowd probe's dominance readout: thread clocks live in
            // RenderCore (RenderTimer.h), the GPU cycle counter in RHI.
            "RHI", "RenderCore",
            // Data/BreakerCensus.cpp: the census export is JSON.
            "Json",
            // UI/BreakerEffectRenderer.cpp: the four moments play an authored
            // Niagara system through a pooled component when one exists.
            "Niagara"
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
