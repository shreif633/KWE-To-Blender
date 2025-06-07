#pragma once
#include "XModel.h"
#include <list>

#define SVMAP_VERSION				1
#define SVMAP_SIZE					(SVMAP_TILE_SIZE * SVMAP_TILE_COUNT) // 8192
#define SVMAP_CELL_COUNT			(SVMAP_TILE_COUNT * SVMAP_CELL_COUNT_PER_TILE) // 1024
#define SVMAP_CELL_COUNT_PER_TILE	(SVMAP_TILE_SIZE / SVMAP_CELL_SIZE)	// 4
#define SVMAP_CELL_SIZE				8
#define SVMAP_TILE_COUNT			256
#define SVMAP_TILE_SIZE				32

#define SVMAPMASK_TEST				1

#define TMAP_INVERSE				1
#define TMAP_OR						2

class MLeaf;
class MNode;
class MRegion;
class MPart;
class XObject;
class LObject;

enum eSeverMapAttr
{
	SMA_NOTMOVEABLE = 0,	// 못가는곳
	SMA_PORTAL,				// 포탈지역
	SMA_VILLAGE,			// 마을속성
	SMA_SAFETY,				// 안전지역
	SMA_ATTACK_CASTLE,		// 공성지역
	SMA_DEFENCE_CASTLE,		// 수성지역
	SMA_OBJECT				// 오브젝트지역

	/*
	던전과 필드의 서버맵 읽는 순서가 틀리다.
	던전에서는 기타 마을, 안전, 공성, 수성 관련한 속성을 쓰지 않아서 그동안 문제가 없었던거 같다.
	*/
};

class MRegionLink
{
public:
	CLink	m_linkRegion;
	CLink	m_linkLeaf;
	MLeaf	*m_pLeaf;
	MRegion	*m_pRegion;
	DWORD	m_dwBitIndex;

};

typedef linked_list<MRegionLink *> MRegionList;
typedef linked_list<MRegionLink *, offsetof(MRegionLink, m_linkLeaf)> MRegionLeafList;

typedef struct mapInfo_s {
    int				x, y;
    int				sizex, sizey;
    int				mapIndex;
    char			mapname[64];
    bool			occupied;
    bool			reserved;		// roam area	

#ifdef DEF_EFFECT_ADDON_AMB_BG_081001
    int             fogEnd;
    float           fogColor[3];
#endif

} mapInfo_t;
typedef struct portal_s {
	char			name[64];
	RECT			srcRect;
	int				dstVec[3];
	struct portal_s	*prev;
	struct portal_s *next;
} portal_t;

//
// 던전 맵용 네임 공간
//
namespace MMap
{
	extern char	m_szPath[MAX_PATH];
	extern char *m_pMemory;
	extern int		m_nLeaf;
	extern MLeaf	*m_pLeaf;
	extern int		m_nNode;
	extern MNode	*m_pNode;
	extern int		m_nLight;
	extern LObject	*m_pLight;
	extern int		m_nRegion;
	extern MRegion *m_pRegion;
	extern MRegion	*m_pCurrentRegion;
	extern DWORD	m_dwBitIndex;
	extern MLeaf	*m_pCameraLeaf;         //@ 카메라가 있는 공간
	extern XObject	*m_pCurrentObject;
	extern D3DXVECTOR3	m_vMax;
	extern D3DXVECTOR3	m_vMin;
	extern XBaseModel	*m_pModelList;
	extern bool m_bViewUpdate;

	/**
	 *  맵을 로딩한다
	 * \param szPath 맵의 디렉토리 및 파일명 패스?
	 * \param x 서버 패치 x?
	 * \param y 서버 패치 y?
	 * \return 
	 */
	BOOL	Load(LPCSTR szPath, int x, int y);

	/**
	 *  맵관련 자료들을 삭제한다
	 */
	void	Unload();

	inline BOOL	IsLoaded() { return m_pMemory != 0; }
	inline void		InsertRegion(MRegion *pRegion);
	void InsertRegion(MNode *pNode);
	inline void		RemoveRegion(MRegion *pRegion);
	inline BOOL GetBit()
	{ 
		DWORD nIndex = m_dwBitIndex++; 
		return (m_pMemory[nIndex>>3] & (1 << (nIndex & 7)));
	}
	inline void SetBitIndex(DWORD dwBitIndex)
	{
		m_dwBitIndex = dwBitIndex;
	}

	/**
	 *  맵을 랜더링 한다!!!(xengine에 의해 관리)
	 */
	void RenderMap();

