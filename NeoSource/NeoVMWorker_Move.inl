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
