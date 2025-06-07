#include "StdAfx.h"
#include "gameenv.h"
#include "../roam.h"
#include "../Effect/Shader.h"
#include "../KGameSys.h"
#include "../KGameSysEx.h"
#include "../SunLight.h"
#include "../main.h"

typedef struct skyVert_s {
	D3DXVECTOR3		position;
	float			v, u;
} skyVert_t;

typedef struct fogSkyVert_s {
	D3DXVECTOR3		position;
	DWORD			color;
} fogSkyVert_t;

CSkyBox::CSkyBox()
{
	m_initialized = FALSE;
	m_pVB = NULL;
	m_pFogLayerVB = NULL;

	for (int i = 0; i < 6; ++i)
		m_vTexture[i] = NULL;
}

CSkyBox::~CSkyBox()
{}

void CSkyBox::DeleteDevice()
{
	SAFE_RELEASE(m_pVB);
	SAFE_RELEASE(m_pFogLayerVB);

	for (int i = 0; i < 6; ++i)
		SAFE_RELEASE(m_vTexture[i]);
}

BOOL CSkyBox::InitDevice()
{
	skyVert_t *		pVB;
	fogSkyVert_t *	ptr;
	HRESULT			hr;

	m_initialized = FALSE;
	
	hr = XSystem::m_pDevice->CreateVertexBuffer(sizeof(skyVert_t) * 6 * 4, 
		D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &m_pVB, 0);

	if (FAILED(hr)) 
	{ 
		ELOG("FAIL CreateVertexBuffer At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}
		
	hr = m_pVB->Lock(0, 0, (void **)&pVB, 0);
	if (FAILED(hr)) 
	{
		ELOG("FAIL Lock At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}

	// front, back, left, right, top, down
	for (int i = 0; i < 6; ++i)
	{
		pVB[i * 4 + 0].u = 0.0f; pVB[i * 4 + 0].v = 0.0f;
		pVB[i * 4 + 1].u = 0.0f; pVB[i * 4 + 1].v = 1.0f;
		pVB[i * 4 + 2].u = 1.0f; pVB[i * 4 + 2].v = 1.0f;
		pVB[i * 4 + 3].u = 1.0f; pVB[i * 4 + 3].v = 0.0f;
	}

	float f = 100.0f;
	pVB[0].position = D3DXVECTOR3(-f, f, f);
	pVB[1].position = D3DXVECTOR3(f, f, f);
	pVB[2].position = D3DXVECTOR3(f, -f, f);
	pVB[3].position = D3DXVECTOR3(-f, -f, f);

	pVB[4].position = D3DXVECTOR3(f, f, -f);
	pVB[5].position = D3DXVECTOR3(-f, f, -f);
	pVB[6].position = D3DXVECTOR3(-f, -f, -f);
	pVB[7].position = D3DXVECTOR3(f, -f, -f);

	pVB[8].position = D3DXVECTOR3(-f, f, -f);
	pVB[9].position = D3DXVECTOR3(-f, f, f);
	pVB[10].position = D3DXVECTOR3(-f, -f, f);
	pVB[11].position = D3DXVECTOR3(-f, -f, -f);

	pVB[12].position = D3DXVECTOR3(f, f, f);
	pVB[13].position = D3DXVECTOR3(f, f, -f);
	pVB[14].position = D3DXVECTOR3(f, -f, -f);
	pVB[15].position = D3DXVECTOR3(f, -f, f);

	pVB[16].position = D3DXVECTOR3(-f, f, -f);
	pVB[17].position = D3DXVECTOR3(f, f, -f);
	pVB[18].position = D3DXVECTOR3(f, f, f);
	pVB[19].position = D3DXVECTOR3(-f, f, f);

	pVB[20].position = D3DXVECTOR3(-f, -f, f);
	pVB[21].position = D3DXVECTOR3(f, -f, f);
	pVB[22].position = D3DXVECTOR3(f, -f, -f);
	pVB[23].position = D3DXVECTOR3(-f, -f, - f);

	m_pVB->Unlock();

	// fog layer skybox

	hr = XSystem::m_pDevice->CreateVertexBuffer(sizeof(fogSkyVert_t) * 6 * 4, 
		D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &m_pFogLayerVB, 0);

	if (FAILED(hr)) 
	{ 
		ELOG("FAIL CreateVertexBuffer At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}
	
	hr = m_pFogLayerVB->Lock(0, 0, (void **)&ptr, 0);
	if (FAILED(hr))
	{
		ELOG("FAIL Lock At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}

	for (i = 0; i < 4; ++i)
	{
		ptr[i * 6 + 0].color = D3DCOLOR_RGBA(255, 255, 255, 0);
		ptr[i * 6 + 1].color = D3DCOLOR_RGBA(255, 255, 255, 0);
		ptr[i * 6 + 2].color = D3DCOLOR_RGBA(255, 255, 255, 255);
		ptr[i * 6 + 3].color = D3DCOLOR_RGBA(255, 255, 255, 255);
		ptr[i * 6 + 4].color = D3DCOLOR_RGBA(255, 255, 255, 255);
		ptr[i * 6 + 5].color = D3DCOLOR_RGBA(255, 255, 255, 255);
	}

	float h1 = 20.f;
	float h2 = 0.f;

	ptr[0].position  = D3DXVECTOR3(-f, h1,  f);
	ptr[1].position  = D3DXVECTOR3( f, h1,  f);
	ptr[2].position  = D3DXVECTOR3(-f, h2,  f);
	ptr[3].position  = D3DXVECTOR3( f, h2,  f);
	ptr[4].position  = D3DXVECTOR3(-f, -f,  f);
	ptr[5].position  = D3DXVECTOR3( f, -f,  f);
	
	ptr[6].position  = D3DXVECTOR3( f, h1, -f);
	ptr[7].position  = D3DXVECTOR3(-f, h1, -f);
	ptr[8].position  = D3DXVECTOR3( f, h2, -f);
	ptr[9].position  = D3DXVECTOR3(-f, h2, -f);
	ptr[10].position = D3DXVECTOR3( f, -f, -f);
	ptr[11].position = D3DXVECTOR3(-f, -f, -f);
	
	ptr[12].position = D3DXVECTOR3(-f, h1, -f);
	ptr[13].position = D3DXVECTOR3(-f, h1,  f);
	ptr[14].position = D3DXVECTOR3(-f, h2, -f);
	ptr[15].position = D3DXVECTOR3(-f, h2,  f);
	ptr[16].position = D3DXVECTOR3(-f, -f, -f);
	ptr[17].position = D3DXVECTOR3(-f, -f,  f);

	ptr[18].position = D3DXVECTOR3( f, h1,  f);
	ptr[19].position = D3DXVECTOR3( f, h1, -f);
	ptr[20].position = D3DXVECTOR3( f, h2,  f);
	ptr[21].position = D3DXVECTOR3( f, h2, -f);
	ptr[22].position = D3DXVECTOR3( f, -f,  f);
	ptr[23].position = D3DXVECTOR3( f, -f, -f);

	m_pFogLayerVB->Unlock();

	m_initialized = TRUE;
	return TRUE;
}

void CSkyBox::SetTexture(LPCTSTR name)
{
	m_initialized = FALSE;
	m_texName = name;
	char buf[MAX_PATH];

	for (int i = 0; i < 6; ++i)
	{
		wsprintf(buf, "%s%d.dds", name, i);

		m_vTexture[i] = CTexture::Load(buf, 0);
		if (m_vTexture[i] == NULL)
			return;
	}

	m_initialized = TRUE;
}

void CSkyBox::Draw(BOOL bWater)
{
	if (!m_initialized)	return;

	PDIRECT3DDEVICE9 device = XSystem::m_pDevice;

    // 물 반사 평면 생성
	D3DXPLANE waterClipPlane;

	device->SetVertexShader(0);
//	device->SetVertexDeclaration(0);

	D3DXMATRIXA16 ident, view;
	D3DXMatrixIdentity(&ident);
	device->SetTransform(D3DTS_WORLD, &ident);

	view = XSystem::m_mView;
	view._41 = 0;
	view._42 = 0;
	view._43 = 0;

	device->SetTransform(D3DTS_VIEW, &view);
	
	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    // d3dapp 용 편면을 가져온다.
	if (bWater)
	{
		waterClipPlane = g_pMyD3DApp->m_waterClipPlane;
		g_pMyD3DApp->SetClipPlane(NULL);
	}

	device->SetRenderState(D3DRS_LIGHTING, FALSE);

	device->SetSamplerState(0, D3DSAMP_MIPFILTER,  D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

#ifdef _SUN_LIGHT
	DWORD dwColor = bWater ? (KSunLight::m_dwColorSky & 0xff0000ff)
		+ ((KSunLight::m_dwColorSky & 0xfefe00) >> 1) + ((KSunLight::m_dwColorSky & 0xfc00) >> 2) 
		: KSunLight::m_dwColorSky;
	device->SetRenderState(D3DRS_TEXTUREFACTOR, dwColor);
#else
	device->SetRenderState(D3DRS_TEXTUREFACTOR, bWater ? 0xff80c0ff : 0xFFFFFFFF);
#endif

	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
	device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

	device->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);
	device->SetStreamSource(0, m_pVB, 0, sizeof(skyVert_t));

	for (int i = 0; i < 6; ++i)
	{
		if (m_vTexture[i])
		{
			device->SetTexture(0, m_vTexture[i]->m_pTexture);
			device->DrawPrimitive(D3DPT_TRIANGLEFAN, i * 4, 2);	
		}
	}

	// render fog layer	skybox
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_CURRENT);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, XSystem::m_fogColor);
	device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
	device->SetStreamSource(0, m_pFogLayerVB, 0, sizeof(fogSkyVert_t));
	device->SetTexture(0, NULL);

	for (i = 0; i < 4; i++)
		device->DrawPrimitive(D3DPT_TRIANGLESTRIP, i*6, 4);
	
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	device->SetTransform(D3DTS_VIEW, &XSystem::m_mView);

	//device->SetSamplerState(0, D3DSAMP_MIPFILTER,  D3DTEXF_NONE);
	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	device->SetRenderState(D3DRS_ZENABLE, TRUE);
	device->SetRenderState(D3DRS_CLIPPING, TRUE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

    // 물반사 평면 셋팅
	if (bWater)
		g_pMyD3DApp->SetClipPlane(&waterClipPlane);	
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Bump Reflect Water
//////////////////////////////////////////////////////////////////////////////////////////////////

#define WATER_SIZE		(CELL_SIZE * 512)
#define WATER_COLOR		D3DXCOLOR(1.0f, 1.0f, 1.0f, 0.55f)

typedef struct uvBump_s {
	float		u;
	float		v;
	float		iu;
	float		iv;
} uvBump_t;

static const int		c_rt_size = 512;
static const int		c_vsrw_spirit = 16;
static const float		c_bump_scale = 48.f;

CWater *				CWaterSystem::m_pWater = NULL;
BOOL					CWaterSystem::m_bEnableU8V8 = FALSE;
D3DFORMAT				CWaterSystem::m_rtfmt = D3DFMT_UNKNOWN;
D3DVIEWPORT9			CWaterSystem::m_rtViewPort;
D3DVIEWPORT9			CWaterSystem::m_bbViewPort;

static D3DXVECTOR3 MakeWaterVertex(D3DXVECTOR3 position)
{
	D3DXVECTOR3 direction = position - XSystem::m_vCamera;

	float diff = WATER_POS_Y - position.y;
	float mul = diff / direction.y;
	direction = direction * mul;

	return position + direction;
}

static D3DXVECTOR3 ExtendWaterVertex(D3DXVECTOR3 p)
{
	D3DXVECTOR3 v = XSystem::m_vCamera;
	v = p - v;

	D3DXVec3Normalize(&v, &v);

	v *= WATER_SIZE;
	v = XSystem::m_vCamera + v;

	v.y = WATER_POS_Y;

	return v;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CShaderWater
//////////////////////////////////////////////////////////////////////////////////////////////////

CShaderWater::CShaderWater()
{
	m_pVertexDeclaration = NULL;
	m_pVertexShader = NULL;
	m_pVB = NULL;
	m_pIB = NULL;	
	m_bRendered = FALSE;

	D3DXMatrixIdentity(&m_bumpMatrix);
	float r = 0.005f;
	m_bumpMatrix._11 =  r * cosf(0.0f);
	m_bumpMatrix._12 = -r * sinf(D3DX_PI * 0.5f);
	m_bumpMatrix._21 =  r * sinf(0.0f);
	m_bumpMatrix._22 =  r * cosf(D3DX_PI * 0.5f);

	m_bumpAngleWaterMesh = 0.0f;

	m_bump_transform = 0.0f;

	m_pRefractionTexture = NULL;
	m_pReflectionTexture = NULL;
	m_pNoiseTexture = NULL;
	m_pWaterTexture = NULL;

	m_pRefractionRT = NULL;
	m_pReflectionRT = NULL;
	m_pBackBuffer = NULL;

	m_bProjectEnable = FALSE;
	m_bBumpEnable = FALSE;
	m_bFail = TRUE;
}

void CShaderWater::Animation()
{
	static float s_time = g_fCurrentTime;
	float s_diff = g_fCurrentTime - s_time;

	if (m_bBumpEnable)
	{
		m_bump_transform += s_diff * 0.028f; // 0.0005f;

		if (m_bump_transform > 1.0f)
			m_bump_transform = 0.0f;
	}
	else
	{
		m_bump_transform += s_diff * 5.0f; // 0.0005f;

		if (m_bump_transform > (D3DX_PI * 2) )
			m_bump_transform = 0.0f;
	}

	s_time = g_fCurrentTime;
}

static void DebugDrawTexture(const PDIRECT3DTEXTURE9 pTexture)
{
	struct _dbg{
		D3DXVECTOR3 pos;
		float rhw;
		float tu, tv;
		_dbg(){ rhw = 1.0f;}
	} test[4];

	test[0].pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
	test[1].pos = D3DXVECTOR3(float(c_rt_size), 0.0f, 0.0f);
	test[2].pos = D3DXVECTOR3(float(c_rt_size), float(c_rt_size), 0.0f);
	test[3].pos = D3DXVECTOR3(0.0f, float(c_rt_size), 0.0f);
	test[0].tu = 0.0f, test[0].tv = 0.0f;
	test[1].tu = 1.0f, test[1].tv = 0.0f;
	test[2].tu = 1.0f, test[2].tv = 1.0f;
	test[3].tu = 0.0f, test[3].tv = 1.0f;

	XSystem::m_pDevice->SetTexture(0, pTexture);
	XSystem::m_pDevice->SetTexture(1, NULL);
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
	XSystem::m_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, test, sizeof(_dbg));
}

void CShaderWater::Draw()
{
	if (m_bFail)
		return;

	if (!m_bRendered)
		return;

	if (XSystem::m_vCamera.y < WATER_POS_Y)
		return;

	Animation();
	
	PDIRECT3DDEVICE9 device = XSystem::m_pDevice;

	device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);	
	device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	device->SetRenderState(D3DRS_LIGHTING, FALSE);
	device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFF80);
		
	D3DXMATRIXA16 ident;
	D3DXMatrixIdentity(&ident);
	device->SetTransform(D3DTS_WORLD, &ident);
	device->SetSamplerState(0, D3DSAMP_MIPFILTER,  D3DTEXF_NONE);

	D3DXMATRIXA16 rtt;
	D3DXMatrixIdentity(&rtt);
	D3DXMatrixIdentity(&ident);

	float scale_x = 1.0f / XSystem::m_fAspect * 1.2f;
	float scale_y = 1.0f;
	float trans = 0.5f;

	rtt._11 = scale_x;
	rtt._12 = 0.0f;
	rtt._13 = 0.0f;

	rtt._21 = 0.0f; 
	rtt._22 = 1.0f; 
	rtt._23 = 0.0f;

	rtt._31 = trans;
	rtt._32 =-trans; 
	rtt._33 = scale_y;

	rtt._41 = 0.0f;
	rtt._42 = 0.0f;
	rtt._43 = 0.0f;

	if (m_bBumpEnable && m_pNoiseTexture)
	{/*
		//-----------------------------------------------------------------
		// Draw refraction texture
		//-----------------------------------------------------------------
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		//device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		//device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		device->SetTexture(0, m_pNoiseTexture->m_pTexture);

		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT00, *reinterpret_cast<DWORD *>(&m_bumpMatrix._11));
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT01, *reinterpret_cast<DWORD *>(&m_bumpMatrix._12));
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT10, *reinterpret_cast<DWORD *>(&m_bumpMatrix._21));
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT11, *reinterpret_cast<DWORD *>(&m_bumpMatrix._22));
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BUMPENVMAP);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		device->SetTransform(D3DTS_TEXTURE0, &ident);

		device->SetTexture(1, m_pRefractionTexture);
	
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		device->SetTransform(D3DTS_TEXTURE1, &rtt);

		device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);

		SetSecondTSS();
	
		DrawPolygon(false);*/

		//-----------------------------------------------------------------
		// Draw reflection texture
		//-----------------------------------------------------------------
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		device->SetTexture(0, m_pNoiseTexture->m_pTexture);

		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT00, *reinterpret_cast<DWORD *>(&m_bumpMatrix._11));
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT01, *reinterpret_cast<DWORD *>(&m_bumpMatrix._12));
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT10, *reinterpret_cast<DWORD *>(&m_bumpMatrix._21));
		device->SetTextureStageState(0, D3DTSS_BUMPENVMAT11, *reinterpret_cast<DWORD *>(&m_bumpMatrix._22));
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BUMPENVMAP);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		device->SetTransform(D3DTS_TEXTURE0, &ident);

		device->SetTexture(1, m_pReflectionTexture);

		device->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT);
		device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TEXTURE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		device->SetTransform(D3DTS_TEXTURE1, &rtt);

		device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);

		SetSecondTSS();

		DrawPolygon(true);
	}
	else
	{
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		device->SetTexture(0, m_pReflectionTexture);
		//device->SetTexture(0, 0);

		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

		device->SetTexture(1, m_pReflectionTexture);

		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		device->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);

		device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3|D3DTTFF_PROJECTED );
		device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		device->SetTransform(D3DTS_TEXTURE1, &rtt);

		SetSecondTSS();

		DrawPolygon(true);
	}
	
	//device->SetSamplerState(0, D3DSAMP_MIPFILTER,  D3DTEXF_NONE);
	device->SetTexture(0, NULL);	
	device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);

	device->SetTexture(1, NULL);
	device->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);

	device->SetTexture(2, NULL);
	device->SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2);

	device->SetTransform( D3DTS_TEXTURE0, &ident);
	device->SetTransform( D3DTS_TEXTURE1, &ident);
	device->SetTransform( D3DTS_TEXTURE2, &ident);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	device->SetRenderState(D3DRS_ALPHAREF, 128);

