#include "StdAfx.h"
#include "model.h"
#include "ModelHeader.h"
#include "Resource.h"
#include "Utility.h"
#include "XParser.h"
#if	!defined(MODELVIEW) && !defined(TERRAIN_EDITOR)
#include "main.h"
#include "roam.h"
#include "GameData/GameEnv.h"
#include "MMap.h"
#include "XScript.h"
#include "KGameSys.h"
#endif
#if !defined (TERRAIN_EDITOR) // && defined (GAME_XENGINE)
#include "Effect/Effect_Explosion/Effect_Explosion.h"
#endif
#if !defined (TERRAIN_EDITOR) && !defined(MODELVIEW) // && defined(GAME_XENGINE)
#include "Effect/Effect_Sprite/Effect_Sprite.h"
#endif

CModel *			CModel::s_pModel;
D3DXMATRIXA16		CModel::s_vMatrix[MODEL_MAX_BONE];
D3DXMATRIXA16		CModel::s_vMatrixWorld[MODEL_MAX_BONE];
D3DXMATRIXA16		CModel::s_vMatrixTrans[MODEL_MAX_BONE];

DWORD				CMesh::s_dwOptions;
float				CMesh::s_fBlendOpacity;

static std::string	g_strTextureName[CMesh::MAX_TEXTURE];
static DWORD		g_dwTextureFlags[CMesh::MAX_TEXTURE];
static std::string	g_strEffectName;
static int			g_nVersion;

struct {
	WORD	vertex_count;
	WORD	face_count;
	float	minimum[3];
	float	maximum[3];
	WORD	vertex;
	WORD	face[3];
	GB_CLS_NODE	node;
} NullCollision = {
	1, 1, {-0.5f * METER, -METER, -0.5f * METER}, {0.5f * METER, METER, 0.5f * METER},
	0,
	{0, 0, 0},
	{GB_CLS_NODE::L_LEAF | GB_CLS_NODE::R_LEAF, 0, 0, 0, 0, 0, 0, 0, 0}
};

#ifdef GAME_XENGINE
namespace XClsModel {
	CStaticModel	*m_pModel;
	float	T;
	D3DXVECTOR3 m_vNormal;
	D3DXVECTOR3	C;
	D3DXVECTOR3	E;
	D3DXVECTOR3 V;
	D3DXVECTOR3 W;
	D3DXVECTOR3 CX2;
	D3DXVECTOR3 EX2;
	D3DXVECTOR3 m_vCenter;
	D3DXVECTOR3 m_vScale;
	WORD	*m_pClsVertex;
	WORD	*m_pClsIndex;
	CClsNode	*m_pClsNode;
	D3DXVECTOR3	A0, A1, A2;
	D3DXVECTOR3	A0xA1, A1xA2, A2xA0;
	D3DXVECTOR3	ABS_A0xA1, ABS_A1xA2, ABS_A2xA0;
	D3DXVECTOR3 Cross;
	D3DXVECTOR3 CrossX2;
	float	ABS_A0xA1_A2;
	float	ABS_A0xA1_A2X2;
	float	A0xA1_W, A1xA2_W, A2xA0_W;
	float	D;
	D3DXVECTOR3 m_vCamera;
	D3DXMATRIXA16 m_vMatrix;
	DWORD flag;
	D3DXVECTOR4 m_vVertex[32];
	D3DXVECTOR4 *m_pBegin;
	D3DXVECTOR4 *m_pEnd;

	void	SetClsPoint(float t);
	void	SetClsRect(float t);
}

#endif //GAME_XENGINE
#ifdef	_DEBUG
int cls_index;
#endif

BOOL CBone::Load(XFileCRC *pFile)
{
	for (int i = 0; i < m_nBoneData; i++) {
		GB_BONE gb_bone;
		pFile->Read(&gb_bone, sizeof(GB_BONE));
		m_pBoneData[i].m_mInverse = gb_bone.matrix;
		if (gb_bone.parent < MODEL_MAX_BONE)
			m_pBoneData[i].m_pmParent = &CModel::s_vMatrix[gb_bone.parent];
		else
			m_pBoneData[i].m_pmParent = 0;
	}
	if (m_nBoneData > 0) {
		ASSERT(m_pBoneData[0].m_pmParent == 0);
		m_pBoneData[0].m_pmParent = 0;
	}
	return TRUE;
}

BOOL CMaterial::Load(XFileCRC *pFile)
{
	//char *string_offset = (char *) m_pModelFile + ALIGN(sizeof(CModelFile));
	m_pKeyFrame = 0;
	for (int i = 0; i < m_nMatrialCount; i++) {
		GB_MATERIAL_KEY gb_key;
		pFile->Read(&gb_key, sizeof(GB_MATERIAL_KEY));
		m_pMaterialData[i].m_szTexture = m_pModelFile->m_pString + gb_key.szTexture;
		m_pMaterialData[i].m_fPower = gb_key.m_power;
		m_pMaterialData[i].m_mapoption = gb_key.mapoption;
		m_pMaterialData[i].m_szOption = m_pModelFile->m_pString + gb_key.szoption;
		m_pMaterialData[i].m_pframe =  (GB_MATERIAL_FRAME *)(m_pModelFile->m_pString + gb_key.m_frame);
		m_pMaterialData[i].m_nWCoord = 0;
		m_pMaterialData[i].m_pWCoord = 0;
	}
	return TRUE;
}	

void CMaterialData::Parse()
{
	if (m_szOption[0] == 0)
		return;

	XFile file((void *) m_szOption, 0xffffffff);
	XParser parser;
	parser.Open(&file);
	XParser::TOKEN token;
	std::string name;
	for ( ; ; ) {
		switch (token = parser.GetToken()) {
		case XParser::T_END:
			return;
		case XParser::T_CLOSE:
			BREAK();
			return;
		case XParser::T_OPEN:
			{
				if (parser.GetToken() != XParser::T_STRING) {
					BREAK();
					return;
				}

				name = parser.GetString();
				std::pair<CMaterialData *, LPCSTR> pair(this, name.c_str());
				parser.ParseList(ParseOption, (DWORD) &pair);
			}
			break;
		}
	}
}

DWORD CMaterialData::ParseOption(DWORD dwParam, lisp::var var)
{
	std::pair<CMaterialData *, LPCSTR> pair = *(std::pair<CMaterialData *, LPCSTR> *) dwParam;
	if (strcmp(pair.second, "*Billboard") == 0) {
		if (strcmp(var.car(), "Y") == 0) 
			pair.first->m_mapoption |= MATERIALEX_BILLBOARD_Y;
		else
			pair.first->m_mapoption |= MATERIALEX_BILLBOARD;
	}
	else if (strcmp(pair.second, "volume") == 0) {
		pair.first->m_mapoption |= MATERIALEX_VOLUME;
		pair.first->m_nWCoord = var.length();
		pair.first->m_pWCoord = (CFloatAnim *) malloc(pair.first->m_nWCoord * sizeof(CFloatAnim));
		for (CFloatAnim *pWCoord = pair.first->m_pWCoord; !var.null(); pWCoord++) {
			lisp::var pair = var.pop();
			pWCoord->dwTime = pair.pop();
			pWCoord->fValue = pair.pop();
		}
	}	
	return 0;
}

BOOL CMesh::Load(XFileCRC *pFile, DWORD dwFlags)
{
	m_pVertexBuffer = 0;
	m_pIndexBuffer = 0;
	for (int i = 0; i < MAX_TEXTURE; i++) {
		m_pTexture[i] = 0;
		m_evTexture[i].Initialize();
	}
	m_pFX = 0;
	m_evFX.Initialize();
	m_pCommand = 0;
	m_nCommand = 0;
	m_varScript = lisp::nil;

	GB_MESH_HEADER	gb_mesh_header;
	pFile->Read(&gb_mesh_header, sizeof(GB_MESH_HEADER));
	//char *string_offset = (char *) m_pModelFile + ALIGN(sizeof(CModelFile));
	m_szName = gb_mesh_header.name + m_pModelFile->m_pString;
	m_pMaterial = &m_pModelFile->m_material;
	m_pMatData = &m_pMaterial->m_pMaterialData[gb_mesh_header.material_ref];
/*
	m_szTextur
    e = gb_mesh_header.material + m_pModelFile->string_offset;
	m_fPower = gb_mesh_header.m_power;
	m_ambient = gb_mesh_header.m_ambient;
	m_diffuse = gb_mesh_header.m_diffuse;
	m_specular = gb_mesh_header.m_specular;
	m_opacity = gb_mesh_header.m_opacity;
	m_mapoption = gb_mesh_header.mapoption;
	m_szOption = gb_mesh_header.szoption + m_pModelFile->string_offset;
*/
	m_nVertex = gb_mesh_header.vertex_count;
	m_nBoneIndex = gb_mesh_header.bone_index_count;
	m_nVertexType = (VERTEX_TYPE) gb_mesh_header.vertex_type;
	m_dwFlags = 0;
	m_szTexture[0] = m_pMatData->m_szTexture;
	m_szTexture[1] = "";

    // 라이트맵 옵션이 있는 속성의 재질 처리
	if (m_pMatData->m_mapoption & MATERIAL_LIGHTMAP) {
		m_dwFlags |= F_LIGHTMAP;
	}

    // 리지드 더블일 경우만 라이트가 가능
	if (g_nVersion < 11 && m_nVertexType > 0) {
		m_nVertexType = (VERTEX_TYPE) (m_nVertexType - 1);
		if (m_nVertexType == VT_RIGID_DOUBLE) {
			m_dwFlags |= F_LIGHTMAP;
		}
	}
	if (gb_mesh_header.face_type == FT_LIST) {
		m_nFaceType = D3DPT_TRIANGLELIST;
		m_nFace = gb_mesh_header.index_count / 3;
	}
	else {
		m_nFaceType = D3DPT_TRIANGLESTRIP;
		m_nFace = gb_mesh_header.index_count - 2;
	}		
	switch (m_nVertexType) {
		case VT_RIGID:
		case VT_RIGID_DOUBLE:
			m_dwFlags |=  ((dwFlags & CModelFile::SHADER) && XSystem::m_caps.VertexShaderVersion < (DWORD)D3DVS_VERSION(1, 1)) ?
				F_SOFTWARE : 0;
			break;
		case VT_BLEND1: // 1개의 블렌딩 사용시 소프트웨어 버텍스 블렌딩 또는 하드웨어 버텍스 블렌딩 사용 유무
			m_dwFlags |= (XSystem::m_caps.VertexShaderVersion < (DWORD)D3DVS_VERSION(1, 1)) ? F_SOFTWARE | F_VSBLEND :  F_VSBLEND;
			break;
		case VT_BLEND2:
		case VT_BLEND3:
		case VT_BLEND4: // 2개 이상의 블렌딩 사용시 소프트웨어 버텍스 블렌딩(버텍스파레트사용) 또는 하드웨어 버텍스 블렌딩 사용 유무
			m_dwFlags |= (XSystem::m_caps.VertexShaderVersion < (DWORD)D3DVS_VERSION(1, 1)) ? F_SOFTWARE | F_FIXEDBLEND :  F_VSBLEND;
			break;
	}

	DWORD	dwUsage;
	D3DPOOL	pool;
	if (m_dwFlags & F_SOFTWARE) {
		pool = D3DPOOL_SYSTEMMEM;
		dwUsage = D3DUSAGE_WRITEONLY | D3DUSAGE_SOFTWAREPROCESSING;
 	}
	else {
		pool = D3DPOOL_MANAGED;
		dwUsage = D3DUSAGE_WRITEONLY; 
	}
	HRESULT hr;
	void *pBuffer;

    // 본 인덱스 로드
	pFile->Read(m_pBoneIndex, m_nBoneIndex);

    // 버텍스 사이즈 계산
	int nSize = XSystem::VertexSize[m_nVertexType] * m_nVertex;

    // 버텍스 생성
	hr = XSystem::m_pDevice->CreateVertexBuffer(nSize, dwUsage, 0, pool, &m_pVertexBuffer, 0);
	ASSERT(SUCCEEDED(hr));
	if( FAILED( hr)) return FALSE;
	hr = m_pVertexBuffer->Lock(0, 0, &pBuffer, 0);
	ASSERT(SUCCEEDED(hr));
	if( FAILED( hr)) return FALSE;

    // 버텍스 데이터 로드 
	pFile->Read(pBuffer, nSize);

	m_pVertexBuffer->Unlock();

    // 인덱싱 생성
	nSize = gb_mesh_header.index_count * sizeof(WORD);
	hr = XSystem::m_pDevice->CreateIndexBuffer(nSize, dwUsage, 
		D3DFMT_INDEX16, pool, &m_pIndexBuffer, 0); 

	ASSERT(SUCCEEDED(hr));

	if( FAILED( hr)) return FALSE;

	hr = m_pIndexBuffer->Lock(0, 0, &pBuffer, 0);

	ASSERT(SUCCEEDED(hr));

	if( FAILED( hr)) return FALSE;

    // 인덱싱 데이터 로드
	pFile->Read(pBuffer, nSize);

	m_pIndexBuffer->Unlock();
	return TRUE;
}

void CMesh::Free()
{
	if (m_pVertexBuffer)
		m_pVertexBuffer->Release();
	if (m_pIndexBuffer)
		m_pIndexBuffer->Release();

	SAFE_RELEASE(m_pFX);
	m_evFX.Clear();
	for (int i =0; i < MAX_TEXTURE; i++) {
		if (m_pTexture[i])
			m_pTexture[i]->Release();
		m_evTexture[i].Clear();
	}
	free(m_pCommand);
}

LPCSTR CMesh::GetTextureName(int nIndex)
{
	if (nIndex >= 2)
		return "";

	return m_szTexture[nIndex];
}

void CMesh::LoadTexture(int nIndex, LPCSTR szName, DWORD dwFlags)
{
	ASSERT(nIndex < MAX_TEXTURE);

	if (szName[0] == 0) 
		dwFlags = 0;
	g_strTextureName[nIndex] = szName;
	g_dwTextureFlags[nIndex] = dwFlags;
}

void CMesh::LoadEffect(LPCSTR szName)
{
	g_strEffectName = szName;
}


__forceinline void	CMesh::Draw()
{
#ifndef	_SOCKET_RELEASE
	if (XSystem::m_nWireFrame == 1 && (XEngine::m_nAction & XEngine::RENDER)) {
			XSystem::m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
#endif

#ifndef	TERRAIN_EDITOR
	if (m_pCommand)
		ExecuteScript();
	else
#endif
		XSystem::m_pDevice->DrawIndexedPrimitive(m_nFaceType, 0, 0, m_nVertex, 0, m_nFace);

#ifndef	_SOCKET_RELEASE 
	if (XSystem::m_nWireFrame == 2 && (XEngine::m_nAction & XEngine::RENDER)) 
    {
		XSystem::m_pDevice->SetTexture(0, 0);
		XSystem::m_pDevice->SetTexture(1, 0);
		XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		XSystem::m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
		if (XSystem::m_caps.RasterCaps & D3DPRASTERCAPS_DEPTHBIAS) {
			float fDepthBias = -0.001f;
			XSystem::m_pDevice->SetRenderState(D3DRS_DEPTHBIAS, *(DWORD *)&fDepthBias);
		}

		XSystem::m_pDevice->DrawIndexedPrimitive(m_nFaceType, 0, 0, m_nVertex, 0, m_nFace);
	
		XSystem::m_pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
		XSystem::m_pDevice->SetRenderState(D3DRS_DEPTHBIAS, 0);
	}
#endif
}

void CMesh::MapOptionPre()
{
	DWORD dwOptions = m_pMatData->m_mapoption;
	
	if (dwOptions | s_dwOptions) {
		if (dwOptions & (MATERIAL_LIGHTMAP | MATERIAL_FX)) {
			m_pMatData->m_mapoption &= ~(MATERIAL_LIGHTMAP | MATERIAL_FX);
		}
		if ( dwOptions & MATERIAL_TWOSIDED )
			XSystem::m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		
		if (dwOptions & MATERIAL_MAP_OPACITY )
		{
			int mode = m_pMatData->m_specular & 0xff;

			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHAREF, 128);
			
			if (mode)
			{
				XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

				if (m_pMatData->m_mapoption & MATERIAL_MAP_ALPHA_RGB)
				{				
					switch (mode)
					{
						case 1:		// blend
							XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
							XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
							break;
						case 2:		// add
							XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
							XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
							break;
						case 3:		// modulate
							XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
							XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
							break;
						case 4:		// modulate2x
							XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
							XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
							break;
						case 5:		// filter add
							XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
							XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
							break;
						case 228:	// alpha add
						case 229:
						default:
							XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
							XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
							break;
					}

					XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
					XSystem::m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				}
				else
				{
					XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
					XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);					
				}				
			}				
		}

		if ( m_pMatData->m_opacity < 1.0f)
		{
			XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 
				D3DXCOLOR(XSystem::m_material.Diffuse.r, XSystem::m_material.Diffuse.g, XSystem::m_material.Diffuse.b, m_pMatData->m_opacity));
			XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR );
		}	
		
		if ( dwOptions & MATERIAL_LIGHT )
		{
			XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE);
		}

		if ( dwOptions & MATERIAL_SPECULAR )
		{
			XSystem::m_pDevice->SetRenderState(D3DRS_SPECULARENABLE, TRUE);
			XSystem::m_pDevice->SetVertexShaderConstantF(0, 
				(float *)D3DXVECTOR4( 1.0f, m_pMatData->m_fPower, 0.0f, 765.01f ), 1);
			if (m_dwFlags & (F_VSBLEND | F_EFFECT)) {
				XSystem::m_pDevice->SetVertexShader(XSystem::VertexShaderSpecular[m_nVertexType]);
			}
			else {
				XSystem::m_material.Power = m_pMatData->m_fPower;
				XSystem::m_pDevice->SetMaterial(&XSystem::m_material);
			}
		}

		if ( dwOptions & MATERIALEX_TEXTURE)
		{
			if (!(m_dwFlags & F_VSBLEND)) // jufoot: VS 랑 TEXTURETRANSFORM 이랑 같이쓰면 에러난다
			{
				D3DXMATRIXA16 mat;
				D3DXMatrixIdentity(&mat);
				D3DXMatrixRotationYawPitchRoll(&mat, m_pMatData->m_angle.z, m_pMatData->m_angle.x, m_pMatData->m_angle.y);
				mat._31 = -m_pMatData->m_offset.x;
				mat._32 = m_pMatData->m_offset.y;
				XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE0, &mat);
				XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2 );
			}	
		}

		if (dwOptions & MATERIALEX_VOLUME) {
			CFloatAnim *pLast = m_pMatData->m_pWCoord + m_pMatData->m_nWCoord;
			CFloatAnim *pWCoord = ::upper_bound(m_pMatData->m_pWCoord, pLast, CModel::s_pModel->m_dwTime);
			ASSERT(pWCoord != m_pMatData->m_pWCoord);
			float fWCoord = (pWCoord != pLast) ? pWCoord[-1].fValue + (pWCoord[0].fValue - pWCoord[-1].fValue) * 
				(CModel::s_pModel->m_dwTime  - pWCoord[-1].dwTime) / (pWCoord[0].dwTime - pWCoord[-1].dwTime)
							: pWCoord[-1].fValue;
			m_pTexture[0]->SetWCoord(fWCoord);
		}
		if (s_dwOptions & F_BLEND_OPACITY) {
			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE); // DEF_THIEF_DIOB_081002 // 투명
			//XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
			XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR,
				D3DXCOLOR(XSystem::m_material.Diffuse.r, XSystem::m_material.Diffuse.g, XSystem::m_material.Diffuse.b,
				s_fBlendOpacity));
			XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR );
		}		
		if (dwOptions & MATERIALEX_SKIN)
		{
			// 원래 텍스쳐는 색깔만 바꿔서 렌더링
			XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR,m_skinColor);
			XSystem::m_pDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_TFACTOR);
			//XSystem::m_pDevice->SetTexture(0, m_pTexture->m_pTexture);
		}
	}
}

void CMesh::MapOptionPost()
{
	DWORD dwOptions = m_pMatData->m_mapoption;

	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

	if (dwOptions | s_dwOptions) {
		if (dwOptions & MATERIALEX_SKIN)
		{
			XSystem::m_pDevice->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
			
			// 그위에 블렌딩 텍스쳐
			if (m_pSkinTexture)
			{
				XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
				XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
				XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
				XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
				XSystem::m_pDevice->SetTexture(0,m_pSkinTexture->m_pTexture);
				Draw();
				XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
			}
		}
		if (s_dwOptions & F_BLEND_OPACITY) {
			//XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
			XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE); // DEF_THIEF_DIOB_081002 // 투명
		}
		if (dwOptions & MATERIALEX_VOLUME) {
			m_pTexture[0]->ResetWCoord();
		}

		if ( dwOptions & MATERIAL_TWOSIDED )
			XSystem::m_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);

		if (dwOptions & MATERIAL_MAP_OPACITY)
		{
			XSystem::m_pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE);
			XSystem::m_pDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE);
			XSystem::m_pDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE);
		}

		if (dwOptions & MATERIAL_MAP_OPACITY && m_pMatData->m_opacity < 1.0f )
		{
			XSystem::m_pDevice->SetRenderState( D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0x0F, 0x0F, 0xFF, 0xFF));
			XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
		}

	//	if ( m_pMatData->m_mapoption & (MATERIALEX_BILLBOARD | MATERAILEX_BILLBOARD_Y) )
	//	{
	//		XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, TRUE);
	//		XSystem::m_pDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE);
	//	}

		if ( dwOptions & MATERIAL_LIGHT )
		{
			XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, TRUE);
		}
		if ( dwOptions & MATERIAL_SPECULAR )
		{
			XSystem::m_pDevice->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
		}
		if ( dwOptions & MATERIALEX_TEXTURE)
		{
	//		D3DXMATRIXA16 mat;
	//		D3DXMatrixIdentity(&mat);
	//		XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE0, &mat);
			XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		}
	}
}

