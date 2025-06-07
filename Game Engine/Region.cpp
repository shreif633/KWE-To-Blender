
#include "StdAfx.h"
#include ".\Region.h"
#include "XSystem.h"
#include "gamedata/GameCharacter.h"
#include "gamedata/GameUser.h"
#include "kgamesysex.h"

const float Region::ENV_BLENDING_TIME = 2.f;
const float Region::INDICATOR_BLENDING_TIME = 3.f; 

Region::Region()
: m_bRestoreColor( false )
, m_pTexture( NULL )
{
}

Region::~Region()
{
	Delete();
}

bool Region::Init()
{
	// �ε��� ���� ���� - XFile�� �� ������ ���̱� ������ ���� xml�� ���̱� ���� ���� Init���� �ε�
	if ( !_LoadRegionInfo() )
	{
		return false;
	}

// 	if ( !_LoadRegionIndicator() )
// 	{
// 		return false;
// 	}

	return true;
}

void Region::Delete()
{
	SAFE_RELEASE( m_pTexture );
}

void Region::Update()
{
	if ( CGame_Character::m_Master != NULL )
	{
		const int nLocationX = REGION_SIZE * BASE_PATCH_X;
		const int nLocationY = REGION_SIZE * BASE_PATCH_Y;
		D3DXVECTOR3 vPos = CGame_Character::m_Master->m_vPosi;
		int nLoopCount = m_RegionEnv.size();

		for ( int i = 0; i < nLoopCount; ++i )
		{
			const int nGapX = ( BETWEEN_TOOL_AND_CLIENT_GAP_X * ( m_RegionEnv[ i ].sizeStandard.cx - BASE_PATCH_X ) );
			const int nGapY = ( BETWEEN_TOOL_AND_CLIENT_GAP_Y * ( m_RegionEnv[ i ].sizeStandard.cy - BASE_PATCH_Y ) );

			D3DXVECTOR3 vLeftFront = m_RegionEnv[ i ].vLF;
			D3DXVECTOR3 vRightBack = m_RegionEnv[ i ].vRB;

			vLeftFront.x = ( m_RegionEnv[ i ].vLF.x - nLocationX ) + nGapX;
			vLeftFront.y = ( m_RegionEnv[ i ].vLF.y - nLocationY ) + nGapY;
			vRightBack.x = ( m_RegionEnv[ i ].vRB.x - nLocationX ) + nGapX;
			vRightBack.y = ( m_RegionEnv[ i ].vRB.y - nLocationY ) + nGapY;

			int nIndex = m_RegionEnv[ i ].nIndex;

			if ( vLeftFront.x < vPos.x && vRightBack.x > vPos.x && vLeftFront.y < vPos.z && vRightBack.y > vPos.z )
			{
				// ó�� �ð� ����
				if ( !m_RegionEnv[ i ].bSelect )
				{
					m_RegionEnv[ i ].fCurrentTime = g_fCurrentTime;
				}

				m_RegionEnv[ i ].bSelect = true;

				// Index�� 0�̸� ȯ�氪 �ƴϸ� �����ε����̼�
				if ( nIndex == 0 )
				{
					_BeginEnv( i );
				}
				else
				{
					// 1. �������� �ѹ��� ���̰�
					// 2. �����̸� �ٽ� ������ �ʰ�
					// 3. ���ö� ����
					_RenderIndicator( i, nIndex );
				}
			}
			else
			{
				if ( nIndex == 0 )
				{
					if ( m_RegionEnv[ i ].bSelect )
					{
						_EndEnv( i );
					}
					else
					{
						m_RegionEnv[ i ].bSelect = false;
					}
				}
				else
				{
					if ( m_RegionEnv[ i ].m_eIndicator == S_END )
					{
						m_RegionEnv[ i ].m_eIndicator = S_STOP;
					}

					m_RegionEnv[ i ].bSelect = false;
				}
			}
		}
	}
}

