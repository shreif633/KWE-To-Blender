#pragma once

namespace KSunLight
{
	D3DXCOLOR GetColor( );
	void FrameMove( DWORD dwElapsed);
	void SetTime( WORD wTime);
	void GetTime( WORD& wHour, WORD& wMinute);
	
	void SetHour( int hour);

	typedef struct _stSunLightKey
	{
		BOOL bKey;
		D3DXCOLOR color;
	} SUNLIGHTKEY;

	extern SUNLIGHTKEY m_Table[ 24];
	extern DWORD m_dwColor;
	extern DWORD m_dwColorSky;
	extern D3DXCOLOR m_cColor;
}