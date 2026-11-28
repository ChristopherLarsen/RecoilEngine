# KT1 UNITDRAW family, system MinimapUnits (LUA-FREE D02): the minimap's unit layer, from the icon set.
list(APPEND KT1_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../Core/IconLexicon/IconMinimapState.cpp")
kt1_system(NAMES MinimapUnits FAMILY unitdraw SOURCES KT1MinimapUnits.cpp MinimapPainter.cpp)
