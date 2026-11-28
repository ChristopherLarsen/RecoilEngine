# KT1 COMBAT family (LUA-FREE H01): stubs behind the timed rules headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/TimedRulesApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/TimedRulesBridge.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/TimedRules/TimedState.cpp")
