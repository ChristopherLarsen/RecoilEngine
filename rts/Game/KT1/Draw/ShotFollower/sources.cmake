# KT1 EFFECTS family (LUA-FREE H01): stubs behind the shot follower headers, and its engine-free decisions
# (Core/ShotFollower, from branch kt1-d05-shots). No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/PrjDot.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ShotFollowerApi.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/ShotTrack.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/ShotFollower/ShotFollowerCore.cpp")
