#include "StdAfx.h"
#include "MMap.h"
#include "MapHeader.h"
#include "XFile.h"
#include "LEngine.h"
#include <list>
#include "XMacro.h"

namespace MMap {
	char		m_szPath[MAX_PATH];
	char *		m_pMemory;
	int			m_nLeaf;
	MLeaf *		m_pLeaf;
	int			m_nNode;
	MNode *		m_pNode;
	int			m_nLight;
	LObject *	m_pLight;
	int			m_nRegion;
	MRegion *	m_pRegion;
	MRegion *	m_pCurrentRegion;
	DWORD		m_dwBitIndex;
	MLeaf *		m_pCameraLeaf;
	XObject *	m_pCurrentObject;
	D3DXVECTOR3	m_vMax;
	D3DXVECTOR3	m_vMin;
	XBaseModel*	m_pModelList;
	bool		m_bViewUpdate;

	void	RemoveAllObject(MNode *pNode);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// 서버맵 부분

#ifndef	_SOCKET_TEST

static POINT				g_svMapCoord = { -1, -1 };
static int					g_svMapEditMode = 0;
static IDirect3DTexture9 *	g_pSvMapTexture[4] = { NULL, };
static bool					g_bSvMapUpdated[4] = { false, };

static DWORD				g_dwSvMap[4][SVMAP_TILE_COUNT * SVMAP_TILE_COUNT];	// 서버맵
static DWORD				g_dwTMap[4][SVMAP_TILE_COUNT * SVMAP_TILE_COUNT];	// 서버맵의 텍스처맵

#ifdef DEF_PATH_FINDING_LETSBE_090218
static WORD					g_dwSvMapObject[4][SVMAP_TILE_COUNT * SVMAP_TILE_COUNT];	// 오브젝트용 서버맵
static WORD					g_dwTMapObject[4][SVMAP_TILE_COUNT * SVMAP_TILE_COUNT];		// 오브젝트용 서버맵의 텍스처 맵
#endif // DEF_PATH_FINDING_LETSBE_090218

static bool					g_mapInfoInitialized = false;
static mapInfo_t			g_mapInfo[64][64];
static portal_t				g_portalList;
static bool					g_showSvMapDebug = false;
static int					g_mapIndexCount = 0;

// mask == 0 이면 셀단위 못가는 곳(하위 16비트), mask > 0 이면 타일단위 마스크(상위 16비트)
DWORD MMap::GetSvMap(int cx, int cy, int mask)
{
    // 예외 체크
#ifdef DEF_EDITOR_OEN_081123
    // y 역시 0 이하 예외 처리
    if (cx < 0 || cy > SVMAP_SIZE*2 || cy < 0)
#else
    if (cx < 0 || cy > SVMAP_SIZE*2)
#endif
		return 0;

	int nMaskValue = mask;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE || mask == SMA_OBJECT )
#else // DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE )
#endif // DEF_PATH_FINDING_LETSBE_090218
	{
		nMaskValue = 1 << (((cy & (SVMAP_CELL_COUNT_PER_TILE - 1)) * SVMAP_CELL_COUNT_PER_TILE) + (cx & (SVMAP_CELL_COUNT_PER_TILE-1)));
	}
	else
		nMaskValue = (1 << (mask - 1)) << 16;

	int ty = (cy / SVMAP_CELL_COUNT_PER_TILE);
	int tx = (cx / SVMAP_CELL_COUNT_PER_TILE);
	int mindex = (ty >> 8) * 2 + (tx >> 8);
	int tindex = (ty & (SVMAP_TILE_COUNT-1)) * SVMAP_TILE_COUNT + (tx & (SVMAP_TILE_COUNT-1));

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_OBJECT )
	{
		return (g_dwSvMapObject[mindex][tindex] & nMaskValue);
	}
#endif // DEF_PATH_FINDING_LETSBE_090218

	return (g_dwSvMap[mindex][tindex] & nMaskValue);
}

void MMap::SetSvMap(int cx, int cy, int mask)
{
#ifdef DEF_EDITOR_OEN_081123
    // y 역시 0 이하 예외 처리
    if (cx < 0 || cy > SVMAP_SIZE*2 || cy < 0)
#else
	if (cx < 0 || cy > SVMAP_SIZE*2)
#endif
		return;

	int nMaskValue = mask;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE || mask == SMA_OBJECT )
#else // DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE )
#endif // DEF_PATH_FINDING_LETSBE_090218
		nMaskValue = 1 << (((cy & (SVMAP_CELL_COUNT_PER_TILE - 1)) * SVMAP_CELL_COUNT_PER_TILE) + (cx & (SVMAP_CELL_COUNT_PER_TILE-1)));
	else
		nMaskValue = (1 << (mask - 1)) << 16;

	int ty = (cy / SVMAP_CELL_COUNT_PER_TILE);
	int tx = (cx / SVMAP_CELL_COUNT_PER_TILE);
	int mindex = (ty >> 8) * 2 + (tx >> 8);
	int tindex = (ty & (SVMAP_TILE_COUNT-1)) * SVMAP_TILE_COUNT + (tx & (SVMAP_TILE_COUNT-1));

	g_bSvMapUpdated[mindex] = false;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_OBJECT )
	{
		g_dwSvMapObject[mindex][tindex] |= nMaskValue;
	}
	else
#endif // DEF_PATH_FINDING_LETSBE_090218
	{
		g_dwSvMap[mindex][tindex] |= nMaskValue;
	}
}

void MMap::ResetSvMap(int cx, int cy, int mask)
{
#ifdef DEF_EDITOR_OEN_081123
    // y 역시 0 이하 예외 처리
    if (cx < 0 || cy > SVMAP_SIZE*2 || cy < 0)
#else
    if (cx < 0 || cy > SVMAP_SIZE*2)
#endif
		return;

	int nMaskValue = mask;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE || mask == SMA_OBJECT )
#else // DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE )
#endif // DEF_PATH_FINDING_LETSBE_090218
		nMaskValue = 1 << (((cy & (SVMAP_CELL_COUNT_PER_TILE - 1)) * SVMAP_CELL_COUNT_PER_TILE) + (cx & (SVMAP_CELL_COUNT_PER_TILE-1)));
	else
		nMaskValue = (1 << (mask - 1)) << 16;

	int ty = (cy / SVMAP_CELL_COUNT_PER_TILE);
	int tx = (cx / SVMAP_CELL_COUNT_PER_TILE);
	int mindex = (ty >> 8) * 2 + (tx >> 8);
	int tindex = (ty & (SVMAP_TILE_COUNT-1)) * SVMAP_TILE_COUNT + (tx & (SVMAP_TILE_COUNT-1));

	g_bSvMapUpdated[mindex] = false;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_OBJECT )
	{
		g_dwSvMapObject[mindex][tindex] &= ~nMaskValue;
	}
	else
#endif // DEF_PATH_FINDING_LETSBE_090218
	{
		g_dwSvMap[mindex][tindex] &= ~nMaskValue;
	}
}

DWORD MMap::GetTMap(int cx, int cy, int mask)
{
#ifdef DEF_EDITOR_OEN_081123
    // y 역시 0 이하 예외 처리
    if (cx < 0 || cy > SVMAP_SIZE*2 || cy < 0)
#else
    if (cx < 0 || cy > SVMAP_SIZE*2)
#endif
		return 0;

	int nMaskValue = mask;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE || mask == SMA_OBJECT )
#else // DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE )
#endif // DEF_PATH_FINDING_LETSBE_090218
		nMaskValue = 1 << (((cy & (SVMAP_CELL_COUNT_PER_TILE - 1)) * SVMAP_CELL_COUNT_PER_TILE) + (cx & (SVMAP_CELL_COUNT_PER_TILE-1)));
	else
		nMaskValue = (1 << (mask - 1)) << 16;

	int ty = (cy / SVMAP_CELL_COUNT_PER_TILE);
	int tx = (cx / SVMAP_CELL_COUNT_PER_TILE);
	int mindex = (ty >> 8) * 2 + (tx >> 8);
	int tindex = (ty & (SVMAP_TILE_COUNT-1)) * SVMAP_TILE_COUNT + (tx & (SVMAP_TILE_COUNT-1));

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_OBJECT )
	{
		return g_dwTMapObject[mindex][tindex] & nMaskValue;
	}
