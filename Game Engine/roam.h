#ifndef WARJO_ROAM_H
#define WARJO_ROAM_H

#include <math.h>
#include <stdio.h>
#include "utility.h"
#include "XSystem.h"	
#include "model.h"
#include "XModel.h"
#include "XFile.h"

#define WJ_VERTEXBUFFERSIZE (VB_SIZE/sizeof(TERRAIN_VERTEX))
#define	WJ_INDEXBUFFERSIZE	32768

#define MAP_LEVEL		8  // 256
#define PATCH_LEVEL	6	// 64

#define MAP_LAYER	8

#define CELL_SIZE	32
#define WATER_POS_Y ( (CELL_SIZE * 99) / 2.0f)

#define MAP_SIZE	(1 << MAP_LEVEL)
#define	MAP_SIZE2	(MAP_SIZE * MAP_SIZE)

#define PATCH_SIZE  (1 << PATCH_LEVEL)
#define PATCH_SIZE2 (PATCH_SIZE * PATCH_SIZE)

#define MAX_DECO_KIND 256
#define MAX_DECO_PALETTE 8

typedef LPDIRECT3D9 LPWJD3D;
typedef LPDIRECT3DDEVICE9 LPWJD3DDEVICE;
typedef LPDIRECT3DVERTEXBUFFER9  LPWJD3DVERTEXBUFFER;

class Patch;
class PatchInfo;

//custom vetex definition for dx8
struct TERRAIN_VERTEX
{
	D3DXVECTOR3 position; // The 3D position for the vertex
};


// 
class PerPatchOp
{
public:
	virtual void exec( Patch* p, int flag) {}
};

class PerTriOp
{
public:
	virtual void exec( Patch* p, D3DXVECTOR3& rCenter) {}
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_TERRAIN (D3DFVF_XYZ)

//******************************************************************
//
// TriangleTreeNode
//
// The node in my tesselation engine
//
struct TriangleTreeNode
{
	//conectivity information
	TriangleTreeNode *LChild;		

	//down/base neighbor
	TriangleTreeNode *DNeighbor;	

	struct nonterminal
	{
		TriangleTreeNode *RChild;
		short	index;
		BYTE	mergeable;
	};

	struct terminal
	{
		TriangleTreeNode *LNeighbor;
		TriangleTreeNode *RNeighbor;
	};

	union
	{
		nonterminal nonterm;
		terminal term;
	}; 

	static int count;

	//id of the patch this node is child of

	//constructor
	TriangleTreeNode();

	void	Init(TriangleTreeNode *left,  TriangleTreeNode **left_ptr,
							   TriangleTreeNode *right, TriangleTreeNode **right_ptr, 
							   TriangleTreeNode *down,  TriangleTreeNode **down_ptr);
	void	Destroy();
	
	

	//memory operators
	void* operator new(size_t) { 	count++; return CMemoryPool<sizeof(TriangleTreeNode)>::Alloc(); }
	void operator delete(void *p) {	count--; CMemoryPool<sizeof(TriangleTreeNode)>::Free(p); }

	//split is quite the same as in the original algorythm...
	void Split(); 

	//merges single node keeping the connections...
	void Merge(); 

	//is this triangle node good for merge?
	inline BOOL GoodForMerge();
	inline BOOL	IsTerminal() { return !LChild; }	

	//go down in the tree and merges the leafs if good for merge
	void MergeDown(); 

};

//***********************************************************************
//
// Patch 
//
// The heart of the ROAM algorytm.... represents one square patch
// of the landscape...
//
#include <vector>
#include <stack>

class Patch : public XPriorityMessage {
public:

	struct TREEINFO
	{
		BYTE variance;
		BYTE alpha_mask;
	};

	struct CVINFO
	{
		int	x;
		int	y;
		int	height;
		WORD alpha_mask;
	};

#pragma pack(push, 1)
	struct VERTEXINFO
	{
		WORD height;
		DWORD color;
	};
#pragma pack(pop)
	struct TREE_DATA
	{
		WORD variance;
		BYTE alpha_mask;
	};

    //yoffset corresponds to z...where is this patch in the space?

