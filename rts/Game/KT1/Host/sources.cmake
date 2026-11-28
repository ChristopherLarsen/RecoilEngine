# KT1 Host (rts/Game/KT1/sources.cmake): the code stock files call. No system registers here: every .cpp of this folder
# is built, so a new Host file (F05, F06, ...) needs no edit to this fragment.
file(GLOB kt1_host_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")
list(SORT kt1_host_sources)
list(APPEND KT1_SOURCES ${kt1_host_sources})
