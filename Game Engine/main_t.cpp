#include "stdafx.h"
#include "HShield.h"
#include <Windows.h>
#include <commctrl.h>
#include <basetsd.h>
#include <math.h>
#include <stdio.h>
#include <D3DX9.h>
#include <tchar.h>
#include <shellapi.h>
#include <Tlhelp32.h>
#include "ijl.h"

#include "main.h"

#include "roam.h"

#include "Model.h"
#include "Resource.h"
#include "XObjectDB.h"
#include "XMacro.h"
#include "KGameSys.h"
#include "KGameSysex.h"
#include "Control.h"
#include "CControlEX.h"
#include "KControl.h"

#include "GameData/GameObject.h"
#include "GameData/GameUser.h"
#include "GameData/GameMonster.h"
#include "GameData/GameNpc.h"
#include "GameData/GameItem.h"
#include "GameData/GameEffect.h"

#include "Effect/shader.h"
#include "Effect/Effect_Particle/Effect_Particle.h"
#include "Effect/Effect_Explosion/Effect_Explosion.h"
#include "Effect/Effect_Sprite/Effect_Sprite.h"
#include "Effect/Effect_Fx/Effect_Fx.h"
#include "Effect/Effect_Projectile/Effect_Projectile.h"

#include "EffectEditor.h"
#include "MapEditor.h"

#include "SunLight.h"

#ifdef _SOCKET_TEST
#include "KSocket.h"
#include "../Common/Common.h"
#endif

#include "XFont.h"
//#include "XPointSystem.h"
#include "XSound.h"
#include "MMap.h"
#include "LEngine.h"
#include "XScript.h"

#ifdef DEF_SEED_DIOB_20070417 //#include "SharedMemoryLib.h"
#include "SharedMemoryLib.h"
#endif //DEF_SEED_DIOB_20070417

#ifdef DEF_COSMO_DEVELOPER_VER
// #test1112
#include "_tool/KAL_IDE_Win32/KAL_IDE_Win32.h"
#endif

#include "security/MD5Checksum.h"

using namespace	Control;
//using namespace	KGameSys;
#define MAX_TRIANGLE_NODES 4*8192

#ifdef _MAP_TEST
char g_szTestMapName[ MAX_PATH];
int g_nTestMapX;
int g_nTestMapY;
#endif

float g_fMipMapLodBias = 0.0f;

#ifdef	_DEBUG
BOOL  m_bDrawText = TRUE;
#else
BOOL  m_bDrawText = FALSE;
#endif

#define CANNON_ZOOM 120.0f

#define CONFIG_PK_MD5 "4d54841d92612c8eda94c6f1ec5dbe62"

CMyD3DApplication* g_pMyD3DApp = NULL;

/*
#if defined(_SOCKET_TEST) 
	#define DEF_HSHIELD_KENSHIN_2006_02_01
#else
	#undef DEF_HSHIELD_KENSHIN_2006_02_01
#endif
*/

#ifdef DEF_HSHIELD_KENSHIN_2006_02_01 //핵쉴드 설치

BOOL g_Check_Hack = 0;
HWND g_MainHWND = NULL;
DWORD g_dwMainThreadID;
//TCHAR szTitle[MAX_LOADSTRING];								// The title bar text

int __stdcall AhnHS_Callback(long lCode, long lParamSize, void* pParam);

#endif

//-----------------------------------------------------------------------------
// External function-prototypes
//-----------------------------------------------------------------------------
extern HRESULT GetDXVersion( DWORD* pdwDirectXVersion, TCHAR* strDirectXVersion, int cchDirectXVersion );

//-----------------------------------------------------------------------------
// Name: class CMyD3DApplication
// Desc: Application class. The base class (CD3DApplication) provides the 
//       generic functionality needed in all Direct3D samples. CMyD3DApplication 
//       adds functionality specific to this sample program.
//-----------------------------------------------------------------------------

void ComboBoxAdd( HWND hWnd, int id, LPCTSTR pstrDesc )
{
    HWND hwndCtrl = GetDlgItem( hWnd, id );
    ComboBox_AddString( hwndCtrl, pstrDesc );
}
void ComboBoxSelectIndex( HWND hWnd, int id, int index )
{
    HWND hwndCtrl = GetDlgItem( hWnd, id );
    ComboBox_SetCurSel( hwndCtrl, index );
    PostMessage( hWnd, WM_COMMAND, MAKEWPARAM( id, CBN_SELCHANGE ), (LPARAM)hwndCtrl );
}
int ComboBoxCurSel( HWND hWnd, int id )
{
    HWND hwndCtrl = GetDlgItem( hWnd, id );
    return ComboBox_GetCurSel( hwndCtrl );
}

typedef struct 
{
	int width;
	int height;
} RESOLUTION;

RESOLUTION g_vResolutionCandidates[] = { { 800, 600}, { 1024, 768}, { 1280, 960}, { 1280, 1024} };

BOOL CALLBACK DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch( uMsg)
	{
		case WM_INITDIALOG:
			{
				KGameSys::ReadClientCountryCode();
				int nCountry = KGameSys::GetClientCountryCode();
				int nACP=949;

				if( nCountry == 0)
					nACP = GetACP();
				else if( nCountry == 1)
					nACP = 949;
				else if( nCountry == 2)
					nACP = 1252;
				else if( nCountry == 3)
					nACP = 936;
				else 
					nACP = 949;

				Control::m_nCodePage = nACP;
				Control::m_nClientCP = Control::m_nCodePage;
				if (Control::m_nCodePage == Control::CP_SIMPLIFIED_CHINESE && GetACP() == Control::CP_TRADITIONAL_CHINESE) {
					Control::m_bHongKong = TRUE;
					Control::m_nClientCP = Control::CP_TRADITIONAL_CHINESE;
				}

				SetDlgItemText( hwndDlg, IDC_VISUAL_GROUP, KMsg::Get_EX( 90));
				SetDlgItemText( hwndDlg, IDC_LABEL_TEXTURE_LOD, KMsg::Get_EX( 91));
				SetDlgItemText( hwndDlg, IDC_LABEL_WATER_EFFECT, KMsg::Get_EX( 98));
				SetDlgItemText( hwndDlg, IDC_LABEL_GRASS_RENDERING, KMsg::Get_EX( 97));
				SetDlgItemText( hwndDlg, IDC_LABEL_SHADOW, KMsg::Get_EX( 103));
				SetDlgItemText( hwndDlg, IDC_LABEL_RESOLUTION, KMsg::Get_EX( 106));
				SetDlgItemText( hwndDlg, IDC_LABEL_TERRAIN_LOD, KMsg::Get_EX( 124));
				SetDlgItemText( hwndDlg, IDC_LABEL_GAMMA, KMsg::Get_EX( 153));
				SetDlgItemText( hwndDlg, IDC_LABEL_CHARFOV, KMsg::Get_EX( 155));

				ComboBoxAdd( hwndDlg, IDC_COMBO_TEXTURE_LOD, KMsg::Get_EX( 92));
				ComboBoxAdd( hwndDlg, IDC_COMBO_TEXTURE_LOD, KMsg::Get_EX( 93));
				ComboBoxAdd( hwndDlg, IDC_COMBO_TEXTURE_LOD, KMsg::Get_EX( 94));
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_TEXTURE_LOD, KSetting::op.m_nTextureLOD);

				ComboBoxAdd( hwndDlg, IDC_COMBO_TERRAIN_LOD, KMsg::Get_EX( 92));
				ComboBoxAdd( hwndDlg, IDC_COMBO_TERRAIN_LOD, KMsg::Get_EX( 93));
				ComboBoxAdd( hwndDlg, IDC_COMBO_TERRAIN_LOD, KMsg::Get_EX( 94));
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_TERRAIN_LOD, KSetting::op.m_nTerrain);

				ComboBoxAdd( hwndDlg, IDC_COMBO_CHARFOV, KMsg::Get_EX( 865));
				ComboBoxAdd( hwndDlg, IDC_COMBO_CHARFOV, KMsg::Get_EX( 92));
				ComboBoxAdd( hwndDlg, IDC_COMBO_CHARFOV, KMsg::Get_EX( 93));
				ComboBoxAdd( hwndDlg, IDC_COMBO_CHARFOV, KMsg::Get_EX( 94));
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_CHARFOV, KSetting::op.m_nCharFOV);

				ComboBoxAdd( hwndDlg, IDC_COMBO_GAMMA, "1");
				ComboBoxAdd( hwndDlg, IDC_COMBO_GAMMA, "2");
				ComboBoxAdd( hwndDlg, IDC_COMBO_GAMMA, "3");
				ComboBoxAdd( hwndDlg, IDC_COMBO_GAMMA, "4");
				ComboBoxAdd( hwndDlg, IDC_COMBO_GAMMA, "5");
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_GAMMA, KSetting::op.m_nGamma);

				ComboBoxAdd( hwndDlg, IDC_COMBO_WATER_EFFECT, KMsg::Get_EX( 95));
				ComboBoxAdd( hwndDlg, IDC_COMBO_WATER_EFFECT, KMsg::Get_EX( 96));
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_WATER_EFFECT, KSetting::op.m_nWaterEffect);

				ComboBoxAdd( hwndDlg, IDC_COMBO_GRASS_RENDERING, KMsg::Get_EX( 95));
				ComboBoxAdd( hwndDlg, IDC_COMBO_GRASS_RENDERING, KMsg::Get_EX( 96));
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_GRASS_RENDERING, KSetting::op.m_nGrassRendering);

				ComboBoxAdd( hwndDlg, IDC_COMBO_SHADOW, KMsg::Get_EX( 92));
				ComboBoxAdd( hwndDlg, IDC_COMBO_SHADOW, KMsg::Get_EX( 93));
				ComboBoxAdd( hwndDlg, IDC_COMBO_SHADOW, KMsg::Get_EX( 94));
				ComboBoxAdd( hwndDlg, IDC_COMBO_SHADOW, KMsg::Get_EX( 96));
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_SHADOW, KSetting::op.m_nShadow);

				// 해상도
				int nCandidates = sizeof( g_vResolutionCandidates) / sizeof( RESOLUTION);
				int nSelected = -1;
				for( int i = 0 ; i < nCandidates; ++i) {
					std::strstream s;
					RESOLUTION& rResolution = g_vResolutionCandidates[ i];
					s << rResolution.width << "*" << rResolution.height << std::ends;
					ComboBoxAdd( hwndDlg, IDC_COMBO_RESOLUTION, s.str());

					if( rResolution.width == KSetting::op.m_nResolutionWidth &&
						rResolution.height == KSetting::op.m_nResolutionHeight)
						nSelected = i;
				}
				ComboBoxSelectIndex( hwndDlg, IDC_COMBO_RESOLUTION, nSelected);

				// 확인 ,취소
				SetDlgItemText( hwndDlg, IDOK, KMsg::Get_EX(99));
				SetDlgItemText( hwndDlg, IDCANCEL, KMsg::Get_EX( 100));
			}
			break;

		case WM_COMMAND:
			{
				switch( LOWORD( wParam))
				{
					case IDOK:
						{
							KSetting::op.m_nTextureLOD = ComboBoxCurSel( hwndDlg, IDC_COMBO_TEXTURE_LOD);
							KSetting::op.m_nWaterEffect = ComboBoxCurSel( hwndDlg, IDC_COMBO_WATER_EFFECT);
							KSetting::op.m_nGrassRendering = ComboBoxCurSel( hwndDlg, IDC_COMBO_GRASS_RENDERING);
							KSetting::op.m_nShadow = ComboBoxCurSel( hwndDlg, IDC_COMBO_SHADOW);
							KSetting::op.m_nTerrain = ComboBoxCurSel( hwndDlg, IDC_COMBO_TERRAIN_LOD);
							KSetting::op.m_nCharFOV = ComboBoxCurSel( hwndDlg, IDC_COMBO_CHARFOV);
							KSetting::op.m_nGamma = ComboBoxCurSel( hwndDlg, IDC_COMBO_GAMMA);

							int nSelResolution = ComboBoxCurSel( hwndDlg, IDC_COMBO_RESOLUTION);
							if( nSelResolution != -1) {
								int nCandidates = sizeof( g_vResolutionCandidates) / sizeof( RESOLUTION);
								if( nSelResolution >= 0 && nSelResolution < nCandidates) {
									KSetting::op.m_nResolutionWidth = g_vResolutionCandidates[ nSelResolution].width;
									KSetting::op.m_nResolutionHeight = g_vResolutionCandidates[ nSelResolution].height;
								}
							}

							KSetting::Save();

							EndDialog( hwndDlg, TRUE);
						}
						break;

					case IDCANCEL:
						EndDialog( hwndDlg, FALSE);
						break;
				}
			}
			break;

		default:
			return (FALSE);
	}
	
	return (TRUE);
}


BOOL CALLBACK DialogProc_ConfigError( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch( uMsg)
	{
		case WM_INITDIALOG:
			{				
				int nCountry = KGameSys::GetClientCountryCode();
				int nACP=949;

				if( nCountry == 0)
					nACP = GetACP();
				else if( nCountry == 1)
					nACP = 949;
				else if( nCountry == 2)
					nACP = 1252;
				else if( nCountry == 3)
					nACP = 936;
				else 
					nACP = 949;

				Control::m_nCodePage = nACP;
				Control::m_nClientCP = Control::m_nCodePage;
				if (Control::m_nCodePage == Control::CP_SIMPLIFIED_CHINESE && GetACP() == Control::CP_TRADITIONAL_CHINESE) {
					Control::m_bHongKong = TRUE;
					Control::m_nClientCP = Control::CP_TRADITIONAL_CHINESE;
				}

				SetDlgItemText( hwndDlg, IDC_CONFIGERR_TITLE, KMsg::Get_EX( 1082));
				SetDlgItemText( hwndDlg, IDC_MOVEWEBPAGE, KMsg::Get_EX( 1083));
				SetDlgItemText( hwndDlg, IDCANCEL, KMsg::Get_EX( 101));

				if( nCountry != 1 && nCountry != 2)
					EnableWindow( GetDlgItem( hwndDlg,IDC_MOVEWEBPAGE), FALSE);
			}
			break;

		case WM_COMMAND:
			{
				switch( LOWORD( wParam))
				{
					case ID_MOVEWEBPAGE:
						{
							int nCountry = KGameSys::GetClientCountryCode();
							if( nCountry == 1)
								ShellExecute( NULL, "open", "http://www.netmarble.net/cp_site/kal/Pds/download.asp?btn=1", NULL, NULL, SW_SHOWDEFAULT);
							else
								ShellExecute( NULL, "open", "http://www.kalonline.com/Download/Client.asp?btn=1", NULL, NULL, SW_SHOWDEFAULT);

							EndDialog( hwndDlg, FALSE);
						} break;
					case IDCANCEL:
						EndDialog( hwndDlg, FALSE);
						break;
				}
			}
			break;

		default:
			return (FALSE);
	}
	
	return (TRUE);
}


static char *GetToken(char *str)
{
	str += strcspn(str, " ");
	if (*str) 
		*str++ = 0;
	str += strspn(str, " ");
	return str;
}

static BOOL IsProcessAlive(const char *exename)
{
	PROCESSENTRY32 pe32;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);

	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hSnapshot, &pe32))
	{
		do 
		{
			if (!strcmp(pe32.szExeFile, exename))
			{
				CloseHandle(hSnapshot);
				return true;
			}
		}
		while (Process32Next(hSnapshot, &pe32));
	}
	CloseHandle(hSnapshot);	
	return false;
}

#ifdef DEF_SEED_DIOB_20070417 //void DoClient(SeedValue* sSeed)
extern SeedValue g_Seed;