void CMesh::RenderStatic()
{
	if( NULL == m_pIndexBuffer || NULL == m_pVertexBuffer) {
		BREAK();
		return;
	}    

//	if (m_bSoftware)
//		XSystem::m_pDevice->SetSoftwareVertexProcessing(TRUE);
#ifdef GAME_XENGINE
//	if( XEngine::m_nAction == XEngine::RENDER)
//		XSystem::m_pDevice->SetRenderState( D3DRS_FOGENABLE, TRUE);
#endif

    XSystem::m_pDevice->SetIndices(m_pIndexBuffer);

    if (m_dwFlags & F_LIGHTMAP) {
        CBaseModel::PreRenderLM();
        //		XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX2);
        XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID_DOUBLE));
        XSystem::m_pDevice->SetTexture(0, m_pTexture[1]->m_pTexture);
        XSystem::m_pDevice->SetTexture(1, m_pTexture[0]->m_pTexture);
#ifndef	_SOCKET_RELEASE
        if (XSystem::m_dwFlags & XSystem::LM_HIDE) {
            XSystem::m_pDevice->SetTexture(0, XSystem::m_pWhite->m_pTexture);
        }
        if (XSystem::m_dwFlags & XSystem::TEXTURE_HIDE) {
            XSystem::m_pDevice->SetTexture(1, XSystem::m_pWhite->m_pTexture);
        }
#endif
    }
    else {
        CBaseModel::PreRender();
        //		XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
        XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID));
        XSystem::m_pDevice->SetTexture(0, m_pTexture[0]->m_pTexture);
    }
    XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);

	MapOptionPre();
	Draw();
	MapOptionPost();

#ifdef GAME_XENGINE
//	if( XEngine::m_nAction == XEngine::RENDER)
//		XSystem::m_pDevice->SetRenderState( D3DRS_FOGENABLE, FALSE);
#endif
//	if (m_bSoftware)
//		XSystem::m_pDevice->SetSoftwareVertexProcessing(FALSE);
}

void CMesh::RenderStaticDecal()
{

	XSystem::m_pDevice->SetIndices(m_pIndexBuffer);
	XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, XSystem::VertexSize[m_nVertexType]);
	//XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID));
	XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);

	XSystem::m_pDevice->DrawIndexedPrimitive(m_nFaceType, 0, 0, m_nVertex, 0, m_nFace);
} 

void CMesh::RenderStaticShadow()
{
	
	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(TRUE);

	ASSERT(!(m_dwFlags & (F_FIXEDBLEND | F_VSBLEND)));
	XSystem::m_pDevice->SetIndices(m_pIndexBuffer);
	XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID));
	
	XSystem::m_pDevice->DrawIndexedPrimitive(m_nFaceType, 0, 0, m_nVertex, 0, m_nFace);

	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(FALSE);
}

void CMesh::DrawPrimitiveOnly()
{
	ASSERT(!(m_dwFlags & (F_VSBLEND | F_FIXEDBLEND)));
	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(TRUE);
	XSystem::m_pDevice->SetIndices(m_pIndexBuffer);
	XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID));

	XSystem::m_pDevice->SetTexture(0, m_pTexture[0]->m_pTexture);

	Draw();
	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(FALSE);
}

void CMesh::Render()
{
	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(TRUE);

	if (m_dwFlags & (F_VSBLEND|F_EFFECT)) {
		// NOTE: 지포스4ti 에서 VS 를 키고 포그연산모드는 전부다 NONE 으로 세팅해야한다
		XSystem::m_pDevice->SetRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_NONE);
#if	0 && !defined(MODELVIEW) && !defined(TERRAIN_EDITOR)
		if (XEngine::m_nAction & XEngine::RENDER_WATER && XSystem::m_caps.MaxUserClipPlanes > 0)
		{
			// VS 에서 Clip Plane 변환을 안해주므로 직접해준다
			D3DXPLANE plane;
			clipPlane = g_pMyD3DApp->m_waterClipPlane;
			D3DXPlaneTransform(&plane, &clipPlane, &XSystem::m_mViewProjInvTrans);
			g_pMyD3DApp->SetClipPlane(&plane);
		}
#endif
		XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclVS[m_nVertexType]);
		XSystem::m_pDevice->SetVertexShader(XSystem::VertexShader[m_nVertexType]);
		int nIndex = 12;
		for (int i = 0 ; i < m_nBoneIndex; i++) {
			XSystem::m_pDevice->SetVertexShaderConstantF(nIndex, 
				(float *)&CModel::s_vMatrixTrans[m_pBoneIndex[i]], 3);
			nIndex += 3;
		}
	}
    // 버텍스 블렌딩 일 경우
	else if (!(m_dwFlags & F_FIXEDBLEND)) 
    {
		XSystem::m_pDevice->SetVertexShader(0);
		XSystem::m_pDevice->SetRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_EXP2);
		//XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
		XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);
		XSystem::m_pDevice->SetTransform(D3DTS_WORLD, &CModel::s_vMatrixWorld[m_pBoneIndex[0]]);
	}
    // 고정 블렌딩 일 경우 
	else 
    {
		XSystem::m_pDevice->SetVertexShader(0);
		XSystem::m_pDevice->SetRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_EXP2);
		XSystem::m_pDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE);  
		XSystem::m_pDevice->SetRenderState( D3DRS_VERTEXBLEND, XSystem::VertexBlendFlags[m_nVertexType]);
		XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);

		for (int i = 0 ; i < m_nBoneIndex; i++) 
        {
			XSystem::m_pDevice->SetTransform(D3DTS_WORLDMATRIX(i), &CModel::s_vMatrixWorld[m_pBoneIndex[i]]);
		}
	}
	XSystem::m_pDevice->SetIndices(m_pIndexBuffer);
	XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, XSystem::VertexSize[m_nVertexType]);
	
	XSystem::m_pDevice->SetTexture(0, m_pTexture[0]->m_pTexture);

	MapOptionPre();
	Draw();
	MapOptionPost();

#if	0 && !defined(MODELVIEW) && !defined(TERRAIN_EDITOR)
	if (dwVertexFlags & XSystem::VF_SHADER) 
	{ 
		if (XEngine::m_nAction & XEngine::RENDER_WATER && XSystem::m_caps.MaxUserClipPlanes > 0)
		{
			// 원래 클립 평면으로 돌려놓는다
			g_pMyD3DApp->SetClipPlane(&clipPlane);
		}
	}
#endif
	if (m_dwFlags & F_FIXEDBLEND) {
		XSystem::m_pDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);    
		XSystem::m_pDevice->SetRenderState( D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	}
	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(FALSE);
}


void CMesh::RenderShadow()
{

	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(TRUE);

#ifdef	SHADOW_EFFECT
	if (m_pShadowEffect || (m_dwFlags & (F_VSBLEND|F_EFFECT))) 
#else
	if (m_dwFlags & (F_VSBLEND|F_EFFECT)) 
#endif
	{
		XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclVS[m_nVertexType]);
		XSystem::m_pDevice->SetVertexShader(XSystem::m_pShadowVShader[m_nVertexType]);
		int nIndex = 12;
		for (int i = 0 ; i < m_nBoneIndex; i++) {
			XSystem::m_pDevice->SetVertexShaderConstantF(nIndex, 
				(float *)&CModel::s_vMatrixTrans[m_pBoneIndex[i]], 3);
			nIndex += 3;
		}
	}
	else if (!(m_dwFlags & F_FIXEDBLEND)) {
		//XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
		XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);
		XSystem::m_pDevice->SetVertexShader(0);
		XSystem::m_pDevice->SetTransform(D3DTS_WORLD, &CModel::s_vMatrixWorld[m_pBoneIndex[0]]);
	}
	else {
		XSystem::m_pDevice->SetVertexShader(0);
		XSystem::m_pDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, TRUE);  
		XSystem::m_pDevice->SetRenderState( D3DRS_VERTEXBLEND, XSystem::VertexBlendFlags[m_nVertexType]);
		XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);
		for (int i = 0 ; i < m_nBoneIndex; i++) {
			XSystem::m_pDevice->SetTransform(D3DTS_WORLDMATRIX(i), &CModel::s_vMatrixWorld[m_pBoneIndex[i]]);
		}
	}
	XSystem::m_pDevice->SetIndices(m_pIndexBuffer);
	XSystem::m_pDevice->SetStreamSource(0,m_pVertexBuffer, 0, XSystem::VertexSize[m_nVertexType]);

	XSystem::m_pDevice->DrawIndexedPrimitive(m_nFaceType, 0, 0, m_nVertex, 0, m_nFace);
	
	if (m_dwFlags & F_FIXEDBLEND) {
		XSystem::m_pDevice->SetRenderState( D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);    
		XSystem::m_pDevice->SetRenderState( D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	}
	if (m_dwFlags & F_SOFTWARE)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(FALSE);

}

BOOL CAnim::Load(XFileCRC *pFile, int bone_count, GB_ANIM *pAnimData)
{
	GB_ANIM_HEADER gb_anim_header;
	pFile->Read(&gb_anim_header, sizeof(GB_ANIM_HEADER));
	m_nKeyFrame = gb_anim_header.keyframe_count;

	//char *string_offset = (char *) m_pModelFile + ALIGN(sizeof(CModelFile));

	m_szAniOption = gb_anim_header.szoption + m_pModelFile->m_pString;

	int i;
	for ( i = 0; i < m_nKeyFrame; i++) {
		GB_KEYFRAME gb_keyframe;
		pFile->Read(&gb_keyframe, sizeof(GB_KEYFRAME));
		m_pKeyFrame[i].m_nTime = gb_keyframe.time;
		m_pKeyFrame[i].m_szOption = gb_keyframe.option + m_pModelFile->m_pString;
	}

	GB_ANIM **ppAnimData = m_ppAnimData;
	for ( i = 0; i < m_nKeyFrame; i++) {
		for (int j = 0; j < bone_count; j++) {
			WORD index;
			pFile->Read(&index, sizeof(WORD));
			*ppAnimData++ = pAnimData + index;
		}
	}

	return TRUE;
}

#ifdef GAME_XENGINE

void XClsModel::SetClsPoint(float t)
{
	if (XEngine::m_nAction & XEngine::LOW_FLOOR) {
		if (!(XClsModel::flag & (GB_CLS_NODE::L_FLOOR | GB_CLS_NODE::R_FLOOR))) {
			if (t <= XCollision::T) {
				XClsModel::m_vNormal = XCollision::m_vNormal; // restore
				return;
			}
			XCollision::T = t;
			XCollision::m_vNormal = XClsModel::m_vNormal; // save
			XClsModel::T = 0;
			return;
		}
		XEngine::m_nAction = XEngine::FLOOR;
		XClsModel::T = 1.0f;
		XCollision::T = 1.0f;
	}
	XClsModel::C += XClsModel::V * (t - 1);
	XClsModel::V *= t;
	XClsModel::T *= t;

	XClsModel::E.x = fabsf(XClsModel::V.x);
	XClsModel::E.y = fabsf(XClsModel::V.y);
	XClsModel::E.z = fabsf(XClsModel::V.z);
	XClsModel::CX2 = XClsModel::C * 2.0f;
	XClsModel::EX2 = XClsModel::E * 2.0f;
}

void XClsModel::SetClsRect(float t)
{
	XClsModel::V *= t;
	XClsModel::T *= t;

	XClsModel::C = XClsModel::m_vCenter + XClsModel::V;
	XClsModel::W = XClsModel::V * 2;
	XClsModel::A0xA1_W = D3DXVec3Dot(&XClsModel::A0xA1, &XClsModel::W);
	ASSERT(!NEGATIVE(XClsModel::A0xA1_W));
	XClsModel::Cross *= t;  
	XClsModel::CrossX2 = XClsModel::Cross * 2.0f;
	XClsModel::CX2 = XClsModel::C * 2.0f;
}


void CClsNode::GetVertex(D3DXVECTOR3* v, int index)
{
	WORD *pIndex = &XClsModel::m_pClsIndex[index];
	WORD *pVertex;

	pVertex = &XClsModel::m_pClsVertex[*pIndex];
	v->x = *pVertex * XClsModel::m_vScale.x;
	v->y = *++pVertex * XClsModel::m_vScale.y;
	v->z = *++pVertex * XClsModel::m_vScale.z;

	pVertex = &XClsModel::m_pClsVertex[*++pIndex];
	(++v)->x = *pVertex * XClsModel::m_vScale.x;
	v->y = *++pVertex * XClsModel::m_vScale.y;
	v->z = *++pVertex * XClsModel::m_vScale.z;

	pVertex = &XClsModel::m_pClsVertex[*++pIndex];
	(++v)->x = *pVertex * XClsModel::m_vScale.x;
	v->y = *++pVertex * XClsModel::m_vScale.y;
	v->z = *++pVertex * XClsModel::m_vScale.z;
}

void	CClsNode::CollidePoint(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax)
{
	D3DXVECTOR3 vExtentX2;
	D3DXVECTOR3 diffX2;
	// X
	if (XClsModel::EX2.x + (vExtentX2.x = vMax.x - vMin.x) <= fabsf(diffX2.x = vMin.x + vMax.x - XClsModel::CX2.x))
		return;
	// Y
	if (XClsModel::EX2.y + (vExtentX2.y = vMax.y - vMin.y) <= fabsf(diffX2.y = vMin.y + vMax.y - XClsModel::CX2.y))
		return;
	// Z
	if (XClsModel::EX2.z + (vExtentX2.z = vMax.z - vMin.z) <= fabsf(diffX2.z = vMin.z + vMax.z - XClsModel::CX2.z))
		return;
	// XClsModel::V Cross X
	if (vExtentX2.y * XClsModel::E.z + vExtentX2.z * XClsModel::E.y 
		< fabsf(diffX2.y * XClsModel::V.z - diffX2.z * XClsModel::V.y))
		return;
	// XClsModel::V Cross Y
	if (vExtentX2.x * XClsModel::E.z + vExtentX2.z * XClsModel::E.x 
		< fabsf(diffX2.z * XClsModel::V.x - diffX2.x * XClsModel::V.z))
		return;
	// XClsModel::V Cross Z
	if (vExtentX2.x * XClsModel::E.y + vExtentX2.y * XClsModel::E.x 
		< fabsf(diffX2.x * XClsModel::V.y - diffX2.y * XClsModel::V.x))
		return;
	D3DXVECTOR3 lmin, rmin, lmax, rmax;
	MinMax(&lmin, &rmin, &lmax, &rmax, vMin, vMax);
	if (flag & L_LEAF) {
		XClsModel::flag = (flag & (L_CAMERA|L_FLOOR|L_HIDDEN|L_NOPICK));
		if ((flag & (L_CAMERA|L_FLOOR|L_HIDDEN|L_NOPICK)) == 0) {
			if (XEngine::m_nAction & (XEngine::PICK | XEngine::COLLISION | XEngine::LOW_FLOOR))
				CollidePoint(left);
		}
		else if (flag & L_FLOOR) {
			if (XEngine::m_nAction & (XEngine::CAMERA | XEngine::PICK | XEngine::COLLISION | XEngine::FLOOR))
				CollidePoint(left);
		}
		else if (flag & L_CAMERA) {
			if (XEngine::m_nAction & (XEngine::CAMERA | XEngine::PICK | XEngine::COLLISION | XEngine::LOW_FLOOR))
				CollidePoint(left);
		}
		else if (flag & L_HIDDEN) {
			if (XEngine::m_nAction & (XEngine::COLLISION))
				CollidePoint(left);
		}
		else { // if (flag & L_NOPICK)
			if (XEngine::m_nAction & (XEngine::CAMERA | XEngine::COLLISION | XEngine::LOW_FLOOR))
				CollidePoint(left);
		}
	}
	else {
		XClsModel::m_pClsNode[left].CollidePoint(lmin, lmax);
	}
	if (flag & R_LEAF) {
		XClsModel::flag = (flag & (R_CAMERA|R_FLOOR|R_HIDDEN|R_NOPICK));
		if ((flag & (R_CAMERA|R_FLOOR|R_HIDDEN|R_NOPICK)) == 0) {
			if (XEngine::m_nAction & (XEngine::PICK | XEngine::COLLISION | XEngine::LOW_FLOOR))
				CollidePoint(right);
		}
		else if (flag & R_FLOOR) {
			if (XEngine::m_nAction & (XEngine::CAMERA | XEngine::PICK | XEngine::COLLISION | XEngine::FLOOR))
				CollidePoint(right);
		}
		else if (flag & R_CAMERA) {
			if (XEngine::m_nAction & (XEngine::CAMERA | XEngine::PICK | XEngine::COLLISION | XEngine::LOW_FLOOR))
				CollidePoint(right);
		}
		else if (flag & R_HIDDEN) {
			if (XEngine::m_nAction & (XEngine::COLLISION))
				CollidePoint(right);
		}
		else { // if (flag & R_NOPICK)
			if (XEngine::m_nAction & (XEngine::CAMERA | XEngine::COLLISION | XEngine::LOW_FLOOR))
				CollidePoint(right);
		}
	}
	else {
		XClsModel::m_pClsNode[right].CollidePoint(rmin, rmax);
	}

}

void CClsNode::CollidePoint(int index)
{
	D3DXVECTOR3 v[3];
	GetVertex(v, index);
#ifdef	_DEBUG
	D3DXVECTOR4 vWorld[3];
	for (int i = 0; i < 3; i++) {
		D3DXVec3Transform(&vWorld[i], 
			&D3DXVECTOR3(v[i] + XClsModel::m_pModel->m_pCollision->m_pModelFile->m_vMinimum),
			&XClsModel::m_pModel->m_matrix);
	}
#endif
	D3DXVECTOR3 e0 = v[1] - v[0];
	D3DXVECTOR3 e1 = v[2] - v[1];
	D3DXVECTOR3 n;
	D3DXVec3Cross(&n, &e0, &e1);
	// N
	float r = D3DXVec3Dot(&XClsModel::V, &n);
#ifdef	TWOSIDE
	if (!NEGATIVE(r)) {
		CHGSIGN(r);
		CHGSIGN(n);
		std::swap(v[0], v[2]);
		std::swap(e0, e1);
		CHGSIGN(e0);
		CHGSIGN(e1);
	}
#endif
	D3DXVECTOR3 v0 = v[0] - XClsModel::C;
	float d = D3DXVec3Dot(&v0, &n);

	if (r + fabsf(d) >= 0)
		return;
	// e0 cross XClsModel::V 
	D3DXVECTOR3 cross;
	if (D3DXVec3Dot(D3DXVec3Cross(&cross, &e0, &XClsModel::V), &v0) > 0)
		return;
	// e1 cross XClsModel::V 
	D3DXVECTOR3 v1 = v[1] - XClsModel::C;
	if (D3DXVec3Dot(D3DXVec3Cross(&cross, &e1, &XClsModel::V), &v1) > 0)
		return;
	// e2 cross XClsModel::V 
	D3DXVECTOR3 e2 = v[0] - v[2];
	if (D3DXVec3Dot(D3DXVec3Cross(&cross, &e2, &XClsModel::V), &v0) > 0)
		return;

	float t = d / r ;
	if (fabsf(t) >= 1.0f) {
		BREAK();
		return;
	}
	XClsModel::m_vNormal = n;
	XClsModel::SetClsPoint((t + 1) * 0.5f);
#ifdef	_DEBUG
	cls_index = index;
#endif
}

