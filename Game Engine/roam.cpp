#include "stdafx.h"
#include "roam.h"
#include "Utility.h"
#include "mmsystem.h"
#include "XFile.h"

#ifndef TERRAIN_EDITOR
#include "SunLight.h"
#include "KGameSys.h"
#include "KGameSysex.h"
#include "XMacro.h"
#endif

#ifdef DEF_MD5_OEN_071107
    #include "security/MD5Checksum.h"
#endif

#ifdef DEF_PUL_DIOB_20070610
const float c_fDecoFarRate = 0.054f;
#else
const float c_fDecoFarRate = 0.027f;
#endif

#ifdef _MAP_TEST
extern int g_nTestMapX;
extern int g_nTestMapY;
#endif

enum { MaxDecoPerFrame = 384 };

typedef struct decoItem_s {	
	float			distance;
	D3DXMATRIXA16	matrix;
	DWORD			tfactor;
	BYTE			mask;
	BYTE			decoPalette[MAX_DECO_PALETTE];
} decoItem_t;

class RenderDecoPerTile : public PerTriOp {
private:
	decoItem_t	m_items[ MaxDecoPerFrame];
	int			m_count;

public:
	static D3DXVECTOR3 s_vMaxDecoExtent;

	RenderDecoPerTile() : m_count( 0) {}

	void exec( Patch* p, D3DXVECTOR3& rCenter)
	{	
		if( m_count == MaxDecoPerFrame)
			return;		

		if( 0 != ( m_items[ m_count].mask = p->GetDeco( int( rCenter.x), int( rCenter.z))))
		{
			memcpy(m_items[m_count].decoPalette,p->m_decoPalette,MAX_DECO_PALETTE);

			D3DXVECTOR3 t( rCenter.x * CELL_SIZE + p->m_mWorld._41, 
				rCenter.y,
				rCenter.z * CELL_SIZE + p->m_mWorld._43);

			D3DXMatrixTranslation( &m_items[ m_count].matrix, t.x, t.y, t.z);
		
#ifdef MAX_DECO_KIND
			if( XSystem::IsBoxIn( t, s_vMaxDecoExtent, m_items[ m_count].matrix, 0x3f) < 0)
				return;
#else
			if( XSystem::IsBoxIn( t, Terrain::m_pDeco->m_vExtent, m_items[ m_count].matrix, 0x3f) < 0)
				return;
#endif
			
			// Rotation
			int x = int( rCenter.x);
			int z = int( rCenter.z);
			if( x <= MAP_SIZE && z <= MAP_SIZE)
			{
				float right_diff = ( p->GetHeight( x + 1, z) - rCenter.y);
				float up_diff = ( p->GetHeight( x, z + 1) - rCenter.y);
				float ry = atanf( fabsf( right_diff) / CELL_SIZE);
				if( right_diff < 0) ry *= -1;
				float rx = atanf( fabsf( up_diff) / CELL_SIZE);
				if( up_diff > 0) rx *= -1;
				D3DXMATRIXA16 rotation; 
				D3DXMatrixRotationYawPitchRoll( &rotation, 0, rx, ry); //y, x, z
				D3DXMatrixMultiply( &m_items[ m_count].matrix, &rotation, &m_items[ m_count].matrix);
			}

			t = t - XSystem::m_vCamera;

			float length = D3DXVec3Length( &t);
			float max = XSystem::m_vFar.z * c_fDecoFarRate;
			if (length > max)
				return;

			m_items[m_count].distance = length;

			//BYTE alpha = 0xFF;			
			//if( length / max > 0.5f)
			//	alpha = BYTE( ( 1.0f - ((length / max) - 0.5f) * 2.0f ) * 255.0f);
			BYTE alpha = (BYTE)(255.f * ((max - length) / max));

#ifdef TERRAIN_EDITOR
			D3DXCOLOR color = D3DXCOLOR(1.f, 1.f, 1.f, 1.f);
#else
			D3DXCOLOR color = KSunLight::GetColor();
#endif

			D3DCOLOR tfactor = D3DCOLOR_ARGB( alpha, (int)(color.r * 255), (int)(color.g * 255), (int)(color.b * 255));
			m_items[ m_count].tfactor = tfactor;
			++m_count;
		}
	}

	static int CDECL CompareDecoItem(const void *elem1,const void *elem2)
	{
		decoItem_t *item1 = (decoItem_t *)elem1;
		decoItem_t *item2 = (decoItem_t *)elem2;

		if (item1->distance > item2->distance)
			return -1;
		if (item1->distance < item2->distance)
			return 1;
		return 0;
	}

	void Flush()
	{
		qsort(m_items,m_count,sizeof(m_items[0]),CompareDecoItem);

		for (int i = 0 ; i < m_count; i++)	
		{				
			for (int kind = 0; kind < MAX_DECO_PALETTE; kind++)
			{
				int decoIndex = m_items[i].decoPalette[kind];
				if (m_items[ i].mask & ( 1 << kind) && NULL != Terrain::m_decoTable[decoIndex].pModel)
				{
					XSystem::m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, m_items[ i].tfactor);
					XSystem::m_pDevice->SetTransform( D3DTS_WORLD, &m_items[ i].matrix);
						
					Terrain::m_decoTable[decoIndex].pModel->DrawPrimitiveOnly();
				}
			}
		}
	}
};

D3DXVECTOR3 RenderDecoPerTile::s_vMaxDecoExtent;

class RenderDecoPerPatch : public PerPatchOp
{
public:
	RenderDecoPerPatch( PerTriOp* top) 
		: m_top( top) {}

	void exec( Patch *p, int flag) {
		ASSERT( p);
		p->ForVisibleTile( 0, 0, MAP_SIZE, flag, m_top);
	}

private:
	PerTriOp* m_top;
};

#define MAPFILE_VERSION		7
#define MAPFILE_VERSION_NO(x) x
#define MAPFILE_SEED			0xa0b0c0d0
//***********************************************************************
//
//TriangleTreeNode implementation
//

//-----------------------------------------------------------------------
//
// statics members of the Triangle node
//
#define SWAP(x, y) { float __t__ = x; x = y; y = __t__; }

#ifdef	_DEBUG

BOOL	IsPtIn(D3DXVECTOR2& p, D3DXVECTOR2& v1, D3DXVECTOR2& v2, D3DXVECTOR2& v3)
{
	D3DXVECTOR2 v12 = v2 - v1;
	D3DXVECTOR2 p1 = p - v1;
	if (D3DXVec2CCW(&v12, &p1) > -0.1f)
		return FALSE;
	D3DXVECTOR2 v23 = v3- v2;
	D3DXVECTOR2 p2 = p - v2;
	if (D3DXVec2CCW(&v23, &p2) > -0.1f)
		return FALSE;
	D3DXVECTOR2 v31 = v1 - v3;
	D3DXVECTOR2 p3 = p - v3;
	if (D3DXVec2CCW(&v31, &p3) > -0.1f)
		return FALSE;
	return TRUE;
}
/*
D3DXVECTOR2 l((float) (lx + m_xoffset) * 2, (float) (ly + m_yoffset) * 2);
D3DXVECTOR2 r((float) (rx + m_xoffset) * 2, (float) (ry + m_yoffset) * 2);
D3DXVECTOR2 a((float) (ax + m_xoffset) * 2, (float) (ay + m_yoffset) * 2);
D3DXVECTOR2 p(257, 731);
if (IsPtIn(p, a, r, l))
	printf("");  
*/
#endif

int TriangleTreeNode::count=0;   

//-----------------------------------------------------------------------
//
// constructor
//
TriangleTreeNode::TriangleTreeNode()
{
	LChild=NULL;
	//index = -1;
}

void	TriangleTreeNode::Init(TriangleTreeNode *left,  TriangleTreeNode **left_ptr,
							   TriangleTreeNode *right, TriangleTreeNode **right_ptr, 
							   TriangleTreeNode *down,  TriangleTreeNode **down_ptr)
{
	ASSERT(!left_ptr || !*left_ptr);
	ASSERT(!right_ptr || !*right_ptr);
	ASSERT(!down_ptr || !*down_ptr);
	if (down && !down->IsTerminal() || left && !left->IsTerminal() || right && !right->IsTerminal()) {
		TriangleTreeNode *LHalf, *RHalf;
		TriangleTreeNode **LPtr, **RPtr;
		if (down) {
			if (down->IsTerminal()) {
				if (down_ptr == &down->term.LNeighbor) {
					down->Split();
					down = down->LChild;
					down_ptr = &down->DNeighbor;
				}
				else if (down_ptr == &down->term.RNeighbor) {
					down->Split();
					down = down->nonterm.RChild;
					down_ptr = &down->DNeighbor;
				}
                down->Split();
			}
			
			LHalf = down->LChild;
			if (!LHalf->IsTerminal()) {
				LHalf = LHalf->nonterm.RChild;
				LPtr = &LHalf->DNeighbor;
			}
			else
				LPtr = &LHalf->term.RNeighbor;
			RHalf = down->nonterm.RChild;
			if (!RHalf->IsTerminal()) {
				RHalf = RHalf->LChild;
				RPtr = &RHalf->DNeighbor;
			}
			else
				RPtr = &RHalf->term.LNeighbor;
		}
		else {
			LHalf = RHalf = 0;
			LPtr = RPtr = 0;
		}
		LChild = new TriangleTreeNode();
		nonterm.RChild = new TriangleTreeNode();
		LChild->Init(NULL, NULL, RHalf, RPtr, left, left_ptr);
		if (LChild->IsTerminal()) {
			RHalf = LChild;
			RPtr = &LChild->term.LNeighbor;
		}
		else {
			RHalf = LChild->LChild;
			RPtr = &RHalf->DNeighbor;
		}
		nonterm.RChild->Init(LHalf, LPtr, RHalf, RPtr, right, right_ptr);
		nonterm.index = -1;
		nonterm.mergeable = FALSE;
	}
	else {
		term.LNeighbor = left;
		term.RNeighbor = right;
		if (left_ptr)
			*left_ptr = this;
		if (right_ptr)
			*right_ptr = this;
	}
	DNeighbor = down;
	if (down_ptr) 
		*down_ptr = this;
}

void	TriangleTreeNode::Destroy()
{
	if (!IsTerminal()) {
		LChild->Destroy();
		delete LChild;
		nonterm.RChild->Destroy();
		delete nonterm.RChild;
	}
	else {
		if (term.LNeighbor) {
			ASSERT(term.LNeighbor->LChild == 0); // LNeighbor is terminal
			if (term.LNeighbor->DNeighbor == this) {
				term.LNeighbor->DNeighbor = 0;
			}
			else if (term.LNeighbor->term.LNeighbor == this)
				term.LNeighbor->term.LNeighbor = 0;
			else {
				ASSERT(term.LNeighbor->term.RNeighbor == this);
				term.LNeighbor->term.RNeighbor = 0;
			}
		}
		if (term.RNeighbor) {
			ASSERT(term.RNeighbor->IsTerminal()); // RNeighbor is terminal
			if (term.RNeighbor->DNeighbor == this) {
				term.RNeighbor->DNeighbor = 0;
				if (!term.RNeighbor->IsTerminal() && term.RNeighbor->nonterm.mergeable  && term.RNeighbor->GoodForMerge())
					term.RNeighbor->Merge();
			}
			else if (term.RNeighbor->term.LNeighbor == this)
				term.RNeighbor->term.LNeighbor = 0;
			else {
				ASSERT(term.RNeighbor->term.RNeighbor == this);
				term.RNeighbor->term.RNeighbor = 0;
			}
		}
	}
	if (DNeighbor) {
		if (DNeighbor->DNeighbor == this) {
			DNeighbor->DNeighbor = 0;
			if (!DNeighbor->IsTerminal() && DNeighbor->nonterm.mergeable && DNeighbor->GoodForMerge()) 
				DNeighbor->Merge();
		}
		else {
			ASSERT(DNeighbor->IsTerminal());
			if (DNeighbor->term.LNeighbor == this)
				DNeighbor->term.LNeighbor = 0;
			else {
				ASSERT(DNeighbor->term.RNeighbor == this);
				DNeighbor->term.RNeighbor = 0;
			}
		}
	}

}
//-----------------------------------------------------------------------
//
// GoodForMerge determines if this node's children are leaves and whether
// they are ready to be merged (i.e. the variance is high enough)
//
inline BOOL TriangleTreeNode::GoodForMerge()
{
	//already merged?
	ASSERT(!IsTerminal());

	//there are no grandchildren
	if (LChild->IsTerminal() && nonterm.RChild->IsTerminal()) 
	{
		return TRUE;
	}
	return FALSE;
}
//-----------------------------------------------------------------------
//
// merge down goes down in the tree and when leafs are found they are 
// deleted so the parent node remains
//
void TriangleTreeNode::MergeDown()
{
	ASSERT(!IsTerminal()); 

	nonterm.mergeable = TRUE;
	if (!LChild->IsTerminal())
		LChild->MergeDown();
	if (!nonterm.RChild->IsTerminal())
		nonterm.RChild->MergeDown();
	if (GoodForMerge()) 
	{
		//we are about to merge - check the neighbor if we have a diamont
		if (DNeighbor==NULL) 
		{
			//no diamond
			//at the border - trivial  merge
			Merge();  
		} 
		else 
		{
			ASSERT(!DNeighbor->IsTerminal()); 
			// diamond!!!
			// check the base for good children...
			if (DNeighbor->GoodForMerge() && DNeighbor->nonterm.mergeable)  // only if base diamond neighbor is ready for merge...
			{
				DNeighbor->Merge();
				Merge();
			}
		}
	}
}

//-----------------------------------------------------------------------
//
// merges the children of the spcified triangle node and "frees" the memory
//
void TriangleTreeNode::Merge()
{
	TriangleTreeNode *LChild = this->LChild;
	TriangleTreeNode *RChild = this->nonterm.RChild;
	//keep the connectivity information prior to deleteing nodes

	//get the base neighbor of the left child and connect to parent...
	if ((term.LNeighbor = LChild->DNeighbor) != 0) {
		ASSERT(term.LNeighbor->IsTerminal()); 
		if (term.LNeighbor->term.LNeighbor == LChild)
			term.LNeighbor->term.LNeighbor = this;
		else if (term.LNeighbor->term.RNeighbor == LChild)
			term.LNeighbor->term.RNeighbor = this;
		else {
			ASSERT(term.LNeighbor->DNeighbor == LChild);
			term.LNeighbor->DNeighbor = this;
		}
	}
	
	//same for the rchild
	if ((term.RNeighbor = RChild->DNeighbor)) 
	{
		ASSERT(term.RNeighbor->IsTerminal()); 
		if (term.RNeighbor->term.LNeighbor == RChild)
			term.RNeighbor->term.LNeighbor = this;
		else if (term.RNeighbor->term.RNeighbor == RChild)
			term.RNeighbor->term.RNeighbor = this;
		else {
			ASSERT(term.RNeighbor->DNeighbor == RChild);
			term.RNeighbor->DNeighbor = this;
		}
	}

	//"delete" the nodes		
	delete(LChild);
	delete(RChild);

	//make the current node a "leaf"
	this->LChild= 0;
}