void DoClient(SeedValue* sSeed)
{
	DWORD data;
    int i;

	OpenSharedMemory();

//	printf("Reading data from the shared memory area...\n");
	
	ReadFromSharedMemory(&data, 0);
	for (i=1; i< 5; i++) 
	{
		ReadFromSharedMemory( &(sSeed->nSeed[i-1]), i);
	}

	WriteOnSharedMemory(0, 0);

//	printf("%d, %d,%d,%d,%d\n", data, sSeed->nSeed[0], sSeed->nSeed[1], sSeed->nSeed[2], sSeed->nSeed[3]);

//	printf("The reading is done.\n");
	DestroySharedMemoryArea ();
//	printf("The End.");
}
#endif //DEF_SEED_DIOB_20070417

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE prehInst, LPSTR lpCmdLine, INT nCmdShow)
{

#ifdef DEF_COSMO_DEVELOPER_VER
    // #test1112
    // win32 객체 생성
    CKal_IDE kalide;
    // win32 객체 생성 인자 생성
    CKal_IDE::custom_WND wndInfo;

    // win32 객체 생성 인자 값 입력
    wndInfo.m_hInstance     = hInst;
    wndInfo.m_hPrevInstance = prehInst;
    wndInfo.m_szCmdLine     = lpCmdLine;
    wndInfo.m_iCmdShow      = nCmdShow;

    int nResult = 0;

    if ((nResult = kalide.createWindows(&wndInfo)) == -1)
    {
        MessageBox(NULL, "error!", NULL, 0);      
    }

#endif

#ifdef DEF_SEED_DIOB_20070417 //DoClient(&g_Seed);
	DoClient(&g_Seed);
#endif //DEF_SEED_DIOB_20070417
#ifdef _DEBUG
	// turn on leak detection
	int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	tmpFlag |= _CRTDBG_LEAK_CHECK_DF;
	_CrtSetDbgFlag(tmpFlag);
#endif
	ASSERT( sizeof( g_szOs) == MAX_PATH);
	ASSERT( sizeof( g_szGpu) == MAX_PATH);
	ASSERT( sizeof( g_szFmt) == MAX_PATH);

	InitCommonControls();

#ifdef DEF_HACKCRCCHECKSUM_KENSHIN_2006_06_09					// 해킹이 시도되는 모든 변수에 대해 CRC값을 만들어 조작할수 없도록 한다.
	KGameSys::gen_crc_table();
	int nTestCount = 0;
	KGameSys::m_nRunSpeed = KGameSys::Get_crcValue(15^0xffff, &nTestCount, TRUE);

	nTestCount = 0;
	KGameSys::m_nMoveRange = KGameSys::Get_crcValue(64^0xffff, &nTestCount, TRUE);

/*
	unsigned int nCheckCrc = 0;
	unsigned int nReturnVal = 0;
	nCheckCrc = KGameSys::Check_crcValue(KGameSys::m_nMoveRange, &nReturnVal, nTestCount, TRUE);
*/
#endif


	OSVERSIONINFO	osvi;
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	if (GetVersionEx(&osvi)) {
		_sntprintf( g_szOs, MAX_PATH, "OS : Major %d, Minor %d, Name ", osvi.dwMajorVersion, osvi.dwMinorVersion);
		switch( osvi.dwMajorVersion << 16 | osvi.dwMinorVersion) {
			case ( 4 << 16 | 0):
				{
					if( osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
						_tcscat( g_szOs, _T( "Windows NT 4.0"));
					else
						_tcscat( g_szOs, _T( "Windows 95"));
				}
				break;
			case ( 4 << 16 | 10):
				_tcscat( g_szOs, _T( "Windows 98"));
				break;
			case ( 4 << 16 | 90):
				_tcscat( g_szOs, _T( "Windows Me"));
				break;
			case ( 3 << 16 | 51):
				_tcscat( g_szOs, _T( "Windows NT 3.51"));
				break;
			case ( 5 << 16 | 0):
				_tcscat( g_szOs, _T( "Windows  2000"));
				break;
			case ( 5 << 16 | 1):
				_tcscat( g_szOs, _T( "Windows XP"));
				break;
			case ( 5 << 16 | 2):
				_tcscat( g_szOs, _T( "Windows Server 2003 family"));
				break;
			default:
				_tcscat( g_szOs, _T( "Unknown System"));
		}
		
		_tcscat( g_szOs, _T( " "));
		_tcscat( g_szOs, osvi.szCSDVersion);
		_tcscat( g_szOs, _T( "\r\n"));
	}

	CIOException::Open("kalonline.co.kr", "kaldebug@kalonline.co.kr", "kaldebug@kalonline.co.kr\0");

	std::string strArg( lpCmdLine);
	for( unsigned int i = 0; i < strArg.size(); ++i)
		strArg[ i] = tolower( strArg[i]);
	if( strstr( lpCmdLine, "english")){
		KGameSys::m_szLocale = "e";
	} else {
		switch( Control::CodePageToLanguage(KGameSys::m_nACP)){
			case Control::KOREAN:
				KGameSys::m_szLocale = "k";
				break;
			case Control::SIMPLIFIED_CHINESE:
			case Control::TRADITIONAL_CHINESE:
				KGameSys::m_szLocale = "c";
				break;
			default:
				KGameSys::m_szLocale = "e";
		}
	}

	std::string s = "message-";
	s += KGameSys::m_szLocale;

	if( KGameSys::ChkFile_Country( s.c_str()))
	{
		// "message-k" 로딩
		// 1.sys		2.itemname		3.itemdesc		4.prefixname
		// 5.npcname	6.monstername	7.shinsuname	8.revivalname 순서
		XObjectDB::Load( s.c_str());
	}
	else
	{
		
		DialogBox( hInst, MAKEINTRESOURCE( IDD_DIALOG_CONFIGERR), NULL, DialogProc_ConfigError);
		return 0;
	}	
	KGameSystemChk::InitSystemKey();


#ifdef DEF_HSHIELD_KENSHIN_2006_02_01 //핵쉴드 설치
	//EhSvc.dll 파일의 Full File Name을 얻어옵니다.
	
	g_dwMainThreadID = GetCurrentThreadId();
//	LoadString(hInst, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);

	TCHAR szFullFileName[MAX_PATH];
	GetCurrentDirectory(MAX_PATH,  
	  szFullFileName
	);
	lstrcat(szFullFileName, _T("\\HShield\\EhSvc.dll"));

	//초기화 함수 호출 
	int nRet;
	nRet = _AhnHS_Initialize(szFullFileName, AhnHS_Callback, 

	#ifdef DEF_HSHIELDCODE_ENG_KENSHIN_2006_0306	//핵쉴드 영문 인증번호
		4102, "460D7272BB0E59B7",					//4102, "460D7272BB0E59B7" (영문 본서버) // 4101, "CAE3E471662B95A4" (국내 본서버)
	#else
		4101, "CAE3E471662B95A4",
	#endif

	#if !defined(_DEBUG)
		#ifdef DEF_HACKSHIELD_DEBUGVERSION_OFF							// 핵쉴드 사용중 디버깅을 하고 싶다면 이것을 주석 처리할 것!
		AHNHS_CHKOPT_ALL | 
		AHNHS_DISPLAY_HACKSHIELD_LOGO |
		#endif
	#endif
		#ifdef DEF_HACKSHIELD_DEBUGVERSION_OFF							// 핵쉴드 사용중 디버깅을 하고 싶다면 이것을 주석 처리할 것!
		AHNHS_USE_LOG_FILE | AHNHS_ALLOW_SVCHOST_OPENPROCESS |
		AHNHS_ALLOW_LSASS_OPENPROCESS | AHNHS_ALLOW_CSRSS_OPENPROCESS |AHNHS_DONOT_TERMINATE_PROCESS,
		AHNHS_SPEEDHACK_SENSING_RATIO_NORMAL
		#else
		AHNHS_USE_LOG_FILE ,
//    | AHNHS_ALLOW_SVCHOST_OPENPROCESS |
//		AHNHS_ALLOW_LSASS_OPENPROCESS | AHNHS_ALLOW_CSRSS_OPENPROCESS |AHNHS_DONOT_TERMINATE_PROCESS,
		AHNHS_SPEEDHACK_SENSING_RATIO_NORMAL
		#endif
		);

	//아래 에러는 개발과정에서만 발생할 수 있으면 
	//이후 결코 발생해서는 안되는 에러이므로 assert처리를 했습니다.
	assert(nRet != HS_ERR_INVALID_PARAM);
	assert(nRet != HS_ERR_INVALID_LICENSE);
	assert(nRet != HS_ERR_ALREADY_INITIALIZED);
	
	#ifdef DEF_HACKSHIELD_DEBUGVERSION_OFF							// 핵쉴드 사용중 디버깅을 하고 싶다면 이것을 주석 처리할 것!
	if (nRet != HS_ERR_OK) 
	{
		//Error 처리 
		switch(nRet)
		{
			case HS_ERR_ANOTHER_SERVICE_RUNNING:
			{//MessageBox( NULL, KMsg::Get( 130), KMsg::Get( 113), MB_ICONWARNING);
				//MessageBox(NULL, _T("다른 게임이 실행중입니다.\n프로그램을 종료합니다."), "KalOnline", MB_OK);
				KWindowCollector::CloseAll();
				KWindowCollector::GameExit();
				MessageBox(NULL, KMsg::Get( 1231), "KalOnline", MB_OK);
				break;
			}
			case HS_ERR_INVALID_FILES:
			{
				//MessageBox(NULL, _T("잘못된 파일 설치되었습니다.\n프로그램을 재설치하시기 바랍니다."), "KalOnline", MB_OK);
				KWindowCollector::CloseAll();
				KWindowCollector::GameExit();
				MessageBox(NULL, KMsg::Get( 1232), "KalOnline", MB_OK);
				break;
			}
			case HS_ERR_DEBUGGER_DETECT:
			{
				//MessageBox(NULL, _T("컴퓨터에서 디버거 실행이 감지되었습니다.\n디버거의 실행을 중지시킨 뒤에 다시 실행시켜주시기바랍니다."), "KalOnline", MB_OK);
				KWindowCollector::CloseAll();
				KWindowCollector::GameExit();
				MessageBox(NULL, KMsg::Get( 1233), "KalOnline", MB_OK);
				break;
			}
			case HS_ERR_NEED_ADMIN_RIGHTS:
			{
				//MessageBox(NULL, _T("Admin 권한으로 실행되어야 합니다.\n프로그램을 종료합니다."), "KalOnline", MB_OK);
				KWindowCollector::CloseAll();
				KWindowCollector::GameExit();
				MessageBox(NULL, KMsg::Get( 1234), "KalOnline", MB_OK);
				break;
			}
			case HS_ERR_COMPATIBILITY_MODE_RUNNING:
			{
				//MessageBox(NULL, _T("호환성 모드로 프로그램이 실행중입니다.\n프로그램을 종료합니다."), "KalOnline", MB_OK);
				KWindowCollector::CloseAll();
				KWindowCollector::GameExit();
				MessageBox(NULL, KMsg::Get( 1235), "KalOnline", MB_OK);
				break;				
			}
			default:
			{
				//TCHAR szMsg[255];
				//wsprintf(szMsg, _T("해킹방지 기능에 문제가 발생하였습니다.(Error Code = %x)\n프로그램을 종료합니다."), nRet);
				//MessageBox(NULL, szMsg, "KalOnline", MB_OK);
				KWindowCollector::CloseAll();
				KWindowCollector::GameExit();
				MessageBox(NULL, KMsg::Get( 1236), "KalOnline", MB_OK);
				break;
			}
		}
		return FALSE;
	}
	#endif

	//시작 함수 호출 
	nRet = _AhnHS_StartService();
	#ifdef DEF_HACKSHIELD_DEBUGVERSION_OFF							// 핵쉴드 사용중 디버깅을 하고 싶다면 이것을 주석 처리할 것!
	assert(nRet != HS_ERR_NOT_INITIALIZED);
	assert(nRet != HS_ERR_ALREADY_SERVICE_RUNNING);

	if (nRet != HS_ERR_OK)
	{
		//TCHAR szMsg[255];
		//wsprintf(szMsg, _T("해킹방지 기능에 문제가 발생하였습니다.(Error Code = %x)\n프로그램을 종료합니다."), nRet);
		//MessageBox(NULL, szMsg, "KalOnline", MB_OK);
		KWindowCollector::CloseAll();
		KWindowCollector::GameExit();
		MessageBox(NULL, KMsg::Get( 1236), "KalOnline", MB_OK);
		return FALSE;
	}
	#endif

	#ifdef DEF_HSHIELD_TRADEHACK_DEFENCE_KENSHIN_2006_08_07	// 트레이드핵 방지
	
		#ifdef DEF_HACKSHIELD_DEBUGVERSION_OFF							// 핵쉴드 사용중 디버깅을 하고 싶다면 이것을 주석 처리할 것!
	char szSysDir[MAX_PATH] = {0,}; 
    GetSystemDirectory(szSysDir, MAX_PATH); 
                  
    // 2가지 API를 후킹하고 있지만 한 가지를 선택해 적용 하도록 합니다. 
    int nResultCount = _AhnHS_CheckAPIHooked("ws2_32.dll", "send", szSysDir); //ws2_32.dll파일을 인젝션 시켜 트레이드핵 사용하던것 방지.

	if( nResultCount != ERROR_SUCCESS )
	{
		KWindowCollector::CloseAll();
		KWindowCollector::GameExit();
		MessageBox(NULL, KMsg::Get( 1236), "KalOnline", MB_OK);
		return FALSE;
	}
		#endif

	#endif

#endif


#if !(defined(DEF_MULTY_WINDOW_USE_KENSHIN_2006_03_22))
	#if 1 || !defined( _DEBUG)  // 윈도우창 ( 게임창 )이 여러개 실행 가능한지 판단.
	CreateMutex(NULL, FALSE, GetUniqueName().c_str());
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		MessageBox(NULL, KMsg::Get(154), NULL, MB_ICONINFORMATION);
		return 0;
	}
	#endif
#endif

	KSetting::Load();

	int len = strlen(lpCmdLine)+1;
	char *argv = (char *) _alloca(len);
//	char _argv[128];
//	char *argv = _argv;
	memcpy(argv, lpCmdLine, len);

#ifdef _MAP_TEST
	if( argv == NULL)
	{
		MessageBox( NULL, "단독으로 실행할 수 없습니다.", "경고", MB_OK);
		return 0;
	}
	
	std::string full( argv);
	std::string mapname( full.begin(), full.begin() + full.find( '@'));
	strcpy( g_szTestMapName, mapname.c_str());
	char* loc = &full[ full.find( '@')];
	sscanf( ++loc, "%d%d", &g_nTestMapX, &g_nTestMapY);
#endif

    // 인자값 추출
	argv += strspn(argv, " ");
	char *next = GetToken(argv);

    // 인자가 setup일때
	if(strcmp( argv, "/setup") == 0) {
		DialogBox( hInst, MAKEINTRESOURCE( IDD_DIALOG_SETTING), NULL, DialogProc);
		return 0;
	}

#ifdef	_SOCKET_RELEASE
#ifndef _SDW_DEBUG 



//	Decode(52, ( BYTE*)argv,6);

//	MessageBox( NULL, argv, NULL, MB_OK);

    // load 옵션이 아니거나 인자가 없을때
	if(argv[0] == 0 || strcmp(argv, "/load") != 0)// || !IsProcessAlive("KalOnline.exe"))
	{
		if( GetFileAttributes( "KalOnline0.exe") != 0xffffffff)
		{
#ifdef DEF_LAUNCHER_UPDATE_DIOB_20070423 //막음: 업데이트된 런처로 교체하는 작업
			/*
			Sleep( 1000);
			if( DeleteFile( "KalOnline.exe") )
				MoveFile( "KalOnline0.exe", "KalOnline.exe");

			if( GetFileAttributes( "KalTest0.exe") != 0xffffffff)
			{
				Sleep( 1000);
				if( DeleteFile( "KalTest.exe") )
					MoveFile( "KalTest0.exe", "KalTest.exe");
			}

			MessageBox( NULL, KMsg::Get( 241), KMsg::Get( 113), MB_ICONWARNING);
			return 0;
			*/
#endif //DEF_LAUNCHER_UPDATE_DIOB_20070423
		}
		else
		{
//#test1 런처 무실행
#ifndef DEF_NO_LAUNCH
			MessageBox( NULL, KMsg::Get( 130), KMsg::Get( 113), MB_ICONWARNING);
			return 0;
#endif
		}
	}

//#test1 런처 무실행
#ifndef DEF_NO_LAUNCH
    //실행인자의 다음 토큰을 얻는다..
	next = GetToken(argv = next);
#endif

//	if (GetPrivateProfileInt("CLIENTVERSION", "VERSION", 0, "./system.ini") < 1) {
//		MessageBox( NULL, "this is old version. retry again", KMsg::Get( 113), MB_ICONWARNING);
//		return 0;
//	}
#endif
#endif


#ifdef DEF_LAUNCHER_UPDATE_DIOB_20070423 //막음: 업데이트된 런처로 교체하는 작업
/*
	if( GetFileAttributes( "KalOnline0.exe") != 0xffffffff)
	{
		Sleep( 1000);
		if( DeleteFile( "KalOnline.exe") )
			MoveFile( "KalOnline0.exe", "KalOnline.exe");
		
		MessageBox( NULL, KMsg::Get( 241), KMsg::Get( 113), MB_ICONWARNING);
		return 0;
	}
	if( GetFileAttributes( "KalTest0.exe") != 0xffffffff)
	{
		Sleep( 1000);
		if( DeleteFile( "KalTest.exe") )
			MoveFile( "KalTest0.exe", "KalTest.exe");
	}
*/
#endif //DEF_LAUNCHER_UPDATE_DIOB_20070423

	HRESULT hr;
    DWORD dwDirectXVersion = 0;
    TCHAR strDirectXVersion[10];
	//SetThreadPriorityBoost(GetCurrentThread(), TRUE);
	CoInitialize(0);
    hr = GetDXVersion( &dwDirectXVersion, strDirectXVersion, 10 );
    if( FAILED(hr) || dwDirectXVersion < 0x00090002 )
    {
		MessageBox( NULL, KMsg::Get( 151), NULL, MB_ICONINFORMATION);
		return 0;
    }

#ifdef	_DEBUG
#ifdef DEF_NEWSEVERCOSMO_DIOB_20070618
	XObjectDB::m_strConfig = "cosmo";
#else //DEF_NEWSEVERCOSMO_DIOB_20070618
	XObjectDB::m_strConfig = "debug";
#endif //DEF_NEWSEVERCOSMO_DIOB_20070618
	KGameSys::m_bDebugClient = TRUE;
	#ifdef DEF_NEWSEVER_DIOB_20070618
	KGameSys::m_bCosmoClient = TRUE;
	#endif //DEF_NEWSEVER_DIOB_20070618
#endif

	for ( ; argv[0] ; ) {
		if (strcmp(argv, "/config") == 0) {
			next = GetToken(argv = next);
			XObjectDB::m_strConfig = argv;
			if ( strcmp( XObjectDB::m_strConfig.c_str(), "debug") == 0)
				KGameSys::m_bDebugClient = TRUE;
#ifdef DEF_NEWSEVERCOSMO_DIOB_20070618
			else if ( strcmp( XObjectDB::m_strConfig.c_str(), "cosmo") == 0)
				KGameSys::m_bDebugClient = TRUE;
#endif DEF_NEWSEVERCOSMO_DIOB_20070618
#ifdef DEF_NEWSEVER_DIOB_20070618
			else if ( strcmp( XObjectDB::m_strConfig.c_str(), "cosmo") == 0)
				KGameSys::m_bCosmoClient = TRUE;
#endif DEF_NEWSEVER_DIOB_20070618
		}
		next = GetToken(argv = next);
	}

	//XObjectDB::m_strConfig = "debug";
	//KGameSys::m_bDebugClient = TRUE;

	if( GetPrivateProfileInt( "CLIENTVERSION", "MODE_HYPERTEXT_VIEW", 0, "./system.ini"))
		KGameSys::m_bHyperText_TXT = TRUE;

#ifdef DOWRITELOG
	WRITELOG( "KalOnline Start - LOG VERSION 6\n");
	char	UserName[200];
	DWORD UserNameSize = sizeof(UserName);
	if (!GetUserName(UserName, &UserNameSize))
		lstrcpy(UserName, "Unknown");
	WRITELOG( "RUN BY %s\n", UserName);
	WRITELOG( "OS : %08x\n", CResource::m_dwOSVersion);
#endif

    CMyD3DApplication d3dApp;
	g_pMyD3DApp = &d3dApp;
//	try {
	if( FAILED( d3dApp.Create( hInst ) ) ) {
		WRITELOG( "Application Create FAIL\n");
		KMessageBox::ProcessReservation();
		return 0;
	}

#ifdef DEF_HSHIELD_KENSHIN_2006_02_01 //핵쉴드 설치
	g_MainHWND = d3dApp.m_hWndFocus;

#ifndef DEF_HSHIELD_B2
	//	TCHAR szMsg[MAX_PATH];
	int nResult = _AhnHS_SaveFuncAddress(5, AhnHS_Callback, _AhnHS_Initialize, _AhnHS_StartService, KSocket::OnRecv, KSocket::WritePacket);
	//	wsprintf(szMsg, "_AhnHS_SaveFucnAddress() : %d", nResult);
	//	MessageBox(NULL, szMsg, "OK", MB_OK);
#endif // DEF_HSHIELD_B2

#endif

    // 실질적 메인 게임 시작 부분!!!
    INT nReturnValue = d3dApp.Run();

	WRITELOG( "KalOnline Exit\n");

#ifdef DEF_HSHIELD_KENSHIN_2006_02_01 //핵쉴드 설치
	//서비스 종료 함수 호출 
	_AhnHS_StopService();
	//완료 함수 호출 
	_AhnHS_Uninitialize();
#endif


#ifdef DOWRITELOG
	DeleteFile( "./log.txt");
#endif

	KMessageBox::ProcessReservation();

	return nReturnValue;
//	}
//	catch (MyException e) {
//		MessageBox(NULL,e.GetMessage(),"Demo",MB_OK);
//	}
//	return 0;
}

//-----------------------------------------------------------------------------
// Name: CMyD3DApplication()
// Desc: Application constructor. Sets attributes for the app.
//-----------------------------------------------------------------------------
CMyD3DApplication::CMyD3DApplication()
{
    m_strWindowTitle    = _T("KalOnline");

	ZeroMemory( &m_d3dpp, sizeof( m_d3dpp));

    m_d3dEnumeration.AppUsesDepthBuffer           = TRUE;
    m_d3dEnumeration.AppUsesMixedVP = TRUE;

#if	defined(_DEBUG) || !defined(_SOCKET_TEST)
    m_dwCreationWidth   = 800;
    m_dwCreationHeight  = 600;
	KSetting::op.m_nResolutionWidth = 800;
	KSetting::op.m_nResolutionHeight = 600;
    m_bShowCursorWhenFullscreen = FALSE;
#else
	m_dwCreationWidth   = KSetting::op.m_nResolutionWidth;
    m_dwCreationHeight  = KSetting::op.m_nResolutionHeight;

	if (m_dwCreationWidth < 800 || m_dwCreationHeight < 600 ){
		m_dwCreationWidth = 800;
		m_dwCreationHeight = 600;
	} else if( m_dwCreationWidth > 1600 || m_dwCreationHeight > 1200 ) {
		m_dwCreationWidth = 1600;
		m_dwCreationHeight = 1200;
	}

	m_bShowCursorWhenFullscreen = FALSE;

	//diob 스크린모드: 풀모드(기본값, 0), 창모드(1)
	if( GetPrivateProfileInt( "CLIENTVERSION", "MODE_WINDOW", 0, "./system.ini"))
		m_bStartFullscreen = FALSE;
	else
		m_bStartFullscreen = TRUE;
#endif
    m_pFont           = new CD3DFont( _T("Arial"), 12, D3DFONT_BOLD );
	XSystem::m_pFont = m_pFont;
	m_nRender = 0;

	XEngine::Init();
	YEngine::Init();
	ZEngine::Init();
	LEngine::Init();
	
	m_nMoveFlag = -1;
	m_vPick.x = m_vPick.y = m_vPick.z = 0;
	m_bEarthQuake = FALSE;
	m_bGEarthQuake = FALSE;

	m_pBackBuffer = NULL;
	m_pBackBufferTexture = NULL;
	m_pBlurTexture[0] = NULL;
	m_pBlurTexture[1] = NULL;
	m_pLuminanceTexture = NULL;
	m_pBlurVertexShader = NULL;
	m_pBlurPixelShader = NULL;

	m_bBackBufferTextureUpdated = false;
	
	m_bDialogBoxMode = FALSE;
	m_nCursorPrev = m_nCursor = 0;
#ifdef	_DEBUG
	m_dwStartTick = GetTickCount() + 20000;
#else
	m_dwStartTick = GetTickCount() + 10 * 60000;
#endif
}

CMyD3DApplication::~CMyD3DApplication()
{
	evQuake_t *ev;
	for (std::list<evQuake_t *>::iterator it = m_eventQuakeList.begin(); it != m_eventQuakeList.end(); )
	{
		ev = *it++;
		m_eventQuakeList.remove(ev);
		delete ev;
	}
	m_eventQuakeList.clear();
	FreeModelTable();
	XScript::FreeScript();
	KMacro::FreeMacro();

	earthtime = -1.0f;
}