//	DebugDrawTexture(m_pReflectionTexture);
}

void CShaderWater::DrawMesh(const CMesh *mesh)
{
	static D3DXMATRIXA16	bump;
	static const float		scale = 0.005f;
	D3DXMATRIXA16			rtt;
	D3DXMATRIXA16			ident;
	static float			lasttime = g_fCurrentTime;
	float					frametime = g_fCurrentTime - lasttime;

	if (!m_bRendered)
		return;

	Animation();
	
	lasttime = g_fCurrentTime;

	m_bumpAngleWaterMesh += frametime * D3DX_PI;
	m_bumpAngleWaterMesh -= floor(m_bumpAngleWaterMesh / (D3DX_PI * 2)) * (D3DX_PI * 2);

	D3DXMatrixIdentity(&ident);
	
	D3DXMatrixIdentity(&bump);
	bump._11 = scale * cosf(m_bumpAngleWaterMesh);
	bump._12 = -scale * sinf(m_bumpAngleWaterMesh);
	bump._21 = scale * sinf(m_bumpAngleWaterMesh);
	bump._22 = scale * cosf(m_bumpAngleWaterMesh);	

	rtt._11 = 1.0f / XSystem::m_fAspect * 1.2f;
	rtt._12 = 0.0f;
	rtt._13 = 0.0f;
	rtt._14 = 0.0f;

	rtt._21 = 0.0f;		
	rtt._22 = 1.0f;		
	rtt._23 = 0.0f;
	rtt._24 = 0.0f;

	rtt._31 = 0.5f;
	rtt._32 = -0.5f;
	rtt._33 = 1.0f;
	rtt._34 = 0.0f;

	rtt._41 = 0.0f;
	rtt._42 = 0.0f;		
	rtt._43 = 0.0f;
	rtt._44 = 1.0f;
	
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0x80808080);

	XSystem::m_pDevice->SetIndices(mesh->m_pIndexBuffer);

	XSystem::m_pDevice->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1);
	XSystem::m_pDevice->SetStreamSource(0, mesh->m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID));
		
	if (m_bBumpEnable && m_pNoiseTexture)
	{/*
		XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

		XSystem::m_pDevice->SetTexture(0, m_pNoiseTexture->m_pTexture);			
		XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE0, &ident);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT00, *reinterpret_cast<DWORD *>(&bump._11));
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT01, *reinterpret_cast<DWORD *>(&bump._12));
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT10, *reinterpret_cast<DWORD *>(&bump._21));
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT11, *reinterpret_cast<DWORD *>(&bump._22));

		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BUMPENVMAP);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG0, D3DTA_TEXTURE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

		XSystem::m_pDevice->SetTexture(1, m_pRefractionTexture);
		XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE1, &rtt);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3 | D3DTTFF_PROJECTED);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);

		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		XSystem::m_pDevice->DrawIndexedPrimitive(mesh->m_nFaceType, 0, 0, mesh->m_nVertex, 0, mesh->m_nFace);*/

		XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		XSystem::m_pDevice->SetTexture(0, m_pNoiseTexture->m_pTexture);			
		XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
		XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE0, &ident);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT00, *reinterpret_cast<DWORD *>(&bump._11));
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT01, *reinterpret_cast<DWORD *>(&bump._12));
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT10, *reinterpret_cast<DWORD *>(&bump._21));
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_BUMPENVMAT11, *reinterpret_cast<DWORD *>(&bump._22));

		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_BUMPENVMAP);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		XSystem::m_pDevice->SetTexture(1, m_pReflectionTexture);
		XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE1, &rtt);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3 | D3DTTFF_PROJECTED);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);

		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		XSystem::m_pDevice->DrawIndexedPrimitive(mesh->m_nFaceType, 0, 0, mesh->m_nVertex, 0, mesh->m_nFace);
	}
	else
	{		
		XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

		XSystem::m_pDevice->SetTexture(0, m_pReflectionTexture);
		XSystem::m_pDevice->SetTransform(D3DTS_TEXTURE0, &rtt);

		XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		XSystem::m_pDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT3 | D3DTTFF_PROJECTED);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);

		XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

		XSystem::m_pDevice->DrawIndexedPrimitive(mesh->m_nFaceType, 0, 0, mesh->m_nVertex, 0, mesh->m_nFace);
	}		

	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);

	XSystem::m_pDevice->SetTexture(1, NULL);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);	
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
	/*
	XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
	XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);

	XSystem::m_pDevice->SetTexture(0, m_pModulateTexture->m_pTexture);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);

	XSystem::m_pDevice->DrawIndexedPrimitive(mesh->m_nFaceType, 0, 0, mesh->m_nVertex, 0, mesh->m_nFace);
	*/
	XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

	//DebugDrawTexture(m_pReflectionTexture);
}

