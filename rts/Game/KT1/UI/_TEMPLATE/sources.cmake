# KT1 template (LUA-FREE F09): compiled so it cannot rot, never registered. A copy replaces the line below with
#     kt1_system(NAMES <System> SOURCES KT1<System>.cpp)
# A system that links a library names a target the engine build already defines (a new library needs the owner's OK):
#     kt1_system(NAMES <System> SOURCES KT1<System>.cpp LIBS fmt::fmt)
list(APPEND KT1_SOURCES "${CMAKE_CURRENT_LIST_DIR}/KT1TemplatePanel.cpp")