void Region::RenderForDebug()
{
	if ( CGame_Character::m_Master == NULL )
	{
		return;
	}

	D3DXVECTOR3 vPos = CGame_Character::m_Master->m_vPosi;
 	const int nLocationX = REGION_SIZE * BASE_PATCH_X;
 	const int nLocationY = REGION_SIZE * BASE_PATCH_Y;
	int nLoopCount = m_RegionEnv.size();

	for ( int i = 0; i < nLoopCount; ++i )
	{
		D3DXVECTOR3 vLeftFront;
		D3DXVECTOR3 vRightBack;

		const int nGapX = ( BETWEEN_TOOL_AND_CLIENT_GAP_X * ( m_RegionEnv[ i ].sizeStandard.cx - BASE_PATCH_X ) );
		const int nGapY = ( BETWEEN_TOOL_AND_CLIENT_GAP_Y * ( m_RegionEnv[ i ].sizeStandard.cy - BASE_PATCH_Y ) );

		// ��ǥ�� Ʋ���� �������
		vLeftFront.x = ( m_RegionEnv[ i ].vLF.x - nLocationX ) + nGapX;
		vLeftFront.y = vPos.y - 500.f;
		vLeftFront.z = ( m_RegionEnv[ i ].vLF.y - nLocationY ) + nGapY;

		vRightBack.x = ( m_RegionEnv[ i ].vRB.x - nLocationX ) + nGapX;
		vRightBack.y = vPos.y + 500.f;
		vRightBack.z = ( m_RegionEnv[ i ].vRB.y - nLocationY ) + nGapY;

		XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
		XSystem::m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
		XSystem::m_pDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
		XSystem::m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
		XSystem::m_pDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
		XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
		XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

		XSystem::m_pDevice->SetFVF( D3DFVF_XYZ | D3DFVF_DIFFUSE );

		WORD Index[ 24 ];

		//ó��
		Index[ 0 ] = 0;		Index[ 1 ] = 1;		Index[ 2 ] = 0;		Index[ 3 ] = 2;	
		Index[ 4 ] = 2;		Index[ 5 ] = 3;		Index[ 6 ] = 1;		Index[ 7 ] = 3;

		//���
		Index[ 8 ] = 0;		Index[ 9 ] = 4;		Index[ 10] = 1;		Index[ 11] = 5;	
		Index[ 12 ] = 2;	Index[ 13 ] = 6;	Index[ 14] = 3;		Index[ 15] = 7;

		//��
		Index[ 16 ] = 4;	Index[ 17 ] = 5;	Index[ 18 ] = 4;	Index[ 19 ] = 6;	
		Index[ 20 ] = 6;	Index[ 21 ] = 7;	Index[ 22 ] = 5;	Index[ 23 ] = 7;

		D3DXVECTOR3 vtemp[ 8 ];
		SIZE_T VertexCount = 0;

		vtemp[ 0 ].x = vLeftFront.x;	vtemp[ 0 ].y = vLeftFront.y;	vtemp[ 0 ].z = vRightBack.z;
		vtemp[ 1 ].x = vLeftFront.x;	vtemp[ 1 ].y = vLeftFront.y;	vtemp[ 1 ].z = vLeftFront.z;
		vtemp[ 2 ].x = vRightBack.x;	vtemp[ 2 ].y = vLeftFront.y;	vtemp[ 2 ].z = vRightBack.z;
		vtemp[ 3 ].x = vRightBack.x;	vtemp[ 3 ].y = vLeftFront.y;	vtemp[ 3 ].z = vLeftFront.z;
		vtemp[ 4 ].x = vLeftFront.x;	vtemp[ 4 ].y = vRightBack.y;	vtemp[ 4 ].z = vRightBack.z;
		vtemp[ 5 ].x = vLeftFront.x;	vtemp[ 5 ].y = vRightBack.y;	vtemp[ 5 ].z = vLeftFront.z;
		vtemp[ 6 ].x = vRightBack.x;	vtemp[ 6 ].y = vRightBack.y;	vtemp[ 6 ].z = vRightBack.z;
		vtemp[ 7 ].x = vRightBack.x;	vtemp[ 7 ].y = vRightBack.y;	vtemp[ 7 ].z = vLeftFront.z;

		XSystem::m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFF0000 );
		XSystem::m_pDevice->DrawIndexedPrimitiveUP( D3DPT_LINELIST, 0, 8, 12, &Index, D3DFMT_INDEX16, &vtemp, sizeof(D3DXVECTOR3) );
	}

	XSystem::m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFFFFFF );
//	XSystem::m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
//	XSystem::m_pDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
}