	/**
	 *  매 랜더링 시마다 카메라를 검사하여 던전의 LOD를 체크한다.
	 * \date 2008-05-21
	 * \author oen
	 */
	void OnSetCamera();
	void InsertObject(MNode *pNode);
	inline void InsertObject(XObject *obj);
	inline void	RemoveObject(XObject *obj);
	void RenderObject(MNode *pNode);
	inline void RenderObject();
	
	void	SetSvMapEditMode(int bit);

	void	SetSvMapCoord(int x, int y);
	bool	LoadCurrentSvMap();
	void	SaveCurrentSvMap();	
	void	SaveAllSvMap();

	void	LoadMapInfo();
	void	SaveMapInfo();
	mapInfo_t	*GetMapInfo(int x, int y);
	bool	SetMapInfo(int x, int y, int sizex, int sizey, char *mapname);
	void	ResetMapInfo(mapInfo_t *mapInfo);

	void	InitPortalList();
	void	FreePortalList();
	portal_t *FindPortal(const char *name);
	bool	AddPortal(const char *name, const RECT *srcRect, const int dstVec[3]);	
	bool	RemovePortal(const char *name);
	portal_t *GetPortalList();

	DWORD	GetSvMap(int cx, int cy, int mask);
	void	SetSvMap(int cx, int cy, int mask);
	void	ResetSvMap(int cx, int cy, int mask);
    void    ResetTMap(int cx, int cy, int mask);
	void	GenerateSvMap();
	void	ClearSvMap();

	DWORD	GetTMap(int cx, int cy, int mask);
	void	SetTMap(int cx, int cy, int mask);
	void	ClearTMap();
	void	FillTMap(int cellx, int celly, int mask);
	void	ApplyTMap(int mask, int op);	

	void	RenderSvMap();
	void	ShowSvMapDebug(bool show);

#ifdef _TOOL
    void SwitchingHide();
#endif
}

/**
*  S-BSP 에서 최종 공간 정보
*
* \date 2008-05-22
* \write 
* \author oen
*/
class MLeaf
{
public:
	DWORD	m_nLeafList;        //@ 리프 리스트?
	MRegionList	m_vRegions;     //@ 리전 리스트?
	MObjectList	m_vObjects;     //@ 오브젝트 리스트?
};

/**
 *  S-BSP 의 공간 나눔 정보
 *
 * \date 2008-05-22
 * \write 
 * \author oen
 */
class MNode
{
public:
	D3DXPLANE	m_plane;        //@ 공간 분할 평명
	MLeaf		*m_pLeaf;       //@ 공간 데이터
	MNode		*m_pSides[2];   //@ 앞 뒤 노드
};

class MRegion : public XStaticModel
{
public:
	typedef	XStaticModel	Super;
	LPCSTR	m_szName;
	int		m_nPart;
	MPart	*m_pPart;
	DWORD	m_nLeafList;
	MRegionLeafList	m_vLeaves;

	~MRegion();
	void	Create();
	virtual void	OnEnter();
	virtual void	OnLeave();
	void	OnLoadModel();
	virtual void RenderFlag(int);
	void	RenderRegion();
	virtual void	OnInsertObject() {} 
};

class MPart
{
public:
	LPCSTR	m_szName;
	D3DXVECTOR3	m_vCenter;
	D3DXVECTOR3	m_vExtent;
};



inline void MMap::InsertRegion(MRegion *pRegion)
{
	m_dwBitIndex = pRegion->m_nLeafList;
	m_pCurrentRegion = pRegion;
	InsertRegion(m_pNode);
}

inline void MMap::RemoveRegion(MRegion *pRegion)
{
	for (MRegionLeafList::iterator it = pRegion->m_vLeaves.begin(); it != pRegion->m_vLeaves.end(); ) {
		MRegionLink *pLink = *it++;
		pLink->m_pLeaf->m_vRegions.erase(pLink);
		XDestruct(pLink);
	}
	pRegion->m_vLeaves.clear();
}

inline void	MMap::InsertObject(XObject *obj)
{
	m_pCurrentObject = obj;
	InsertObject(m_pNode);
}

inline void	MMap::RemoveObject(XObject *obj)
{
	for (MObjectLeafList::iterator it = obj->m_vLeaves.begin(); it != obj->m_vLeaves.end(); ) {
		MObjectLink *pLink = *it++;
		pLink->m_pLeaf->m_vObjects.erase(pLink);
		XDestruct(pLink);
	}
	obj->m_vLeaves.clear();
}

inline void MMap::RenderObject() 
{ 
	if (!m_pCameraLeaf)
		return;
	SetBitIndex(m_pCameraLeaf->m_nLeafList);
	RenderObject(m_pNode);
}