//-----------------------------------------------------------------------------
// Name: OneTimeSceneInit()
// Desc: Called during initial app startup, this function performs all the
//       permanent initialization.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::OneTimeSceneInit()
{
	WRITELOG( "OneTimeSceneInit ENTER\n");

	srand( (unsigned)time( NULL));
	Secure<int>::m_nKey = rand();

#ifdef DEF_MD5_OEN_071107
    if (CMD5Checksum::GetMD5("data\\config\\config.pk") != std::string(CONFIG_PK_MD5))
    {
        // 로그
        _WRITE_PACKET_LOG("log_md5_test error = %s\n", "config.pk");

        KWindowCollector::GameExit();
    }
#endif

	BOOL bLoad;
#ifdef	_DEBUG
	bLoad = XObjectDB::Load("script");
#endif
	bLoad = XObjectDB::Load("object");
	ASSERT(bLoad);

	// 1.macro			2.monsterinfo	3.monsterbone	4.npcinfo		5.npcbone		6.characterinfo
	// 7.characterbone	8.shinsuinfo	9.store			10.skillteacher	11.skillbuff	12.minimap		13.initmap 순서
	bLoad = XObjectDB::Load("macro");
	ASSERT(bLoad);

	bLoad = XObjectDB::Load("prefix");
	ASSERT(bLoad);

    bLoad = XObjectDB::Load("inititem");
    ASSERT(bLoad);

	bLoad = XObjectDB::Load("event");
	ASSERT(bLoad);
	bLoad = XObjectDB::Load("sound");
	ASSERT(bLoad);

#ifdef DEF_TIPSYSTEMADD_SEA_20061026
// 국가가 추가되어 나라별루 바뀜 sea 20061026
	std::string tipsystem = "tipsystem-";
	tipsystem += KGameSys::m_szLocale;
	if( KGameSys::ChkFile_Country( tipsystem.c_str()))
	{
		bLoad = XObjectDB::Load( tipsystem.c_str());
		ASSERT(bLoad);
	}
	else
	{
		DialogBox( (HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCE( IDD_DIALOG_CONFIGERR), NULL, DialogProc_ConfigError);		
		KWindowCollector::GameExit();
		return S_OK;
	}
#else 
	// 기존 시스템 유지 
	bLoad = XObjectDB::Load("tipsystem");
	ASSERT(bLoad);

#endif // DEF_TIPSYSTEMADD_SEA_20061026

	std::string jobsystem = "jobsystem-";
	jobsystem += KGameSys::m_szLocale;
	if( KGameSys::ChkFile_Country( jobsystem.c_str()))
	{
		bLoad = XObjectDB::Load( jobsystem.c_str());
		ASSERT(bLoad);
	}
	else
	{
		DialogBox( (HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCE( IDD_DIALOG_CONFIGERR), NULL, DialogProc_ConfigError);		
		KWindowCollector::GameExit();
		return S_OK;
	}

    // 서버 정보 입력
	std::string xlate = "xlate-";
	xlate += KGameSys::m_szLocale;
	if( KGameSys::ChkFile_Country( xlate.c_str()))
	{
		bLoad = XObjectDB::Load( xlate.c_str());
		ASSERT(bLoad);
	}
	else
	{
		DialogBox( (HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCE( IDD_DIALOG_CONFIGERR), NULL, DialogProc_ConfigError);
		KWindowCollector::GameExit();
		return S_OK;
	}

	std::string task = "task-";
	task += KGameSys::m_szLocale;
	if( KGameSys::ChkFile_Country( task.c_str()))
	{
		bLoad = XObjectDB::Load( task.c_str());
		ASSERT(bLoad);
	}
	else
	{
		DialogBox( (HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCE( IDD_DIALOG_CONFIGERR), NULL, DialogProc_ConfigError);
		KWindowCollector::GameExit();
		return S_OK;
	}

	//- sea 2006-02-01---------------------------------------------------------------------------------------
	//std::string guidebook = "guidebook-";
	//guidebook += KGameSys::m_szLocale;
	//if( KGameSys::ChkFile_Country( guidebook.c_str()))
	//{
	//	bLoad = XObjectDB::Load( guidebook.c_str());
	//	ASSERT(bLoad);
	//}
	//else
	//{
	//	DialogBox( (HINSTANCE)GetModuleHandle(NULL), MAKEINTRESOURCE( IDD_DIALOG_CONFIGERR), NULL, DialogProc_ConfigError);
	//	KWindowCollector::GameExit();
	//	return S_OK;
	//}

#ifdef DEF_MD5_OEN_071107
    bLoad = XObjectDB::Load("md5");
    ASSERT(bLoad);
#endif


	KDebugingSystem::SetShowMonsterName();

	WRITELOG( "OneTimeSceneInit SUCCESS\n");

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::FrameMove() //diob= 카메라: 루프 돈다.
{
//	if ( !Pub_Activate )
//		return -1;

	XSystem::m_dwTimeDiff = DWORD(m_fElapsedTime * 1000.f);

	ZEngine::FrameMove(XSystem::m_dwTimeDiff);
	
	KSunLight::FrameMove( XSystem::m_dwTimeDiff);

	BOOL bSetView = FALSE;

	if (CGame_Character::GroupFrameMove(m_fElapsedTime))
		bSetView = TRUE;
		
	if (KCamera::FrameMove(m_fElapsedTime))
		bSetView = TRUE;

	CalEarthQuakeTime( m_fElapsedTime );
	ProcessEarthQuakeEvent(m_fElapsedTime);
	
	if (bSetView || (XSystem::m_dwFlags & XSystem::SET_CAMERA) || m_bEarthQuake)
		SetView();
	
	g_fCurrentTime = m_fTime;             // Current time in seconds

	pG_EffectGroup.GroupFrameMove(m_fElapsedTime);
	g_fxEntityManager.FrameMove(m_fElapsedTime);
	g_particleEntityManager.FrameMove(m_fElapsedTime);
	CParticleBundle::FrameMove();
	g_effectEntityManager.FrameMove(m_fElapsedTime);
	CEffect_Explosion::FrameMove(m_fElapsedTime);
	CEffect_Particle::FrameMove(m_fElapsedTime);
	g_screenOverlay.FrameMove(m_fElapsedTime);

	if (!m_nMoveFlag)
		return S_OK;

	float fElapsedTime = (GetKeyState(VK_CONTROL) < 0) ? m_fElapsedTime / 10 : m_fElapsedTime ;

	D3DXVECTOR3 vTran;

	switch (m_nMoveFlag)
	{
		case LEFTTURN:
			XSystem::m_fYaw -= fElapsedTime * D3DX_PI / 5.0f;
			SetView();
			break;
		case RIGHTTURN:
			XSystem::m_fYaw += fElapsedTime * D3DX_PI / 5.0f;
			SetView();
			break;
		case UPTURN:
			XSystem::m_fPitch += fElapsedTime * D3DX_PI / 5.0f;
			if (XSystem::m_fPitch > 1.f)
				XSystem::m_fPitch = 1.f;
			SetView();
			break;
		case DOWNTURN:
			XSystem::m_fPitch -= fElapsedTime * D3DX_PI / 5.0f;
			if (XSystem::m_fPitch < -3.1f / 2.0f)
				XSystem::m_fPitch = -3.1f / 2.0f;
			SetView();
			break;
		case ZOOMIN:
			//pl.y *= expf(fElapsedTime);
			//if (pl.y > 1024.0f)
			//	pl.y = 1024.0f;
			{
				float Speed = expf(fElapsedTime);
				XSystem::m_fZoom -= (Speed);
				if ( XSystem::m_fZoom < 16 )	XSystem::m_fZoom = 16;
			}
			SetView();
			break;
		case ZOOMOUT:
			//pl.y /= expf(fElapsedTime);
			//if (pl.y < 1.0f)
			//	pl.y = 1.0f;
			{
				float Speed = expf(fElapsedTime);
				XSystem::m_fZoom += (Speed);
				if ( XSystem::m_fZoom > 156 )	XSystem::m_fZoom = 156;
			}
			SetView();
			break;
		case FOREWARD:
		case BACKWARD:
		case LEFTWARD:
		case RIGHTWARD:
		case UPWARD:
		case DOWNWARD:
			if( CGame_Character::m_Master){

				D3DXVECTOR3 vFrom, vTo, vPick, vTran;

				if ( m_nMoveFlag == FOREWARD )
					vTran = D3DXVECTOR3(sinf(XSystem::m_fYaw), 0, cosf(XSystem::m_fYaw));
				else if ( m_nMoveFlag == BACKWARD )
					vTran = - D3DXVECTOR3(sinf(XSystem::m_fYaw), 0, cosf(XSystem::m_fYaw));
				else if ( m_nMoveFlag == LEFTWARD )
					vTran = D3DXVECTOR3(-cosf(XSystem::m_fYaw), 0, sinf(XSystem::m_fYaw));
				else if ( m_nMoveFlag == RIGHTWARD )
					vTran = - D3DXVECTOR3(-cosf(XSystem::m_fYaw), 0, sinf(XSystem::m_fYaw));
				else if ( m_nMoveFlag == UPWARD )
					vTran = D3DXVECTOR3(0, 5, 0);
				else if ( m_nMoveFlag == DOWNWARD )
					vTran = D3DXVECTOR3(0, -5, 0);
				else
					break;
#ifndef _SOCKET_TEST
				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * fElapsedTime * 1000;
				vFrom.y += 20;	vTo.y -= 20; vPick.y = 0;
				if ( XEngine::Pick(&vPick, vFrom, vTo) )
					CGame_Character::m_Master->m_vPosi = vPick;
				else
				{
					vFrom.y -= 20;
					CGame_Character::m_Master->m_vPosi = vFrom;
				}

				CGame_Character::m_Master->ModelPosition();
#endif
				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * 10;
				vFrom.y += 10;	vTo.y -= 10; vPick.y = 0;
				if ( XEngine::Pick(&vPick, vFrom, vTo) )
					CGame_Character::m_Master->SetMove(vPick);
				else
				{
					vFrom.y -= 10;
					CGame_Character::m_Master->SetMove(vFrom);
				}
				
			}
			SetView();
			break;
/*
		case FOREWARD:
			//pl += D3DXVECTOR3(sinf(m_fYaw), 0, cosf(m_fYaw)) * fElapsedTime * 100;
			{
				if( !CGame_Character::m_Master)
					break;

				D3DXVECTOR3 vFrom, vTo, vPick;
				vTran = D3DXVECTOR3(sinf(XSystem::m_fYaw), 0, cosf(XSystem::m_fYaw));

				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * fElapsedTime * 400;
				vFrom.y += 20;	vTo.y -= 20; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->m_vPosi = vPick;
				else
				{
					vFrom.y -= 20;
					CGame_Character::m_Master->m_vPosi = vFrom;
				}
				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * 10;
				vFrom.y += 10;	vTo.y -= 10; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->SetMove(vPick);
				else
				{
					vFrom.y -= 10;
					CGame_Character::m_Master->SetMove(vFrom);
				}
			}
			break;
		case BACKWARD:
			//pl -= D3DXVECTOR3(sinf(m_fYaw), 0, cosf(m_fYaw)) * fElapsedTime * 100;
			{
				if( !CGame_Character::m_Master)
					break;

				D3DXVECTOR3 vFrom, vTo, vPick;
				vTran = - D3DXVECTOR3(sinf(XSystem::m_fYaw), 0, cosf(XSystem::m_fYaw));

				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * fElapsedTime * 400;
				vFrom.y += 20;	vTo.y -= 20; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->m_vPosi = vPick;
				else
				{
					vFrom.y -= 20;
					CGame_Character::m_Master->m_vPosi = vFrom;
				}
				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * 10;
				vFrom.y += 10;	vTo.y -= 10; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->SetMove(vPick);
				else
				{
					vFrom.y -= 10;
					CGame_Character::m_Master->SetMove(vFrom);
				}
			}
			break;
		case LEFTWARD:
			//pl += D3DXVECTOR3(-cosf(m_fYaw), 0, sinf(m_fYaw)) * fElapsedTime * 100;
			{
				if( !CGame_Character::m_Master)
					break;

				D3DXVECTOR3 vFrom, vTo, vPick;
				vTran = D3DXVECTOR3(-cosf(XSystem::m_fYaw), 0, sinf(XSystem::m_fYaw));

				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * fElapsedTime * 400;
				vFrom.y += 20;	vTo.y -= 20; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->m_vPosi = vPick;
				else
				{
					vFrom.y -= 20;
					CGame_Character::m_Master->m_vPosi = vFrom;
				}
				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * 10;
				vFrom.y += 10;	vTo.y -= 10; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->SetMove(vPick);
				else
				{
					vFrom.y -= 10;
					CGame_Character::m_Master->SetMove(vFrom);
				}
			}
			break;
		case RIGHTWARD:
			{
				if( !CGame_Character::m_Master)
					break;

				D3DXVECTOR3 vFrom, vTo, vPick;
				vTran = - D3DXVECTOR3(-cosf(XSystem::m_fYaw), 0, sinf(XSystem::m_fYaw));

				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * fElapsedTime * 400;
				vFrom.y += 20;	vTo.y -= 20; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->m_vPosi = vPick;
				else
				{
					vFrom.y -= 20;
					CGame_Character::m_Master->m_vPosi = vFrom;
				}
				vFrom = vTo = CGame_Character::m_Master->m_vPosi + vTran * 10;
				vFrom.y += 10;	vTo.y -= 10; vPick.y = 0;
				XEngine::Pick(&vPick, vFrom, vTo);
				if ( vPick.y != 0 )
					CGame_Character::m_Master->SetMove(vPick);
				else
				{
					vFrom.y -= 10;
					CGame_Character::m_Master->SetMove(vFrom);
				}
			}
			break;
*/
		case VAR_INC:
			Terrain::m_fVariance *= expf(fElapsedTime * 2);
			if (Terrain::m_fVariance > 1.0f)
				Terrain::m_fVariance = 1.0f;
			Terrain::Update();
			break;
		case VAR_DEC:
			Terrain::m_fVariance /= expf(fElapsedTime * 2);
			if (Terrain::m_fVariance < 0.0001f)
				Terrain::m_fVariance = 0.0001f;
			Terrain::Update();
			break;
		default:
			if( CGame_Character::m_Master)
			{
				CGame_Character::m_Master->ModelPosition();
				m_nMoveFlag = 0;
			}
			break;
	}
	
    return S_OK;
}

void CMyD3DApplication::SetView() //diob= 카메라3: 화면 움직일때 카메라 이동
{    
	D3DXVECTOR3 vEye, vAt, vPick;   // 기본 카메라 설정 벡터
    D3DXVECTOR3 vUp( 0, 1.0f, 0);   // 기본 y up 벡터    
	float fYaw, fPitch;             // 요, 피치 변수 선언
	float fNearZ;                   // 클로즈 z값 변수
	float quake;                    // 지진값?

    // 현재 시스템 상태가 셋카메라 상태라고 설정?
	XSystem::m_dwFlags &= ~XSystem::SET_CAMERA;	

    // 카메라의 현재 값 입력이 된다면?
    // 입력이 되는 경우는 게임내의 상태외에 모두이다.(즉 캐릭터 선택화면 등등)
	if( KCamera::SetCamera( vEye, vAt, fYaw, fPitch))
	{
		XSystem::m_fYaw = fYaw;
		XSystem::m_fPitch = fPitch;
		fNearZ = NEAR_Z;
	}
    // 마스터가 없을 때 무효
	else if ( !CGame_Character::m_Master )
	{
		return;
	}
    // 나머지 경우
	else
	{
        // 최고 지진값을 구한다.
		quake = max(m_fEarthQuakeIntensity, m_fEarthQuakeEventIntensity);

        // 현재 지진상태라면 임의의 지진값 파형을 구한다.
		float pitchJitter = (m_bEarthQuake ? D3DXToRadian(RANDOM_FLOAT(-quake, quake) * 0.25f) : 0.f);

        // 요, 피치, 롤 순으로 회전행렬을 구한다.
		D3DXMATRIXA16 matAngle;
		D3DXMatrixRotationYawPitchRoll(&matAngle, XSystem::m_fYaw, -XSystem::m_fPitch + pitchJitter, 0);

//#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06 // 대포 시스템 추가 연습

		CGameTarget* pCannon = NULL;

        // 마스터의 상태가 숨은캐넌상태?
		if(CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON)
		{
            // 캐넌id가 존재한다면?
			if(CGame_Character::m_nCannonID != -1)
			{
                // 캐넌을 찾아서
				pCannon = CGame_Character::FindCharacter(CGame_Character::m_nCannonID);

                // 캐넌의 위치에 보는 방향을 고정?
				vAt = pCannon->m_vPosi;
				vAt.y += CANNON_HEIGHT;
			}
			else
			{
                // 캐넌이 없다면 위치에 보는 방향 설정
				vAt = CGame_Character::m_Master->m_vPosi;
				vAt.y += 15.7f;
			}
		}
        // 마스터의 상태가 숨은캐넌상태가 아니라면?
		else
		{
			vAt = CGame_Character::m_Master->m_vPosi;   // 보는 방향 설정
			vAt.y += 15.7f;                             // 보정
		}
//#else
//		vAt = CGame_Character::m_Master->m_vPosi;
//		vAt.y += 15.7f;
//#endif
        
		float fLength = XSystem::m_fZoom;       // 줌상태 입력
		D3DXVECTOR3 velocity;                   // 이동량 
		XEngine::m_nAction = XEngine::CAMERA;   // x엔진의 현재 상태 설정

        //////////////////////////////////////////////////////////////////////////
        // 맵이 로드되어 있지 않다면 즉 월드맵일때
		if (!MMap::IsLoaded()) {

            // 행렬에 줌을 적용
			velocity = *(D3DXVECTOR3 *) &matAngle._31 * -fLength;

            // 줌 적용 후 배경과 충돌 체크
			if (Terrain::CollidePoint(vAt, velocity)) {
                // 충돌 체크 된 줌 설정
				fLength *= Terrain::T;
                // 행렬에 줌 새로 적용
				velocity = *(D3DXVECTOR3 *) &matAngle._31 * -fLength;
			}

            // 카메라 이동이 물 아래라면
			if (vAt.y + velocity.y < WATER_POS_Y) {
                // 카메라 위치 수정
				if (vAt.y > WATER_POS_Y || velocity.y > 0) 
					fLength *= (WATER_POS_Y - vAt.y) / velocity.y;
				else
					fLength = 0;
                // 변화량 제 수정
				velocity = *(D3DXVECTOR3 *) &matAngle._31 * -fLength;
			}


			vEye = vAt + velocity;
			vEye.y += 8;	

			// zaxis = normal(at - eye)
			// xaxis = normal(cross(up, zaxis))
			// yaxis = cross(zaxis, xaxis)
            // 보는 방향 각도 설정
			*(D3DXVECTOR3 *) &matAngle._31 = vAt - vEye;
            // 보는 길이 설정
			fLength = D3DXVec3Length((D3DXVECTOR3 *) &matAngle._31);
            // 정규화
			D3DXVec3Normalize((D3DXVECTOR3 *) &matAngle._31, (D3DXVECTOR3 *) &matAngle._31);
            // 값을 변환?
			matAngle._11 = matAngle._33;
			matAngle._12 = 0;
			matAngle._13 = -matAngle._31; 
            // 정규화
			D3DXVec3Normalize((D3DXVECTOR3 *) &matAngle._11, (D3DXVECTOR3 *) &matAngle._11);
            // 3행렬과 1행렬을 외적해서 2행렬에 대입
			D3DXVec3Cross((D3DXVECTOR3 *) &matAngle._21, (D3DXVECTOR3 *) &matAngle._31, (D3DXVECTOR3 *) &matAngle._11);
            // 최대값 산출?
			maximize(fLength, NEAR_Z + 4);
		}

        // 줌 x, y 값 계산
		D3DXVECTOR3 a0 = *(D3DXVECTOR3 *) &matAngle._11 * (XSystem::m_vNear.x * NEAR_Z / XSystem::m_vNear.z);
		D3DXVECTOR3 a1 = *(D3DXVECTOR3 *) &matAngle._21 * (XSystem::m_vNear.y * NEAR_Z / XSystem::m_vNear.z);
        // 변화량 산출
		velocity = *(D3DXVECTOR3 *) &matAngle._31 * (NEAR_Z - fLength);
		fNearZ = NEAR_Z;

        // 카메라와 배경모델들 충돌 체크를 위한 임시 행렬 설정
		D3DXMATRIX mat = matAngle;
		*(D3DXVECTOR3 *)&mat._41 = vAt;

#ifndef	_SOCKET_TEST
		if (!(XSystem::m_dwFlags & XSystem::EDIT_MODE))
		{
#endif
        //////////////////////////////////////////////////////////////////////////
        // 카메라와 물체간의 충돌을 체크하여 줌인아웃
		// jufoot: CollideRect 가 오작동하는 case 가 있는거 같아서 CollideBox 로 대체했음
		XEngine::m_nAction = XEngine::CAMERA;
		if (XEngine::CollideBox(mat, D3DXVECTOR3(5,5,5), velocity)) {  
			float fOldLength = fLength;
			fLength = (fLength - NEAR_Z) * XCollision::T + NEAR_Z;
			/*if (fLength < NEAR_Z + 4) {
				fLength = fOldLength;
				a0 = *(D3DXVECTOR3 *) &matAngle._11 * 3.5f;
				a1 = *(D3DXVECTOR3 *) &matAngle._21 * (XSystem::m_vNear.y * 3.5f / XSystem::m_vNear.x);
				fNearZ = XSystem::m_vNear.z * 3.5f / XSystem::m_vNear.x;
				fLength -= fNearZ;
				velocity = *(D3DXVECTOR3 *) &matAngle._31 * -fLength;
				XEngine::CollideRect(vAt, a0, a1, velocity);
				fLength *= XCollision::T;
				fLength += fNearZ;
			}*/
		}		
#ifndef	_SOCKET_TEST		
		}
#endif
        // 눈의 위치를 최종적으로 계산
		vEye = vAt - *(D3DXVECTOR3 *) &matAngle._31 * fLength;
#if 0
		if (MMap::IsLoaded()) {
			D3DXVECTOR3 a0 = *(D3DXVECTOR3 *) &matAngle._11 * XSystem::m_vNear.x;
			D3DXVECTOR3 a1 = *(D3DXVECTOR3 *) &matAngle._21 * XSystem::m_vNear.y;
			D3DXVECTOR3 velocity = *(D3DXVECTOR3 *) &matAngle._31 * (XSystem::m_vNear.z - XSystem::m_fZoom);
			XEngine::m_nAction = XEngine::CAMERA;
			XEngine::CollideRect(vAt, a0, a1, velocity);
			velocity *= XCollision::T;
			vEye = vAt - *(D3DXVECTOR3 *)&matAngle._31 * 
				((XSystem::m_fZoom - XSystem::m_vNear.z) * XCollision::T + XSystem::m_vNear.z);
			fNearZ = NEAR_Z;
		}
		else {
			D3DXVECTOR3	vRange(0, 0, -XSystem::m_fZoom);
			D3DXVECTOR3 vResult;
			D3DXVec3TransformNormal(&vResult, &vRange, &matAngle);
			vEye = vAt;
			vEye += vResult;
			float fLength;
			if (XEngine::Pick(&vPick, vAt, vEye, XEngine::CAMERA))
			{
				D3DXVECTOR3 vTemp = vPick-vAt;
				fLength =  D3DXVec3Length(&vTemp);
				if ( fLength < 12 )
				{
					D3DXVECTOR3 vDiff = vEye - vAt;
					vEye = vAt + vDiff * (12 / D3DXVec3Length(&vDiff));
					fLength = 12;
				}
				else {
					vEye = vPick;
				}
			}
			else
			{
				D3DXVECTOR3 vTemp = vEye-vAt;
				fLength =  D3DXVec3Length(&vTemp);
			}
			if ( fLength < 24 )
			{
				vEye.y += (24-fLength) / 4 ;
				vAt.y += (24-fLength ) / 2;
			}
			vEye.y += 8;
			D3DXVECTOR3 vDiff = (vEye - vAt);
			fNearZ = D3DXVec3Length(&vDiff) * 0.33f;
			if (fNearZ > NEAR_Z)
				fNearZ = NEAR_Z;
		}
#endif

	}

    // 현재 계산 값 입력
	m_vAt = vAt;
	m_vUp = vUp;
	m_vEye = vEye;

	// Projection
	if (fNearZ != XSystem::m_vNear.z) {
		// Set the projection matrix
		D3DXMATRIXA16 matProj;
        // 화면 비율 계산
		FLOAT fAspect = ((FLOAT)m_d3dsdBackBuffer.Width) / m_d3dsdBackBuffer.Height;
        // 투영 계산
		D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, fAspect, fNearZ, (XSystem::m_dwFlags & XSystem::EDIT_MODE) ? EDIT_FAR_Z : FAR_Z );

        // 투영
		if( m_pd3dDevice)
			m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

        // 내부 투영 입력
		//set presprctive specs to our user frustum
		XSystem::SetProjection(matProj, D3DX_PI/4, fAspect, fNearZ, (XSystem::m_dwFlags & XSystem::EDIT_MODE) ? EDIT_FAR_Z : FAR_Z );

	}
	
	D3DXMATRIXA16 matView;

    // 지진이 있다면
	if (m_bEarthQuake) // 지진
	{
        // 파형을 계산하여 입력
		D3DXVECTOR3 vJitter = D3DXVECTOR3(RANDOM_FLOAT(-quake, quake), RANDOM_FLOAT(-quake, quake), RANDOM_FLOAT(-quake, quake));
		vEye = vEye + vJitter;
		vAt = vAt + vJitter;
	}

    // 뷰행렬 계산
	D3DXMatrixLookAtLH((D3DXMATRIX*) &matView, (D3DXVECTOR3*)&vEye,
		(D3DXVECTOR3*)&vAt,(D3DXVECTOR3*) &vUp );

    // 뷰행렬 적용
	//set the view matrix
	if( m_pd3dDevice)
		m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // 현재 뷰행렬 시스템에 입력
	// if dynamic roam then set the current transform to our frustum class
	// and update the tesselation
	XSystem::SetViewTransform((D3DXMATRIX)matView);
	XSystem::m_dwFlags |= XSystem::SET_TERRAIN;
    // 테셀레이트?(맵 분할?)
	Terrain::Tesselate();

	//XSystem::m_fYaw = m_fYaw;
	//XSystem::m_fPitch = m_fPitch;	
}

float g_waterHeight = WATER_POS_Y;


// 가장먼저 랜더링 되는 부분, 예를들어 지형 관련 부분
void CMyD3DApplication::RenderView()
{
	BOOL bWater = !KCommand::m_bHideWater && Terrain::m_bWater;

	if (g_waterHeight != WATER_POS_Y)
		bWater = TRUE;

	if (bWater)
	{
		if (m_nRender & 1)
		{
			if (m_Water.BeginRenderTarget(RT_REFLECTION)) // 물반사텍스쳐 그리기
			{
				BeginWaterView(WV_REFLECTION);

				if (!MMap::IsLoaded())
					m_SkyBox.Draw(TRUE);

				m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

				if (!MMap::IsLoaded())
					Terrain::Render(TRUE);

				D3DXPLANE WorldFarPlane = XSystem::m_vPlaneWorld[XSystem::P_FAR];
				D3DXPLANE DecoWorldFarPlane = XSystem::m_vPlaneView[XSystem::P_FAR];
				DecoWorldFarPlane.d = METER * 300;
				D3DXMATRIXA16 mTranspose;
				D3DXMatrixTranspose(&mTranspose, &XSystem::m_mView);
				D3DXPlaneTransform(&XSystem::m_vPlaneWorld[XSystem::P_FAR], &DecoWorldFarPlane, &mTranspose);

				XEngine::RenderWater();		

				XSystem::m_vPlaneWorld[XSystem::P_FAR] = WorldFarPlane;

				EndWaterView();
				m_Water.EndRenderTarget();
			}
		}
	}

	if (m_nRender & 15)
		goto skip;

    // 지형이 로딩안되 있다면 로딩시키도록
	if (!MMap::IsLoaded() && !KCommand::m_bHideTerrain)
	{
		if (XSystem::m_dwFlags & XSystem::SET_TERRAIN) {
			Terrain::Tesselate();
			XSystem::m_dwFlags &= ~XSystem::SET_TERRAIN;
		}
	}

	if (XSystem::m_dwFlags & XSystem::SET_CENTER) {
		YEngine::EnterAndLeave();
		XSystem::m_dwFlags &= ~XSystem::SET_CENTER;
	}

skip:
	m_nRender++;

#if	defined(_DEBUG) || !defined(_SOCKET_TEST) 
	if (XSystem::m_nWireFrame == 1)
		m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, ~XSystem::m_fogColor, 1.0f, 0L );
	else
#endif
		m_pd3dDevice->Clear(0L, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, XSystem::m_fogColor, 1.0f, 0L );

	XSystem::m_nFrameNumber++;
    
    // Set render mode to lit, solid triangles
	m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	m_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

	if (!KCommand::m_bHideTerrain && !MMap::IsLoaded())
		m_SkyBox.Draw(FALSE);

	if (!MMap::IsLoaded() && !KCommand::m_bHideTerrain)
	{
		Terrain::Render(FALSE);
	}		

#ifdef _DEBUG
	//if( CGame_Character::m_Master) {
	//	D3DXVECTOR3 v = ToClientV( CGame_Character::m_Master->m_vServerPt);
	//	CResource::DrawAxis( &v);
	//}
#endif

	if (!KCommand::m_bHideModel)
	{
		g_waterHeight = WATER_POS_Y;
		XEngine::Render();
	}

#ifndef _SOCKET_TEST
	if (XSystem::m_dwFlags & XSystem::EDIT_MODE)
		XEngine::RenderSvMap();
#endif

    // 데칼 처리
	if (CGame_Character::m_Master && CGame_Character::m_Master->m_MoveGubun)
		XSystem::DrawDecal(CGame_Character::m_Master->m_vMove, 8, XSystem::m_pTexture_Decal->m_pTexture);


//#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06  //대포 시스템
	if(CGame_Character::m_Master && CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON)
	{
        if(KGameSys::m_bMoveLockCannon)
		{
            XSystem::DrawDecal(CGame_Character::m_Master->m_vMove, 32.0f, XSystem::m_pTexture_Decal->m_pTexture);
		}
		else
		{
#ifdef DEF_CANNONFLOORTEXTURE_KENSHIN_2006_02_09	//대포상태에서의 택스쳐
			XSystem::DrawDecal(CGame_Character::m_Master->m_vMove, 32.0f, XSystem::m_pTexture_Decal_Cannon->m_pTexture);
#endif
		}
		if(KGameSys::m_bMoveLockCannon)
		{

			if(KGameSys::m_byCannonRtating == 2 && KGameSys::m_bCannonContinue == 2)  // 패킷 완성되면 KGameSys::m_byCannonRtating == 2 ->> 이렇게 해야 한다.
			{
				KHyperView* pView = (KHyperView*) KWindowCollector::Find( "cannonready");
				if( pView == NULL)
				{
					KGameSys::m_bCannonContinue = 1;
					KGameSys::m_bCannonReady = TRUE;
					KWindowCollector::Load( "cannonready");
				}
			}
		}
	}
//#endif

//#ifdef DEF_CANNON_ANGLE_KENSHIN_2006_01_24  //대포 시스템
	if(CGame_Character::m_Master && (CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON) && CGame_Character::m_nCannonID != -1)
	{
		if(KGameSys::m_bCannonRangeCheck)
		{
			CGameTarget* pCannon = NULL;
			KGameSys::P3CVERTEX STemp[2];

			pCannon = CGame_Character::FindCharacter(CGame_Character::m_nCannonID);
			
			for( int i = 0 ; i < 2 ; i++ )
			{
				if( i == 0 )
				{			
					STemp[0].vp = pCannon->m_vPosi;
					STemp[1].vp = KGameSys::m_LineVert[0].vp;
				}
				else if( i == 1)
				{
					STemp[0].vp = pCannon->m_vPosi;
					STemp[1].vp = KGameSys::m_LineVert[1].vp;
				}

				STemp[0].vc = 0xffffff00;
				STemp[1].vc = 0xffffff00;

				XSystem::DrawColorLine(STemp[0], STemp[1]);				
			}
		}
	}
//#endif

#ifdef DEF_SCENARIO3_BOMB
	KGameSys::DrawRangeBomb();
#endif	//DEF_SCENARIO3_BOMB

    // 지형처리와 같이 범위 스킬 캐스트가 진행되는듯!!!
	KGameSys::DrawRangeSkillCast();

	if (!KCommand::m_bHideTerrain && XSystem::m_pShadow)
	{
		XSystem::RenderShadow();
	}

	if (!MMap::IsLoaded() && !KCommand::m_bHideTerrain && bWater)
		m_Water.Draw();//WATER_POS_Y,CELL_SIZE * 512);

	if (!MMap::IsLoaded() && !KCommand::m_bHideTerrain && KSetting::op.m_nGrassRendering == 0) 
		Terrain::RenderDecoLayers();

    // 다양한 이펙트 처리
	pG_EffectGroup.GroupRender();

	// HACK!!
	g_particleEntityManager.FrameMove(0);

	g_fxEntityManager.Render();
	g_particleEntityManager.Render(false);
	g_effectEntityManager.Render();

	CEffect_Explosion::Render();
	CEffect_Particle::Render();
}

static int GetHighPowerOfTwo(int len)
{
	int pot = 1;
	while (pot < len)
		pot <<= 1;
	return pot;
}

void CMyD3DApplication::InitPostProcess()
{
	HRESULT hr;

	if (KSetting::op.m_nEffect == 0 || KSetting::op.m_nPostProcess == 0)
	{
		D3DDISPLAYMODE mode;
		XSystem::m_pDevice->GetDisplayMode(0, &mode);

		if (XSystem::m_caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL)
		{
			m_bbtWidth = m_d3dpp.BackBufferWidth;
			m_bbtHeight = m_d3dpp.BackBufferHeight;
		}
		else
		{
			m_bbtWidth = GetHighPowerOfTwo(m_d3dpp.BackBufferWidth) >> 1;
			m_bbtHeight = GetHighPowerOfTwo(m_d3dpp.BackBufferHeight) >> 1;
		}

		if (XSystem::m_caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY)
		{
			int size = max(m_bbtWidth, m_bbtHeight);
			m_bbtWidth  = size;
			m_bbtHeight = size;
		}

		if (KSetting::op.m_nEffect == 0) // '상' 이펙트 옵션
		{
			D3DXCreateTexture(XSystem::m_pDevice, m_bbtWidth, m_bbtHeight, 1, 
				D3DUSAGE_RENDERTARGET, mode.Format, D3DPOOL_DEFAULT, &m_pBackBufferTexture);
		}

		if (KSetting::op.m_nPostProcess == 0) // '뽀샤시' 옵션
		{
			if (XSystem::m_caps.VertexShaderVersion >= (DWORD)D3DVS_VERSION(1, 1) && XSystem::m_caps.PixelShaderVersion >= (DWORD)D3DPS_VERSION(1, 1))
			{
				LoadAndCreateVertexShader("blur", &m_pBlurVertexShader);
				LoadAndCreatePixelShader("blur", &m_pBlurPixelShader);

				if (XSystem::m_caps.Caps2 & D3DCAPS2_CANAUTOGENMIPMAP)
				{
					int size = GetHighPowerOfTwo(max(m_bbtWidth >> 2, m_bbtHeight >> 2)) >> 1;

					hr = D3DXCreateTexture(XSystem::m_pDevice, size, size, 1,
						D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, mode.Format, D3DPOOL_DEFAULT, &m_pLuminanceTexture);
					ASSERT(hr == D3D_OK);
				}
			}

			hr = D3DXCreateTexture(XSystem::m_pDevice, m_bbtWidth >> 2, m_bbtHeight >> 2, 1,
				D3DUSAGE_RENDERTARGET, mode.Format, D3DPOOL_DEFAULT, &m_pBlurTexture[0]);
			ASSERT(hr == D3D_OK);

			hr = D3DXCreateTexture(XSystem::m_pDevice, m_bbtWidth >> 2, m_bbtHeight >> 2, 1,
				D3DUSAGE_RENDERTARGET, mode.Format, D3DPOOL_DEFAULT, &m_pBlurTexture[1]);		
			ASSERT(hr == D3D_OK);
		}
	}
}

void CMyD3DApplication::ShutdownPostProcess()
{
	SAFE_RELEASE(m_pBlurVertexShader);
	SAFE_RELEASE(m_pBlurPixelShader);

	SAFE_RELEASE(m_pLuminanceTexture);
	SAFE_RELEASE(m_pBlurTexture[0]);
	SAFE_RELEASE(m_pBlurTexture[1]);

	SAFE_RELEASE(m_pBackBufferTexture);
}

void CMyD3DApplication::RestartPostProcess()
{
	ShutdownPostProcess();
	InitPostProcess();
}

static void BlurTexture(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, int dstWidth, int dstHeight)
{
	static struct { D3DXVECTOR4 xyzw; D3DXVECTOR2 uv; } v[4] = {
		{ D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f), D3DXVECTOR2(0, 1) },
		{ D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f), D3DXVECTOR2(0, 0) },
		{ D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f), D3DXVECTOR2(1, 1) },
		{ D3DXVECTOR4(0.0f, 0.0f, 0.0f, 1.0f), D3DXVECTOR2(1, 0) }
	};

	LPDIRECT3DSURFACE9 pOriginalSurface, pSurface;

	XSystem::m_pDevice->GetRenderTarget(0, &pOriginalSurface);
	
	pDstTexture->GetSurfaceLevel(0, &pSurface);
	XSystem::m_pDevice->SetRenderTarget(0, pSurface);
	//XSystem::m_pDevice->BeginScene();
	XSystem::m_pDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0);

	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
	XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(64, 64, 64, 64));

	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);	

	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	XSystem::m_pDevice->SetTexture(0, pSrcTexture);

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	
	v[0].xyzw.x = -0.5f + 0.6f;
	v[0].xyzw.y = dstHeight - 0.5f;
	v[1].xyzw.x = -0.5f + 0.6f;
	v[1].xyzw.y = -0.5f;
	v[2].xyzw.x = dstWidth - 0.5f + 0.6f;
	v[2].xyzw.y = dstHeight - 0.5f;
	v[3].xyzw.x = dstWidth - 0.5f + 0.6f;
	v[3].xyzw.y = -0.5f;
	XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));

	v[0].xyzw.x = -0.5f - 0.6f;
	v[0].xyzw.y = dstHeight - 0.5f;
	v[1].xyzw.x = -0.5f - 0.6f;
	v[1].xyzw.y = -0.5f;
	v[2].xyzw.x = dstWidth - 0.5f - 0.6f;
	v[2].xyzw.y = dstHeight - 0.5f;
	v[3].xyzw.x = dstWidth - 0.5f - 0.6f;
	v[3].xyzw.y = -0.5f;
	XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));

	v[0].xyzw.x = -0.5f;
	v[0].xyzw.y = dstHeight - 0.5f + 0.6f;
	v[1].xyzw.x = -0.5f;
	v[1].xyzw.y = -0.5f + 0.6f;
	v[2].xyzw.x = dstWidth - 0.5f;
	v[2].xyzw.y = dstHeight - 0.5f + 0.6f;
	v[3].xyzw.x = dstWidth - 0.5f;
	v[3].xyzw.y = -0.5f + 0.6f;
	XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));

	v[0].xyzw.x = -0.5f;
	v[0].xyzw.y = dstHeight - 0.5f - 0.6f;
	v[1].xyzw.x = -0.5f;
	v[1].xyzw.y = -0.5f - 0.6f;
	v[2].xyzw.x = dstWidth - 0.5f;
	v[2].xyzw.y = dstHeight - 0.5f - 0.6f;
	v[3].xyzw.x = dstWidth - 0.5f;
	v[3].xyzw.y = -0.5f - 0.6f;
	XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
	
	//XSystem::m_pDevice->EndScene();
	XSystem::m_pDevice->SetRenderTarget(0, pOriginalSurface);

	SAFE_RELEASE(pSurface);
	SAFE_RELEASE(pOriginalSurface);
	
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