void CClsNode::CollideRect(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax)
{
	D3DXVECTOR3	vExtentX2, diffX2;
	// X
	if (XClsModel::EX2.x + (vExtentX2.x = vMax.x - vMin.x) <= fabsf(diffX2.x = vMin.x + vMax.x - XClsModel::CX2.x))
		return;
	// Y
	if (XClsModel::EX2.y + (vExtentX2.y = vMax.y - vMin.y) <= fabsf(diffX2.y = vMin.y + vMax.y - XClsModel::CX2.y))
		return;
	// Z
	if (XClsModel::EX2.z + (vExtentX2.z = vMax.z - vMin.z) <= fabsf(diffX2.z = vMin.z + vMax.z - XClsModel::CX2.z))
		return;
	// XClsModel::V Cross X
	if (XClsModel::CrossX2.x + vExtentX2.y * fabsf(XClsModel::V.z) + vExtentX2.z * fabsf(XClsModel::V.y)
		< fabsf(diffX2.y * XClsModel::V.z - diffX2.z * XClsModel::V.y))
		return;
	// XClsModel::V Cross Y
	if (XClsModel::CrossX2.y + vExtentX2.x * fabsf(XClsModel::V.z) + vExtentX2.z * fabsf(XClsModel::V.x) 
		< fabsf(diffX2.z * XClsModel::V.x - diffX2.x * XClsModel::V.z))
		return;
	// XClsModel::V Cross Z
	if (XClsModel::CrossX2.z + vExtentX2.x * fabsf(XClsModel::V.y) + vExtentX2.y * fabsf(XClsModel::V.x) 
		< fabsf(diffX2.x * XClsModel::V.y - diffX2.y * XClsModel::V.x))
		return;
	D3DXVECTOR3 lmin, rmin, lmax, rmax;
	MinMax(&lmin, &rmin, &lmax, &rmax, vMin, vMax);

	if (flag & L_LEAF) {
		if (flag & (L_CAMERA | L_FLOOR | L_NOPICK))
			CollideRect(left);
	}	
	else {
		XClsModel::m_pClsNode[left].CollideRect(lmin, lmax);
	}
	if (flag & R_LEAF) {
		if (flag & (R_CAMERA | R_FLOOR | R_NOPICK))
			CollideRect(right);
	}
	else {
		XClsModel::m_pClsNode[right].CollideRect(rmin, rmax);
	}

}

__forceinline BOOL CollideRect(
	D3DXVECTOR3 &A0xA1, 
	float A0xA1_W,
	D3DXVECTOR3 &v0, 
	D3DXVECTOR3 &e0, 
	D3DXVECTOR3 &e2, 
	float &tmin, 
	float &tmax)
{
	float pmin;
	float pmax;
	float p0;
	pmin = pmax = p0 = D3DXVec3Dot(&A0xA1, &v0);
	float p = D3DXVec3Dot(&A0xA1, &e0);
	if (p >= 0) 
		pmax += p;
	else
		pmin += p;
	p = p0 - D3DXVec3Dot(&A0xA1, &e2);
	if (p > pmax)
		pmax = p;
	else if (p < pmin)
		pmin = p;
	if (pmin  > tmin * A0xA1_W) 	{ 
		tmin = (pmin) / A0xA1_W;
		if (tmin >= tmax)
			return FALSE;
	}

	if (pmax  < tmax * A0xA1_W) {
		tmax = (pmax) / A0xA1_W;
		if (tmin >= tmax)
			return FALSE;
	}

	return TRUE;
}

__forceinline BOOL CollideRect(
	D3DXVECTOR3	&A0,
	D3DXVECTOR3 &e0,
	float		A0_n,
	D3DXVECTOR3 &v0,
	float	&tmin,
	float	&tmax)
{
	float r;
	D3DXVECTOR3 cross;
	D3DXVec3Cross(&cross, &A0, &e0);
	float w = D3DXVec3Dot(&XClsModel::W, &cross);
	float p0 = D3DXVec3Dot(&cross, &v0);
	if (NEGATIVE(w)) {
		CHGSIGN(w);
		CHGSIGN(p0);
		CHGSIGN(A0_n);
	}
	r = fabsf(D3DXVec3Dot(&XClsModel::A0xA1, &e0));
	if (A0_n > 0) {
		if (p0 - r > tmin * w) {
			tmin = (p0 - r) / w;
			if (tmin >= tmax)
				return FALSE;
		}
	}
	else {
		if (p0 + r < tmax * w) {
			tmax = (p0 + r)/w;
			if (tmin >= tmax)
				return FALSE;
		}
	}
	return TRUE;
}

void CClsNode::CollideRect(int index)
{
	float tmin = 0; 
	float tmax = 1.0f;
	D3DXVECTOR3 v[3];
	GetVertex(v, index);
#ifdef	_DEBUG
	D3DXVECTOR4 vWorld[3];
	for (int i = 0; i < 3; i++) {
		D3DXVec3Transform(&vWorld[i], 
			&D3DXVECTOR3(v[i] + XClsModel::m_pModel->m_pCollision->m_pModelFile->m_vMinimum),
			&XClsModel::m_pModel->m_matrix);
	}
#endif
	D3DXVECTOR3 e0 = v[1] - v[0];
	D3DXVECTOR3 e1 = v[2] - v[1];
	D3DXVECTOR3 n;
	D3DXVECTOR3 normal;
	D3DXVec3Cross(&n, &e0, &e1);
	// N
	float w = D3DXVec3Dot(&XClsModel::W, &n);
	if (!NEGATIVE(w)) {
#if		1
		CHGSIGN(w);
		n = -n;
		D3DXVECTOR3 t;
		t = v[0];
		v[0] = v[2], v[2] = t;
		t = e0;
		e0 = -e1; e1 = -t;
#else
		return;
#endif
	}
	D3DXVECTOR3 v0 = v[0] - XClsModel::m_vCenter;

	float d = D3DXVec3Dot(&v0, &n);
#if 0
	if (d >= 0)
		return;
#endif
	float R = fabsf(D3DXVec3Dot(&XClsModel::A0, &n)) + fabsf(D3DXVec3Dot(&XClsModel::A1, &n));
	if (tmin * w > d + R) { // (|d| - r)/|w| > tmin
		tmin = (d+R)/w;
		if (tmin >= tmax)
			return;
	}
	if (tmax * w < d - R) { // (|d| + r)/|w| < tmax
		tmax = (d-R)/w;
		if (tmin >= tmax)
			return;
	}
	// XClsModel::A0xA1
	D3DXVECTOR3 e2 = v[0] - v[2];
	if (!::CollideRect(XClsModel::A0xA1, XClsModel::A0xA1_W, v0, e0, e2, tmin, tmax))
		return;
	// XClsModel::A0 * e0
	float p;
	p = D3DXVec3Dot(&XClsModel::A0, &n);
	if (!::CollideRect(XClsModel::A0, e0, p, v0, tmin, tmax))
		return;
	// XClsModel::A0 * e1
	D3DXVECTOR3 v1 = v[1] - XClsModel::m_vCenter;
	if (!::CollideRect(XClsModel::A0, e1, p, v1, tmin, tmax))
		return;
	// XClsModel::A0 * e2
	if (!::CollideRect(XClsModel::A0, e2, p, v0, tmin, tmax))
		return;
	// XClsModel::A1 * e0
	p = D3DXVec3Dot(&XClsModel::A1, &n);
	if (!::CollideRect(XClsModel::A1, e0, p, v0, tmin, tmax))
		return;
	// XClsModel::A1 * e1
	if (!::CollideRect(XClsModel::A1, e1, p, v1, tmin, tmax))
		return;
	// XClsModel::A1 * e2
	if (!::CollideRect(XClsModel::A1, e2, p, v0, tmin, tmax))
		return;

	ASSERT(tmin < 1.0f);
	XClsModel::m_vNormal = n;
	XClsModel::D = D3DXVec3Dot(&v[0], &n);
	XClsModel::SetClsRect(tmin);
#ifdef	_DEBUG
	cls_index = index;
#endif
}

#if	!defined(TERRAIN_EDITOR)
template<int shift> __forceinline BOOL DrawDecal(D3DXVECTOR3& A1xA2, D3DXVECTOR3 &ABS_A1xA2, D3DXVECTOR3 &vSum, D3DXVECTOR3 &vDiff, int nFlags)
{
	if (nFlags & (3 << shift)) {
		float d = D3DXVec3Dot(&ABS_A1xA2, &vDiff);
		float c = D3DXVec3Dot(&vSum, &A1xA2);
		if (nFlags & (1 << shift)) {
			if (c <= XClsModel::ABS_A0xA1_A2X2 - d)
				nFlags &= ~(1 << shift);
			else if ( c >= XClsModel::ABS_A0xA1_A2X2 + d)
				return FALSE;
		}
		if (nFlags & (2 << shift)) {
			if (c <= -XClsModel::ABS_A0xA1_A2X2 - d) 
				return FALSE;
			else if ( c >= -XClsModel::ABS_A0xA1_A2X2 + d)
				nFlags &= ~(2 << shift);
		}
	}
	return TRUE;
}

void CClsNode::DrawDecal(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax, int nFlags)
{
	if (nFlags) {
	
		D3DXVECTOR3 vSum = vMax + vMin - XClsModel::CX2;
		D3DXVECTOR3 vDiff = vMax - vMin;

		if (!::DrawDecal<0>(XClsModel::A1xA2, XClsModel::ABS_A1xA2, vSum, vDiff, nFlags))
			return;
		if (!::DrawDecal<2>(XClsModel::A2xA0, XClsModel::ABS_A2xA0, vSum, vDiff, nFlags))
			return;
		if (!::DrawDecal<4>(XClsModel::A0xA1, XClsModel::ABS_A0xA1, vSum, vDiff, nFlags))
			return;
	}
	D3DXVECTOR3 lmin, rmin, lmax, rmax;
	MinMax(&lmin, &rmin, &lmax, &rmax, vMin, vMax);

	if (flag & L_LEAF) {
		if (!(flag & L_HIDDEN)) {
			DrawDecal(left, nFlags);
		}
	}	
	else {
		XClsModel::m_pClsNode[left].DrawDecal(lmin, lmax, nFlags);
	}
	if (flag & R_LEAF) {
		if (!(flag & R_HIDDEN)) {
			DrawDecal(right, nFlags);
		}
	}
	else {
		XClsModel::m_pClsNode[right].DrawDecal(rmin, rmax, nFlags);
	}
}

template<int shift> __forceinline BOOL DrawDecal(D3DXVECTOR3 &A1xA2, int nFlags)
{
	if (nFlags & (3 << shift)) {
		int nPositive = 0;
		for (D3DXVECTOR4 *p = XClsModel::m_pBegin; p != XClsModel::m_pEnd; p++) {
			D3DXPLANE plane(A1xA2.x * XClsModel::m_vScale.x,
				A1xA2.y * XClsModel::m_vScale.y,
				A1xA2.z * XClsModel::m_vScale.z,
				-D3DXVec3Dot(&XClsModel::C, &A1xA2) - XClsModel::ABS_A0xA1_A2);
			p->w = D3DXPlaneDotCoord(&plane, (D3DXVECTOR3 *)p);
			if (!NEGATIVE(p->w))
				nPositive++;
		}
		if (nFlags & ( 1 << shift)) {
			if (nPositive) {
				if (nPositive == XClsModel::m_pEnd - XClsModel::m_pBegin)
					return FALSE;
				D3DXVECTOR4 *p = XClsModel::m_pBegin;
				*(DWORD *) &XClsModel::m_pBegin ^= (DWORD) &XClsModel::m_vVertex[0] ^ (DWORD) &XClsModel::m_vVertex[16];
				D3DXVECTOR4 *q = XClsModel::m_pBegin;
				D3DXVECTOR4 *r = XClsModel::m_pEnd - 1;
				for (; p != XClsModel::m_pEnd;) {
					float fDistance = p->w;
					if (fDistance * r->w <= -0.0001f) {
						float t = fDistance / (fDistance - r->w);
						*(D3DXVECTOR3 *) q = *(D3DXVECTOR3 *) p + 
							(*(D3DXVECTOR3 *) r - *(D3DXVECTOR3 *) p) * t;
						q++->w = 0;
					}
					if (NEGATIVE(fDistance)) {
						*q++ = *p;
					}
					r = p++;
				}
				XClsModel::m_pEnd = q;
			}
		}
		if (nFlags & ( 2 << shift)) {
			nPositive = 0;
			for (D3DXVECTOR4 *p = XClsModel::m_pBegin; p != XClsModel::m_pEnd; p++) {
				p->w = -p->w - XClsModel::ABS_A0xA1_A2X2;
				if (!NEGATIVE(p->w))
					nPositive++;
			}
			if (nPositive) {
				if (nPositive == XClsModel::m_pEnd - XClsModel::m_pBegin)
					return FALSE;
				D3DXVECTOR4 *p = XClsModel::m_pBegin;
				*(DWORD *) &XClsModel::m_pBegin ^= (DWORD) &XClsModel::m_vVertex[0] ^ (DWORD) &XClsModel::m_vVertex[16];
				D3DXVECTOR4 *q = XClsModel::m_pBegin;
				D3DXVECTOR4 *r = XClsModel::m_pEnd - 1;
				for (; p != XClsModel::m_pEnd; ) {
					float fDistance = p->w;
					if (fDistance * r->w <= -0.0001f) {
						float t = fDistance / (fDistance - r->w);
						*(D3DXVECTOR3 *) q = *(D3DXVECTOR3 *) p + 
							(*(D3DXVECTOR3 *) r - *(D3DXVECTOR3 *) p) * t;
						q++->w = 0;
					}
					if (NEGATIVE(fDistance)) {
						*q++ = *p;
					}
					r = p++;
				}
				XClsModel::m_pEnd = q;
			}
		}
	}
	return TRUE;
}

void CClsNode::DrawDecal(int index, int nFlags)
{
	// GetVertex
	WORD *pIndex = &XClsModel::m_pClsIndex[index];
	WORD *pVertex;

	pVertex = &XClsModel::m_pClsVertex[*pIndex];
	XClsModel::m_vVertex[0].x = *pVertex;
	XClsModel::m_vVertex[0].y = *++pVertex;
	XClsModel::m_vVertex[0].z = *++pVertex;

	pVertex = &XClsModel::m_pClsVertex[*++pIndex];
	XClsModel::m_vVertex[1].x = *pVertex;
	XClsModel::m_vVertex[1].y = *++pVertex;
	XClsModel::m_vVertex[1].z = *++pVertex;

	pVertex = &XClsModel::m_pClsVertex[*++pIndex];
	XClsModel::m_vVertex[2].x = *pVertex;
	XClsModel::m_vVertex[2].y = *++pVertex;
	XClsModel::m_vVertex[2].z = *++pVertex;

	D3DXVECTOR3 e0 = *(D3DXVECTOR3 *) &XClsModel::m_vVertex[1] - *(D3DXVECTOR3 *) &XClsModel::m_vVertex[0];
	D3DXVECTOR3 e1 = *(D3DXVECTOR3 *) &XClsModel::m_vVertex[2] - *(D3DXVECTOR3 *) &XClsModel::m_vVertex[1];
	D3DXVECTOR3 n;
	D3DXVec3Cross(&n, &e0, &e1);
	// light direction
    if (D3DXVec3Dot(&n, &XClsModel::V) >= 0)
		return;
	// back face removal
	D3DXVECTOR3 v0 = *(D3DXVECTOR3 *) &XClsModel::m_vVertex[0] - XClsModel::m_vCamera;
	if (D3DXVec3Dot(&n, &v0) >= 0)
		return;

	XClsModel::m_pBegin = &XClsModel::m_vVertex[0];
	XClsModel::m_pEnd = &XClsModel::m_vVertex[3];
	if (nFlags) {
		if (!::DrawDecal<0>(XClsModel::A1xA2, nFlags))
			return;
		if (!::DrawDecal<2>(XClsModel::A2xA0, nFlags))
			return;
		if (!::DrawDecal<4>(XClsModel::A0xA1, nFlags))
			return;
	}
	ASSERT(XClsModel::m_pEnd - XClsModel::m_pBegin >= 3);

	D3DXVECTOR4 vOut[3]; 
	D3DXVECTOR4 *p = XClsModel::m_pBegin;
	D3DXVec3Transform(&vOut[0], (D3DXVECTOR3 *) p++, &XClsModel::m_vMatrix);
	D3DXVec3Transform(&vOut[1], (D3DXVECTOR3 *) p++, &XClsModel::m_vMatrix);
	for ( ; p < XClsModel::m_pEnd; ) {
		D3DXVec3Transform(&vOut[2], (D3DXVECTOR3 *) p++, &XClsModel::m_vMatrix);
		*XSystem::m_pVertex++ = *(D3DXVECTOR3 *)&vOut[0];
		*XSystem::m_pVertex++ = *(D3DXVECTOR3 *)&vOut[1];
		*XSystem::m_pVertex++ = *(D3DXVECTOR3 *)&vOut[2];
		*(D3DXVECTOR3 *) &vOut[1] = *(D3DXVECTOR3 *) &vOut[2];
	}
	if (XSystem::m_pVertex >= XSystem::m_pVertexEnd) {
		XSystem::UnlockVertex();
		XSystem::LockVertex();
	}
}
#endif

#if 0

void CClsNode::IntersectBox(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax)
{
	// X
	if (XClsModel::EX2.x + vMax.x - vMin.x <= fabsf(vMin.x + vMax.x - XClsModel::CX2.x))
		return;
	// Y
	if (XClsModel::EX2.y + vMax.y - vMin.y <= fabsf(vMin.y + vMax.y - XClsModel::CX2.y))
		return;
	// Z
	if (XClsModel::EX2.z + vMax.z - vMin.z <= fabsf(vMin.z + vMax.z - XClsModel::CX2.z))
		return;
	D3DXVECTOR3 lmin, rmin, lmax, rmax;
	MinMax(&lmin, &rmin, &lmax, &rmax, vMin, vMax);

	if (flag & L_LEAF) {
		IntersectBox(left);
	}	
	else {
		XClsModel::m_pClsNode[left].IntersectBox(lmin, lmax);
	}
	if (flag & R_LEAF) {
		IntersectBox(right);
	}
	else {
		XClsModel::m_pClsNode[right].IntersectBox(rmin, rmax);
	}
}

__forceinline BOOL IntersectBox(
	D3DXVECTOR3 &A0xA1, 
	D3DXVECTOR3 &v0, 
	D3DXVECTOR3 &e0, 
	D3DXVECTOR3 &e2)
{

	float pmin;
	float pmax;
	float p0;
	pmin = pmax = p0 = D3DXVec3Dot(&A0xA1, &v0);
	float p = D3DXVec3Dot(&A0xA1, &e0);
	if (p >= 0) 
		pmax += p;
	else
		pmin += p;
	p = p0 - D3DXVec3Dot(&A0xA1, &e2);
	if (p > pmax)
		pmax = p;
	else if (p < pmin)
		pmin = p;
	return (pmin - XClsModel::A0xA1_A2 < 0 && pmax + XClsModel::A0xA1_A2 > 0);
}


__forceinline BOOL IntersectBox(
	D3DXVECTOR3	&A0,
	D3DXVECTOR3 &e0,
	float		A0_n,
	D3DXVECTOR3 &A0xA1,
	D3DXVECTOR3 &A2xA0,
	D3DXVECTOR3 &v0)
{
	float r;
	D3DXVECTOR3 cross;
	D3DXVec3Cross(&cross, &A0, &e0);
	float p0 = D3DXVec3Dot(&cross, &v0);
	r = fabsf(D3DXVec3Dot(&A0xA1, &e0)) + fabsf(D3DXVec3Dot(&A2xA0, &e0));
	if (A0_n > 0) {
		return (p0 - r < 0);
	}
	else {
		return (p0 + r > 0);
	}
}