//-----------------------------------------------------------------------
//
// Split - does the work with the childnodes
//
// attaches them each to other as well as connects them with the neighbors...
//
// pretty same as in Bryan's article
//
void TriangleTreeNode::Split()
{
	ASSERT(IsTerminal()); 

	//if this is not a diamond make it so by spliting base
	if (DNeighbor && (DNeighbor->DNeighbor != this)) 
	{
		DNeighbor->Split();
	}

	//crate nodes...
	TriangleTreeNode *LChild=new TriangleTreeNode();
	TriangleTreeNode *RChild=new TriangleTreeNode();

	//attach neighbors
	LChild->DNeighbor  = term.LNeighbor;
	LChild->term.LNeighbor  = RChild;

	RChild->DNeighbor  = term.RNeighbor;
	RChild->term.RNeighbor  = LChild;

	// Left
	if (term.LNeighbor != NULL)
	{
		ASSERT(term.LNeighbor->IsTerminal()); 
		if (term.LNeighbor->DNeighbor == this) 
			term.LNeighbor->DNeighbor = LChild;
		else if (term.LNeighbor->term.LNeighbor == this) 
			term.LNeighbor->term.LNeighbor = LChild;
		else {
			ASSERT(term.LNeighbor->term.RNeighbor == this); 
			term.LNeighbor->term.RNeighbor = LChild;
		}
	}

	//  Right 
	if (term.RNeighbor != NULL)
	{
		ASSERT(term.RNeighbor->IsTerminal()); 
		if (term.RNeighbor->DNeighbor == this)
			term.RNeighbor->DNeighbor = RChild;
		else if (term.RNeighbor->term.RNeighbor == this)
			term.RNeighbor->term.RNeighbor = RChild;
		else {
			ASSERT(term.RNeighbor->term.LNeighbor == this);
			term.RNeighbor->term.LNeighbor = RChild;
		}
	}

	this->LChild = LChild;
	this->nonterm.RChild = RChild;
	nonterm.index = -1;
//	nonterm.mergeable = FALSE;  see Patch::RecurseTesslate
	// down -base - a bit diferrent scheme
	if (DNeighbor != NULL)
	{
		if (!DNeighbor->IsTerminal()) 
		{
			ASSERT(DNeighbor->DNeighbor==this);
			ASSERT(!DNeighbor->IsTerminal());
			ASSERT(DNeighbor->LChild->IsTerminal()); // terminal
			ASSERT(DNeighbor->nonterm.RChild->IsTerminal()); // terminal
			//link our children to his
			DNeighbor->LChild->term.RNeighbor = RChild;
			DNeighbor->nonterm.RChild->term.LNeighbor = LChild;
			//link his to ours...
			LChild->term.RNeighbor = DNeighbor->nonterm.RChild;
			RChild->term.LNeighbor = DNeighbor->LChild;
		}
		else
		{
			DNeighbor->Split();  // Base Neighbor (in a diamond with us) was not split yet, so do that now.
		}
	}
	else
	{
		// An edge triangle, trivial case.
		LChild->term.RNeighbor = NULL;
		nonterm.RChild->term.LNeighbor = NULL;
	}
}


//-----------------------------------------------------------------------
//
// clears all indices in the allocated nodes...
//


//***********************************************************************
//
// Patch class implementation
//
//-----------------------------------------------------------------------
//
// statics members of the Triangle node
//
int Patch::var_cnt;
//----------------------------------------------------------------------
//
// constructor
//
Patch::Patch() 
: m_nRef( 1 )
, m_bCancel( false )
, m_xoffset( 0 )
, m_yoffset( 0 )
, m_nLayer( 0 )
, ilt( -1 )
, irt( -1 )
, ilb( -1 )
, irb( -1 )
, m_pModelList( 0 )
{
	memset(&m_LBase, 0, sizeof(m_LBase));
	memset(&m_RBase, 0, sizeof(m_RBase));

	for (int i = 0; i < MAP_LAYER; i++)
	{
		m_texColor[i] = 0;
		m_pTexture[i] = 0;
	}

#ifdef DEF_LM_LETSBE_090202
	m_pTexLightMap = NULL;
#endif // DEF_LM_LETSBE_090202
}

void	Patch::Destroy()
{
	m_pPatchInfo->m_pLoading = 0;

	m_LBase.Destroy();
	m_RBase.Destroy();

	for (int i = 0; i < MAP_LAYER; i++)
	{
 		SAFE_RELEASE( m_texColor[ i ] );
 		SAFE_RELEASE( m_pTexture[ i ] );
	}

#ifdef DEF_LM_LETSBE_090202
	SAFE_RELEASE( m_pTexLightMap );
#endif // DEF_LM_LETSBE_090202

	for ( ; ; )
	{
		XBaseModel *pBaseModel = m_pModelList;
		if (pBaseModel == NULL)
			break;
		m_pModelList = m_pModelList->m_pNext;
		for ( ; ; )
		{
			XBaseModel *pOuter = pBaseModel->m_pOuter;
			if (!pOuter)
				break;
			pBaseModel->m_pOuter = pOuter->m_pOuter;
			pOuter->FreeModel();
		}
		pBaseModel->FreeModel();
	}

	XDestruct(this);
	XSystem::Release();
}

#define VERSION_4_MAXLAYER 4
#ifdef	_DEBUG
#define MAP_TRACE(x, y) map_trace(x, y, this, pMask, i, j, layer, *pAlpha, (BYTE *)MapView.m_pView);
static void map_trace(int x, int y, Patch *patch, WORD *pMask, int i, int j, int layer, int nAlpha, BYTE *color)	
{ 
	int index = pMask - Terrain::m_vAlphaMask; 
	int xindex = (index & (MAP_SIZE-1));
	int yindex = index >> MAP_LEVEL;
	int	xpos = xindex * CELL_SIZE + (i - PATCH_SIZE/2) * MAP_SIZE * CELL_SIZE; 
	int	ypos = yindex * CELL_SIZE + (j - PATCH_SIZE/2)* MAP_SIZE * CELL_SIZE; 
	if (abs(x - xpos) <= 80 && abs(y - ypos) <= 80)
	{ 
		TRACE("Pos(%d, %d) Layer(%d) a(%u) color(%u %u %u) height(%d)\n", xpos, ypos, layer, nAlpha, 
		color[0], color[1], color[2], patch->GetHeight(xindex, yindex)); 
	}
}

#else
#define MAP_TRACE(x, y)
#endif

void	Patch::LoadAsync(PatchInfo *pPatchInfo, DWORD dwPriority)
{
	m_pPatchInfo = pPatchInfo;
	if (dwPriority)
	{
		SetPriority(dwPriority);
		BgPostMessage((FUNCTION) &Patch::OnRead);
		return;
	}
	OnRead();
}

void	Patch::OnRead()
{

	LPCSTR mapName = Terrain::m_mapName.c_str();
	int i = m_pPatchInfo->m_x;
	int j = m_pPatchInfo->m_y;

	char buf[ MAX_PATH];
	char szOplPath[ MAX_PATH];
	
	wsprintf( buf, "%s_%03d_%03d.kcm", mapName, i, j);
	wsprintf( szOplPath, "%s_%03d_%03d.opl", mapName, i, j);

    // kcm 파일이 없다면 아웃
	if (!m_file.Open(buf))
	{
		if (XSystem::IsBackground())
			FgPostMessage((FUNCTION) &Patch::OnFail);
		else
			OnFail();
		return;
	}
    
#ifdef DEF_MD5_OEN_071107
    // opl 파일 체크
    std::string     oriSum;

    if (KMacro::FindFileInfo(std::string(szOplPath), oriSum))
    {
        std::string sum;

#ifdef DEF_COSMO_DEVELOPER_VER
        // 영문용은 영문용으로 테스트 한다.
        if (KGameSys::m_szLocale == "e")
        {
            sum += CMD5Checksum::GetMD5(std::string("d:/proj/bins/kalonlineeng/") + szOplPath);
        }
        else
#endif
        sum += CMD5Checksum::GetMD5(szOplPath);
        
        sum.insert(sum.begin(), MD5_RANDOM_KEY);
        
        if (sum != oriSum)
		{
#ifdef DEF_S34_DIOB_080424_ERR
			{
				MessageBox(NULL, "31", "KalOnline", MB_OK);
				KWindowCollector::GameExit();
			}
#else
			KWindowCollector::GameExit();
#endif
        }
    }
    else
	{
#ifdef DEF_S34_DIOB_080424_ERR
		{
			MessageBox(NULL, "32", "KalOnline", MB_OK);
			KWindowCollector::GameExit();
		}
#else
		KWindowCollector::GameExit();
#endif
    }
#endif

	if (m_opl.Open(szOplPath))
	{
		if (GetPriority() && !m_bCancel)
		{
			m_opl.Touch();
#ifdef	_DEBUG
//			Sleep(500);
#endif
		}
	}
	if (XSystem::IsBackground())
	{
		if (!m_bCancel) 
			m_file.Touch();
		FgPostMessage((FUNCTION) &Patch::OnLoad);
	}
	else
	{
		OnLoad();
	}
}

void	Patch::OnFail()
{
	m_file.Close();
	m_opl.Close();
	for (int i = 0; i < m_nLayer; i++)
	{
		m_vEvent[i].Clear();
	}
	if (!m_bCancel)
	{
		SetStatus(FAIL);
		m_pPatchInfo->OnLoadModel();
	}
	Release();
}

void	Patch::OnLoad()
{
	if (m_bCancel)
	{
		OnFail();
		return;
	}
	if (!Load())
	{
		OnFail();
		return;
	}
	OnEvent();
}

void	Patch::OnEvent()
{
	if (m_bCancel)
	{
		OnFail();
		return;
	}
	for (int i = 0; i < m_nLayer; i++)
	{
		if (!m_vEvent[i].Empty()) 
			return;
		if (m_pTexture[i]->GetStatus() != OK)
		{
			OnFail();
			return;
		}
	}
	LoadOpl();
}

void	Patch::RaisePriority(DWORD dwPriority)
{
	for (int i = 0; i < m_nLayer; i++)
	{
		m_pTexture[i]->CTexture::RaisePriority(dwPriority);
	}
	XPriorityMessage::RaisePriority(dwPriority);
}