static void BlurTextureWithShader(LPDIRECT3DTEXTURE9 pSrcTexture, LPDIRECT3DTEXTURE9 pDstTexture, int width, int height, 
								  LPDIRECT3DVERTEXSHADER9 pVertexShader, LPDIRECT3DPIXELSHADER9 pPixelShader)
{
	static struct { D3DXVECTOR3 xyz; D3DXVECTOR2 uv; } v[4] = {
		{ D3DXVECTOR3(0.0f, 0.0f, 0.0f), D3DXVECTOR2(0, 1) },
		{ D3DXVECTOR3(0.0f, 0.0f, 0.0f), D3DXVECTOR2(0, 0) },
		{ D3DXVECTOR3(0.0f, 0.0f, 0.0f), D3DXVECTOR2(1, 1) },
		{ D3DXVECTOR3(0.0f, 0.0f, 0.0f), D3DXVECTOR2(1, 0) }
	};

	LPDIRECT3DSURFACE9 pOriginalSurface, pSrcSurface, pDstSurface;
	
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	XSystem::m_pDevice->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	XSystem::m_pDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(2, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	XSystem::m_pDevice->SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	XSystem::m_pDevice->SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(3, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	XSystem::m_pDevice->SetTexture(0, pSrcTexture);
	XSystem::m_pDevice->SetTexture(1, pSrcTexture);
	XSystem::m_pDevice->SetTexture(2, pSrcTexture);
	XSystem::m_pDevice->SetTexture(3, pSrcTexture);

	XSystem::m_pDevice->SetVertexShader(pVertexShader);
	XSystem::m_pDevice->SetPixelShader(pPixelShader);

	D3DXMATRIX pOrthoProj;
	D3DXMatrixOrthoOffCenterRH((D3DXMATRIX *)&pOrthoProj, 0, 1, 1, 0, 0.0f, 1.0f);
	D3DXMatrixTranspose(&pOrthoProj, &pOrthoProj);
	XSystem::m_pDevice->SetVertexShaderConstantF(0, &pOrthoProj._11, 4);

	float u1 = 1.0f / width;
	float v1 = 1.0f / height;
	float u_off = u1 * 0.5f;
	float v_off = v1 * 0.5f;

	XSystem::m_pDevice->GetRenderTarget(0, &pOriginalSurface);
	pDstTexture->GetSurfaceLevel(0, &pDstSurface);
	XSystem::m_pDevice->SetRenderTarget(0, pDstSurface);

	int pass = 0;
	int maxpass = 3;

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
    XSystem::m_pDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 2 );
    XSystem::m_pDevice->SetTextureStageState( 3, D3DTSS_TEXCOORDINDEX, 3 );
	while (1)
	{
		float c0[4] = {  u1*0.5f*pass+u_off,  v1*pass+v_off, -u1*pass-u_off,  v1*0.5f*pass+v_off };
		float c1[4] = { -u1*0.5f*pass-u_off, -v1*pass-v_off,  u1*pass+u_off, -v1*0.5f*pass-v_off };
		XSystem::m_pDevice->SetVertexShaderConstantF(4, (float *)&c0, 1);
		XSystem::m_pDevice->SetVertexShaderConstantF(5, (float *)&c1, 1);

		v[0].xyz.x = -u_off;
		v[0].xyz.y = 1.0f - v_off;
		v[1].xyz.x = -u_off;
		v[1].xyz.y = -v_off;
		v[2].xyz.x = 1.0f - u_off;
		v[2].xyz.y = 1.0f - v_off;
		v[3].xyz.x = 1.0f - u_off;
		v[3].xyz.y = -v_off;

		XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));

		if (++pass == maxpass)
			break;

		pSrcTexture->GetSurfaceLevel(0, &pSrcSurface);
		XSystem::m_pDevice->StretchRect(pDstSurface, NULL, pSrcSurface, NULL, D3DTEXF_NONE);
		SAFE_RELEASE(pSrcSurface);
	}

	XSystem::m_pDevice->SetRenderTarget(0, pOriginalSurface);
	SAFE_RELEASE(pOriginalSurface);
	SAFE_RELEASE(pDstSurface);

	XSystem::m_pDevice->SetVertexShader(0);
	XSystem::m_pDevice->SetPixelShader(0);	
}

static void DrawRect(int screenWidth, int screenHeight)
{
	struct { D3DXVECTOR4 xyzw; D3DXVECTOR2 uv; } v[4] = { 
		{ D3DXVECTOR4(-0.5f,			screenHeight-0.5f,	0.0f, 1.0f), D3DXVECTOR2(0, 1) },
		{ D3DXVECTOR4(-0.5f,			-0.5f,				0.0f, 1.0f), D3DXVECTOR2(0, 0) },
		{ D3DXVECTOR4(screenWidth-0.5f,	screenHeight-0.5f,	0.0f, 1.0f), D3DXVECTOR2(1, 1) },
		{ D3DXVECTOR4(screenWidth-0.5f,	-0.5f,				0.0f, 1.0f), D3DXVECTOR2(1, 0) } 
	};

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));	
}

void CMyD3DApplication::RenderPostProcess()
{
	XSystem::m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		
	// shader 1.1 codepath
	if ((XSystem::m_caps.VertexShaderVersion >= (DWORD)D3DVS_VERSION(1, 1) && m_pBlurVertexShader) && 
		(XSystem::m_caps.PixelShaderVersion >= (DWORD)D3DPS_VERSION(1, 1) && m_pBlurPixelShader))
	{
		LPDIRECT3DSURFACE9 pSurface;
		m_pBlurTexture[0]->GetSurfaceLevel(0, &pSurface);
		XSystem::m_pDevice->StretchRect(m_pBackBuffer, NULL, pSurface, NULL, D3DTEXF_LINEAR);
		pSurface->Release();

		if (m_pLuminanceTexture)
		{
			m_pLuminanceTexture->GetSurfaceLevel(0, &pSurface);
			XSystem::m_pDevice->StretchRect(m_pBackBuffer, NULL, pSurface, NULL, D3DTEXF_LINEAR);
			pSurface->Release();

			m_pLuminanceTexture->GenerateMipSubLevels();
		}

		BlurTextureWithShader(m_pBlurTexture[0], m_pBlurTexture[1], m_bbtWidth >> 2, m_bbtHeight >> 2, 
			m_pBlurVertexShader, m_pBlurPixelShader);

	}
	// no shader codepath
	else
	{
		LPDIRECT3DSURFACE9 pSurface;
		m_pBlurTexture[0]->GetSurfaceLevel(0, &pSurface);
		XSystem::m_pDevice->StretchRect(m_pBackBuffer, NULL, pSurface, NULL, D3DTEXF_LINEAR);
		pSurface->Release();

		BlurTexture(m_pBlurTexture[0], m_pBlurTexture[1], m_bbtWidth >> 2, m_bbtHeight >> 2);
		BlurTexture(m_pBlurTexture[1], m_pBlurTexture[0], m_bbtWidth >> 2, m_bbtHeight >> 2);
		BlurTexture(m_pBlurTexture[0], m_pBlurTexture[1], m_bbtWidth >> 2, m_bbtHeight >> 2);
		BlurTexture(m_pBlurTexture[1], m_pBlurTexture[0], m_bbtWidth >> 2, m_bbtHeight >> 2);
		BlurTexture(m_pBlurTexture[0], m_pBlurTexture[1], m_bbtWidth >> 2, m_bbtHeight >> 2);
	}

	int c = (int)(((float)KSetting::op.m_nPostProcessBrightness / 100.0f) * 255);

	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(c, c, c, 255));

	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
	XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);

	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 0);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	
	XSystem::m_pDevice->SetTexture(0, m_pBlurTexture[1]);
	XSystem::m_pDevice->SetTexture(1, m_pBlurTexture[1]);

	DrawRect(m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight);

	XSystem::m_pDevice->SetTexture(1, NULL);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	XSystem::m_pDevice->SetRenderState(D3DRS_LIGHTING, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
}

