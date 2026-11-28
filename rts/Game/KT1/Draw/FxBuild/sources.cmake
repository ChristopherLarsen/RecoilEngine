# KT1 EFFECTS family (LUA-FREE H01): stubs behind the build effect headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/BuildBeams.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/CriteriaFx.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/FogPuff.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/FxBuildApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ReclaimStream.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/SiteGhost.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/SiteGhostDraw.cpp")
