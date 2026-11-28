# KT1 UNITDRAW family, system Icons (LUA-FREE D01): the strategic icons and the icon set the minimap layer shares.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/IconLexicon/IconClass.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/IconLexicon/IconState.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/IconLexicon/IconHud.cpp")
kt1_system(NAMES Icons FAMILY unitdraw SOURCES KT1Icons.cpp IconSet.cpp IconQuery.cpp IconPainter.cpp IconsBridge.cpp)
