#pragma once

namespace NeoScript
{

bool Write(CArchiveRdWC& arText, CNArchive& ar, SFunctions& funs, SVars& vars);
bool WriteLog(CArchiveRdWC& arText, CNArchive& ar, SFunctions& funs, SVars& vars);

};