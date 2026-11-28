# KT1 FRAME family (LUA-FREE D07). Zoom: the level and the gates other families read (always on). ViewSettings: the engine
# settings and draw masks that follow them (family frame).
kt1_system(NAMES Zoom SOURCES KT1Zoom.cpp ZoomGatesApi.cpp ZoomBridge.cpp)
kt1_system(NAMES ViewSettings FAMILY frame SOURCES KT1ViewSettings.cpp ViewObjects.cpp ViewProps.cpp ViewWorld.cpp ViewWater.cpp ViewNow.cpp)
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ZoomLevel.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewSettingsState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewCullState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewSceneState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewPropState.cpp")
