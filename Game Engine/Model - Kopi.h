#pragma once

#include "XSystem.h"
#include "ModelHeader.h"
#include "XFile.h"
#include "lisp.h"
#include "XEngine.h"

#if	!defined(MODELVIEW) && !defined(TERRAIN_EDITOR)
#include "Effect/Effect_Sprite/Effect_Sprite.h"
#include "Effect/Effect_Fx/Effect_Fx.h"
#endif

class CBone;
class CMesh;
class CAnim;
//class CBbox;
class CKeyFrame;
class CCollision;
class CModelFile;
class CMaterialData;
class CModel;

enum {
	MATERIALEX_TEXTURE = 0x10000,
	MATERIALEX_VOLUME = 0x20000,
	MATERIALEX_BILLBOARD = 0x40000,
	MATERIALEX_BILLBOARD_Y = 0x80000,
	MATERIALEX_SKIN = 0x100000,
};

class CMaterial
{
public:
	CModelFile *m_pModelFile;
	WORD		m_nMatrialCount;
	WORD		m_nMatrialFrameCount;
	CMaterialData * m_pMaterialData;
	CKeyFrame	*m_pKeyFrame;
	BOOL	Load(XFileCRC *pFile);
};

class CModelFile : public CResource
{
public:
	enum {
		SHADER = 1,
		SCRIPT = 2,
		NO_CACHE = 0x10000, // CResource::NO_CACHE
		CRC = 0x20000,
	};
	CBone	*m_pBone;
	CMaterial m_material;
	CMesh	*m_pMesh;	
	int		m_nMesh;
	CAnim	*m_pAnim;
	int		m_nAnim;
	LPCSTR	m_szOption;	
	CCollision	*m_pCollision;
	XEventList	m_xEventList;
	XFileCRC	m_file;
	int		m_nEvent;
	D3DXVECTOR3	m_vMinimum;
	D3DXVECTOR3	m_vMaximum;
#ifdef	_DEBUG
	int		m_nBoneCount;
#endif
	lisp::var	m_varScript;
	char *	m_pString;

	CModelFile();
	static CModelFile * Load(LPCSTR szName, DWORD dwFlags, DWORD dwPriority, lisp::var varScript);
	void	AddRefAll();
	void	ReleaseAll();
	virtual void Destroy();

	void	Clear();
	void	OnRead();
	void	OnLoad();
	void	OnFail();
	void	Load();
	void	OnEvent();
	virtual void	RaisePriority(DWORD dwPriority);
};

class CBoneData 
{
public:
	D3DXMATRIX	m_mInverse;
	D3DXMATRIX	*m_pmParent;
};

class CBone 
{
public:
	CModelFile *m_pModelFile;
	int	m_nBoneData;
	CBoneData *m_pBoneData;

	void	AddRef() { m_pModelFile->AddRef(); }
	void	Release() { m_pModelFile->Release(); }
	BOOL	Load(XFileCRC *pFile);
};

struct CFloatAnim
{
	DWORD	dwTime;
	float	fValue;
	int operator - (DWORD time) const { return this->dwTime - time; }
};

class CMaterialData 
{
public:
	LPCSTR		m_szTexture;
	float		m_fPower;
	DWORD		m_mapoption;
	LPCSTR		m_szOption;

	D3DCOLOR	m_ambient;
	D3DCOLOR	m_diffuse;
	D3DCOLOR	m_specular;
	float		m_opacity;
	D3DXVECTOR2 m_offset;
	D3DXVECTOR3 m_angle;

	GB_MATERIAL_FRAME * m_pframe;
	DWORD		m_nWCoord;
	CFloatAnim	*m_pWCoord;

	void	Parse();
	static DWORD ParseOption(DWORD dwParam, lisp::var var);
};


class CMesh 
{
public:
	enum {
		MAX_TEXTURE = 2,
	};
	enum { 
		F_VSBLEND = 1,
		F_FIXEDBLEND = 2,
		F_SOFTWARE = 4,
		F_EFFECT = 8,
		F_BLEND_OPACITY = 0x10,
		F_LIGHTMAP = 0x20,
	};

	static DWORD s_dwOptions;
	static float s_fBlendOpacity;

	CModelFile *m_pModelFile;
	LPCSTR	m_szName;
	CMaterial		*m_pMaterial;
	CMaterialData	*m_pMatData;
	DWORD		m_dwFlags;
	LPCSTR	m_szTexture[2];

	VERTEX_TYPE		m_nVertexType;
	D3DPRIMITIVETYPE m_nFaceType;
	int		m_nVertex;
	int		m_nFace;
	IDirect3DVertexBuffer9	*m_pVertexBuffer;
	IDirect3DIndexBuffer9	*m_pIndexBuffer;
	CTexture	*m_pTexture[2];
	XEvent			m_evTexture[2];
	CEffectFile	*m_pFX;
	XEvent			m_evFX;
	int		m_nBoneIndex;
	BYTE	*m_pBoneIndex;

	D3DCOLOR m_skinColor;
	CTexture *m_pSkinTexture;