	int m_xoffset,m_yoffset;  
	int	m_hoffset;
	float	m_fHeightScaled, m_fMinScaled;
	float m_fScale;
	float m_fVariance;
	int	m_nLayer;
	int m_nDecoPalette;
	BYTE m_decoPalette[ MAX_DECO_PALETTE];
	int	m_nRef;
	
	//current variance tree
	TREEINFO *m_CVariance;        

	//the left and the right base children
	TriangleTreeNode m_LBase,m_RBase; 
	short ilt,irt,ilb,irb; 
	WORD	m_vHeight[(MAP_SIZE+1)*(MAP_SIZE+1)];
	LPDIRECT3DTEXTURE9	m_texColor[MAP_LAYER];
	CTexture	*m_pTexture[MAP_LAYER];
	XEvent		m_vEvent[MAP_LAYER];
	D3DXMATRIXA16	m_mWorld;
	D3DXMATRIXA16	m_mTextureVtx;
	D3DXMATRIXA16	m_mTexture[MAP_LAYER];
    float	m_fRepeat[MAP_LAYER];
	TREEINFO m_LVariance[MAP_SIZE2*2-1], m_RVariance[MAP_SIZE2*2-1]; 

	BYTE m_arDecolayer[ MAP_SIZE2];
	XBaseModel *m_pModelList;
	XFileCRC	m_file;
	XFileCRC	m_opl;
	PatchInfo	*m_pPatchInfo;
	BOOL	m_bCancel;
	DWORD	m_dwVersion;
	DWORD	m_dwChecksum;
	DWORD	m_dwOplChecksum;
	int		m_nModelCount;
	XFileCRC	*m_pOplView;

	//counts the variance computation - just for tracking
	static int var_cnt;

#ifdef DEF_LM_LETSBE_090202
	IDirect3DTexture9* m_pTexLightMap;
#endif // DEF_LM_LETSBE_090202

public:
							  
	//construction and initialization
	Patch();
	void	Destroy();
	void	AddRef() { m_nRef++; }
	void	Release() { if (--m_nRef == 0) Destroy(); }
	BOOL	Init(int x_off,int y_off, Patch *l, Patch *r, Patch *u, Patch *d);
	//load the bitmap for the file
	void	LoadAsync(PatchInfo *pPatchInfo, DWORD dwPriority);
	virtual void	RaisePriority(DWORD dwPriority);
	
	//resets the connections for new general tesselation...
	void	PreRender();
	void	PostRender();
	static void	PreRenderShadow();
	static void	PostRenderShadow();
	// void	RenderMiniMap( LPRECT pDest, LPRECT pClip);

	//tessellate recursive and iterative
	void Tesselate();

	//render
	void Render(int flag);
	void RenderShadow(int flag);

	//precompute the variance
	void ComputeVariance();

	//Precomputed variance
	//
	//recurseCV computes recursevly the variance down the tree...for
	//the current map ... variance is stored in the map so it should
	///not be computed for another patch that uses the same map...
	TREE_DATA RecurseCV(const CVINFO &left, const CVINFO &right, const CVINFO &apex);

	

	//tesselates the patch - recursive version
	void RecurseTesselate(TriangleTreeNode *tri,
							 int lx,  int ly,
							 int rx, int ry,
							 int ax,  int ay,
							 int flag);

	
    //insert 3 indices for this triangle
	inline void PutTriIndices(WORD i1,WORD i2,WORD i3, int alpha_mask);
	inline void PutTriIndicesShadow(WORD i1,WORD i2,WORD i3);

	//insert a vertex and return an index
	inline void PutVertex(int x, int z, short *p);
	
	//recurses the tree rendering... node,coordinates of the three points,
	//indeces for three points (0xffff) means the index is empty...
	void RecurseRender(TriangleTreeNode *tri,
							 int lx,  int ly,
							 int rx,  int ry,
							 int ax,  int ay,
							 short *il,short *ir,short *ia, int flag);
	
	void RecurseRenderShadow(TriangleTreeNode *tri,
							 int lx,  int ly,
							 int rx,  int ry,
							 int ax,  int ay,
							 short *il,short *ir,short *ia, int flag);

