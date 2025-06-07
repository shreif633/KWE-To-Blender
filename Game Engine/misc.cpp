#include "StdAfx.h"
#include ".\misc.h"
#include "Utility.h"
#include "china\china.h"
#include "Control.h"
#include "XSystem.h"

#define PROGRAM_SEED 0x12348921

void	CheckCRC(DWORD dwTime)
{
#ifdef _SDW_DEBUG
	return;
#endif 

	dwTime += GetTickCount();
	char path[MAX_PATH];

    // 모듈 파일 ( = 실행파일 ) 을 로딩한다.
	GetModuleFileName(0, path, sizeof(path));
	HANDLE hFile = CreateFile(path, GENERIC_READ,
							FILE_SHARE_READ, 0, OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL, 0);
    
	if (hFile != INVALID_HANDLE_VALUE) 
    {
		char buf[4096];

		DWORD nCRC = PROGRAM_SEED;
		for ( ; ; ) {
			DWORD dwRead;
			if (!ReadFile(hFile, buf, sizeof(buf), &dwRead, 0))
				break;
			if (dwRead == 0)
				break;
			nCRC = UpdateCRC(nCRC, buf, dwRead);
		}
		CloseHandle(hFile);
		DWORD nTimestamp = (DWORD) GetPrivateProfileInt("CLIENTVERSION", "TIMESTAMP", 0, "./system.ini");
		DecodeCRC(PROGRAM_SEED, &nTimestamp, 4);

        // 시스템 정보 파일의 스템프와 현재 exe 파일의 계산된 crc를 비교하여 변조 여부를 체크한다.
        // 런처에서 시스템 파일에 crc를 써준다.
		if (nCRC != nTimestamp) 
        {
            // 중국 버전이거나 본섭 버전일 때는 종료하도록 한다.
            if (Control::m_nCodePage == Control::CP_TRADITIONAL_CHINESE || GetPrivateProfileInt("CLIENTVERSION", "VERSION", 0, "./system.ini") > 1)
            	XSystem::m_dwFlags |= XSystem::CORRUPT;
		}
	}
	dwTime -= GetTickCount();
	if ((int) dwTime > 0)
		Sleep(dwTime);
}
	
std::string GB2Big(LPCSTR str)
{
	int len = strlen(str);
	char *new_str = (char *) _alloca(len + 1);
	GB2Big(new_str, str, -1);
	return new_str;
}

std::string Big2GB(LPCSTR str)
{
	int len = strlen(str);
	char *new_str = (char *) _alloca(len + 1);
	Big2GB(new_str, str, -1);
	return new_str;
}