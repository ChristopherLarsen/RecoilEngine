# KT1 COMBAT family (LUA-FREE H01): stubs behind the damage headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/DamageApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/DamageBridge.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Damage/DamageState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/Damage/BlastState.cpp")