// 정말 주석 드럽게 안달아 놓네 개시끼들
// 하도 짜증나서 한소리 적어 놓는다. 
// 주석 좀 달아라 이 허접 새끼들아
BOOL CShaderWater::BeginRenderTarget(int target)
{
	PDIRECT3DSURFACE9 rt;

	if (m_bFail)
		return FALSE;

	if (target == RT_REFRACTION)
	{
		return FALSE;

		if (!m_bBumpEnable)
			return FALSE;

		rt = m_pRefractionRT;
	}
	else
	{
		rt = m_pReflectionRT;
	}

    // 원본 백 버퍼를 가져온다.
	XSystem::m_pDevice->GetRenderTarget(0, &m_pBackBuffer);

	if (rt)
	{
		XSystem::m_pDevice->EndScene();

        // 지정 서피스를 설정한다.
		if (D3D_OK != XSystem::m_pDevice->SetRenderTarget(0, rt))
			return FALSE;

        // 스텐실 버프를 설정한다.
		XSystem::m_pDevice->SetDepthStencilSurface(m_pTempDSSurface);

        // 다시 신을 시작한다.
		XSystem::m_pDevice->BeginScene();

        // 화면을 청소한다.
		XSystem::m_pDevice->Clear(0L, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET, XSystem::m_fogColor, 1.0f, 0L );		
	}
	
	m_bRendered = TRUE;

	return TRUE;
}

