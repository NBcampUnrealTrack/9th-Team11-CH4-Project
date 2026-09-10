import unreal


success = unreal.NPRelicReturnVisualBlueprintBuilder.build_relic_return_visual_blueprint()
if not success:
    raise RuntimeError("BP_RelicReturnVisual graph generation failed")

unreal.log("BP_RelicReturnVisual graph generated and saved")
