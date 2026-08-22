#pragma once

// CNeoVMWorker::Move is used by NeoLib.cpp as well as the interpreter TU.
// Its definition must therefore be visible from NeoVMWorker.h; placing it only
// in NeoVMWorker.cpp works accidentally with MSVC LTCG but is invalid without
// cross-TU optimization on GCC/Clang.
NEOS_FORCEINLINE void CNeoVMWorker::Move(VarInfo* v1, VarInfo* v2)
{
	if (v1->IsAllocType())
	{
		if (v1 == v2)
			return;
		Var_Release(v1);
	}
	if (v2->IsAllocType() == false)
		*v1 = *v2;
	else
		Move_DestNoRelease(v1, v2);
}

// source가 들고 있던 값을 destination으로 넘긴다. source는 release하지 않고
// none으로만 만들어, 호출 프레임 종료 뒤에도 소유권이 closure 보관함 하나에만 남는다.
NEOS_FORCEINLINE void CNeoVMWorker::MoveTake(VarInfo* v1, VarInfo* v2)
{
	if (v1 == v2)
		return;
	if (v1->IsAllocType())
		Var_Release(v1);
	*v1 = *v2;
	v2->ClearType();
}

// [핫패스] 스크립트 함수 호출. 측정상 호출 비용의 78%가 이 프레임 push/pop 에 있어
// (bench_call.ns: 프레임 6.80ns / 반환 0.40ns / 인자 1.50ns) 인라인 대상으로 옮겼다.
// Move 와 같은 이유로 .cpp 가 아니라 헤더에서 보이는 .inl 에 둔다.
NEOS_FORCEINLINE void CNeoVMWorker::Call(int n1, int n2, VarInfo* pReturnValue)
{
	const SFunctionTable& fun = Functions()[n1];
	// n2 is Arg Count not use
	// 호출 스택을 push하거나 현재 프레임을 변경하기 전에 새 프레임 전체를 검증한다.
	if (!EnsureStackRange(_iSP_VarsMax, fun._localAddCount - 1))
		return;
	ClosureInfo* const outerClosure = m_pActiveClosure;
#if _DEBUG
	SCallStack callStack;
	callStack._iReturnOffset = GetCodeptr();
	callStack._iSP_Vars = _iSP_Vars;
	callStack._iSP_VarsMax = _iSP_VarsMax;
	callStack._pReturnValue = pReturnValue;
	callStack._pAsyncWaitReturnValue = nullptr;
	callStack._asyncWaitReturnValue = false;
	callStack._activeClosure = outerClosure;
	m_pCallStack->push_back(callStack);
#else
	SCallStack& callStack = m_pCallStack->push_back();
	callStack._iReturnOffset = GetCodeptr();
	callStack._iSP_Vars = _iSP_Vars;
	callStack._iSP_VarsMax = _iSP_VarsMax;
	callStack._pReturnValue = pReturnValue;
	callStack._pAsyncWaitReturnValue = nullptr;
	callStack._asyncWaitReturnValue = false;
	callStack._activeClosure = outerClosure;
#endif

	SetCodePtr(fun._codePtr);
	_iSP_Vars = _iSP_VarsMax;
	SetStackPointer(_iSP_Vars);
	_iSP_VarsMax = _iSP_Vars + fun._localAddCount;
	if (_iSP_Vars_Max2 < _iSP_VarsMax)
		_iSP_Vars_Max2 = _iSP_VarsMax;
	// 일반 함수 호출은 캡처 환경을 갖지 않는다. 부모 closure의 active 참조는
	// callStack에 보관돼 return/error에서 다시 복원된다.
	if (outerClosure != nullptr)
		m_pActiveClosure = nullptr;

}