bool CMyD3DApplication::UpdateBackBufferTexture()
{
	if (m_pBackBufferTexture && !m_bBackBufferTextureUpdated)
	{
		LPDIRECT3DSURFACE9 pSurface;
		m_pBackBufferTexture->GetSurfaceLevel(0, &pSurface);
		XSystem::m_pDevice->StretchRect(m_pBackBuffer, NULL, pSurface, NULL, D3DTEXF_NONE);
		SAFE_RELEASE(pSurface);
		m_bBackBufferTextureUpdated = true;
	}

	return m_bBackBufferTextureUpdated;
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::Render()
{
	g_particleEntityManager.m_numDrawParticles = 0;

	m_bBackBufferTextureUpdated = false;

	XSystem::ProcessMessage();

	if (XSystem::m_dwFlags & XSystem::RESET_RENDER)
	{
		XSystem::m_dwFlags &= ~XSystem::RESET_RENDER;
		m_nRender = 0;
	}

	XSystem::ComputeFogValue(XSystem::m_vCamera);
		
	if (SUCCEEDED(m_pd3dDevice->BeginScene()))
	{
        // 지형 처리
		RenderView();

		if (KSetting::op.m_nEffect == 0)
		{
			g_particleEntityManager.Render(true);
		}

        // 쉐이더 설정
		if (KSetting::op.m_nPostProcess == 0)
		{
			RenderPostProcess();
		}

        // 화면 오버레이 렌더
		g_screenOverlay.Render();

        // 폰트 렌더
		XFont::Render();

#ifdef DEF_POINTSYSTEM_BLUEYALU_20061204
		XPointSystem::Render();
#endif //DEF_POINTSYSTEM_BLUEYALU_20061204

        // 로그와 로딩 드로우
		KGameSys::DrawLoadingAndLogin();

        // UI 렌더
		if ( !KCommand::m_bHideUI)
		{
            // 이벤트를 그리거나 UI를 그리거나 둘중 선택적으로 한다!!!
			if( g_cEvent.IsActive())
			{
				g_cEvent.OnPaint();
			}
			else
			{
				// 모든 UI 그린다.
				CContext::OnPaint();
			}

            // 툴팁과 ??을 그려준다.
			KGameSys::DrawTipAndDrag();

            // 몹과 유저, npc를 렌더 단 디버깅 모드에서만 쓴다.
			KGameSys::DrawTrace();

			KGameSys::OnDamageEX();
		}

        // 상위 탑 렌더
		XFont::RenderTop();

		// "/Set Frame" 화면에 여러가지 정보들을 텍스트로 표시 해 주는 부분
		if( KCommand::m_bShowFrame )
		{
			char sztmp[128];

			// Output fps statistics
			m_pFont->DrawText( 2,  0, D3DCOLOR_ARGB(255,255,255,0), m_strFrameStats );
			if( XSystem::m_nWireFrame )
			{
				sprintf(sztmp, "pick: %.3f %.3f %.3f", m_vPick.x, m_vPick.y, m_vPick.z);
				m_pFont->DrawText( 2, 20, D3DCOLOR_ARGB(255,255,255,0), sztmp);
			}
			else
				m_pFont->DrawText( 2, 20, D3DCOLOR_ARGB(255,255,255,0), m_strDeviceStats );

			sprintf(sztmp,"Vertices:%d  Triangle:%d TreeNode:: %d", Terrain::m_vtx_cnt, Terrain::m_tri_cnt,	TriangleTreeNode::count);
			m_pFont->DrawText( 2,  40, D3DCOLOR_ARGB(255,255,255,0), sztmp );
			sprintf(sztmp,"Camera:%.2f %.2f %.2f yaw:%f pitch:%f zoom:%f variance:%f", XSystem::m_vCamera.x, XSystem::m_vCamera.y, XSystem::m_vCamera.z,
				XSystem::m_fYaw, XSystem::m_fPitch, XSystem::m_fZoom, Terrain::m_fVariance);
			m_pFont->DrawText( 2,  60, D3DCOLOR_ARGB(255,255,255,0), sztmp );

			int cc = 0, mc = 0;
			/*
			CGameTarget * pTemp = CGame_Character::m_First;
			while (pTemp != NULL)
			{
				if ( pTemp->m_byKind == CK_PLAYER )
					cc++;
				else if ( pTemp->m_byKind == CK_MONSTER )
					mc++;
				pTemp = pTemp->Next;
			}
			*/
			for( CGame_Character::t_TargetInfoMap::iterator it = CGame_Character::m_TargetInfoMap.begin(); it != CGame_Character::m_TargetInfoMap.end(); ++it)
			{
				if ( (*it).second.m_pTarget->m_byKind == CK_PLAYER )
					cc++;
				else if ( (*it).second.m_pTarget->m_byKind == CK_MONSTER )
					mc++;
			}

			sprintf( sztmp, "CharCount = %d, MonsterCount = %d, WaterHeight = %.3f, DialogBoxMode = %s", cc, mc, g_waterHeight, m_bDialogBoxMode?"true":"false");
			m_pFont->DrawText( 2, 80, D3DCOLOR_ARGB( 255, 255, 255, 0), sztmp);

			if( CGame_Character::m_Master) {
				sprintf( sztmp, "SERVER %d %d %d, MAP %d %d, CLIENT %.2f %.2f %.2f", 
					ToServer( CGame_Character::m_Master->m_vPosi.x, XSystem::m_originPatchX),
					ToServer( CGame_Character::m_Master->m_vPosi.z, XSystem::m_originPatchY),
					ToServerY( CGame_Character::m_Master->m_vPosi.y),
					PATCH_SIZE/2 + (int)floor(XSystem::m_vCenter.x/(MAP_SIZE*CELL_SIZE)),
					PATCH_SIZE/2 + (int)floor(XSystem::m_vCenter.z/(MAP_SIZE*CELL_SIZE)),
					XSystem::m_vCenter.x, XSystem::m_vCenter.z, XSystem::m_vCenter.y);					
				m_pFont->DrawText( 2, 100, D3DCOLOR_ARGB( 255, 255, 255, 0), sztmp);
//				m_pFont->DrawTextScaled( XSystem::m_vCenter.x, XSystem::m_vCenter.z, XSystem::m_vCenter.y, 30, 30,  D3DCOLOR_ARGB( 255, 255, 255, 0), sztmp);
			}
#if	!defined(_SOCKET_TEST)
//			sprintf( sztmp, "GetFloor = %f", XEngine::GetFloor( XSystem::m_vCenter.x, XSystem::m_vCenter.z));
			sprintf( sztmp, "GetFloor = %.2f, CanStand = %d", XEngine::GetFloor( XSystem::m_vCenter.x, XSystem::m_vCenter.z), 
				XEngine::CanStand( XSystem::m_vCenter.x, XSystem::m_vCenter.z));
			m_pFont->DrawText( 2, 120, D3DCOLOR_ARGB( 255, 255, 255, 0), sztmp);
#endif
		}// end 화면 정보 표시

#ifndef _SOCKET_TEST
		if (XSystem::m_dwFlags & XSystem::EDITEFFECT_MODE)
			m_pFont->DrawText(0, 0, D3DCOLOR_RGBA(255, 255, 255, 255), va("particles: %d", g_particleEntityManager.m_numDrawParticles));				
#endif

        m_pd3dDevice->EndScene();
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
#include "KControl.h"
HRESULT CMyD3DApplication::InitDeviceObjects()
{
	WRITELOG( "InitDevice ENTER\n");

	if (XSystem::m_dwOSVersion < 0x500) { // at Windows 98 has bug
#if 0
		m_pd3dDevice->SetDialogBoxMode(TRUE);
#else
		SetDialogBoxMode( TRUE);
#endif
	}	

    // Restore the font
    m_pFont->InitDeviceObjects( m_pd3dDevice );

	//init devices in the texutre manager
	XSystem::InitDevice(m_pd3dDevice);	
	
	XSoundSystem::Initialize(m_hWnd);

	KSetting::SetGammaTable();

	CParticleBundle::Init();
	Terrain::InitDevice(m_pd3dDevice);

	CContext::m_pd3dDevice = m_pd3dDevice;
	KWindowCollector::InitDevice( m_pd3dDevice);

#ifndef _MAP_TEST
	if (GetPrivateProfileInt( "location", "edit", 0, "./debug.ini"))
		XSystem::m_dwFlags |= XSystem::EDIT_MODE;

	if (GetPrivateProfileInt( "location", "editMap", 0, "./debug.ini"))
		XSystem::m_dwFlags |= XSystem::EDITMAP_MODE;

	if (GetPrivateProfileInt( "location", "editEffect", 0, "./debug.ini"))
		XSystem::m_dwFlags |= XSystem::EDITEFFECT_MODE;
#endif
	
#ifdef _MAP_TEST
	Terrain::LoadMap( g_szTestMapName);
#else

#ifdef _SOCKET_TEST
	MMap::Load("map\\login_map", 0, 6);
	XSystem::m_originPatchX = 0;
	XSystem::m_originPatchY = 6;
#else
	char szMapName[20];
	GetPrivateProfileString("location", "map", "", szMapName, sizeof(szMapName), "./debug.ini");
	if (szMapName[0])
	{
		char path[MAX_PATH];
		wsprintf(path, "map\\%s", szMapName);
		MMap::Load(path, 0, 0);

		XSystem::m_originPatchX = 0;
		XSystem::m_originPatchY = 0;
	}
	else
	{		
		Terrain::LoadMap("DATA\\MAPS\\n");		
		XSystem::m_originPatchX = 32;
		XSystem::m_originPatchY = 32;
	}
#endif

#endif

	GameObject::InitDevice();
	//GameMemory_Setup();

	KWindowCollector::SetViewPort( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight);

#ifndef _SOCKET_TEST
	KGameSys::BeginPlay();
	CHARDESC desc;
	desc.byHero = 1;
	desc.byKind = CK_PLAYER;
	desc.byArmor = GetPrivateProfileInt( "location", "Clothes", 1, "./debug.ini");
	desc.byWeapon = GetPrivateProfileInt( "location", "Weapon", 1, "./debug.ini");
	desc.byShield = GetPrivateProfileInt( "location", "Shield", 1, "./debug.ini");

	KCommand::m_bShowFrame = TRUE;
	// D3DXVECTOR3 v( ToClient( 265484), 0, ToClient( 285144));
	D3DXVECTOR3 v(0,0,0);


	if (GetPrivateProfileInt( "location", "client", 0, "./debug.ini")) {
		v.x = (float)(int)GetPrivateProfileInt( "location", "x", 265609, "./debug.ini");
		v.z = (float)(int)GetPrivateProfileInt( "location", "z", 285401, "./debug.ini");
		v.y = (float)(int)GetPrivateProfileInt( "location", "y", 22640, "./debug.ini");
	}
	else {
		v.x = ToClient(GetPrivateProfileInt( "location", "x", 265609, "./debug.ini"), XSystem::m_originPatchX);
		v.z = ToClient(GetPrivateProfileInt( "location", "z", 285401, "./debug.ini"), XSystem::m_originPatchY);
		v.y = ToClientY(GetPrivateProfileInt( "location", "y", 22640, "./debug.ini"));
	}
	{
		char buff[80];
		GetPrivateProfileString("location", "yaw", "", buff, sizeof(buff), "./debug.ini");
		XSystem::m_fYaw = (float) atof(buff);
		GetPrivateProfileString("location", "pitch", "", buff, sizeof(buff), "./debug.ini");
		XSystem::m_fPitch = (float) atof(buff);
		GetPrivateProfileString("location", "zoom", "80", buff, sizeof(buff), "./debug.ini");
		XSystem::m_fZoom = (float) atof(buff);
	}

#ifdef _MAP_TEST
	v.x = float( (g_nTestMapX) * CELL_SIZE * MAP_SIZE - CELL_SIZE * MAP_SIZE * PATCH_SIZE / 2 + CELL_SIZE * MAP_SIZE * 0.5f);
	v.z = float( (g_nTestMapY) * CELL_SIZE * MAP_SIZE - CELL_SIZE * MAP_SIZE * PATCH_SIZE / 2 + CELL_SIZE * MAP_SIZE * 0.5f);
#endif
	if (v.y < WATER_POS_Y)
		v.y = WATER_POS_Y;

	XSystem::SetCenter(v);	
	XSystem::WaitLoading();

	CGame_Character::CreateUser( &desc, v);
	CGame_Character::m_Master->ModelPosition();

#endif

#ifdef _SOCKET_TEST
	KWindowCollector::CloseAll();
	
	if( ((KGameSys::m_byTestClient & KGameSys::KOR_TEST) && KGameSys::GetClientCountryCode() == 1) || KGameSys::GetClientCountryCode() == 3)
		KWindowCollector::Load( "testserver_warning");
	else
		KNoticeSystem::OpenNoticeWindow();
	
	if( !KMessageBox::m_szDeviceMessage.empty())
		KWindowCollector::MBox( KMessageBox::m_szDeviceMessage.c_str(), MB_OK);
#endif

	CEffect_Particle::InitDeviceObjects(m_pd3dDevice);
	CEffect_Explosion::InitDeviceObjects(m_pd3dDevice);
	g_particleEntityManager.InitDeviceObjects(m_pd3dDevice);
	g_fxEntityManager.Init();	
	g_effectEntityManager.Init();

	g_screenOverlay.Init();
	g_SlotEffect.Init();

	KMiniMapSystem::InitMiniMap();
	KWorldMapSystem::InitWorldMap();
	KWeaponUpgradeSystem::Init();

	HRESULT hr = m_pD3D->CheckDeviceFormat(
				XSystem::m_caps.AdapterOrdinal,
				XSystem::m_caps.DeviceType,
				m_d3dSettings.DisplayMode().Format,
				0, D3DRTYPE_TEXTURE,
				D3DFMT_V8U8);

	m_Water.Init( hr == D3D_OK, m_d3dsdBackBuffer.Format);
	
	m_SkyBox.InitDevice();
	m_SkyBox.SetTexture("sample_sky_");

	XFont::PresetFont(XSystem::m_hFont);
	XFont::InitDevice();
//	XFont *pFont = XFont::Create("abc가나다", XFont::TOP);
//	pFont->m_bShow = TRUE;
//	pFont->m_vPos.x = 95.5f;
//	pFont->m_vPos.y = 95.5f;

//#ifdef	_DEBUG
//	D3DXMATRIXA16 m, m1, m2;
//	D3DXMatrixRotationYawPitchRoll(&m1, D3DX_PI/4, 0, 0); 
//	D3DXMatrixScaling(&m2, 2, 1, 1);
//	D3DXMatrixMultiply(&m1, &m1, &m2);
//	D3DXMatrixTranslation(&m2, -5184, 1589, -4640);
//	D3DXMatrixMultiply(&m, &m1, &m2);
//	CStaticModel::CreateModel("demo1", m);
//#endif

	WRITELOG( "InitDevice SUCCESS\n");

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::RestoreDeviceObjects()
{
	WRITELOG( "RESTOREDEVICE ENTER\n");

	KWindowCollector::SetViewPort( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight);

    m_pFont->RestoreDeviceObjects();
	
	//recreate textures and index buffers

    // Set miscellaneous render states
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,        TRUE );

	//m_pd3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHAREF, 128);
	m_pd3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

	// Assume d3dDevice is a valid pointer to an IDirect3DDevice9 interface.
	D3DLIGHT9 d3dLight;
	HRESULT   hr;

	// Initialize the structure.
	ZeroMemory(&d3dLight, sizeof(d3dLight));

	// Set up a white point light.

	d3dLight.Type = D3DLIGHT_DIRECTIONAL;
	d3dLight.Ambient = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Diffuse = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Specular = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Direction = D3DXVECTOR3(1.0f, -1.0f, 1.0f);
	D3DXVec3Normalize((D3DXVECTOR3 *)&d3dLight.Direction, (D3DXVECTOR3 *) &d3dLight.Direction);
	hr = m_pd3dDevice->SetLight(0, &d3dLight);
	hr = m_pd3dDevice->LightEnable(0, TRUE);
	XSystem::SetLightDirection(* (D3DXVECTOR3 *) &d3dLight.Direction);
	//hr = m_pd3dDevice->SetRenderState(D3DRS_AMBIENT, 0xffffffff);

    // Setup a material

	D3DMATERIAL9 mtrl;
    ZeroMemory( &mtrl, sizeof(D3DMATERIAL9) );
	mtrl.Ambient = g_bOverBright ? D3DXCOLOR(0.25f, 0.25f, 0.25f, 1) : D3DXCOLOR(0.5f, 0.5f, 0.5f, 1); //(1, 1, 1, 1);
    mtrl.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1);
	mtrl.Specular = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1);
	mtrl.Power = 10;
    hr = m_pd3dDevice->SetMaterial( &mtrl );
	XSystem::SetMaterial(mtrl);

	// Set the world matrix
    D3DXMATRIXA16 matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_WORLD,  &matIdentity );

	D3DXMATRIXA16 matProj;
	FLOAT fAspect = ((FLOAT)m_d3dsdBackBuffer.Width) / m_d3dsdBackBuffer.Height;
	D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, fAspect, NEAR_Z, (XSystem::m_dwFlags & XSystem::EDIT_MODE) ? EDIT_FAR_Z : FAR_Z );
	m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
	//set presprctive specs to our user frustum
	XSystem::SetProjection(matProj, D3DX_PI/4, fAspect, NEAR_Z, (XSystem::m_dwFlags & XSystem::EDIT_MODE) ? EDIT_FAR_Z : FAR_Z );
	SetView();

	if (!XSystem::RestoreDevice())
		return E_FAIL;	

	XSystem::m_pDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_pBackBuffer);

	InitPostProcess();	

	if (!Terrain::RestoreDevice())
		return E_FAIL;

	CWaterSystem::SetViewPort( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight);

	if( !m_Water.RestoreDevice())
		return E_FAIL;

#ifdef _DEBUG
	if( XSystem::m_caps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS)
		m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&g_fMipMapLodBias);
#endif

	GameObject::RestoreDevice();

#ifdef _DEBUG
	if( GetPrivateProfileInt( "unknown", "LOWEST", 0, "./setting.ini") == 0)
	{
		CEffect_Particle::RestoreDeviceObjects();
		CEffect_Explosion::RestoreDeviceObjects();
		g_particleEntityManager.RestoreDeviceObjects();		
	}
#else
	CEffect_Particle::RestoreDeviceObjects();
	CEffect_Explosion::RestoreDeviceObjects();
	g_particleEntityManager.RestoreDeviceObjects();
