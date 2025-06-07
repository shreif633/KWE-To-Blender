/************************************************************************/
/* ���ϸ� : WorldMap.cpp												*/
/* ��  �� : ��ü �� Ŭ���� ����											*/
/************************************************************************/
#include "StdAfx.h"
#include "WorldMap.h"
#include "../roam.h"

#ifdef DEF_UI_CONTENTS_KUMADORI_090227

//--------------------------------------------------------------------------------------------------------------//
// class name : WorldMap Ŭ���� ����
//--------------------------------------------------------------------------------------------------------------//
void WorldMap::DrawTexture( int x, int y, DWORD dwColor, CTexture* pTexture, float fHalfsize, float fRotation) 
{
	int size = m_sizeWindow.cx;
	POINT pt = { x, y};
	RECT clip = { m_ptWindow.x + BORDERX, m_ptWindow.y + BORDERY, m_ptWindow.x + BORDERX + size, m_ptWindow.y + BORDERY + size};
	InflateRect( &clip, 1, 1);
	if( !PtInRect( &clip, pt))
		return;

	if( NULL == pTexture)
		return;

	FillRectWithD3D( 
		(float) x - fHalfsize, 
		(float) y - fHalfsize, 
		(float) x + fHalfsize, 
		(float) y + fHalfsize, 
		dwColor, 
		pTexture->m_pTexture, 
		fRotation);
}

void WorldMap::OnInit()
{
	SetCapture();
	Control::CContext::LockCapture();
	KCommand::m_bShowWorldMap = TRUE;
	InvalidateRect();
}

void WorldMap::OnCreate()
{
	XSystem::SetPath( 0, "Data\\HyperText\\");
	m_pHero = CTexture::Load( "mmaphero.GTX", CTexture::NO_MIPMAP);
	m_pCamera = CTexture::Load( "camera.GTX", CTexture::NO_MIPMAP);
}

void WorldMap::OnDelete() 
{
	SAFE_RELEASE( m_pHero);
	SAFE_RELEASE( m_pCamera);

	ReleaseCapture();
	Control::CContext::UnlockCapture();

	KGameSys::ShowMapSubMenu( 0);
	KCommand::m_bShowWorldMap = FALSE;
	KWindowCollector::HideAll( TRUE, NULL);
}

void WorldMap::OnPaint() 
{
	MoveButton();

	PaintWithDC();
	if( CGame_Character::m_Master) {
		KWorldMapSystem::DrawWorldMap();

		const float rate		= 0.5f;		// �̴ϸ� �� �ȼ� / CELL_SIZE
		const int	map_left	= 30;		// �̴ϸ� �̹��� �ҽ��� ���� �� �ش��ϴ� ��ġ ��ǥ
		const int	map_bottom	= 26;		// �̴ϸ� �̹��� �ҽ��� �Ʒ��� �� �ش��ϴ� ��ġ ��ǥ

		int nXOnImg = int( float( MAP_SIZE * PATCH_SIZE / 2 + ( (int) CGame_Character::m_Master->m_vPosi.x/ CELL_SIZE) - map_left * MAP_SIZE ) * rate / 4);
		int nYOnImg = int( float( MAP_SIZE * PATCH_SIZE / 2 + ( (int) CGame_Character::m_Master->m_vPosi.z/ CELL_SIZE) - map_bottom * MAP_SIZE ) * rate / 4);

		float fWidth = 800;
		float fHeight = 600;
		float fMiniMapSize = 768;
		float fAddY = float(KWorldMapSystem::nY);

		// �ػ󵵿� ���� ��ǥ ����
		if ( KSetting::op.m_nResolutionWidth == 1024)
		{
			fWidth = fWidth * 1.28f;
			fMiniMapSize = fMiniMapSize * 1.28f;
			nXOnImg = int( float(nXOnImg) * 1.28f);
		}
		else if ( KSetting::op.m_nResolutionWidth == 1280)
		{
			fWidth = fWidth * 1.6f;
			fMiniMapSize = fMiniMapSize * 1.6f;
			nXOnImg = int( float(nXOnImg) * 1.6f);
		}

		if ( KSetting::op.m_nResolutionHeight == 600)
		{
			fHeight = (fHeight - fAddY);
		}
		else if ( KSetting::op.m_nResolutionHeight == 768)
		{
			fHeight = (fHeight - fAddY) * 1.28f;
			nYOnImg = int( float(nYOnImg) * 1.28f);
		}
		else if ( KSetting::op.m_nResolutionHeight == 960)
		{
			fHeight = (fHeight - fAddY) * 1.6f;	
			nYOnImg = int( float(nYOnImg) * 1.6f);
		}
		else if ( KSetting::op.m_nResolutionHeight == 1024)
		{
			fHeight = (fHeight - fAddY) * 1.7f;
			nYOnImg = int( float(nYOnImg) * 1.7f);
		}

		nXOnImg += int( fWidth / 2 - float( fMiniMapSize / 8));				// X��ġ ����
		nYOnImg = (int)fHeight -nYOnImg;									// Y��ġ ����

		// ī�޶� ����
		DrawTexture( nXOnImg, nYOnImg, D3DCOLOR_ARGB( 255, 255, 255, 255), m_pCamera, 20, XSystem::m_fYaw);
		// ���ΰ� ����
		DrawTexture( nXOnImg, nYOnImg, D3DCOLOR_ARGB( 255, 255, 255, 255), m_pHero, TEXTUREHALFSIZE*2, CGame_Character::m_Master->m_Rotate + D3DX_PI);			
	}
}

void WorldMap::MoveButton()
{
	HyperElement::CButton* pButton = (HyperElement::CButton*) FindElement( "close");
	if( pButton)
	{
		// �ػ󵵿� ���� ��ǥ ����
		float fX = 13;
		float cx = (float)pButton->m_size.cx;
		float cy = (float)pButton->m_size.cy;
		if ( KSetting::op.m_nResolutionWidth == 1024)
		{
			fX = fX * 1.28f;
			cx = cx * 1.28f; 
		}
		else if ( KSetting::op.m_nResolutionWidth == 1280)
		{
			fX = fX * 1.6f;
			cx = cx * 1.6f;
		}

		if ( KSetting::op.m_nResolutionHeight == 768)
			cy = cy * 1.28f;
		else if ( KSetting::op.m_nResolutionHeight == 960)	
			cy = cy * 1.6f;		
		else if ( KSetting::op.m_nResolutionHeight == 1024)
			cy = cy * 1.7f;

		pButton->m_pt.x =  KWindowCollector::GetViewWidth() - (int)fX - (int)cx;
		pButton->m_pt.y =  KWindowCollector::GetViewHeight() - (int)cy;			
	}
}

#endif