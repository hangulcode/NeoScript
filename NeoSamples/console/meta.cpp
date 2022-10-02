#include "stdafx.h"
#include "../../NeoSource/Neo.h"

class Vector3 : public SNeoMeta
{
public:
		float _x = 0;
		float _y = 1.0;
		float _z = -1.0;

		void set(float x, float y, float z)
		{
			_x = x;
			_y = y;
			_z = z;
		}

		virtual  bool GetFunction(std::string& fname, SVarWrapper* pVar)
		{
			if (fname == "set")
			{
				//pVar->SetTableFun(CNeoVM::RegisterNative(set)); 가상 함수가 처리가 되나 ???
			}
			return false;
		}
		virtual  bool GetVariable(std::string& fname, SVarWrapper* pVar)
		{
			if (fname == "x")
			{
				pVar->SetFloat(_x);
				return true;
			}
			else if (fname == "y")
			{
				pVar->SetFloat(_y);
				return true;
			}
			else if (fname == "z")
			{
				pVar->SetFloat(_z);
				return true;
			}
			return false;
		}
};

class Transform : public SNeoMeta
{
public:
	Vector3 _pos;

	virtual  bool GetFunction(std::string& fname, SVarWrapper* pVar)
	{
		return false;
	}
	virtual  bool GetVariable(std::string& fname, SVarWrapper* pVar)
	{
		if (fname == "_pos")
		{
			pVar->SetMeta(&_pos);
			return true;
		}
		return false;
	}
};


Transform tr;
static SNeoMeta* _GetObject()
{
	//return &tr;
	return new Transform();
}





int SAMPLE_meta()
{
	void* pFileBuffer = NULL;
	int iFileLen = 0;
	if (false == FileLoad("meta.neo", pFileBuffer, iFileLen))
	{
		printf("file read error");
		return -1;
	}

	std::string err;
	CNeoVM* pVM = CNeoVM::CompileAndLoadVM(pFileBuffer, iFileLen, err, true, true);
	if (pVM != NULL)
	{
		printf("Compile Success. Code : %d bytes !!\n\n", pVM->GetBytesSize());

		NeoHelper::Register(pVM, "GetObject", _GetObject);
			

		DWORD t1 = GetTickCount();
		pVM->CallN("meta");
		DWORD t2 = GetTickCount();

		if (pVM->IsLastErrorMsg())
		{
			printf("Error - VM Call : %s (Elapse:%d)\n", pVM->GetLastErrorMsg(), t2 - t1);
			pVM->ClearLastErrorMsg();
		}
		else
			printf("(Elapse:%d)\n", t2 - t1);

		CNeoVM::ReleaseVM(pVM);
	}
	else
	{
		printf(err.c_str());
	}
	delete[] pFileBuffer;

    return 0;
}