void CClsNode::IntersectBox(int index)
{
	if (XCollision::T <= 0)
		return;
	D3DXVECTOR3 v[3];
	GetVertex(v, index);
#ifdef	_DEBUG
	D3DXVECTOR4 vWorld[3];
	for (int i = 0; i < 3; i++) {
		D3DXVec3Transform(&vWorld[i], 
			&D3DXVECTOR3(v[i] + XClsModel::m_pModel->m_pCollision->m_pModelFile->m_vMinimum),
			&XClsModel::m_pModel->m_matrix);
	}
#endif
	D3DXVECTOR3 e0 = v[1] - v[0];
	D3DXVECTOR3 e1 = v[2] - v[1];
	D3DXVECTOR3 n;
	D3DXVec3Cross(&n, &e0, &e1);
	// N
	D3DXVECTOR3 v0 = v[0] - XClsModel::C;
	float d = fabsf(D3DXVec3Dot(&v0, &n));
	float R = fabsf(D3DXVec3Dot(&XClsModel::A0, &n)) + fabsf(D3DXVec3Dot(&XClsModel::A1, &n)) 
		    + fabsf(D3DXVec3Dot(&XClsModel::A2, &n));
	if (d >= R)
		return;
	// XClsModel::A0xA1
	D3DXVECTOR3 e2 = v[0] - v[2];
	if (!::IntersectBox(XClsModel::A0xA1, v0, e0, e2))
		return;
	// XClsModel::A1xA2
	if (!::IntersectBox(XClsModel::A1xA2, v0, e0, e2))
		return;
	// XClsModel::A2xA0
	if (!::IntersectBox(XClsModel::A2xA0, v0, e0, e2))
		return;
	// XClsModel::A0 * e0
	float p;
	p = D3DXVec3Dot(&XClsModel::A0, &n);
	if (!::IntersectBox(XClsModel::A0, e0, p, XClsModel::A0xA1, XClsModel::A2xA0, v0))
		return;
	// XClsModel::A0 * e1
	D3DXVECTOR3 v1 = v[1] - XClsModel::C;
	if (!::IntersectBox(XClsModel::A0, e1, p, XClsModel::A0xA1, XClsModel::A2xA0, v1))
		return;
	// XClsModel::A0 * e2
	if (!::IntersectBox(XClsModel::A0, e2, p, XClsModel::A0xA1, XClsModel::A2xA0, v0))
		return;
	// XClsModel::A1 * e0
	p = D3DXVec3Dot(&XClsModel::A1, &n);
	if (!::IntersectBox(XClsModel::A1, e0, p, XClsModel::A0xA1, XClsModel::A1xA2, v0))
		return;
	// XClsModel::A1 * e1
	if (!::IntersectBox(XClsModel::A1, e1, p, XClsModel::A0xA1, XClsModel::A1xA2, v1))
		return;
	// XClsModel::A1 * e2
	if (!::IntersectBox(XClsModel::A1, e2, p, XClsModel::A0xA1, XClsModel::A1xA2, v0))
		return;
	// XClsModel::A2 * e0
	p = D3DXVec3Dot(&XClsModel::A2, &n);
	if (!::IntersectBox(XClsModel::A2, e0, p, XClsModel::A2xA0, XClsModel::A1xA2, v0))
		return;
	// XClsModel::A2 * e1
	if (!::IntersectBox(XClsModel::A2, e1, p, XClsModel::A2xA0, XClsModel::A1xA2, v1))
		return;
	// XClsModel::A2 * e2
	if (!::IntersectBox(XClsModel::A2, e2, p, XClsModel::A2xA0, XClsModel::A1xA2, v0))
		return;

	XClsModel::m_vNormal = n;
	XClsModel::D = D3DXVec3Dot(&v[0], &n);
	XCollision::T = 0;
#ifdef	_DEBUG
	cls_index = index;
#endif
}
#endif

void CClsNode::CollideBox(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax)
{
	// X
	if (XClsModel::EX2.x + vMax.x - vMin.x <= fabsf(vMin.x + vMax.x - XClsModel::CX2.x))
		return;
	// Y
	if (XClsModel::EX2.y + vMax.y - vMin.y <= fabsf(vMin.y + vMax.y - XClsModel::CX2.y))
		return;
	// Z
	if (XClsModel::EX2.z + vMax.z - vMin.z <= fabsf(vMin.z + vMax.z - XClsModel::CX2.z))
		return;
	D3DXVECTOR3 lmin, rmin, lmax, rmax;
	MinMax(&lmin, &rmin, &lmax, &rmax, vMin, vMax);

#ifdef TERRAIN_EDITOR
	if (flag & L_LEAF) {
		if ( (XEngine::m_nAction & XEngine::COLLISION) || (XEngine::m_nAction & XEngine::PICK) )
		{
			if ( !(flag & L_NOPICK) )
				CollideBox( left);
		}
		else if ( !(flag & L_HIDDEN) && (flag & (L_CAMERA|L_FLOOR)))
			CollideBox( left);
	}
	else {
		XClsModel::m_pClsNode[left].CollideBox(lmin, lmax);
	}
	if (flag & R_LEAF) {
		if ( (XEngine::m_nAction & XEngine::COLLISION) || (XEngine::m_nAction & XEngine::PICK) )
		{
			if ( !(flag & R_NOPICK) )
				CollideBox(right);
		}
		else if (!(flag & R_HIDDEN) && (flag & (R_CAMERA|R_FLOOR)))
			CollideBox(right);
	}
	else {
		XClsModel::m_pClsNode[right].CollideBox(rmin, rmax);
	}

#else
	if (flag & L_LEAF) {
		CollideBox(left);
	}	
	else {
		XClsModel::m_pClsNode[left].CollideBox(lmin, lmax);
	}
	if (flag & R_LEAF) {
		CollideBox(right);
	}
	else {
		XClsModel::m_pClsNode[right].CollideBox(rmin, rmax);
	}

#endif
}

__forceinline BOOL CollideBox(
	D3DXVECTOR3 &A0xA1, 
	float A0xA1_W,
	D3DXVECTOR3 &v0, 
	D3DXVECTOR3 &e0, 
	D3DXVECTOR3 &e2, 
	float &tmin, 
	float &tmax)
{

	float pmin;
	float pmax;
	float p0;
	pmin = pmax = p0 = D3DXVec3Dot(&A0xA1, &v0);
	float p = D3DXVec3Dot(&A0xA1, &e0);
	if (p >= 0) 
		pmax += p;
	else
		pmin += p;
	p = p0 - D3DXVec3Dot(&A0xA1, &e2);
	if (p > pmax)
		pmax = p;
	else if (p < pmin)
		pmin = p;
	if (pmin - XClsModel::ABS_A0xA1_A2 > tmin * A0xA1_W) 	{ 
		tmin = (pmin - XClsModel::ABS_A0xA1_A2) / A0xA1_W;
		if (tmin >= tmax)
			return FALSE;
	}

	if (pmax + XClsModel::ABS_A0xA1_A2 < tmax * A0xA1_W) {
		tmax = (pmax + XClsModel::ABS_A0xA1_A2) / A0xA1_W;
		if (tmin >= tmax)
			return FALSE;
	}
	return TRUE;
}


__forceinline BOOL CollideBox(
	D3DXVECTOR3	&A0,
	D3DXVECTOR3 &e0,
	float		A0_n,
	D3DXVECTOR3 &A0xA1,
	D3DXVECTOR3 &A2xA0,
	D3DXVECTOR3 &v0,
	float	&tmin,
	float	&tmax)
{
	float r;
	D3DXVECTOR3 cross;
	D3DXVec3Cross(&cross, &A0, &e0);
	float w = D3DXVec3Dot(&XClsModel::W, &cross);
	float p0 = D3DXVec3Dot(&cross, &v0);
	if (NEGATIVE(w)) {
		CHGSIGN(w);
		CHGSIGN(p0);
		CHGSIGN(A0_n);
	}
	r = fabsf(D3DXVec3Dot(&A0xA1, &e0)) + fabsf(D3DXVec3Dot(&A2xA0, &e0));
	if (A0_n > 0) {
		if (p0 - r > tmin * w) {
			tmin = (p0 - r) / w;
			if (tmin >= tmax)
				return FALSE;
		}
	}
	else {
		if (p0 + r < tmax * w) {
			tmax = (p0 + r)/w;
			if (tmin >= tmax)
				return FALSE;
		}
	}
	return TRUE;
}

void CClsNode::CollideBox(int index)
{
	if (XCollision::T <= 0)
		return;
	float tmin = 0; 
	float tmax = XCollision::T;
	D3DXVECTOR3 v[3];
	GetVertex(v, index);
#ifdef	_DEBUG
	D3DXVECTOR4 vWorld[3];
	for (int i = 0; i < 3; i++) {
		D3DXVec3Transform(&vWorld[i], 
			&D3DXVECTOR3(v[i] + XClsModel::m_pModel->m_pCollision->m_pModelFile->m_vMinimum),
			&XClsModel::m_pModel->m_matrix);
	}
#endif
	D3DXVECTOR3 e0 = v[1] - v[0];
	D3DXVECTOR3 e1 = v[2] - v[1];
	D3DXVECTOR3 n;
	D3DXVec3Cross(&n, &e0, &e1);
	// N
	float w = D3DXVec3Dot(&XClsModel::W, &n);
	if (!NEGATIVE(w)) {
#ifdef	TWOSIDE
		CHGSIGN(w);
		n = -n;
		D3DXVECTOR3 t;
		t = v[0];
		v[0] = v[2], v[2] = t;
		t = e0;
		e0 = -e1; e1 = -t;
#else
		return;
#endif
	}
	D3DXVECTOR3 v0 = v[0] - XClsModel::m_vCenter;
	float d = D3DXVec3Dot(&v0, &n);
#if 1
	if (d >= 0)
		return;
#endif
	float R = fabsf(D3DXVec3Dot(&XClsModel::A0, &n)) + fabsf(D3DXVec3Dot(&XClsModel::A1, &n)) 
		    + fabsf(D3DXVec3Dot(&XClsModel::A2, &n));
	if (tmin * w > d + R) { // (|d| - r)/|w| > tmin
		tmin = (d+R)/w;
		if (tmin >= tmax)
			return;
	}
	if (tmax * w < d - R) { // (|d| + r)/|w| < tmax
		tmax = (d-R)/w;
		if (tmin >= tmax)
			return;
	}
	// XClsModel::A0xA1
	D3DXVECTOR3 e2 = v[0] - v[2];
	if (!::CollideBox(XClsModel::A0xA1, XClsModel::A0xA1_W, v0, e0, e2, tmin, tmax))
		return;
	// XClsModel::A1xA2
	if (!::CollideBox(XClsModel::A1xA2, XClsModel::A1xA2_W, v0, e0, e2, tmin, tmax))
		return;
	// XClsModel::A2xA0
	if (!::CollideBox(XClsModel::A2xA0, XClsModel::A2xA0_W, v0, e0, e2, tmin, tmax))
		return;
	// XClsModel::A0 * e0
	float p;
	p = D3DXVec3Dot(&XClsModel::A0, &n);
	if (!::CollideBox(XClsModel::A0, e0, p, XClsModel::A0xA1, XClsModel::A2xA0, v0, tmin, tmax))
		return;
	// XClsModel::A0 * e1
	D3DXVECTOR3 v1 = v[1] - XClsModel::m_vCenter;
	if (!::CollideBox(XClsModel::A0, e1, p, XClsModel::A0xA1, XClsModel::A2xA0, v1, tmin, tmax))
		return;
	// XClsModel::A0 * e2
	if (!::CollideBox(XClsModel::A0, e2, p, XClsModel::A0xA1, XClsModel::A2xA0, v0, tmin, tmax))
		return;
	// XClsModel::A1 * e0
	p = D3DXVec3Dot(&XClsModel::A1, &n);
	if (!::CollideBox(XClsModel::A1, e0, p, XClsModel::A0xA1, XClsModel::A1xA2, v0, tmin, tmax))
		return;
	// XClsModel::A1 * e1
	if (!::CollideBox(XClsModel::A1, e1, p, XClsModel::A0xA1, XClsModel::A1xA2, v1, tmin, tmax))
		return;
	// XClsModel::A1 * e2
	if (!::CollideBox(XClsModel::A1, e2, p, XClsModel::A0xA1, XClsModel::A1xA2, v0, tmin, tmax))
		return;
	// XClsModel::A2 * e0
	p = D3DXVec3Dot(&XClsModel::A2, &n);
	if (!::CollideBox(XClsModel::A2, e0, p, XClsModel::A2xA0, XClsModel::A1xA2, v0, tmin, tmax))
		return;
	// XClsModel::A2 * e1
	if (!::CollideBox(XClsModel::A2, e1, p, XClsModel::A2xA0, XClsModel::A1xA2, v1, tmin, tmax))
		return;
	// XClsModel::A2 * e2
	if (!::CollideBox(XClsModel::A2, e2, p, XClsModel::A2xA0, XClsModel::A1xA2, v0, tmin, tmax))
		return;

	ASSERT(tmin < XCollision::T);
	XClsModel::m_vNormal = n;
	XClsModel::D = D3DXVec3Dot(&v[0], &n);
	XCollision::T = tmin;
#ifdef	_DEBUG
	cls_index = index;
#endif
}

#endif //GAME_XENGINE

CModelFile::CModelFile()
{
	m_nMesh = 0;
	m_pString = 0;
	m_material.m_nMatrialCount = 0;	
}