#endif // DEF_PATH_FINDING_LETSBE_090218

	return (g_dwTMap[mindex][tindex] & nMaskValue);
}

void MMap::SetTMap(int cx, int cy, int mask)
{
#ifdef DEF_EDITOR_OEN_081123
    // y 역시 0 이하 예외 처리
    if (cx < 0 || cy > SVMAP_SIZE*2 || cy < 0)
#else
    if (cx < 0 || cy > SVMAP_SIZE*2)
#endif
		return;

	int nMaskValue = mask;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE || mask == SMA_OBJECT )
#else // DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE )
#endif // DEF_PATH_FINDING_LETSBE_090218
		nMaskValue = 1 << (((cy & (SVMAP_CELL_COUNT_PER_TILE - 1)) * SVMAP_CELL_COUNT_PER_TILE) + (cx & (SVMAP_CELL_COUNT_PER_TILE-1)));
	else
		nMaskValue = (1 << (mask - 1)) << 16;

	int ty = (cy / SVMAP_CELL_COUNT_PER_TILE);
	int tx = (cx / SVMAP_CELL_COUNT_PER_TILE);
	int mindex = (ty >> 8) * 2 + (tx >> 8);
	int tindex = (ty & (SVMAP_TILE_COUNT-1)) * SVMAP_TILE_COUNT + (tx & (SVMAP_TILE_COUNT-1));

	g_bSvMapUpdated[mindex] = false;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_OBJECT )
	{
		g_dwTMapObject[mindex][tindex] |= nMaskValue;
	}
	else
#endif // DEF_PATH_FINDING_LETSBE_090218
	{
		g_dwTMap[mindex][tindex] |= nMaskValue;
	}
}

void MMap::ResetTMap(int cx, int cy, int mask)
{
#ifdef DEF_EDITOR_OEN_081123
    // y 역시 0 이하 예외 처리
    if (cx < 0 || cy > SVMAP_SIZE*2 || cy < 0)
#else
    if (cx < 0 || cy > SVMAP_SIZE*2)
#endif
        return;

	int nMaskValue = mask;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE || mask == SMA_OBJECT )
#else // DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_NOTMOVEABLE )
#endif // DEF_PATH_FINDING_LETSBE_090218
        nMaskValue = 1 << (((cy & (SVMAP_CELL_COUNT_PER_TILE - 1)) * SVMAP_CELL_COUNT_PER_TILE) + (cx & (SVMAP_CELL_COUNT_PER_TILE-1)));
    else
        nMaskValue = (1 << (mask - 1)) << 16;

    int ty = (cy / SVMAP_CELL_COUNT_PER_TILE);
    int tx = (cx / SVMAP_CELL_COUNT_PER_TILE);
    int mindex = (ty >> 8) * 2 + (tx >> 8);
    int tindex = (ty & (SVMAP_TILE_COUNT-1)) * SVMAP_TILE_COUNT + (tx & (SVMAP_TILE_COUNT-1));

    g_bSvMapUpdated[mindex] = false;

#ifdef DEF_PATH_FINDING_LETSBE_090218
	if ( mask == SMA_OBJECT )
	{
		g_dwTMapObject[mindex][tindex] &= ~nMaskValue;
	}
	else
#endif // DEF_PATH_FINDING_LETSBE_090218
	{
		g_dwTMap[mindex][tindex] &= ~nMaskValue;
	}
}

void MMap::SetSvMapEditMode(int bit)
{
	g_svMapEditMode = bit;

	g_bSvMapUpdated[0] = false;
	g_bSvMapUpdated[1] = false;
	g_bSvMapUpdated[2] = false;
	g_bSvMapUpdated[3] = false;
}

void MMap::SetSvMapCoord(int x, int y)
{
	g_svMapCoord.x = x;
	g_svMapCoord.y = y;

	g_bSvMapUpdated[0] = false;
	g_bSvMapUpdated[1] = false;
	g_bSvMapUpdated[2] = false;
	g_bSvMapUpdated[3] = false;

	LoadCurrentSvMap();

	MMap::Load(NULL, x, y);
}

/*
임시: 포팅용 Define, On -> 저장, Off -> 6Byte
*/
//#define DEF_PORTING_20090224

bool MMap::LoadCurrentSvMap()
{
	FILE	*fp;
	DWORD	version;

	if (g_svMapCoord.x < 0 || g_svMapCoord.x > 63 || g_svMapCoord.y < 0 || g_svMapCoord.y > 63)
		return FALSE;

	ClearTMap();

	int x = g_svMapCoord.x & ~1;
	int y = g_svMapCoord.y & ~1;

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			fp = fopen(va("DATA\\Maps\\n_%03d_%03d.ksm", x + i, y + j), "rb");
			if (!fp)
			{
				memset( g_dwSvMap[ j*  2 + i ], 0, sizeof(g_dwSvMap[ 0 ]) );

#ifdef DEF_PATH_FINDING_LETSBE_090218
				memset(g_dwSvMapObject[ j * 2 + i ], 0, sizeof(g_dwSvMapObject[ 0 ]) );
#endif // DEF_PATH_FINDING_LETSBE_090218

				continue;
			}

			fread(&version, sizeof(version), 1, fp);
			if (version != SVMAP_VERSION)
			{
				fclose(fp);
				continue;
			}

#ifdef DEF_PORTING_20090224
			fread( &g_dwSvMap[ j * 2 + i ], sizeof(g_dwSvMap[0]), 1, fp);
#else // DEF_PORTING_20090224
			int nLoopCount = SVMAP_TILE_COUNT * SVMAP_TILE_COUNT;
			for ( int n = 0; n < nLoopCount; ++n )
			{
				fread( &g_dwSvMap[ j * 2 + i ][ n ], sizeof(DWORD), 1, fp);

#ifdef DEF_PATH_FINDING_LETSBE_090218
				fread( &g_dwSvMapObject[ j * 2 + i ][ n ], sizeof(WORD), 1, fp);
#endif // DEF_PATH_FINDING_LETSBE_090218
			}
#endif // DEF_PORTING_20090224
			fclose(fp);
		}
	}
	
	return TRUE;
}

void MMap::SaveCurrentSvMap()
{
	FILE	*fp;
	DWORD	version;

	if (g_svMapCoord.x < 0 || g_svMapCoord.x > 63 || g_svMapCoord.y < 0 || g_svMapCoord.y > 63)
		return;

	int x = g_svMapCoord.x & ~1;
	int y = g_svMapCoord.y & ~1;

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			char* szFile = va( "DATA\\Maps\\n_%03d_%03d.ksm", x + i, y + j );

#ifdef DEF_PORTING_20090224
			// 포팅할땐 있는 파일만 하자
			if ( access( szFile , 0 ) != 0 )
			{
				continue;
			}
#endif // DEF_PORTING_20090224

			fp = fopen( szFile, "wb");

			if (!fp)
				continue;

			version = SVMAP_VERSION;
			fwrite(&version, sizeof(version), 1, fp);

			int nLoopCount = SVMAP_TILE_COUNT * SVMAP_TILE_COUNT;
			for ( int n = 0; n < nLoopCount; ++n )
			{
				fwrite( &g_dwSvMap[ j * 2 + i ][ n ], sizeof(DWORD), 1, fp );

#ifdef DEF_PATH_FINDING_LETSBE_090218
				fwrite( &g_dwSvMapObject[ j * 2 + i ][ n ], sizeof(WORD), 1, fp );
#endif // DEF_PATH_FINDING_LETSBE_090218
			}

			fclose(fp);
		}
	}
}

