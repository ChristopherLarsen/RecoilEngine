# KT1 FRAME family (LUA-FREE H01): the zoom gate API other families link against, and the stubs behind the ViewSettings
# headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/ZoomGatesApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ZoomBridge.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ZoomLevel.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewSettingsState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewCullState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/ViewSceneState.cpp")
