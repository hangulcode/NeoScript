#pragma once

struct TableData;

struct TableInfo
{
	std::map<std::string, TableData>	_strMap;
	int	_TableID;
	int _refCount;
//	SNeoMeta*	_meta;
	void* _pUserData;
};

