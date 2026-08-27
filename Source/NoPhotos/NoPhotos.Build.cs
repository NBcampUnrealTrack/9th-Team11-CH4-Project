// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NoPhotos : ModuleRules
{
	public NoPhotos(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"ChaosSolverEngine",
			"PhysicsControl",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"CableComponent",
			"Niagara",
			"UMG",
			"Slate",
			"SlateCore",
			"OnlineSubsystem"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"OnlineSubsystemUtils",
			"ImageCore",
			"GeometryCollectionEngine",
			"PhysicsCore",
			"NavigationSystem"
		});

		PublicIncludePaths.Add(ModuleDirectory);

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

	}
}