void MMap::SaveAllSvMap()
{
	for ( int nX = 0; nX < 64; ++nX )
	{
		for ( int nY = 0; nY < 64; ++nY )
		{
			// 던전 지역은  0 ~ MAP_START or MAP_END ~ 63 지역만 쓰기로 되어 있다
 			if ( nX > MAP_START && nX < MAP_END )
 			{
 				continue;
 			}
 			if ( nY > MAP_START && nY < MAP_END )
 			{
 				continue;
 			}

			SetSvMapCoord( nX, nY );

			int x = g_svMapCoord.x & ~1;
			int y = g_svMapCoord.y & ~1;

			for (int j = 0; j < 2; j++)
			{
				for (int i = 0; i < 2; i++)
				{
					DWORD	version;
					FILE* fp = NULL;
					char* szFile = va( "DATA\\Maps\\n_%03d_%03d.ksm", x + i, y + j );

					// 모두 다 저장이 하는게 아니라 존재하는 파일만 저장한다.
					// 모두 다 저장하면 .....
					if ( access( szFile , 0 ) == 0 )
					{
						fp = fopen( szFile, "wb");
						if (!fp)
							continue;

						version = SVMAP_VERSION;
						fwrite(&version, sizeof(version), 1, fp);

						int nLoopCount = SVMAP_TILE_COUNT * SVMAP_TILE_COUNT;
						for ( int n = 0; n < nLoopCount; ++n )
						{
							fwrite( &g_dwSvMap[ j * 2 + i ][ n ], sizeof(DWORD), 1, fp);

#ifdef DEF_PATH_FINDING_LETSBE_090218
							fwrite( &g_dwSvMapObject[ j * 2 + i ][ n ], sizeof(WORD), 1, fp);
#endif // DEF_PATH_FINDING_LETSBE_090218
						}

						fclose(fp);
					}
				}
			}
		}
	}
}

void MMap::GenerateSvMap()
{
	D3DXVECTOR3 vOrg;
	float		x, z;
	int			cx, cy;

	for (cy = 0; cy < SVMAP_CELL_COUNT * 2; cy++)
		for (cx = 0; cx < SVMAP_CELL_COUNT * 2; cx++)
			ResetSvMap(cx, cy, 0);
	
	vOrg.x = ToClient((g_svMapCoord.x & ~1) * SVMAP_SIZE, XSystem::m_originPatchX);
	vOrg.y = m_vMin.y;
	vOrg.z = ToClient((g_svMapCoord.y & ~1) * SVMAP_SIZE, XSystem::m_originPatchY);

	D3DXVECTOR3 vCenter = XSystem::m_vCenter;

	DWORD dwStart = GetTickCount();	

	XSystem::SetCenter(D3DXVECTOR3(vOrg.x + SVMAP_SIZE, vOrg.y, vOrg.z + SVMAP_SIZE));
	XSystem::m_vExtent = D3DXVECTOR3(SVMAP_SIZE, 0, SVMAP_SIZE);
	XSystem::WaitLoading();	

	for (cy = 0; cy < SVMAP_CELL_COUNT * 2; )
	{
		for (cx = 0; cx < SVMAP_CELL_COUNT * 2; cx++)
		{
			x = vOrg.x + cx * SVMAP_CELL_SIZE + SVMAP_CELL_SIZE * 0.5f;
			z = vOrg.z + cy * SVMAP_CELL_SIZE + SVMAP_CELL_SIZE * 0.5f;
			if (!XEngine::CanStand(x, z))
				SetSvMap(cx, cy, 0); // 못가는 곳
		}

		cy++;
		if ((cy & (SVMAP_CELL_COUNT * 2 / 16 - 1)) == 0)
		{
			DWORD dwEnd = GetTickCount();
			//OutputDebugString(va("Generate SvMap %.2f%%  %d sec\n",	cy * 100.0f / (SVMAP_CELL_COUNT * 2), (dwEnd - dwStart) / 1000));		
		}	
	}

	XSystem::SetCenter(vCenter);
	XSystem::m_vExtent = D3DXVECTOR3(0, 0, 0);
	XSystem::WaitLoading();	
}

void MMap::ClearSvMap()
{
	memset(g_dwSvMap[0], 0, sizeof(g_dwSvMap[0]));
	memset(g_dwSvMap[1], 0, sizeof(g_dwSvMap[1]));
	memset(g_dwSvMap[2], 0, sizeof(g_dwSvMap[2]));
	memset(g_dwSvMap[3], 0, sizeof(g_dwSvMap[3]));

#ifdef DEF_PATH_FINDING_LETSBE_090218
	memset( g_dwSvMapObject[ 0 ], 0, sizeof(g_dwSvMapObject[ 0 ]) );
	memset( g_dwSvMapObject[ 1 ], 0, sizeof(g_dwSvMapObject[ 1 ]) );
	memset( g_dwSvMapObject[ 2 ], 0, sizeof(g_dwSvMapObject[ 2 ]) );
	memset( g_dwSvMapObject[ 3 ], 0, sizeof(g_dwSvMapObject[ 3 ]) );
#endif // DEF_PATH_FINDING_LETSBE_090218

	g_bSvMapUpdated[0] = false;
	g_bSvMapUpdated[1] = false;
	g_bSvMapUpdated[2] = false;
	g_bSvMapUpdated[3] = false;
}

void MMap::ClearTMap()
{
	memset(g_dwTMap[0], 0, sizeof(g_dwTMap[0]));
	memset(g_dwTMap[1], 0, sizeof(g_dwTMap[1]));
	memset(g_dwTMap[2], 0, sizeof(g_dwTMap[2]));
	memset(g_dwTMap[3], 0, sizeof(g_dwTMap[3]));

#ifdef DEF_PATH_FINDING_LETSBE_090218
	memset( g_dwTMapObject[ 0 ], 0, sizeof(g_dwTMapObject[ 0 ]) );
	memset( g_dwTMapObject[ 1 ], 0, sizeof(g_dwTMapObject[ 1 ]) );
	memset( g_dwTMapObject[ 2 ], 0, sizeof(g_dwTMapObject[ 2 ]) );
	memset( g_dwTMapObject[ 3 ], 0, sizeof(g_dwTMapObject[ 3 ]) );
#endif // DEF_PATH_FINDING_LETSBE_090218

	g_bSvMapUpdated[0] = false;
	g_bSvMapUpdated[1] = false;
	g_bSvMapUpdated[2] = false;
	g_bSvMapUpdated[3] = false;
}

typedef std::vector<CPoint> POINT_VECTOR;

static void PushCandidate(POINT_VECTOR &vCandidateList, CPoint point, int mask)
{
	if (point.x < 0 || point.x >= SVMAP_CELL_COUNT * 2|| point.y < 0 || point.y >= SVMAP_CELL_COUNT * 2)
		return;
	if (MMap::GetTMap(point.x, point.y, mask))
		return;
	if (MMap::GetSvMap(point.x, point.y, mask))
		return;

	MMap::SetTMap(point.x, point.y, mask);
	vCandidateList.push_back(point);
}

void MMap::FillTMap(int x, int y, int mask)
{
	POINT_VECTOR	vCandidateList;
	int				interval;

	if (mask == 0)
		interval = 1;
	else
		interval = SVMAP_CELL_COUNT_PER_TILE;
		
	PushCandidate(vCandidateList, CPoint(x, y), mask);
	for (; !vCandidateList.empty(); )
	{
		CPoint point = vCandidateList.back();
		vCandidateList.pop_back();
		PushCandidate(vCandidateList, CPoint(point.x - interval, point.y), mask); // left
		PushCandidate(vCandidateList, CPoint(point.x + interval, point.y), mask); // right
		PushCandidate(vCandidateList, CPoint(point.x, point.y - interval), mask); // top
		PushCandidate(vCandidateList, CPoint(point.x, point.y + interval), mask); // bottom
	}
}

void MMap::ApplyTMap(int mask, int op)
{
	int nMaskValue = mask;
	if ( mask == SMA_NOTMOVEABLE )
	{
		nMaskValue = 0xFFFF;
	}
	else
	{
		nMaskValue = ((1 << (mask - 1)) << 16);
	}

	switch (op)
	{
		case TMAP_INVERSE:
			{
				for (int j=0; j < 4; j++)
				{
					for (int i=0; i < SVMAP_TILE_COUNT * SVMAP_TILE_COUNT; i++)
					{
						if ( mask != SMA_OBJECT )
						{
							g_dwSvMap[ j ][ i ] |= (~g_dwTMap[ j ][ i ]) & nMaskValue;
						}
						else
						{
#ifdef DEF_PATH_FINDING_LETSBE_090218
							g_dwSvMapObject[ j ][ i ] |= ( ~g_dwTMapObject[ j ][ i ] ) & nMaskValue;
#endif // DEF_PATH_FINDING_LETSBE_090218
						}
					}
				}
			}
			break;
		case TMAP_OR:
			{
				for (int j=0; j < 4; j++)
				{
					for (int i=0; i < SVMAP_TILE_COUNT * SVMAP_TILE_COUNT; i++)
					{
						if ( mask != SMA_OBJECT )
						{
							g_dwSvMap[j][i] |= g_dwTMap[j][i] & nMaskValue;
						}
						else
						{
#ifdef DEF_PATH_FINDING_LETSBE_090218
							g_dwSvMapObject[ j ][ i ] |= g_dwTMapObject[ j ][ i ] & nMaskValue;
#endif // DEF_PATH_FINDING_LETSBE_090218
						}
					}
				}
			}
			break;
		default:
			assert(0);
			break;
	}

	ClearTMap();

	g_bSvMapUpdated[0] = false;
	g_bSvMapUpdated[1] = false;
	g_bSvMapUpdated[2] = false;
	g_bSvMapUpdated[3] = false;
}