void CShaderWater::EndRenderTarget()
{	
	XSystem::m_pDevice->EndScene();
	XSystem::m_pDevice->SetRenderTarget(0, m_pBackBuffer);
	XSystem::m_pDevice->SetDepthStencilSurface(m_pBackBufferDSSurface);
	XSystem::m_pDevice->BeginScene();

	SAFE_RELEASE(m_pBackBuffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CShaderWater
//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CShaderWater::RestoreDevice()
{
	m_bFail = TRUE;

	HRESULT hr;

	m_bSoftware = XSystem::m_caps.VertexShaderVersion < (DWORD)D3DVS_VERSION(1, 1);
	m_bProjectEnable = 0 && BOOL(XSystem::m_caps.TextureCaps & D3DPTEXTURECAPS_PROJECTED);
	m_bBumpEnable = BOOL(XSystem::m_caps.TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAP) && CWaterSystem::m_bEnableU8V8;

	m_pWaterTexture = CTexture::Load("water_001.dds", 0);

	if (m_bBumpEnable)
		m_pNoiseTexture = CTexture::Load("wnoise128.bmp", CTexture::NOISEBUMP);
	else
		m_pNoiseTexture = NULL;

	D3DDISPLAYMODE mode;
    XSystem::m_pDevice->GetDisplayMode(0, &mode);

	hr = D3DXCreateTexture(XSystem::m_pDevice, c_rt_size, c_rt_size, 1, 
		D3DUSAGE_RENDERTARGET, mode.Format, D3DPOOL_DEFAULT, &m_pReflectionTexture);
	if (FAILED(hr))
	{
		ELOG("FAIL D3DXCreateTexture At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;		
	}

	hr = m_pReflectionTexture->GetSurfaceLevel(0, &m_pReflectionRT);
	if (FAILED(hr))
	{
		ELOG("FAIL GetSurfaceLevel At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}

	if (m_bBumpEnable)
	{				
		hr = D3DXCreateTexture(XSystem::m_pDevice, c_rt_size, c_rt_size, 1, 
			D3DUSAGE_RENDERTARGET, mode.Format, D3DPOOL_DEFAULT, &m_pRefractionTexture);
		if (FAILED(hr))
		{
			ELOG("FAIL D3DXCreateTexture At %s, %s\n, __FILE__ , __LINE__");
			return FALSE;		
		}

		hr = m_pRefractionTexture->GetSurfaceLevel(0, &m_pRefractionRT);
		if (FAILED(hr))
		{
			ELOG("FAIL GetSurfaceLevel At %s, %s\n, __FILE__ , __LINE__");
			return FALSE;
		}
	}

	// 백버퍼 호환 depth/stencil 버퍼 생성
	D3DSURFACE_DESC desc;
	XSystem::m_pDevice->GetDepthStencilSurface(&m_pBackBufferDSSurface);
	m_pBackBufferDSSurface->GetDesc(&desc);	
	hr = XSystem::m_pDevice->CreateDepthStencilSurface(desc.Width, desc.Height, desc.Format, 
		D3DMULTISAMPLE_NONE, 0, TRUE, &m_pTempDSSurface, NULL);	
	if (FAILED(hr))
		return FALSE;

    D3DVERTEXELEMENT9 decl[] = {
		{ 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 }, 
		{ 0, 12, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_PSIZE, 0 }, 
        D3DDECL_END()
    };

	if (FAILED(XSystem::m_pDevice->CreateVertexDeclaration(decl, &m_pVertexDeclaration)))
        return FALSE;

	if (FAILED(LoadAndCreateVertexShader(m_bBumpEnable ? "BumpWaves" : "UVWaves", &m_pVertexShader)))
		return FALSE;

	int nVertexCount1D = c_vsrw_spirit;
	int nTileCount1d= (nVertexCount1D - 1);
	int nTileCount2d = nTileCount1d * nTileCount1d;

	DWORD usage;
	D3DPOOL pool;

	if (m_bSoftware)
	{
		pool = D3DPOOL_SYSTEMMEM;
		usage = D3DUSAGE_WRITEONLY | D3DUSAGE_SOFTWAREPROCESSING;
	}
	else
	{
		pool = D3DPOOL_DEFAULT;
		usage = D3DUSAGE_WRITEONLY; 
	}

	hr = XSystem::m_pDevice->CreateIndexBuffer(
		sizeof(WORD) * nTileCount2d * 6, 
		usage, 
		D3DFMT_INDEX16, 
		pool, &m_pIB, 0);
	
	if (FAILED(hr)) {
		ELOG("FAIL CreateIndexBuffer At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}
	
	void* p;
	hr = m_pIB->Lock(0, 0, &p, 0);

	if (FAILED(hr)) return FALSE;

	WORD* pIB = (WORD*) p;
	for (int x = 0; x < nTileCount1d; ++x)
	{
		for (int y = 0 ; y < nTileCount1d; ++y)
		{
			int dest_offset = nTileCount1d * y + x;
			int index = nVertexCount1D * y + x;

			pIB[dest_offset * 6 + 0] = index;
			pIB[dest_offset * 6 + 1] = index + 1;
			pIB[dest_offset * 6 + 2] = index + nVertexCount1D + 1;

			pIB[dest_offset * 6 + 3] = index + nVertexCount1D + 1;
			pIB[dest_offset * 6 + 4] = index + nVertexCount1D;
			pIB[dest_offset * 6 + 5] = index;
		}
	}

	m_pIB->Unlock();

	hr = XSystem::m_pDevice->CreateVertexBuffer(sizeof(uvBump_t) * nVertexCount1D * nVertexCount1D, 
		usage, 0, pool, &m_pVB, 0);

	if (FAILED(hr)) {
		ELOG("FAIL CreateVertexBuffer At %s, %s\n, __FILE__ , __LINE__");
		return FALSE;
	}

	hr = m_pVB->Lock(0, 0, &p, 0);

	if (FAILED(hr)) return FALSE;

	uvBump_t* pVB = (uvBump_t*) p;

	for (int u = 0; u < nVertexCount1D; ++u)
	{
		float fv = 1.0f;

		for (int v = 0; v < nVertexCount1D; ++v)
		{
			pVB->u = float(u) / float(nVertexCount1D - 1);
			// pVB->v = float(v) / float(nVertexCount1D - 1);
			pVB->v = 1.0f - fv;
			fv *= 0.5f;

			pVB->iu = 1.0f - pVB->u;
			pVB->iv = 1.0f - pVB->v;

			++pVB;
		}
	}

	m_pVB->Unlock();

	m_bFail = FALSE; 
	return TRUE;
}

void CShaderWater::InvalidateDevice()
{
	SAFE_RELEASE(m_pWaterTexture);
	SAFE_RELEASE(m_pNoiseTexture);
	SAFE_RELEASE(m_pRefractionRT);
	SAFE_RELEASE(m_pReflectionRT);
	SAFE_RELEASE(m_pRefractionTexture);
	SAFE_RELEASE(m_pReflectionTexture);
	SAFE_RELEASE(m_pTempDSSurface);
	SAFE_RELEASE(m_pBackBufferDSSurface);
	SAFE_RELEASE(m_pBackBuffer);

	SAFE_RELEASE(m_pVertexDeclaration);
	SAFE_RELEASE(m_pVertexShader);
	SAFE_RELEASE(m_pVB);
	SAFE_RELEASE(m_pIB);
}

void CShaderWater::SetSecondTSS()
{
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0 );

	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
	XSystem::m_pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1 );

	D3DXMATRIXA16 ident;
	D3DXMatrixIdentity(&ident);
	XSystem::m_pDevice->SetTransform( D3DTS_TEXTURE0, &ident);
	XSystem::m_pDevice->SetTransform( D3DTS_TEXTURE1, &ident);
}

namespace _help
{
	D3DXVECTOR4& operator << (D3DXVECTOR4& dest, const D3DXVECTOR3& source) 
	{
		dest.x = source.x;
		dest.y = source.y;
		dest.z = source.z;
		dest.w = 1.0f;

		return dest;
	}
};

void CShaderWater::DrawPolygon(bool reflection)
{	
	m_bFail = TRUE;

	PDIRECT3DDEVICE9 device = XSystem::m_pDevice;

    D3DXMATRIXA16 matCamera = XSystem::m_mView;
    D3DXMatrixTranspose(&matCamera, &matCamera);

	float scale_x = 1.0f / XSystem::m_fAspect * 1.2f;
	float scale_y = -1.2f;
	//float scale_y = 1.0f;

	if (reflection)
		scale_y *= -1.0f;

	matCamera(0, 0) *= scale_x;
    matCamera(0, 1) *= scale_x;
    matCamera(0, 2) *= scale_x;
    matCamera(0, 3) *= scale_x;
    matCamera(1, 0) *= scale_y;
    matCamera(1, 1) *= scale_y;
    matCamera(1, 2) *= scale_y;
    matCamera(1, 3) *= scale_y;

    if (FAILED(device->SetVertexShaderConstantF(0, (float*)&matCamera, 3) ) )
		return;

    if (FAILED(device->SetVertexShaderConstantF(3, (float*)XSystem::m_mViewProjTrans, 4) ) )
		return;

    FLOAT data[4] = {0.5f, -0.5f, 0, 0};
    if (FAILED(device->SetVertexShaderConstantF(8, (float*)&data, 1) ) )
		return;

	D3DXVECTOR4 vertex[4];
	D3DXVECTOR3 v = (XSystem::m_vWorldNLT + XSystem::m_vWorldNRT) / 2.0f - XSystem::m_vCamera;
	D3DXVec3Normalize(&v, &v);
	D3DXVECTOR3 vup(0.0f, 1.0f, 0.0f);

	using namespace _help;

	if (D3DXVec3Dot(&v, &vup) > 0.0f) 
	{
		vertex[2] << ExtendWaterVertex(XSystem::m_vWorldNLB);
		vertex[3] << ExtendWaterVertex(XSystem::m_vWorldNRB);
	} 
	else 
	{
		vertex[2] << MakeWaterVertex(XSystem::m_vWorldNLT);
		vertex[3] << MakeWaterVertex(XSystem::m_vWorldNRT); 
	}
	vertex[0] << MakeWaterVertex(XSystem::m_vWorldNLB);
	vertex[1] << MakeWaterVertex(XSystem::m_vWorldNRB);

	if (FAILED(device->SetVertexShaderConstantF(9, (float*)vertex, 4)))
		return;

	D3DXCOLOR color = WATER_COLOR;
	D3DXVECTOR4 distortionTransform(m_bump_transform, m_bump_transform, c_bump_scale / WATER_SIZE, 0);
	float waterPos[4] = { 0.0f, 1.0f, 0.0f, WATER_POS_Y };

	if (FAILED(device->SetVertexShaderConstantF(13, (float*)&color, 1)))
		return;
	if (FAILED(device->SetVertexShaderConstantF(14, (float*)&distortionTransform, 1)))
		return;
	if (FAILED(device->SetVertexShaderConstantF(15, (float*)&XSystem::m_vCamera, 1)))
		return;

	int nVertexCount1D = c_vsrw_spirit;
	int nTileCount1d = (nVertexCount1D - 1);
	int nTileCount2d = nTileCount1d * nTileCount1d;

	device->SetVertexShader(m_pVertexShader);
	device->SetVertexDeclaration(m_pVertexDeclaration);

	device->SetIndices(m_pIB);
	device->SetStreamSource(0, m_pVB, 0, sizeof(uvBump_t));

	if (m_bSoftware)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(TRUE);

	XSystem::m_pDevice->DrawIndexedPrimitive(
		D3DPT_TRIANGLELIST, 
		0, 	0, 
		nVertexCount1D * nVertexCount1D, 
		0, nTileCount2d * 2);

	if (m_bSoftware)
		XSystem::m_pDevice->SetSoftwareVertexProcessing(FALSE);

	device->SetVertexShader(0);
//	device->SetVertexDeclaration(0);

	m_bFail = FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CSimpleWater
//////////////////////////////////////////////////////////////////////////////////////////////////

CSimpleWater::CSimpleWater()
{
	m_pTexture = NULL;
}
void CSimpleWater::InitDevice()
{
	XSystem::SetPath(0, "Data\\Objects\\common\\tex\\");
	m_pTexture = CTexture::Load("water_001.dds", 0);
}
void CSimpleWater::DeleteDevice()
{
	SAFE_RELEASE(m_pTexture);
}

void CSimpleWater::Draw()
{
	if (XSystem::m_vCamera.y < WATER_POS_Y)
		return;

	if (NULL == m_pTexture)
		return;
	
	PDIRECT3DDEVICE9 device = XSystem::m_pDevice;

	D3DXVECTOR3 vertex[4];
	D3DXVECTOR3 v = (XSystem::m_vWorldNLT + XSystem::m_vWorldNRT) / 2.0f - XSystem::m_vCamera;
	D3DXVec3Normalize(&v, &v);
	D3DXVECTOR3 vup(0.0f, 1.0f, 0.0f);

	vertex[0] = MakeWaterVertex(XSystem::m_vWorldNLB);
	vertex[2] = MakeWaterVertex(XSystem::m_vWorldNRB);
	if (D3DXVec3Dot(&v, &vup) > 0.0f) 
	{
		vertex[1] = ExtendWaterVertex(XSystem::m_vWorldNLB);
		vertex[3] = ExtendWaterVertex(XSystem::m_vWorldNRB);
	} 
	else 
	{
		vertex[1] = MakeWaterVertex(XSystem::m_vWorldNLT);
		vertex[3] = MakeWaterVertex(XSystem::m_vWorldNRT); 
	}

	device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT);
	device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);

	device->SetRenderState(D3DRS_TEXTUREFACTOR, g_bOverBright ? 
		D3DCOLOR_ARGB((int)(255.0f * 0.4f), 127, 127, 127) :
		D3DCOLOR_ARGB((int)(255.0f * 0.4f), 255, 255, 255));

	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTexture(0, m_pTexture->m_pTexture);

	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);
	device->SetTransform(D3DTS_WORLD,  &identity);

	device->SetFVF(D3DFVF_XYZ);
	
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, D3DTSS_TCI_CAMERASPACEPOSITION);

	D3DXMATRIXA16 tt0, swap;
	
	D3DXMatrixIdentity(&swap);
	std::swap(swap._22, swap._23);
	std::swap(swap._32, swap._33);

	const float c_fScale = 1.0f / (CELL_SIZE * 4);
	D3DXMatrixScaling(&tt0, c_fScale, c_fScale, c_fScale);
	D3DXMatrixMultiply(&tt0, &swap, &tt0);
	D3DXMatrixMultiply(&tt0, &XSystem::m_mViewInv, &tt0);
	
	device->SetTransform(D3DTS_TEXTURE0, &tt0);

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);

	device->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
	device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

#ifdef _USE_DRAWPRIMITIVEUP
	device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertex, sizeof(D3DXVECTOR3));
#else
	DrawPrimitiveUPTest(D3DPT_TRIANGLESTRIP, 2, vertex, sizeof(D3DXVECTOR3), 4 * sizeof(D3DXVECTOR3));
#endif

	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

	device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
	device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
}

