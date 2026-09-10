using UnrealBuildTool;

public class NoPhotosEditor : ModuleRules
{
    public NoPhotosEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "NoPhotos",
                "GoogleSheetLoader",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "BlueprintGraph",
                "KismetCompiler",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "UnrealEd"
            }
        );
    }
}