static void UpdateSvMapTexture()
{
	static D3DCOLOR colorForBit[16] = 
	{
		D3DCOLOR_RGBA(255,	0,		0,		127),	// 못가는곳
		D3DCOLOR_RGBA(255,	0,		255,	127),	// 포탈
		D3DCOLOR_RGBA(0,	0,		255,	127),	// 마을
		D3DCOLOR_RGBA(0,	255,	0,		127),	// 안전
		D3DCOLOR_RGBA(0,	255,	255,	127),	// 공성		
		D3DCOLOR_RGBA(255,	255,	0,		127),	// 수성

#ifdef DEF_PATH_FINDING_LETSBE_090218
		D3DCOLOR_RGBA(255,	255,	255,	127),	// 오브젝트
#endif // DEF_PATH_FINDING_LETSBE_090218
	};

	D3DLOCKED_RECT	lr;
	DWORD			*ptr;
	int				cx, cy;

	if (g_svMapEditMode == -1)
		return;

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			if (!g_pSvMapTexture[j*2+i])
				XSystem::m_pDevice->CreateTexture(SVMAP_CELL_COUNT, SVMAP_CELL_COUNT, 0, 0, 
					D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, (IDirect3DTexture9 **)&g_pSvMapTexture[j*2+i], NULL);
		
			if (!g_bSvMapUpdated[j*2+i])
			{
				g_bSvMapUpdated[j*2+i] = true;

				D3DCOLOR color = colorForBit[g_svMapEditMode];

				g_pSvMapTexture[j*2+i]->LockRect(0, &lr, NULL, 0);				

				for (cy = 0; cy < SVMAP_CELL_COUNT; cy++)
				{
					ptr = (DWORD *)((BYTE *)lr.pBits + lr.Pitch * cy);
					for (cx = 0; cx < SVMAP_CELL_COUNT; cx++)
					{
						if (!(XSystem::m_caps.TextureAddressCaps & D3DPTADDRESSCAPS_BORDER) && 
							(cx == 0 || cx == SVMAP_CELL_COUNT - 1 || cy == 0 || cy == SVMAP_CELL_COUNT - 1))
						{
							*ptr++ = 0;
						}
						else
						{
							*ptr++ = MMap::GetSvMap(i*SVMAP_CELL_COUNT + cx, j*SVMAP_CELL_COUNT + cy, g_svMapEditMode) ? color : 
								(MMap::GetTMap(i*SVMAP_CELL_COUNT + cx, j*SVMAP_CELL_COUNT + cy, g_svMapEditMode) ? D3DCOLOR_RGBA(0, 255, 255, 127) : 
								D3DCOLOR_RGBA(0, 0, 0, 0));
						}
					}
				}

				g_pSvMapTexture[j*2+i]->UnlockRect(0);
			}
		}
	}
}

static void DrawSvMapDebug()
{
	typedef struct testVert_s {
		D3DXVECTOR4	xyzw;
		D3DXVECTOR2	texCoord;
	} testVert_t;
	
	testVert_t v[4];

	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			if (!g_pSvMapTexture[j*2 + i])
				continue;

			v[0].xyzw = D3DXVECTOR4(256.f * i, 256.f * (2 - j), 0.f, 1.f);
			v[1].xyzw = D3DXVECTOR4(256.f * (i + 1), 256.f * (2 - j), 0.f, 1.f);
			v[2].xyzw = D3DXVECTOR4(256.f * (i + 1), 256.f * (1 - j), 0.f, 1.f);
			v[3].xyzw = D3DXVECTOR4(256.f * i, 256.f * (1 - j), 0.f, 1.f);
			
			v[0].texCoord = D3DXVECTOR2(0.0f, 0.0f);
			v[1].texCoord = D3DXVECTOR2(1.0f, 0.0f);
			v[2].texCoord = D3DXVECTOR2(1.0f, 1.0f);
			v[3].texCoord = D3DXVECTOR2(0.0f, 1.0f);

			XSystem::m_pDevice->SetTexture(0, g_pSvMapTexture[j*2 + i]);
			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			XSystem::m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW); //
			XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

			XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);			

			XSystem::m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
			XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, v, sizeof(testVert_t));

			XSystem::m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}
	}
}

void MMap::RenderSvMap()
{	
	if (g_svMapEditMode == -1)
		return;
	
	// 서버맵 그리기
	UpdateSvMapTexture();
	
	int x = g_svMapCoord.x & ~1;
	int y = g_svMapCoord.y & ~1;
    
	for (int j = 0; j < 2; j++)
	{
		for (int i = 0; i < 2; i++)
		{
			D3DXVECTOR3 vCenter;
			vCenter.x = ToClient((x + i) * SVMAP_SIZE, XSystem::m_originPatchX) + SVMAP_SIZE / 2;
			vCenter.y = 0.f;
			vCenter.z = ToClient((y + j) * SVMAP_SIZE, XSystem::m_originPatchY) + SVMAP_SIZE / 2;
			
#ifndef DEF_EDITOR_OEN_081123
			XSystem::RenderDecal(vCenter, SVMAP_SIZE / 2, g_pSvMapTexture[j*2 + i], false, true);
#else
            XSystem::RenderDecal(vCenter, SVMAP_SIZE / 2, SVMAP_SIZE, g_pSvMapTexture[j*2 + i], false, true);
#endif
		}
	}

	if (g_showSvMapDebug)
		DrawSvMapDebug();

	// 2패치 * 2패치 라인 그리기
	
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(255, 255, 255, 255));
	XSystem::m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
	XSystem::m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	
	D3DXMATRIX m;
#ifndef DEF_EDITOR_OEN_081123
    // 이동 행렬 생성
    D3DXMatrixTranslation(&m, 
                          ToClient(x * SVMAP_SIZE, XSystem::m_originPatchX) + SVMAP_SIZE, 
                          2000.f, 
                          ToClient(y * SVMAP_SIZE, XSystem::m_originPatchY) + SVMAP_SIZE
                          );
#else
	D3DXMatrixTranslation(&m, 
                          ToClient(x * SVMAP_SIZE, XSystem::m_originPatchX) + SVMAP_SIZE, 
                          2000.f, 
                          ToClient(y * SVMAP_SIZE, XSystem::m_originPatchY) + SVMAP_SIZE
                          );
#endif
	XSystem::m_pDevice->SetTransform(D3DTS_WORLD, &m);

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZ);

	D3DXVECTOR3 xyz[16] = 
	{ 
		D3DXVECTOR3(-SVMAP_SIZE, -2000.f, -SVMAP_SIZE),
		D3DXVECTOR3(SVMAP_SIZE, -2000.f, -SVMAP_SIZE),
		D3DXVECTOR3(SVMAP_SIZE, 2000.f, -SVMAP_SIZE),
		D3DXVECTOR3(-SVMAP_SIZE, 2000.f, -SVMAP_SIZE),

		D3DXVECTOR3(SVMAP_SIZE, -2000.f, -SVMAP_SIZE),
		D3DXVECTOR3(SVMAP_SIZE, -2000.f, SVMAP_SIZE),
		D3DXVECTOR3(SVMAP_SIZE, 2000.f, SVMAP_SIZE),
		D3DXVECTOR3(SVMAP_SIZE, 2000.f, -SVMAP_SIZE),

		D3DXVECTOR3(SVMAP_SIZE, -2000.f, SVMAP_SIZE),
		D3DXVECTOR3(-SVMAP_SIZE, -2000.f, SVMAP_SIZE),
		D3DXVECTOR3(-SVMAP_SIZE, 2000.f, SVMAP_SIZE),
		D3DXVECTOR3(SVMAP_SIZE, 2000.f, SVMAP_SIZE),

		D3DXVECTOR3(-SVMAP_SIZE, -2000.f, SVMAP_SIZE),
		D3DXVECTOR3(-SVMAP_SIZE, -2000.f, -SVMAP_SIZE),
		D3DXVECTOR3(-SVMAP_SIZE, 2000.f, -SVMAP_SIZE),
		D3DXVECTOR3(-SVMAP_SIZE, 2000.f, SVMAP_SIZE)
	};

	for (int i = 0; i < 4; i++)
		XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_LINESTRIP, 3, xyz[i*4], sizeof(xyz[0]));
	
	XSystem::m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	XSystem::m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
}

