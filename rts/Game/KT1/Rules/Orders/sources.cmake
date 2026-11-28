# KT1 system fragment (rts/Game/KT1/sources.cmake)
kt1_system(NAMES Orders SOURCES KT1Orders.cpp KT1OrdersFallback.cpp)
# KT1 ORDERS family (LUA-FREE H01): stubs behind the family's headers; the system above is registered already.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/OrdersApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/OrdersBridge.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/OrdersForeign.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/OrdersInternal.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/OrdersShared.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/OrdersTargetPick.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/DefFacts.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/Grids.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/GuardRules.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/MoveTracking.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/OrderTypes.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/OrdersConfig.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/OrdersState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/QueueHandling.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Orders/QueueRules.cpp")