	void ForVisibleTile( int x, int y, int size, int flag, PerTriOp* op);

	WORD GetHeight(int x, int y) 
	{
		ASSERT(x >= 0 && x <= MAP_SIZE && y >= 0 && y <= MAP_SIZE);
		return m_vHeight[x + y * (MAP_SIZE+1)];
	}

	WORD GetHeight(int x, int y, int nMapping)
	{
		ASSERT(x >= 0 && x <= MAP_SIZE && y >= 0 && y <= MAP_SIZE);
		if (nMapping & 4) {
			int t = x;
			x = y;
			y = t;
		}
		if (nMapping & 2) {
			y = MAP_SIZE - y;
		}
		if (nMapping & 1) {
			x = MAP_SIZE - x;
		}
		return m_vHeight[x + y * (MAP_SIZE+1)];
	}

	BYTE GetDeco( int x, int y)
	{
		if( 0 <= x && 0 <= y && x < MAP_SIZE && y < MAP_SIZE)
			return m_arDecolayer[ x + y * MAP_SIZE];

		return 0;
	}

	inline WORD GetAlphaMask(int x, int y);
	void	Update();
	void	CollidePoint(const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vVelocity);

	void	OnRead();
	void	OnLoad();
	void	OnFail();
	BOOL	Load();
	void	LoadOpl();
	void	OnEvent();
	void	OnLoadOpl();
};

typedef std::vector<Patch *> PatchPtrVector;

class LayerInfo
{
public:
	LayerInfo() 
		: m_ScaleU( 0)
		, m_ScaleV( 0) {}

	int m_ScaleU;
	int m_ScaleV;
	std::string m_TextureName;

	BOOL Load( XFileCRC& rMapView)
	{
		rMapView.Read( &m_ScaleU, sizeof( int));
		rMapView.Read( &m_ScaleV, sizeof( int));
		int len;
		rMapView.Read( &len, sizeof( int));
		m_TextureName.resize( len);
		rMapView.Read( &m_TextureName[ 0], len);
		return TRUE;
	}
};

class	PatchInfo : public XBaseModel
{
public:
	int	m_x;
	int	m_y;
	Patch	*m_pLoading;
	Patch	*m_pPatch;

	virtual void	OnEnter();
	virtual void	OnLeave();
	virtual void	OnWait();
	virtual	void	FreeModel();
	void	LoadAsync();
	void	Unload();
};
//***********************************************************************
//
// Terrain
//
// the main terrain class
// contains the pathches, implements recursive quadtree
//
class Terrain
{
public:
	class Layer
	{
	public:
		IDirect3DIndexBuffer9 *m_pIB;
		WORD	*m_pIndex, *m_pIndexBegin, *m_pIndexEnd;
	};

protected:

	typedef std::vector< LayerInfo> t_LayerInfoTable;
	static t_LayerInfoTable m_LayerInfoTable;

	static void ForVisiblePatch( int cx, int cy, int half, int flag, PerPatchOp* op);

	typedef struct  _decoItem
	{
		CStaticModel* pModel;
		std::string name;
		_decoItem() { pModel = NULL;}
	} t_decoItem;

public:
	//side of single patch
	static LPWJD3DDEVICE m_pd3dDevice;
	static LPWJD3DVERTEXBUFFER m_pVB;
	static TERRAIN_VERTEX *m_pVertex, *m_pVertexBegin, *m_pVertexEnd;
	static Layer m_layer[MAP_LAYER];
	static short *m_vpIndex[WJ_VERTEXBUFFERSIZE];
	static short m_last_vcnt;
	static int m_vtx_cnt;
	static int m_tri_cnt;
	static WORD m_vAlphaMask[MAP_SIZE2];
	static int m_nNode;
	static int m_nLevel;
	static float m_vfRadius[MAP_LEVEL*2+1];
	static DWORD m_vMaskTable[1 << MAP_LAYER];
	static float m_fMinScaled, m_fHeightScaled;
	static float m_fVariance;
	static D3DXVECTOR3	m_vNormal;
	static float T;
	static BOOL	m_bWater;
	static float m_vExtent[3];

