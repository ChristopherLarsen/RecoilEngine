# KT1 FRAME family (LUA-FREE V01): the frame services for widgets and native readers.
kt1_system(NAMES Frame FAMILY frame SOURCES FrameServices.cpp FrameBridge.cpp KT1Frame.cpp)
list(APPEND KT1_SOURCES "${CMAKE_CURRENT_LIST_DIR}/../../Core/ViewSettings/FrameChanges.cpp")