void CSimpleWater::DrawMesh(const CMesh *mesh)
{
	if (!m_pTexture)
		return;
	
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	XSystem::m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	XSystem::m_pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, g_bOverBright ? 
		D3DCOLOR_ARGB((int)(255.0f * 0.4f), 127, 127, 127) :
		D3DCOLOR_ARGB((int)(255.0f * 0.4f), 255, 255, 255));

	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	XSystem::m_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);

	XSystem::m_pDevice->SetIndices(mesh->m_pIndexBuffer);
	XSystem::m_pDevice->SetVertexDeclaration(XSystem::VertexDeclFixed[mesh->m_nVertexType]);
	XSystem::m_pDevice->SetStreamSource(0, mesh->m_pVertexBuffer, 0, sizeof(GB_VERTEX_RIGID));

	XSystem::m_pDevice->SetTexture(0, m_pTexture->m_pTexture);

	XSystem::m_pDevice->DrawIndexedPrimitive(mesh->m_nFaceType, 0, 0, mesh->m_nVertex, 0, mesh->m_nFace);

	XSystem::m_pDevice->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	XSystem::m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// CWaterSystem
//////////////////////////////////////////////////////////////////////////////////////////////////

CWaterSystem::~CWaterSystem() 
{
	SAFE_DELETE(m_pWater);
}

