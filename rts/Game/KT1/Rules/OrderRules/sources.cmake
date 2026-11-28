# KT1 ORDERS family (LUA-FREE H01): stubs behind the family's headers. No system registers yet.
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/OrderRules/GuardLogic.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/../../Core/OrderRules/OrderRuleRow.cpp")
# >>> kt1_scaffold orderrules
kt1_system(NAMES OrderRules FAMILY orders SOURCES KT1OrderRules.cpp OrderRulesCheck.cpp)
# <<< kt1_scaffold orderrules
# the table's rows (S11, S14)
list(APPEND KT1_SOURCES
	"${CMAKE_CURRENT_LIST_DIR}/OrderRulesCommand.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/OrderRulesSteps.cpp")