BOOL	Patch::Load()
{
	char buf[ MAX_PATH];

#ifdef DEF_TEMP_MAP_OEN_080609
    XSystem::SetPath( 0, "DATA\\testMAPS\\TEX\\");
#else
	XSystem::SetPath( 0, "DATA\\MAPS\\TEX\\");
#endif
	
	int	nMaxHeight = 0;
	int	nMinHeight = 0xffff;

	// header->alpha->height->color * light->decolayer->object->color->light

    // kcm파일의 헤더 정보!!!
	struct
	{
		DWORD	checksum;
		DWORD	map_no;
		DWORD	x;
		DWORD	y;
		DWORD	reserved[4];
		DWORD version;
	} header;

	m_file.XFile::Read( &header, sizeof( header));
	m_file.m_nCRC = UpdateCRC(MAPFILE_SEED, (char *) &header + 4, sizeof(header) - 4);

#ifndef DEF_MOVE_AREA_OEN_071010
    // 임의의 지형을 패치에 인식시키기 위해
    // kcm파일헤더의 x, y 정보와 패치 정보를 비교
	if( header.version < MAPFILE_VERSION_NO(4))
		return FALSE;
	if (header.version >= MAPFILE_VERSION_NO(7))
	{
		if (header.x != m_pPatchInfo->m_x || header.y != m_pPatchInfo->m_y)
			return FALSE;
	}
#endif

	m_dwVersion = header.version;
	m_dwChecksum = header.checksum;

	BYTE vLayers[ MAP_LAYER];
	std::fill( vLayers, vLayers + MAP_LAYER, 0xFF);
	if( header.version >= MAPFILE_VERSION_NO(5))
		m_file.Read( vLayers, 8);
	else
		m_file.Read( vLayers, 4);

	// MAPFILE_VERSION 6 에 데코 팔레트 추가	
	if( header.version >= MAPFILE_VERSION_NO(6))
	{
		m_file.Read( m_decoPalette, MAX_DECO_PALETTE);
		std::sort( m_decoPalette, m_decoPalette + MAX_DECO_PALETTE);

		m_nDecoPalette = 0;
		for( int loop = 0; loop < MAX_DECO_PALETTE; ++loop)
			if( m_decoPalette[loop] != 0xFF)
				m_nDecoPalette++;

		ASSERT(m_nDecoPalette > 0);
	}
	else
	{
		for (int i = 0; i < MAX_DECO_PALETTE; i++)
			m_decoPalette[i] = i;
		m_nDecoPalette = MAX_DECO_PALETTE;
	}
	
	std::sort( vLayers, vLayers + MAP_LAYER);	

	m_nLayer = 0;
	for( int nLayerLoop = 0; nLayerLoop < MAP_LAYER; ++nLayerLoop)
		if( vLayers[ nLayerLoop] != 0xFF)
			m_nLayer++;

	ASSERT(m_nLayer > 0);
	// Read Alpha
	BYTE* pAlpha = NULL;
	if( m_nLayer > 1)
	{
		pAlpha = ( BYTE*) m_file.m_pView;
		m_file.m_nCRC = UpdateCRC(m_file.m_nCRC, m_file.m_pView, ( MAP_SIZE2 * ( m_nLayer - 1)));
		m_file.m_pView += ( MAP_SIZE2 * ( m_nLayer - 1));
	}

	m_file.Read( m_vHeight, sizeof( m_vHeight));

	WORD *pHeight = m_vHeight;
	WORD *pHeightEnd = m_vHeight + sizeof(m_vHeight)/ sizeof(WORD);
	for (; pHeight != pHeightEnd; pHeight++)
	{
		if (*pHeight > nMaxHeight)
			nMaxHeight = *pHeight;
		if (*pHeight < nMinHeight)
			nMinHeight = *pHeight;
	}

	// alpha
	int layer = 0;
	int	nAlphaBit1 = 1;

	char *pView = m_file.m_pView;
	int nAlphaBit2;
	ASSERT(m_nLayer >= 2);
	{
		WORD	*pMask = Terrain::m_vAlphaMask;
		WORD	*pMaskEnd  = &Terrain::m_vAlphaMask[MAP_SIZE2];
		for ( ; pMask != pMaskEnd; )
		{
			*pMask++ = (((1 << MAP_LAYER) - 1 - 1) << MAP_LAYER) + 1;
		}
	}

	m_file.m_nCRC = UpdateCRC(m_file.m_nCRC, m_file.m_pView,  MAP_SIZE2 * 3);
	m_file.m_pView += MAP_SIZE2 * 3;

	for (layer = 1; layer < m_nLayer; layer++)
	{

		m_file.m_pView -= MAP_SIZE2 * 3;
		nAlphaBit1 <<= 1;
		nAlphaBit2 = (((1 << MAP_LAYER) - nAlphaBit1) << MAP_LAYER) - 1;


#if !defined( TERRAIN_EDITOR)
		if (KSetting::op.m_nTerrain <= 1 && SUCCEEDED(Terrain::m_pd3dDevice->CreateTexture(
			MAP_SIZE, MAP_SIZE, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_texColor[layer], NULL))) 
#else
		if (SUCCEEDED(Terrain::m_pd3dDevice->CreateTexture(
			MAP_SIZE, MAP_SIZE, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &m_texColor[layer], NULL))) 
#endif
		{
			D3DLOCKED_RECT rect;
			m_texColor[layer]->LockRect(0, &rect, NULL, 0);
			BYTE *pBits = (BYTE *) rect.pBits;
			BYTE *pBitEnd = pBits + MAP_SIZE * 4;
			int	nPitch = rect.Pitch - MAP_SIZE * 4;
			WORD	*pMask = Terrain::m_vAlphaMask;
			WORD	* const pMaskEnd  = &Terrain::m_vAlphaMask[MAP_SIZE2];
			for ( ; pMask != pMaskEnd; )
			{
				for ( ; pBits != pBitEnd; )
				{
					//MAP_TRACE(2944, -32);
					int nAlpha = *pAlpha++;
					if (nAlpha != 0)
					{
						*pMask |= nAlphaBit1;
						if (nAlpha == 255) 
							*pMask &= nAlphaBit2;
					}
					pMask++;
					*pBits++ = *m_file.m_pView++;
					*pBits++ = *m_file.m_pView++;
					*pBits++ = *m_file.m_pView++;
					*pBits++ = nAlpha;
				}
				pBits += nPitch;
				pBitEnd = pBits + MAP_SIZE * 4;
			}
			m_texColor[layer]->UnlockRect(0);
		}
		else if (SUCCEEDED(Terrain::m_pd3dDevice->CreateTexture(MAP_SIZE, MAP_SIZE, 0, 0, 
			D3DFMT_A4R4G4B4, D3DPOOL_MANAGED, &m_texColor[layer], NULL)))
		{
			D3DLOCKED_RECT rect;
			m_texColor[layer]->LockRect(0, &rect, NULL, 0);
			BYTE *pBits = (BYTE *) rect.pBits;
			BYTE *pBitEnd = pBits + MAP_SIZE * 2;
			int	nPitch = rect.Pitch - MAP_SIZE * 2;
			WORD	*pMask = Terrain::m_vAlphaMask;
			WORD	* const pMaskEnd  = &Terrain::m_vAlphaMask[MAP_SIZE2];
			for ( ; pMask != pMaskEnd; )
			{
				for ( ; pBits != pBitEnd; )
				{
					int nAlpha = *pAlpha++;
					if (nAlpha != 0)
					{
						*pMask |= nAlphaBit1;
						if (nAlpha == 255) 
							*pMask &= nAlphaBit2;
					}
					pMask++;
					DWORD bit = (*m_file.m_pView++ & 0xf0) >> 4;
					bit |= (*m_file.m_pView++ & 0xf0);
					bit |= (*m_file.m_pView++ & 0xf0) << 4;
					bit |= (nAlpha & 0xf0) << 8;
					*(WORD *) pBits = (WORD) bit;
					pBits += 2;
				}
				pBits += nPitch;
				pBitEnd = pBits + MAP_SIZE * 2;
			}
			m_texColor[layer]->UnlockRect(0);
		}
		else 
			return false;
//		m_texColor[layer]->GenerateMipSubLevels();
		D3DXFilterTexture(m_texColor[layer], 0, 0, D3DX_FILTER_BOX); 
	}
	m_texColor[0] = m_texColor[1];
#ifdef DEF_TEMP_MAP_OEN_080609
    if (m_texColor[0])
        m_texColor[0]->AddRef();
#else
	if (m_texColor[0])
		m_texColor[0]->AddRef();
#endif

	ComputeVariance();

	for (int layer = 0; layer < m_nLayer; layer++)
	{
		LayerInfo* pLayerInfo = Terrain::GetLayerInfo( vLayers[ layer]);
		if(!pLayerInfo)
			return false;
		wsprintf( buf, "%s.dds", pLayerInfo->m_TextureName.c_str());

		CTexture *pTexture = CTexture::Load( buf, CTexture::NO_CACHE, GetPriority());
		if (pTexture == 0) 
			return false;
		if (pTexture->GetStatus() & XMessage::MASK)
		{
			m_vEvent[layer].AddEvent(this, (XCallee::FUNCTION) &Patch::OnEvent, &pTexture->m_xEventList);
		}
		m_pTexture[layer] = pTexture;
		m_fRepeat[ layer] = float( pLayerInfo->m_ScaleU);
	}
	// decolayer
	m_file.Read( m_arDecolayer, MAP_SIZE2);

	m_hoffset = 0;
	m_fScale = 1.0f;

//	m_fScale = 715.0f * 1024.0f / 65535.0f / 65535.0f;
	m_fHeightScaled = (nMaxHeight - nMinHeight) * m_fScale;
	m_fMinScaled = (nMinHeight + m_hoffset) * m_fScale;

#ifdef DEF_LM_LETSBE_090202
	SAFE_RELEASE( m_pTexLightMap );

	// 디렉토리 패스
	char szBuf[ MAX_PATH ] = { 0, };
	sprintf( szBuf, "%s%d%d%s", "DATA\\MAPS\\LightMap\\", header.x, header.y, ".dds" );

	D3DXCreateTextureFromFile( Terrain::m_pd3dDevice, szBuf, &m_pTexLightMap );
#endif // DEF_LM_LETSBE_090202

	return TRUE;
}

void	Patch::LoadOpl()
{
	// Load Model Info	
	m_pOplView = 0;
	if( m_dwVersion >= MAPFILE_VERSION_NO(5)) 
    {
		if( m_opl.m_hFile != INVALID_HANDLE_VALUE) 
        {
			struct{
				DWORD	checksum;
				DWORD	map_no;
				DWORD	x;
				DWORD	y;
				DWORD	reserved[4];
				DWORD	version;
			} OplHeader;

			m_opl.XFile::Read( &OplHeader, sizeof( OplHeader));
			
			if (m_dwVersion >= MAPFILE_VERSION_NO(7)) 
            {
				m_dwOplChecksum = OplHeader.checksum;
				m_opl.m_nCRC = UpdateCRC(MAPFILE_SEED, (char *) &OplHeader + 4, sizeof(OplHeader) - 4);
#ifndef DEF_MOVE_AREA_OEN_071010
                // 임의의 지형을 패치에 인식시키기 위해
				if (OplHeader.version == m_dwVersion 
					&& OplHeader.x == m_pPatchInfo->m_x 
					&& OplHeader.y == m_pPatchInfo->m_y)
#endif
					m_pOplView = &m_opl;
			}
			else if( OplHeader.version == 1)
				m_pOplView = &m_opl;
		}
	} 
    else 
    {
		m_pOplView = &m_file;
	}

	if( m_pOplView)
	{
		m_pOplView->Read( &m_nModelCount, sizeof( int));
	}
	else
	{
		m_nModelCount = 0;
		return;
	}
	OnLoadOpl();
}

void	Patch::OnLoadOpl()
{
	if (m_bCancel)
	{
		OnFail();
		return;
	}

	int i = m_pPatchInfo->m_x;
	int j = m_pPatchInfo->m_y;
	
	DWORD	dwEndTick = GetTickCount() + 30;
	while( --m_nModelCount >= 0)
	{
		D3DXVECTOR3 vTrans;
		D3DXQUATERNION qRot;
		int nNameLen;

		m_pOplView->Read( &nNameLen, sizeof( int));

		ASSERT( nNameLen <= XSystem::NAME_SIZE);
		char szName[XSystem::NAME_SIZE+1];
		m_pOplView->Read( szName, nNameLen);

		szName[ nNameLen] = '\0';

		m_pOplView->Read( &vTrans, sizeof( D3DXVECTOR3));
		m_pOplView->Read( &qRot, sizeof( D3DXQUATERNION));

		D3DXMATRIXA16 mScale;
		if( m_dwVersion >= 4)
		{
			D3DXVECTOR3 vScale;
			m_pOplView->Read( &vScale, sizeof( D3DXVECTOR3));
			D3DXMatrixScaling( &mScale, vScale.x, vScale.y, vScale.z);
		}
		else
		{
			float fScale;
			m_pOplView->Read( &fScale, sizeof( float));
			D3DXMatrixScaling( &mScale, fScale, fScale, fScale);
		}

		const float c_patch_scale = MAP_SIZE * CELL_SIZE;
		const float c_full_scale = MAP_SIZE * CELL_SIZE * PATCH_SIZE;

		D3DXMATRIXA16 mRot;
		D3DXMatrixRotationQuaternion( &mRot, &qRot);
		D3DXMATRIXA16 matrix;
		D3DXMatrixMultiply( &matrix, &mScale, &mRot);

		matrix._41 = vTrans.x * c_patch_scale + c_patch_scale * i - c_full_scale / 2.0f;
		matrix._42 = vTrans.z;
		matrix._43 = vTrans.y * c_patch_scale + c_patch_scale * j - c_full_scale / 2.0f;

		XBaseModel *pModel = XBaseModel::Create(szName, matrix);
		if (pModel)
		{
			pModel->m_pNext = m_pModelList;
			m_pModelList = pModel;
		}
		if (GetPriority() && (long)(GetTickCount() - dwEndTick) > 0)
		{
			FgPostMessage((FUNCTION) &Patch::OnLoadOpl);
			return;
		}
			
	}


#ifdef	_SOCKET_RELEASE
	if (m_dwVersion >= MAPFILE_VERSION_NO(7)) 
#endif
	{
		ASSERT( m_file.GetPosition() == m_file.GetLength());
		ASSERT( m_opl.GetPosition() == m_opl.GetLength());
		if (m_file.m_nCRC != m_dwChecksum || m_opl.m_nCRC != m_dwOplChecksum)
		{
			OnFail();
			return;
		}

	}
	Update();

	m_file.Close();
	m_opl.Close();

	ASSERT(!m_pPatchInfo->m_pPatch);
	m_pPatchInfo->m_pPatch = this;
	Terrain::m_vpPatch.push_back(this);
	Terrain::Link( m_pPatchInfo->m_x, m_pPatchInfo->m_y, this);
	Tesselate();
	TRACE("Patch Loaded '%s' %d %d (%d)\n", Terrain::m_mapName.c_str(), m_pPatchInfo->m_x, m_pPatchInfo->m_y, GetPriority());
	m_pPatchInfo->OnLoadModel();
	SetStatus(OK);
//	Release();
}

//-----------------------------------------------------------------------
//
// init function initializes the 
// member variables of width,height and offsets
//
BOOL Patch::Init(int x_off,int y_off, Patch *l, Patch *r, Patch *u, Patch *d)
{
	m_xoffset=x_off;
	m_yoffset=y_off;

//	m_LBase.term.RNeighbor = m_LBase.term.LNeighbor = m_RBase.term.RNeighbor = m_RBase.term.LNeighbor = NULL;
	m_LBase.LChild = m_RBase.LChild = NULL;
	
//	m_LBase.DNeighbor = &m_RBase;
//	m_RBase.DNeighbor = &m_LBase;

	//connect to neighbor patches..
	TriangleTreeNode *left, **left_ptr;
	TriangleTreeNode *right, **right_ptr;

	if (d)
	{
		left = &d->m_RBase;
		if (left->IsTerminal())
			left_ptr = &left->term.LNeighbor;
		else
		{
			left = left->LChild;
			left_ptr = &left->DNeighbor;
		}
	}
	else 
		left = 0, left_ptr = 0;
	
	if (l)
	{
		right = &l->m_RBase;
		if (right->IsTerminal())
			right_ptr = &right->term.RNeighbor;
		else
		{
			right = right->nonterm.RChild;
			right_ptr = &right->DNeighbor;
		}
	}
	else 
		right = 0, right_ptr = 0;

	m_LBase.Init(left, left_ptr, right, right_ptr, NULL, NULL);

	if (u)
	{
		left = &u->m_LBase;
		if (left->IsTerminal())
			left_ptr = &left->term.LNeighbor;
		else
		{
			left = left->LChild;
			left_ptr = &left->DNeighbor;
		}
	}
	else 
		left = 0, left_ptr = 0;
	
	if (r)
	{
		right = &r->m_LBase;
		if (right->IsTerminal())
			right_ptr = &right->term.RNeighbor;
		else
		{
			right = right->nonterm.RChild;
			right_ptr = &right->DNeighbor;
		}
	}
	else 
		right = 0, right_ptr = 0;

	m_RBase.Init(left, left_ptr, right, right_ptr, &m_LBase, &m_LBase.DNeighbor);

	memset(&m_mWorld, 0, sizeof(D3DXMATRIX));
	m_mWorld._11 = CELL_SIZE;
	m_mWorld._41 = (float)(x_off * CELL_SIZE);
	m_mWorld._22 = m_fScale;
	m_mWorld._42 = m_fScale * m_hoffset;
	m_mWorld._33 = CELL_SIZE;
	m_mWorld._43 = (float)(y_off * CELL_SIZE);
	m_mWorld._44 = 1.0f;

	memset(&m_mTextureVtx, 0, sizeof(D3DXMATRIX));
	memset(&m_mTexture, 0, sizeof(m_mTexture));
	m_mTextureVtx._11 =  1.0f / (float) (MAP_SIZE * CELL_SIZE);
	m_mTextureVtx._41 = x_off * (-1.0f / (float) MAP_SIZE);
	m_mTextureVtx._32 = -1.0f / (float) (MAP_SIZE * CELL_SIZE);
	m_mTextureVtx._42 = y_off * (1.0f / (float) MAP_SIZE) + 1.0f;
	for (int layer = 0; layer < m_nLayer; layer++)
	{
		m_mTexture[layer]._11 = m_fRepeat[layer] * m_mTextureVtx._11;
		m_mTexture[layer]._41 = m_fRepeat[layer] * m_mTextureVtx._41;
		m_mTexture[layer]._32 = m_fRepeat[layer] * m_mTextureVtx._32;
		m_mTexture[layer]._42 = m_fRepeat[layer] * m_mTextureVtx._42;

#ifdef DEF_LM_LETSBE_090202
		/*
		if ( layer == 0 )
		{
			D3DXMatrixIdentity( &m_mTexLightMap[ 32 ] );
			m_mTexLightMap[ 32 ]._11 = 64.f * m_mTextureVtx._11;
			m_mTexLightMap[ 32 ]._41 = 64.f * m_mTextureVtx._41;
			m_mTexLightMap[ 32 ]._32 = 64.f * m_mTextureVtx._32;
			m_mTexLightMap[ 32 ]._42 = 64.f * m_mTextureVtx._42;
		}
		*/
#endif // DEF_LM_LETSBE_090202
	}
	m_mTextureVtx._41 += 0.5f / MAP_SIZE;
	m_mTextureVtx._32 = m_mTextureVtx._11;
	m_mTextureVtx._42 = y_off * (-1.0f / (float) MAP_SIZE) + 0.5f / MAP_SIZE;

	//ComputeVariance();
	return true;
};