#endif

	WRITELOG( "RESTOREDEVICE SUCCESS\n");

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::InvalidateDeviceObjects()
{
	WRITELOG( "INVALIDATE DEVICE ENTER\n");

	TRACE("CMyD3DApplication::InvalidateDeviceObjects()\n");
    m_pFont->InvalidateDeviceObjects();

	CGame_Character::InvalidateDeviceObjects();
	//cleanup all device dependent stuff
	CEffect_Particle::InvalidateDeviceObjects();
	CEffect_Explosion::InvalidateDeviceObjects();
	g_particleEntityManager.InvalidateDeviceObjects();
	Terrain::InvalidateDevice();

	pG_EffectGroup.DestroyAll();

	//CGame_Item::Destroy();
	//CGame_Character::Destroy();

	ShutdownPostProcess();

	SAFE_RELEASE(m_pBackBuffer);

	m_Water.InvalidateDevice();

	GameObject::InvalidateDevice();
	XSystem::InvalidateDevice();
	if (XSystem::m_dwOSVersion < 0x500) {
#if 0
		m_pd3dDevice->SetDialogBoxMode(FALSE);
#else
		SetDialogBoxMode( FALSE);
#endif
	}	

	WRITELOG( "INVALIDATE DEVICE SUCCESS\n");

	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Called when the app is exiting, or the device is being changed,
//       this function deletes any device dependent objects.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::DeleteDeviceObjects()
{
	WRITELOG( "DELETEDEVICE ENTER\n");

	KSetting::RestoreGammaTable();

	XFont::DeleteDevice();
#ifdef _SOCKET_TEST
	KSocket::DisConnect();
	KSocket::DestroySocket();
#endif

	KGameSys::DeleteDevice();

	CEffect_Particle::DeleteDeviceObjects();
	CEffect_Explosion::DeleteDeviceObjects();
	g_particleEntityManager.DeleteDeviceObjects();
	CParticleBundle::Free();
	g_fxEntityManager.Shutdown();
	g_effectEntityManager.Shutdown();

	KGameSys::EndPlay();

	KWindowCollector::DeleteDevice();

	Terrain::DeleteDevice();
    m_pFont->DeleteDeviceObjects();

	g_screenOverlay.Free();
	g_SlotEffect.Free();

	KMiniMapSystem::MiniMapTexturClear();
	KWorldMapSystem::WorldMapTexturClear();
	
	m_SkyBox.DeleteDevice();
	m_Water.DeleteDevice();

	GameObject::DeleteDevice();

	CGame_Item::Destroy();
	CGame_Character::Destroy();
	MMap::Unload();
	
	ShutdownPostProcess();
	
	SAFE_RELEASE(m_pBackBuffer);

	XSoundSystem::Terminate();
	XSystem::DeleteDevice();
	
	XSystem::m_pDevice = NULL;	

	WRITELOG( "DELETEDEVICE SUCCESS\n");

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: FinalCleanup()
// Desc: Called before the app exits, this function gives the app the chance
//       to cleanup after itself.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::FinalCleanup()
{
    SAFE_DELETE( m_pFont );
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ConfirmDevice()
// Desc: Called during device intialization, this code checks the device
//       for some minimum set of capabilities
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::ConfirmDevice( D3DCAPS9* pCaps, DWORD dwBehavior,
                                          D3DFORMAT Format1, D3DFORMAT Format2)
{
	//static bool bConfirmDevice = false;
#ifdef DEBUG_VS
    if( pCaps->DeviceType != D3DDEVTYPE_REF && 
        (dwBehavior & D3DCREATE_SOFTWARE_VERTEXPROCESSING) == 0 )
        return E_FAIL;
#endif
#ifdef DEBUG_PS
    if( pCaps->DeviceType != D3DDEVTYPE_REF )
        return E_FAIL;
#endif
	if ((dwBehavior & D3DCREATE_PUREDEVICE) || (dwBehavior & D3DCREATE_HARDWARE_VERTEXPROCESSING)) {
		if (pCaps->VertexShaderVersion < D3DVS_VERSION(1, 1))
			return E_FAIL;
	}
    // If this is a TnL device, make sure it supports directional lights
    if( (dwBehavior & D3DCREATE_HARDWARE_VERTEXPROCESSING ) ||
        (dwBehavior & D3DCREATE_MIXED_VERTEXPROCESSING ) )
    {
        if( !(pCaps->VertexProcessingCaps & D3DVTXPCAPS_DIRECTIONALLIGHTS ) )
            return E_FAIL;
    }

	if (pCaps->MaxTextureHeight < 512 || pCaps->MaxTextureWidth < 512)
		return E_FAIL;

    return S_OK;
}

static UINT g_nLeadByte;
//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: Overrrides the main WndProc, so the sample can do custom message
//       handling (e.g. processing mouse, keyboard, or menu commands).
//-----------------------------------------------------------------------------
LRESULT CMyD3DApplication::MsgProc( HWND hWnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam )
{
	static POINT m_mousePos={0,0};

#ifndef _SOCKET_TEST
	if (MapEditor_MsgProc(msg, wParam, lParam))
		goto END_OF_MSGPROC;
#endif

    switch( msg )
    {
		case WM_CREATE:

#ifdef DEF_LAUNCHER_UPDATE_DIOB_20070423 //추가: SetTimer(2) 런처 업데이트 / SEED
			SetTimer(hWnd, 2, 1000, NULL);
#endif //DEF_LAUNCHER_UPDATE_DIOB_20070423

			CContext::Create(hWnd);
			break;
  		case WM_SETFOCUS:
			CContext::OnSetFocus();
			break;

		case WM_KILLFOCUS:
			CContext::OnKillFocus();
			break;

		case WM_INPUTLANGCHANGE:
			CContext::OnInputLangChange();
			break; //goto default_proc;
		
		case WM_IME_STARTCOMPOSITION:
			TRACE("OnImeStartComposition\n");
			if (!CContext::OnImeStartComposition())
				break; //goto default_proc;
			return 0;

		case WM_IME_COMPOSITION:
			TRACE("OnImeComposition\n");
			if (!CContext::OnImeComposition(lParam))
				break; //goto default_proc;
			return 0;

		case WM_IME_ENDCOMPOSITION:
			TRACE("OnImeEndComposition\n");
			if (!CContext::OnImeEndComposition())
				break; // goto default_proc;
			return 0;

		case WM_IME_NOTIFY:
			TRACE("OnImeNotify\n");
			CContext::OnImeNotify(wParam);
			break; //goto default_proc;

		case WM_ACTIVATEAPP:
			//SetThreadPriority(GetCurrentThread(), wParam ? THREAD_PRIORITY_NORMAL : THREAD_PRIORITY_IDLE);
			SetPriorityClass(GetCurrentProcess(), wParam ? NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS);
			break;
		case WM_SYSKEYDOWN:
			{
				if( KGameSys::InGamePlay() && wParam == VK_F10)
				{
					KGameSys::Chk_ChattingMacroKey( wParam);
				}
			}
			break;
		case WM_KEYDOWN:
			{

#ifdef DEF_TEST_MAKE_PACKET_ONE_070927
                // #test4
                // 강제로 패킷을 오게 한다
                if (wParam == VK_F5)
                {
                    PACKETBUFFER pPAcketBuf = {0};

                    // 지역 생성 테스트
                    int nPatchX = 0;
                    int nPatchZ = 0;    
                    int x = 0;
                    int y = 0;
                    
                    nPatchX = g_UsedTestInfo.nPatchX;
                    nPatchZ = g_UsedTestInfo.nPatchY;
                    x += g_UsedTestInfo.nMoveX;
                    y += g_UsedTestInfo.nMoveY;

                    x += nPatchX * (MAP_SIZE * CELL_SIZE) + ((MAP_SIZE * CELL_SIZE) / 2);
                    y += nPatchZ * (MAP_SIZE * CELL_SIZE) + ((MAP_SIZE * CELL_SIZE) / 2);

                    // 지역 이동 패킷
                    pPAcketBuf = KSocket::makePacket(S2C_TELEPORT,
                                                     "bdddb",
                                                     0,         // map index
                                                     x,         // svr x
                                                     y,         // svr z
                                                     0,         // svr y
                                                     1);        // temp C2S_TELEPORT not used

                    KSocket::OnRecv(&pPAcketBuf);
                    break;
                }
                if (wParam == VK_F11)
                {
                    // 낚시왕 순위 스크립트 테스트
                    PACKETBUFFER pPAcketBuf2 = {0};

                    FishChampion testFish[4] = { {"낚시왕2", FISH_NU, 120},
                                                 {"낚시왕3", FISH_HWAN, 120},
                                                 {"낚시왕4", FISH_HWAN, 120},
                                                 {"낚시왕5", FISH_SING, 120} };

                    pPAcketBuf2 = KSocket::makePacket(S2C_EVENT_FISHING,
                                                    "bbbm",
                                                    2,
                                                    1,
                                                    4,
                                                    &testFish, sizeof(FishChampion) * 4
                                                    );

                    KSocket::OnRecv(&pPAcketBuf2);
                    break;
                }
                if (wParam == VK_F3)
                {
                    PACKETBUFFER pPAcketBuf = {0};

                    // 스킬 시전 테스트 패킷
                    //int nx = ToServer( CGame_Character::m_Master->m_vPosi.x, XSystem::m_originPatchX);
                    //int nz = ToServer( CGame_Character::m_Master->m_vPosi.z, XSystem::m_originPatchY);

                    //KSocket::WritePacket2(C2S_SKILL, "bdd", 64, nx, nz);

                    //pPAcketBuf = KSocket::makePacket(S2C_SKILL,
                    //                                 "bddbb",
                    //                                 65,
                    //                                 CGame_Character::m_Master->m_CharaterID,
                    //                                 0,
                    //                                 0,
                    //                                 1);

                    // npc 생성 테스트
                    //DWORD a = (255891 << 16) + 254605;

                    //pPAcketBuf = KSocket::makePacket(S2C_CREATENPC,
                    //                                 "dwbdddwId",
                    //                                 10101,             // 캐릭터 고유 넘버
                    //                                 551,               // npc 넘버
                    //                                 51,                // 형태 정보(3번은 상인npc, macro.txt npcinfo에 존재)
                    //                                 257330,            // 위치x
                    //                                 259390,            // 위치y
                    //                                 //42 * (MAP_SIZE * CELL_SIZE) + ((MAP_SIZE * CELL_SIZE) / 2),
                    //                                 //34 * (MAP_SIZE * CELL_SIZE) + ((MAP_SIZE * CELL_SIZE) / 2),
                    //                                 //24897,           // 위치z
                    //                                 16168,             // 위치z
                    //                                 &a,                // 방향
                    //                                 0,                 // 상태 기본적인 상태 CGS NPC는 무조건 0인것 같다
                    //                                 0);                // 아이템 상태값??? 현재 사용하지 않는다. 처음 설계시에 사용한듯 

                    // npc 스크립트 테스트
                    //pPAcketBuf = KSocket::makePacket(S2C_SENDHTML,
                    //                                 "dd",
                    //                                 300055, 10);    

                    // 이벤트 결과 패킷 테스트
                    //pPAcketBuf = KSocket::makePacket(S2C_EVENT_FISHING,
                    //                                 "bbsbbb",
                    //                                 1,         // 결과
                    //                                 3,         // 주
                    //                                 "낚시왕",  // 낚시왕
                    //                                 FISH_NU,   // 어종
                    //                                 12,        // m
                    //                                 12);       // cm

                    // 새로운 메시지 테스트                    
                    //pPAcketBuf = KSocket::makePacket(S2C_MESSAGE,
                    //                                 "b",
                    //                                 MSG_CONNECT_TIME_FAILED);

                    //pPAcketBuf = KSocket::makePacket(S2C_EVENT_FISHING,
                    //                                "b",
                    //                                0);

                    // 고급낚시 결과 테스트
                    //pPAcketBuf = KSocket::makePacket(S2C_EVENT_FISHING,
                    //                                "bbsbbbsbbbsbbbsbbb",
                    //                                2,         // 결과
                    //                                1,         // 주
                    //                                "낚시왕2", 1, 12, 12,
                    //                                "낚시왕3", 2, 12, 12,
                    //                                "낚시왕4", 3, 12, 12,
                    //                                "낚시왕5", 1, 12, 12
                    //                                );

                    //FishChampion testFish[4] = { {"낚시왕2", FISH_NU, 120},
                    //                             {"낚시왕3", FISH_HWAN, 120},
                    //                             {"낚시왕4", FISH_HWAN, 120},
                    //                             {"낚시왕5", FISH_SING, 120} };

                    //pPAcketBuf = KSocket::makePacket(S2C_EVENT_FISHING,
                    //                                "bbbm",
                    //                                2,
                    //                                1,
                    //                                4,
                    //                                &testFish, sizeof(FishChampion) * 4
                    //                                );

                    // 조합 조작 패킷 테스트
                    //KSocket::WritePacketAutoCrc(C2S_MIXING, "bddddd", 2, 
                    //                            g_UsedTestInfo.nMoveX,  
                    //                            g_UsedTestInfo.nMoveY,  
                    //                            g_UsedTestInfo.nPatchX, 
                    //                            g_UsedTestInfo.nPatchY,
                    //                            g_UsedTestInfo.nExp);

                    //KSocket::WritePacket(C2S_SETSTALLINFO, "bddd", 0, 
                    //                     g_UsedTestInfo.nMoveX, 
                    //                     g_UsedTestInfo.nMoveY, 
                    //                     g_UsedTestInfo.nPatchX);

                    //KSocket::WritePacket( C2S_DROPITEM, "dd", g_UsedTestInfo.nMoveX, 1);

                    // 결과 코드 테스트
                    //pPAcketBuf = KSocket::makePacket(S2C_RESULT_CODE,
                    //                                 "b",
                    //                                 1234);

                    // 무한 컨넥트 테스트
                    //int nAppTime = (int)(DXUtil_Timer( TIMER_GETAPPTIME ) * 1000.0f);

                    //KSocket::WritePacket( C2S_CONNECT, "d", nAppTime);
                    
                    // 낚시 시간 설정 테스트
                    //pPAcketBuf = KSocket::makePacket(S2C_MESSAGE,
                    //                                 "bb",
                    //                                 MSG_FISH_PROGRESS,
                    //                                 1);
                    
                    // 지역 생성 테스트
                    //int nPatchX = 0;
                    //int nPatchZ = 0;    
                    //int x = 0;
                    //int y = 0;
                    //
                    //nPatchX = g_UsedTestInfo.nPatchX;
                    //nPatchZ = g_UsedTestInfo.nPatchY;
                    //x += g_UsedTestInfo.nMoveX;
                    //y += g_UsedTestInfo.nMoveY;

                    //x += nPatchX * (MAP_SIZE * CELL_SIZE) + ((MAP_SIZE * CELL_SIZE) / 2);
                    //y += nPatchZ * (MAP_SIZE * CELL_SIZE) + ((MAP_SIZE * CELL_SIZE) / 2);

                    //// 지역 이동 패킷
                    //pPAcketBuf = KSocket::makePacket(S2C_TELEPORT,
                    //                                 "bdddb",
                    //                                 0,         // map index
                    //                                 x,         // svr x
                    //                                 y,         // svr z
                    //                                 0,         // svr y
                    //                                 1);        // temp C2S_TELEPORT not used

                    // 임의의 아이템 생성 패킷
                    //pPAcketBuf = KSocket::makePacket(S2C_INSERTITEM,
                    //                                 "dwbdd",                                                     
                    //                                 -2000000000,           // 아이템 고유 id
                    //                                 g_UsedTestInfo.nMoveX, // 아이템 종류 id
                    //                                 0,                     // 아이템 prefix
                    //                                 0,                     // 아이템 정보
                    //                                 1);                    // 아이템 개수

                    //KSocket::OnRecv(&pPAcketBuf);
                    break;
                }

                if (wParam == VK_F4)
                {
#ifdef  DEF_TEST_RELOAD_CONFIG_OEN_071009
                    // config파일 리로딩 테스트
                    KMacro::Reload();
#endif                    
#ifdef  DEF_TEST_LIGHTING_OEN_070917
                    // 라이트닝 테스트
                    //PACKETBUFFER pPAcketBuf = {0};
                    //int nx = ToServer( CGame_Character::m_Master->m_vPosi.x, XSystem::m_originPatchX);
                    //int nz = ToServer( CGame_Character::m_Master->m_vPosi.z, XSystem::m_originPatchY);

                    //KSocket::WritePacket2(C2S_SKILL, "bdd", 45, nx, nz);

                    // 시전 스킬 범위 선정
                    //POINT pt;
                    //GetCursorPos(&pt);		
                    //ScreenToClient(CContext::m_hWnd, &pt);
                    //D3DXVECTOR2 vec2((pt.x * 2 + 1) / (float) g_pMyD3DApp->m_d3dpp.BackBufferWidth - 1.0f,
                    //    -(pt.y * 2 + 1) / (float) g_pMyD3DApp->m_d3dpp.BackBufferHeight + 1.0f);
                    //D3DXVECTOR3 vFrom(vec2.x * XSystem::m_vNear.x, vec2.y * XSystem::m_vNear.y, XSystem::m_vNear.z);
                    //D3DXVECTOR3 vTo(vec2.x * XSystem::m_vFar.x, vec2.y * XSystem::m_vFar.y, XSystem::m_vFar.z);
                    //D3DXVECTOR3 vOut;
                    //D3DXVec3TransformCoord(&vFrom, &vFrom, &XSystem::m_mViewInv);
                    //D3DXVec3TransformCoord(&vTo, &vTo, &XSystem::m_mViewInv);
                    //XEngine::Pick( &vOut, vFrom, vTo, XEngine::CAMERA);

                    //// 범위 공격을 할 위치를 저장
                    //CGame_Character::m_Master->m_vPosRangeSkill.x = vOut.x;
                    //CGame_Character::m_Master->m_vPosRangeSkill.y = vOut.z;
                    //CGame_Character::m_Master->m_vPosRangeSkill.z = vOut.y;
                    //CGame_Character::m_Master->m_byRangeSkillCast = 2;                    
                    //CGame_Character::m_Master->m_byRangeSkillIndex = 45;

                    //CGame_Character::m_Master->ModelSkill(45, CGame_Character::m_Master->m_CharaterID, 0, 0, 0, 0);                    

                    // 스킬 시전하라
                    //CGame_Character::m_Master->m_byRangeSkillCast = 1;
                    //CGame_Character::m_Master->m_byRangeSkillIndex = 45;
                    //KGameSys::ChkRangeSkillCast(WM_LBUTTONDOWN);

                    //pPAcketBuf = KSocket::makePacket(S2C_ACTION,
                    //                                 "db",
                    //                                 CGame_Character::m_Master->m_CharaterID,
                    //                                 0);

                    //pPAcketBuf = KSocket::makePacket(S2C_SKILL,
                    //                                 "bddbb",
                    //                                 45,
                    //                                 CGame_Character::m_Master->m_CharaterID,
                    //                                 0,
                    //                                 0,
                    //                                 1);
                    //KSocket::OnRecv(&pPAcketBuf);
#endif
#ifdef DEF_TEST_OVERUSER_OEN_070917
                    // 서버인원 초과 테스트
                    //PACKETBUFFER pPAcketBuf = {0};
                    //pPAcketBuf = KSocket::makePacket(S2C_CLOSE,
                    //                                 "b",
                    //                                 CC_OVERPOPULATION);
                    //KSocket::OnRecv(&pPAcketBuf);
#endif                                     
                    break;
                }                
#endif

#ifdef _DEBUG   // 디버그 모드시

				if( wParam == VK_F2) {
                    Pause(true);
                    UserSelectNewDevice();
                    Pause(false);
				}

                
				if( wParam == VK_SHIFT && GetKeyState(VK_RSHIFT) < 0 )
				{
					if( KGameSys::InGamePlay())
					{
						XSystem::m_fYaw = CGame_Character::m_Master->m_Rotate;

						while( XSystem::m_fYaw < 0.0f )
						{
							XSystem::m_fYaw += ( 2.0f * D3DX_PI );
						}

						while( XSystem::m_fYaw >= ( 2.0f * D3DX_PI ) )
						{
							XSystem::m_fYaw -= ( 2.0f * D3DX_PI );
						}

						if( XSystem::m_fYaw > D3DX_PI )
						{
							XSystem::m_fYaw -= D3DX_PI;
						}
						else
						{
							XSystem::m_fYaw += D3DX_PI;
						}

						SetView();
					}
				}
#endif          // 디버그 모드가 종료
                // 스샷찍기
				if (wParam == VK_F12 || wParam == VK_SNAPSHOT)
				{
					SYSTEMTIME	st;
					char		path[128];
					char		picname[128];
					char		n[3];
					FILE		*fp;
					int			i;

					GetLocalTime(&st);

					CreateDirectory("screenshot",NULL);

					sprintf(picname,"shot_%d_%d_%d_",st.wYear,st.wMonth,st.wDay);
					for (i=0; i <= 999; i++)
					{
						n[0] = '0' + i / 100;
						n[1] = '0' + (i % 100) / 10;
						n[2] = '0' + (i % 10);
						sprintf(path,"screenshot/%s%c%c%c.jpg",picname,n[0],n[1],n[2]);
						if (!(fp = fopen(path,"rb")))
							break;

						fclose(fp);
					}

					if (i < 1000)
					{
						TakeScreenShotJpeg(path);
						std::strstream s;
						s << KMsg::Get( 775) << " '" << path << "'" << std::ends;
						KGameSys::AddInfoMessage(s.str());
					}
					else
					{
						// 같은 날 스샷 1000개 넘으면 저장안한다
					}
				}

                // 기타 처리!!!
				if( KGameSys::InGamePlay())
				{
					KGameSys::Chk_ChattingMacroKey( wParam);
					KGameSys::CancelRangeSkillCast();
#ifdef DEF_SCENARIO3_BOMB
					KGameSys::CancelRangeBomb();
#endif	//DEF_SCENARIO3_BOMB
				}

#ifdef _SOCKET_TEST

                // 도움말 오픈유무
				if( KGameSys::InGamePlay())
				{
					if( wParam == VK_F1) {
						bool opened = false;

						for (int i=0; i < 16; i++)
						{
							char name[64];
							sprintf(name,"s1000%02d",i);
							if (KWindowCollector::Find(name))
							{
								KWindowCollector::CloseReservation(name);
								opened = true;
							}
						}
						
						if (!opened)
							KWindowCollector::Load( "s100001");
					}						
				}
				
#ifdef DEF_SCENARIO3_BOMB
				if( wParam == VK_ESCAPE)
					KGameSys::CancelRangeBomb();
#endif	//DEF_SCENARIO3_BOMB

                // 현재 컨텍스트의 포커스를 얻어온다.
				CControl* pFocus = CContext::GetFocus();

                // 이스케이프를 눌렀을 때
				if( wParam == VK_ESCAPE && pFocus && pFocus->IsEditor())
					CContext::SetFocus( NULL);

				// 스페이스바와 쉬프트? 같이 누를때?
				if( wParam == VK_SPACE && ( 0xFFF0 & GetAsyncKeyState( VK_SHIFT))) {
					if( KGameSys::OnWisperCandidate())
						break;
				}

                // 일반적인 윈도우 창 키다운 처리
				if (CContext::OnKeyDown(wParam))
					break;

                // 일반적인 시스템 키다운 처리
				if( KGameSys::OnKeyDown( wParam, lParam))
					break;			

#else           // _SOCKET_TEST 모드가 아니라면

				if (wParam==VK_F1) {
					InvalidateDeviceObjects();
					RestoreDeviceObjects();
				}
				
				// toggle full screen
				if (wParam==VK_F4) {
					ToggleFullscreen();
				}			
/*
#ifdef EFFECT_EDITOR

				if (wParam == VK_F5)
				{
					if (!m_bWindowed)
						ToggleFullscreen();
					g_effectEditor.Toggle();
				}
#endif*/

				if (wParam == VK_DIVIDE)
				{
					KCommand::m_bHideModel = !KCommand::m_bHideModel;
				}

				if (wParam == VK_MULTIPLY)
				{
					KCommand::m_bHideMe = !KCommand::m_bHideMe;
				}

				if (wParam == VK_SUBTRACT)
				{
					KCommand::m_bHideUI = !KCommand::m_bHideUI;
				}

				if (wParam == VK_ADD)
				{
					KCommand::m_bHideTerrain = !KCommand::m_bHideTerrain;
				}

				if (CContext::OnKeyDown(wParam))
					break;

#endif
				//move flags
				switch (wParam) {
#ifndef	_SOCKET_RELEASE
				case VK_SHIFT:
				#ifdef _DEBUG
					if( GetKeyState(VK_RSHIFT) < 0 )
						break;
				#endif
					XSystem::m_nWireFrame = (XSystem::m_nWireFrame+1) % 3;
					m_vPick.x = m_vPick.y = m_vPick.z = 0;
					if (XSystem::m_nWireFrame) {
						CPoint pt;
						GetCursorPos(&pt);
						ScreenToClient(hWnd, &pt);
						D3DXVECTOR2 vec2((pt.x * 2 + 1) / (float) m_d3dpp.BackBufferWidth - 1.0f,
							-(pt.y * 2 + 1) / (float) m_d3dpp.BackBufferHeight + 1.0f);
						D3DXVECTOR3 vFrom(vec2.x * XSystem::m_vNear.x, vec2.y * XSystem::m_vNear.y, XSystem::m_vNear.z);
						D3DXVECTOR3 vTo(vec2.x * XSystem::m_vFar.x, vec2.y * XSystem::m_vFar.y, XSystem::m_vFar.z);
						D3DXVec3TransformCoord(&vFrom, &vFrom, &XSystem::m_mViewInv);
						D3DXVec3TransformCoord(&vTo, &vTo, &XSystem::m_mViewInv);
						XEngine::Pick(&m_vPick, vFrom, vTo);
					}
					break;

				case VK_UP:
					//KWindowCollector::CloseAll();			// 그림 삽입 테스트
					//KWindowCollector::Load( "loading" );
					m_nMoveFlag = FOREWARD;
					break;

				case VK_DOWN:
					//KWindowCollector::CloseReservation( "loading");	// 그림 삽입 테스트
					m_nMoveFlag = BACKWARD;
					break;

				case VK_LEFT:
					m_nMoveFlag = LEFTWARD;
					break;

				case VK_RIGHT:
					m_nMoveFlag = RIGHTWARD;
					break;

				case VK_HOME:
					m_nMoveFlag = UPWARD;
					break;

				case VK_END:
					m_nMoveFlag = DOWNWARD;
					break;
#else	
				case VK_UP:
					m_nMoveFlag = UPTURN;
					break;

				case VK_DOWN:
					m_nMoveFlag = DOWNTURN;
					break;

				case VK_LEFT:
					m_nMoveFlag = LEFTTURN;
					break;

				case VK_RIGHT:
					m_nMoveFlag = RIGHTTURN;
					break;
#endif
				case VK_PRIOR:
					m_nMoveFlag = ZOOMIN;
					break;

				case VK_NEXT:
					m_nMoveFlag = ZOOMOUT;
					break;

				case 'P':
					m_bDrawText = !m_bDrawText;
					break;

#ifndef _SOCKET_TEST
				case 'A':
					m_nMoveFlag = LEFTTURN;
					break;
				case 'D':
					m_nMoveFlag = RIGHTTURN;
					break;
				case 'W':
					m_nMoveFlag = UPTURN;
					break;
				case 'S':
					m_nMoveFlag = DOWNTURN;
					break;
				case 'R':
					m_nMoveFlag = VAR_INC;
					break;
				case 'F':
					m_nMoveFlag = VAR_DEC;
					break;
				case VK_TAB:
					if( CGame_Character::m_Master)
					{
						if ( CGame_Character::m_Master->m_ModeNo == 0 )
							CGame_Character::m_Master->m_ModeNo = 1;
						else
							CGame_Character::m_Master->m_ModeNo = 0;
						CGame_Character::m_Master->m_ActionNo = -1;
						CGame_Character::m_Master->MoveEnd();
						CGame_Character::m_Master->ModelChange(0);
					}
					break;
				case VK_DELETE:
					Terrain::m_nMaxDrawLayer--;
					if (Terrain::m_nMaxDrawLayer < 0)
						Terrain::m_nMaxDrawLayer = 0;
					break;
				case VK_INSERT:
					Terrain::m_nMaxDrawLayer++;
					break;
#endif
				}
			}
			break;

		// 키입력 처리
		case WM_CHAR:
			if (g_nLeadByte) {
				UINT nChar = (g_nLeadByte << 8) | (BYTE) wParam;
				g_nLeadByte = 0;
				CContext::OnImeChar(nChar);
				break;
			}
			if (::IsDBCSLeadByteEx(Control::m_nCodePage, wParam)) {
				g_nLeadByte = (BYTE) wParam;
				break;
			}

			if( KGameSys::OnChar(wParam, lParam))
				break;

			if( CContext::OnChar(wParam))
				break;

#ifdef _DEBUG
			switch( wParam)
			{
			case '+':
			case '-':
				g_fMipMapLodBias += ( 0.1f * (( wParam == '-')?-1.0f:1.0f));
				if( XSystem::m_caps.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS)
					m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, *(DWORD*)&g_fMipMapLodBias);
				break;
			}
#endif
			break;

		case WM_KEYUP:
			{
				if (wParam == VK_SNAPSHOT)
				{
					SYSTEMTIME	st;
					char		path[128];
					char		picname[128];
					char		n[3];
					FILE		*fp;
					int			i;

					GetLocalTime(&st);

					CreateDirectory("screenshot",NULL);

					sprintf(picname,"shot_%d_%d_%d_",st.wYear,st.wMonth,st.wDay);
					for (i=0; i <= 999; i++)
					{
						n[0] = '0' + i / 100;
						n[1] = '0' + (i % 100) / 10;
						n[2] = '0' + (i % 10);
						sprintf(path,"screenshot/%s%c%c%c.jpg",picname,n[0],n[1],n[2]);
						if (!(fp = fopen(path,"rb")))
							break;

						fclose(fp);
					}

					if (i < 1000)
					{
						TakeScreenShotJpeg(path);
						std::strstream s;
						s << KMsg::Get( 775) << " '" << path << "'" << std::ends;
						KGameSys::AddInfoMessage(s.str());
					}
					else
					{
						// 같은 날 스샷 1000개 넘으면 저장안한다
					}
				}
				m_nMoveFlag = 0;

			}
			
			break;
		case WM_SETCURSOR:
			::SetCursor(m_hCursor[m_nCursorPrev]);
			return TRUE;
//			if (!m_bShowCursorWhenFullscreen)
//				return DefWindowProc( hWnd, msg, wParam, lParam );
//			break;
		case WM_MOUSEMOVE:
			{
				if( m_d3dpp.BackBufferWidth == 0 || m_d3dpp.BackBufferHeight == 0)
					break;

				POINT pt;

				pt.x = GET_X_LPARAM(lParam); 
				pt.y = GET_Y_LPARAM(lParam);
				if ( pt.x < 0 || pt.x > (long)m_d3dpp.BackBufferWidth-1 || pt.y < 0 || pt.y > (long)m_d3dpp.BackBufferHeight-1 )
					break;				

//				SetCursor(m_hCursor[m_iCursorNo]);

				if (!KCommand::m_bHideUI)
				{
					if (CContext::OnMouseMove(pt, wParam))
						break;
				}

				short nFlags = LOWORD(wParam);    // key flags

				if (XSystem::m_nWireFrame)
				{
					D3DXVECTOR2 vec2((pt.x * 2 + 1) / (float) m_d3dpp.BackBufferWidth - 1.0f,
						-(pt.y * 2 + 1) / (float) m_d3dpp.BackBufferHeight + 1.0f);
					D3DXVECTOR3 vFrom(vec2.x * XSystem::m_vNear.x, vec2.y * XSystem::m_vNear.y, XSystem::m_vNear.z);
					D3DXVECTOR3 vTo(vec2.x * XSystem::m_vFar.x, vec2.y * XSystem::m_vFar.y, XSystem::m_vFar.z);
					D3DXVec3TransformCoord(&vFrom, &vFrom, &XSystem::m_mViewInv);
					D3DXVec3TransformCoord(&vTo, &vTo, &XSystem::m_mViewInv);
					m_vPick.x = m_vPick.y = m_vPick.z = 0;
					XEngine::Pick(&m_vPick, vFrom, vTo);
				}

                //////////////////////////////////////////////////////////////////////////
                // 마우스 오른쪽 키와 중간키(휠키) 누룸 상태 처리
				if (nFlags & MK_MBUTTON || nFlags & MK_RBUTTON )
				{
					/*  //캐논 발사 도중에는 대포가 안움직인다.
					if(CGame_Character::m_Master && CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON)
					{
						if(KGameSys::m_bCannonReady == 1)
						{
							break;
						}
					}
					*/

					long	dx = pt.x - m_mousePos.x;
					long	dy = pt.y - m_mousePos.y;
					if (dx == 0 && dy == 0)	break;
					if ( m_mousePos.x == 0 && m_mousePos.y == 0 )	break;

					XSystem::m_fYaw += dx / 100.f;
					while (XSystem::m_fYaw < 0) XSystem::m_fYaw += 2 * D3DX_PI;
					while (XSystem::m_fYaw >= 2 * D3DX_PI) XSystem::m_fYaw -= 2 * D3DX_PI;
					XSystem::m_fPitch += -dy / 100.f;
					if ( XSystem::m_fPitch > 1.f )
						XSystem::m_fPitch = 1.f;
					else if ( XSystem::m_fPitch < -3.1f / 2 )
						XSystem::m_fPitch = -3.1f / 2;
					//m_nRender=0;
					SetView();
				}
#ifdef _SOCKET_TEST
				else if (XSystem::m_bInit)
				{				
					D3DXVECTOR2 vec2((pt.x * 2 + 1) / (float) m_d3dpp.BackBufferWidth - 1.0f,
						-(pt.y * 2 + 1) / (float) m_d3dpp.BackBufferHeight + 1.0f);
					D3DXVECTOR3 vFrom(vec2.x * XSystem::m_vNear.x, vec2.y * XSystem::m_vNear.y, XSystem::m_vNear.z);
					D3DXVECTOR3 vTo(vec2.x * XSystem::m_vFar.x, vec2.y * XSystem::m_vFar.y, XSystem::m_vFar.z);

					D3DXVec3TransformCoord(&vFrom, &vFrom, &XSystem::m_mViewInv);
					D3DXVec3TransformCoord(&vTo, &vTo, &XSystem::m_mViewInv);
					D3DXVECTOR3 vPick;
					if( XEngine::Pick(&vPick, vFrom, vTo) && XCollision::m_pModel) 
					{
						CGameTarget* pTarget = NULL;
//                      EXPL:마우스가 움직이면 이미 타켓 캐취 시작!!!!
						pTarget = CGame_Character::FindFromPtr( XCollision::m_pModel);

//	#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06 // 대포 시스템 추가 연습
						if ( pTarget && !(pTarget->m_n64MState & CMS_HIDE || pTarget->m_n64MState & CMS_HIDE_CANNON) && pTarget->m_pCharaterName) 
//	#else
//						if ( pTarget && !(pTarget->m_n64MState & CMS_HIDE) && pTarget->m_pCharaterName)
//	#endif
						{
							// 마우스 커서를 인덱스, -1인 경우는 바뀌지 않고 표시하지도 않는다.
							int nCur = KGameSys::GetMouseCursorIndex( pTarget);
							if( nCur == -1)
								break;

							if( KGameSys::ChkEventBoxMonster( pTarget))
								SetCursor( nCur);
							else
								SetCursor( nCur);

#ifdef DEF_SCENARIO3_2_SEA_20070105							// 시나리오 3-2 군주보호

							//if( KGameSys::CHkPrayerMonster( pTarget))
							//	SetCursor( nCur);
							//else
							//	SetCursor( nCur);

#endif
							if( KGameSys::ChkShowMonsterName( pTarget))
							{
								if( NULL == pTarget->m_pFont)
								{
//	#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06 // 대포 시스템 추가 연습
								if (pTarget->m_n64MState & CMS_HIDE || pTarget->m_n64MState & CMS_HIDE_CANNON)
//	#else
//								if (pTarget->m_n64MState & CMS_HIDE)
//	#endif
									{
										; // hide 상태일때는 머리이름을 표시하지 않는다
									}
									else if( pTarget->m_byKind == CK_MONSTER)
									{
										DWORD dwColor = KGameSys::GetMonsterNameColor( pTarget);

										if( KGameSys::ChkEventBoxMonster( pTarget)

#ifdef DEF_SCENARIO3
											|| (KGameSys::ChkScenario3TowerMonster(pTarget)|| KGameSys::ChkScenario3TowerMonster(pTarget, 0))	// 탑은 이름을 흰색으로 표시
#endif // DEF_SCENARIO3
											)
										{
											// 아이템상자 몬스터는 이름을 흰색으로 표시
											pTarget->SetTip( pTarget->m_pCharaterName, RGB( 255,255,255), 5000);
										}
									
#ifdef DEF_SCENARIO3_2_SEA_20070105							// 시나리오 3-2 군주보호
										else if( KGameSys::CHkPrayerMonster( pTarget) )
										{
											pTarget->SetTip( pTarget->m_pCharaterName, RGB( 0,255,0), 5000);
										}
#endif
										else
										{
											pTarget->m_dwNameColor = dwColor;
											//- sea 2006-01-13---------------------------------------------------------------------------------------
											/*
											if( ((CGameMonster*)pTarget)->m_nCodeNo == 100 && strlen( pTarget->m_pCharaterGuildName) > 0)
											>> 빠꾼거 
											else if( ((CGameMonster*)pTarget)->m_nCodeNo == 319 && strlen( ((CGameMonster*)pTarget)->m_pCharaterGuildName) > 0  && ((CGameMonster*)pTarget)->m_dwGState & NGS_SIEGEGUNSET))
											// 공성병기((CGameMonster*)pTarget)->m_nCodeNo == 455 )의 코드넘버를 체크해서 자신의 부가 아니면 다른 색깔로 보여준다. 
											// 단 설치된 상태에서만 공성병기에 부의이름을 표시하고 평상시에는 공성병기 이름을 표시한다. 
											*/ 
											// 깃발 몬스터는 깃발 부 이름을 표시한다.
											if( ((CGameMonster*)pTarget)->m_nCodeNo == 100 && strlen( pTarget->m_pCharaterGuildName) > 0)
												pTarget->SetTip( pTarget->m_pCharaterGuildName, dwColor, 5000);	
											else if( KGameSys::GetCannonID( ((CGameMonster*)pTarget)->m_nCodeNo ) && strlen( ((CGameMonster*)pTarget)->m_pCharaterGuildName) > 0 )
												pTarget->SetTip( pTarget->m_pCharaterGuildName, dwColor, 5000);
#ifdef DEF_SCENARIO3_2_SEA_20070105							// 시나리오 3-2 군주보호
											else if( ((CGameMonster*)pTarget)->m_nCodeNo == 369 || ((CGameMonster*)pTarget)->m_nCodeNo == 370 )
												pTarget->SetTip( pTarget->m_pCharaterGuildName, dwColor, 5000);
#endif 
											else											
												pTarget->SetTip( pTarget->m_pCharaterName, dwColor, 5000);
										}

									}
									else if( pTarget->m_byKind == CK_NPC)
									{
										// 점령전용 NPC중 깃발 밑에 지주는 표시 안함
										//- sea 2006-01-13---------------------------------------------------------------------------------------
										// 나중에 깃발 엔피씨의 m_nShape번호로 이름위에 부의 이름을 띄어 주면 된다 
										
										if( ((CGameNpc*)pTarget)->m_nShape != 16)
											pTarget->SetTip( pTarget->m_pCharaterName, pTarget->m_textColor, 5000);
									}
#ifdef DEF_NAMECOLOR_SEA_20060703
									/*else
									{
										DWORD dwColor = KGameSys::GetCharacterNameColor( pTarget);

										pTarget->m_dwNameColor = dwColor;
										pTarget->SetTip( pTarget->m_pCharaterName, dwColor, 5000);
									}*/
#else	// DEF_NAMECOLOR_SEA_20060703
#ifdef DEF_DEATH_MATCH_BLUEYALU_20070214 //비무대전모드: 상대팀의 이름표시(팀색깔)
									else if(pTarget->m_dwGState & CGS_SHOWDOWN_PLAYER)
									{
										if(pTarget->m_dwGState & CGS_SHOWDOWN_RED)
											pTarget->SetTip( pTarget->m_pCharaterName, KGameSys::TEXTCOLOR_RED, 5000);
										else if(pTarget->m_dwGState & CGS_SHOWDOWN_BLUE)
											pTarget->SetTip( pTarget->m_pCharaterName, KGameSys::TEXTCOLOR_BLUE, 5000);
									}
#endif // DEF_DEATH_MATCH_BLUEYALU_20070214
									else	// 캐릭터들의 팁(이름, 길드, 직급, 명예계급)을 Set --> 내게 보여지는 Target들의 툴팁
									{	
#ifdef DEF_GAME_OPTION_HONOR_OFF_BLUEYALU_20070126
										// 기존 코드(지워준다) - 상대방의 툴팁 붙이는 부분
										pTarget->SetTip( pTarget->m_pCharaterName, pTarget->m_textColor, 5000);
#endif // DEF_GAME_OPTION_HONOR_OFF_BLUEYALU_20070126

#ifdef DEF_GAME_OPTION_HONOR_BLUEYALU_20070126
										int SysNum;
										switch( pTarget->m_nHonorGrade )
										{
										case 0:
											SysNum = 1389; break;
										case 1:
											SysNum = 1390;	break;
										case 2:
											SysNum = 1391;	break;
										case 3:
											SysNum = 1392;	break;
										case 4:
											SysNum = 1393;	break;
										case 5:
											SysNum = 1394;	break;
										case 6:
											SysNum = 1395;	break;
										case 7:
											SysNum = 1396;	break;
										case 8:
											SysNum = 1397;	break;
										case 9:
											SysNum = 1398;	break;
										case 10:
											SysNum = 1399;	break;
										}

										if( SysNum < 1389 || SysNum > 1399 )
											SysNum = 1389;

										const char* HonorName = KMsg::Get(SysNum);										

										std::strstream line;

																				
										if(*(pTarget->m_pCharaterName) == NULL)
										{		
											line << pTarget->m_pCharaterName << std::ends;											
										}
										else
										{
											if( pTarget->m_nHonorGrade == 0)
											{
												line << "[" << pTarget->m_pCharaterName << "]" << std::ends;
											}
											else
												line << "[" << HonorName << " " << pTarget->m_pCharaterName << "]" << std::ends;
										}
										
										
										if (KSetting::op.m_nShow_honorGrade == 0 )
										{
											if(pTarget->m_nHonorOpt == 2)
											{
												pTarget->SetTip( pTarget->m_pCharaterName, pTarget->m_textColor, 5000);
											}
											else
                                                pTarget->SetTip( line.str(), pTarget->m_textColor, 5000);
										}
										else if ( KSetting::op.m_nShow_honorGrade == 1 )
										{
											pTarget->SetTip( pTarget->m_pCharaterName, pTarget->m_textColor, 5000);											
										}
										else if ( KSetting::op.m_nShow_honorGrade == 2 )
										{
											if(pTarget->m_nHonorOpt == 2)
											{
												pTarget->SetTip( pTarget->m_pCharaterName, pTarget->m_textColor, 5000);
											}
											else
												pTarget->SetTip( line.str(), pTarget->m_textColor, 5000);											
										}	
#endif // DEF_GAME_OPTION_HONOR_BLUEYALU_20070126
									}
#endif	// DEF_NAMECOLOR_SEA_20060703
									
								}
							}
						}
						else
						{
							CGameDropItem * pItem = CGame_Item::FindFromPtr( XCollision::m_pModel);
							if ( pItem && pItem->m_pItemInfo )
							{
								SetCursor(2);
								KGameSys::SetToolTip( KMsg::Get( KMsg::T_ITEMNAME, pItem->m_pItemInfo->m_nName));
							}
							else
							{
								SetCursor(0);
								 KGameSys::SetToolTip("");
							}
						}
					}
					else
					{
						SetCursor(0);
						KGameSys::SetToolTip("");
					}
				}
#else
				KGameSys::SetToolTip("");
#endif
				m_mousePos = pt;
			}
			break;
		case WM_MOUSEWHEEL:
			{
				POINT pt;
				pt.x = GET_X_LPARAM(lParam); 
				pt.y = GET_Y_LPARAM(lParam); 

				if( g_pMyD3DApp){
					ScreenToClient( CContext::m_hWnd, &pt);
					if (CContext::OnMouseWheel( pt, wParam))
						break;
				}

				short nFlags = LOWORD(wParam);    // key flags
				short zDelta = (short)HIWORD(wParam);    // wheel rotation
				float Speed = XSystem::m_fZoom/10;
				float camMaxDistance = 156;

				XSystem::m_fZoom -= (Speed*zDelta/WHEEL_DELTA);

#if	(!defined(_SOCKET_TEST) || defined(_MAP_TEST) || defined(_DEBUG) || defined(COSMO_TEST) || defined(_SDW_DEBUG))
				if (XSystem::m_dwFlags & XSystem::EDIT_MODE)
					camMaxDistance = 20000;
#endif

//#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06 // 대포 시스템 추가 연습
				if(CGame_Character::m_Master && CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON)
				{
					XSystem::m_fZoom = CANNON_ZOOM;
				}
				else
				{
					if (XSystem::m_fZoom < 16)
						XSystem::m_fZoom = 16;
					else if (XSystem::m_fZoom > camMaxDistance)
						XSystem::m_fZoom = camMaxDistance;
				}
//#endif	
				SetView();
			}
			break;

		case WM_LBUTTONDBLCLK:
			{
				POINT pt;
				pt.x = GET_X_LPARAM(lParam); 
				pt.y = GET_Y_LPARAM(lParam); 
				CContext::OnLButtonDblClk( pt, wParam);
				//SetCursor(m_hCursor[m_iCursorNo]);
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			{
				if( m_d3dpp.BackBufferWidth == 0 || m_d3dpp.BackBufferHeight == 0)
					break;

                // 현재 마우스의 위치를 얻는다!
				POINT pt;
				pt.x = GET_X_LPARAM(lParam); 
				pt.y = GET_Y_LPARAM(lParam);

#ifndef _SOCKET_TEST
				if( GameObject::m_iBone_Gubun[0] != 0)
					break;
#endif
				//SetCursor(m_hCursor[m_iCursorNo]);

				if (!KCommand::m_bHideUI)
				{
					if (g_cEvent.IsActive())
					{
						g_cEvent.OnLButtonDown( pt, wParam);
						break;
					}

                    // 게임내 윈도우 버튼 누름시 처리
					if ( msg == WM_LBUTTONDOWN )
					{
						if (CContext::OnLButtonDown(pt,wParam))	{break;}
					}
					else
					{
						if (CContext::OnRButtonDown(pt,wParam))	{break;}
					}				
				}
				
				if( !KGameSys::IsMovable())
					break;

				if( CGame_Character::m_bReturnSkill_DontMove)
					break;
					
				if( !XSystem::m_bInit)
					break;

                // 스킬 캐스트 상태를 체크한다.
				if( KGameSys::ChkRangeSkillCast( msg))
					break;
#ifdef DEF_SCENARIO3_BOMB
				if( KGameSys::ChkRangeBomb(msg))
					break;
#endif	//DEF_SCENARIO3_BOMB
#ifdef _SOCKET_TEST

				D3DXVECTOR2 vec2((pt.x * 2 + 1) / (float) m_d3dpp.BackBufferWidth - 1.0f,
					-(pt.y * 2 + 1) / (float) m_d3dpp.BackBufferHeight + 1.0f);
				D3DXVECTOR3 vFrom(vec2.x * XSystem::m_vNear.x, vec2.y * XSystem::m_vNear.y, XSystem::m_vNear.z);
				D3DXVECTOR3 vTo(vec2.x * XSystem::m_vFar.x, vec2.y * XSystem::m_vFar.y, XSystem::m_vFar.z);
				D3DXVec3TransformCoord(&vFrom, &vFrom, &XSystem::m_mViewInv);
				D3DXVec3TransformCoord(&vTo, &vTo, &XSystem::m_mViewInv);
	
				CGameTarget* pTarget = 0;
				XCollision::m_pModel = 0;
				D3DXVECTOR3 vPick;

				if ( XEngine::Pick(&vPick, vFrom, vTo) )
				{
                    // 충돌한 모델이 존재한다면
					if ( XCollision::m_pModel )
					{
                        // 캐릭터가 존재하고 대포상태라면                        
						if(!(CGame_Character::m_Master && CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON))
						{
							pTarget = CGame_Character::FindFromPtr(XCollision::m_pModel);
							if( pTarget && KGameSys::GetMouseCursorIndex( pTarget) == -1)
								pTarget = NULL;
						}
						else
						{
							pTarget = NULL;
						}
					}
				}
				else
					break;

                // 캐릭터 처리
				if( CGame_Character::m_Master)
				{
                    // 입력 시간 조정
					KGameSys::m_nButtonClickTime = GetTickCount();
					if ( msg == WM_LBUTTONDOWN && KGameSys::m_byPileSkill_TypeB_Cast == 1)
					{
                        // 타켓 설정
						if( pTarget)
						{
							KGameSys::m_byPileSkill_TypeB_Cast = 2;

							if ( pTarget->m_byKind == CK_MONSTER )	
								KGameSys::SelUID( pTarget->m_CharaterID, CK_MONSTER, KGameSys::SK_LBUTTON);			
							else if ( pTarget->m_byKind == CK_PLAYER )			
								KGameSys::SelUID( pTarget->m_CharaterID, CK_PLAYER, KGameSys::SK_LBUTTON);
							else if( pTarget->m_byKind == CK_NPC )
								KGameSys::SelUID( pTarget->m_CharaterID, CK_NPC, KGameSys::SK_LBUTTON);

							break;
						}
						else
						{
							KGameSys::m_byPileSkill_TypeB_Cast = 0;
						}
					}

					if( msg == WM_LBUTTONDOWN)
					{
//	#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06 // 대포 시스템 추가 연습
						if(!(CGame_Character::m_Master->m_n64MState & CMS_HIDE_CANNON))
						{
							CGame_Character::Reset_ComboSkill( 0);
							CGame_Character::Reset_PileSkill();
						}
/*	#else
						CGame_Character::Reset_ComboSkill( 0);
						CGame_Character::Reset_PileSkill();
	#endif*/
						if( CGame_Character::m_Master->m_byDashkSkillCast == 1)
							KGameSys::FreeData();
					}
							
                    // 콤보스킬 초기화
					else if( msg == WM_RBUTTONDOWN && pTarget)
						CGame_Character::Reset_ComboSkill( CGame_Character::m_bComboSkill ? CGame_Character::m_byUseComboSkill : -1);					
				}

#ifdef _DEBUG
				if (pTarget)
					CGame_Character::m_DebugID = pTarget->m_CharaterID;
#endif
				SHORT mchKey = ( ( 0xFFF0 & GetAsyncKeyState( VK_SHIFT)) ? 1 : 0);
				
				if ( msg == WM_LBUTTONDOWN )
				{
					KGameSys::OnLButtonDown( pTarget, vPick, mchKey);
				}
				else
				{
					if ( pTarget )
						KGameSys::OnRButtonDown(pTarget, vPick, mchKey);

				}
#else // _SOCKET_TEST 일경우

				if ( CGame_Character::m_Master && !CGame_Character::m_Master->m_iDie)
				{
					D3DXVECTOR2 vec2((pt.x * 2 + 1) / (float) m_d3dpp.BackBufferWidth - 1.0f,
						-(pt.y * 2 + 1) / (float) m_d3dpp.BackBufferHeight + 1.0f);
					D3DXVECTOR3 vFrom(vec2.x * XSystem::m_vNear.x, vec2.y * XSystem::m_vNear.y, XSystem::m_vNear.z);
					D3DXVECTOR3 vTo(vec2.x * XSystem::m_vFar.x, vec2.y * XSystem::m_vFar.y, XSystem::m_vFar.z);
					D3DXVec3TransformCoord(&vFrom, &vFrom, &XSystem::m_mViewInv);
					D3DXVec3TransformCoord(&vTo, &vTo, &XSystem::m_mViewInv);

					D3DXVECTOR3 vPick(0,0,0);
					XCollision::m_pModel = NULL;
					if ( XEngine::Pick(&vPick, vFrom, vTo) )
					{
						if (!(CGame_Character::m_Master->m_n64MState & CMS_STUN) && !(CGame_Character::m_Master->m_n64MState & CMS_SLEEP))
						{
							if ( msg == WM_LBUTTONDOWN && !(CGame_Character::m_Master->m_n64MState & CMS_MOVESTOP))
                            {
								CGame_Character::m_Master->SetMove(vPick);
                            }
//							else
//								CGame_Character::m_Master->ModelAttack(vPick);
						}
					}
				}
#endif //end _SOCKET_TEST가 아닐 경우
			}
			break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			{
				if( m_d3dpp.BackBufferWidth == 0 || m_d3dpp.BackBufferHeight == 0)
					break;

				POINT pt;
				pt.x = GET_X_LPARAM(lParam); 
				pt.y = GET_Y_LPARAM(lParam); 
				//SetCursor(m_hCursor[m_iCursorNo]);

				if ( msg == WM_LBUTTONUP )
				{
					if (CContext::OnLButtonUp(pt,wParam))	{break;}
				}
				else
				{
					if (CContext::OnRButtonUp(pt,wParam))	{break;}
				}
				break;
			}
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			{
				if( m_d3dpp.BackBufferWidth == 0 || m_d3dpp.BackBufferHeight == 0)
					break;

				POINT pt;

				pt.x = GET_X_LPARAM(lParam); 
				pt.y = GET_Y_LPARAM(lParam); 
				//SetCursor(m_hCursor[m_iCursorNo]);
				if ( msg == WM_MBUTTONDOWN )
					CContext::OnMButtonDown(pt,wParam);
				else
					CContext::OnMButtonUp(pt,wParam);
				break;
			}

    }
#ifndef _SOCKET_TEST
END_OF_MSGPROC:
#endif
	return CD3DApplication::MsgProc( hWnd, msg, wParam, lParam );
}

void CMyD3DApplication::SetClipPlane(const D3DXPLANE *pPlane)
{
	static D3DXMATRIX matProjTrans;

	if (pPlane)
	{
		m_waterClipPlane = *pPlane;

#if 0
		if (XSystem::m_caps.MaxUserClipPlanes > 0)
		{
			XSystem::m_pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0);
			XSystem::m_pDevice->SetClipPlane(0, (const float *)pPlane);
		}
		else
#endif
		{
			// user clip plane 이 지원안될 경우 투영행렬의 near clip plane 을 shearing 해서 넣는다
			D3DXMATRIX matClipProj;
			D3DXPLANE clipPlane;
			D3DXPLANE worldPlane;

			worldPlane = *pPlane;
			D3DXPlaneTransform(&clipPlane, &worldPlane, &XSystem::m_mViewProjInvTrans);
			if (clipPlane.d > 0)
				D3DXVec3Transform((D3DXVECTOR4 *)&clipPlane, (D3DXVECTOR3 *)&(-worldPlane), &XSystem::m_mViewProj);

			D3DXMatrixIdentity(&matClipProj);
			matClipProj(0, 2) = clipPlane.a;
			matClipProj(1, 2) = clipPlane.b;
			matClipProj(2, 2) = clipPlane.c;
			matClipProj(3, 2) = clipPlane.d;

			matClipProj = XSystem::m_mProj * matClipProj;

			XSystem::m_pDevice->SetTransform(D3DTS_PROJECTION, &matClipProj);
			matProjTrans = XSystem::m_mProjTrans;
			D3DXMatrixTranspose(&XSystem::m_mProjTrans, &matClipProj);
		}
	}
	else
	{
#if 0
		if (XSystem::m_caps.MaxUserClipPlanes > 0)
		{
			XSystem::m_pDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
		}
		else
#endif
		{
			XSystem::m_pDevice->SetTransform(D3DTS_PROJECTION, &XSystem::m_mProj);
			XSystem::m_mProjTrans = matProjTrans;
		}
	}
}

void CMyD3DApplication::BeginWaterView(int view)
{
	D3DXMATRIXA16	matView;
	D3DXPLANE		clipPlane;
	D3DXVECTOR3		eye, at;

	// 이전 뷰 백업
	m_mTmpView = XSystem::m_mView;

	if (view == WV_REFLECTION)
	{
		eye	= D3DXVECTOR3(m_vEye.x, g_waterHeight - (m_vEye.y - g_waterHeight), m_vEye.z);
		at	= D3DXVECTOR3(m_vAt.x, g_waterHeight - (m_vAt.y - g_waterHeight), m_vAt.z);
		clipPlane = D3DXPLANE(0, 1, 0, -g_waterHeight);
	}
	else
	{
		eye	= m_vEye;
		at	= m_vAt;
		clipPlane = D3DXPLANE(0, -1, 0, g_waterHeight);
	}
	
	D3DXMatrixLookAtLH((D3DXMATRIX*)&matView, (D3DXVECTOR3*)&eye, (D3DXVECTOR3*)&at, (D3DXVECTOR3*)&m_vUp);
	
    m_pd3dDevice->SetTransform(D3DTS_VIEW, &matView);
	XSystem::SetViewTransform((D3DXMATRIX)matView);
	
	SetClipPlane(&clipPlane);
}

void CMyD3DApplication::EndWaterView()
{
	SetClipPlane(NULL);

	m_pd3dDevice->SetTransform(D3DTS_VIEW, &m_mTmpView);
	XSystem::SetViewTransform((D3DXMATRIX)m_mTmpView);
}

static void WriteTGA(const char *filename,const unsigned char *src,int width,int height)
{
#pragma pack(1)
	typedef struct tagTGAFILEHEADER
	{
		BYTE idLength;
		BYTE ColorMapType;
		BYTE ImageType;
		WORD ColorMapFirst;
		WORD ColorMapLast;
		BYTE ColorMapBits;
		WORD FirstX;
		WORD FirstY;
		WORD Width;
		WORD Height;
		BYTE Bits;
		BYTE Descriptor;
	}TGAFILEHEADER;
#pragma pack()

	TGAFILEHEADER	tgaHeader;
	DWORD			dwBytesWritten;
	HANDLE			hFile;
	int				i;

	int HeadSize = sizeof(tgaHeader);
	int BitsSize = width * height * 3;

	hFile=CreateFile(filename,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	tgaHeader.idLength      = 0;
	tgaHeader.ColorMapType  = 0;
	tgaHeader.ImageType     = 2;
	tgaHeader.ColorMapFirst = 0;
	tgaHeader.ColorMapLast  = 0;
	tgaHeader.ColorMapBits  = 0;
	tgaHeader.FirstX        = 0;
	tgaHeader.FirstY        = 0;
	tgaHeader.Width         = width;
	tgaHeader.Height        = height;
	tgaHeader.Bits          = 24;
	tgaHeader.Descriptor    = 8;

	WriteFile(hFile,&tgaHeader,HeadSize,&dwBytesWritten,NULL);

	for (i=height-1; i >= 0; i--)
		WriteFile(hFile,src + width * i * 3,width * 3,&dwBytesWritten,NULL);

	CloseHandle(hFile);
	return;
}

static void WriteJPG(const char *filename,const unsigned char *src,int width,int height)
{
	JPEG_CORE_PROPERTIES	jcp;

	memset(&jcp,0,sizeof(jcp));

	if (ijlInit(&jcp)!=IJL_OK)
		return;

	// Setup DIB
	jcp.DIBWidth = width;
	jcp.DIBHeight = height;
	jcp.DIBPadBytes = IJL_DIB_PAD_BYTES(jcp.DIBWidth,3);
	jcp.DIBChannels = 3;
	jcp.DIBColor = IJL_BGR;
	jcp.DIBBytes = (unsigned char *)src;

	// Setup JPEG
	jcp.JPGFile = filename;
	jcp.JPGWidth = width;
	jcp.JPGHeight = height;

	jcp.JPGChannels = 3;
	jcp.JPGColor = IJL_YCBCR;
	jcp.JPGSubsampling = IJL_411;

	jcp.jquality = 75;

	ijlWrite(&jcp,IJL_JFILE_WRITEWHOLEIMAGE);

	ijlFree(&jcp);
}

IDirect3DSurface9 *CMyD3DApplication::GetBackBufferImageSurface(D3DFORMAT *format)
{
	IDirect3DSurface9	*pSurface;
	IDirect3DSurface9	*pBackBuffer;
	IDirect3DSurface9	*pTmpSurface = NULL;
	D3DSURFACE_DESC		desc;
	HRESULT				hr;

	hr = m_pd3dDevice->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&pBackBuffer);
	if (FAILED(hr))
		return NULL;

	hr = pBackBuffer->GetDesc(&desc);
	if (FAILED(hr))
	{
		pBackBuffer->Release();
		return NULL;
	}

	// multisample 이 켜져있다면 백버퍼의 내용을 GetRenderTargetData 함수로 가져올수 없다
	if (m_d3dpp.MultiSampleType != D3DMULTISAMPLE_NONE)
	{
		hr = m_pd3dDevice->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,TRUE,&pTmpSurface,NULL);
		if (FAILED(hr))
		{
			pBackBuffer->Release();
			return NULL;
		}

		hr = m_pd3dDevice->StretchRect(pBackBuffer,NULL,pTmpSurface,NULL,D3DTEXF_NONE);
		if (FAILED(hr))
		{
			pBackBuffer->Release();
			if (pTmpSurface)
				pTmpSurface->Release();
			return NULL;
		}

		pBackBuffer->Release();
		pBackBuffer = pTmpSurface;
	}

	// 백버퍼와 동일한 포맷의 시스템 메모리 오프스크린 서피스 생성
	hr = m_pd3dDevice->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&pSurface,NULL);
	if (FAILED(hr))
	{
		pBackBuffer->Release();
		return NULL;
	}

	// 백버퍼의 내용을 복사
	hr = m_pd3dDevice->GetRenderTargetData(pBackBuffer,pSurface);
	if (FAILED(hr))
	{
		pBackBuffer->Release();
		pSurface->Release();
		return NULL;
	}	

	pBackBuffer->Release();

	*format = desc.Format;

	return pSurface;
}