void MMap::ShowSvMapDebug(bool show)
{
	g_showSvMapDebug = show;
}

void MMap::LoadMapInfo()
{
	FILE *fp;
	char *buffer;
	char *ptr;
	char *token;
	int size;
	int i, j;

	memset(g_mapInfo, 0, sizeof(g_mapInfo));
	for (i = MAP_START; i < MAP_END; i++)
	{
		for (j = MAP_START; j < MAP_END; j++)
		{
			g_mapInfo[i][j].reserved = true;

			fp = fopen(va("DATA\\MAPS\\n_%03d_%03d.ksm", j, i), "rb");
			if (fp)
			{
				fclose(fp);
				g_mapInfo[i][j].occupied = true;
			}
		}
	}
	
	g_mapInfoInitialized = true;

	fp = fopen("Map\\mapinfo.txt", "rb");
	if (!fp)
		return;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	buffer = (char *)malloc(size+1);
	fseek(fp, 0, SEEK_SET);

	fread(buffer, size, 1, fp);
	buffer[size] = NULL;
	
	ptr = buffer;

	while (*ptr)
	{
		token = ParseEx(&ptr, true);
		if (!ptr)
			break;

		if (token[0] != '(')
		{
			TRACE("expecting '(', found '%s'\n", token);
			goto END_OF_FUNCTION;
		}

		token = ParseEx(&ptr, true);
		if (!stricmp(token, "map") || !stricmp(token, "initmap"))
		{
			// '('
			token = ParseEx(&ptr, true);
			// 'index'
			token = ParseEx(&ptr, true);
			token = ParseEx(&ptr, true);
			int mapIndex = atoi(token);
			// ')'
			token = ParseEx(&ptr, true);

			// '('
			token = ParseEx(&ptr, true);
			// 'kind'
			token = ParseEx(&ptr, true);
			// '1'
			token = ParseEx(&ptr, true);		            
			// ')'
			token = ParseEx(&ptr, true);

#ifdef DEF_EDITOR_OEN_081123
            // '('
            token = ParseEx(&ptr, true);
            // 'fog'
            token = ParseEx(&ptr, true);
            // '1'
            token = ParseEx(&ptr, true);
            int fogEnd = atoi(token);
            // '2'
            token = ParseEx(&ptr, true);
            float fogColorR = (float)atof(token);
            // '3'
            token = ParseEx(&ptr, true);
            float fogColorG = (float)atof(token);
            // '4'
            token = ParseEx(&ptr, true);
            float fogColorB = (float)atof(token);
            // ')'
            token = ParseEx(&ptr, true);
#endif
			// '('
			token = ParseEx(&ptr, true);
			// 'xy'
			token = ParseEx(&ptr, true);		            
			token = ParseEx(&ptr, true);
			int x = atoi(token);			
			token = ParseEx(&ptr, true);
			int y = atoi(token);
			// ')'
			token = ParseEx(&ptr, true);

			// '('
			token = ParseEx(&ptr, true);
			// 'size'
			token = ParseEx(&ptr, true);
			token = ParseEx(&ptr, true);
			int sizex = atoi(token);
			token = ParseEx(&ptr, true);
			int sizey = atoi(token);
			// ')'
			token = ParseEx(&ptr, true);

			// '('
			token = ParseEx(&ptr, true);
			// 'mapname'
			token = ParseEx(&ptr, true);
			token = ParseEx(&ptr, true);
			strcpy(g_mapInfo[y][x].mapname, token);
			// ')'
			token = ParseEx(&ptr, true);

			g_mapInfo[y][x].mapIndex = mapIndex;
			g_mapInfo[y][x].x = x;
			g_mapInfo[y][x].y = y;
			g_mapInfo[y][x].sizex = sizex;
			g_mapInfo[y][x].sizey = sizey;
			g_mapInfo[y][x].occupied = true;

#ifdef DEF_EDITOR_OEN_081123
            g_mapInfo[y][x].fogEnd = fogEnd; 
            g_mapInfo[y][x].fogColor[0] = fogColorR;
            g_mapInfo[y][x].fogColor[1] = fogColorG;
            g_mapInfo[y][x].fogColor[2] = fogColorB;
#endif

			g_mapIndexCount = max(g_mapIndexCount, mapIndex);			
		}
		else if (!stricmp(token, "portal"))
		{
			char name[64];
			RECT srcRect;
			int dstVec[3];

			// name
			token = ParseEx(&ptr, true);
			strcpy(name, token);

			// '('
			token = ParseEx(&ptr, true);
			// 'from'
			token = ParseEx(&ptr, true);
			// srcRect map index (저장할때 계산하므로 무시)
			token = ParseEx(&ptr, true);
			token = ParseEx(&ptr, true);
			srcRect.left = atoi(token);
			token = ParseEx(&ptr, true);
			srcRect.top = atoi(token);
			token = ParseEx(&ptr, true);
			srcRect.right = atoi(token);
			token = ParseEx(&ptr, true);
			srcRect.bottom = atoi(token);
			// ')'
			token = ParseEx(&ptr, true);

			// '('
			token = ParseEx(&ptr, true);
			// to
			token = ParseEx(&ptr, true);
			// dstVec map index (저장할때 계산하므로 무시)
			token = ParseEx(&ptr, true);
			token = ParseEx(&ptr, true);
			dstVec[0] = atoi(token);
			token = ParseEx(&ptr, true);
			dstVec[1] = atoi(token);
			token = ParseEx(&ptr, true);
			dstVec[2] = atoi(token);
			// ')'
			token = ParseEx(&ptr, true);
				
			MMap::AddPortal(name, &srcRect, dstVec);
		}
		else
		{
			break;
		}

		token = ParseEx(&ptr, true);
		if (token[0] != ')')
		{
			TRACE("expecting ')', found '%s'\n", token);
			goto END_OF_FUNCTION;
		}
	}

END_OF_FUNCTION:	

	fclose(fp);	

	free(buffer);
}

void MMap::SaveMapInfo()
{
	FILE *fp;
	int i, j;

	fp = fopen("Map\\mapinfo.txt", "wt");
	if (!fp)
		return;

	for (i = 0; i < 64; i++)
	{
		for (j = 0; j < 64; j++)
		{
			if (g_mapInfo[j][i].occupied && !g_mapInfo[j][i].reserved)

#ifdef DEF_EDITOR_OEN_081123
                fprintf(fp, "( initmap ( index %d ) ( kind 1 ) ( fog %d %.2f %.2f %.2f ) ( xy %d %d ) ( size %d %d ) ( mapname \"%s\" ) )\n", 
                        g_mapInfo[j][i].mapIndex, g_mapInfo[j][i].fogEnd, g_mapInfo[j][i].fogColor[0], g_mapInfo[j][i].fogColor[1], g_mapInfo[j][i].fogColor[2], 
                        i, j, g_mapInfo[j][i].sizex, g_mapInfo[j][i].sizey, g_mapInfo[j][i].mapname);
#else
				fprintf(fp, "( initmap ( index %d ) ( kind 1 ) ( xy %d %d ) ( size %d %d ) ( mapname \"%s\" ) )\n", g_mapInfo[j][i].mapIndex, 
					i, j, g_mapInfo[j][i].sizex, g_mapInfo[j][i].sizey, g_mapInfo[j][i].mapname);
#endif
		}
	}

	for (portal_t *p = g_portalList.next; p != &g_portalList; p = p->next)
	{
		mapInfo_t *srcMapInfo = GetMapInfo(p->srcRect.left / SVMAP_TILE_COUNT, p->srcRect.top / SVMAP_TILE_COUNT);
		assert(srcMapInfo);
		
		mapInfo_t *dstMapInfo = GetMapInfo(p->dstVec[0] / SVMAP_SIZE, p->dstVec[1] / SVMAP_SIZE);
		assert(dstMapInfo);
		
		fprintf(fp, "( portal \"%s\" ( from %d %d %d %d %d ) ( to %d %d %d %d ) )\n", p->name,
			srcMapInfo->mapIndex,
			p->srcRect.left, p->srcRect.top, p->srcRect.right, p->srcRect.bottom,
			dstMapInfo->mapIndex,
			p->dstVec[0], p->dstVec[1], p->dstVec[2]);
	}

	fclose(fp);
}

