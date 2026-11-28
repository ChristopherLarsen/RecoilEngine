# KT1 COMBAT family (LUA-FREE H01): stubs behind the targeting headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/TargetingApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/TargetingBridge.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Targeting/TargetState.cpp")