void Region::_RenderIndicator( int nIndex, int nTexIndex )
{
	// ���� ó��
	if ( m_RegionEnv[ nIndex ].m_eIndicator == S_END )
	{
		return;
	}

	m_RegionEnv[ nIndex ].m_eIndicator = S_BEGIN;

	// ���� �н� �����
	std::string strTable = "UI-zone_";

	if ( KGameSys::m_szLocale == "k" )
	{
		strTable += "k";
	}
	else if ( KGameSys::m_szLocale == "e" )
	{
		strTable += "e";
	}
	else if ( KGameSys::m_szLocale == "c" )
	{
		strTable += "c";
	}

	char szTemp[ MAX_PATH ] = { 0, };
	sprintf( szTemp, "%d", nTexIndex );

	strTable += szTemp;
	strTable += ".bmp";

	// �ؽ�ó �ε�
	if ( m_pTexture == NULL )
	{
		m_pTexture = CTexture::LoadFullPath( ( "data\\UI\\" + strTable ).c_str(), CTexture::NO_CACHE | CTexture::NO_MIPMAP );
	}

	if ( m_pTexture == NULL )
	{
		return;
	}

	float fTime = g_fCurrentTime - m_RegionEnv[ nIndex ].fCurrentTime;
	if ( fTime < INDICATOR_BLENDING_TIME )
	{
		BYTE byA = 0;

		if ( fTime <= 1.f )
		{
			byA = (BYTE)( 255 * fTime );
		}
		else if ( fTime <= 2.f )
		{
			byA = 255;
		}
		else
		{
			byA = (BYTE)( 255 * ( 3.f - fTime ) );
		}

		D3DSURFACE_DESC desc;
		m_pTexture->m_pTexture->GetLevelDesc( 0, &desc );

		int nX = 0;
#ifdef DEF_HAPPY_TIME_OEN_080507
		nX = ( KWindowCollector::GetViewWidth() / 2 ) - ( m_pTexture->m_nWidth / 2 );
#else
		nX = ( KWindowCollector::GetViewWidth() / 2 );// - ( m_pTexture->m_nWidth / 2 );
#endif // DEF_HAPPY_TIME_OEN_080507
		int nY = ( KWindowCollector::GetViewHeight() / 2 ) - 100;

		Render2D( nX, nY, 251, 66, m_pTexture->m_pTexture, D3DCOLOR_RGBA( 255, 255, 255, byA )  );
	}
	else
	{
 		m_RegionEnv[ nIndex ].m_eIndicator = S_END;
		m_RegionEnv[ nIndex ].bSelect = false;
 		SAFE_RELEASE( m_pTexture );
	}

	/*

	// ���� �ý��ۿ��� ������ �ڵ带 ���� ����ҷ��� ������ ���� �߰��Ǵ� UI Lib�� �̿��� ������ �� �� ������ ���Ƽ�
	// �н� �Ѵ�. ���� �۾��е� �ƴϱ⿡...... 3�ʰ� ó�� �ؾ� �Ѵ�. FI 1, HO 1, FO 1

	if ( !MMap::IsLoaded() )
	{
		���� ȯ�� ó��
		�ε������� ó��
	}
	else
	{
		�ε������� ó��
	}
	*/
}

bool Region::_LoadRegionInfo()
{
	FILE *fp = fopen( "DATA\\MAPS\\Region.dat", "rt" );
	if ( fp == NULL )
	{
		return false;
	}

	int nLoopCount = 0;
	fscanf( fp, "%d", &nLoopCount );

	for ( int i = 0; i < nLoopCount; ++i )
	{
		// �ε���
		int nID, nIndex;
		fscanf( fp, "%d%d", &nID, &nIndex );

		// ���� ��ġ
		SIZE sizeStandard;
		fscanf( fp, "%d%d", &sizeStandard.cx, &sizeStandard.cy );

		// ����
		D3DXVECTOR3 vLF, vRB;
		fscanf( fp, "%f%f%f", &vLF.x, &vLF.y, &vLF.z );
		fscanf( fp, "%f%f%f", &vRB.x, &vRB.y, &vRB.z );

		vLF.x += BETWEEN_TOOL_AND_CLIENT_GAP_X;
		vLF.y += BETWEEN_TOOL_AND_CLIENT_GAP_Y;
		vRB.x += BETWEEN_TOOL_AND_CLIENT_GAP_X;
		vRB.y += BETWEEN_TOOL_AND_CLIENT_GAP_Y;

		// ����
		D3DXCOLOR clr;
		fscanf( fp, "%f%f%f%f", &clr.a, &clr.r, &clr.g, &clr.b );

		sRegionEnv RegionEnv;
		RegionEnv.nIndex = nIndex;
		RegionEnv.vLF = vLF;
		RegionEnv.vRB = vRB;
		RegionEnv.sizeStandard = sizeStandard;
		RegionEnv.clr = clr;
		m_RegionEnv.push_back( RegionEnv );
	}

	fclose( fp );

	return true;
}