void	Patch::Update()
{
	m_fVariance = m_fScale / Terrain::m_fVariance;
}
//-----------------------------------------------------------------------
//
// ComputeVariance 
// will use a recurse function to compute and fill 
// implicit variance trees for both nodes
//
void Patch::ComputeVariance()
{
	//set the current variance tree to the left
	CVINFO lt = { 0, MAP_SIZE, GetHeight(0, MAP_SIZE), GetAlphaMask(0, MAP_SIZE) };
	CVINFO rt = { MAP_SIZE, MAP_SIZE, GetHeight(MAP_SIZE, MAP_SIZE), GetAlphaMask(MAP_SIZE, MAP_SIZE) };
	CVINFO lb = { 0, 0, GetHeight(0, 0), GetAlphaMask(0, 0) };
	CVINFO rb = { MAP_SIZE, 0, GetHeight(MAP_SIZE, 0), GetAlphaMask(MAP_SIZE, 0) };

	m_CVariance = m_LVariance-1;

	//run the left recursion
	RecurseCV(rb, lt, lb);

	//set the current variance tree to the right
	m_CVariance = m_RVariance-1;

	//run the right recursion
	RecurseCV(lt, rb, rt);
}

//-----------------------------------------------------------------------
//
// RecurseCV
// recursively computes the variance as splitting
// the nodes
//
Patch::TREE_DATA Patch::RecurseCV(const CVINFO &left, const CVINFO &right, const CVINFO &apex)
{
	//the next split point is
	CVINFO center;
	center.x = (left.x + right.x) >> 1;
	center.y = (left.y+right.y) >> 1;
	center.height = GetHeight(center.x,center.y);
	center.alpha_mask = GetAlphaMask(center.x, center.y);

	//the variance is the sum of the recurse variance and current
	TREE_DATA tinfo1, tinfo2;
	int var;
	Terrain::m_nNode <<= 1; 
	if (Terrain::m_nNode < MAP_SIZE2)
	{
		tinfo1 = RecurseCV(apex,left, center);
		Terrain::m_nNode++;
		tinfo2 = RecurseCV(right, apex, center);
		var = max(tinfo1.variance, tinfo2.variance);
		tinfo1.alpha_mask |= tinfo2.alpha_mask;
	}
	else
	{
		int alpha_mask = center.alpha_mask | apex.alpha_mask;
		int alpha_mask1 = alpha_mask | left.alpha_mask | 
			GetAlphaMask(left.x+apex.x-center.x, left.y+apex.y-center.y);
		alpha_mask1 &= Terrain::m_vMaskTable[alpha_mask1 >> MAP_LAYER];
		m_CVariance[Terrain::m_nNode].alpha_mask = alpha_mask1;
		m_CVariance[Terrain::m_nNode].variance = 0;
		Terrain::m_nNode++;
		int alpha_mask2 = alpha_mask | right.alpha_mask |
			GetAlphaMask(right.x+apex.x-center.x, right.y+apex.y-center.y);
		alpha_mask2 &= Terrain::m_vMaskTable[alpha_mask2 >> MAP_LAYER];
		m_CVariance[Terrain::m_nNode].alpha_mask = alpha_mask2;
		m_CVariance[Terrain::m_nNode].variance = 0;
		var = 0;
		tinfo1.alpha_mask = alpha_mask1 | alpha_mask2;
	}
	Terrain::m_nNode >>= 1;

	//add the current variance
	var += (abs(center.height*2-(left.height+right.height))+1) >> 1;

	if (var > 255)
		var = 255;

	//put it in the variance tree -> implicit binary tree 
	//parent set -> node >> 1
	m_CVariance[Terrain::m_nNode].variance = var;
	tinfo1.variance = var;
	m_CVariance[Terrain::m_nNode].alpha_mask = tinfo1.alpha_mask;

	return tinfo1; 
}

//-----------------------------------------------------------------------
//
// RecursTesselate 
//
// tessellates and updates our landscape
//
void Patch::RecurseTesselate(TriangleTreeNode *tri,
							 int lx,  int ly,
							 int rx, int ry,
							 int ax,  int ay,
							 int flag)
{
	//calculate the center point (eventual split one)
	int cx=(lx+rx)>>1;
	int cy=(ly+ry)>>1;

	if (flag > 0)
	{
		D3DXVECTOR3 center((float)((cx + m_xoffset) * CELL_SIZE), m_fMinScaled, 
			(float) ((cy + m_yoffset) * CELL_SIZE));
		flag = XSystem::IsCapsuleUpIn(center, m_fHeightScaled, Terrain::m_vfRadius[Terrain::m_nLevel], flag);
		if (flag < 0)
		{
			if (tri->LChild) 
				tri->MergeDown();
			return;
		}
	}

	int var = m_CVariance[Terrain::m_nNode].variance;
	if (var)
	{
 		D3DXVECTOR2 vec((cx + m_xoffset) * CELL_SIZE - XSystem::m_vCamera.x, 
 			(cy + m_yoffset) * CELL_SIZE - XSystem::m_vCamera.z);

		float lensq = D3DXVec2LengthSq(&vec);
		float r = var * m_fVariance + Terrain::m_vfRadius[Terrain::m_nLevel];

		if (r * r > lensq)
		{ 
			//split operation - for update it will just exit
			if ( !tri->LChild )
				tri->Split();

			//recurse the left and right leafs
			ASSERT(tri->LChild);	// nonterminal
			tri->nonterm.mergeable = FALSE;
			if (Terrain::m_nNode < MAP_SIZE2/2) 
			{
				Terrain::m_nNode <<= 1;
				Terrain::m_nLevel++;
				RecurseTesselate(tri->LChild,ax,ay,lx,ly,cx,cy, flag);
				Terrain::m_nNode++;
				RecurseTesselate(tri->nonterm.RChild,rx,ry,ax,ay,cx,cy, flag);
				Terrain::m_nLevel--;
				Terrain::m_nNode >>= 1;
			}
			return;
		} 
	}
	//if current variance is lower and this node is split...
	if (tri->LChild) 
		tri->MergeDown();
}


//-----------------------------------------------------------------------
//
// Tessellates/updates the patch
// runs recurse tesselate function
//
void Patch::Tesselate() 
{
	m_CVariance = m_LVariance-1;
	RecurseTesselate(&m_LBase, MAP_SIZE,0,
			0, MAP_SIZE,
			0	,0,
			0x3f);
	m_CVariance = m_RVariance-1;
	RecurseTesselate(&m_RBase, 0, MAP_SIZE,
			MAP_SIZE, 0, 
			MAP_SIZE,MAP_SIZE,
			0x3f);
}

//-----------------------------------------------------------------------
//
// adds a new vertex in the vertex array and returns the index
//
inline void Patch::PutVertex(int x, int z, short *p)
{
	Terrain::m_pVertex->position.x = (float) x;
	Terrain::m_pVertex->position.z = (float) z;
	Terrain::m_pVertex->position.y = (float) GetHeight(x, z);
//	if ((Terrain::m_pVertex->position.y = (float) GetHeight(x, z)) < WATER_POS_Y)
//		Terrain::m_bWater = TRUE;
	Terrain::m_pVertex++;
	Terrain::m_vpIndex[*p = Terrain::m_last_vcnt++] = p;
}

//-----------------------------------------------------------------------
//
// PutTriIndeces insert the tree indeces of the triangle
// into the index buffer...
//
inline void Patch::PutTriIndices(WORD i1,WORD i2,WORD i3, int alpha_mask)
{
	ASSERT(alpha_mask);
	BOOL bFlush = (Terrain::m_last_vcnt >= WJ_VERTEXBUFFERSIZE - 2);
	int shift = 1;

#ifdef DEF_LM_LETSBE_090202
	int nLayerIndex = 0;
	Terrain::Layer *pLayer = Terrain::m_layer;
	for (; alpha_mask; )
	{
		if (alpha_mask & shift)
		{
// 			*pLayer->m_pIndex++ = i1;
// 			*pLayer->m_pIndex++ = i2;
// 			*pLayer->m_pIndex++ = i3;

			if ( pLayer->m_pIndex != pLayer->m_pIndexEnd )
			{
				*pLayer->m_pIndex++ = i1;
			}

			if ( pLayer->m_pIndex != pLayer->m_pIndexEnd )
			{
				*pLayer->m_pIndex++ = i2;
			}

			if ( pLayer->m_pIndex != pLayer->m_pIndexEnd )
			{
				*pLayer->m_pIndex++ = i3;
			}

			if (pLayer->m_pIndex >= pLayer->m_pIndexEnd)
				bFlush = TRUE;

			alpha_mask ^= shift;

			nLayerIndex++;
		}

		if ( nLayerIndex == 0 )
		{
// 			*pLayer->m_pIndex++ = i1;
// 			*pLayer->m_pIndex++ = i2;
// 			*pLayer->m_pIndex++ = i3;

			if ( pLayer->m_pIndex != pLayer->m_pIndexEnd )
			{
				*pLayer->m_pIndex++ = i1;
			}

			if ( pLayer->m_pIndex != pLayer->m_pIndexEnd )
			{
				*pLayer->m_pIndex++ = i2;
			}

			if ( pLayer->m_pIndex != pLayer->m_pIndexEnd )
			{
				*pLayer->m_pIndex++ = i3;
			}
		}

		shift <<= 1;
		pLayer++;
		nLayerIndex++;
	}
#else // DEF_LM_LETSBE_090202
	Terrain::Layer *pLayer = Terrain::m_layer;
	for (; alpha_mask; )
	{
		if (alpha_mask & shift)
		{
			*pLayer->m_pIndex++ = i1;
			*pLayer->m_pIndex++ = i2;
			*pLayer->m_pIndex++ = i3;

			if (pLayer->m_pIndex >= pLayer->m_pIndexEnd)
				bFlush = TRUE;

			alpha_mask ^= shift;
		}

		shift <<= 1;
		pLayer++;
	}
#endif // DEF_LM_LETSBE_090202

	if (bFlush)
	{
		PostRender();
		PreRender();
	}
}

inline void Patch::PutTriIndicesShadow(WORD i1,WORD i2,WORD i3)
{
	Terrain::Layer *pLayer = Terrain::m_layer;
	*pLayer->m_pIndex++ = i1;
	*pLayer->m_pIndex++ = i2;
	*pLayer->m_pIndex++ = i3;
	if (Terrain::m_last_vcnt >= WJ_VERTEXBUFFERSIZE - 2 || pLayer->m_pIndex >= pLayer->m_pIndexEnd)
	{
		PostRenderShadow();
		PreRenderShadow();
	}
}
//-----------------------------------------------------------------------
//
// recurse render follows the logical structure and costructs the vertex 
// and index info... adding verteces to the vertex buffer and indices to 
// the index one...
//
void Patch::RecurseRender(TriangleTreeNode *tri,
							 int lx,  int ly,
							 int rx,  int ry,
							 int ax,  int ay,
							 short *il, short *ir, short *ia, int flag)
{
	//are there any children?
	if( !tri)
		return;

	int cx=(lx+rx);
	int cy=(ly+ry);
	if (flag > 0)
	{
		D3DXVECTOR3 vec((float) ((cx + m_xoffset * 2) * (CELL_SIZE/2)), 
			m_fMinScaled, (float) ((cy + m_yoffset * 2) * (CELL_SIZE/2)));
		flag = XSystem::IsCapsuleUpIn(vec, m_fHeightScaled, Terrain::m_vfRadius[Terrain::m_nLevel], flag);
		if (flag < 0)
		{
			return;
		}
	}
	if (tri->LChild) 
	{ 
		cx >>= 1;
		cy >>= 1;
		//traverse the children
		//share the aquired index information with our base neighbor
		short *ic;
		if (tri < tri->DNeighbor)
		{
			ASSERT (tri->DNeighbor && tri->DNeighbor->DNeighbor==tri); //if is our diamond
			ASSERT(tri->DNeighbor->LChild);
			ic = &tri->DNeighbor->nonterm.index;
		}
		else
			ic = &tri->nonterm.index;
		Terrain::m_nNode <<= 1;
		Terrain::m_nLevel++;
		RecurseRender(tri->LChild,ax,ay,lx,ly,cx,cy,ia,il,ic, flag);
		Terrain::m_nNode++;
		RecurseRender(tri->nonterm.RChild,rx,ry,ax,ay,cx,cy,ir,ia,ic, flag);
		Terrain::m_nLevel--;
		Terrain::m_nNode >>= 1;
	}
	else 
	{ 
		if (*il < 0) 
			PutVertex(lx, ly, il);
		if (*ir < 0)
			PutVertex(rx, ry, ir);
		if (*ia < 0)
			PutVertex(ax, ay, ia);
		PutTriIndices(*ia, *ir, *il, m_CVariance[Terrain::m_nNode].alpha_mask);
	}
}

void Patch::RecurseRenderShadow(TriangleTreeNode *tri,
							 int lx,  int ly,
							 int rx,  int ry,
							 int ax,  int ay,
							 short *il, short *ir, short *ia, int flag)
{
	int cx=(lx+rx);
	int cy=(ly+ry);
	if (flag > 0) {
		D3DXVECTOR3 vec((float) ((cx + m_xoffset * 2) * (CELL_SIZE/2)), 
			m_fMinScaled, (float) ((cy + m_yoffset * 2) * (CELL_SIZE/2)));
		flag = XSystem::IsCapsuleUpInShadow(vec, m_fHeightScaled, Terrain::m_vfRadius[Terrain::m_nLevel], flag);
		if (flag < 0) {
			return;
		}
	}
	//are there any children?
	if (tri->LChild) 
	{ 
		cx >>= 1;
		cy >>= 1;
		//traverse the children
		//share the aquired index information with our base neighbor
		short *ic;
		if (tri < tri->DNeighbor) {
			ASSERT (tri->DNeighbor && tri->DNeighbor->DNeighbor==tri); //if is our diamond
			ASSERT(tri->DNeighbor->LChild);
			ic = &tri->DNeighbor->nonterm.index;
		}
		else
			ic = &tri->nonterm.index;
		Terrain::m_nNode <<= 1;
		Terrain::m_nLevel++;
		RecurseRenderShadow(tri->LChild,ax,ay,lx,ly,cx,cy,ia,il,ic, flag);
		Terrain::m_nNode++;
		RecurseRenderShadow(tri->nonterm.RChild,rx,ry,ax,ay,cx,cy,ir,ia,ic, flag);
		Terrain::m_nLevel--;
		Terrain::m_nNode >>= 1;
	}
	else 
	{ 
		if (*il < 0) 
			PutVertex(lx, ly, il);
		if (*ir < 0)
			PutVertex(rx, ry, ir);
		if (*ia < 0)
			PutVertex(ax, ay, ia);
		PutTriIndicesShadow(*ia, *ir, *il);
	}
}

void Patch::ForVisibleTile( int x, int y, int size, int flag, PerTriOp* op)
{
	int child_size = size >> 1;
	int cx = x + child_size;
	int cy = y + child_size;

	D3DXVECTOR3 center(
		(float) ((cx + m_xoffset) * CELL_SIZE), 
		m_fMinScaled, 
		(float) ((cy + m_yoffset) * CELL_SIZE));

	flag = XSystem::IsCapsuleUpInShadow( center, m_fHeightScaled, Terrain::m_vfRadius[Terrain::m_nLevel], flag);
	if( flag < 0)
		return;

	if( size == 1)
	{
		center.x = float( x);
		center.z = float( y);
		center.y = GetHeight( x, y);

		op->exec( this, center);
	}
	else
	{
		++Terrain::m_nLevel;

		ForVisibleTile( x, y, child_size, flag, op);		
		ForVisibleTile( cx, y, child_size, flag, op);
		ForVisibleTile( x, cy, child_size, flag, op);
		ForVisibleTile( cx, cy, child_size, flag, op);

		--Terrain::m_nLevel;
	}
}