	lisp::var	m_varScript;
	char	*m_pCommand;
	int		m_nCommand;

	void	AddRef() { m_pModelFile->AddRef(); }
	void	Release() { m_pModelFile->Release(); }
	BOOL	Load(XFileCRC *pFile, DWORD dwFlags);
	void	Free();
	void	RenderStatic();
	void	RenderStaticDecal();
	void	RenderStaticShadow();
	void	Render();
	void	RenderShadow();
	void	MapOptionPre();
	void	MapOptionPost();
	__forceinline void	Draw();
	LPCSTR	GetTextureName(int nIndex);
	void	LoadTexture(int nIndex, LPCSTR szName, DWORD dwFlags);
	void	LoadEffect(LPCSTR szName);

	void DrawPrimitiveOnly();
	void	PrepareCode(lisp::var varCode);
	void	GenerateCode(lisp::var varCode);
	void	PrepareScript(lisp::var varMesh);
	void	GenerateScript();
	void	ExecuteScript();
};

class CKeyFrame
{
public:
	DWORD	m_nTime;
	char	*m_szOption;
	char	*m_strEffectModel;
	char	*m_strSoundNo;
	float	m_fMotionNo;
};

class	CAnim 
{
public:
	CModelFile	*m_pModelFile;
	CKeyFrame	*m_pKeyFrame;
	GB_ANIM	**m_ppAnimData;
	int		m_nKeyFrame;
	char	*m_szAniOption;
	int		m_iAniLoopNo;

#ifdef DEF_LOOP_OPT_OEN_080829
    // 애니매이션 루프시 끝 프레임
    int		m_iAniLoopEndNo;
#endif

	void	AddRef() { m_pModelFile->AddRef(); }
	void	Release() { m_pModelFile->Release(); }
	BOOL	Load(XFileCRC *pFile, int bone_count, GB_ANIM *pAnimData);
};

#ifdef GAME_XENGINE
class	CClsNode : public GB_CLS_NODE
{
public:
	void	GetVertex(D3DXVECTOR3*v, int index);
	void	CollidePoint(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax);
	void	CollidePoint(int index);
	void	CollideRect(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax);
	void	CollideRect(int index);
	void	DrawDecal(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax, int nFlags);
	void	DrawDecal(int index, int nFlags);
	void	IntersectBox(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax);
	void	IntersectBox(int index);
	void	CollideBox(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax);
	void	CollideBox(int index);
};

class CCollision 
{
public:
	CModelFile	*m_pModelFile;
	GB_CLS_HEADER	m_header;

	void	AddRef() { m_pModelFile->AddRef(); }
	void	Release() { m_pModelFile->Release(); }
};
#endif //GAME_XENGINE

class CBaseModel : public XObject
{
public:
	D3DXMATRIX	m_matrix;
	D3DXMATRIX	m_inverse;
	CLink	m_linkSort;
	FLOAT	m_fSort;
	
	virtual ~CBaseModel() {}

	static void	PreRender();
	static void PreRenderShadow();
	static void PreRenderLM();
};

typedef struct particleInfo_s {
	std::string	name;
	int			effectbone;
	float		offset[3];
} particleInfo_t;

class CStaticModel : public CBaseModel
{
public:
	enum { 
		F_FORCE_AMBIENT = 1,
		F_BLEND_OPACITY = 2,
		F_REFLECT_TRANSFORM = 4,
		F_WATER = 8,
		F_CONTAIN_TRANSPARENT = 0x10,
		F_BLENDABLE = 0x20,
	};
	DWORD			m_dwFlags;
	CMesh			*m_vpMesh[MODEL_MAX_MESH];
	int				m_nMesh;
	CCollision		*m_pCollision;
	D3DXCOLOR		m_ambientColor;

#ifdef GAME_XENGINE
	WORD		*m_pClsVertex;
	WORD		*m_pClsIndex;
	CClsNode	*m_pClsNode;
#endif //GAME_XENGINE
	D3DXVECTOR3		m_vEdge;                        //@ 최대 길이에서 중앙 까지의 크기 벡터
	LPCSTR			m_szOption;
	CModelFile		*m_pModelFile;
	XEvent			m_event;
	XCaller			m_caller;

	float	m_fBlendOpacity;

	lisp::var		m_varScript;
#ifndef TERRAIN_EDITOR
	std::vector<particleInfo_t> m_prtInfoVector;
	CParticleBundle	m_prtBundle;
	CFxBundle		m_fxBundle;
#endif

	CStaticModel();
	~CStaticModel() { Destroy(); }
	BOOL	Load(LPCSTR szName, DWORD dwFlags, XCallee *pCallee, XCallee::FUNCTION pFunction, DWORD dwPriority=0);
	BOOL	Load(LPCSTR szName, DWORD dwFlags = 0) 	{ return Load(szName, dwFlags, this, (XCallee::FUNCTION) OnLoad);}

	void	Destroy();
	void	PreProcessOption();
	void	PostProcessOption();
	virtual void	Render();
	virtual void	RenderDecal();
	virtual void	RenderShadow();
	void	DrawPrimitiveOnly();

