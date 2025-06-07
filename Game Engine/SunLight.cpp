#include"stdafx.h"
#include"SunLight.h"
#include <D3DX9.h>
#include "XSystem.h"
#include "KGameSys.h"
#include "KGameSysex.h"

//#define _SPEED_FACTOR_ 60000		// 실제시간보다 6000배 빠르게
#define _SPEED_FACTOR_ 6		// 실제시간보다 6배 빠르게

#define _1_MINUTE_ ( 60 * 1000 / _SPEED_FACTOR_)
#define _1_HOUR_ ( _1_MINUTE_ * 60)
#define _24_HOURS_ ( _1_HOUR_ * 24)

namespace KSunLight
{
	SUNLIGHTKEY m_Table[ 24];
	DWORD m_dwTime = 12 * _1_HOUR_;
	DWORD m_dwColor = 0xFFFFFFFF;
	DWORD m_dwColorSky = 0xFFFFFFFF;
	D3DXCOLOR	m_cColor;
};

D3DXCOLOR KSunLight::GetColor()
{
	if( KGameSys::InGamePlay())
	{
		ldiv_t ret = ldiv( m_dwTime, _1_HOUR_);
		ASSERT( ret.quot < 24);

		DWORD from, to;
		float rate;

		if( ret.quot < 24)
		{
			rate = float( ret.rem) / float( _1_HOUR_);
			from = ret.quot;
			if( ret.quot == 23)
				to = 0;
			else
				to = ret.quot + 1;

			// TRACE( "현재 게임시간 %d 시\n", to);

			ASSERT( 0 <= from && from < 24 && 0 <= to && to < 24);
			D3DXCOLOR CurColor = m_Table[ from].color * ( 1 - rate ) + m_Table[ to].color * rate;

			if (g_bOverBright)
			{
				CurColor.r *= 0.5f;
				CurColor.g *= 0.5f;
				CurColor.b *= 0.5f;
			}

			return CurColor;
		}
	}

	if (g_bOverBright)
		return D3DXCOLOR(0.5f,0.5f,0.5f,1.f);
	return D3DXCOLOR(1.f,1.f,1.f,1.f);
}

void KSunLight::FrameMove( DWORD dwElapsed)
{
#ifdef _SUN_LIGHT
	m_dwTime += dwElapsed;
	m_dwTime %= _24_HOURS_;

	D3DXCOLOR color = GetColor();
	m_cColor = color;
	
	m_dwColor = color;
	
	float fAvr = ( m_cColor.r + m_cColor.g + m_cColor.b) / 3.0f;
	fAvr *= fAvr;
	color.r *= fAvr;
	color.g *= fAvr;
	color.b *= fAvr;
	m_dwColorSky = color;
	
	XSystem::m_material.Ambient = D3DXCOLOR(m_cColor.r * 0.5f, m_cColor.r * 0.5f, m_cColor.r * 0.5f, 1.0f);
	XSystem::m_material.Diffuse = m_cColor;
	//XSystem::m_pDevice->SetMaterial( &XSystem::m_material);
#endif
}

void KSunLight::SetTime( WORD wTime)
{
	m_dwTime = ( wTime >> 8) * _1_HOUR_ + ( wTime & 0xFF) * _1_MINUTE_;
	m_dwTime *= _SPEED_FACTOR_;
}

void KSunLight::SetHour( int hour)
{
	m_dwTime = hour * _1_HOUR_;
}

void KSunLight::GetTime( WORD& wHour, WORD& wMinute)
{
	m_dwTime %= _24_HOURS_;

	wHour = int( m_dwTime / _1_HOUR_);
	wMinute = int( ( m_dwTime % _1_HOUR_) / _1_MINUTE_ );
}