mapInfo_t *MMap::GetMapInfo(int x, int y)
{
	if (!g_mapInfoInitialized)
		LoadMapInfo();

	return &g_mapInfo[y][x];
}

bool MMap::SetMapInfo(int x, int y, int sizex, int sizey, char *mapname)
{
	for (int j = 0; j < sizey; j++)
	{
		for (int i = 0; i < sizex; i++)
		{
			mapInfo_t *mi = &g_mapInfo[y + j][x + i];
			if (mi->reserved)
			{
				assert(0);
				return false;
			}
		}
	}

	g_mapIndexCount++;

	for (int j = 0; j < sizey; j++)
	{
		for (int i = 0; i < sizex; i++)
		{
			mapInfo_t *mi = &g_mapInfo[y + j][x + i];

			mi->mapIndex = g_mapIndexCount;
			mi->x = x;
			mi->y = y;
			mi->sizex = sizex;
			mi->sizey = sizey;
			mi->occupied = true;
			strcpy(mi->mapname, mapname);
		}
	}

	return true;
}

void MMap::ResetMapInfo(mapInfo_t *mapInfo)
{
	for (int j = 0; j < mapInfo->sizey; j++)
	{
		for (int i = 0; i < mapInfo->sizex; i++)
		{
			mapInfo_t *mi = &g_mapInfo[mapInfo->y + j][mapInfo->x + i];
			mi->occupied = false;
		}
	}
}

void MMap::InitPortalList()
{
	 g_portalList.next = &g_portalList;
	 g_portalList.prev = &g_portalList;
}

void MMap::FreePortalList()
{
	portal_t *next;

	for (portal_t *p = g_portalList.next; p != &g_portalList; p = next)
	{
		next = p->next;
		delete p;
	}
}

portal_t *MMap::FindPortal(const char *name)
{
	for (portal_t *p = g_portalList.next; p != &g_portalList; p = p->next)
		if (!stricmp(p->name, name))
			return p;
	return NULL;
}

bool MMap::AddPortal(const char *name, const RECT *srcRect, const int dstVec[3])
{
	portal_t *newportal;

	if (FindPortal(name))
		return false;
	
	newportal = new portal_t;
	strcpy(newportal->name, name);
	newportal->srcRect = *srcRect;
	memcpy(newportal->dstVec, dstVec, sizeof(int) * 3);

	newportal->next = g_portalList.next;
	g_portalList.next->prev = newportal;
	newportal->prev = &g_portalList;
	g_portalList.next = newportal;

	return true;
}

bool MMap::RemovePortal(const char *name)
{
	portal_t *p = FindPortal(name);
	if (p)
	{
		p->prev->next = p->next;
		p->next->prev = p->prev;
		delete p;
		return true;		
	}

	return false;
}