	static int m_nMaxDrawLayer;

#ifdef MAX_DECO_KIND
	static t_decoItem m_decoTable[MAX_DECO_KIND];
	static void ClearDecoTable();
	static void ValidateDecoTable();
#else
	static CStaticModel* m_pDeco;
#endif

	static PatchInfo	m_vPatchInfo[PATCH_SIZE2];
	static PatchPtrVector	m_vpPatch;
	//array of patches (maximum 16x16) - owner flag in the triangle tree is BYTE
	static std::string m_mapName;	
	static BOOL m_bLoaded;

// Meber functions

	//init and clear of direcx devices
	static void InitDevice(LPWJD3DDEVICE pdev);
	static BOOL	RestoreDevice();
	static void InvalidateDevice();
	static void DeleteDevice();
	static void	Reset();

	//Loads the height map and inits the patches
	static BOOL LoadMap(LPCTSTR mapName);
	static BOOL IsLoaded() { return m_bLoaded; }

	static void PreCache( D3DXVECTOR3 v);

	static void UnloadMap();
	static void LoadPatch( int i, int j);
	static LayerInfo* GetLayerInfo( unsigned int layer)
	{
		if( layer < m_LayerInfoTable.size())
			return &m_LayerInfoTable[ layer];
		return NULL;
	}
	static void Link( int i, int j, Patch* pPatch);

	static void Render(BOOL bWater);
	static void RenderDecoLayers();
	static void RenderDecal( float x, float y, float size, LPDIRECT3DTEXTURE9 texture, bool add = false, bool pointSample = false);
	static void RenderQuad(int cx,int cy, int half, int flag);
	static void RenderQuadShadow(int cx,int cy, int half, int flag);
	// static void RenderMiniMap( int i, int j, LPRECT pDest, LPRECT pClip);

	static void Tesselate();
	static void Update();
	static float GetHeight(float x, float z);
	static BOOL CollidePoint(const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vVelocity);
	static void	CollidePoint(CPoint pt, int nMapping, const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vVelocity);
};

inline WORD Patch::GetAlphaMask(int x, int y)	
{
	if ( x > MAP_SIZE-1) x = MAP_SIZE-1;
	if ( y > MAP_SIZE-1) y = MAP_SIZE-1;
	return Terrain::m_vAlphaMask[x + y * MAP_SIZE];
}

inline void Terrain::CollidePoint(CPoint pt, int nMapping, const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vVelocity)
{
	if (!(pt.x >= 0 && pt.x < PATCH_SIZE && pt.y >= 0 && pt.y < PATCH_SIZE))
		return;

	ASSERT(pt.x >= 0 && pt.x < PATCH_SIZE && pt.y >= 0 && pt.y < PATCH_SIZE);
	if (nMapping & 4) {
		int t = pt.x;
		pt.x = pt.y;
		pt.y = t;
	}
	if (nMapping & 2) {
		pt.y = PATCH_SIZE -1 - pt.y;
	}
	if (nMapping & 1) {
		pt.x = PATCH_SIZE -1 - pt.x;
	}
	Patch *patch = m_vPatchInfo[pt.x + pt.y * PATCH_SIZE].m_pPatch;
	if (!patch)
		return;
	patch->CollidePoint(vFrom, vVelocity);
}

inline void DrawPrimitiveUPTest( D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void *pVertexStreamZeroData, UINT VertexStreamZeroStride, UINT BufferSize)
{
	if( XSystem::m_pDevice && Terrain::m_pVB) {	
		void *pVertex = NULL;
		HRESULT hr = Terrain::m_pVB->Lock( 0, BufferSize, (void **) &pVertex, D3DLOCK_DISCARD);
		if( SUCCEEDED( hr) ) {
			memcpy( pVertex, pVertexStreamZeroData, BufferSize);
			Terrain::m_pVB->Unlock();
			XSystem::m_pDevice->SetStreamSource( 0, Terrain::m_pVB, 0, VertexStreamZeroStride);
			XSystem::m_pDevice->DrawPrimitive( PrimitiveType, 0, PrimitiveCount);
		}
	}
}

#endif