//-----------------------------------------------------------------------
//
// calls recursive render funcs - initial render /reset
//
void	Patch::PreRender()
{
	Terrain::m_pVB->Lock(0, 0, (void **) &Terrain::m_pVertex, D3DLOCK_DISCARD);
	Terrain::Layer *pLayer = Terrain::m_layer;

	for (int layer = m_nLayer; --layer >= 0; )
	{
		pLayer->m_pIB->Lock(0, 0, (void **)&pLayer->m_pIndex, D3DLOCK_DISCARD);
		pLayer->m_pIndexBegin = pLayer->m_pIndex;
		pLayer->m_pIndexEnd = pLayer->m_pIndex + WJ_INDEXBUFFERSIZE - 2;
		pLayer++;
	}

#ifdef	_DEBUG
	for (int layer = m_nLayer; layer < MAP_LAYER; layer++)
	{
		pLayer->m_pIndex = 0;
		pLayer++;
	}

//	Terrain::m_pIndex = 0;
#endif
}

void	Patch::PreRenderShadow()
{
	Terrain::m_pVB->Lock(0, 0, (void **) &Terrain::m_pVertex, D3DLOCK_DISCARD);
	Terrain::Layer *pLayer = Terrain::m_layer;
	pLayer->m_pIB->Lock(0, 0, (void **)&pLayer->m_pIndex, D3DLOCK_DISCARD);
	pLayer->m_pIndexBegin = pLayer->m_pIndex;
	pLayer->m_pIndexEnd = pLayer->m_pIndex + WJ_INDEXBUFFERSIZE - 2;
}

void Patch::PostRender()
{
	Terrain::m_pVB->Unlock();
	int layer;
	for (layer = 0; layer < m_nLayer; layer++)
	{
		Terrain::m_layer[layer].m_pIB->Unlock();
	}

	//set the vertex buffer - stream source
	Terrain::m_pd3dDevice->SetStreamSource(0,Terrain::m_pVB, 0, sizeof(TERRAIN_VERTEX));
	Terrain::m_vtx_cnt += Terrain::m_last_vcnt;

	// render layer 0
	D3DXMATRIXA16 mTexture;
	Terrain::Layer *pLayer = Terrain::m_layer;
	int nTriBase = (pLayer->m_pIndex - pLayer->m_pIndexBegin ) / 3;
	if ( nTriBase )
	{
		D3DXMatrixMultiply(&mTexture, &XSystem::m_mViewInv, &m_mTexture[0]);
		Terrain::m_pd3dDevice->SetTransform(D3DTS_TEXTURE1, &mTexture);
		Terrain::m_pd3dDevice->SetTexture(0, m_texColor[0]);
		Terrain::m_pd3dDevice->SetTexture(1, m_pTexture[0]->m_pTexture);
		Terrain::m_pd3dDevice->SetIndices(pLayer->m_pIB);
		Terrain::m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, Terrain::m_last_vcnt, 0, nTriBase );
		Terrain::m_tri_cnt += nTriBase;
	}

	// render layer
#ifdef _DEBUG
	int nDrawLayer = max(min(Terrain::m_nMaxDrawLayer, m_nLayer), 0);
#else
	int nDrawLayer = m_nLayer;
#endif
	for ( layer = 1; layer < nDrawLayer; layer++)
	{
		++pLayer;
		int nTriLayer = (pLayer->m_pIndex - pLayer->m_pIndexBegin)/3;
		if ( nTriLayer )
		{
			D3DXMatrixMultiply(&mTexture, &XSystem::m_mViewInv, &m_mTexture[layer]);
			Terrain::m_pd3dDevice->SetTransform(D3DTS_TEXTURE1, &mTexture);
			Terrain::m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			Terrain::m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			Terrain::m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			Terrain::m_pd3dDevice->SetTexture(0, m_texColor[layer]);
			Terrain::m_pd3dDevice->SetTexture(1, m_pTexture[layer]->m_pTexture);
			Terrain::m_pd3dDevice->SetIndices(pLayer->m_pIB);
			Terrain::m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, Terrain::m_last_vcnt, 0, nTriLayer);
			Terrain::m_tri_cnt += nTriLayer;
		}
	}

#ifdef DEF_LM_LETSBE_090202
	if ( m_pTexLightMap != NULL )
	{
  		Terrain::Layer *pLayer = Terrain::m_layer;
  		int nTriLM = ( pLayer->m_pIndex - pLayer->m_pIndexBegin ) / 3;
		
		if ( nTriLM )
		{
			/*
			// 패치 단위 렌더링이 아니라 한 Region단위 렌더링이다.
			// 이 폴리곤 구성을 뽑아내서 한장씩 매트릭스 계산해서 맵핑하느니
			// 툴에서 텍스처를 합치는걸 만들고 통으로 찍는게 시간이나 속도나 등등 훨씬 빠를거 같다. -_-;;
			*/

			/*
			// 테스트 코드
			D3DXMatrixIdentity( &m_mTexLightMap[ 32 ] );
			memset( &m_mTexLightMap[ 32 ], 0, sizeof(D3DXMATRIX) );

			m_mTexLightMap[ 32 ]._11 = 64.f * m_mTextureVtx._11;
			m_mTexLightMap[ 32 ]._41 = 64.f * m_mTextureVtx._41;
			m_mTexLightMap[ 32 ]._32 = 64.f * m_mTextureVtx._32;
			m_mTexLightMap[ 32 ]._42 = 64.f * m_mTextureVtx._42;

			D3DXMatrixMultiply( &mTexture, &XSystem::m_mViewInv, &m_mTexLightMap[ 32 ] );
			*/

// 			m_mTexture[layer]._11 = m_fRepeat[layer] * m_mTextureVtx._11;
// 			m_mTexture[layer]._41 = m_fRepeat[layer] * m_mTextureVtx._41;
// 			m_mTexture[layer]._32 = m_fRepeat[layer] * m_mTextureVtx._32;
// 			m_mTexture[layer]._42 = m_fRepeat[layer] * m_mTextureVtx._42;

		//	D3DXMatrixMultiply( &mTexture, &XSystem::m_mViewInv, &m_mTexture[0] );
			D3DXMatrixMultiply( &mTexture, &XSystem::m_mViewInv, &m_mTextureVtx );
			Terrain::m_pd3dDevice->SetTransform( D3DTS_TEXTURE1, &mTexture );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

 			Terrain::m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
 			Terrain::m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

			Terrain::m_pd3dDevice->SetTexture( 1, m_pTexLightMap );
			Terrain::m_pd3dDevice->SetIndices( pLayer->m_pIB );
			Terrain::m_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, Terrain::m_last_vcnt, 0, nTriLM );
		//	Terrain::m_tri_cnt += nTriLM;

 			Terrain::m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
 			Terrain::m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

			// 매트릭스 복구
// 			D3DXMATRIXA16 mTextureVtxInv;
// 			D3DXMatrixMultiply( &mTextureVtxInv, &XSystem::m_mViewInv, &m_mTextureVtx );
// 			Terrain::m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_mWorld );
// 			Terrain::m_pd3dDevice->SetTransform( D3DTS_TEXTURE0, &mTextureVtxInv );

			// 복구
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION );
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			Terrain::m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 | D3DTSS_TCI_CAMERASPACEPOSITION );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATE2X );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
			Terrain::m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
		}

		if ( nTriBase != nTriLM )
		{
			int a = 0;
		}
	}
#endif // DEF_LM_LETSBE_090202

	Terrain::m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

#if	defined(_DEBUG) || !defined(_SOCKET_TEST) 
	if (XSystem::m_nWireFrame == 2)
	{
		Terrain::m_pd3dDevice->SetTexture(0, 0);
		Terrain::m_pd3dDevice->SetTexture(1, 0);
		Terrain::m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		if (XSystem::m_caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS)
		{
			float fDepthBias = -0.001f;
			XSystem::m_pDevice->SetRenderState(D3DRS_DEPTHBIAS, *(DWORD *)&fDepthBias);
		}

		// 베이스 레이어
		pLayer = Terrain::m_layer;
		int nTri = (pLayer->m_pIndex - pLayer->m_pIndexBegin)/3;
 		if (nTri)
		{
			Terrain::m_pd3dDevice->SetIndices(pLayer->m_pIB);
			Terrain::m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, Terrain::m_last_vcnt, 0, nTri);

		}

		// 그 밖에 레이어
		for ( layer = 1; layer < m_nLayer; layer++)
		{
			++pLayer;
			nTri = (pLayer->m_pIndex - pLayer->m_pIndexBegin)/3;
			if (nTri)
			{
				Terrain::m_pd3dDevice->SetIndices(pLayer->m_pIB);
				Terrain::m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, Terrain::m_last_vcnt, 0, nTri);
			}
		}

		Terrain::m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		Terrain::m_pd3dDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
	}
#endif

	// clean index
	short **pIndex = Terrain::m_vpIndex;
	short **pIndexEnd = Terrain::m_vpIndex + Terrain::m_last_vcnt;
	for ( ; pIndex != pIndexEnd; ) {
		**pIndex++ = -1;
	}
	Terrain::m_last_vcnt = 0;
}

/*
void Patch::RenderMiniMap( LPRECT pDest, LPRECT pClip)
{
	typedef struct { D3DXVECTOR4 position; float u; float v;} VERTEX;

	RECT rc;
	if( IntersectRect( &rc, pDest, pClip) ) {

		float width = float( pDest->right - pDest->left);
		float height = float( pDest->bottom - pDest->top);

		XSystem::m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);

		VERTEX v[ 4] = {
			{ D3DXVECTOR4( rc.left-0.5f, rc.top-0.5f, 0.9f, 1.0f),		float(rc.left-pDest->left)/width,	1-float(rc.top-pDest->top)/height},
			{ D3DXVECTOR4( rc.right-0.5f, rc.top-0.5f, 0.9f, 1.0f),		float(rc.right-pDest->left)/width,	1-float(rc.top-pDest->top)/height},
			{ D3DXVECTOR4( rc.left-0.5f, rc.bottom-0.5f, 0.9f, 1.0f),	float(rc.left-pDest->left)/width,	1-float(rc.bottom-pDest->top)/height},
			{ D3DXVECTOR4( rc.right-0.5f, rc.bottom-0.5f, 0.9f, 1.0f),	float(rc.right-pDest->left)/width,	1-float(rc.bottom-pDest->top)/height},
		};

		// render layer 0
		D3DXMATRIXA16 mTexture;
		Terrain::Layer *pLayer = Terrain::m_layer;
		
		Terrain::m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		Terrain::m_pd3dDevice->SetTexture(0, m_texColor[0]);
		Terrain::m_pd3dDevice->SetTexture(1, m_pTexture[0]->m_pTexture);

#ifdef _USE_DRAWPRIMITIVEUP
		Terrain::m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof(VERTEX));
#else
		DrawPrimitiveUPTest( D3DPT_TRIANGLESTRIP, 2, v, sizeof(VERTEX), 4 * sizeof( VERTEX));
#endif

		// render layer
		for ( int layer = 1; layer < m_nLayer; layer++) {
			++pLayer;
			Terrain::m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			Terrain::m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			Terrain::m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			Terrain::m_pd3dDevice->SetTexture(0, m_texColor[layer]);
			Terrain::m_pd3dDevice->SetTexture(1, m_pTexture[layer]->m_pTexture);

#ifdef _USE_DRAWPRIMITIVEUP
			Terrain::m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, sizeof(VERTEX));
#else
			DrawPrimitiveUPTest( D3DPT_TRIANGLESTRIP, 2, v, sizeof(VERTEX), 4 * sizeof(VERTEX));
#endif
		}

		Terrain::m_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	}
}
*/
void Patch::PostRenderShadow()
{
	Terrain::m_pVB->Unlock();
	Terrain::m_layer[0].m_pIB->Unlock();
	//set the vertex buffer - stream source
	Terrain::m_pd3dDevice->SetStreamSource(0,Terrain::m_pVB, 0, sizeof(TERRAIN_VERTEX));

	// render layer 0
	D3DXMATRIXA16 mTexture;
	Terrain::Layer *pLayer = Terrain::m_layer;
	int nTri = (pLayer->m_pIndex - pLayer->m_pIndexBegin)/3;
	if (nTri) {
		Terrain::m_pd3dDevice->SetIndices(pLayer->m_pIB);
		Terrain::m_pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, Terrain::m_last_vcnt, 0, nTri);
	}

	// clean index
	short **pIndex = Terrain::m_vpIndex;
	short **pIndexEnd = Terrain::m_vpIndex + Terrain::m_last_vcnt;
	for ( ; pIndex != pIndexEnd; ) {
		**pIndex++ = -1;
	}
	Terrain::m_last_vcnt = 0;
}

void Patch::Render(int flag) 
{
	D3DXMATRIXA16 mTextureVtx;
	D3DXMatrixMultiply(&mTextureVtx, &XSystem::m_mViewInv, &m_mTextureVtx);
 	Terrain::m_pd3dDevice->SetTransform(D3DTS_WORLD,  &m_mWorld);
	Terrain::m_pd3dDevice->SetTransform(D3DTS_TEXTURE0, &mTextureVtx);
	if (m_fMinScaled < WATER_POS_Y)
		Terrain::m_bWater = TRUE;

	PreRender();

	//set the current variance to the left and run a left traversal...
	m_CVariance=m_LVariance-1;
	RecurseRender(&m_LBase, MAP_SIZE,0, 0,MAP_SIZE, 0,0, &irb,&ilt,&ilb, flag);
	//set the current variance to the right and run a right traversal...
	m_CVariance=m_RVariance-1;
	RecurseRender(&m_RBase, 0,MAP_SIZE, MAP_SIZE,0, MAP_SIZE,MAP_SIZE,  &ilt,&irb,&irt, flag);
	PostRender();	

}