bool CMyD3DApplication::TakeScreenShotJpeg(const char *filename)
{
	LPDIRECT3DSURFACE9	surface;
	D3DLOCKED_RECT		lockedRect;
	unsigned char		*data;
	unsigned char		*src, *dest;
//	POINT				topLeft;
//	unsigned int		screenWidth, screenHeight;
	unsigned int		x, y;
	D3DFORMAT			fmt;

	if (!m_pd3dDevice)
		return false;
/*	
	topLeft.x = 0;
	topLeft.y = 0;
	ClientToScreen(m_hWnd,&topLeft);

	screenWidth = GetSystemMetrics(SM_CXSCREEN);
	screenHeight = GetSystemMetrics(SM_CYSCREEN);

	// 창 모드일 경우 데스크탑 화면 전체가 카피된다.
	m_pd3dDevice->CreateOffscreenPlainSurface(screenWidth,screenHeight,D3DFMT_A8R8G8B8,D3DPOOL_SCRATCH,&surface,NULL);
	if (m_pd3dDevice->GetFrontBufferData(0,surface) != D3D_OK)
	{
		surface->Release();
		return false;
	}*/

	surface = GetBackBufferImageSurface(&fmt);
	if (!surface)
		return false;

	memset(&lockedRect,0,sizeof(lockedRect));
	if (surface->LockRect(&lockedRect,NULL,D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK) != D3D_OK)
	{
		surface->Release();
		return false;
	}

	data = new unsigned char [m_d3dpp.BackBufferWidth * m_d3dpp.BackBufferHeight * 3];
	dest = data;
	for (y = 0; y < m_d3dpp.BackBufferHeight; y++)
	{
		src = ((unsigned char *)lockedRect.pBits) + (y * lockedRect.Pitch);
		for (x = 0; x < m_d3dpp.BackBufferWidth; x++)
		{
			dest[0] = src[0];
			dest[1] = src[1];
			dest[2] = src[2];
			dest += 3;
			src += 4;
		}
	}

	surface->UnlockRect();
	
	//WriteTGA(filename,data,m_d3dpp.BackBufferWidth,m_d3dpp.BackBufferHeight);
	WriteJPG(filename,data,m_d3dpp.BackBufferWidth,m_d3dpp.BackBufferHeight);

	delete[] data;
	
	surface->Release();

	return true;
}

