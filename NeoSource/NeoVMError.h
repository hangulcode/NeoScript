#pragma once

// 런타임(실행 중) 에러 메시지를 한 곳에 모은 테이블.
// 컴파일 에러(NeoParser.cpp 의 PARSER_COMPILE_ERROR_LIST)와 같은 X-매크로 방식이다.
// 메시지에 위치(IP/Line)와 콜스택은 SetError 가 자동으로 덧붙이므로 여기엔 원인만 적는다.

namespace NeoScript
{

#define VM_RUNTIME_ERROR_LIST(X) \
	/* 연산 */ \
	X(RTE_INVALID_OPERATOR,        "invalid operator '%s' for %s") \
	X(RTE_INVALID_COMPARE,         "cannot compare %s with '%s'") \
	/* 컨테이너 접근 */ \
	X(RTE_INDEX_READ,              "cannot read by index from %s") \
	X(RTE_INDEX_WRITE,             "cannot assign by index to %s") \
	X(RTE_KEY_NOT_FOUND,           "key not found in map") \
	X(RTE_VECTOR_INDEX_READ,       "vector index out of range on read") \
	X(RTE_VECTOR_INDEX_WRITE,      "vector index out of range on assign") \
	/* foreach */ \
	X(RTE_FOREACH_UNSUPPORTED,     "foreach does not support %s") \
	X(RTE_FOREACH_MODIFIED,        "collection was modified during foreach") \
	X(RTE_FOREACH_LIST_TWOVAR,     "foreach on list does not support two variables. use: foreach(var v in list)") \
	X(RTE_FOREACH_SET_TWOVAR,      "foreach on set does not support two variables. use: foreach(var v in set)") \
	/* 호출 */ \
	X(RTE_CALL_INVALID,            "invalid function call") \
	X(RTE_CALL_NULL,               "cannot call a null function") \
	X(RTE_CALL_ARG_COUNT,          "function call argument count mismatch") \
	X(RTE_FUNCTION_NOT_FOUND,      "function not found") \
	X(RTE_CALL_STACK_OVERFLOW,     "call stack overflow") \
	/* 코루틴 / async */ \
	X(RTE_COROUTINE_INVALID,       "invalid coroutine operation") \
	X(RTE_COROUTINE_STATE,         "invalid coroutine state") \
	X(RTE_ASYNC_NO_CONTEXT,        "async dispatch without an execution context") \
	X(RTE_ASYNC_RESUME_STATE,      "async resume state is missing") \
	/* native 재진입 제약 (Script -> native -> Script) */ \
	X(RTE_NESTED_NOT_ALLOWED,      "%s is not allowed in a synchronous native-to-script call") \
	/* 기타 */ \
	X(RTE_SLEEP_VALUE,             "invalid sleep value") \
	X(RTE_UNKNOWN_OP,              "unknown opcode") \
	X(RTE_SWITCH_TABLE,            "invalid switch table index") \
	X(RTE_INVALID_VAR_TYPE,        "invalid variable type in image") \
	X(RTE_SET_UNSUPPORTED,         "set does not support '%s'") \
	X(RTE_EXCEPTION,               "exception") \
	X(RTE_PROGRAM_REQUIRED,        "CNeoVMProgram is required") \
	X(RTE_EXEC_POOL_REQUIRED,      "NeoExecContextPool is required") \
	X(RTE_DEFAULT_VALUE,           "unknown default value")

enum ENeoRuntimeError
{
#define X(name, message) name,
	VM_RUNTIME_ERROR_LIST(X)
#undef X
	RTE_COUNT
};

extern const char* g_sNeoRuntimeErrors[RTE_COUNT];

};
