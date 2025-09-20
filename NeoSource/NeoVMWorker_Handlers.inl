// NeoVMWorker_Handlers.inl

#pragma once

// --- Instruction Handlers ---
// Return 'true' to exit the RunInternal loop, 'false' to continue.

NEOS_FORCEINLINE bool handle_MOV(const SVMOperation& OP) {
    Move(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_MOVI(const SVMOperation& OP) {
    MoveI(GetVarPtrF1(OP), OP.n23);
    return false;
}

NEOS_FORCEINLINE bool handle_MOV_MINUS(const SVMOperation& OP) {
    MoveMinus(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_ADD2(const SVMOperation& OP) {
    Add2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_SUB2(const SVMOperation& OP) {
    Sub2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_MUL2(const SVMOperation& OP) {
    Mul2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_DIV2(const SVMOperation& OP) {
    Div2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_PERSENT2(const SVMOperation& OP) {
    Per2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_LSHIFT2(const SVMOperation& OP) {
    LSh2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_RSHIFT2(const SVMOperation& OP) {
    RSh2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_AND2(const SVMOperation& OP) {
    And2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_OR2(const SVMOperation& OP) {
    Or2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_XOR2(const SVMOperation& OP) {
    Xor2(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_VAR_CLEAR(const SVMOperation& OP) {
    Var_Release(GetVarPtrF1(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_INC(const SVMOperation& OP) {
    Inc(GetVarPtrF1(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_DEC(const SVMOperation& OP) {
    Dec(GetVarPtrF1(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_ADD3(const SVMOperation& OP) {
    Add3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_SUB3(const SVMOperation& OP) {
    Sub3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_MUL3(const SVMOperation& OP) {
    Mul3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_DIV3(const SVMOperation& OP) {
    Div3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_PERSENT3(const SVMOperation& OP) {
    Per3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_LSHIFT3(const SVMOperation& OP) {
    LSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_RSHIFT3(const SVMOperation& OP) {
    RSh3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_AND3(const SVMOperation& OP) {
    And3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_OR3(const SVMOperation& OP) {
    Or3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_XOR3(const SVMOperation& OP) {
    Xor3(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_GREAT(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr2(OP), GetVarPtr3(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_GREAT_EQ(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr2(OP), GetVarPtr3(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_LESS(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), CompareGR(GetVarPtr3(OP), GetVarPtr2(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_LESS_EQ(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), CompareGE(GetVarPtr3(OP), GetVarPtr2(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_EQUAL2(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_NEQUAL(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), !CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)));
    return false;
}

NEOS_FORCEINLINE bool handle_AND(const SVMOperation& OP) {
    And(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_OR(const SVMOperation& OP) {
    Or(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_LOG_AND(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), GetVarPtr3(OP)->IsTrue() && GetVarPtr2(OP)->IsTrue());
    return false;
}

NEOS_FORCEINLINE bool handle_LOG_OR(const SVMOperation& OP) {
    Var_SetBool(GetVarPtrF1(OP), GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue());
    return false;
}

NEOS_FORCEINLINE bool handle_JMP(const SVMOperation& OP) {
    SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_FALSE(const SVMOperation& OP) {
    if (false == GetVarPtr2(OP)->IsTrue())
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_TRUE(const SVMOperation& OP) {
    if (true == GetVarPtr2(OP)->IsTrue())
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_GREAT(const SVMOperation& OP) {
    if (true == CompareGR(GetVarPtr2(OP), GetVarPtr3(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_GREAT_EQ(const SVMOperation& OP) {
    if (true == CompareGE(GetVarPtr2(OP), GetVarPtr3(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_LESS(const SVMOperation& OP) {
    if (true == CompareGR(GetVarPtr3(OP), GetVarPtr2(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_LESS_EQ(const SVMOperation& OP) {
    if (true == CompareGE(GetVarPtr3(OP), GetVarPtr2(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_EQUAL2(const SVMOperation& OP) {
    if (true == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_NEQUAL(const SVMOperation& OP) {
    if (false == CompareEQ(GetVarPtr2(OP), GetVarPtr3(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_AND(const SVMOperation& OP) {
    if (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue())
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_OR(const SVMOperation& OP) {
    if (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue())
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_NAND(const SVMOperation& OP) {
    if (false == (GetVarPtr2(OP)->IsTrue() && GetVarPtr3(OP)->IsTrue()))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_NOR(const SVMOperation& OP) {
    if (false == (GetVarPtr2(OP)->IsTrue() || GetVarPtr3(OP)->IsTrue()))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_FOR(const SVMOperation& OP) {
    if (For(GetVarPtr_L(OP.n2)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_JMP_FOREACH(const SVMOperation& OP) {
    if (ForEach(GetVarPtr2(OP), GetVarPtr3(OP)))
        SetCodeIncPtr(OP.n1);
    return false;
}

NEOS_FORCEINLINE bool handle_STR_ADD(const SVMOperation& OP) {
    Var_SetStringA(GetVarPtrF1(OP), ToString(GetVarPtr2(OP)) + ToString(GetVarPtr3(OP)));
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

NEOS_FORCEINLINE bool handle_SLEEP(const SVMOperation& OP) {
    int r = Sleep(m_iTimeout, GetVarPtr2(OP));
    if (r == 0) {
        _preClock = clock();
        return true; // Exit RunInternal
    }
    else if (r == 2) {
        m_op_process = 0;
    }
    return false; // Continue RunInternal
}

NEOS_FORCEINLINE bool handle_FMOV1(const SVMOperation& OP) {
    Var_SetFun(GetVarPtrF1(OP), OP.n2);
    return false;
}

NEOS_FORCEINLINE bool handle_FMOV2(const SVMOperation& OP) {
    _funA3._fun_index = OP.n3;
    CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), &_funA3);
    return false;
}

NEOS_FORCEINLINE bool handle_CALL(const SVMOperation& OP) {
    Call(OP.n1, OP.n2, (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_PTRCALL(const SVMOperation& OP) {
    VarInfo* pVar1 = GetVarPtrF1(OP);
    short n3 = OP.n3;
    VarInfo* pFunName = nullptr;
    switch (pVar1->GetType())
    {
    case VAR_FUN:
        if ((OP.argFlag & 0x02) == 0 && -1 == OP.n2)
            Call(pVar1->_fun_index, OP.n3);
        break;
    case VAR_FUN_NATIVE:
        if ((OP.argFlag & 0x02) == 0 && -1 == OP.n2)
            Call(pVar1->_funPtr, OP.n3);
        break;
    case VAR_CHAR:
        Var_SetString(pVar1, pVar1->_c.c); // char -> string
    case VAR_STRING:
        pFunName = GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_String, pVar1, pFunName->_str, n3);
        break;
    case VAR_MAP:
    {
        pFunName = GetVarPtr2(OP);
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
        }
        FunctionPtrNative fun = pVar1->_tbl->_fun;
        if (fun._func)
        {
            CallNative(fun, pVar1->_tbl->_pUserData, pFunName->_str, n3);
            break;
        }
        CallNative(GetVM()->_funLib_Map, pVar1, pFunName->_str, n3);
        break;
    }
    case VAR_LIST:
        pFunName = GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_List, pVar1, pFunName->_str, n3);
        break;
    case VAR_SET:
        break;
    case VAR_ASYNC:
        pFunName = GetVarPtr2(OP);
        CallNative(GetVM()->_funLib_Async, pVar1, pFunName->_str, n3);
        break;
    default:
        SetError("Ptr Call Error");
        break;
    }
    return false;
}

NEOS_FORCEINLINE bool handle_PTRCALL2(const SVMOperation& OP) {
    short n2 = OP.n2;
    VarInfo* pFunName = GetVarPtrF1(OP);
    if (pFunName->GetType() == VAR_CHAR)
    {
        GetVM()->Var_SetStringA(pFunName, pFunName->_c.c);
    }
    if (pFunName->GetType() == VAR_STRING)
    {
        CallNative(GetVM()->_funLib_Default, NULL, pFunName->_str, n2, (OP.argFlag & NEOS_OP_CALL_NORESULT) ? nullptr : GetVarPtr3(OP));
    }

    if (_iSP_Vars_Max2 < _iSP_VarsMax + (1 + n2))
        _iSP_Vars_Max2 = _iSP_VarsMax + (1 + n2);
    return false;
}

NEOS_FORCEINLINE bool handle_RETURN(const SVMOperation& OP) {
    if (iBreakingCallStack == (int)m_pCallStack->size())
    {
        if ((OP.argFlag & NEOS_OP_CALL_NORESULT))
            Var_Release(m_pVarStack_Pointer); // Clear
        else
            Move(m_pVarStack_Pointer, GetVarPtrF1(OP));

        if (iBreakingCallStack == 0 && IsMainCoroutine(m_pCur) == false)
        {
            if (StopCoroutine(true) == true) // Other Coroutine Active (No Stop)
                return false; // break, continue loop
        }
        _iSP_VarsMax = _iSP_Vars;
        return true; // exit RunInternal
    }

    int iTemp = (int)m_pCallStack->size() - 1;
    SCallStack& callStack = (*m_pCallStack)[iTemp];
    m_pCallStack->resize(iTemp);

    if (callStack._pReturnValue)
    {
        if ((OP.argFlag & NEOS_OP_CALL_NORESULT))
            Var_Release(callStack._pReturnValue); // Clear
        else
            Move(callStack._pReturnValue, GetVarPtrF1(OP));
    }
    else
    {
        if ((OP.argFlag & NEOS_OP_CALL_NORESULT))
            Var_Release(m_pVarStack_Pointer); // Clear
        else
            Move(m_pVarStack_Pointer, GetVarPtrF1(OP));
    }

    SetCodePtr(callStack._iReturnOffset);
    _iSP_Vars = callStack._iSP_Vars;
    SetStackPointer(_iSP_Vars);
    _iSP_VarsMax = callStack._iSP_VarsMax;
    return false; // break, continue loop
}

NEOS_FORCEINLINE bool handle_TABLE_ALLOC(const SVMOperation& OP) {
    Var_SetTable(GetVarPtrF1(OP), GetVM()->TableAlloc(OP.n23));
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_READ(const SVMOperation& OP) {
    CltRead(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_TABLE_REMOVE(const SVMOperation& OP) {
    TableRemove(GetVarPtrF1(OP), GetVarPtr2(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_CLT_MOV(const SVMOperation& OP) {
    CltInsert(GetVarPtrF1(OP), GetVarPtr2(OP), GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_TABLE_ADD2(const SVMOperation& OP) {
    VarInfo* pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
    if (pVarTemp) Add2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_TABLE_SUB2(const SVMOperation& OP) {
    VarInfo* pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
    if (pVarTemp) Sub2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_TABLE_MUL2(const SVMOperation& OP) {
    VarInfo* pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
    if (pVarTemp) Mul2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_TABLE_DIV2(const SVMOperation& OP) {
    VarInfo* pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
    if (pVarTemp) Div2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_TABLE_PERSENT2(const SVMOperation& OP) {
    VarInfo* pVarTemp = GetTableItemValid(GetVarPtrF1(OP), GetVarPtr2(OP));
    if (pVarTemp) Per2(pVarTemp, GetVarPtr3(OP));
    return false;
}

NEOS_FORCEINLINE bool handle_LIST_ALLOC(const SVMOperation& OP) {
    Var_SetList(GetVarPtrF1(OP), GetVM()->ListAlloc(OP.n23));
    return false;
}

NEOS_FORCEINLINE bool handle_LIST_REMOVE(const SVMOperation& OP) {
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

NEOS_FORCEINLINE bool handle_YIELD(const SVMOperation& OP) {
    if (StopCoroutine(false) == false)
        return true; // Exit RunInternal
    return false; // Continue RunInternal
}

NEOS_FORCEINLINE bool handle_IDLE(const SVMOperation& OP) {
    SetCodeIncPtr(OP.n23);
    while (true)
    {
        AsyncInfo* p = GetVM()->Pop_AsyncInfo();
        if (p == nullptr) break;
        Var_SetBool(GetStackFromBase(_iSP_VarsMax + 1), p->_success);
        Var_SetStringA(GetStackFromBase(_iSP_VarsMax + 2), p->_resultValue);
        Call(p->_fun_index, 2);
        GetVM()->Var_Release(&p->_LockReferance);
    }
    return false;
}

NEOS_FORCEINLINE bool handle_NONE(const SVMOperation& OP) {
    // No operation
    return false;
}

NEOS_FORCEINLINE bool handle_ERROR(const SVMOperation& OP) {
    int idx = _isErrorOPIndex;
    _isErrorOPIndex = 0;
    m_pCallStack->clear();
    _iSP_Vars = 0;
    SetStackPointer(_iSP_Vars);
    bool blDebugInfo = IsDebugInfo();
    int _lineseq = -1;
    if (blDebugInfo)
        _lineseq = GetDebugLine(idx);

    char chMsg[256];
#ifdef _WIN32
    sprintf_s(chMsg, _countof(chMsg), "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#else
    sprintf(chMsg, "%s : IP(%d), Line(%d)", GetVM()->_pErrorMsg.c_str(), idx, _lineseq);
#endif
    if (GetVM()->_sErrorMsgDetail.empty())
        GetVM()->_sErrorMsgDetail = chMsg;

    if (INeoVM::m_pFunError) {
        INeoVM::m_pFunError(chMsg);
    }
    return true; // Exit RunInternal
}

NEOS_FORCEINLINE bool handle_UNKNOWN(const SVMOperation& OP) {
    SetError("Unknown OP");
    return true; // Exit RunInternal
}