void Patch::RenderShadow(int flag) 
{
#ifdef	SHADOW_EFFECT
	if (XSystem::m_pShadowEffect) {
		D3DXMATRIXA16 mWorldViewProj;
		D3DXMatrixMultiply(&mWorldViewProj, &m_mWorld, &XSystem::m_mViewProj);
		XSystem::m_pShadowEffect->SetMatrix("g_mWorldViewProj", &mWorldViewProj);
		D3DXMATRIXA16 mWorldToLightProj;
		D3DXMatrixMultiply(&mWorldToLightProj, &m_mWorld, &XSystem::m_mViewProjTrans);
		XSystem::m_pShadowEffect->SetMatrix("g_mToLightProj", &mWorldToLightProj);
		UINT nPass;
		XSystem::m_pShadowEffect->Begin(&nPass, 0);
	#if (D3D_SDK_VERSION > 31) // 9.0c
		XSystem::m_pShadowEffect->BeginPass(0);
	#else
		XSystem::m_pShadowEffect->Pass(0);
	#endif
	}
	else 
#endif
	{
 		Terrain::m_pd3dDevice->SetTransform(D3DTS_WORLD,  &m_mWorld);
	}

	PreRenderShadow();

	//set the current variance to the left and run a left traversal...
	m_CVariance=m_LVariance-1;
	RecurseRenderShadow(&m_LBase, MAP_SIZE,0, 0,MAP_SIZE, 0,0, &irb,&ilt,&ilb, flag);
	//set the current variance to the right and run a right traversal...
	m_CVariance=m_RVariance-1;
	RecurseRenderShadow(&m_RBase, 0,MAP_SIZE, MAP_SIZE,0, MAP_SIZE,MAP_SIZE,  &ilt,&irb,&irt, flag);
	PostRenderShadow();
#ifdef	SHADOW_EFFECT
	if (XSystem::m_pShadowEffect) {
	#if (D3D_SDK_VERSION > 31) // 9.0c
		XSystem::m_pShadowEffect->EndPass();
	#endif
		XSystem::m_pShadowEffect->End();
	}
#endif
}

void	Patch::CollidePoint(const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vVelocity)
{
#ifdef	_DEBUG
	float &T = Terrain::T;
#endif
	Terrain::T = 1.0f;
	D3DXVECTOR3 delta1(vVelocity.x * (1.0f / CELL_SIZE), vVelocity.y / m_fScale, vVelocity.z * (1.0f / CELL_SIZE));
	D3DXVECTOR3 delta2;

	D3DXVECTOR3 from(vFrom.x * (1.0f / CELL_SIZE) - m_xoffset,
		vFrom.y / m_fScale - m_hoffset, vFrom.z * (1.0f / CELL_SIZE) - m_yoffset);
	D3DXVECTOR3 to = from + delta1;
	CPoint ptFrom;
	int	nTo;

	D3DXVECTOR3 point;
	int h1, h2;

	float t1;

	delta1 = to - from;
	int	nMapping = 0;
	if (delta1.x < 0) {
		nMapping |= 1;
		from.x = MAP_SIZE - from.x;
		to.x = MAP_SIZE - to.x;
		delta1.x = -delta1.x;
	}
	if (delta1.z < 0) {
		nMapping |= 2;
		from.z = MAP_SIZE - from.z;
		to.z = MAP_SIZE - to.z;
		delta1.z = -delta1.z;
	}
	if (delta1.x < delta1.z) {
		nMapping |= 4;
		SWAP(from.x, from.z);
		SWAP(to.x, to.z);
		SWAP(delta1.x, delta1.z);
	}
		
	delta2 = delta1;
	if (delta1.x >= 0.0001f) {
		delta1.y /= delta1.x;
		delta1.z /= delta1.x;
	}
	else {
		delta1.y = delta1.z = 0;
	}
	delta1.x = 1.0f;

	if (delta2.z >= 0.00001f) {
		delta2.x /= delta2.z;
		delta2.y /= delta2.z;
	}
	else {
		delta2.x = delta2.y = 0;
	}
	delta2.z = 1.0f;

	point = from;

	if (point.x < 0) {
		if (to.x <= 0)
			return;
		point -= delta1 * point.x;
		ASSERT(point.x == 0);
	}
	else if (point.x >= MAP_SIZE)
		return;

	if (point.z < 0) {
		if (to.z <= 0)
			return;
		point -= delta2 * point.z;
		ASSERT(point.z == 0);
		if (point.x >= MAP_SIZE)
			return;
	}
	else if (point.z >= MAP_SIZE)
		return;

	nTo = (int) floorf(to.x);
	if (nTo > MAP_SIZE-1) {
		nTo = MAP_SIZE-1;
	}

	{
		// aligin at grid
		float x = floorf(point.x);
		float z = floorf(point.z);
		ptFrom.x = (int) x;
		ptFrom.y = (int) z;
		if (point.x == 0) {
			h1 = GetHeight(ptFrom.x, ptFrom.y, nMapping);
			h2 = GetHeight(ptFrom.x, ptFrom.y+1, nMapping);
			if ((point.z - z) * (h2 - h1) + h1 > point.y)
				return;
		}
		else if (point.z == 0) {
			h1 = GetHeight(ptFrom.x, ptFrom.y, nMapping);
			h2 = GetHeight(ptFrom.x+1, ptFrom.y, nMapping);
			if ((point.x - x) * (h2 - h1) + h1 > point.y)
				return;
		}
		point  -= delta1 * (point.x - x);
		t1 =  z + 1.0f - point.z;
	}
	
	for ( ; ; ) {

		float t2 = t1 - delta1.z;
		if (t2 <= 0) {
			ptFrom.y++;
			h1 = GetHeight(ptFrom.x, ptFrom.y, nMapping);
			h2 = GetHeight(ptFrom.x+1, ptFrom.y, nMapping);
			if (t1 * (delta2.x * (h2 - h1) - delta2.y) + h1 >= point.y ) {
				ptFrom.y--;
				break;
			}
			if (ptFrom.y == MAP_SIZE)
				return;
			t2 += 1.0f;
		}
		ptFrom.x++;
		point += delta1;
		h1 = GetHeight(ptFrom.x, ptFrom.y+1, nMapping);
		h2 = GetHeight(ptFrom.x, ptFrom.y, nMapping);
		if (t2 * (h2 - h1) + h1 >= point.y) {
			ptFrom.x--;
			break;
		}
		if (ptFrom.x > nTo)
			return;
		t1 = t2;
	}

	D3DXVECTOR3 v00((float) ptFrom.x, GetHeight(ptFrom.x, ptFrom.y, nMapping), (float) ptFrom.y);
	D3DXVECTOR3 v10((float) (ptFrom.x+1), GetHeight(ptFrom.x+1, ptFrom.y, nMapping), (float) ptFrom.y);
	D3DXVECTOR3 v01((float) ptFrom.x, GetHeight(ptFrom.x, ptFrom.y+1, nMapping), (float) (ptFrom.y+1));
	D3DXVECTOR3 v11((float) (ptFrom.x+1), GetHeight(ptFrom.x+1, ptFrom.y+1, nMapping), (float) (ptFrom.y+1));
	D3DXVECTOR3 v1, v2, n;
	D3DXPLANE	plane;
	if ((ptFrom.x + ptFrom.y) & 1) {
		v1 = to - from;
		v2 = v10 - v01;
		D3DXVec3Cross(&v1, &v1, &v2);
		v2 = to - v01;
		if (D3DXVec3Dot(&v1, &v2) >= 0) {
			v1 = v01 - v00;
			v2 = v10 - v00;
			D3DXVec3Cross(&n, &v1, &v2);
			v1 = to - from;
			v2 = v00 - from;
			Terrain::T = D3DXVec3Dot(&v2, &n) / D3DXVec3Dot(&v1, &n);
		}
		else {
			v1 = v10 - v11;
			v2 = v01 - v11;
			D3DXVec3Cross(&n, &v1, &v2);
			v1 = to - from;
			v2 = v11 - from;
			Terrain::T = D3DXVec3Dot(&v2, &n) / D3DXVec3Dot(&v1, &n);
		}
	}
	else {
		v1 = to - from;
		v2 = v11 - v00;
		D3DXVec3Cross(&v1, &v1, &v2);
		v2 = to - v00;
		if (D3DXVec3Dot(&v1, &v2) >= 0) {
			v1 = v11 - v01;
			v2 = v00 - v01;
			D3DXVec3Cross(&n, &v1, &v2);
			v1 = to - from;
			v2 = v01 - from;
			Terrain::T = D3DXVec3Dot(&v2, &n) / D3DXVec3Dot(&v1, &n);
		}
		else {
			v1 = v00 - v10;
			v2 = v11 - v10;
			D3DXVec3Cross(&n, &v1, &v2);
			v1 = to - from;
			v2 = v10 - from;
			Terrain::T = D3DXVec3Dot(&v2, &n) / D3DXVec3Dot(&v1, &n);
		}
	}
	if (Terrain::T < 0.0f || Terrain::T > 1.0f) {
		Terrain::T = 1.0f;
	}
}



void	PatchInfo::OnEnter()
{
	LoadAsync();
}

void	PatchInfo::OnLeave()
{
	Unload();
}

void	PatchInfo::FreeModel()
{
	XBaseModel::FreeModel();
	Unload();
}

void	PatchInfo::OnWait()
{
	ASSERT(m_pLoading);
	m_pLoading->AddRef();
	TRACE("Patch::OnWait (tick=%d x=%d y=%d priority=%d)\n", GetTickCount(), m_x, m_y, m_pLoading->GetPriority());
	m_pLoading->WaitMessage();
	TRACE("\tPatch::OnWait (tick=%d x=%d y=%d)\n", GetTickCount(), m_x, m_y);
	m_pLoading->Release();
}

void	PatchInfo::LoadAsync()
{
	if( m_pPatch) {
		BREAK();
		return;
	}

	DWORD	dwPriority = GetModelPriority();
	if (m_pLoading) {
		ASSERT(m_pLoading->m_bCancel);
		m_pLoading->m_bCancel = FALSE;
		if (!dwPriority) {
			m_pLoading->AddRef();
			m_pLoading->WaitMessage();
			m_pLoading->Release();
		}
		else {
			m_pLoading->RaisePriority(dwPriority);
		}
		return;
	}
	m_pLoading = XConstruct<Patch>();
	XSystem::AddRef();
	m_pLoading->LoadAsync(this, dwPriority);
	//TRACE("Load %p %.2f %.2f %.2f %.2f %.2f %d\n", this, m_vCenter.x, m_vCenter.y, XSystem::m_vCenter.x, XSystem::m_vCenter.z,		m_vExtent[0], dwPriority);
}

void	PatchInfo::Unload()
{
	if (m_pPatch) {
		//TRACE("Unload patch %d %d\n", m_x, m_y);
		m_pPatch->Destroy();
		PatchPtrVector::iterator it = std::find(Terrain::m_vpPatch.begin(), Terrain::m_vpPatch.end(), m_pPatch);
		ASSERT(it != Terrain::m_vpPatch.end());
		Terrain::m_vpPatch.erase(it);
		m_pPatch = 0;
	}
	if (m_pLoading) {
		m_pLoading->m_bCancel = TRUE;
	}
}

//***********************************************************************
//
// Terrain class implementation
//
//-----------------------------------------------------------------------
//
// statics members 
//
PatchInfo	Terrain::m_vPatchInfo[PATCH_SIZE2];
PatchPtrVector	Terrain::m_vpPatch;
LPWJD3DDEVICE Terrain::m_pd3dDevice;
LPWJD3DVERTEXBUFFER  Terrain::m_pVB = NULL;
TERRAIN_VERTEX	*Terrain::m_pVertex, *Terrain::m_pVertexBegin, *Terrain::m_pVertexEnd;
Terrain::Layer Terrain::m_layer[MAP_LAYER];

int	Terrain::m_vtx_cnt;
int	Terrain::m_tri_cnt;
float Terrain::m_fVariance =  0.02f;
short *Terrain::m_vpIndex[WJ_VERTEXBUFFERSIZE];
short Terrain::m_last_vcnt;
WORD Terrain::m_vAlphaMask[MAP_SIZE2];
int		Terrain::m_nLevel = 0;
int		Terrain::m_nNode = 1;
float	Terrain::m_vfRadius[MAP_LEVEL*2+1];
DWORD	Terrain::m_vMaskTable[1 << MAP_LAYER];
float	Terrain::m_fMinScaled, Terrain::m_fHeightScaled;
std::string Terrain::m_mapName;
BOOL	Terrain::m_bLoaded = FALSE;
Terrain::t_LayerInfoTable Terrain::m_LayerInfoTable;

#ifdef MAX_DECO_KIND
Terrain::t_decoItem Terrain::m_decoTable[ MAX_DECO_KIND];
#else
CStaticModel* Terrain::m_pDeco = NULL;
#endif

D3DXVECTOR3	Terrain::m_vNormal;
float	Terrain::T;
BOOL	Terrain::m_bWater;
float	Terrain::m_vExtent[3];

int		Terrain::m_nMaxDrawLayer = 8;
//-----------------------------------------------------------------------
//
// constructore (may be italian for constuctor!!??)
//
#define EXTENT0	(300.0f * METER)
static float patch_extent_table[] =
{
	800.0f * METER,
	600.0f * METER,
	350.0f * METER, 
};

static float variance_table[] =
{
	0.01f,
	0.015f,
	0.02f
};

void Terrain::InitDevice(LPWJD3DDEVICE pdev)
{
	m_vExtent[1] = patch_extent_table[KSetting::op.m_nTerrain] + MAP_SIZE * CELL_SIZE/2;
	m_vExtent[2] = m_vExtent[1] + 50.0f * METER;
	m_vExtent[0] = EXTENT0 + MAP_SIZE * CELL_SIZE/2;
	m_fVariance = variance_table[KSetting::op.m_nTerrain];

	m_vfRadius[0] = MAP_SIZE * CELL_SIZE * 0.70710678f;
	for (int i = 1; i < MAP_LEVEL*2 + 1; i++) {
		m_vfRadius[i] = m_vfRadius[i-1] * 0.70710678f;
	}

	for (int i = 0; i < (1 << MAP_LAYER)-1; i++) {
		int nTestBit = 1 << (MAP_LAYER-1);
		for ( ; ; ) {
			if ((i & nTestBit) == 0) {
				m_vMaskTable[i] = (1 << MAP_LAYER) - nTestBit;
				break;
			}
			nTestBit >>= 1;
		}
	}
	m_vMaskTable[(1 << MAP_LAYER)-1] = (1 << MAP_LAYER)-1;

	m_pd3dDevice=pdev;

#ifdef MAX_DECO_KIND
	ValidateDecoTable();
#else
	m_pDeco = new CStaticModel();
	if( m_pDeco)
	{
		m_pDeco->Load( "Data\\Objects\\Common\\grass");
		if( m_pDeco->m_pCollision)
		{
			m_pDeco->m_vCenter = 0.5f * ( m_pDeco->m_pCollision->maximum + m_pDeco->m_pCollision->minimum);
			m_pDeco->m_vExtent = m_pDeco->m_pCollision->maximum - m_pDeco->m_vCenter;
		}
	}
#endif
}

void	Terrain::DeleteDevice()
{
	UnloadMap();
}

BOOL	Terrain::RestoreDevice()
{
	ASSERT(XSystem::m_pVB);
	m_pVB = XSystem::m_pVB;

	for (int layer = 0; layer < MAP_LAYER; layer++)
	{
		if (FAILED( m_pd3dDevice->CreateIndexBuffer(WJ_INDEXBUFFERSIZE*2, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 
			D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_layer[layer].m_pIB, 0)))
			return FALSE;
	}

	return TRUE;
}

void Terrain::InvalidateDevice()
{
	//clear allocated device objects
	m_pVB = NULL;

	for (int layer = 0; layer < MAP_LAYER; layer++)
	{
		SAFE_RELEASE( m_layer[ layer].m_pIB);
	}
}

