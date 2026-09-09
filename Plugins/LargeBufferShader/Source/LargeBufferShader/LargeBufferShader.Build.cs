using UnrealBuildTool;

public class LargeBufferShader : ModuleRules

{

    public LargeBufferShader(ReadOnlyTargetRules Target) : base(Target)

    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects"
        });
    }

}