	void	SetSkin(int index,D3DCOLOR color,CTexture *texture = NULL);

	void	OnEvent();
	void	OnLoad();
#ifdef GAME_XENGINE
	BOOL	LoadModel(LPCSTR szName) { return Load(szName, 0, this, (XCallee::FUNCTION) OnLoadModel); }
	void	OnLoadModel();
	virtual void		RenderFlag(int flag);
#if	!defined(TERRAIN_EDITOR)
	virtual void	RenderFlagShadow(int flag);
#endif	
	static CStaticModel *CreateModel(LPCSTR szName, const D3DXMATRIX &matrix);
	virtual void	CollidePoint();
	virtual void	CollideRect();
#if	!defined(TERRAIN_EDITOR)
	virtual void	DrawDecal(int nFlags);
	virtual void	RenderFlagDecal(int flag);
#endif
#if 0
	virtual void	IntersectBox();
#endif
	virtual void	CollideBox();
#endif
	void	FreeMesh(int nMesh);

    void    FreeModel();

};

class CSkin {
public:	
	D3DXCOLOR	m_color;
	CTexture	*m_pTexture;
};

class CModel : public CStaticModel
{
public:
	static	CModel	*s_pModel;

	CBone	*m_pBone;
	CAnim	*m_vpAnim[MODEL_MAX_ANIM];
	int		m_nAnim;
	float	m_fRate;
	int		m_nBoneData;
	int		m_nKeyFrame;
	DWORD	m_nBlend;
	GB_ANIM	m_vAnimData[2*MODEL_MAX_BONE];
	D3DXVECTOR3	m_vCenterOffset;                //@ 모델 중앙까지의 거리
	BYTE	m_bDirection[MODEL_MAX_BONE];

#if	!defined(MODELVIEW) && !defined(TERRAIN_EDITOR)
	CSkin				m_Skin;
#endif

	DWORD	m_dwTime;

    int     m_defaultAnimationNumber;

protected:
	GB_ANIM	**m_ppAnimData;
	CKeyFrame	*m_pKeyFrame;	
	CBoneData	*m_pBoneData;


public:
	CModel();
	~CModel() { Reset(); }
	void	Destroy() { Reset(); CStaticModel::Destroy(); }
	BOOL	Load(LPCSTR szName, DWORD dwFlags = 0) { 	
		return CStaticModel::Load(szName, dwFlags | CModelFile::SHADER, this, (XCallee::FUNCTION) OnLoad); }
	void	OnLoad();

	void	FreeAnim(int nAnim);
	virtual void	Render();
	virtual void	RenderShadow();
#ifdef DEF_SCENARIO_3_3_OEN_080114
    /**
    *  시작 키 프레임을 설정하여 애니매이션을 플레이
    * \param nAnim 실행할 애니매이션 넘버
    * \param nBlend 블렌딩 시간?
    * \param key_ 실행할 키 넘버
    */
    void	PlayAnimation(int nAnim, int nBlend = 200, int key_ = 0);	
#else
    void	PlayAnimation(int nAnim, int nBlend = 200);
#endif

	/**
	 *  애니매이션을 처음 부터가 아니라 time만큼 후에 부터 재생한다.
	 * \param nAnim 실행할 애니매이션 넘버
	 * \param dwSkip ?
	 * \param dwTime ?
	 */
	void	PlayAnimationEx(int nAnim, DWORD dwSkip, DWORD dwTime);
	void	FrameMove(DWORD dwTime);
	void	MaterialMove(CMesh *pMesh);
	
	virtual void OnAnimationComplete();
	virtual void OnAnimationOption(CKeyFrame *pKeyFrame);
	virtual void ProcessDirection(BYTE iDataNo);
	virtual void ProcessTransform();
	void	SetTransform();
#ifdef GAME_XENGINE
	virtual void	RenderFlag(int flag);	
#if	!defined(TERRAIN_EDITOR)
	virtual void	RenderFlagDecal(int flag) {}
	virtual void	RenderFlagShadow(int flag);
	virtual void	DrawDecal(int nFlags) {}
#endif
	BOOL	InitModel(const D3DXVECTOR3& vCenter, float fScale);
	BOOL	LoadModel(LPCSTR szName) {
		return CStaticModel::Load(szName, CModelFile::SHADER, this, (XCallee::FUNCTION) OnLoadModel); }
	void	RemoveModel();
	void	OnLoadModel();
#endif
	static D3DXMATRIXA16	s_vMatrix[MODEL_MAX_BONE];          //! 본 매트릭스
	static D3DXMATRIXA16	s_vMatrixWorld[MODEL_MAX_BONE];
	static D3DXMATRIXA16	s_vMatrixTrans[MODEL_MAX_BONE];

protected:
	void	Reset();
};

namespace CToken
{
	int		tokenint(const char* p,int offset);
	float	tokenfloat(const char* p,int offset);
	int		tokencount(const char* p);
	const char * tokenchar(const char* p,int offset);
}