void	Terrain::Reset()
{
	BOOL loaded = IsLoaded();
	DeleteDevice();
	InitDevice(XSystem::m_pDevice);
	if (loaded)
	{
#ifdef DEF_TEMP_MAP_OEN_080609
        Terrain::LoadMap(MAPS_FOLDER);
#else
		Terrain::LoadMap( "DATA\\MAPS\\n");
#endif
		XSystem::WaitLoading();
	}
}


void Terrain::Link( int i, int j, Patch* pPatch)
{
	//init the patch
	pPatch->Init( (i - PATCH_SIZE/2)* MAP_SIZE, (j - PATCH_SIZE/2) * MAP_SIZE,  
		(i != 0) ? m_vPatchInfo[i-1+j*PATCH_SIZE].m_pPatch : 0,
		(i != PATCH_SIZE-1) ? m_vPatchInfo[i+1+j*PATCH_SIZE].m_pPatch : 0,
		(j != PATCH_SIZE-1) ? m_vPatchInfo[i+(j+1)*PATCH_SIZE].m_pPatch : 0,
		(j != 0) ? m_vPatchInfo[i+(j-1)*PATCH_SIZE].m_pPatch : 0);

	if (pPatch->m_fMinScaled < m_fMinScaled) {
		m_fMinScaled = pPatch->m_fMinScaled;
	}
	if (pPatch->m_fHeightScaled + pPatch->m_fMinScaled > m_fHeightScaled + m_fMinScaled) {
		m_fHeightScaled = pPatch->m_fHeightScaled + pPatch->m_fMinScaled - m_fMinScaled;
	}
}

/*
void Terrain::RenderMiniMap( int i, int j, LPRECT pDest, LPRECT pClip)
{
	if( i < 0 || i >= PATCH_SIZE || j < 0 || j >= PATCH_SIZE)
		return;

	PatchInfo &patch = m_vPatchInfo[i + j * PATCH_SIZE];
	if( patch.m_pPatch)
		patch.m_pPatch->RenderMiniMap( pDest, pClip);
}
*/

void Terrain::PreCache( D3DXVECTOR3 v)
{
//	WRITELOG( "PRECACHE %f %f %f ENTER\n", v.x, v.y, v.z);
	XSystem::SetCenter( v);

	int nX = int( v.x + ( MAP_SIZE * CELL_SIZE * PATCH_SIZE / 2)) / ( MAP_SIZE * CELL_SIZE);
	int nY = int( v.z + ( MAP_SIZE * CELL_SIZE * PATCH_SIZE / 2)) / ( MAP_SIZE * CELL_SIZE);

	LoadPatch( nX, nY);
//	WRITELOG( "PRECACHE LEAVE\n");
}

BOOL Terrain::LoadMap( LPCTSTR mapName)
{
	if (IsLoaded())
		UnloadMap();

//	GLog << "Load Map Name = \"" << mapName << "\"" << std::endl;
	m_mapName = "";
	m_fMinScaled = 0;
	m_fHeightScaled = 0;
	struct {
		DWORD	checksum;
		DWORD	map_no;
		DWORD	reserved[6];
		DWORD version;
	} header;

	char buf[ MAX_PATH];
	wsprintf( buf, "%s.env", mapName);

	XFileCRC MapView;
	if( !MapView.Open( buf))
		return FALSE;

	MapView.XFile::Read( &header, sizeof( header));

	MapView.m_nCRC = UpdateCRC(MAPFILE_SEED, (char *) &header + 4, sizeof(header) - 4);

//bool IsValidFileTitle( const char* title)
//{
//	return bool( strcmp( title, "INix Soft Co., Ltd.") == 0);
//}
//	if( !IsValidFileTitle( header.title)) return FALSE;
	if( header.version < MAPFILE_VERSION_NO(4))
		return FALSE;

	int ndecotable;
	MapView.Read( &ndecotable, sizeof( int));
	
#ifdef MAX_DECO_KIND
	ClearDecoTable();

	while( ndecotable--)
	{
		int index, size;
		MapView.Read( &index, sizeof( int));
		MapView.Read( &size, sizeof( int));
		m_decoTable[ index].name.resize( size);
		MapView.Read( &m_decoTable[ index].name[ 0], size);
	}

	ValidateDecoTable();
#else
	ASSERT( ndecotable == 0); 
#endif

#ifndef TERRAIN_EDITOR
	if( header.version >= 4)
	{
		ASSERT( sizeof( KSunLight::m_Table) == 24 * 5 * 4);
		MapView.Read( KSunLight::m_Table, sizeof( KSunLight::m_Table));
	}
#endif

	DWORD layerCount;
	MapView.Read( &layerCount, sizeof( DWORD));

	m_LayerInfoTable.clear();
	m_LayerInfoTable.resize( layerCount);
	for( unsigned int i = 0; i < layerCount; ++i)
	{
		if( !m_LayerInfoTable[ i].Load( MapView))
			return FALSE;
	}


	m_mapName = mapName;

	for (int j = 0; j < PATCH_SIZE; j++) {
		for (int i = 0; i < PATCH_SIZE; i++) {
			PatchInfo &patch = m_vPatchInfo[i + j * PATCH_SIZE];
			patch.m_x = i;
			patch.m_y = j;
			D3DXVECTOR2 vCenter(
				(float)(i * MAP_SIZE * CELL_SIZE + MAP_SIZE * CELL_SIZE / 2 - PATCH_SIZE * MAP_SIZE * CELL_SIZE / 2),
				(float)(j * MAP_SIZE * CELL_SIZE + MAP_SIZE * CELL_SIZE / 2 - PATCH_SIZE * MAP_SIZE * CELL_SIZE / 2));
			float fPlus = (float)(((i + j) % 3) * (10 * METER));
			patch.m_vModelExtent[0] = m_vExtent[0] + fPlus;
			patch.m_vModelExtent[1] = m_vExtent[1] + fPlus;
			patch.m_vModelExtent[2] = m_vExtent[2] + fPlus;
			patch.SetYCenter(vCenter);
			patch.m_vYExtent = D3DXVECTOR2(patch.m_vModelExtent[1], patch.m_vModelExtent[1]);
			patch.m_nState = XBaseModel::ST_UNLOAD;
			YEngine::InsertObjectA(&patch);
		}
	}
#ifdef _MAP_TEST
	LoadPatch( g_nTestMapX, g_nTestMapY);
#else

	#ifdef _SOCKET_TEST
	D3DXVECTOR3 v0, v1;
	float f0, f1;
	if (KCamera::SetCamera( v0, v1, f0, f1)) { //diob= 로딩때 들어온다.
		
		XSystem::SetCenter( v0);
		XSystem::m_dwFlags |= XSystem::RESET_RENDER | XSystem::SET_TERRAIN;
		XSystem::WaitLoading();

	}
	#else 
	LoadPatch( 31, 31);
	#endif

#endif

	D3DLIGHT9 d3dLight;
	memset(&d3dLight, 0, sizeof(d3dLight));
	d3dLight.Type = D3DLIGHT_DIRECTIONAL;
	d3dLight.Ambient = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Diffuse = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Specular = D3DXCOLOR(1, 1, 1, 1);
	d3dLight.Direction = D3DXVECTOR3(1, -1, 1);
	D3DXVec3Normalize((D3DXVECTOR3 *)&d3dLight.Direction, (D3DXVECTOR3 *) &d3dLight.Direction);
	m_pd3dDevice->SetLight(0, &d3dLight);
	m_pd3dDevice->LightEnable(0, TRUE);
	XSystem::SetLightDirection(*(D3DXVECTOR3 *) &d3dLight.Direction);

	D3DMATERIAL9 mtrl;
	mtrl.Ambient = D3DXCOLOR(0.5f, 0.5f, 0.5f, 1);
    mtrl.Diffuse = D3DXCOLOR(1, 1, 1, 1);
	mtrl.Specular = D3DXCOLOR(1, 1, 1, 1);
	mtrl.Emissive = D3DXCOLOR(0, 0, 0, 1);
	mtrl.Power = 10;
    m_pd3dDevice->SetMaterial( &mtrl );
	XSystem::SetMaterial(mtrl);

	m_bLoaded = true;

#ifdef	_SOCKET_RELEASE
	if (header.version >= MAPFILE_VERSION_NO(7)) 
#endif
	{
		if (header.checksum != MapView.m_nCRC) {
			UnloadMap();
			return FALSE;
		}
	}

	return TRUE;
}

#ifdef MAX_DECO_KIND
void Terrain::ClearDecoTable()
{
	for( int i = 0 ; i < MAX_DECO_KIND ; ++i)
	{
		m_decoTable[ i].name = "";
		SAFE_DELETE( m_decoTable[ i].pModel);
	}
}

void Terrain::ValidateDecoTable()
{
	D3DXVECTOR3& rMaxExtent = RenderDecoPerTile::s_vMaxDecoExtent;
	rMaxExtent = D3DXVECTOR3( 0, 0, 0);

	for( int i = 0 ; i < MAX_DECO_KIND ; ++i)
	{
		if( !m_decoTable[ i].name.empty())
		{
			ASSERT( m_decoTable[ i].pModel == NULL);

			m_decoTable[ i].pModel = new CStaticModel();
			m_decoTable[ i].pModel->Load( m_decoTable[ i].name.c_str());

			CStaticModel* p = m_decoTable[ i].pModel;
			if( p && p->m_pCollision)
			{
				p->m_vCenter = ( p->m_pCollision->m_pModelFile->m_vMaximum + p->m_pCollision->m_pModelFile->m_vMinimum) * 0.5f;
				D3DXVECTOR3 extent = p->m_pCollision->m_pModelFile->m_vMaximum - p->m_vCenter;
				rMaxExtent.x = max( rMaxExtent.x, extent.x);
				rMaxExtent.y = max( rMaxExtent.y, extent.y);
				rMaxExtent.z = max( rMaxExtent.z, extent.z);
			}
		}
	}
}
#endif

void	Terrain::UnloadMap()
{
	for (int i = 0; i < PATCH_SIZE; i++) {
		for (int j = 0; j < PATCH_SIZE; j++) {
			PatchInfo *pPatchInfo = &m_vPatchInfo[i + j * PATCH_SIZE];
			pPatchInfo->FreeModel();
		}
	}

#ifdef MAX_DECO_KIND
	ClearDecoTable();
#else
	SAFE_DELETE( m_pDeco);
#endif

	m_bLoaded = false;
}

void Terrain::LoadPatch(int i, int j)
{
	if( !m_vPatchInfo[i + j * PATCH_SIZE].m_pPatch)
		m_vPatchInfo[i + j * PATCH_SIZE].PatchInfo::Enter();
}

/*
void Terrain::OnMove()
{
	float fx = XSystem::m_vCenter.x / float( CELL_SIZE * MAP_SIZE);
	float fz = XSystem::m_vCenter.z / float( CELL_SIZE * MAP_SIZE);
	fx = ( fabs( floorf( fx) - fx) < fabs( ceilf( fx) - fx)) ? floorf( fx) : ceilf( fx);
	fz = ( fabs( floorf( fz) - fz) < fabs( ceilf( fz) - fz)) ? floorf( fz) : ceilf( fz);
	int x = int( fx) + PATCH_SIZE/2;
	int z = int( fz) + PATCH_SIZE/2;

	if( !m_bFirstMove && m_xcenter == x && m_zcenter == z)
		return;

	m_xcenter = x;
	m_zcenter = z;
	bool compute = false;

	int i, j;
	for( i = 0; i < PATCH_SIZE; ++i)
	{
		for( j = 0; j < PATCH_SIZE; ++j)
		{
			bool loaded = bool( 0 != m_Patches[ i + j * PATCH_SIZE]);

			if(  ( i == x || i == x-1) && ( j == z || j == z-1) )
			{
				if( !loaded)	
				{
					LoadPatch( i, j);
					compute = true;
				}
			}
			else
			{
				if( loaded)
				{
					Unload( i, j);
					compute = true;
				}
			}
		}
	}

	if( compute)
	{
		for( int loop = 0; loop < PATCH_SIZE2; ++loop)
		{
			if( m_Patches[ loop])
			{
				m_Patches[ loop]->m_LBase.Destroy();
				m_Patches[ loop]->m_RBase.Destroy();
			}
		}

		for( i = 0; i < PATCH_SIZE; ++i)
		{
			for( j = 0; j < PATCH_SIZE; ++j)
			{
				Patch* pPatch = m_Patches[ i + j * PATCH_SIZE];
				if( pPatch)
					pPatch->Init( (i - PATCH_SIZE/2)* MAP_SIZE, (j - PATCH_SIZE/2) * MAP_SIZE);
			}
		}
	}

	m_bFirstMove = false;
}
*/

//
//-----------------------------------------------------------------------
//
// renders the virtual quadratic hierarchy
//
void Terrain::RenderQuad(int cx,int cy, int half, int flag)
{

	if (flag > 0) {
		D3DXVECTOR3 vec((float) cx, m_fMinScaled, (float) cy);
		flag = XSystem::IsCapsuleUpIn(vec, m_fHeightScaled, half * SQRT_TWO, flag);
		if (flag < 0)
			return;
	}

	if (half >= MAP_SIZE * CELL_SIZE) {
		half >>= 1;
		RenderQuad(cx - half, cy - half, half, flag);
		RenderQuad(cx + half, cy - half, half, flag);
		RenderQuad(cx - half, cy + half, half, flag);
		RenderQuad(cx + half, cy + half, half, flag);
	}
	else {
		unsigned offset = (unsigned)(cx + PATCH_SIZE*MAP_SIZE*CELL_SIZE/2) / (MAP_SIZE * CELL_SIZE) + 
			((unsigned) (cy + PATCH_SIZE*MAP_SIZE*CELL_SIZE/2) / (MAP_SIZE * CELL_SIZE)) * PATCH_SIZE;
		if (m_vPatchInfo[offset].m_pPatch)
			m_vPatchInfo[offset].m_pPatch->Render(flag);
	}
}

void Terrain::RenderQuadShadow(int cx,int cy, int half, int flag)
{
	if (flag > 0) {
		D3DXVECTOR3 vec((float) cx, m_fMinScaled, (float) cy);
		flag = XSystem::IsCapsuleUpInShadow(vec, m_fHeightScaled, half * SQRT_TWO, flag);
		if (flag < 0)
			return;
	}

	if (half >= MAP_SIZE * CELL_SIZE) {
		half >>= 1;
		RenderQuadShadow(cx - half, cy - half, half, flag);
		RenderQuadShadow(cx + half, cy - half, half, flag);
		RenderQuadShadow(cx - half, cy + half, half, flag);
		RenderQuadShadow(cx + half, cy + half, half, flag);
	}
	else {
		unsigned offset = (unsigned)(cx + PATCH_SIZE*MAP_SIZE*CELL_SIZE/2) / (MAP_SIZE * CELL_SIZE) + 
			((unsigned) (cy + PATCH_SIZE*MAP_SIZE*CELL_SIZE/2) / (MAP_SIZE * CELL_SIZE)) * PATCH_SIZE;
		if (m_vPatchInfo[offset].m_pPatch)
			m_vPatchInfo[offset].m_pPatch->RenderShadow(flag);
	}

}

