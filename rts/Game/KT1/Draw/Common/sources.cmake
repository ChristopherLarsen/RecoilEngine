# KT1 Draw/Common: the shared draw code (FrameFacts, InstanceBatch). No system registers here: every .cpp is built.
file(GLOB kt1_draw_common_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
list(SORT kt1_draw_common_sources)
list(APPEND KT1_SOURCES ${kt1_draw_common_sources})
