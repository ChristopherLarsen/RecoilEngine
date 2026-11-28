# KT1 EFFECTS family (LUA-FREE H01): stubs behind the synced effect system's headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/FxEventsApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/FxEventsBridge.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxBudget.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxBuildWork.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxCriteria.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxDeath.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxLogic.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxLogicUnits.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxStrategic.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/FxEvents/FxWreck.cpp")