portal_t *MMap::GetPortalList()
{
	return &g_portalList;
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////

static const D3DXVECTOR3	LuminanceVector(0.2125f, 0.7154f, 0.0721f);

// x, y 는 서버 패치 좌표 (0 ~ 63)
BOOL	MMap::Load(LPCSTR szPath, int x, int y)
{
	int i, j;

#ifndef _SOCKET_TEST
	// 파일이름이 NULL 이면 mapInfo 테이블에서 파일이름을 얻어온다
	if (!szPath)
	{
		mapInfo_t *mapInfo = GetMapInfo(x, y);
		if (!mapInfo->occupied)
		{
			//Unload();
			return FALSE;
		}

		szPath = mapInfo->mapname;
	}
#endif

	XSystem::m_fogMaxEnd = 2000.0f;

    // 컨피그로 부터 맵 정보를 가져온다.
	for (i = 0; i < 64; i++)
	{
		for (j = 0; j < 64; j++)
		{
			KMacro::MapInfo *mapInfo = KMacro::FindMapInfo(i, j);
			if (mapInfo)
			{
				if (!stricmp(mapInfo->mapname.c_str(), szPath))
				{
					if (mapInfo->fogEnd > XSystem::m_fogMaxEnd)
						XSystem::m_fogMaxEnd = mapInfo->fogEnd;
				}
			}
		}
	}

	// 이미 로딩 되어 있거나 패스가 같다면 통과
	if (IsLoaded() && !stricmp(m_szPath, szPath))
		return TRUE;

    // 로딩 자료 제거
	Unload();

    // 파일 패스 복사
	strcpy(m_szPath, szPath);

    // 파일 생성
	XFileCRC file;
	char path[MAX_PATH];
	wsprintf(path, "%s.map", szPath);

    // 파일 로딩
	if (!file.Open(path))
		return FALSE;

    // 버전 입력
	BYTE version = *file.m_pView;

#ifndef	_SOCKET_RELEASE
	if (version < MAP_VERSION_NO(2))
		return FALSE;
#endif

	MAP_HEADER	header;

    // 맵 버전 에 따른 헤더 로딩
	if (version == MAP_VERSION) 
    {
        // 헤더 로딩 및 crc 생성
		file.XFile::Read(&header, sizeof(header));
		file.m_nCRC = UpdateCRC(*(DWORD *) &header ^ MAP_CRC_SEED, (char *) &header + 8, sizeof(header) - 8);
	}
	else
    {
		memset(&header, 0, sizeof(header));
		file.XFile::Read(&header, sizeof(DWORD));
		if (version >= MAP_VERSION_NO(2))
			file.XFile::Read(&header.crc, sizeof(header.crc));
		file.m_nCRC = *(DWORD *) &header ^ MAP_CRC_SEED;
		file.Read(&header.leaf_count, sizeof(header.leaf_count));
		file.Read(&header.part_count, sizeof(header.part_count));
		file.Read(&header.string_size, sizeof(header.string_size));
		file.Read(&header.maximum, sizeof(header.maximum));
		file.Read(&header.minimum, sizeof(header.minimum));
		file.Read(&header.option, sizeof(header.option));
		file.Read(&header.light_count, sizeof(header.light_count));
		file.Read(&header.object_count, sizeof(header.object_count));
		file.Read(&header.reserved2, sizeof(header.reserved2));
	}

    // 민맥스(맵의 최소 최대 크기 일듯) 설정
	m_vMin = header.minimum;
	m_vMax = header.maximum;

    // 총 사이즈 계산
	int size = ALIGN(header.string_size);
	size += ALIGN(header.leaf_count * sizeof(MLeaf));
	size += ALIGN(header.node_count * sizeof(MNode));
	size += ALIGN(header.light_count * sizeof(LObject));
	size += ALIGN(header.region_count * sizeof(MRegion));
	size += ALIGN(header.part_count * sizeof(MPart));

    // 총 사이즈 만큼 메모리 할당
	char *pMemory = (char *) malloc(size);

    // 시작 포인터 지정
	m_pMemory = pMemory;

    // 문자열 로딩
	file.Read(m_pMemory, header.string_size);
	pMemory += ALIGN(header.string_size);

    // 리프 로딩
	m_nLeaf = header.leaf_count;
	m_pLeaf = (MLeaf *) pMemory;
	pMemory += ALIGN(header.leaf_count * sizeof(MLeaf));

    // 리프 데이터 로딩
	MLeaf *pLeaf = m_pLeaf;
	for (int i = 0; i < m_nLeaf; i++) 
    {
		MAP_LEAF map_leaf;
		file.Read(&map_leaf, sizeof(MAP_LEAF));
		new (pLeaf) MLeaf;
		pLeaf->m_nLeafList = map_leaf.leaf_list << 3;
		pLeaf++;
	}

    // 노드 로딩
	m_nNode = header.node_count;
	m_pNode = (MNode *) pMemory;
	pMemory += ALIGN(header.node_count * sizeof(MNode));

    // 노드 데이터 로딩
	MNode *pNode = m_pNode;
	for (int i = 0; i < m_nNode; i++) 
    {
		MAP_NODE map_node;
		file.Read(&map_node, sizeof(MAP_NODE));
		new (pNode) MNode;
		pNode->m_plane = map_node.plane;
		pNode->m_pLeaf = ((short) map_node.leaf >= 0) ? &m_pLeaf[map_node.leaf] : 0;
		pNode->m_pSides[0] = ((short) map_node.side[0] >= 0) ? &m_pNode[map_node.side[0]] : 0;
		pNode->m_pSides[1] = ((short) map_node.side[1] >= 0) ? &m_pNode[map_node.side[1]] : 0;
		pNode++;
	}

    // 광원 관리 클래스(LObject) 로딩
	m_nLight = header.light_count;
	m_pLight = (LObject *) pMemory;
	pMemory += ALIGN(header.light_count * sizeof(LObject));

    // 광원 데이터 로딩
	LObject	*pLight = m_pLight;
	for (int i = 0; i < m_nLight; i++) 
    {
		MAP_LIGHT map_light;
		file.Read(&map_light, sizeof(MAP_LIGHT));
		new (pLight) LObject;
		pLight->m_szName = &m_pMemory[map_light.name];
		pLight->SetCenter(map_light.position);
		pLight->m_fRadius = map_light.atten_end;
		pLight->m_vExtent = D3DXVECTOR3(map_light.atten_end, map_light.atten_end, map_light.atten_end);
		pLight->m_nType = (D3DLIGHTTYPE) map_light.type;
		pLight->m_nDecayType = (LIGHT_DECAY_TYPE) map_light.decay_type;
		float fIntensity = map_light.intensity / 255.0f;
		pLight->m_vColor.x = (BYTE) (map_light.color >> 16) * fIntensity;
		pLight->m_vColor.y = (BYTE) (map_light.color >> 8) * fIntensity;
		pLight->m_vColor.z = (BYTE) (map_light.color)  * fIntensity;
		pLight->m_vColor.w = D3DXVec3Dot((D3DXVECTOR3 *) &pLight->m_vColor, &LuminanceVector);
			// refer to D3DXColorAdjustSaturation()'s documentation
		pLight->m_fDecayRadius = map_light.decay_radius;
		pLight->m_fAttenStart = map_light.atten_start;
		pLight->m_vDirection = map_light.direction;
		pLight->m_fTheta = cosf(map_light.theta * 0.5f);
		pLight->m_fPhi = cosf(map_light.phi * 0.5f);
		LEngine::InsertObject(pLight);
		pLight++;
	}

    // 지역 로딩
	m_nRegion = header.region_count;
	m_pRegion = (MRegion *) pMemory;
	pMemory += ALIGN(header.region_count * sizeof(MRegion));

    // 지역 데이터 로딩
	MRegion *pRegion = m_pRegion;
	MPart	*pPart = (MPart *) pMemory;
	for (int i = 0; i < m_nRegion; i++) 
    {
		MAP_REGION	map_region;
		file.Read(&map_region, sizeof(MAP_REGION));
		new (pRegion) MRegion;
		pRegion->m_szName = &m_pMemory[map_region.name];
		pRegion->m_vCenter = (map_region.minimum + map_region.maximum) * 0.5f;
		pRegion->m_vExtent = map_region.maximum - (map_region.minimum + map_region.maximum) * 0.5f;
		pRegion->m_nLeafList = map_region.leaf_list << 3;
		pRegion->m_nPart = map_region.part_count;
		pRegion->m_pPart = pPart;
		for (int j = 0; j < pRegion->m_nPart; j++) {
			MAP_PART map_part;
			file.Read(&map_part, sizeof(MAP_PART));
			new (pPart) MPart;
			pPart->m_szName = &m_pMemory[map_part.name];
			pPart->m_vCenter = (map_part.minimum + map_part.maximum) * 0.5f;
			pPart->m_vExtent = map_part.maximum - pPart->m_vCenter;
			pPart++;
		}
		pRegion->Create();
		pRegion++;
	}

	m_pModelList = 0;
	D3DXMATRIXA16 matrix;
	D3DXMatrixIdentity(&matrix);

	char map_path[MAX_PATH];
	strcpy(map_path, "MAP\\");

    // 지형 위의 오브젝트 로딩
	for (int i = 0; i < header.object_count; i++) 
    {
		MAP_OBJECT	map_object;
		file.Read(&map_object, sizeof(MAP_OBJECT));
		*(D3DXVECTOR3 *) &matrix._11 = *(D3DXVECTOR3 *) &map_object.matrix[0];
		*(D3DXVECTOR3 *) &matrix._21 = *(D3DXVECTOR3 *) &map_object.matrix[3];
		*(D3DXVECTOR3 *) &matrix._31 = *(D3DXVECTOR3 *) &map_object.matrix[6];
		*(D3DXVECTOR3 *) &matrix._41 = *(D3DXVECTOR3 *) &map_object.matrix[9];
		
		strcpy(map_path + 4, &m_pMemory[map_object.path]);
		XBaseModel *pModel = XBaseModel::Create(map_path, matrix);

		if (pModel) 
        {
			pModel->m_pNext = m_pModelList;
			m_pModelList = pModel;
		}
	}

	OnSetCamera();

#ifndef _SOCKET_TEST
	g_svMapCoord.x = x;
	g_svMapCoord.y = y;
#endif
    
	if (version >= 2)
    {
		if (file.m_nCRC != header.crc || stricmp(GetNamePart(path), m_pMemory) != 0) 
        {
			Unload();
			return FALSE;
		}
	}

	m_bViewUpdate = true;

	return TRUE;
}


void	MMap::Unload()
{
	if (IsLoaded())
	{
		for ( ; ; )
		{
			XBaseModel *pBaseModel = m_pModelList;
			if ( pBaseModel == NULL )
				break;

			m_pModelList = m_pModelList->m_pNext;

			for ( ; ; )
			{
				XBaseModel *pOuter = pBaseModel->m_pOuter;
				if ( pOuter == NULL )
					break;

				pBaseModel->m_pOuter = pOuter->m_pOuter;
				pOuter->FreeModel();
			}
			pBaseModel->FreeModel();
		}

		RemoveAllObject(m_pNode);

		if ( m_pRegion != NULL )
		{
			MRegion *pRegion = m_pRegion;
			for (int i = 0; i < m_nRegion; i++)
			{
				MPart *pPart = pRegion->m_pPart;
				for (int j = 0; j < pRegion->m_nPart; j++)
				{
					pPart->~MPart();
					pPart++;
				}
				pRegion->~MRegion();
				pRegion++;
			}
		}

		if ( m_pLight != NULL )
		{
			LObject *pLight = m_pLight;
			for (int i = 0; i < m_nLight; i++) {
				LEngine::RemoveObject(pLight);
				pLight->~LObject();
				pLight++;
			}
		}

		if ( m_pNode != NULL )
		{
			MNode *pNode = m_pNode;
			for (int i = 0; i < m_nNode; i++) {
				pNode->~MNode();
				pNode++;
			}
		}

		if ( m_pLeaf != NULL )
		{
			MLeaf *pLeaf = m_pLeaf;
			for (int i = 0; i < m_nLeaf; i++) {
				pLeaf->~MLeaf();
				pLeaf++;
			}
		}

		free(m_pMemory);
		m_pMemory = 0;
		m_pNode = 0;
		m_pCameraLeaf = 0;
	}

	D3DLIGHT9 d3dLight;
	memset(&d3dLight, 0, sizeof(d3dLight));
	d3dLight.Type = D3DLIGHT_DIRECTIONAL;
	d3dLight.Ambient = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Diffuse = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Specular = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Direction = D3DXVECTOR3(1, -1, 1);
	D3DXVec3Normalize((D3DXVECTOR3 *)&d3dLight.Direction, (D3DXVECTOR3 *) &d3dLight.Direction);
	XSystem::m_pDevice->SetLight(0, &d3dLight);
	XSystem::m_pDevice->LightEnable(0, TRUE);
	XSystem::SetLightDirection(*(D3DXVECTOR3 *) &d3dLight.Direction);

	D3DMATERIAL9 mtrl;
	mtrl.Ambient = D3DXCOLOR(0.5f, 0.5f, 0.5f, 1);
	mtrl.Diffuse = D3DXCOLOR(1, 1, 1, 1);
	mtrl.Specular = D3DXCOLOR(1, 1, 1, 1);
	mtrl.Emissive = D3DXCOLOR(0, 0, 0, 1);
	mtrl.Power = 10;
	XSystem::m_pDevice->SetMaterial( &mtrl );
	XSystem::SetMaterial(mtrl);

	m_bViewUpdate = false;

#ifndef _SOCKET_TEST
	SAFE_RELEASE(g_pSvMapTexture[0]);
	SAFE_RELEASE(g_pSvMapTexture[1]);
	SAFE_RELEASE(g_pSvMapTexture[2]);
	SAFE_RELEASE(g_pSvMapTexture[3]);
#endif
}

void	MMap::InsertRegion(MNode *pNode)
{
	if (pNode->m_pLeaf && GetBit()) {
		MRegionLink *pLink = XConstruct<MRegionLink>();
		pLink->m_dwBitIndex  = m_dwBitIndex;
		pLink->m_pLeaf = pNode->m_pLeaf;
		pLink->m_pRegion = m_pCurrentRegion;
		pNode->m_pLeaf->m_vRegions.push_back(pLink);
		m_pCurrentRegion->m_vLeaves.push_back(pLink);
		m_dwBitIndex += m_pCurrentRegion->m_nPart;
	}

	if (pNode->m_pSides[0] && GetBit()) {
		InsertRegion(pNode->m_pSides[0]);
	}
	if (pNode->m_pSides[1] && GetBit()) {
		InsertRegion(pNode->m_pSides[1]);
	}
}

void	MMap::OnSetCamera()
{
	D3DXVECTOR3 vCamera = XSystem::m_vCamera + 
		(XSystem::m_vNear.z + 0.05f * METER) * *(D3DXVECTOR3 *) &XSystem::m_mViewInv._31;
//	D3DXVECTOR3 vCamera = XSystem::m_vCamera;
	m_pCameraLeaf = 0;
	MNode *pNode = m_pNode;
#ifdef	_SOCKET_TEST
	for ( ;pNode; ) {
		if (pNode->m_pLeaf) {
			m_pCameraLeaf = pNode->m_pLeaf;
		}
		if (D3DXPlaneDotCoord(&pNode->m_plane, &vCamera) < 0)
			pNode = pNode->m_pSides[0];
		else
			pNode = pNode->m_pSides[1];
	}
#else
	for ( ;pNode; ) {
		if (D3DXPlaneDotCoord(&pNode->m_plane, &vCamera) < 0) {
			pNode = pNode->m_pSides[0];
		}
		else {
			if (pNode->m_pSides[1] == 0 && pNode->m_pLeaf) {
				m_pCameraLeaf = pNode->m_pLeaf;
			}
			pNode = pNode->m_pSides[1];
		}
	}
#endif
}

void	MMap::RenderMap()
{
	if (!m_pCameraLeaf)
	{
		return;
	}

    // 카메라 노드의 영역을 렌더링
	for (MRegionList::iterator it = m_pCameraLeaf->m_vRegions.begin(); it != m_pCameraLeaf->m_vRegions.end(); ) {
		MRegionLink *pLink = *it++;
		SetBitIndex(pLink->m_dwBitIndex);
		pLink->m_pRegion->RenderRegion();
	}
}

void	MMap::InsertObject(MNode *pNode)
{
	float d = D3DXPlaneDotCoord(&pNode->m_plane, &m_pCurrentObject->m_vCenter); 
	float r = m_pCurrentObject->m_fRadius;
	if (d + r >= 0) {
		if (!pNode->m_pSides[1]) {
			if (pNode->m_pLeaf) {
				MObjectLink *pLink = XConstruct<MObjectLink>();
				pLink->m_pLeaf = pNode->m_pLeaf;
				pLink->m_pObject = m_pCurrentObject;
				pNode->m_pLeaf->m_vObjects.push_back(pLink);
				m_pCurrentObject->m_vLeaves.push_back(pLink);
			}
			return;
		}
		InsertObject(pNode->m_pSides[1]);
	}
	if (d < r) {
		if (!pNode->m_pSides[0]) {
			return;
		}
		InsertObject(pNode->m_pSides[0]);
	}
}

void	MMap::RemoveAllObject(MNode *pNode)
{
	if (pNode == 0)
		return;
	if (pNode->m_pLeaf) {
		MObjectList &vObjects = pNode->m_pLeaf->m_vObjects;
		for (MObjectList::iterator it = vObjects.begin(); it != vObjects.end(); ) {
			MObjectLink *pLink = *it++;
			pLink->m_pObject->m_vLeaves.erase(pLink);
			XDestruct(pLink);
		}
		vObjects.clear();
	}
	RemoveAllObject(pNode->m_pSides[0]);
	RemoveAllObject(pNode->m_pSides[1]);
}

void	MMap::RenderObject(MNode *pNode)
{
	if (pNode->m_pLeaf && GetBit()) {
		MObjectList &vObjects = pNode->m_pLeaf->m_vObjects;
		for (MObjectList::iterator it = vObjects.begin(); it != vObjects.end(); ) {
			XObject *pObject = (*it++)->m_pObject;
			if (pObject->m_nRenderNumber != XSystem::m_nRenderNumber) {
				pObject->RenderFlag(0x3f);
			}
		}
	}
	if (pNode->m_pSides[0] && GetBit()) 
		RenderObject(pNode->m_pSides[0]);
	if (pNode->m_pSides[1] && GetBit()) 
		RenderObject(pNode->m_pSides[1]);
}

MRegion::~MRegion()
{
	XBaseModel::FreeModel();
	MMap::RemoveRegion(this);
}

void	MRegion::Create()
{
	wsprintf(Super::m_szName, "%s\\%s", MMap::m_szPath, m_szName);
	D3DXMatrixIdentity(&m_matrix);
	D3DXMatrixIdentity(&m_inverse);
	XBaseModel::InitXModel(m_vCenter, m_vExtent);
}

void	MRegion::OnEnter()
{
//	TRACE("MRegion::OnEnter %s\n", Super::m_szName);
	CStaticModel::Load(Super::m_szName, CModelFile::NO_CACHE, this, (XCallee::FUNCTION) &MRegion::OnLoadModel, GetModelPriority());
}

void	MRegion::OnLeave()
{
//	TRACE("MRegion::OnLeave %s\n", Super::m_szName);
	Super::OnLeave();
	MMap::RemoveRegion(this);
}

void	MRegion::OnLoadModel()
{
	Super::OnLoadModel();
	if (m_nMesh != m_nPart) {
		BREAK();
		Destroy();
	}
	MMap::InsertRegion(this);
}

void	MRegion::RenderRegion()
{
	m_nRenderNumber = XSystem::m_nRenderNumber;

	if (XEngine::m_nAction & XEngine::RENDER)
		m_nFrameNumber = XSystem::m_nFrameNumber;
//	PreRender();
//	XSystem::m_pDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);	
	OnPreRender();
	XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &m_matrix);
	XSystem::m_pDevice->SetVertexShader(0);

    // 해당 메쉬가 지역 안에 존재한다면 랜더링 시켜준다.
	for (int i = 0; i < m_nMesh; i++)
    {
		if (MMap::GetBit() && XSystem::IsBoxIn(m_pPart[i].m_vCenter, m_pPart[i].m_vExtent))
		{
			CMesh *pMesh = m_vpMesh[i];
			pMesh->RenderStatic();
		}
	}
}

void	MRegion::RenderFlag(int flag)
{
	m_nRenderNumber = XSystem::m_nRenderNumber;
	if (flag > 0) {
		flag = XSystem::IsBoxIn(m_vCenter, m_vEdge, m_matrix, flag);
		if (flag < 0)
			return;
	}

	if (XEngine::m_nAction & XEngine::RENDER)
		m_nFrameNumber = XSystem::m_nFrameNumber;
	Render();
}

#ifdef _TOOL
void MMap::SwitchingHide()
{
    XBaseModel* pModel = m_pModelList;

    while (pModel)
    {
        pModel->m_bHide = !pModel->m_bHide;
        pModel = pModel->m_pNext;
    }
}
#endif