void CWaterSystem::SetViewPort(int bgWidth, int bgHeight)
{
	m_rtViewPort.X = 0;
	m_rtViewPort.Y = 0;
	m_rtViewPort.Width = c_rt_size;
	m_rtViewPort.Height = c_rt_size;
	m_rtViewPort.MinZ = 0;
	m_rtViewPort.MaxZ = 1;

	m_bbViewPort.X = 0;
	m_bbViewPort.Y = 0;
	m_bbViewPort.Width = bgWidth;
	m_bbViewPort.Height = bgHeight;
	m_bbViewPort.MinZ = 0;
	m_bbViewPort.MaxZ = 1;
}

BOOL CWaterSystem::BeginRenderTarget(int target) 
{
	if (m_pWater && KSetting::WaterReflect() ) 
		return m_pWater->BeginRenderTarget(target);

	return FALSE; 
}

void CWaterSystem::EndRenderTarget() 
{
	if (m_pWater && KSetting::WaterReflect()) 
		return m_pWater->EndRenderTarget();
}

BOOL CWaterSystem::RestoreDevice() 
{
	if (m_pWater) 
		m_pWater->RestoreDevice();

	return TRUE;
}

void CWaterSystem::InvalidateDevice() 
{
	if (m_pWater) 
		m_pWater->InvalidateDevice();
}

