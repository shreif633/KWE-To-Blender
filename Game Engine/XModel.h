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
    // X���� ���� ���� �Ѵ�.
    static std::map<DWORD, XBaseModel*> m_mapXModel;

    // ���� ã�´�.
    static XBaseModel* FindXModel(DWORD nXModelID);
    // �� ����
    static void InsertXModel(DWORD nXModelID, XBaseModel* pXModel);
    // ���� ����
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
	 *	���� ����(��)�� ó���Ѵ�.
     *  ���� �ݰ������ ������ ���ο� ���� OnEnter�� ȣ���Ѵ�.
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
    /// XModel �� ������ ����Ʈ �� ����� ���� �� �ִ�.
    std::map< int, D3DXMATRIX > m_mapEffectBoneMatrix;
    std::map< int, D3DXMATRIX >::iterator m_itEffectBoneMatrix;

    /// �������� ��� �޾� ���� �����͸� ���δ�.
    virtual void Render();

    /// ����Ʈ�� ������ ���� �����Ѵ�.
    void InsertEffectBone(int nBone);

    /// ���� TM �� ã�´�.
    D3DXMATRIX* FindBoneMatrix(int nBoneIndex);

    /// �̵� ó���� �� �� ��ϵ� �� ������ �д´�.
    virtual void ProcessTransform();

    /// ������ ó���� ��� ���� �Լ��� ���� ��ġ �� TM�� ��´�.
    virtual D3DXMATRIX* GetEffectBoneMatrix(int nBoneIndex);
    virtual D3DXVECTOR3* GetEffectBoneTranslation(int nBoneIndex);

    /// ���� ��� ���� �Ѵ�.
    void InsertXModel();
    void DeleteXModel();
    /// ���� ������ Ű�� �����Ѵ�.
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
