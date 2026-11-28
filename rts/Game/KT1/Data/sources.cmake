# KT1 Data (LUA-FREE F04): the JSON config loader and its engine side (rts/Game/KT1/sources.cmake includes this).
# Not a system: it registers nothing, so it adds its sources without kt1_system.
# Json.cpp needs simdjson's headers and definitions in the Game library, which the executable's own link of it does not
# give: KT1_LIBRARIES is linked to GameCommon (rts/Game/CMakeLists.txt).
list(APPEND KT1_LIBRARIES simdjson::simdjson)
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/Check.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Config.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/DataEngine.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Json.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Number.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Reload.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/Schema.cpp"
)