void CWaterSystem::Init(BOOL bEnableU8V8, D3DFORMAT renderTargetFormat)
{
	m_bEnableU8V8 = bEnableU8V8;
	m_rtfmt = renderTargetFormat;

	SAFE_DELETE(m_pWater);
	
	if (KSetting::WaterReflect())
	{
		m_pWater = new CShaderWater();
	}
	else
	{
		m_pWater = new CSimpleWater();
		m_pWater->InitDevice();
	}
}

void CWaterSystem::Reset()
{
	if (m_pWater) {
		m_pWater->InvalidateDevice();
		m_pWater->DeleteDevice();
		SAFE_DELETE(m_pWater);
	}

	if (KSetting::WaterReflect())
	{
		m_pWater = new CShaderWater();
		m_pWater->RestoreDevice();
	}
	else
	{
		m_pWater = new CSimpleWater();
		m_pWater->InitDevice();
	}
}

void CWaterSystem::DeleteDevice()
{
	if (m_pWater) 
		m_pWater->DeleteDevice();
}

void CWaterSystem::Draw()
{
	if (m_pWater)
		m_pWater->Draw();
}

void CWaterSystem::DrawMesh(const CMesh *mesh)
{
	if (m_pWater)
		m_pWater->DrawMesh(mesh);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

static int g_prtEntity = -1;

void ChangeEnv( envEnum_t envEnum)
{
	static char particleName[64] = "";
	bool newParticle = false;

	switch( envEnum)
	{
		case ENV_STOP:
			break;
		case ENV_RESUME:
			newParticle = true;
			break;
#ifdef DEF_FLOWEREVENT5_DIOB_20070502		//가정의 달 5월 카네이션 이벤트
		case ENV_SNOW:
			newParticle = true;
			strcpy( particleName, "test_flower");
			break;
		case ENV_SNOW_FLAKES:
			newParticle = true;
			strcpy( particleName, "test_flower");
			break;
		case ENV_SNOW_STORM:
			newParticle = true;
			strcpy( particleName, "test_flower");
			break;
#else
#ifdef DEF_FLOWEREVENT_SEA_20060327			//진달래 이벤트
		case ENV_SNOW:
			newParticle = true;
			strcpy( particleName, "flower03");
			break;
		case ENV_SNOW_FLAKES:
			newParticle = true;
			strcpy( particleName, "flower02");
			break;
		case ENV_SNOW_STORM:
			newParticle = true;
			strcpy( particleName, "flower01");
			break;
#else
#ifdef DEF_MAPLELEAVES_EVENT_SEA_20061010	//단풍잎 이벤트
		case ENV_SNOW:
			newParticle = true;
			strcpy( particleName, "test_leaf_small");
			break;
		case ENV_SNOW_FLAKES:
			newParticle = true;
			strcpy( particleName, "test_leaf");
			break;
		case ENV_SNOW_STORM:
			newParticle = true;
			strcpy( particleName, "test_typoon_leaf");
			break;
#else										//눈 이벤트
		case ENV_SNOW:
			newParticle = true;
			strcpy( particleName, "nunbora03");
			break;
		case ENV_SNOW_FLAKES:
			newParticle = true;
			strcpy( particleName, "nunbora02");
			break;
		case ENV_SNOW_STORM:
			newParticle = true;
			strcpy( particleName, "nunbora01");
			break;
#endif //DEF_MAPLELEAVES_EVENT_SEA_20061010
#endif //DEF_FLOWEREVENT_SEA_20060327
#endif
		case ENV_SNOW_FALL:
			newParticle = true;
			strcpy( particleName, "test_snow");
			break; //sea 20061120 천하타설 이벤트( 공격력 10% 향상)
		
		default:
			particleName[0] = 0;
			break;
	}

	if( g_prtEntity != -1)
	{
		g_particleEntityManager.RemoveParticle( g_prtEntity, false);
		g_prtEntity = -1;
	}

	if( newParticle && particleName[0])
	{
		// 필드 영역일 때만 (던젼 영역이 아니라면)
		if (XSystem::m_originPatchX == 32 && XSystem::m_originPatchY == 32)
			g_prtEntity = g_particleEntityManager.AddParticleToCameraCenter(particleName, false);
	}

	
	if (envEnum == ENV_CLEAR)
		g_pMyD3DApp->EndEarthQuake();
}