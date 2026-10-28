// NeoVMWorker_Handlers.inl

#pragma once

// --- Instruction Handlers ---
// Return 'true' to exit the RunInternal loop, 'false' to continue.

NEOS_NOINLINE bool handle_STR_ADD(const SVMOperation& OP) {
    VarInfo* first = GetVarPtr2(OP);
    VarInfo* second = GetVarPtr3(OP);
    if (first->GetType() == VAR_STRING && second->GetType() == VAR_STRING)
        CheckRuntimeStringGrowth(first->_str->_str.size(), second->_str->_str.size());
    std::string left = ToString(first), right = ToString(second);
    CheckRuntimeStringGrowth(left.size(), right.size());
    left += right;
    Var_SetStringA(GetVarPtrF1(OP), left);
    return false;
}

NEOS_NOINLINE bool handle_STR_ADD_L(const SVMOperation& OP) {
    VarInfo* first = GetVarPtr_L(OP.n2);
    VarInfo* second = GetVarPtr_L(OP.n3);
    if (first->GetType() == VAR_STRING && second->GetType() == VAR_STRING)
        CheckRuntimeStringGrowth(first->_str->_str.size(), second->_str->_str.size());
    std::string left = ToString(first), right = ToString(second);
    CheckRuntimeStringGrowth(left.size(), right.size());
    left += right;
    Var_SetStringA(GetVarPtr_L(OP.n1), left);
    return false;
}

NEOS_NOINLINE bool handle_TOSTRING(const SVMOperation& OP) {
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

NEOS_NOINLINE bool handle_TOSIZE(const SVMOperation& OP) {
    Var_SetInt(GetVarPtrF1(OP), ToSize(GetVarPtr2(OP)));
    return false;
}

NEOS_NOINLINE bool handle_GETTYPE(const SVMOperation& OP) {
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

NEOS_NOINLINE bool handle_FMOV1(const SVMOperation& OP) {
    set_script_function_value(GetVarPtrF1(OP), OP.n2);
    return false;
}

NEOS_NOINLINE bool handle_FMOV2(const SVMOperation& OP) {
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
    // 전역 목적지는 opcode와 같은 음수 오프셋을 저장한다. 기준점은 상수 0이다.
    return OP.n3;
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
    if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && OP.n2 == -1) {
        SetErrorUnsupport(g_sNeoRuntimeErrors[RTE_CALL_VALUE], pVar1);
        return false;
    }
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
                CallFunctionValue(pVarMeta->_fun_index, n3);
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
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_Set, pVar1, pFunName->_str, n3);
        break;
    case VAR_ASYNC:
        pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_Async, pVar1, pFunName->_str, n3);
        break;
    default:
        // 함수값 호출(n2=-1)은 이름 슬롯이 없다. 멤버 호출만 이름을 읽는다.
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) || OP.n2 != -1)
        {
            pFunName = pFunNamePre ? pFunNamePre : GetVarPtr2(OP);
            if (pFunName->GetType() == VAR_STRING)
            {
                SetErrorFormat(RTE_CALL_NAMED, pFunName->_str->_str.c_str());
                break;
            }
        }
        SetErrorUnsupport(g_sNeoRuntimeErrors[RTE_CALL_VALUE], pVar1);
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
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && -1 == OP.n2) {
            CallFunctionValue(pVar1->_fun_index, OP.n3);
            return false;
        }
        break;
    case VAR_FUN_NATIVE:
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && -1 == OP.n2) {
            Call(pVar1->_funPtr, OP.n3);
            return false;
        }
        break;
    case VAR_CLOSURE:
        if ((OP.argFlag & NEOS_ARG_N2_LOCAL) == 0 && -1 == OP.n2) {
            Call(pVar1->_closure, OP.n3);
            return false;
        }
        break;
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

        if (m_iBreakingCallStack == 0 && !IsNativeScriptCallActive() && IsMainCoroutine(m_pCur) == false)
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
            : m_pVarGlobal_Pointer + callStack._iReturnValueIndex;
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

