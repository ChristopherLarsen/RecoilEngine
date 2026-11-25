
# This list was created using this *nix shell command:
# > find . -name "*.cpp" | sort
make_global_var(sources_engine_Game
		"${CMAKE_CURRENT_SOURCE_DIR}/Action.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/AviVideoCapturing.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/CameraController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/FPSController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/FreeController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/OverheadController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/OverviewController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/RotOverheadController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/SpringController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/DollyController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Camera/KardashevCamera.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/CameraHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ChatMessage.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ClientData.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ClientSetup.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/CommandMessage.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Console.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ConsoleHistory.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/DummyVideoCapturing.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/FPSUnitController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Game.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameControllerTextInput.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameInputReceiver.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameData.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameHelper.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameSetup.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GameVersion.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/GlobalUnsynced.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/SupComBuildSnap.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/SupComLocalView.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/SupComFrameStep.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/IVideoCapturing.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/InMapDraw.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/InMapDrawModel.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/LoadScreen.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Players/Player.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Players/PlayerBase.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Players/PlayerHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Players/PlayerStatistics.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Players/TeamController.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PreGame.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/SelectedUnitsHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/SupComSelect.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/KT1Registry.cpp" # KT1 hook: registry (KT1 sources: one line per file)
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/KT1RuleList.cpp" # KT1 hook: registry
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/Census/KT1Census.cpp" # KT1 hook: Census
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/OwnPass/KT1OwnPass.cpp" # KT1 hook: OwnPass
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/Orders/KT1Orders.cpp" # KT1 hook: Orders
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/Plants/KT1Plants.cpp" # KT1 hook: Plants
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/Fuel/KT1Fuel.cpp" # KT1 hook: Fuel
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/Builders/KT1Builders.cpp" # KT1 hook: Builders
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/Intel/KT1Intel.cpp" # KT1 hook: Intel
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/Rules/UnitAnim/KT1UnitAnim.cpp" # KT1 hook: UnitAnim
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/UI/Ribbons/KT1Ribbons.cpp" # KT1 hook: Ribbons
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/UI/IconCull/KT1IconCull.cpp" # KT1 hook: IconCull
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/UI/FarView/KT1FarView.cpp" # KT1 hook: FarView
		"${CMAKE_CURRENT_SOURCE_DIR}/KT1/UI/Rnd005/KT1Rnd005.cpp" # KT1 hook: Terrain, Draw, Decals
		"${CMAKE_CURRENT_SOURCE_DIR}/SelectedUnitsAI.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/SyncedGameCommands.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/TraceRay.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/CommandColors.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/CursorIcons.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/EndGameBox.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/GameInfo.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/GameSetupDrawer.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/GuiHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/SupComBuildPreview.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/Groups/Group.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/Groups/GroupHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/HwMouseCursor.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/InfoConsole.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/InputReceiver.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/KeyBindings.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/KeyCodes.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/ScanCodes.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/IKeys.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/KeySet.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/MiniMap.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/SupComMiniMapZoom.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/MouseCursor.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/MouseHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/PlayerRoster.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/PlayerRosterDrawer.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/ProfileDrawer.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/QuitBox.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/ResourceBar.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/SelectionKeyHandler.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/ShareBox.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/StartPosSelecter.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/TooltipConsole.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UI/UnitTracker.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/UnsyncedGameCommands.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/WaitCommandsAI.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/WordCompletion.cpp"
	)

# Build-variant-agnostic Game library, see recoil_add_variant_library in rts/builds/CMakeLists.txt.
add_library(GameCommon INTERFACE)
target_sources(GameCommon INTERFACE ${sources_engine_Game})
target_include_directories(GameCommon INTERFACE "${CMAKE_BINARY_DIR}/src-generated/engine")
target_link_libraries(GameCommon INTERFACE
	Tracy::TracyClient
	RmlUi::Core
	prd::jsoncpp
	streflop
	fmt::fmt
	nowide::nowide
)
add_dependencies(GameCommon generateVersionFiles)