CModelFile *CModelFile::Load(LPCSTR szName, DWORD dwFlags, DWORD dwPriority, lisp::var varScript)
{
	char szPath[MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	if (XSystem::m_dwFlags & XSystem::EDITEFFECT_MODE)
		dwFlags |= NO_CACHE;

    //@ 절대경로로 부터 드라이브명, 디텍토리명, 파일명, 확자자명을 얻는 함수
	_splitpath( szName, drive, dir, fname, ext );

    //@ 확장자가 없다면 확장자를 더하여 파일명을 만든다.
	if ( ext[0] == 0 )
		wsprintf(szPath, "%s.gb", szName);
	else
		strcpy(szPath, szName);

	if (XSystem::m_caps.VertexShaderVersion >= (DWORD)D3DVS_VERSION(1, 1))
		dwFlags &= ~CModelFile::SHADER;
	if (!varScript.empty()) {
		dwFlags |= CModelFile::SCRIPT;
	}
	int nHash = XSystem::GetHash(szPath);
	CModelFile *pModelFile = (CModelFile *) XSystem::Find(szPath, dwFlags, nHash);

	if (pModelFile) {
		if (dwPriority) {
			pModelFile->RaisePriority(dwPriority);
			return pModelFile;
		}
		return (CModelFile *) pModelFile->Wait();
	}

	pModelFile = XConstruct<CModelFile>();
	if (!pModelFile)
		return 0;
	pModelFile->m_varScript = varScript;
	pModelFile->Create(szPath, dwFlags, nHash);

	pModelFile->SetPriority(dwPriority);
	if (dwPriority) {
		pModelFile->AddRef();
		pModelFile->BgPostMessage((FUNCTION) &CModelFile::OnRead);
		return pModelFile;
	}

	if (!pModelFile->m_file.Open(szPath)) {
		TRACE("CModelFile::OnRead('%s') failed\n", szPath);
		pModelFile->m_dwFlags |= NO_CACHE;
		pModelFile->Release();
		return 0;
	}

	pModelFile->AddRef();
	pModelFile->Load();
	if (pModelFile->GetStatus() == XMessage::FAIL) {
		pModelFile->Release();
		return 0;
	}
	return pModelFile;
}

void CModelFile::OnRead()
{
	if (!m_file.Open(m_szName)) {
		TRACE("CModelFile::OnRead('%s') failed\n", m_szName);
		FgPostMessage((FUNCTION) &CModelFile::OnFail);
		return;
	}
	if (m_nRef != 1)
		m_file.Touch();
	FgPostMessage((FUNCTION) &CModelFile::OnLoad);
}

void CModelFile::OnFail()
{
	Clear();
	SetStatus(FAIL);
	m_xEventList.FireEvent();
	m_dwFlags |= NO_CACHE;
	Release();
}

void CModelFile::OnLoad()
{
	if (m_nRef == 1) {
		OnFail();
		return;
	}
	Load();
}

void CModelFile::AddRefAll()
{
	if (m_pBone)
		m_nRef++;
	if (m_pCollision)
		m_nRef++;
	m_nRef += m_nMesh + m_nAnim;
}

void CModelFile::ReleaseAll()
{
	if (m_pBone)
		m_nRef--;
	if (m_pCollision)
		m_nRef--;
	if ((m_nRef -= m_nMesh + m_nAnim) == 0)
		Destroy();
}

void CModelFile::Destroy()
{
	Clear();
	CResource::Clear();
	XDestruct(this);
}

void CModelFile::Clear()
{
	m_file.Close();
	for (int i = 0; i < m_nMesh; i++) {
		m_pMesh[i].Free();
	}
	m_nMesh = 0;
	for (int i = 0; i < m_material.m_nMatrialCount; i++) {
		free(m_material.m_pMaterialData[i].m_pWCoord);
	}
	m_material.m_nMatrialCount = 0;
	if (m_pString) {
		free(m_pString);
		m_pString = 0;
	}
}

// loader GB file
void CModelFile::Load()
{
    // declare gb_header
	GB_HEADER gb_header;

    // chk error empty file
	if (m_file.GetLength() == 0) {
		TRACE("CModelFile::Load('%s') Failed\n", m_szName);
		goto fail;
	}

    // load versn first
	g_nVersion = *m_file.m_pView;

    // chk versn
	if (g_nVersion != GB_HEADER_VERSION)
	{
        // chk directory
		if (!strnicmp("DATA\\OBJECTS\\b", m_szName, 14))
		{
			TRACE("CModelFile::Load: %s\n", m_szName);
			goto fail;
		}
	}

    // load header
	if (g_nVersion == GB_HEADER_VERSION) {
		m_file.XFile::Read(&gb_header, sizeof(gb_header));
        // builder header crc
		m_file.m_nCRC = UpdateCRC(*(DWORD *) &gb_header ^ MODEL_CRC_SEED, (char *) &gb_header + 8, sizeof(gb_header) - 8);
	}
	else if (g_nVersion >= 8) {
		memset(&gb_header, 0, sizeof(gb_header));
		m_file.XFile::Read(&gb_header, sizeof(DWORD));
		if (g_nVersion >= 12)
			m_file.XFile::Read(&gb_header.name, sizeof(gb_header.name));
		if (g_nVersion >= 10)
			m_file.XFile::Read(&gb_header.crc, sizeof(gb_header.crc));
		m_file.m_nCRC = *(DWORD *) &gb_header ^ MODEL_CRC_SEED;
		m_file.Read(&gb_header.szoption, sizeof(gb_header.szoption));
		if (g_nVersion >= 9)
			m_file.Read(&gb_header.vertex_count, sizeof(WORD) * 12);
		else
			m_file.Read(&gb_header.vertex_count, sizeof(WORD) * 6);
		m_file.Read(&gb_header.index_count, sizeof(gb_header.index_count));
		m_file.Read(&gb_header.bone_index_count, sizeof(gb_header.bone_index_count));
		m_file.Read(&gb_header.keyframe_count, sizeof(gb_header.keyframe_count));
		if (g_nVersion >= 9) {
			m_file.Read(&gb_header.reserved0, sizeof(gb_header.reserved0)); 
			m_file.Read(&gb_header.string_size, sizeof(gb_header.string_size));
			m_file.Read(&gb_header.cls_size, sizeof(gb_header.cls_size));
		}
		else {
			WORD string_count;					
			m_file.Read(&string_count, sizeof(string_count)); // string_count
			m_file.Read(&gb_header.string_size, sizeof(WORD));
			m_file.Read(&gb_header.cls_size, sizeof(WORD));
		}
		m_file.Read(&gb_header.anim_count, sizeof(gb_header.anim_count));
		m_file.Read(&gb_header.anim_file_count, sizeof(gb_header.anim_file_count));
		if (g_nVersion >= 9)
			m_file.Read(&gb_header.reserved1, sizeof(gb_header.reserved1));
		m_file.Read(&gb_header.material_count, sizeof(gb_header.material_count));
		m_file.Read(&gb_header.material_frame_count, sizeof(gb_header.material_frame_count));
		if (g_nVersion >= 11) {
			m_file.Read(&gb_header.minimum, sizeof(gb_header.minimum));
			m_file.Read(&gb_header.maximum, sizeof(gb_header.maximum));
		}
		if (g_nVersion >= 9)
			m_file.Read(&gb_header.reserved2, sizeof(gb_header.reserved2));

	}
	else {
		TRACE("*** ERROR VERSION CModelFile::Load('%s') Version(%d,%d)\n", m_szName, g_nVersion, GB_HEADER_VERSION );
		goto fail;
	}
	
#ifndef DEF_GB_FILE_NAME_CKK_REMOVE_OEN_080201
    // chk headename and basename
	if (g_nVersion >= 12)
	{
		char headername[64], basename[64];
		Decode(4, (BYTE *)headername, (BYTE *)gb_header.name, sizeof(gb_header.name));

        // extract file name
		ExtractFileBase(m_szName, basename);
		if (stricmp(headername, basename))
		{
			TRACE("CModelFile::Load: not match headername %s and filename %s\n", headername, basename);
			goto fail;
		}
	}
#endif

	m_vMinimum = gb_header.minimum;
	m_vMaximum = gb_header.maximum;

    // calc all mem size
	int size =  
		+ ALIGN(gb_header.string_size)
		+ (ALIGN(sizeof(CBone)) + ALIGN(gb_header.bone_count * sizeof(CBoneData))) * (gb_header.flags & MODEL_BONE)
		+ ALIGN(gb_header.material_count * sizeof(CMaterialData))
		+ ALIGN(gb_header.mesh_count * sizeof(CMesh))
		+ ALIGN(gb_header.bone_index_count)
		+ ALIGN(gb_header.anim_file_count * sizeof(CAnim))
		+ ALIGN(gb_header.keyframe_count * sizeof(CKeyFrame))
		+ ALIGN(gb_header.keyframe_count * gb_header.bone_count * sizeof(GB_ANIM *))
		+ ALIGN(gb_header.anim_count * sizeof(GB_ANIM));
	if (gb_header.cls_size)
		size += ALIGN(sizeof(CCollision) - sizeof(GB_CLS_HEADER) + gb_header.cls_size);

	m_pString = (char *) malloc(size);
	if (!m_pString) {
		goto fail;
	}

    // movd point
	char *ptr = m_pString + ALIGN(gb_header.string_size);
#ifdef	_DEBUG
	m_nBoneCount = gb_header.bone_count;
#endif

#ifndef	_SOCKET_TEST
	gb_header.flags |= MODEL_FX; 
#endif
#ifndef	TERRAIN_EDITOR
    // already being model script
	if ((gb_header.flags & MODEL_FX) && m_varScript.empty())
	{
		char szName[_MAX_PATH];
		int nLen = strlen(m_szName) - 3;
		memcpy(szName, m_szName, nLen);
		szName[nLen] = 0;
		XModelTable *pModelDesc = XModelTable::Find(szName);
		if (pModelDesc) {
			m_varScript = pModelDesc->m_varScript;
		}
	}
#endif

    // ----------------
    // insert bone data
    // ----------------
	if (gb_header.flags & MODEL_BONE) {
		m_pBone = (CBone *) ptr;
		m_pBone->m_pModelFile = this;
		ASSERT(gb_header.bone_count <= MODEL_MAX_BONE);
		m_pBone->m_nBoneData = gb_header.bone_count;
		ptr += ALIGN(sizeof(CBone));
		m_pBone->m_pBoneData = (CBoneData *) ptr;
		ptr += ALIGN(gb_header.bone_count * sizeof(CBoneData));
		m_pBone->Load(&m_file);
	}
	else
		m_pBone = 0;

    // --------------------
    // insert material head
    // --------------------
	m_material.m_pModelFile = this;
	m_material.m_nMatrialCount = gb_header.material_count;
	m_material.m_nMatrialFrameCount = gb_header.material_frame_count;
	if (!m_material.m_nMatrialCount) {
		m_material.m_nMatrialFrameCount = 0;
	}
	m_material.m_pMaterialData = (CMaterialData *) ptr;
	ptr += ALIGN(gb_header.material_count * sizeof(CMaterialData));
	m_material.Load(&m_file);

    // ----------------
    // insert mesh
    // ----------------
	m_pMesh = (CMesh *) ptr;
	ptr += ALIGN(gb_header.mesh_count * sizeof(CMesh));
	BYTE	*pBoneIndex = (BYTE *) ptr;
	ptr += ALIGN(gb_header.bone_index_count);
	m_nMesh = gb_header.mesh_count;
	for (int i = 0; i < gb_header.mesh_count; i++) {
		m_pMesh[i].m_pModelFile = this;
		m_pMesh[i].m_pBoneIndex = pBoneIndex;
		if (!m_pMesh[i].Load(&m_file, m_dwFlags)) {
			m_nMesh = i+1;
			goto fail;
		}
		pBoneIndex += m_pMesh[i].m_nBoneIndex;
	}
	ASSERT(!m_nMesh || m_material.m_nMatrialCount);

    // ----------------
    // insert ani
    // ----------------
	m_pAnim = (CAnim *) ptr;
	ptr += ALIGN(gb_header.anim_file_count * sizeof(CAnim));
	CKeyFrame *pKeyFrame = (CKeyFrame *) ptr;
	ptr += ALIGN(gb_header.keyframe_count * sizeof(CKeyFrame));
	GB_ANIM	**ppAnimData = (GB_ANIM **) ptr;

    // 키프레임수와 본의 수만큼 이동 정보를 읽어온다.
	ptr += ALIGN(gb_header.keyframe_count * gb_header.bone_count * sizeof(GB_ANIM *));
	GB_ANIM	*pAnimData = (GB_ANIM *) ptr;
	ptr += ALIGN(gb_header.anim_count * sizeof(GB_ANIM));
	m_nAnim = gb_header.anim_file_count;
	for (int i = 0; i < m_nAnim; i++) {
		m_pAnim[i].m_pModelFile = this;
		m_pAnim[i].m_pKeyFrame = pKeyFrame;
		m_pAnim[i].m_ppAnimData = ppAnimData;

        // GB_ANIM데이터는 이곳에서 로드하지 않는다.
		m_pAnim[i].Load(&m_file, gb_header.bone_count, pAnimData);
		pKeyFrame += m_pAnim[i].m_nKeyFrame;
		ppAnimData += gb_header.bone_count * m_pAnim[i].m_nKeyFrame;
	}
	m_file.Read(pAnimData, gb_header.anim_count * sizeof(GB_ANIM));
    
    // ----------------
    // load collision
    // ----------------
	if (gb_header.cls_size) {
		m_pCollision = (CCollision *) ptr;
		m_pCollision->m_pModelFile = this;
		m_file.Read(&m_pCollision->m_header, gb_header.cls_size);
        
		if (g_nVersion < 11) {
			m_vMinimum = *(D3DXVECTOR3 *)((char *) &m_pCollision->m_header + 4);
			m_vMaximum = *(D3DXVECTOR3 *)((char *) &m_pCollision->m_header + 16);
		}
		ptr += ALIGN(sizeof(CCollision) - sizeof(GB_CLS_HEADER) + gb_header.cls_size);
	}
	else
		m_pCollision = 0;

    // 사이즈와 현재 상태가 같은지 체크
	ASSERT(m_pString + _msize(m_pString) == ptr);
	//////////////////////////////////////////////
	//pFile->Read((char *) this + ALIGN(sizeof(CModelFile)), gb_header.string_size);
	//char *string_offset = (char *) this + ALIGN(sizeof(CModelFile));

    // load string
    // 연속된 스트링 정보를 로딩한다.
	m_file.Read(m_pString, gb_header.string_size);
    // 옵션 처리
	m_szOption =  m_pString + gb_header.szoption;

    // ----------------------
	// load info material
    // ----------------------
	if ( m_material.m_nMatrialFrameCount > 1 && m_nAnim ){
		ASSERT(m_material.m_nMatrialFrameCount == m_pAnim[0].m_nKeyFrame);
		m_material.m_pKeyFrame = m_pAnim[0].m_pKeyFrame;
	}
	for (int i = 0; i < m_material.m_nMatrialCount; i++) {
		m_material.m_pMaterialData[i].m_ambient = m_material.m_pMaterialData[i].m_pframe[0].m_ambient;
		m_material.m_pMaterialData[i].m_specular = m_material.m_pMaterialData[i].m_pframe[0].m_specular;
		m_material.m_pMaterialData[i].m_opacity = m_material.m_pMaterialData[i].m_pframe[0].m_opacity;
		m_material.m_pMaterialData[i].m_offset = m_material.m_pMaterialData[i].m_pframe[0].m_offset;
		m_material.m_pMaterialData[i].m_angle = m_material.m_pMaterialData[i].m_pframe[0].m_angle;
		m_material.m_pMaterialData[i].Parse();
	}

	char szPath[MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	_splitpath( m_szName, drive, dir, fname, ext );
	wsprintf( szPath, "%s%sTEX\\",drive, dir );
	XSystem::SetPath(0, szPath);

    // ----------------------
	// load texture
    // ----------------------
	m_nEvent = 0;
	int i;
	for ( i = 0; i < m_nMesh; i++) {
		CMesh *pMesh = &m_pMesh[i];

		if (pMesh->m_dwFlags & CMesh::F_LIGHTMAP) {
			pMesh->m_szTexture[1] = (g_nVersion >= 11) ? pMesh->m_szTexture[0] + strlen(pMesh->m_szTexture[0]) + 1
				: pMesh->m_szName + strlen(pMesh->m_szName) + 1;
		}
		g_strEffectName.clear();
		memset(g_dwTextureFlags, 0, sizeof(g_dwTextureFlags));
		pMesh->LoadTexture(0, pMesh->m_szTexture[0], 
				(pMesh->m_pMatData->m_mapoption & MATERIALEX_VOLUME) ? 
					CTexture::VOLUME | CTexture::NO_CACHE 	: CTexture::NO_CACHE);
		pMesh->LoadTexture(1, pMesh->m_szTexture[1], CTexture::NO_CACHE);
#ifndef	_SOCKET_TEST
		pMesh->m_pMatData->m_mapoption |= MATERIAL_FX;
#endif
#ifndef	TERRAIN_EDITOR
		if (!m_varScript.empty() && ((m_dwFlags & CModelFile::SCRIPT) || (pMesh->m_pMatData->m_mapoption & MATERIAL_FX)))
		{
			pMesh->PrepareScript(m_varScript);
		}
#endif
		if (!g_strEffectName.empty()) {
			CEffectFile *pEffectFile = CEffectFile::Load(g_strEffectName.c_str(), CResource::NO_CACHE, GetPriority());
			pMesh->m_pFX = pEffectFile;
			if (!pEffectFile) {
				TRACE("Can't load effect '%s' at CMesh::LoadEffect\n", g_strEffectName.c_str());
//				goto fail;
			}
			else if (pEffectFile->GetStatus() & XMessage::MASK) {
				m_nEvent++;
				pMesh->m_evFX.AddEvent(this, (XCallee::FUNCTION) &CModelFile::OnEvent, &pEffectFile->m_xEventList);
			}
		}

		for (int j = 0; j < CMesh::MAX_TEXTURE; j++) {
			if (g_dwTextureFlags[j] == 0)
				continue;
			CTexture *pTexture = CTexture::Load(g_strTextureName[j].c_str(), g_dwTextureFlags[j], GetPriority());
			pMesh->m_pTexture[j] = pTexture;
			if (!pTexture) {
				TRACE("Can't load texture '%s' at CModelFile::Load\n", g_strTextureName[j].c_str());
//				goto fail;
				XSystem::m_pWhite->AddRef();
				pMesh->m_pTexture[j] = XSystem::m_pWhite;
			}
			else if (pTexture->GetStatus() & XMessage::MASK) {
				m_nEvent++;
				pMesh->m_evTexture[j].AddEvent(this, (XCallee::FUNCTION) &CModelFile::OnEvent, &pTexture->m_xEventList);
			}
		}
	}

    // ----------------------
    // load ani
    // ----------------------
	for (i = 0; i < m_nAnim; i++) {
		m_pAnim[i].m_iAniLoopNo = 0;
#ifdef DEF_LOOP_OPT_OEN_080829
        m_pAnim[i].m_iAniLoopEndNo = 0;
#endif
		if (m_pAnim[i].m_szAniOption[0])
		{
			char * strOption;
			strOption = strstr(m_pAnim[i].m_szAniOption, "(*LOOP");
			if ( strOption )
			{
				strOption += 7;
				m_pAnim[i].m_iAniLoopNo = atoi(strOption);
			}
		}
		for (int k = 0; k < m_pAnim[i].m_nKeyFrame; k++){
			CKeyFrame * m_pTempKeyFrame = m_pAnim[i].m_pKeyFrame+k;
			if ( m_pTempKeyFrame->m_szOption[0]){
				m_pTempKeyFrame->m_strEffectModel = strstr(m_pTempKeyFrame->m_szOption, "*EFFECT_MODEL");
				if ( m_pTempKeyFrame->m_strEffectModel ) m_pTempKeyFrame->m_strEffectModel += 14;

				m_pTempKeyFrame->m_strSoundNo = strstr(m_pTempKeyFrame->m_szOption, "*SOUND");
				if ( m_pTempKeyFrame->m_strSoundNo ) m_pTempKeyFrame->m_strSoundNo += 7;

				char * strOption;
				strOption = strstr(m_pTempKeyFrame->m_szOption, "*MOTION");
				if ( strOption )
				{
					strOption += 8;
					m_pTempKeyFrame->m_fMotionNo = (float)atof(strOption);
				}
				else
					m_pTempKeyFrame->m_fMotionNo = -1;
			}
			else
			{
				m_pTempKeyFrame->m_strEffectModel = 0;
				m_pTempKeyFrame->m_strSoundNo = 0;
				m_pTempKeyFrame->m_fMotionNo = -1;
			}
		}
	}

	// Effect
	//if ( m_nMesh > 0 )
	//{
	//	fpos_t posC, posE;
	//	fgetpos(pFile, &posC);
	//	fseek(pFile, 0, SEEK_END);
	//	fgetpos(pFile, &posE);
	//	if ( posE > posC )
	//	{
	//		fsetpos(pFile, &posC);
	//	}
	//}
	m_file.Close();
	m_nEvent++;
	OnEvent();
	return;
fail:
	//Clear();
	OnFail();
	return;
}

void CModelFile::OnEvent()
{
	if (m_nRef == 1) {
		OnFail();
		return;
	}
	if (--m_nEvent) {
		return;
	}
	CMesh *pMesh = m_pMesh;
	for (int i = 0; i < m_nMesh; i++, pMesh++) {
		ASSERT(pMesh->m_evFX.Empty());
		if (pMesh->m_pFX && pMesh->m_pFX->GetStatus() != OK) {
//			OnFail();
//			return;
			pMesh->m_pFX->Release();
			pMesh->m_pFX = 0;
		}
		for (int j = 0; j < CMesh::MAX_TEXTURE; j++) {
			ASSERT(pMesh->m_evTexture[j].Empty());
			if (pMesh->m_pTexture[j] && pMesh->m_pTexture[j]->GetStatus() != OK) {
//				OnFail();
//				return;
				pMesh->m_pTexture[j]->Release();
				XSystem::m_pWhite->AddRef();
				pMesh->m_pTexture[j] = XSystem::m_pWhite;
			}
		}
#ifndef	TERRAIN_EDITOR
		if (!pMesh->m_varScript.null())
			pMesh->GenerateScript();
#endif
	}
	SetStatus(OK);
	m_xEventList.FireEvent();
	Release();
}

void CModelFile::RaisePriority(DWORD dwPriority)
{
	CMesh *pMesh = m_pMesh;
	for (int i = 0; i < m_nMesh; i++, pMesh++) {
		for (int j = 0; j < CMesh::MAX_TEXTURE; j++) {
			if (pMesh->m_pTexture[j])
				pMesh->m_pTexture[j]->CTexture::RaisePriority(dwPriority);
		}
	}
	CResource::RaisePriority(dwPriority);
}


void CBaseModel::PreRender()
{
	XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE);
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP);
}

void CBaseModel::PreRenderLM()
{
	XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
//	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0x0, 0x80, 0x80, 0x80));
	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0, 51, 51, 51));
	XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_ADD);
	XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
	XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
	XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
	XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
	XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 1 );
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP);
	XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP);

	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE);
	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE );
	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_MODULATE);
	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );
	XSystem::m_pDevice->SetTextureStageState( 2, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP);
	XSystem::m_pDevice->SetSamplerState(1, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP);
}


void CBaseModel::PreRenderShadow()
{
#ifdef	SHADOW_EFFECT
	if (XSystem::m_pShadowEffect) {
		XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	}
	else 
#endif
	{
		XSystem::m_pDevice->SetRenderState( D3DRS_LIGHTING, FALSE);
/*
		static const D3DXVECTOR3	LuminanceVector(0.2125f, 0.7154f, 0.0721f);

		if (MMap::IsLoaded()) {
			XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0xff, 0x80, 0x80, 0x80));
		}
		else {
			D3DCOLORVALUE color = XSystem::m_material.Diffuse;
			DWORD nColor = (DWORD) ((1 - D3DXVec3Dot((D3DXVECTOR3 *) &color, &LuminanceVector)) * 255);
			minimize(nColor, (DWORD) 255);
			XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0xff, nColor, nColor, nColor));
		}
*/
		XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0xff, 0x80, 0x80, 0x80));
		XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(0xFF, 0, 0, 0));
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );
		XSystem::m_pDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
		XSystem::m_pDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
	}
}

CStaticModel::CStaticModel()
{
	m_nMesh = 0;
	m_pCollision = 0;
//	m_bLoad = FALSE;
	m_dwFlags = 0;
	m_pModelFile = 0;
	m_fBlendOpacity = 1.0f;
}

void CStaticModel::Destroy()
{
	if (m_pModelFile) {
		m_event.RemoveEvent();
		m_pModelFile->Release();
		m_pModelFile = 0;
	}
	for (int i = 0; i < m_nMesh; i++) 
		m_vpMesh[i]->Release();
	m_nMesh = 0;
	if (m_pCollision) {
		m_pCollision->Release();
        m_pCollision = 0;
	}
#if !defined(TERRAIN_EDITOR)
	m_prtBundle.RemoveAll();
	m_fxBundle.RemoveAll();
#endif
//	m_bLoad = FALSE;
}

void	CStaticModel::FreeMesh(int nMesh)
{
	m_vpMesh[nMesh]->Release();
	memmove(&m_vpMesh[nMesh], &m_vpMesh[nMesh+1], (--m_nMesh - nMesh)*sizeof(CMesh*));
//	m_bLoad = FALSE;
}

void CStaticModel::FreeModel()
{
    if (m_pModelFile) {
        m_event.RemoveEvent();
        m_pModelFile->Release();
        m_pModelFile = 0;
    }
    for (int i = 0; i < m_nMesh; i++) 
        m_vpMesh[i]->Release();
    m_nMesh = 0;
    if (m_pCollision) {
        m_pCollision->Release();
        m_pCollision = 0;
    }
}

BOOL	CStaticModel::Load(LPCSTR szName, DWORD dwFlags, XCallee *pCallee, XCallee::FUNCTION pFunction, DWORD dwPriority)
{
	ASSERT(!m_pModelFile);
	m_caller.Set(pCallee, pFunction);
	m_pModelFile = CModelFile::Load(szName, dwFlags, dwPriority, m_varScript);
	if (m_pModelFile == 0) {
		m_caller.Call();
		return FALSE;
	}
	if (m_pModelFile->GetStatus() != XMessage::OK) {
		m_event.AddEvent(this, (XCallee::FUNCTION) &CStaticModel::OnEvent, &m_pModelFile->m_xEventList);
		return TRUE;
	}
	m_caller.Call();
	m_pModelFile->Release();
	m_pModelFile = 0;
	return TRUE;
}

void CStaticModel::OnEvent()
{
	if (m_pModelFile->GetStatus() != XMessage::OK) {
		m_pModelFile->Release();
		m_pModelFile = 0;
		m_caller.Call();
		return;
	}
	m_caller.Call();
	m_pModelFile->Release();
	m_pModelFile = 0;
}

void CStaticModel::OnLoad()
{
	if (!m_pModelFile)
		return;
	m_pModelFile->AddRefAll();
	ASSERT(!m_pModelFile->m_pBone);
	ASSERT(m_nMesh + m_pModelFile->m_nMesh < MODEL_MAX_MESH);
	for (int i = 0; i < m_pModelFile->m_nMesh; i++) {
		ASSERT(!(m_pModelFile->m_pMesh[i].m_dwFlags & (CMesh::F_VSBLEND | CMesh::F_FIXEDBLEND)));
		m_vpMesh[m_nMesh++] = &m_pModelFile->m_pMesh[i];

		if( m_pModelFile->m_pMesh[i].m_pMatData->m_mapoption & (MATERIAL_MAP_OPACITY | MATERIAL_MAP_ALPHA_RGB))
			m_dwFlags |= F_CONTAIN_TRANSPARENT;

		if (strstr(m_vpMesh[i]->m_pModelFile->m_szOption,"Water")) 
			m_dwFlags |= F_WATER | F_CONTAIN_TRANSPARENT;

	}
	ASSERT(!m_pModelFile->m_nAnim);

	ASSERT(!m_pCollision || !m_pModelFile->m_pCollision);
	m_pCollision = m_pModelFile->m_pCollision;
#ifdef GAME_XENGINE
	if (m_pCollision) {
		m_pClsVertex = (WORD *)(&m_pCollision->m_header + 1);
		m_pClsIndex = m_pClsVertex + m_pCollision->m_header.vertex_count * 3;
		m_pClsNode = (CClsNode *)(m_pClsIndex + m_pCollision->m_header.face_count * 3);
	}
#endif
	m_szOption = m_pModelFile->m_szOption;
	
//	m_bLoad = TRUE;
}
#ifdef GAME_XENGINE

void CStaticModel::OnLoadModel()
{
	if (!m_pModelFile)
		return;
	OnLoad();

	D3DXVECTOR3 vCenter = 0.5f * (m_pModelFile->m_vMaximum + m_pModelFile->m_vMinimum);
	m_vEdge = m_pModelFile->m_vMaximum - vCenter;
	D3DXVec3TransformNormal(&vCenter, &vCenter, &m_matrix);
	vCenter += *(D3DXVECTOR3 *)&m_matrix._41;
	SetCenter(vCenter);

	SetExtent(m_vEdge, m_matrix);
	m_fRadius = sqrtf(m_vEdge.x * m_vEdge.x * D3DXVec3LengthSq((D3DXVECTOR3 *) &m_matrix._11)
		+ m_vEdge.y * m_vEdge.y * D3DXVec3LengthSq((D3DXVECTOR3 *) &m_matrix._21)
		+ m_vEdge.z * m_vEdge.z * D3DXVec3LengthSq((D3DXVECTOR3 *) &m_matrix._31)
		+ 2 * m_vEdge.x * m_vEdge.y * fabsf(D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._11, (D3DXVECTOR3 *) &m_matrix._21))
		+ 2 * m_vEdge.y * m_vEdge.z * fabsf(D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._21, (D3DXVECTOR3 *) &m_matrix._31))
		+ 2 * m_vEdge.z * m_vEdge.x * fabsf(D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._31, (D3DXVECTOR3 *) &m_matrix._11)));
	XEngine::InsertObject(this);
}
CStaticModel *CStaticModel::CreateModel(LPCSTR szName, const D3DXMATRIX& matrix)
{
	CStaticModel *pStatic = new CStaticModel;
	pStatic->m_matrix = matrix;
	D3DXMatrixInverse(&pStatic->m_inverse, NULL, &matrix);
	if (!pStatic->LoadModel(szName)) {
		delete pStatic;
		return NULL;
	}
	return pStatic;
}
#endif

