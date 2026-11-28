# KT1 Data (LUA-FREE F04): the JSON config loader and its engine side (rts/Game/KT1/sources.cmake includes this).
# Not a system: it registers nothing, so it adds its sources without kt1_system. simdjson is already linked by the engine.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/Config.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/DataEngine.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Reload.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Schema.cpp"
)
