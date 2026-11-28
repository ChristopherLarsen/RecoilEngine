# KT1 system fragment (rts/Game/KT1/sources.cmake): the COMBAT family Targeting rule (LUA-FREE S19)
kt1_system(NAMES Targeting FAMILY combat SOURCES
	KT1Targeting.cpp KT1TargetingDefs.cpp KT1TargetingWorld.cpp TargetingApi.cpp TargetingBridge.cpp
	../../Core/Targeting/TargetState.cpp)