// 전처리
void CStaticModel::PreProcessOption()
{
	if( m_dwFlags)
	{
		if( m_dwFlags & F_FORCE_AMBIENT)
		{
			XSystem::m_material.Ambient = m_ambientColor;
			XSystem::m_pDevice->SetMaterial( &XSystem::m_material);
		}

		if( m_dwFlags & F_BLEND_OPACITY)
		{
			CMesh::s_dwOptions |= CMesh::F_BLEND_OPACITY; // DEF_THIEF_DIOB_081002 // 투명
			CMesh::s_fBlendOpacity = m_fBlendOpacity;
		}

		if( m_dwFlags & F_REFLECT_TRANSFORM)
		{
			XSystem::m_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW);
		}
	}
}

// 후처리
void CStaticModel::PostProcessOption()
{
	if( m_dwFlags)
	{
		if( m_dwFlags & F_REFLECT_TRANSFORM)
		{
			XSystem::m_pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW);
		}

		if( m_dwFlags & F_BLEND_OPACITY)
		{
            CMesh::s_dwOptions &= ~CMesh::F_BLEND_OPACITY; // DEF_THIEF_DIOB_081002 // 투명
		}

		if( m_dwFlags & F_FORCE_AMBIENT)
		{
			XSystem::m_material.Ambient = g_bOverBright ? D3DXCOLOR( 0.25f, 0.25f, 0.25f, 1) : D3DXCOLOR( 0.5f, 0.5f, 0.5f, 1);
			XSystem::m_pDevice->SetMaterial( &XSystem::m_material);
		}
	}
}

extern float g_waterHeight;

void CStaticModel::Render()
{
//	if ( m_bLoad == FALSE ) return;
	
//	PreRender();
//	XSystem::m_pDevice->SetFVF( D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);	
#if !defined(TERRAIN_EDITOR)
	if (MMap::IsLoaded())
		OnPreRender();
#endif

	m_nFrameNumber = XSystem::m_nFrameNumber;
	PreProcessOption();

    // 월드 행렬 설정
	XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &m_matrix);

    // 쉐이더 설정
	XSystem::m_pDevice->SetVertexShader(0);

    // 메쉬 드로윙
	for (int i = 0; i < m_nMesh; i++) 
    {
		BOOL bBillboard = false;

		CMesh *pMesh = m_vpMesh[i];

        // 맵 옵션이 빌보드 며 Y축 빌보드라면
		if (pMesh->m_pMatData->m_mapoption & (MATERIALEX_BILLBOARD | MATERIALEX_BILLBOARD_Y))
		{
			if (pMesh->m_pMatData->m_mapoption & MATERIALEX_BILLBOARD)
			{
				bBillboard = true;
				D3DXMATRIXA16 matTempWorld;
				D3DXMATRIXA16 mRotation;

				D3DXVECTOR3 vFrom(m_matrix._41, m_matrix._42, m_matrix._43);
				vFrom -= XSystem::m_vCamera;
				float  fYaw=0.0f;
				float  fPitch = -XSystem::m_fPitch / 2.0f;

				if ( vFrom.z < 0 )
					fYaw = atanf(vFrom.x / vFrom.z) + D3DX_PI ;
				else
					fYaw = atanf(vFrom.x / vFrom.z);

				//fYaw += XSystem::m_fYaw;
				//fYaw = XSystem::m_fYaw ; //+ ( XSystem::m_fYaw - fYaw );

				//D3DXMatrixRotationYawPitchRoll(&mRotation, XSystem::m_fYaw, -XSystem::m_fPitch/2, 0);
				D3DXMatrixRotationYawPitchRoll(&mRotation, fYaw, fPitch, 0);
				D3DXMatrixMultiply(&matTempWorld, &mRotation, &m_matrix);
				XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &matTempWorld);
			}
			else if (pMesh->m_pMatData->m_mapoption & MATERIALEX_BILLBOARD_Y)
			{
				bBillboard = true;
				D3DXMATRIXA16 matTempWorld;
				D3DXMATRIXA16 mRotation;
				D3DXMatrixRotationY(&mRotation, XSystem::m_fYaw);
				D3DXMatrixMultiply(&matTempWorld, &mRotation, &m_matrix);
				XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &matTempWorld);
			}
		}

#if !defined(MODELVIEW) && !defined(TERRAIN_EDITOR)
		if ((m_dwFlags & F_WATER) && !(XEngine::m_nAction & XEngine::RENDER_WATER))
		{			
			g_waterHeight = m_vCenter.y;
			g_pMyD3DApp->m_Water.DrawMesh(pMesh);
		}
		else
#endif
		{
			pMesh->RenderStatic();
		}

		if ( bBillboard)
			XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &m_matrix);
	}

	PostProcessOption();
#if !defined(TERRAIN_EDITOR)
	for (std::vector<particleInfo_t>::iterator it = m_prtInfoVector.begin(); it != m_prtInfoVector.end(); it++)
	{
		D3DXVECTOR3 t;
		D3DXVECTOR3 offset = it->offset;
		t.x = m_matrix._11 * offset.x + m_matrix._21 * offset.y + m_matrix._31 * offset.z + m_matrix._41;
		t.y = m_matrix._12 * offset.x + m_matrix._22 * offset.y + m_matrix._32 * offset.z + m_matrix._42;
		t.z = m_matrix._13 * offset.x + m_matrix._23 * offset.y + m_matrix._33 * offset.z + m_matrix._43;
		m_prtBundle.AddParticle(this, it->name.c_str(), t, m_matrix);
	}
#endif
}

void CStaticModel::DrawPrimitiveOnly()
{
	for (int i = 0; i < m_nMesh; i++) {
		m_vpMesh[i]->DrawPrimitiveOnly();
	}
}

void CStaticModel::RenderDecal()
{
	XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &m_matrix);
	for (int i = 0; i < m_nMesh; i++)
		m_vpMesh[i]->RenderStaticDecal();
}

void CStaticModel::RenderShadow()
{
//	if ( m_bLoad == FALSE ) return;
	PreRenderShadow();

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
	//XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[m_nVertexType]);
#ifdef	SHADOW_EFFECT
	if (XSystem::m_pShadowEffect) {
		D3DXMATRIXA16 mWorldViewTrans;
		D3DXMatrixMultiplyTranspose(&mWorldViewTrans, &m_matrix, &XSystem::m_mView);
		XSystem::m_pDevice->SetVertexShader(XSystem::m_pShadowVShader[VT_RIGID]);
		XSystem::m_pDevice->SetVertexShaderConstantF(12, (float *)&mWorldViewTrans, 3);
	}
	else 
#endif
	{
		XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &m_matrix);
	}

	for (int i = 0; i < m_nMesh; i++) {
		m_vpMesh[i]->RenderStaticShadow();
	}
	XSystem::m_pDevice->SetVertexShader(0);
}

void CStaticModel::SetSkin(int index,D3DCOLOR color,CTexture *texture)
{
	ASSERT(index >= 0 && index < 10);

	for (int i = 0; i < m_nMesh; i++) {
		CMesh *pMesh = m_vpMesh[i]; 
		if (pMesh->m_pMatData->m_szTexture[0] == '$')
		{
			// 스킨 텍스쳐는 최대 10 개 까지만 지정가능
			if (pMesh->m_pMatData->m_szTexture[1] - '0' == index) {
				pMesh->m_skinColor = color;
				pMesh->m_pSkinTexture = texture;
				pMesh->m_pMatData->m_mapoption |= MATERIALEX_SKIN;
				return;
			}
		}
	}
	TRACE("CStaticModel::SetSkin: skin mesh not found\n");
}

#ifdef GAME_XENGINE

void CStaticModel::RenderFlag(int flag)
{
	m_nRenderNumber = XSystem::m_nRenderNumber;
	ASSERT(IsValidNode());
	if (flag > 0)
	{
		flag = XSystem::IsBoxIn(m_vCenter, m_vEdge, m_matrix, flag);
		if (flag < 0)
			return;
	}
//	if (XEngine::m_nAction & XEngine::RENDER)
//		m_nFrameNumber = XSystem::m_nFrameNumber;
	if( m_dwFlags & F_CONTAIN_TRANSPARENT)
		XSortTransparent::Insert( this);
	else {
		Render();
	}
}

#if	!defined(TERRAIN_EDITOR)
void CStaticModel::RenderFlagDecal(int flag)
{
	if (m_nFrameNumber != XSystem::m_nFrameNumber)
		return;

	if (flag > 0)
	{
		flag = XSystem::IsBoxIn(m_vCenter, m_vExtent, flag);
		if (flag < 0)
			return;

		if (XSystem::m_decalOrigin.x - XSystem::m_decalExtent.x > m_vCenter.x + m_vExtent.x || 
			XSystem::m_decalOrigin.x + XSystem::m_decalExtent.x < m_vCenter.x - m_vExtent.x)
			return;
		if (XSystem::m_decalOrigin.y - XSystem::m_decalExtent.y > m_vCenter.y + m_vExtent.y || 
			XSystem::m_decalOrigin.y + XSystem::m_decalExtent.y < m_vCenter.y - m_vExtent.y)
			return;
		if (XSystem::m_decalOrigin.z - XSystem::m_decalExtent.z > m_vCenter.z + m_vExtent.z || 
			XSystem::m_decalOrigin.z + XSystem::m_decalExtent.z < m_vCenter.z - m_vExtent.z)
			return;
	}

	XSystem::m_pDevice->SetTransform(D3DTS_WORLD,  &m_matrix);
	XSystem::RenderDecal(this);
}
#endif

void CStaticModel::CollidePoint()
{
	if (!m_pCollision || !TestPoint())
		return;

	XClsModel::m_pModel = this;
	XClsModel::T = 1.0f;
	// XClsModel::C = vCenter X Inverse(m_matrix)
	D3DXVECTOR3 vCenter = XCollision::C - *(D3DXVECTOR3 *)&m_matrix._41;
	D3DXVec3TransformNormal(&XClsModel::C, &vCenter, &m_inverse);
	XClsModel::C -= m_pCollision->m_pModelFile->m_vMinimum;
	// XClsModel::V = XCollision::V X Inverse(m_matrix)
	D3DXVec3TransformNormal(&XClsModel::V, &XCollision::V, &m_inverse);
	XClsModel::E.x = fabsf(XClsModel::V.x);
	XClsModel::E.y = fabsf(XClsModel::V.y);
	XClsModel::E.z = fabsf(XClsModel::V.z);
	XClsModel::CX2 = XClsModel::C * 2.0f;
	XClsModel::EX2 = XClsModel::E * 2.0f;
	XClsModel::m_pClsVertex = m_pClsVertex;
	XClsModel::m_pClsIndex = m_pClsIndex;
	XClsModel::m_pClsNode = m_pClsNode;
	D3DXVECTOR3 minimum(0, 0, 0);
	D3DXVECTOR3 maximum = m_pCollision->m_pModelFile->m_vMaximum - m_pCollision->m_pModelFile->m_vMinimum;
	XClsModel::m_vScale = maximum * ( 1 / 65535.0f);
	m_pClsNode->CollidePoint(minimum, maximum);
	if (XClsModel::T >= 1.0f)
		return;
	XCollision::m_pModel = this;
	XCollision::m_vNormal.x =  D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._11);
	XCollision::m_vNormal.y =  D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._21);
	XCollision::m_vNormal.z =  D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._31);

	XCollision::SetClsPoint(XClsModel::T);
}

void CStaticModel::CollideRect()
{
	if (!m_pCollision || !TestRect())
		return;
	XClsModel::m_pModel = this;
	XClsModel::T = 1.0f;

	// XClsModel::m_vCenter = vCenter X Inverse(m_matrix)
	D3DXVECTOR3 vCenter = XCollision::m_vCenter - *(D3DXVECTOR3 *)&m_matrix._41;
	D3DXVec3TransformNormal(&XClsModel::m_vCenter, &vCenter, &m_inverse);
	XClsModel::m_vCenter -= m_pCollision->m_pModelFile->m_vMinimum;
	// XClsModel::V = XCollision::V X Inverse(m_matrix)
	D3DXVec3TransformNormal(&XClsModel::V, &XCollision::V, &m_inverse);
	XClsModel::C = XClsModel::m_vCenter + XClsModel::V;
	XClsModel::W = XClsModel::V * 2;
	D3DXVec3TransformNormal(&XClsModel::A0, &XCollision::A0, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A1, &XCollision::A1, &m_inverse);
	D3DXVec3Cross(&XClsModel::A0xA1, &XClsModel::A0, &XClsModel::A1);
	XClsModel::A0xA1_W = D3DXVec3Dot(&XClsModel::A0xA1, &XClsModel::W);
	if (NEGATIVE(XClsModel::A0xA1_W)) {
		CHGSIGN(XClsModel::A0xA1_W);
		CHGSIGN(XClsModel::A0xA1.x);
		CHGSIGN(XClsModel::A0xA1.y);
		CHGSIGN(XClsModel::A0xA1.z);
	}
	XClsModel::E.x = fabsf(XClsModel::A0.x) + fabsf(XClsModel::A1.x) + fabsf(XClsModel::V.x);
	XClsModel::E.y = fabsf(XClsModel::A0.y)	+ fabsf(XClsModel::A1.y) + fabsf(XClsModel::V.y);
	XClsModel::E.z = fabsf(XClsModel::A0.z)	+ fabsf(XClsModel::A1.z) + fabsf(XClsModel::V.z);
	D3DXVECTOR3 cross0, cross1;
	D3DXVec3Cross(&cross0, &XClsModel::A0, &XClsModel::V);
	D3DXVec3Cross(&cross1, &XClsModel::A1, &XClsModel::V);
	XClsModel::Cross.x = fabsf(cross0.x) + fabsf(cross1.x);  
	XClsModel::Cross.y = fabsf(cross0.y) + fabsf(cross1.y);  
	XClsModel::Cross.z = fabsf(cross0.z) + fabsf(cross1.z);  
	XClsModel::CrossX2 = XClsModel::Cross * 2.0f;
	XClsModel::CX2 = XClsModel::C * 2.0f;
	XClsModel::EX2 = XClsModel::E * 2.0f;
	XClsModel::m_pClsVertex = m_pClsVertex;
	XClsModel::m_pClsIndex = m_pClsIndex;
	XClsModel::m_pClsNode = m_pClsNode;
	D3DXVECTOR3 minimum(0, 0, 0);
	D3DXVECTOR3 maximum = m_pCollision->m_pModelFile->m_vMaximum - m_pCollision->m_pModelFile->m_vMinimum;
	XClsModel::m_vScale = maximum * ( 1 / 65535.0f);
	m_pClsNode->CollideRect(minimum, maximum);
	if (XClsModel::T >= 1.0f)
		return;
	XCollision::m_pModel = this;
	XCollision::m_vNormal.x = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._11);
	XCollision::m_vNormal.y = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._21);
	XCollision::m_vNormal.z = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._31);
	XCollision::D = D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._41, &XCollision::m_vNormal) 
		+ D3DXVec3Dot(&m_pCollision->m_pModelFile->m_vMinimum, &XClsModel::m_vNormal) + XClsModel::D;
	XCollision::SetClsRect(XClsModel::T);
}

#if	!defined(TERRAIN_EDITOR)
void CStaticModel::RenderFlagShadow(int flag)
{
	if (m_nFrameNumber != XSystem::m_nFrameNumber)
		return;

	if (flag > 0) {
		flag = XSystem::IsBoxInShadow(m_vCenter, m_vEdge, m_matrix, flag);
		if (flag < 0)
			return;
	}
	XSystem::DrawShadow(this);
}
#endif

#if	!defined(TERRAIN_EDITOR)
void CStaticModel::DrawDecal(int nFlags)
{
	if (m_nFrameNumber != XSystem::m_nFrameNumber || !m_pCollision)
		return;
	if (nFlags && (nFlags = TestDecal(nFlags)) < 0)
		return;
	D3DXVECTOR3 vCenter = XCollision::C - *(D3DXVECTOR3 *)&m_matrix._41;
	D3DXVec3TransformNormal(&XClsModel::C, &vCenter, &m_inverse);
	XClsModel::C -= m_pCollision->m_pModelFile->m_vMinimum;
	D3DXVec3TransformNormal(&XClsModel::A0, &XCollision::A0, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A1, &XCollision::A1, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A2, &XCollision::A2, &m_inverse);

	D3DXVec3Cross(&XClsModel::A0xA1, &XClsModel::A0, &XClsModel::A1);
	XClsModel::ABS_A0xA1.x = fabsf(XClsModel::A0xA1.x);
	XClsModel::ABS_A0xA1.y = fabsf(XClsModel::A0xA1.y);
	XClsModel::ABS_A0xA1.z = fabsf(XClsModel::A0xA1.z);
	D3DXVec3Cross(&XClsModel::A1xA2, &XClsModel::A1, &XClsModel::A2);
	XClsModel::ABS_A1xA2.x = fabsf(XClsModel::A1xA2.x);
	XClsModel::ABS_A1xA2.y = fabsf(XClsModel::A1xA2.y);
	XClsModel::ABS_A1xA2.z = fabsf(XClsModel::A1xA2.z);
	D3DXVec3Cross(&XClsModel::A2xA0, &XClsModel::A2, &XClsModel::A0);
	XClsModel::ABS_A2xA0.x = fabsf(XClsModel::A2xA0.x);
	XClsModel::ABS_A2xA0.y = fabsf(XClsModel::A2xA0.y);
	XClsModel::ABS_A2xA0.z = fabsf(XClsModel::A2xA0.z);
	XClsModel::ABS_A0xA1_A2 = D3DXVec3Dot(&XClsModel::A1xA2, &XClsModel::A0);
	if (XClsModel::ABS_A0xA1_A2 < 0) {
		XClsModel::ABS_A0xA1_A2 = - XClsModel::ABS_A0xA1_A2;
		nFlags = ((nFlags & 0x2A) >> 1) | ((nFlags & 0x15) << 1);
	}
	XClsModel::CX2 = XClsModel::C * 2.0f;
	XClsModel::ABS_A0xA1_A2X2 = XClsModel::ABS_A0xA1_A2 * 2.0f;
	XClsModel::m_pClsVertex = m_pClsVertex;
	XClsModel::m_pClsIndex = m_pClsIndex;
	XClsModel::m_pClsNode = m_pClsNode;
	D3DXVECTOR3 minimum(0, 0, 0);
	D3DXVECTOR3 maximum = m_pCollision->m_pModelFile->m_vMaximum - m_pCollision->m_pModelFile->m_vMinimum;
	XClsModel::m_vScale = maximum * ( 1 / 65535.0f);
	XClsModel::V.x = D3DXVec3Dot(&XCollision::A2, (D3DXVECTOR3 *) &m_matrix._11) * XClsModel::m_vScale.x;
	XClsModel::V.y = D3DXVec3Dot(&XCollision::A2, (D3DXVECTOR3 *) &m_matrix._21) * XClsModel::m_vScale.y;
	XClsModel::V.z = D3DXVec3Dot(&XCollision::A2, (D3DXVECTOR3 *) &m_matrix._31) * XClsModel::m_vScale.z;
	D3DXVECTOR3 vCamera = XSystem::m_vCamera - *(D3DXVECTOR3 *)&m_matrix._41;
	D3DXVec3TransformNormal(&vCamera, &vCamera, &m_inverse);
	vCamera -= m_pCollision->m_pModelFile->m_vMinimum;
	XClsModel::m_vCamera.x = vCamera.x / XClsModel::m_vScale.x;
	XClsModel::m_vCamera.y = vCamera.y / XClsModel::m_vScale.y;
	XClsModel::m_vCamera.z = vCamera.z / XClsModel::m_vScale.z;
	XClsModel::m_vMatrix = m_matrix;
	D3DXVECTOR3 vOut;
	D3DXVec3TransformNormal(&vOut, &m_pCollision->m_pModelFile->m_vMinimum, &XClsModel::m_vMatrix);
	*(D3DXVECTOR3 *)&XClsModel::m_vMatrix._41 += vOut;
	*(D3DXVECTOR3 *)&XClsModel::m_vMatrix._11 *= XClsModel::m_vScale.x;
	*(D3DXVECTOR3 *)&XClsModel::m_vMatrix._21 *= XClsModel::m_vScale.y;
	*(D3DXVECTOR3 *)&XClsModel::m_vMatrix._31 *= XClsModel::m_vScale.z;
	m_pClsNode->DrawDecal(minimum, maximum, nFlags);
}
#endif

