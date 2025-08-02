// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Gravity : ModuleRules
{
	public Gravity(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"RHI",
			"Core",
            "RenderCore",
			"Renderer",
            "CoreUObject",
			"Engine",
			"InputCore",
            "PhysicsCore"
        });
	}
}