/*
bool Region::_LoadRegionIndicator()
{
	FILE *fp = fopen( "DATA\\MAPS\\RegionIndicator.dat", "rt" );
	if ( fp == NULL )
	{
		return false;
	}

	int nLoopCount = 0;
	fscanf( fp, "%d", &nLoopCount );

	for ( int i = 0; i < nLoopCount; ++i )
	{
		// �ε���
		int nIndex;
		char szTemp[ MAX_PATH ] = { 0, };
		fscanf( fp, "%d%s", &nIndex, &szTemp );

		// �������� �ε����� �� ���� ����
		int nLoopCountSource = m_RegionEnv.size();
		for ( int j = 0; j < nLoopCountSource; ++j )
		{
			if ( m_RegionEnv[ j ].nIndex == nIndex )
			{
				m_RegionEnv[ j ].strPath = szTemp;
			}
		}
	}

	fclose( fp );

	return true;
}
*/

BYTE Region::_UpdateARGB( BYTE byGap, BYTE bySource, float fTime )
{
	BYTE byValue = max( 0, min( 255, byGap - bySource ) );

	if ( byValue == 0 )
	{
		byValue = (BYTE)( bySource * fTime );
	}
	else
	{
		byValue = (BYTE)( byValue * fTime );
		byValue = byGap - byValue;
	}

	return byValue;
}


void Region::_BeginEnv( int nIndex )
{
	// �ε巯�� Alphaó��
	BYTE byA = (BYTE)( m_RegionEnv[ nIndex ].clr.a );
	BYTE byR = (BYTE)( m_RegionEnv[ nIndex].clr.r );
	BYTE byG = (BYTE)( m_RegionEnv[ nIndex ].clr.g );
	BYTE byB = (BYTE)( m_RegionEnv[ nIndex ].clr.b );

	float fTime = g_fCurrentTime - m_RegionEnv[ nIndex ].fCurrentTime;
	if ( fTime < ENV_BLENDING_TIME )
	{
		byA = (BYTE)( byA * ( fTime / ENV_BLENDING_TIME ) );
		byR = (BYTE)( byR * ( fTime / ENV_BLENDING_TIME ) );
		byG = (BYTE)( byG * ( fTime / ENV_BLENDING_TIME ) );
		byB = (BYTE)( byB * ( fTime / ENV_BLENDING_TIME ) );
	}

	XSystem::m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA( byR, byG, byB, byA )/*DWORD( m_RegionEnv[ nIndex ].clr )*/ );
}

void Region::_EndEnv( int nIndex )
{
	// �ѹ��� ������
	if ( !m_bRestoreColor )
	{
		m_RegionEnv[ nIndex ].fCurrentTime = g_fCurrentTime;
		m_bRestoreColor = true;
	}

	// ���� ������ �������� ȸ��
	BYTE byBright = (BYTE)( ( (float)KSetting::op.m_nPostProcessBrightness / 100.0f ) * 255 );
	float fTime = g_fCurrentTime - m_RegionEnv[ nIndex ].fCurrentTime;
	if ( fTime < ENV_BLENDING_TIME )
	{
		BYTE byR = (BYTE)( m_RegionEnv[ nIndex ].clr.r );
		BYTE byG = (BYTE)( m_RegionEnv[ nIndex ].clr.g );
		BYTE byB = (BYTE)( m_RegionEnv[ nIndex ].clr.b );

		// ���� ����
		float fElapsed = fTime / ENV_BLENDING_TIME;
		byR = _UpdateARGB( byR, byBright, fElapsed );
		byG = _UpdateARGB( byG, byBright, fElapsed );
		byB = _UpdateARGB( byB, byBright, fElapsed );

		XSystem::m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA( byR, byG, byB, 255 ) );

	//	TRACE( "Restore Color %d %d %d\n", byR, byG, byB );
	}
	else
	{
		m_RegionEnv[ nIndex ].bSelect = false;
		m_bRestoreColor = false;
	}
}