NEOS_NOINLINE bool handle_TABLE_ALLOC(const SVMOperation& OP) {
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

NEOS_FORCEINLINE bool handle_CLT_MOV(const SVMOperation& OP) {
    CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP), &OP);
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_MOV_L(const SVMOperation& OP) {
    CltInsert(GetVarPtr_L(OP.n1), GetVarPtr_L(OP.n2), GetVarPtr_L(OP.n3), &OP);
    return false;
}

NEOS_NOINLINE bool handle_TABLE_ADD2(const SVMOperation& OP) {
    VarInfo* pTable = GetVarPtrF1(OP);
    if (pTable->GetType() == VAR_ARRAY)
    {
        ArrayModify(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_ADD2);
        return false;
    }
    if (pTable->IsVector())
    {
        if (VecComponentOp(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_ADD2) &&
            (OP.argFlag & NEOS_OP_REF_WRITE)) CommitRefVector(pTable);
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
    if (pTable->IsVector())
    {
        if (VecComponentOp(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_SUB2) &&
            (OP.argFlag & NEOS_OP_REF_WRITE)) CommitRefVector(pTable);
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
    if (pTable->IsVector())
    {
        if (VecComponentOp(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_MUL2) &&
            (OP.argFlag & NEOS_OP_REF_WRITE)) CommitRefVector(pTable);
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
    if (pTable->IsVector())
    {
        if (VecComponentOp(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_DIV2) &&
            (OP.argFlag & NEOS_OP_REF_WRITE)) CommitRefVector(pTable);
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
    if (pTable->IsVector())
    {
        if (VecComponentOp(pTable, GetVarPtr2(OP), GetVarPtr3(OP), NOP_TABLE_PERSENT2) &&
            (OP.argFlag & NEOS_OP_REF_WRITE)) CommitRefVector(pTable);
        return false;
    }
    VarInfo* pVarTemp = GetTableItemValid(pTable, GetVarPtr2(OP));
    if (pVarTemp) Per2(pVarTemp, GetVarPtr3(OP));
    return false;
}

// Keep the atomic vector body out of the dispatch loop. The common list/map
// read below stays inline, just like an ordinary CLT_READ.
NEOS_NOINLINE void WriteStoredVector(VarInfo* target, VarInfo* temp) {
    const SVMOperation write = *GetOP();
    Var_Release(temp); // release old values before deciding whether COW is needed
    {
        temp->SetType(VAR_REF);
        temp->_ref = target;
        struct BorrowScope {
            VarInfo* slot;
            ~BorrowScope() { *slot = VarInfo(); }
        } borrow{temp}; // also clear the borrow if allocation throws during COW
        if (write.op == NOP_CLT_MOV || write.op == NOP_CLT_MOV_L)
            CltInsert(temp->_ref, GetVarPtr2(write), GetVarPtr3(write));
        else
            VecComponentOp(temp->_ref, GetVarPtr2(write), GetVarPtr3(write), write.op);
    }
    if (GetCodeptr() != 0) CltRead(target, GetVarPtr2(write), temp);
}

// Stored vectors consume the validated pair atomically. Other values leave the
// ordinary write to normal dispatch, including after a suspended native getter.
NEOS_NOINLINE void handle_CLT_WRITE_REF(const SVMOperation& OP) {
    VarInfo* temp = GetVarPtr3(OP);
    VarInfo* owner = GetVarPtrF1(OP);
    VarInfo* key = GetVarPtr2(OP);
    VarInfo* target = nullptr;
    if (owner->GetType() == VAR_LIST)
        target = GetTableItem(owner, key);
    else if (owner->GetType() == VAR_MAP) {
        // Match CLT_READ's common key paths without the generic Find dispatch.
        const VAR_TYPE kt = key->GetType();
        target = kt == VAR_STRING ? owner->_tbl->FindString(key->_str) :
            kt == VAR_INT ? owner->_tbl->FindInt(key->_int) : owner->_tbl->Find(key);
    }
    else if (owner->GetType() == VAR_ARRAY && owner->_array->VectorDimension()) {
        // 배열도 리스트와 같이 원소 VarInfo를 직접 빌려 원자적으로 성분을 수정한다.
        if (key->GetType() != VAR_INT) {
            SetError(RTE_ARRAY_INDEX_TYPE);
            return;
        }
        if (!owner->_array->IsValidIndex(key->_int)) {
            SetError(RTE_ARRAY_INDEX_WRITE);
            return;
        }
        target = &owner->_array->VectorValues()[key->_int];
    }

    if (target) {
        if (target->IsVector()) WriteStoredVector(target, temp);
        else if ((OP.argFlag & NEOS_OP_REF_LOADED) == 0) Move(temp, target);
        return;
    }
    if (GetCodeptr() != 0 && (OP.argFlag & NEOS_OP_REF_LOADED) == 0)
        CltRead(owner, key, temp);
}

// Only emitted for a completed indexed assignment. A float temporary is a
// captured vector component; other temporaries still hold their parent value.
// READ_ADD/SUB/MUL/DIV: n3 op= n1[n2]. n1 = 컨테이너, n2 = 키, n3 = 대입 대상.
// 본문은 ReadArith 템플릿이 정의된 NeoVMWorker.inl 에 둔다(다른 번역 단위가 미정의 템플릿을 보지 않게).
NEOS_FORCEINLINE bool handle_READ_ADD(const SVMOperation& OP);
NEOS_FORCEINLINE bool handle_READ_SUB(const SVMOperation& OP);
NEOS_FORCEINLINE bool handle_READ_MUL(const SVMOperation& OP);
NEOS_FORCEINLINE bool handle_READ_DIV(const SVMOperation& OP);

NEOS_NOINLINE void handle_CLT_READ_RESULT(const SVMOperation& OP) {
    VarInfo* value = GetVarPtrF1(OP);
    if (value->GetType() == VAR_FLOAT) Move(GetVarPtr3(OP), value);
    else CltRead(value, GetVarPtr2(OP), GetVarPtr3(OP));
}

NEOS_NOINLINE bool handle_LIST_ALLOC(const SVMOperation& OP) {
    Var_SetList(GetVarPtrF1(OP), GetVM()->ListAlloc(OP.n23));
    return false;
}

NEOS_NOINLINE bool handle_LIST_ALLOC_L(const SVMOperation& OP) {
    Var_SetList(GetVarPtr_L(OP.n1), GetVM()->ListAlloc(OP.n23));
    return false;
}

NEOS_NOINLINE bool handle_VERIFY_TYPE(const SVMOperation& OP) {
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
        _iSP_Vars_Max2 = (std::max)(_iSP_Vars_Max2, _iSP_VarsMax + 3);
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
// 벡터 저장과 타입 검사를 이 핸들러에서 처리하므로 디스패치 루프에 전개되면 그만큼
// 루프가 커진다. 생성은 성분 쓰기·산술만큼 자주 돌지 않으므로 루프 밖에 둔다.
NEOS_NOINLINE bool handle_VEC_MAKE(const SVMOperation& OP) {
    // n2 = 인자 수 = 성분 수. Vector2/3/4/Quaternion 이 모두 이 op 으로 패치되며,
    // 무엇으로 해석할지는(예: wxyz 쿼터니언) 사용자 규약이지 VM 의 타입이 아니다.
    const int n = OP.n2;
    if (n < 2 || n > 4) {
        SetError(RTE_CALL_INVALID);
        return false;
    }
    // 함수 진입에서 인자 영역까지 스택 범위와 해제 상한을 확정한다.
    // 새 벡터를 완성한 뒤 대상에 연결해 인자 오류가 기존 값을 바꾸지 않게 한다.
    VarInfo* args = GetStackFromBase(_iSP_VarsMax + 1);
    float tmp[4] = {};   // n 개만 쓰고 n 개만 복사한다
    for (int i = 0; i < n; ++i) {
        if (!args[i].IsNumber()) {
            SetError(RTE_CALL_INVALID);   // 대상은 아직 건드리지 않았다
            return false;
        }
        tmp[i] = (float)args[i].GetFloatNumber();
    }
    if (OP.argFlag & NEOS_OP_CALL_NORESULT) return false;
    // 2·3성분은 직접 저장하고, 4성분은 단독 소유 저장소를 재사용한다.
    StoreVector(GetVarPtr3(OP), n, tmp[0], tmp[1], tmp[2], tmp[3]);
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

// 실행 빈도가 낮은 opcode 의 핸들러. 디스패치 루프(RunInternal)는 타임아웃×디버그로 네 벌 생성되고 한 벌이
// 50 KB 를 넘어 uop 캐시를 벗어나므로, 드문 핸들러는 본문을 밖으로 빼 핫 루프를 작게 유지한다.
// 선정 근거: 벤치·smoke 의 동적 opcode 계측과 게임 프로젝트 스크립트 101개의 정적 opcode 빈도(둘 다 0.1% 미만인 것만).
NEOS_NOINLINE void handle_LOADK_cold(const SVMOperation& OP) { Move(GetVarPtrF1(OP), m_pVarGlobal_Pointer + OP.n23); }
NEOS_NOINLINE void handle_LOG_NOT_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), !GetVarPtr2(OP)->IsTrue()); }
NEOS_NOINLINE void handle_SUB2_cold(const SVMOperation& OP) { Sub2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_MUL2_cold(const SVMOperation& OP) { Mul2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_DIV2_cold(const SVMOperation& OP) { Div2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_PERSENT2_cold(const SVMOperation& OP) { Per2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_LSHIFT2_cold(const SVMOperation& OP) { LSh2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_RSHIFT2_cold(const SVMOperation& OP) { RSh2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_AND2_cold(const SVMOperation& OP) { And2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_OR2_cold(const SVMOperation& OP) { Or2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_XOR2_cold(const SVMOperation& OP) { Xor2(GetVarPtrF1(OP), GetVarPtr2(OP)); }
NEOS_NOINLINE void handle_LSHIFT3_cold(const SVMOperation& OP) { LSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_RSHIFT3_cold(const SVMOperation& OP) { RSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_AND3_cold(const SVMOperation& OP) { And3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_OR3_cold(const SVMOperation& OP) { Or3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_XOR3_cold(const SVMOperation& OP) { Xor3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_GREAT_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr2(OP), GetVarPtr3(OP))); }
NEOS_NOINLINE void handle_GREAT_EQ_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr2(OP), GetVarPtr3(OP))); }
NEOS_NOINLINE void handle_LESS_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr3(OP), GetVarPtr2(OP), true)); }
NEOS_NOINLINE void handle_LESS_EQ_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr3(OP), GetVarPtr2(OP), true)); }
NEOS_NOINLINE void handle_EQUAL2_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP))); }
NEOS_NOINLINE void handle_NEQUAL_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), !CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP))); }
NEOS_NOINLINE void handle_AND_cold(const SVMOperation& OP) { And(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_OR_cold(const SVMOperation& OP) { Or(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP)); }
NEOS_NOINLINE void handle_LOG_AND_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), GetVarPtr3(OP)->IsTrue() && GetVarPtr2(OP)->IsTrue()); }
NEOS_NOINLINE void handle_LOG_OR_cold(const SVMOperation& OP) { Var_SetBool(GetVarPtrF1(OP), GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()); }
NEOS_NOINLINE void handle_JMP_AND_cold(const SVMOperation& OP) { if (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue()) SetCodeIncPtr(OP.n1); }
NEOS_NOINLINE void handle_JMP_OR_cold(const SVMOperation& OP) { if (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()) SetCodeIncPtr(OP.n1); }
NEOS_NOINLINE void handle_JMP_NAND_cold(const SVMOperation& OP) { if (false == (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue())) SetCodeIncPtr(OP.n1); }
NEOS_NOINLINE void handle_JMP_NOR_cold(const SVMOperation& OP) { if (false == (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue())) SetCodeIncPtr(OP.n1); }
