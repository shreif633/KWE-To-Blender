#pragma once

#include "Model.h"
#include "XEngine.h"
#include "lisp.h"

#ifdef DEF_XMODEL_BONE_EFFECT_OEN_081125
#   include "GameEffectBoneModel.h"
#endif

void	FreeModelTable();

struct XModelTable
{
	std::string m_strName;
	float	m_vExtent[3];
	char m_localizationString[8];
	XModelTable *m_pOutside;
	lisp::var m_varScript;
	std::vector<particleInfo_t> m_prtInfoVector;
	
	int operator - (const char *str) const { return stricmp(m_strName.c_str(), str); }
	BOOL	Init(lisp::var param);
	
	typedef	std::vector<XModelTable> TableVector;
	static TableVector	Table;
	static XModelTable *Find(LPCSTR szName);
};

class XBaseModel : public YObject
{
public:
	enum { 
		ST_UNLOAD = 1, // initial state
		ST_LOADING = 2, // async load
		ST_LOADED  = 4, // load complete
		ST_REPLACED = 8, // replaced by inside object
		ST_WAIT_LOADING = 0x10, 
		ST_POST_LOADED = 0x20,
		ST_POST_UNLOAD = 0x40,
	};
	XBaseModel	*m_pNext;
	XBaseModel	*m_pOuter;
	CLink	m_linkPost;
	float	m_vModelExtent[3];
	int		m_nState;
		// 0 :  initial state
		// 1 :  Async Load
		// 2 :  Load Complete
		// 3 :  Waiting object
	char	m_szName[XSystem::NAME_SIZE+1];

#ifdef _TOOL
    bool m_bHide;
#endif

#ifdef DEF_XMODEL_BONE_EFFECT_OEN_081125
    // X모델을 따로 관리 한다.
    static std::map<DWORD, XBaseModel*> m_mapXModel;

    // 모델을 찾는다.
    static XBaseModel* FindXModel(DWORD nXModelID);
    // 모델 삽입
    static void InsertXModel(DWORD nXModelID, XBaseModel* pXModel);
    // 모델을 삭제
    static void DeleteXModel(DWORD nXModelID);
#endif

    XBaseModel() 
    { 
        m_pNext = 0; m_pOuter = 0; m_nState = ST_UNLOAD; 
#ifdef _TOOL
        m_bHide = false;
#endif
    }

	void	InitXModel(XModelTable *pTable, const D3DXMATRIX &matrix, float ratio);
	void	InitXModel(const D3DXVECTOR3 &vCenter, const D3DXVECTOR3 &vExtent);
	void	OnLoadModel();
	DWORD	GetModelPriority();

	virtual void	FreeModel();

	/**
	 *	모델의 엔터(들어감)을 처리한다.
     *  모델의 반경안으로 들어갔는지 여부에 따라 OnEnter을 호출한다.
	 */
	virtual void	Enter();
	virtual void	Leave();
	virtual void	OnEnter() = NULL;
	virtual void	OnLeave() = NULL;
	virtual void	OnWait() {}
	
	void	OnPostProcess();

	static XBaseModel *Create(LPCSTR szName, const D3DXMATRIX &matrix);
};

class XStaticModel : public CStaticModel, public XBaseModel
{
public:
	D3DXVECTOR3			m_vLightDirection;
	D3DXVECTOR4			m_vLightColor;

	~XStaticModel();
	virtual void FreeModel();
//	virtual void	InitXModel(const D3DXMATRIX &matrix, float ratio);
	virtual void	OnEnter();
	virtual void	OnLeave();
	virtual void	OnWait();
	virtual void	RenderFlagDecal() {}
	virtual void	RenderFlagShadow(int flag) {}
#ifndef TERRAIN_EDITOR
	virtual void	OnCollectLight();
	virtual void	OnPreRender();
#endif

#ifdef _TOOL
    virtual void    RenderFlag(int flag) { if (!m_bHide) CStaticModel::RenderFlag(flag); }
    virtual void	CollidePoint()       { if (!m_bHide) CStaticModel::CollidePoint(); }
#endif

	void	OnLoadModel();
};

#ifndef DEF_XMODEL_BONE_EFFECT_OEN_081125
class XModel : public CModel, public XBaseModel
#else
class XModel : public CGameEffectBoneModel, public XBaseModel
#endif
{
public:
	D3DXVECTOR3			m_vLightDirection;
	D3DXVECTOR4			m_vLightColor;

#ifdef DEF_XMODEL_BONE_EFFECT_OEN_081125
    /// XModel 은 동적인 이펙트 본 행렬을 가질 수 있다.
    std::map< int, D3DXMATRIX > m_mapEffectBoneMatrix;
    std::map< int, D3DXMATRIX >::iterator m_itEffectBoneMatrix;

    /// 렌더링을 삭속 받아 본에 이펙터를 붙인다.
    virtual void Render();

    /// 이펙트를 가지는 본을 저장한다.
    void InsertEffectBone(int nBone);

    /// 본의 TM 을 찾는다.
    D3DXMATRIX* FindBoneMatrix(int nBoneIndex);

    /// 이동 처리를 할 때 등록된 본 정보를 읽는다.
    virtual void ProcessTransform();

    /// 이펙터 처리시 상속 받은 함수를 통해 위치 및 TM을 얻는다.
    virtual D3DXMATRIX* GetEffectBoneMatrix(int nBoneIndex);
    virtual D3DXVECTOR3* GetEffectBoneTranslation(int nBoneIndex);

    /// 모델을 등록 삭제 한다.
    void InsertXModel();
    void DeleteXModel();
    /// 모델의 고유한 키를 생성한다.
    int GenerateKey(D3DXVECTOR3& vPos, char* pName);
#endif

	~XModel();
	virtual void FreeModel();
//	virtual void	InitXModel(const D3DXMATRIX &matrix, float ratio);
	virtual void	OnEnter();
	virtual void	OnLeave();
	virtual void	OnWait();
	virtual void	RenderFlag(int flag);
	virtual void	RenderFlagDecal(int flag) {}
	virtual void	RenderFlagShadow(int flag) {}
#ifndef TERRAIN_EDITOR
	virtual void	OnCollectLight();
	virtual void	OnPreRender();
#endif

#ifdef _TOOL
    virtual void	CollidePoint()       { if (!m_bHide) CGameEffectBoneModel::CollidePoint(); }
#endif

	void	OnLoadModel();
	//virtual void	OnAnimationOption(CKeyFrame *pKeyFrame);
};

class XSoundModel : public CModel, public XBaseModel
{
public:
	XSoundModel();
	~XSoundModel();
	virtual void	FreeModel();
//	virtual void	InitXModel(const D3DXMATRIX &matrix, float ratio);
	virtual void	OnEnter();
	virtual void	OnLeave();
	virtual void	OnWait();
	virtual void	OnAnimationOption(CKeyFrame *pKeyFrame);

	void	OnLoadModel();
	void	Destroy();
};