#if 0
void CStaticModel::IntersectBox()
{
	if (!m_pCollision || !TestBox())
		return;
	if (XCollision::T == 0)
		return;
	XClsModel::m_pModel = this;

	// XClsModel::m_vCenter = vCenter X Inverse(m_matrix)
	D3DXVECTOR3 vCenter = XCollision::C - *(D3DXVECTOR3 *)&m_matrix._41;
	D3DXVec3TransformNormal(&XClsModel::C, &vCenter, &m_inverse);
	XClsModel::C -= m_pCollision->m_header.minimum;
	D3DXVec3TransformNormal(&XClsModel::A0, &XCollision::A0, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A1, &XCollision::A1, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A2, &XCollision::A2, &m_inverse);
	D3DXVec3Cross(&XClsModel::A0xA1, &XClsModel::A0, &XClsModel::A1);
	D3DXVec3Cross(&XClsModel::A1xA2, &XClsModel::A1, &XClsModel::A2);
	D3DXVec3Cross(&XClsModel::A2xA0, &XClsModel::A2, &XClsModel::A0);
	XClsModel::A0xA1_A2 = fabsf(D3DXVec3Dot(&XClsModel::A0xA1, &XClsModel::A2));
	XClsModel::E.x = fabsf(XClsModel::A0.x) + fabsf(XClsModel::A1.x) + fabsf(XClsModel::A2.x);
	XClsModel::E.y = fabsf(XClsModel::A0.y)	+ fabsf(XClsModel::A1.y) + fabsf(XClsModel::A2.y);
	XClsModel::E.z = fabsf(XClsModel::A0.z)	+ fabsf(XClsModel::A1.z) + fabsf(XClsModel::A2.z);
	XClsModel::CX2 = XClsModel::C * 2.0f;
	XClsModel::EX2 = XClsModel::E * 2.0f;
	XClsModel::m_pClsVertex = m_pClsVertex;
	XClsModel::m_pClsIndex = m_pClsIndex;
	XClsModel::m_pClsNode = m_pClsNode;
	D3DXVECTOR3 minimum(0, 0, 0);
	D3DXVECTOR3 maximum = m_pCollision->m_header.maximum - m_pCollision->m_header.minimum;
	XClsModel::m_vScale = maximum * ( 1 / 65535.0f);
	m_pClsNode->IntersectBox(minimum, maximum);
	if (XCollision::T > 0)
		return;
	XCollision::m_pModel = this;
	XCollision::m_vNormal.x = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._11);
	XCollision::m_vNormal.y = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._21);
	XCollision::m_vNormal.z = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._31);
	XCollision::D = D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._41, &XCollision::m_vNormal) 
		+ D3DXVec3Dot(&m_pCollision->m_header.minimum, &XClsModel::m_vNormal) + XClsModel::D;
}
#endif

void CStaticModel::CollideBox()
{
    // 충돌체크 가능 상태인지 체크
	if (!m_pCollision || !TestBox())
		return;

    // 충돌 변수 T가 0이라면 충돌 체크 취소
	if (XCollision::T == 0)
		return;

    // 임수 변수 설정
	float t = XCollision::T;
    // 현재 모델을 충돌 체크 모델로 설정
	XClsModel::m_pModel = this;

    //////////////////////////////////////////////////////////////////////////
    // 오브젝트 축으로 이동 OBB 박스 충돌 연산
    //////////////////////////////////////////////////////////////////////////
    
	// XClsModel::m_vCenter = vCenter X Inverse(m_matrix)

    // 반지름 계산
	D3DXVECTOR3 vCenter = XCollision::m_vCenter - *(D3DXVECTOR3 *)&m_matrix._41;

    // 반지름을 충돌 객체의 로컬 좌표로 변환
	D3DXVec3TransformNormal(&XClsModel::m_vCenter, &vCenter, &m_inverse);

    // 로컬 반지름에서 현재 컬리전의 최소 크기값을 뺀다.
	XClsModel::m_vCenter -= m_pCollision->m_pModelFile->m_vMinimum;
	// XClsModel::V = XCollision::V X Inverse(m_matrix)

    // 반지름 이동량을 로컬 좌표로 변환
	D3DXVec3TransformNormal(&XClsModel::V, &XCollision::V, &m_inverse);

    // 충돌 영역과 바지름 이동량을 더하고 총 지름을 계산
	XClsModel::C = XClsModel::m_vCenter + XClsModel::V;
	XClsModel::W = XClsModel::V * 2;

    // A0, A1, A2를 로컬 변환
	D3DXVec3TransformNormal(&XClsModel::A0, &XCollision::A0, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A1, &XCollision::A1, &m_inverse);
	D3DXVec3TransformNormal(&XClsModel::A2, &XCollision::A2, &m_inverse);

    // 각각의 성분을 외적을 통해 법선 벡터를 구한다.
	D3DXVec3Cross(&XClsModel::A0xA1, &XClsModel::A0, &XClsModel::A1);
	D3DXVec3Cross(&XClsModel::A1xA2, &XClsModel::A1, &XClsModel::A2);
	D3DXVec3Cross(&XClsModel::A2xA0, &XClsModel::A2, &XClsModel::A0);

    // 
	XClsModel::ABS_A0xA1_A2 = fabsf(D3DXVec3Dot(&XClsModel::A0xA1, &XClsModel::A2));

	XClsModel::A0xA1_W = D3DXVec3Dot(&XClsModel::A0xA1, &XClsModel::W);
	if (NEGATIVE(XClsModel::A0xA1_W)) {
		CHGSIGN(XClsModel::A0xA1_W);
		CHGSIGN(XClsModel::A0xA1.x);
		CHGSIGN(XClsModel::A0xA1.y);
		CHGSIGN(XClsModel::A0xA1.z);
	}
	XClsModel::A1xA2_W = D3DXVec3Dot(&XClsModel::A1xA2, &XClsModel::W);
	if (NEGATIVE(XClsModel::A1xA2_W)) {
		CHGSIGN(XClsModel::A1xA2_W);
		CHGSIGN(XClsModel::A1xA2.x);
		CHGSIGN(XClsModel::A1xA2.y);
		CHGSIGN(XClsModel::A1xA2.z);
	}
	XClsModel::A2xA0_W = D3DXVec3Dot(&XClsModel::A2xA0, &XClsModel::W);
	if (NEGATIVE(XClsModel::A2xA0_W)) {
		CHGSIGN(XClsModel::A2xA0_W);
		CHGSIGN(XClsModel::A2xA0.x);
		CHGSIGN(XClsModel::A2xA0.y);
		CHGSIGN(XClsModel::A2xA0.z);
	}
	XClsModel::E.x = fabsf(XClsModel::A0.x) + fabsf(XClsModel::A1.x) + fabsf(XClsModel::A2.x) + fabsf(XClsModel::V.x);
	XClsModel::E.y = fabsf(XClsModel::A0.y)	+ fabsf(XClsModel::A1.y) + fabsf(XClsModel::A2.y) + fabsf(XClsModel::V.y);
	XClsModel::E.z = fabsf(XClsModel::A0.z)	+ fabsf(XClsModel::A1.z) + fabsf(XClsModel::A2.z) + fabsf(XClsModel::V.z);
	XClsModel::CX2 = XClsModel::C * 2.0f;
	XClsModel::EX2 = XClsModel::E * 2.0f;
	XClsModel::m_pClsVertex = m_pClsVertex;
	XClsModel::m_pClsIndex = m_pClsIndex;
	XClsModel::m_pClsNode = m_pClsNode;

    //////////////////////////////////////////////////////////////////////////
    // 충돌 체크
    //////////////////////////////////////////////////////////////////////////

	D3DXVECTOR3 minimum(0, 0, 0);
	D3DXVECTOR3 maximum = m_pCollision->m_pModelFile->m_vMaximum - m_pCollision->m_pModelFile->m_vMinimum;
	XClsModel::m_vScale = maximum * ( 1 / 65535.0f);

	m_pClsNode->CollideBox(minimum, maximum);


    // 충돌이 일어나지 않은 상태
	if (XCollision::T >= t)
		return;

    //////////////////////////////////////////////////////////////////////////
    // 충돌 후 처리?
    //////////////////////////////////////////////////////////////////////////
	XCollision::m_pModel = this;
	XCollision::m_vNormal.x = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._11);
	XCollision::m_vNormal.y = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._21);
	XCollision::m_vNormal.z = D3DXVec3Dot(&XClsModel::m_vNormal, (D3DXVECTOR3 *) &m_inverse._31);
	XCollision::D = D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._41, &XCollision::m_vNormal) 
		+ D3DXVec3Dot(&m_pCollision->m_pModelFile->m_vMinimum, &XClsModel::m_vNormal) + XClsModel::D;
}
#endif //GAME_XENGINE

CModel::CModel()
{
	m_pBone = 0;
	m_nBoneData = 0;
	m_nAnim = 0;
	m_fRate = 1.0;
	m_ppAnimData = 0;
	m_nBlend = 0;
//	m_bLoad = FALSE;
    m_defaultAnimationNumber = 0;
	memset(m_bDirection, 0, MODEL_MAX_BONE);	
}

void CModel::Reset()
{
	if (m_pModelFile) {
		m_event.RemoveEvent();
		m_pModelFile->Release();
		m_pModelFile = 0;
	}
	if (m_pBone) {
		m_pBone->Release();
		m_pBone = 0;
		m_nBoneData = 0;
	}
	for (int i = 0; i < m_nAnim; i++) 
		m_vpAnim[i]->Release();
	if (m_pCollision) {
		m_pCollision->Release();
		m_pCollision = 0;
	}

	m_nAnim = 0;
	m_nBlend = 0;
//	m_bLoad = FALSE;
}

void CModel::OnLoad()
{
	if (!m_pModelFile) 
		return;

	m_pModelFile->AddRefAll();

	if (m_pModelFile->m_pBone) {
		ASSERT(!m_pBone);
		m_pBone = m_pModelFile->m_pBone;
		m_nBoneData = m_pBone->m_nBoneData;
		m_pBoneData = m_pBone->m_pBoneData;
	}
#if defined(_DEBUG)
	else {
		ASSERT(m_pModelFile->m_nBoneCount == m_nBoneData);
	}
#endif
	
	ASSERT(m_nMesh + m_pModelFile->m_nMesh < MODEL_MAX_MESH);
	m_dwFlags &= ~F_CONTAIN_TRANSPARENT;
	for (int i = 0; i < m_pModelFile->m_nMesh; i++) {
		CMesh *pMesh = &m_pModelFile->m_pMesh[i];
		m_vpMesh[m_nMesh++] = pMesh;
		if( m_pModelFile->m_pMesh[i].m_pMatData->m_mapoption & (MATERIAL_MAP_OPACITY | MATERIAL_MAP_ALPHA_RGB))
			m_dwFlags |= F_CONTAIN_TRANSPARENT;
	}

	ASSERT(m_nAnim + m_pModelFile->m_nAnim < MODEL_MAX_ANIM);
	for (int i = 0; i < m_pModelFile->m_nAnim; i++) {
		m_vpAnim[m_nAnim++] = &m_pModelFile->m_pAnim[i];
	}
#ifdef GAME_XENGINE
	if (m_pModelFile->m_pCollision) {
		ASSERT(!m_pCollision);
		m_pCollision = m_pModelFile->m_pCollision;
		m_pClsVertex = (WORD *)(m_pCollision + 1);
		m_pClsIndex = m_pClsVertex + m_pCollision->m_header.vertex_count * 3;
		m_pClsNode = (CClsNode *)(m_pClsIndex + m_pCollision->m_header.face_count * 3);
	}
#endif
	m_szOption = m_pModelFile->m_szOption;
//	m_bLoad = TRUE;

}

void CModel::FreeAnim(int nAnim)
{
	if ( m_nAnim > nAnim )
	{
		CAnim	*pAnim = m_vpAnim[nAnim];
		if (m_ppAnimData >= pAnim->m_ppAnimData && m_ppAnimData < pAnim->m_ppAnimData + pAnim->m_nKeyFrame * m_nBoneData) {
			m_ppAnimData = 0;
		}
		pAnim->Release();
		memmove(&m_vpAnim[nAnim], &m_vpAnim[nAnim+1], (--m_nAnim - nAnim)*sizeof(CAnim*));
	}
}

void CModel::ProcessTransform()
{
	if ( !m_ppAnimData || !m_pBoneData || !m_pBone || !m_nAnim )
	{
		BREAK();
		ASSERT(m_ppAnimData);
		ASSERT(m_pBoneData);
		memset(s_vMatrix, 0, sizeof(s_vMatrix));
		return;
	}

	m_dwFlags |= F_BLENDABLE;

    // 현재 키 시간과 과 다음 키 시간과의 나눔으로 현재 이동 시간비 t를 구할 수 있다.
	float t = (float) (m_dwTime - m_pKeyFrame->m_nTime) / (float) (m_pKeyFrame[1].m_nTime - m_pKeyFrame->m_nTime);

#ifndef _SOCKET_TEST
    //sea debug 상태에서 속도를 조정 할수 있다. 
	t = (float) (m_dwTime - m_pKeyFrame->m_nTime*m_fRate) / (float) (m_pKeyFrame[1].m_nTime*m_fRate - m_pKeyFrame->m_nTime*m_fRate);
#endif

	GB_ANIM **ppAnimData = m_ppAnimData;
	GB_ANIM **ppNextData = ppAnimData + m_nBoneData;    
	GB_ANIM	*pCurr = m_vAnimData;
	GB_ANIM	*pEnd = m_vAnimData + m_nBoneData;
       
#ifdef DEF_SCENARIO_3_3_OEN_080114
    // 애니매이션과의 보간
    // 마지막 애니매이션 처리 후 다음 애니매이션 데이터가 존재하지 않는데 보간 하므로
    // 이전에 쓰인 임의의 데이터를 사용하므로 동일한 애니 보간이 적용 되었던 부분을 수정
    #ifdef DEF_EFFECT_ANI_MODEL_OEN_080520
        if (m_nKeyFrame && m_vpAnim[0]->m_nKeyFrame >= 2)
    #else
        if (m_nKeyFrame && m_vpAnim[0]->m_nKeyFrame > 2)
    #endif
#endif
    {
	    for (; pCurr != pEnd; pCurr++) 
	    {
            GB_ANIM *pAnimData = *ppAnimData;
            GB_ANIM *pNextData = *ppNextData;            
		    D3DXQuaternionSlerp(&pCurr->quat, &pAnimData->quat, &pNextData->quat, t);
		    D3DXVec3Lerp(&pCurr->pos, &pAnimData->pos, &pNextData->pos, t);
		    D3DXVec3Lerp(&pCurr->scale, &pAnimData->scale, &pNextData->scale, t);
		    ppAnimData++;
		    ppNextData++;
	    }
    }
#ifdef DEF_SCENARIO_3_3_OEN_080114
    else
    {
        for ( ; pCurr != pEnd; pCurr++)
        {
            GB_ANIM *pAnimData = *ppAnimData;
            GB_ANIM *pNextData = *ppAnimData;
            D3DXQuaternionSlerp(&pCurr->quat, &pAnimData->quat, &pNextData->quat, 1);
            D3DXVec3Lerp(&pCurr->pos, &pAnimData->pos, &pNextData->pos, 1);
            D3DXVec3Lerp(&pCurr->scale, &pAnimData->scale, &pNextData->scale, 1);
            ppAnimData++;
            ppNextData++;
        }
    }
#endif

    // 버텍스블렌딩 처리? 애니매이션과의 보간
	if (m_dwTime < m_nBlend) 
	{
		t = (float) m_dwTime / m_nBlend;

        // m_vAnimData 의 앞부분에는 현재 애니매이션 정보가 뒷쪽엔 원본 애니매이션 정보가 저장되어 있다.
        // pPrev는 원본 데이터를 가르킨다.
		GB_ANIM	*pPrev = m_vAnimData + MODEL_MAX_BONE;

		for (pCurr = m_vAnimData; pCurr != pEnd; pCurr++) 
		{
			D3DXQuaternionSlerp(&pCurr->quat, &pPrev->quat, &pCurr->quat, t);
			D3DXVec3Lerp(&pCurr->pos, &pPrev->pos, &pCurr->pos, t);
			D3DXVec3Lerp(&pCurr->scale, &pPrev->scale, &pCurr->scale, t);
			pPrev++;
		}
	}

    // 전역 행렬에 주소를 얻어 온다!!!
	D3DXMATRIX *pMatrix = s_vMatrix;

	CBoneData	*pBoneData = m_pBoneData;
	BYTE *pDirection = m_bDirection;
	BYTE iDataNo=0;

    // 보간한 데이터로 부터 실제 본 매트릭스 행렬의 값을 구한다.
	for (pCurr = m_vAnimData ; pCurr != pEnd; pCurr++, iDataNo++) 
	{ 
		// 행렬로 변환
		D3DXMATRIXA16 matrix;
       
		D3DXMatrixRotationQuaternion(&matrix, &pCurr->quat);

		*(D3DXVECTOR3 *)&matrix._11 *= pCurr->scale.x;
		*(D3DXVECTOR3 *)&matrix._21 *= pCurr->scale.y;
		*(D3DXVECTOR3 *)&matrix._31 *= pCurr->scale.z;        
		*(D3DXVECTOR3 *)&matrix._41 = pCurr->pos;

		if (pBoneData->m_pmParent)
			D3DXMatrixMultiply(pMatrix, &matrix, pBoneData->m_pmParent);
		else
			D3DXMatrixMultiply(pMatrix, &matrix, &m_matrix);

        // CGameTarget::ProcessDirection 가상함수 호출 (데미지시 본 회전이나 공격할때 본 회전을 처리한다)
		if (*pDirection)
			ProcessDirection(iDataNo);

		pMatrix++;
		pBoneData++;
		pDirection++;
	}
}

void CModel::SetTransform()
{
	ASSERT(m_pBoneData);

	CBoneData *pBoneData = m_pBoneData;
	D3DXMATRIX *pEndMatrix = s_vMatrix + m_nBoneData;
	D3DXMATRIX *pMatrixWorld = s_vMatrixWorld;
	D3DXMATRIX *pMatrixTrans = s_vMatrixTrans;

	for (D3DXMATRIX *pMatrix = s_vMatrix; pMatrix != pEndMatrix;) 
    {
		D3DXMatrixMultiply(pMatrixWorld, &pBoneData->m_mInverse, pMatrix);
		D3DXMatrixMultiplyTranspose(pMatrixTrans, pMatrixWorld, &XSystem::m_mView);
		pMatrix++;
		pMatrixWorld++;
		pMatrixTrans++;
		pBoneData++;
	}

	XSystem::m_pDevice->SetVertexShaderConstantF(0, (float *)D3DXVECTOR4( 1.0f, 0.0f, 0.0f, 765.01f ), 1);
	XSystem::m_pDevice->SetVertexShaderConstantF(1, (float *)D3DXVECTOR4(-XSystem::m_vLightView.x, -XSystem::m_vLightView.y, -XSystem::m_vLightView.z, 0), 1);
	XSystem::m_pDevice->SetVertexShaderConstantF(2, (float *)XSystem::m_mProjTrans, 4);
	XSystem::m_pDevice->SetVertexShaderConstantF(6, (float *)&XSystem::m_material.Ambient, 1);
	XSystem::m_pDevice->SetVertexShaderConstantF(7, (float *)&XSystem::m_material.Diffuse, 1);
	XSystem::m_pDevice->SetVertexShaderConstantF(8, (float *)&XSystem::m_material.Specular, 1);

#ifdef GAME
	XSystem::m_pDevice->SetVertexShaderConstantF(9, (float *)D3DXVECTOR4(-XSystem::m_fogDensity * XSystem::m_fogDensity / 0.693147f, 0, 0, 0), 1);
#endif

}

inline void ColorLerp
		(D3DCOLOR *pOut, const D3DCOLOR *pC1, const D3DCOLOR *pC2, float s)
	{
	#ifdef _DEBUG
		if(!pOut || !pC1 || !pC2)
		{
			ASSERT(0);
			return;
		}
	#endif
		D3DXCOLOR xC1(*pC1);
		D3DXCOLOR xC2(*pC2);
		D3DXCOLOR xOut;
		D3DXColorLerp(&xOut, &xC1, &xC2, s);
		*pOut = D3DCOLOR_COLORVALUE(xOut.r, xOut.g, xOut.b, xOut.a);
	}

