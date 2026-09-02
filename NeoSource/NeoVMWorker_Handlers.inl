// NeoVMWorker_Handlers.inl

#pragma once

// --- Instruction Handlers ---
// Return 'true' to exit the RunInternal loop, 'false' to continue.

NEOS_NOINLINE bool handle_STR_ADD(const SVMOperation& OP) {
    Var_SetStringA(GetVarPtrF1(OP), ToString(GetVarPtr2(OP)) + ToString(GetVarPtr3(OP)));
    return false;
}

NEOS_NOINLINE bool handle_STR_ADD_L(const SVMOperation& OP) {
    Var_SetStringA(GetVarPtr_L(OP.n1), ToString(GetVarPtr_L(OP.n2)) + ToString(GetVarPtr_L(OP.n3)));
    return false;
}

NEOS_FORCEINLINE bool handle_TOSTRING(const SVMOperation& OP) {
    Var_SetStringA(GetVarPtrF1(OP), ToString(GetVarPtr2(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_TOINT(const SVMOperation& OP) {
    Var_SetInt(GetVarPtrF1(OP), ToInt(GetVarPtr2(OP)));
    return false;
}

// _L 변형: 오퍼랜드가 전부 로컬인 것이 프로그램 빌드 시(PatchLocalOps) 확정된 op.
// argFlag 비트 테스트 없이 스택 베이스로 직행한다(test+cmov 제거).
NEOS_FORCEINLINE bool handle_TOINT_L(const SVMOperation& OP) {
    Var_SetInt(GetVarPtr_L(OP.n1), ToInt(GetVarPtr_L(OP.n2)));
    return false;
}

NEOS_FORCEINLINE bool handle_TOFLOAT(const SVMOperation& OP) {
    Var_SetFloat(GetVarPtrF1(OP), ToFloat(GetVarPtr2(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_TOSIZE(const SVMOperation& OP) {
    Var_SetInt(GetVarPtrF1(OP), ToSize(GetVarPtr2(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_GETTYPE(const SVMOperation& OP) {
    Move(GetVarPtrF1(OP), GetType(GetVarPtr2(OP)));
    return false;
}

NEOS_NOINLINE bool handle_SLEEP(const SVMOperation& OP) {
	if (m_iNativeScriptCallDepth > 0) {
		SetErrorFormat(RTE_NESTED_NOT_ALLOWED, "sleep");
		return false;
	}
    int r = Sleep(m_iTimeout, GetVarPtr2(OP));
    if (r == 0) {
        return true; // Exit RunInternal
    }
    else if (r == 2) {
        m_op_process = 0;
    }
    return false; // Continue RunInternal
}

// 캡처 closure 생성(할당 + 캡처 복사 루프)은 함수값 대입에서만 실행되는 드문 경로다.
// FORCEINLINE으로 두면 FMOV1/FMOV2 두 곳에 전개돼 디스패치 루프의 인라인 크기만 키우므로
// 콜드 헬퍼로 밀어낸다 — RunInternal 의 인라인 크기가 곧 성능이다(For/Add3 에서 실측).
NEOS_NOINLINE void set_script_closure_value(VarInfo* dst, int functionIndex) {
    const SFunctionTable& fun = Functions()[functionIndex];
    ClosureInfo* closure = GetVM()->ClosureAlloc(functionIndex, (int)fun._captures.size());
    bool mayContainContainerChild = false;
    for (size_t i = 0; i < fun._captures.size(); ++i)
    {
        VarInfo* source = GetVarPtr_L(fun._captures[i].first);
        if (source->IsContainerType())
            mayContainContainerChild = true;
        Move(&closure->_captures[i], source);
    }
    if (mayContainContainerChild)
        closure->_cycleState._mayContainContainerChild = true;

    // f = fun() { return f; } 처럼 목적지와 캡처 원본이 같은 경우가 있다.
    // 캡처를 먼저 복사해야 기존 alloc 값을 지우고 난 뒤 null을 보관하지 않는다.
    if (dst->IsAllocType())
        Var_Release(dst);
    dst->SetType(VAR_CLOSURE);
    dst->_closure = closure;
    ++closure->_refCount;
}

// 캡처 없는 함수값 대입(대부분의 FMOV)만 인라인에 남긴다.
NEOS_FORCEINLINE void set_script_function_value(VarInfo* dst, int functionIndex) {
    if (Functions()[functionIndex]._captures.empty()) {
        Var_SetFun(dst, functionIndex);
        return;
    }
    set_script_closure_value(dst, functionIndex);
}

NEOS_FORCEINLINE bool handle_FMOV1(const SVMOperation& OP) {
    set_script_function_value(GetVarPtrF1(OP), OP.n2);
    return false;
}

NEOS_FORCEINLINE bool handle_FMOV2(const SVMOperation& OP) {
    VarInfo fun;
    set_script_function_value(&fun, OP.n3);
    CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), &fun);
    Var_Release(&fun);
    return false;
}

NEOS_FORCEINLINE int GetCallReturnValueIndex(const SVMOperation& OP) {
    if (OP.argFlag & NEOS_OP_CALL_NORESULT)
        return -1;
    if (OP.argFlag & NEOS_ARG_N3_LOCAL)
        return _iSP_Vars + OP.n3;
    return -2 - OP.n3;
}

NEOS_FORCEINLINE bool handle_CALL(const SVMOperation& OP) {
    Call(OP.n1, OP.n2, GetCallReturnValueIndex(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_CALL_L(const SVMOperation& OP) {
    // NORESULT 면 반환 슬롯 자체를 안 뽑으므로 PatchLocalOps 가 _L 로 바꾸지 않는다.
    Call(OP.n1, OP.n2, _iSP_Vars + OP.n3);
    return false;
}

// 본문은 base/_L 공용. pFunNamePre 가 있으면 그것을 쓰고(=_L, 로컬 확정),
// nullptr 이면 기존대로 필요한 분기에서만 늦게 뽑는다(n2 == -1 센티널 보호).
// 네이티브 라이브러리 디스패치. 본문 명령어의 대부분이 여기이고 PTRCALL/PTRCALL_L 두 곳에
// 전개되므로 루프 밖으로 뺀다 — CltInsert/Add3 와 같은 fast/rare 분리다.
// pFunName 은 원본과 같이 분기 안에서 늦게 뽑는다(n2 == -1 센티널 보호).
NEOS_NOINLINE bool handle_PTRCALL_native(const SVMOperation& OP, VarInfo* pVar1, VarInfo* pFunNamePre) {
    short n3 = OP.n3;
    VarInfo* pFunName = nullptr;
    switch (pVar1->GetType())
    {
    case VAR_STRING:
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_String, pVar1, pFunName->_str, n3);
        break;
    case VAR_FP_NATIVE:
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(pVar1->_fpNative->_fun, pVar1->_fpNative->_pUserData, pFunName->_str, n3);
        break;
    case VAR_ARRAY:
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_Array, pVar1, pFunName->_str, n3);
        break;
    case VAR_MAP:
    {
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        VarInfo* pVarMeta = pVar1->_tbl->Find(pFunName);
        if (pVarMeta != NULL)
        {
            if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n3))
                _iSP_Vars_Max2 = _iSP_VarsMax + (1 + n3);

            if (pVarMeta->GetType() == VAR_FUN)
            {
                Call(pVarMeta->_fun_index, n3);
                break;
            }
            if (pVarMeta->GetType() == VAR_CLOSURE)
            {
                Call(pVarMeta->_closure, n3);
                break;
            }
        }
        CallNative(GetVM()->_funLib_Map, pVar1, pFunName->_str, n3);
        break;
    }
    case VAR_LIST:
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_List, pVar1, pFunName->_str, n3);
        break;
    case VAR_SET:
        break;
    case VAR_ASYNC:
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_Async, pVar1, pFunName->_str, n3);
        break;
    default:
        SetError(RTE_CALL_INVALID);
        break;
    }
    return false;
}

// 스크립트/네이티브 함수값을 바로 부르는 세 경우만 인라인에 남긴다. 각 두 줄이라
// 디스패치 루프를 거의 키우지 않으면서, 호출당 분기 하나로 끝난다.
NEOS_FORCEINLINE bool handle_PTRCALL_impl(const SVMOperation& OP, VarInfo* pVar1, VarInfo* pFunNamePre) {
    switch (pVar1->GetType())
    {
    case VAR_FUN:
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && -1 == OP.n2)
            Call(pVar1->_fun_index, OP.n3);
        return false;
    case VAR_FUN_NATIVE:
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && -1 == OP.n2)
            Call(pVar1->_funPtr, OP.n3);
        return false;
    case VAR_CLOSURE:
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && -1 == OP.n2)
            Call(pVar1->_closure, OP.n3);
        return false;
    default:
        break;
    }
    return handle_PTRCALL_native(OP, pVar1, pFunNamePre);
}

NEOS_FORCEINLINE bool handle_PTRCALL(const SVMOperation& OP) {
    return handle_PTRCALL_impl(OP, GetVarPtrF1(OP), nullptr);
}

NEOS_FORCEINLINE bool handle_PTRCALL_L(const SVMOperation& OP) {
    // n1/n2 모두 로컬로 확정 → 메서드명 슬롯도 미리 뽑아 넘긴다.
    return handle_PTRCALL_impl(OP, GetVarPtr_L(OP.n1), GetVarPtr_L(OP.n2));
}

// 본문 공용. pRet 은 NORESULT 를 이미 반영한 반환 슬롯(없으면 nullptr).
NEOS_FORCEINLINE bool handle_PTRCALL2_impl(short n2, VarInfo* pFunName, VarInfo* pRet) {
    if (pFunName->GetType() == VAR_STRING)
    {
        CallNative(GetVM()->_funLib_Default, NULL, pFunName->_str, n2, pRet);
    }

    if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
        _iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);
    return false;
}

NEOS_FORCEINLINE bool handle_PTRCALL2(const SVMOperation& OP) {
    return handle_PTRCALL2_impl(OP.n2, GetVarPtrF1(OP),
        (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr3(OP));
}

NEOS_FORCEINLINE bool handle_PTRCALL2_L(const SVMOperation& OP) {
    return handle_PTRCALL2_impl(OP.n2, GetVarPtr_L(OP.n1),
        (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr_L(OP.n3));
}

NEOS_FORCEINLINE bool handle_NATIVECALL(const SVMOperation& OP) {
    short n2 = OP.n2;
    CallDefaultNativeByIndex(OP.n1, n2, (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr3(OP));

    if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
        _iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);
    return false;
}

// 본문 공용. pSrc = 반환값 슬롯(NORESULT 면 nullptr).
NEOS_FORCEINLINE bool handle_RETURN_impl(VarInfo* pSrc) {
    if (m_iBreakingCallStack == (int)m_pCallStack->size())
    {
		// 최상위/코루틴 시작 closure는 호출 프레임이 없으므로 active 상태로 판별한다.
		if (m_pActiveClosure != nullptr)
			FinishClosureReturn(pSrc);

        if (pSrc == nullptr)
            Var_Release(m_pVarStack_Pointer); // Clear
        else
            Move(m_pVarStack_Pointer, pSrc);

        if (m_iBreakingCallStack == 0 && IsMainCoroutine(m_pCur) == false)
        {
            if (StopCoroutine(true) == true) // Other Coroutine Active (No Stop)
                return false; // break, continue loop
        }
        _iSP_VarsMax = _iSP_Vars;
        return true; // exit RunInternal
    }

    SCallStack& callStack = m_pCallStack->pop_back();
	const u32 returnIP = callStack._ReturnIP;
	if (m_pActiveClosure != nullptr)
		FinishClosureReturn(pSrc);

    if (callStack._iReturnValueIndex != -1)
    {
        VarInfo* pReturnValue = (callStack._iReturnValueIndex >= 0)
            ? &(*m_pVarStack_Base)[callStack._iReturnValueIndex]
            : NEOS_GLOBAL_VAR(-callStack._iReturnValueIndex - 2);
        if (pSrc == nullptr)
            Var_Release(pReturnValue); // Clear
        else
            Move(pReturnValue, pSrc);
    }
    else
    {
        if (pSrc == nullptr)
            Var_Release(m_pVarStack_Pointer); // Clear
        else
            Move(m_pVarStack_Pointer, pSrc);
    }

    if (returnIP & FLAG_ASYNCWAIT)
    {
        if (m_pCur == nullptr || m_pCur->m_sAsyncWaitReturnStack.size() == 0)
        {
            SetError(RTE_ASYNC_RESUME_STATE);
            return false;
        }
        SAsyncWaitReturnState& state = m_pCur->m_sAsyncWaitReturnStack.pop_back();
        Var_SetBool(state._pReturnValue, state._returnValue);
    }

	SetCodePtr((int)(returnIP & IP_MASK));
	_iSP_Vars = callStack._iSP_Vars;
	SetStackPointer(_iSP_Vars);
	_iSP_VarsMax = callStack._iSP_VarsMax;
	if (returnIP & FLAG_CLOSURE)
	{
		SClosureCallState& state = m_pClosureCallStack->pop_back();
		m_pActiveClosure = state._closure;
		if (m_pCur)
			m_pCur->_activeClosure = m_pActiveClosure;
	}
    return false; // break, continue loop
}

NEOS_FORCEINLINE bool handle_RETURN(const SVMOperation& OP) {
    return handle_RETURN_impl((OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtrF1(OP));
}

NEOS_FORCEINLINE bool handle_RETURN_L(const SVMOperation& OP) {
    // NORESULT 면 슬롯을 안 뽑으므로 PatchLocalOps 가 _L 로 바꾸지 않는다.
    return handle_RETURN_impl(GetVarPtr_L(OP.n1));
}

NEOS_FORCEINLINE bool handle_TABLE_ALLOC(const SVMOperation& OP) {
    Var_SetTable(GetVarPtrF1(OP), GetVM()->TableAlloc(OP.n23));
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_READ(const SVMOperation& OP) {
    CltRead(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_READ_L(const SVMOperation& OP) {
    CltRead(GetVarPtr_L(OP.n1), GetVarPtr_L(OP.n2), GetVarPtr_L(OP.n3));
    return false;
}

// Program 로드 시 n2가 static 문자열 상수인 READ만 이 경로로 바뀐다.
// 키는 워커별 static 슬롯에서 매번 읽고 FindString도 매번 수행한다. 따라서 맵 수정이나
// 풀 재사용 뒤의 value 포인터를 보관하는 inline cache와 달리 수명/무효화 상태가 없다.
NEOS_FORCEINLINE bool handle_CLT_READ_STATIC_STRING(const SVMOperation& OP) {
    VarInfo* pClt = GetVarPtrF1(OP);
    VarInfo* pKey = GetVarPtr_G(OP.n2);
    VarInfo* pValue = GetVarPtr3(OP);
    if (pClt->GetType() == VAR_MAP) {
        VarInfo* pFind = pClt->_tbl->FindString(pKey->_str);
        if (pFind)
            Move(pValue, pFind);
        else
            Var_Release(pValue);
        return false;
    }

    // static 문자열로 가능한 나머지 컨테이너/프로퍼티 의미론은 범용 경로와 같다.
    CltReadRare(pClt, pKey, pValue);
    return false;
}

NEOS_NOINLINE bool handle_TABLE_REMOVE(const SVMOperation& OP) {
    TableRemove(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_MOV(const SVMOperation& OP) {
    CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_MOV_L(const SVMOperation& OP) {
    CltInsert(GetVarPtr_L(OP.n1), GetVarPtr_L(OP.n2), GetVarPtr_L(OP.n3));
    return false;
}

NEOS_NOINLINE bool handle_TABLE_ADD2(const SVMOperation& OP) {
    VarInfo* pTable = GetVarPtrF1(OP);
    if (pTable->GetType() == VAR_ARRAY)
    {
        ArrayModify(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_ADD2);
        return false;
    }
    VarInfo* pVarTemp = GetTableItemValid(pTable, GetVarPtr2(OP));
    if (pVarTemp) Add2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_NOINLINE bool handle_TABLE_SUB2(const SVMOperation& OP) {
    VarInfo* pTable = GetVarPtrF1(OP);
    if (pTable->GetType() == VAR_ARRAY)
    {
        ArrayModify(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_SUB2);
        return false;
    }
    VarInfo* pVarTemp = GetTableItemValid(pTable, GetVarPtr2(OP));
    if (pVarTemp) Sub2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_NOINLINE bool handle_TABLE_MUL2(const SVMOperation& OP) {
    VarInfo* pTable = GetVarPtrF1(OP);
    if (pTable->GetType() == VAR_ARRAY)
    {
        ArrayModify(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_MUL2);
        return false;
    }
    VarInfo* pVarTemp = GetTableItemValid(pTable, GetVarPtr2(OP));
    if (pVarTemp) Mul2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_NOINLINE bool handle_TABLE_DIV2(const SVMOperation& OP) {
    VarInfo* pTable = GetVarPtrF1(OP);
    if (pTable->GetType() == VAR_ARRAY)
    {
        ArrayModify(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_DIV2);
        return false;
    }
    VarInfo* pVarTemp = GetTableItemValid(pTable, GetVarPtr2(OP));
    if (pVarTemp) Div2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_NOINLINE bool handle_TABLE_PERSENT2(const SVMOperation& OP) {
    VarInfo* pTable = GetVarPtrF1(OP);
    if (pTable->GetType() == VAR_ARRAY)
    {
        ArrayModify(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_PERSENT2);
        return false;
    }
    VarInfo* pVarTemp = GetTableItemValid(pTable, GetVarPtr2(OP));
    if (pVarTemp) Per2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_LIST_ALLOC(const SVMOperation& OP) {
    Var_SetList(GetVarPtrF1(OP), GetVM()->ListAlloc(OP.n23));
    return false;
}

NEOS_FORCEINLINE bool handle_LIST_ALLOC_L(const SVMOperation& OP) {
    Var_SetList(GetVarPtr_L(OP.n1), GetVM()->ListAlloc(OP.n23));
    return false;
}

NEOS_NOINLINE bool handle_LIST_REMOVE(const SVMOperation& OP) {
    TableRemove(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_VERIFY_TYPE(const SVMOperation& OP) {
    VerifyType(GetVarPtrF1(OP), (VAR_TYPE)OP.n2);
    return false;
}

NEOS_FORCEINLINE bool handle_CHANGE_INT(const SVMOperation& OP) {
    ChangeNumber(GetVarPtrF1(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_CHANGE_INT_L(const SVMOperation& OP) {
    ChangeNumber(GetVarPtr_L(OP.n1));
    return false;
}

NEOS_NOINLINE bool handle_YIELD(const SVMOperation& OP) {
	if (m_iNativeScriptCallDepth > 0) {
		SetErrorFormat(RTE_NESTED_NOT_ALLOWED, "yield");
		return false;
	}
    if (StopCoroutine(false) == false)
        return true; // Exit RunInternal
    return false; // Continue RunInternal
}

NEOS_NOINLINE bool handle_IDLE(const SVMOperation& OP) {
    if (m_pCur == nullptr || m_pCur->m_sAsyncResumeCodePtrs.empty())
    {
        SetError(RTE_ASYNC_RESUME_STATE);
        return false;
    }

    AsyncResumeInfo resumeInfo = m_pCur->m_sAsyncResumeCodePtrs.back();
    m_pCur->m_sAsyncResumeCodePtrs.pop_back();
    if (resumeInfo.pReturnValue != nullptr)
        resumeInfo.returnValue = resumeInfo.pReturnValue->IsTrue();
    SetCodePtr(resumeInfo.codePtr);
    while (true)
    {
        AsyncInfo* p = GetVM()->Pop_AsyncInfo(this);
        if (p == nullptr) break;
        if (!EnsureStackRange(_iSP_VarsMax, 2))
        {
            GetVM()->Var_Release(&p->_LockReferance);
			GetVM()->Var_Release(&p->_callback);
			// EnsureStackRange redirected execution to the error opcode.
			// Continue the loop so RunInternal reports the VM error.
            return false;
        }
        Var_SetBool(GetStackFromBase(_iSP_VarsMax + 1), p->_success);
        Var_SetStringA(GetStackFromBase(_iSP_VarsMax + 2), p->_resultValue);
		VarInfo callback;
		Move_DestNoRelease(&callback, &p->_callback);
		const int callStackSize = m_pCallStack->size();
		if (callback.GetType() == VAR_CLOSURE)
			Call(callback._closure, 2);
		else
			Call(callback._fun_index, 2);
        if (resumeInfo.pReturnValue != nullptr && m_pCallStack->size() == callStackSize + 1)
        {
            SAsyncWaitReturnState& state = m_pCur->m_sAsyncWaitReturnStack.push_back();
            state._pReturnValue = resumeInfo.pReturnValue;
            state._returnValue = resumeInfo.returnValue;
            (*m_pCallStack)[callStackSize]._ReturnIP |= FLAG_ASYNCWAIT;
        }
		GetVM()->Var_Release(&p->_LockReferance);
		GetVM()->Var_Release(&p->_callback);
		GetVM()->Var_Release(&callback);
    }
    return false;
}

// 벡터 생성 intrinsic (LoadVM 에서 PTRCALL2(#Vector*) 패치). native 호출 프레임 없이
// 인자 슬롯(_iSP_VarsMax+1..N)을 직접 읽어 값타입 세팅. n3=결과 슬롯.
// VecStoreFor 를 인라인으로 옮긴 뒤로는 이 핸들러가 디스패치 루프에 전개되면 그만큼
// 루프가 커진다. 생성은 성분 쓰기·산술만큼 자주 돌지 않으므로 루프 밖에 둔다.
NEOS_NOINLINE bool handle_VEC_MAKE(const SVMOperation& OP) {
    if (OP.argFlag & NEOS_OP_CALL_NORESULT) return false;
    // n2 = 인자 수 = 성분 수. Vector2/3/4/Quaternion 이 모두 이 op 으로 패치되며,
    // 무엇으로 해석할지는(예: wxyz 쿼터니언) 사용자 규약이지 VM 의 타입이 아니다.
    int n = OP.n2;
    if (n < 1) n = 1; else if (n > 4) n = 4;
    VecInfo* p = VecStoreFor(GetVarPtr3(OP), n);
    for (int i = 0; i < n; i++)
        p->v[i] = (float)GetStackFromBase(_iSP_VarsMax + 1 + i)->GetFloatNumber();
    if (_iSP_Vars_Max2 < _iSP_VarsMax + 1 + n) _iSP_Vars_Max2 = _iSP_VarsMax + 1 + n;
    return false;
}

// switch: n1 = table index, n2 = 조건식 위치. 매칭 실패/지원 외 타입이면 default 로.
// 점프는 op 단위 상대(다음 op 기준)라 기존 JMP 계열과 동일하다.
NEOS_NOINLINE bool handle_SWITCH(const SVMOperation& OP) {
    int jumpOffset = 0;
    if (false == TryGetSwitchJumpOffset((u16)OP.n1, GetVarPtr2(OP), jumpOffset))
    {   // 정상 컴파일 결과에서는 발생하지 않는다 (손상된 이미지 등)
        SetError(RTE_SWITCH_TABLE);
        return false;
    }
    SetCodeIncPtr(jumpOffset);
    return false;
}

NEOS_FORCEINLINE bool handle_NONE(const SVMOperation& OP) {
    // No operation
    return false;
}

NEOS_NOINLINE bool handle_ERROR(const SVMOperation& OP) {
    int idx = _isErrorOPIndex;
    bool blDebugInfo = IsDebugInfo();
    int _lineseq = -1;
    if (blDebugInfo)
        _lineseq = GetDebugLine(idx);

    char chMsg[256];
#ifdef _WIN32
    snprintf(chMsg, _countof(chMsg), "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#else
    sprintf(chMsg, "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#endif

    std::string detail = chMsg;
#if 0
    if (GetVM()->_sErrorMsgDetail.empty())
        GetVM()->_sErrorMsgDetail = chMsg;
#else
    detail += FormatStackTrace(idx);
    if (GetVM()->_sErrorMsgDetail.empty())
        GetVM()->_sErrorMsgDetail = detail;
#endif

    // Report the VM error even when a debugger converts execution into a
    // fault snapshot.  Keep the VM log payload bounded as well; the editor
    // log stores only its first line.
    if (NeoVMSystem::m_pFunError) {
        static const size_t kMaxErrorLogLength = 4096;
        if (detail.size() > kMaxErrorLogLength)
        {
            std::string logDetail = detail.substr(0, kMaxErrorLogLength);
            logDetail += "\n... Script call stack truncated in log (see debugger for full stack)";
            NeoVMSystem::m_pFunError(logDetail.c_str());
        }
        else
        {
            NeoVMSystem::m_pFunError(detail.c_str());
        }
    }

    if (m_pDebugListener || m_iDebugBreakCount > 0 || m_eDebugRunMode != DBG_CONTINUE || m_bDebugPauseRequested)
    {
        if (idx >= 0 && idx < (int)DebugData().size())
            StopDebug(idx, NEO_DEBUG_STOP_EXCEPTION);
        return true;
    }

    _isErrorOPIndex = 0;
	// 오류 unwind는 정상 return을 거치지 않는다. 현재/부모 closure의 캡처 슬롯을
	// 먼저 동기화하고 실행 보유분을 모두 놓는다.
	UnwindActiveClosures();
    _iSP_Vars = 0;
    SetStackPointer(_iSP_Vars);

    return true; // Exit RunInternal
}