void CMyD3DApplication::EventEarthQuake(const D3DXVECTOR3 &origin, float intensity, float time)
{ 
	evQuake_t *ev = new evQuake_t;
	ev->frametime = 0.0f;
	ev->timer = time;
	ev->intensity = intensity;
	ev->origin = origin;
	m_eventQuakeList.push_back(ev);
}

void CMyD3DApplication::SetFrameEarthQuake( BOOL setearth, float intensity, float time )
{
	if(setearth == TRUE)
	{
        m_bEarthQuake = setearth;
		m_bGEarthQuake = setearth;
        m_fEarthQuakeIntensity = intensity;
		earthtime = time;
	}
}

void CMyD3DApplication::CalEarthQuakeTime( float frametime )
{
	if(m_bGEarthQuake == TRUE)
	{
        earthtime -= frametime;		
          
		if ( earthtime > 0.0f)
		{
			return;
		}
		else
		{
			m_bEarthQuake = FALSE;
		}
	}
}

void CMyD3DApplication::ProcessEarthQuakeEvent(float frametime)
{
	std::list<evQuake_t *>::iterator it;
	evQuake_t *ev;
	
	m_fEarthQuakeEventIntensity = 0.0f;

	for (it = m_eventQuakeList.begin(); it != m_eventQuakeList.end(); )
	{
		ev = *it++;

		ev->frametime += frametime;
		if (ev->frametime > ev->timer)
		{
			m_eventQuakeList.remove(ev);
			delete ev;
			continue;
		}

		float curIntensity = (ev->timer - ev->frametime) / ev->timer;

		D3DXVECTOR3 d = XSystem::m_vCamera - ev->origin;
		float distance = D3DXVec3Length(&d);
		float radius = ev->intensity * 100.0f;
		if (radius > distance)
		{
			curIntensity = curIntensity * (radius - distance) / radius;
			if (curIntensity > m_fEarthQuakeEventIntensity)
				m_fEarthQuakeEventIntensity = curIntensity;
		}
	}

	if (m_fEarthQuakeEventIntensity > 0.0f)
		m_bEarthQuake = TRUE;	
}

void CMyD3DApplication::SetDialogBoxMode( BOOL bEnableDialogs)
{
	if( m_pd3dDevice == NULL) 
		return;

	if( bEnableDialogs) {
		if( !m_bDialogBoxMode) {
			// See if the current settings comply with the rules
			// for allowing SetDialogBoxMode().  
			if( (m_d3dpp.BackBufferFormat == D3DFMT_X1R5G5B5 || m_d3dpp.BackBufferFormat == D3DFMT_R5G6B5 || m_d3dpp.BackBufferFormat == D3DFMT_X8R8G8B8 ) &&
				( m_d3dpp.MultiSampleType == D3DMULTISAMPLE_NONE ) &&
				( m_d3dpp.SwapEffect == D3DSWAPEFFECT_DISCARD ) &&
				( (m_d3dpp.Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER) == D3DPRESENTFLAG_LOCKABLE_BACKBUFFER ) &&
				( (m_dwCreateFlags & D3DCREATE_ADAPTERGROUP_DEVICE) != D3DCREATE_ADAPTERGROUP_DEVICE ) )
			{
				if( SUCCEEDED( m_pd3dDevice->SetDialogBoxMode( true ) ) )
					m_bDialogBoxMode = true;
			}
		}
	} else {
		if( m_bDialogBoxMode) {
			m_pd3dDevice->SetDialogBoxMode( false );
			m_bDialogBoxMode = false;
		}
	}
}

class XCursorMsg : public XMessage
{
public:
	void	OnTimer();
};

void	XCursorMsg::OnTimer()
{
	if (g_pMyD3DApp->m_nCursorPrev != g_pMyD3DApp->m_nCursor) {
		g_pMyD3DApp->m_nCursorPrev = g_pMyD3DApp->m_nCursor;
		::SetCursor(g_pMyD3DApp->m_hCursor[g_pMyD3DApp->m_nCursor]);
	}

}

static XCursorMsg CursorMsg;

void	CMyD3DApplication::SetCursor(int nCursor)
{
	// Playmacro hooks SetCursor function, so I decide to postpone that 
	if (m_nCursorPrev == m_nCursor) {
		if (m_nCursorPrev != nCursor) {
			DWORD dwTick = GetTickCount();
			if ((int)(dwTick - m_dwStartTick) >= 0) {
				m_nCursor = nCursor;
				CursorMsg.FgAddTimer((XMessage::FUNCTION) XCursorMsg::OnTimer, 200);				
			}
			else {
				m_nCursorPrev = m_nCursor = nCursor; // do immediately
				::SetCursor(m_hCursor[nCursor]);
			}
		}
	}
	else {
		m_nCursor = nCursor;
		if (m_nCursorPrev == m_nCursor)
			CursorMsg.FgCancelTimer();
	}
}

#ifdef DEF_HSHIELD_KENSHIN_2006_02_01 //핵쉴드 설치
int __stdcall AhnHS_Callback(long lCode, long lParamSize, void* pParam)
{
	#ifdef DEF_HACKSHIELD_DEBUGVERSION_OFF							// 핵쉴드 사용중 디버깅을 하고 싶다면 이것을 주석 처리할 것!
	switch(lCode)
	{
		//Engine Callback
		case AHNHS_ENGINE_DETECT_GAME_HACK:
		{
			g_Check_Hack = 1;
			KWindowCollector::CloseAll();
			KWindowCollector::GameExit();
			//TCHAR szMsg[255];
			//wsprintf(szMsg, _T("다음 위치에서 해킹툴이 발견되어 프로그램을 종료시켰습니다.\n%s"), (char*)pParam);
			//MessageBox(g_MainHWND, szMsg, "KalOnline", MB_OK);
			if(g_MainHWND)
			{
				MessageBox(g_MainHWND, KMsg::Get( 1237), "KalOnline", MB_OK);
			}
			else
			{
				MessageBox(NULL, KMsg::Get( 1237), "KalOnline", MB_OK);
			}

			PostThreadMessage(g_dwMainThreadID, WM_QUIT, 0, 0);
			break;
		}

		//일부 API가 이미 후킹되어 있는 상태
		//그러나 실제로는 이를 차단하고 있기 때문에 다른 후킹시도 프로그램은 동작하지 않습니다.
		//이 Callback은 단지 경고 내지는 정보제공 차원에서 제공되므로 게임을 종료할 필요가 없습니다.
		case AHNHS_ACTAPC_DETECT_ALREADYHOOKED:
		{
			PACTAPCPARAM_DETECT_HOOKFUNCTION pHookFunction = (PACTAPCPARAM_DETECT_HOOKFUNCTION)pParam;
			TCHAR szMsg[255];
			wsprintf(szMsg, _T("[HACKSHIELD] Already Hooked\n- szFunctionName : %s\n- szModuleName : %s\n"), 
				pHookFunction->szFunctionName, pHookFunction->szModuleName);
			OutputDebugString(szMsg);
			break;
		}

		//Speed 관련
		case AHNHS_ACTAPC_DETECT_SPEEDHACK:
		case AHNHS_ACTAPC_DETECT_SPEEDHACK_APP:
		{
			g_Check_Hack = 1;
			KWindowCollector::CloseAll();
			KWindowCollector::GameExit();
			//MessageBox(g_MainHWND, _T("현재 이 PC에서 SpeedHack으로 의심되는 동작이 감지되었습니다."), "KalOnline", MB_OK);
			if(g_MainHWND)
			{
				MessageBox(g_MainHWND, KMsg::Get( 1238), "KalOnline", MB_OK);
			}
			else
			{
				MessageBox(NULL, KMsg::Get( 1238), "KalOnline", MB_OK);
			}

			PostThreadMessage(g_dwMainThreadID, WM_QUIT, 0, 0);
			break;
		}

		//디버깅 방지 
		case AHNHS_ACTAPC_DETECT_KDTRACE:	
		case AHNHS_ACTAPC_DETECT_KDTRACE_CHANGED:
		{
			g_Check_Hack = 1;
			KWindowCollector::CloseAll();
			KWindowCollector::GameExit();
			//TCHAR szMsg[255];
			//wsprintf(szMsg, _T("프로그램에 대하여 디버깅 시도가 발생하였습니다. (Code = %x)\n프로그램을 종료합니다."), lCode);
			//MessageBox(g_MainHWND, szMsg, "KalOnline", MB_OK);
			if(g_MainHWND)
			{
				MessageBox(g_MainHWND, KMsg::Get( 1239), "KalOnline", MB_OK);
			}
			else
			{
				MessageBox(NULL, KMsg::Get( 1239), "KalOnline", MB_OK);
			}

			PostThreadMessage(g_dwMainThreadID, WM_QUIT, 0, 0);
			break;
		}

		//그외 해킹 방지 기능 이상 
		case AHNHS_ACTAPC_DETECT_AUTOMOUSE:
		case AHNHS_ACTAPC_DETECT_DRIVERFAILED:
		case AHNHS_ACTAPC_DETECT_HOOKFUNCTION:
		case AHNHS_ACTAPC_DETECT_MESSAGEHOOK:
		case AHNHS_ACTAPC_DETECT_MODULE_CHANGE:
		{
			g_Check_Hack = 1;
			KWindowCollector::CloseAll();
			KWindowCollector::GameExit();
			//TCHAR szMsg[255];
			//wsprintf(szMsg, _T("해킹방지 기능에 이상이 발생하였습니다. (Code = %x)\n프로그램을 종료합니다."), lCode);
			//MessageBox(g_MainHWND, szMsg, "KalOnline", MB_OK);
			if(g_MainHWND)
			{
				MessageBox(g_MainHWND, KMsg::Get( 1236), "KalOnline", MB_OK);
			}
			else
			{
				MessageBox(NULL, KMsg::Get( 1236), "KalOnline", MB_OK);
			}

			PostThreadMessage(g_dwMainThreadID, WM_QUIT, 0, 0);
			break;
		}
	}

	if(g_Check_Hack)
	{
		KWindowCollector::CloseAll();
		KWindowCollector::GameExit();
	}
	#endif

	return 1;
}
#endif