void CModel::MaterialMove(CMesh *pMesh)
{
	if ( pMesh->m_pMaterial->m_pKeyFrame )
	{
		int iSeq = m_vpAnim[0]->m_nKeyFrame-m_nKeyFrame-1;
		ASSERT( pMesh->m_pMaterial->m_nMatrialFrameCount > iSeq+1);

		CKeyFrame *pKeyFrame = pMesh->m_pMaterial->m_pKeyFrame+iSeq;

		float t = (float) (m_dwTime - pKeyFrame->m_nTime) / (float) (pKeyFrame[1].m_nTime - pKeyFrame->m_nTime);

#ifndef _SOCKET_TEST	//sea debug 상태에서 속도를 조정 할수 있다
		t = (float) (m_dwTime - pKeyFrame->m_nTime*m_fRate) / (float) (pKeyFrame[1].m_nTime*m_fRate - pKeyFrame->m_nTime*m_fRate);
#endif
		
		GB_MATERIAL_FRAME * pframe = pMesh->m_pMatData->m_pframe + iSeq ;

		ColorLerp(&pMesh->m_pMatData->m_ambient, &pframe->m_ambient,  &pframe[1].m_ambient, t);
		ColorLerp(&pMesh->m_pMatData->m_diffuse, &pframe->m_diffuse,  &pframe[1].m_diffuse, t);
		ColorLerp(&pMesh->m_pMatData->m_specular, &pframe->m_specular,  &pframe[1].m_specular, t);

		if (g_bOverBright)
		{
			pMesh->m_pMatData->m_ambient = ((pMesh->m_pMatData->m_ambient & 0x00FEFEFE) >> 1) | (pMesh->m_pMatData->m_ambient & 0xFF000000);
			pMesh->m_pMatData->m_diffuse = ((pMesh->m_pMatData->m_ambient & 0x00FEFEFE) >> 1) | (pMesh->m_pMatData->m_ambient & 0xFF000000);
			pMesh->m_pMatData->m_specular = ((pMesh->m_pMatData->m_ambient & 0x00FEFEFE) >> 1) | (pMesh->m_pMatData->m_ambient & 0xFF000000);
		}

		pMesh->m_pMatData->m_opacity = pframe->m_opacity + t * (pframe[1].m_opacity-pframe->m_opacity);
		D3DXVec2Lerp(&pMesh->m_pMatData->m_offset,&pframe->m_offset, &pframe[1].m_offset, t);
		D3DXVec3Lerp(&pMesh->m_pMatData->m_angle,&pframe->m_angle, &pframe[1].m_angle, t);
		if ( pMesh->m_pMatData->m_opacity < 1.0f )
			pMesh->m_pMatData->m_mapoption |= MATERIAL_MAP_OPACITY;
		//else
		//	pMesh->m_pMatData->m_mapoption &= ~MATERIAL_MAP_OPACITY;

		if ( pMesh->m_pMatData->m_offset.x || pMesh->m_pMatData->m_offset.y )
			pMesh->m_pMatData->m_mapoption |= MATERIALEX_TEXTURE;
		else
			pMesh->m_pMatData->m_mapoption &= ~MATERIALEX_TEXTURE;

		if ( (pMesh->m_pMatData->m_specular & 0xff00) == 0 ) // green
			pMesh->m_pMatData->m_mapoption |= MATERIAL_LIGHT;
		else
			pMesh->m_pMatData->m_mapoption &= ~MATERIAL_LIGHT;

		if ( (pMesh->m_pMatData->m_specular & 0xff0000) == 0 ) // red
		{
			pMesh->m_pMatData->m_mapoption |= MATERIAL_SPECULAR;
			pMesh->m_pMatData->m_fPower = 40.0f;
		}
		else
			pMesh->m_pMatData->m_mapoption &= ~MATERIAL_SPECULAR;		
	}
}

void CModel::Render()
{
//	if ( m_bLoad == FALSE )
//		return;

    if (!m_ppAnimData)
    	return;

#if !defined(TERRAIN_EDITOR)
	if (MMap::IsLoaded())
		OnPreRender();
#endif

	m_nFrameNumber = XSystem::m_nFrameNumber;
	s_pModel = this;

    // 애니매이션 정보를 보간 이동 시킨다.
    ProcessTransform();
	PreRender();
	PreProcessOption();
	SetTransform();

	for (int i = 0; i < m_nMesh; i++) 
    {
		CMesh *pMesh = m_vpMesh[i];
		if ( pMesh->m_pMaterial->m_pKeyFrame ) MaterialMove(pMesh);
		pMesh->Render();
	}

	XSystem::m_pDevice->SetVertexShader(0);
	XSystem::m_pDevice->SetRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_EXP2);

    PostProcessOption();

	s_pModel = 0;

#if !defined(TERRAIN_EDITOR)
	for (std::vector<particleInfo_t>::iterator it = m_prtInfoVector.begin(); it != m_prtInfoVector.end(); it++)
	{
		D3DXVECTOR3 t;
		D3DXVECTOR3 offset = it->offset;
		t.x = m_matrix._11 * offset.x + m_matrix._21 * offset.y + m_matrix._31 * offset.z + m_matrix._41;
		t.y = m_matrix._12 * offset.x + m_matrix._22 * offset.y + m_matrix._32 * offset.z + m_matrix._42;
		t.z = m_matrix._13 * offset.x + m_matrix._23 * offset.y + m_matrix._33 * offset.z + m_matrix._43;
		m_prtBundle.AddParticle(this, it->name.c_str(), t, m_matrix);
	}
#endif


}

void CModel::RenderShadow()
{
//	if ( m_bLoad == FALSE )	return;
	if (!m_ppAnimData)
		return;
	ProcessTransform();
	PreRenderShadow();

	SetTransform();
	{
		for (int i = 0; i < m_nMesh; i++) {
			CMesh *pMesh = m_vpMesh[i];
			pMesh->RenderShadow();
		}
		XSystem::m_pDevice->SetVertexShader(0);

	}
	//XSystem::m_pDevice->SetVertexDeclaration(0);
}

#ifdef DEF_SCENARIO_3_3_OEN_080114
void CModel::PlayAnimation(int nAnim, int nBlend, int key_)
#else
void CModel::PlayAnimation(int nAnim, int nBlend)
#endif
{
    // 본이 없거나 출력 애니 가 총 애니수보다 많을 때
	if (!m_pBone || nAnim >= m_nAnim)	
		return;

    //ASSERT(m_ppAnimData || nBlend == 0);
    // 블랜드 설정
	if (m_dwFlags & F_BLENDABLE) 
    {
		m_nBlend = nBlend;
		if (nBlend) // 현재 애니메이션 데이터 저장
        {
            //memset(m_vAnimData + MODEL_MAX_BONE, 0, sizeof(GB_ANIM) * MODEL_MAX_BONE);
			memcpy(m_vAnimData + MODEL_MAX_BONE, m_vAnimData, m_nBoneData * sizeof(GB_ANIM));
        }
	}
	else
		m_nBlend = 0;

    // 애니 정보 습득
	CAnim *pAnim = m_vpAnim[nAnim];
	m_dwTime = 0;

    // 키 프레임 설정
	m_pKeyFrame = pAnim->m_pKeyFrame;

    // 애니 데이터 설정
	m_ppAnimData = pAnim->m_ppAnimData;

    // 키 프레임 애니 옵션 출력
	LPCSTR szOption = m_pKeyFrame->m_szOption;

    // 옵션이 존재하면 설정
	if (szOption[0])
    {
		OnAnimationOption(m_pKeyFrame);
    }

    // 키 프레임 개수 설정
	m_nKeyFrame = pAnim->m_nKeyFrame - 1;

#ifdef DEF_SCENARIO_3_3_OEN_080114
    if (key_)
    {
        int moveKey = m_nKeyFrame - key_;
        m_nKeyFrame = key_;
        m_pKeyFrame += moveKey;
        m_dwTime = m_pKeyFrame->m_nTime;
        for (int i = 0; i < moveKey; i++)
        {
            m_ppAnimData += m_nBoneData;
        }
    }
#endif
}

void CModel::FrameMove(DWORD dwTime)
{
#ifdef _DEBUG
	if (!m_nAnim || !m_nKeyFrame || m_ppAnimData == 0 || m_nKeyFrame == 0 || m_nBoneData == 0) 
        return;
#endif

	//ASSERT(m_ppAnimData); 
#ifndef _SOCKET_TEST	//sea debug 상태에서 속도를 조정 할수 있다
	if (m_ppAnimData == 0 || m_nKeyFrame == 0)
		return;
	m_dwTime += dwTime;
	for ( ; ; ) {
		CKeyFrame *pNextFrame = m_pKeyFrame + 1;
		if (m_dwTime < pNextFrame->m_nTime*m_fRate)
			break;
		LPCSTR szOption = pNextFrame->m_szOption;
		if (szOption[0])
			OnAnimationOption(pNextFrame);

#ifdef DEF_LOOP_OPT_OEN_080829
        // 다음 키가 끝키거나 끝나는 루프라면 종료한다.
        if ((--m_nKeyFrame == 0)  || ( m_vpAnim[0] && m_vpAnim[0]->m_iAniLoopEndNo != 0 && m_dwTime > (DWORD)m_vpAnim[0]->m_iAniLoopEndNo))
#else
		if (--m_nKeyFrame == 0) 
#endif
        {
			m_dwTime -= (DWORD)(pNextFrame->m_nTime*m_fRate);
			OnAnimationComplete();
			break;
		}

		m_pKeyFrame = pNextFrame;
		m_ppAnimData += m_nBoneData;
	}

#else
	if (m_ppAnimData == 0 || m_nKeyFrame == 0)
		return;

	m_dwTime += dwTime;

	for ( ; ; )
    {
        // 다음 키 프레임 을 얻는다.
		CKeyFrame *pNextFrame = m_pKeyFrame + 1;

        // 다음 키프레임의 시간이 총 재생 시간 보다 크다면 중지
		if (m_dwTime < pNextFrame->m_nTime)
			break;

        // 다음 프레임의 옵션을 읽는다.
		LPCSTR szOption = pNextFrame->m_szOption;

        // 옵션이 존재하면
		if (szOption[0])
			OnAnimationOption(pNextFrame);

        

#ifdef DEF_LOOP_OPT_OEN_080829
        // 다음 키가 끝키거나 끝나는 루프라면 종료한다.
        if ( ( --m_nKeyFrame == 0) ||
			( m_vpAnim[0] && m_vpAnim[0]->m_iAniLoopEndNo != 0 &&
			m_dwTime > (DWORD)( m_vpAnim[0]->m_iAniLoopEndNo ) ) )
#else
        // 다음 키 프레임이 없을 때 컴플리트를 실행해 의사를 결정한다
		if (--m_nKeyFrame == 0) 
#endif
        {
            // 누적 시간에서 현재 시간을 빼준다 그렇게 함으로써 처음으로 시간을 돌린다.
			m_dwTime -= pNextFrame->m_nTime;
            OnAnimationComplete();
			break;
		}
        
        // 다음 프레임으로 설정
		m_pKeyFrame = pNextFrame;
		m_ppAnimData += m_nBoneData;
	}
#endif
}

void CModel::PlayAnimationEx(int nAnim, DWORD dwSkip, DWORD dwTime)
{
//	if ( m_bLoad == FALSE )	return;
	//ASSERT(m_pBone);

#ifndef _SOCKET_TEST	//sea debug 상태에서 속도를 조정 할수 있다
	if (!m_pBone || nAnim >= m_nAnim )	
		return;
	m_nBlend = 0;
	m_dwTime = dwSkip;
	CAnim *pAnim = m_vpAnim[nAnim];
	m_pKeyFrame = pAnim->m_pKeyFrame;
	m_ppAnimData = pAnim->m_ppAnimData;
	m_nKeyFrame = pAnim->m_nKeyFrame - 1;
	for ( ; ; ) {
		CKeyFrame *pNextFrame = m_pKeyFrame + 1;
		if (m_dwTime < pNextFrame->m_nTime*m_fRate)
			break;
		if (--m_nKeyFrame == 0) {
			m_dwTime = (DWORD)(pNextFrame->m_nTime*m_fRate);
			m_nKeyFrame = 1;
			return;
		}
		m_pKeyFrame = pNextFrame;
		m_ppAnimData += m_nBoneData;
	}	
	if (m_pKeyFrame->m_nTime*m_fRate == m_dwTime)
	{
		LPCSTR szOption = m_pKeyFrame->m_szOption;
		if (szOption[0])
			OnAnimationOption(m_pKeyFrame);
	}
	m_dwTime += dwTime;
	for ( ; ; ) {
		CKeyFrame *pNextFrame = m_pKeyFrame + 1;
		if (m_dwTime < pNextFrame->m_nTime)
			break;
		LPCSTR szOption = pNextFrame->m_szOption;
		if (szOption[0])
			OnAnimationOption(pNextFrame);
		if (--m_nKeyFrame == 0) {
			m_dwTime = pNextFrame->m_nTime;
			m_nKeyFrame = 1;
			return;
		}
		m_pKeyFrame = pNextFrame;
		m_ppAnimData += m_nBoneData;
	}	
#else
	if (!m_pBone || nAnim >= m_nAnim )	
		return;
	m_nBlend = 0;
	m_dwTime = dwSkip;
	CAnim *pAnim = m_vpAnim[nAnim];
	m_pKeyFrame = pAnim->m_pKeyFrame;
	m_ppAnimData = pAnim->m_ppAnimData;
	m_nKeyFrame = pAnim->m_nKeyFrame - 1;
	for ( ; ; ) {
		CKeyFrame *pNextFrame = m_pKeyFrame + 1;
		if (m_dwTime < pNextFrame->m_nTime)
			break;
		if (--m_nKeyFrame == 0) {
			m_dwTime = pNextFrame->m_nTime;
			m_nKeyFrame = 1;
			return;
		}
		m_pKeyFrame = pNextFrame;
		m_ppAnimData += m_nBoneData;
	}
	if (m_pKeyFrame->m_nTime == m_dwTime)
	{
		LPCSTR szOption = m_pKeyFrame->m_szOption;
		if (szOption[0])
			OnAnimationOption(m_pKeyFrame);
	}
	m_dwTime += dwTime;
	for ( ; ; ) {
		CKeyFrame *pNextFrame = m_pKeyFrame + 1;
		if (m_dwTime < pNextFrame->m_nTime)
			break;
		LPCSTR szOption = pNextFrame->m_szOption;
		if (szOption[0])
			OnAnimationOption(pNextFrame);
		if (--m_nKeyFrame == 0) {
			m_dwTime = pNextFrame->m_nTime;
			m_nKeyFrame = 1;
			return;
		}
		m_pKeyFrame = pNextFrame;
		m_ppAnimData += m_nBoneData;
	}	
#endif
}

void CModel::OnAnimationComplete()
{
    // 기본 적인 애니매이션 설정!!!
	PlayAnimation(m_defaultAnimationNumber, 0);
}

void CModel::OnAnimationOption(CKeyFrame *pKeyFrame)
{
#if !defined( TERRAIN_EDITOR)
	char *str, *substr;

	str = pKeyFrame->m_szOption;
	while (str)
	{
		if (!strstr(str,"(*"))
			break;

		if (substr = strstr(str,"(*EFFECT_"))
		{
			str = substr + strlen("(*EFFECT_");

			if (!strncmp(str,"EXPLOSION",strlen("EXPLOSION")))
			{
				str = str + strlen("EXPLOSION");

				int iEffectNo = atoi(str);
				CEffect_Explosion::AddExplosion(iEffectNo, m_matrix, 0, 0);
			}
		}
	}
	
#endif
}

void CModel::ProcessDirection(BYTE iDataNo)
{
}

#ifdef GAME_XENGINE
void CModel::RenderFlag(int flag)
{
	m_nRenderNumber = XSystem::m_nRenderNumber;
	ASSERT(IsValidNode());
	if (flag > 0) {
		flag = XSystem::IsBoxIn(m_vCenter, m_vEdge, m_matrix, flag);
		if (flag < 0)
		{
			return;
		}
	}
	if (XEngine::m_nAction & XEngine::RENDER_WATER) {
		return;
	}
	//m_nFrameNumber = XSystem::m_nFrameNumber;	
	if( m_dwFlags & F_CONTAIN_TRANSPARENT)
		XSortTransparent::Insert( this);
	else {
		Render();
	}
}

#if	!defined(TERRAIN_EDITOR)
void	CModel::RenderFlagShadow(int flag)
{
	if (flag > 0) {
		flag = XSystem::IsBoxInShadow(m_vCenter, m_vEdge, m_matrix, flag);
		if (flag < 0)
			return;
	}
	if (m_nFrameNumber != XSystem::m_nFrameNumber)
		return;
	XSystem::DrawShadow(this);
}
#endif

BOOL CModel::InitModel(const D3DXVECTOR3 &vCenter, float fScale)
{
	if (m_pCollision) {
		m_vCenterOffset = 0.5f * (m_pCollision->m_pModelFile->m_vMaximum + m_pCollision->m_pModelFile->m_vMinimum);
		m_vEdge = m_pCollision->m_pModelFile->m_vMaximum - m_vCenterOffset;
		SetCenter(vCenter + m_vCenterOffset * fScale);
	}
	else {
		m_vCenterOffset = D3DXVECTOR3(0, 0, 0);
		m_vEdge = D3DXVECTOR3(0.5f * METER, METER, 0.5f * METER);
		SetCenter(vCenter);
	}
	m_vExtent.x = m_vExtent.z = sqrtf(m_vEdge.x * m_vEdge.x + m_vEdge.z * m_vEdge.z);
	m_vExtent.y = m_vEdge.y;
	m_vExtent *= fScale;
	m_fRadius = fScale * D3DXVec3Length(&m_vEdge);
	XEngine::InsertObject(this);
	return TRUE;
}

void CModel::OnLoadModel()
{
	if (!m_pModelFile)
		return;

    // 모델을 로딩 한 후 모델 데이터를 자신에게 삽입한다.
	OnLoad();

    // 충돌 박스 영역을 생성한다.
	m_vCenterOffset = 0.5f * (m_pModelFile->m_vMaximum + m_pModelFile->m_vMinimum);
	m_vEdge = m_pModelFile->m_vMaximum - m_vCenterOffset;
	D3DXVECTOR3 vCenter;
	D3DXVec3TransformNormal(&vCenter, &m_vCenterOffset, &m_matrix);
	SetCenter(*(D3DXVECTOR3 *)&m_matrix._41 + vCenter);

	SetExtent(m_vEdge, m_matrix);
	m_fRadius = sqrtf(m_vEdge.x * m_vEdge.x * D3DXVec3LengthSq((D3DXVECTOR3 *) &m_matrix._11)
		+ m_vEdge.y * m_vEdge.y * D3DXVec3LengthSq((D3DXVECTOR3 *) &m_matrix._21)
		+ m_vEdge.z * m_vEdge.z * D3DXVec3LengthSq((D3DXVECTOR3 *) &m_matrix._31)
		+ 2 * m_vEdge.x * m_vEdge.y * fabsf(D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._11, (D3DXVECTOR3 *) &m_matrix._21))
		+ 2 * m_vEdge.y * m_vEdge.z * fabsf(D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._21, (D3DXVECTOR3 *) &m_matrix._31))
		+ 2 * m_vEdge.z * m_vEdge.x * fabsf(D3DXVec3Dot((D3DXVECTOR3 *) &m_matrix._31, (D3DXVECTOR3 *) &m_matrix._11)));
	XEngine::InsertObject(this);
	PlayAnimation(0, 0);
}

void CModel::RemoveModel()
{
	XEngine::RemoveObject(this);
	Destroy();
}
#endif //GAME_XENGINE

namespace CToken
{
	BOOL MeanTrue( char ch)
	{
		return ch != ' ' && ch != ')';
	}

	BOOL MeanFalse( char ch)
	{
		return ch == ' ';
	}

	int tokenint(const char* p,int offset)
	{
		BOOL enter = MeanTrue( *p);

		for( ; *p ;)
		{
			if( enter)
			{
				if( offset == 0)
				{
					const char* end = p;
					while( *end && MeanTrue( *end)) ++end;

					return atoi(p);
				}

				while( *p && MeanTrue( *p)) ++p;
				--offset;
				enter = false;

			}
			else
			{
				while( *p && MeanFalse( *p)) ++p;
				enter = true;
			}
		}

		return 0;
	}

	float tokenfloat(const char* p,int offset)
	{
		BOOL enter = MeanTrue( *p);

		for( ; *p ;)
		{
			if( enter)
			{
				if( offset == 0)
				{
					const char* end = p;
					while( *end && MeanTrue( *end)) ++end;
					return (float)atof(p);
				}

				while( *p && MeanTrue( *p)) ++p;
				--offset;
				enter = false;
			}
			else
			{
				while( *p && MeanFalse( *p)) ++p;
				enter = true;
			}
		}

		return 0;
	}
	int tokencount(const char* p) {
		BOOL enter = MeanTrue( *p);
		const char* before;
		int  icount=0;
		for( ; *p ;) {
			before = p;
			if( enter) {
				while( *p && MeanTrue( *p)) ++p;
				if ( before == p ) break;
				++icount;
				enter = false;
			} else {
				while( *p && MeanFalse( *p)) ++p;
				if ( before == p ) break;
				enter = true;
			}
		}
		return icount;
	}
	const char * tokenchar(const char* p,int offset) {
		BOOL enter = MeanTrue( *p);
		for( ; *p ;) {
			if( enter) {
				if( offset == 0){
					const char* end = p;
					while( *end && MeanTrue( *end)) ++end;
					return (p);
				}
				while( *p && MeanTrue( *p)) ++p;
				--offset;
				enter = false;
			} else {
				while( *p && MeanFalse( *p)) ++p;
				enter = true;
			}
		}
		return 0;
	}
}