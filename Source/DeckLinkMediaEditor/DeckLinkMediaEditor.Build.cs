// Copyright 2017 The Mill, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DeckLinkMediaEditor : ModuleRules
	{
		public DeckLinkMediaEditor( ReadOnlyTargetRules Target ) : base(Target)
        {
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
                    "DeckLinkMedia",
					"MediaAssets",
					"UnrealEd",
                }
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					"DeckLinkMediaEditor/Private",
				}
			);
		}
	}
}