void Terrain::ForVisiblePatch( int cx, int cy, int half, int flag, PerPatchOp* op)
{
	if (flag > 0) {
		D3DXVECTOR3 vec((float) cx, m_fMinScaled, (float) cy);
		flag = XSystem::IsCapsuleUpInShadow(vec, m_fHeightScaled, half * SQRT_TWO, flag);
		if (flag < 0)
			return;
	}

	if (half >= MAP_SIZE * CELL_SIZE) {
		half >>= 1;
		ForVisiblePatch(cx - half, cy - half, half, flag, op);
		ForVisiblePatch(cx + half, cy - half, half, flag, op);
		ForVisiblePatch(cx - half, cy + half, half, flag, op);
		ForVisiblePatch(cx + half, cy + half, half, flag, op);
	}
	else {
		unsigned offset = (unsigned)(cx + PATCH_SIZE*MAP_SIZE*CELL_SIZE/2) / (MAP_SIZE * CELL_SIZE) + 
			((unsigned) (cy + PATCH_SIZE*MAP_SIZE*CELL_SIZE/2) / (MAP_SIZE * CELL_SIZE)) * PATCH_SIZE;
		if (m_vPatchInfo[offset].m_pPatch)
			op->exec(m_vPatchInfo[offset].m_pPatch, flag);
	}
}

//-----------------------------------------------------------------------
//
// Tesselate the landscape
// 
void Terrain::Tesselate() 
{

	for (PatchPtrVector::iterator it = m_vpPatch.begin(); it != m_vpPatch.end(); it++) {
		(*it)->Tesselate();
	}
}

//-----------------------------------------------------------------------
//
// Renders the landscape
// 

void Terrain::Render(BOOL bWater)
{
#ifdef GAME
	XSystem::SetFog();

    m_pd3dDevice->SetRenderState(D3DRS_LIGHTING,FALSE);
	m_pd3dDevice->SetFVF(D3DFVF_TERRAIN);

	DWORD dwColor = bWater ? (KSunLight::m_dwColor & 0xff0000ff)
		+ ((KSunLight::m_dwColor & 0xfefe00) >> 1) + ((KSunLight::m_dwColor & 0xfc00) >> 2) 
		: KSunLight::m_dwColor;
	Terrain::m_pd3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, dwColor);
#endif

	Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
    Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);


    Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
	Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1 | D3DTSS_TCI_CAMERASPACEPOSITION);
    
	Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_MODULATE2X);
	Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);

    Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
	Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
    Terrain::m_pd3dDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    Terrain::m_pd3dDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    Terrain::m_pd3dDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    Terrain::m_pd3dDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    Terrain::m_pd3dDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

    Terrain::m_pd3dDevice->SetTextureStageState(2, D3DTSS_COLOROP,   D3DTOP_DISABLE);
	//render all the patches that are not marked modified...
	Terrain::m_vtx_cnt = 0;
	Terrain::m_tri_cnt = 0;

	//update indices of all the modified patches
	Terrain::m_bWater = FALSE;
#if	defined(_DEBUG) || !defined(_SOCKET_TEST)
	if (XSystem::m_nWireFrame == 1)
		Terrain::m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
#endif
	RenderQuad(0, 0, PATCH_SIZE * MAP_SIZE * CELL_SIZE/2, 0x3f);
#if	defined(_DEBUG) || !defined(_SOCKET_TEST)
	if (XSystem::m_nWireFrame == 1)
		Terrain::m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
#endif

    //Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    //Terrain::m_pd3dDevice->SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	Terrain::m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	Terrain::m_pd3dDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

	m_pd3dDevice->SetRenderState( D3DRS_FOGENABLE, FALSE);
}


void Terrain::RenderDecoLayers()
{
#ifdef MAX_DECO_KIND
	if( ! m_decoTable[ 0].pModel)
		return;

	m_decoTable[ 0].pModel->PreRender();
#else
	if( !m_pDeco)
		return;

	m_pDeco->PreRender();
#endif

	PDIRECT3DDEVICE9 device = XSystem::m_pDevice;

	device->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    device->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	device->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
    device->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    device->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    device->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    device->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );

	device->SetSamplerState( 0, D3DSAMP_MIPFILTER,  D3DTEXF_POINT);
	device->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	device->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE);
//	device->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE);
//	device->SetRenderState( D3DRS_ALPHAREF, 128);

	device->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState( D3DRS_ZWRITEENABLE, FALSE);	

	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	device->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);

	RenderDecoPerTile top;
	RenderDecoPerPatch pop( &top);

	D3DXPLANE WorldFarPlane = XSystem::m_vPlaneWorld[ XSystem::P_FAR];
	D3DXPLANE DecoWorldFarPlane = XSystem::m_vPlaneView[ XSystem::P_FAR];
	DecoWorldFarPlane.d = ( XSystem::m_vFar.z - XSystem::m_vNear.z) * c_fDecoFarRate + XSystem::m_vNear.z;
	D3DXMATRIXA16 mTranspose;
	D3DXMatrixTranspose(&mTranspose, &XSystem::m_mView);
	D3DXPlaneTransform(&XSystem::m_vPlaneWorld[ XSystem::P_FAR], &DecoWorldFarPlane, &mTranspose);

	ForVisiblePatch( 0, 0, PATCH_SIZE * MAP_SIZE * CELL_SIZE / 2, 0x3f, &pop);

	XSystem::m_vPlaneWorld[ XSystem::P_FAR] = WorldFarPlane;

	top.Flush();

	device->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE);
//	device->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE);
//	device->SetRenderState( D3DRS_ALPHAREF, 0x08);
	device->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFF);
	device->SetRenderState( D3DRS_ZWRITEENABLE, TRUE);
}

void Terrain::RenderDecal( float x, float y, float size, LPDIRECT3DTEXTURE9 texture, bool add, bool pointSample)
{
	if (!texture)
		return;

	m_pd3dDevice->SetTexture(0, texture);

	m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
	m_pd3dDevice->SetFVF( D3DFVF_TERRAIN);

	m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	if (pointSample)
	{
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
	}
	else
	{
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
	}

	m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);

    m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

    m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

	m_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP,   D3DTOP_DISABLE);

	m_pd3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

	m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);

	if (add)
	{
		m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
		m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
	}
	else
	{
		m_pd3dDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
		m_pd3dDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	}
	
	D3DXMATRIXA16 mat, tt;
	float invSize = 0.5f / size;

	mat._11 = invSize;
	mat._21 = 0.f;
	mat._31 = 0.f;		
	mat._41 = -x * invSize + 0.5f;

	mat._12 = 0.f;
	mat._22 = 0.f;
	mat._32 = invSize;	
	mat._42 = -y * invSize + 0.5f;

	mat._13 = 0.f;
	mat._23 = 0.f;
	mat._33 = 0.f;		
	mat._43 = 0.f;

	mat._14 = 0.f;
	mat._24 = 0.f;
	mat._34 = 0.f;		
	mat._44 = 1.f;

	XSystem::m_pDevice->SetTransform(D3DTS_VIEW, &XSystem::m_mView);
	D3DXMatrixMultiply(&tt, &XSystem::m_mViewInv, &mat);
	XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE0, &tt);

	XSystem::m_vPlaneWorld[ XSystem::P_SHADOW_T].d = y + size;
	XSystem::m_vPlaneWorld[ XSystem::P_SHADOW_B].d = size - y; 
	XSystem::m_vPlaneWorld[ XSystem::P_SHADOW_R].d = x + size; 
	XSystem::m_vPlaneWorld[ XSystem::P_SHADOW_L].d = size - x;

#ifdef	SHADOW_EFFECT
	ID3DXEffect *pEffect = XSystem::m_pShadowEffect;
	XSystem::m_pShadowEffect = 0;
#endif
	RenderQuadShadow(0, 0, PATCH_SIZE * MAP_SIZE * CELL_SIZE/2, 0x3ff);
#ifdef	SHADOW_EFFECT
	XSystem::m_pShadowEffect = pEffect;
#endif

	m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE);
	m_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
	
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);

	Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    Terrain::m_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	XSystem::SetShadowDistance(XSystem::m_fShadowDistance);	
}


void Terrain::Update()
{
	for (PatchPtrVector::iterator it = m_vpPatch.begin(); it != m_vpPatch.end(); it++) {
		(*it)->Update();
	}
}

float	Terrain::GetHeight(float x, float z)
{
	x /= CELL_SIZE;
	z /= CELL_SIZE;
	unsigned ix  = (unsigned) floorf(x + PATCH_SIZE * MAP_SIZE / 2);
	unsigned iz  = (unsigned) floorf(z + PATCH_SIZE * MAP_SIZE / 2);
	if (ix >= PATCH_SIZE * MAP_SIZE || iz >= PATCH_SIZE * MAP_SIZE) {
		Terrain::m_vNormal.x = 0;
		Terrain::m_vNormal.y = 1;
		Terrain::m_vNormal.z = 0;
		return 0.0f;
	}
	Patch *patch = m_vPatchInfo[ ix / MAP_SIZE + iz / MAP_SIZE * PATCH_SIZE].m_pPatch;
	if (patch) {
		ix &= MAP_SIZE - 1;
		iz &= MAP_SIZE - 1;	
		x -= floorf(x);
		z -= floorf(z);
		if ((ix + iz) & 1) {
			if (x + z <= 1.0f) {
				float h00 = patch->GetHeight(ix, iz);
				float h10 = patch->GetHeight(ix+1, iz);
				float h01 = patch->GetHeight(ix, iz+1);
				Terrain::m_vNormal.x = (h00 - h10) * patch->m_fScale;
				Terrain::m_vNormal.y = CELL_SIZE;
				Terrain::m_vNormal.z = (h00 - h01) * patch->m_fScale;
				return (h00 + (h10 - h00) * x + (h01 - h00) * z + patch->m_hoffset) * patch->m_fScale;
			}
			else {
				x = 1.0f - x;
				z = 1.0f - z;
				float h11 = patch->GetHeight(ix+1, iz+1);
				float h10 = patch->GetHeight(ix+1, iz);
				float h01 = patch->GetHeight(ix, iz+1);
				Terrain::m_vNormal.x = (h01 - h11) * patch->m_fScale;
				Terrain::m_vNormal.y = CELL_SIZE;
				Terrain::m_vNormal.z = (h10 - h11) * patch->m_fScale;
				return (h11 + (h01 - h11) * x + (h10 - h11) * z + patch->m_hoffset) * patch->m_fScale;
			}
		}
		else {
			if (z < x) {
				x = 1.0f - x;
				float h10 = patch->GetHeight(ix+1, iz);
				float h00 = patch->GetHeight(ix, iz);
				float h11 = patch->GetHeight(ix+1, iz+1);
				Terrain::m_vNormal.x = (h00 - h10) * patch->m_fScale;
				Terrain::m_vNormal.y = CELL_SIZE;
				Terrain::m_vNormal.z = (h10 - h11) * patch->m_fScale;
				return (h10 + (h00 - h10) * x + (h11 - h10) * z + patch->m_hoffset) * patch->m_fScale;
			}
			else {
				z = 1.0f - z;
				float h01 = patch->GetHeight(ix, iz+1);
				float h00 = patch->GetHeight(ix, iz);
				float h11 = patch->GetHeight(ix+1, iz+1);
				Terrain::m_vNormal.x = (h01 - h11) * patch->m_fScale;
				Terrain::m_vNormal.y = CELL_SIZE;
				Terrain::m_vNormal.z = (h00 - h01) * patch->m_fScale;
				return (h01 + (h11 - h01) * x + (h00 - h01) * z + patch->m_hoffset) * patch->m_fScale;
			}
		}
	}
	Terrain::m_vNormal.x = 0;
	Terrain::m_vNormal.y = 1;
	Terrain::m_vNormal.z = 0;
	return 0.0f;
}

BOOL	Terrain::CollidePoint(const D3DXVECTOR3 &vFrom, const D3DXVECTOR3 &vVelocity)
{
	Terrain::T = 1.0f;
	D3DXVECTOR2	delta1(vVelocity.x * (1.0f / (MAP_SIZE * CELL_SIZE)), vVelocity.z * (1.0f / (MAP_SIZE * CELL_SIZE)));
	D3DXVECTOR2 from(vFrom.x * (1.0f / (MAP_SIZE * CELL_SIZE)) + PATCH_SIZE/2,
		vFrom.z * (1.0f / (MAP_SIZE * CELL_SIZE)) + PATCH_SIZE/2);
	D3DXVECTOR2 to = from + delta1;
	CPoint ptFrom;
	int	nTo;

	D3DXVECTOR2 point;
	D3DXVECTOR2 delta2;

	float t1;

	delta1 = to - from;
	int	nMapping = 0;
	if (delta1.x < 0) {
		nMapping |= 1;
		from.x = PATCH_SIZE - from.x;
		to.x = PATCH_SIZE - to.x;
		delta1.x = -delta1.x;
	}
	if (delta1.y < 0) {
		nMapping |= 2;
		from.y = PATCH_SIZE - from.y;
		to.y = PATCH_SIZE - to.y;
		delta1.y = -delta1.y;
	}
	if (delta1.x < delta1.y) {
		nMapping |= 4;
		SWAP(from.x, from.y);
		SWAP(to.x, to.y);
		SWAP(delta1.x, delta1.y);
	}
		
	delta2 = delta1;
	if (delta1.x >= 0.0001f) {
		delta1.y /= delta1.x;
	}
	else {
		delta1.y = 0;
	}
	delta1.x = 1.0f;

	if (delta2.y >= 0.00001f) {
		delta2.x /= delta2.y;
	}
	else {
		delta2.x = 0;
	}
	delta2.y = 1.0f;

	point = from;

	if (point.x < 0) {
		if (to.x <= 0)
			return FALSE;
		point -= delta1 * point.x;
		ASSERT(point.x == 0);
	}
	else if (point.x >= PATCH_SIZE)
		return FALSE;

	if (point.y < 0) {
		if (to.y <= 0)
			return FALSE;
		point -= delta2 * point.y;
		ASSERT(point.y == 0);
		if (point.x >= PATCH_SIZE)
			return FALSE;
	}
	else if (point.y >= PATCH_SIZE)
		return FALSE;

	nTo = (int) floorf(to.x);
	if (nTo > PATCH_SIZE-1) {
		nTo = PATCH_SIZE-1;
	}

	{
		// aligin at grid
		float x = floorf(point.x);
		float y = floorf(point.y);
		ptFrom.x = (int) x;
		ptFrom.y = (int) y;
		point  -= delta1 * (point.x - x);
		t1 =  y + 1.0f - point.y;
	}
	
	for ( ; ; ) {

		CollidePoint(ptFrom, nMapping, vFrom, vVelocity);
		if (Terrain::T < 1.0f) 
			return TRUE;
		float t2 = t1 - delta1.y;
		if (t2 <= 0) {
			ptFrom.y++;
			if (ptFrom.y == PATCH_SIZE)
				return FALSE;
			CollidePoint(ptFrom, nMapping, vFrom, vVelocity);
			if (Terrain::T < 1.0f)
                return TRUE;
			t2 += 1.0f;
		}
		if (ptFrom.x == nTo)
			return FALSE;
		ptFrom.x++;
		t1 = t2;
	}

	return FALSE;
}
