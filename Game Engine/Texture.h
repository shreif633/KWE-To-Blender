#ifndef __TEXTURE_H__INCLUDED_
#define __TEXTURE_H__INCLUDED_

#pragma once
#define VB_SIZE (8192*12)

#include "Link.h"
#include "Utility.h"
#include "XFile.h"
#include "ModelHeader.h"

class CD3DFont;

class XCallee
{
public:
	typedef	void (XCallee::*FUNCTION)();

	virtual	char *GetType();
};

class XMessage : public XCallee
{
public:
	enum { INVALID = 0xffffffff, OK = 0, FAIL = 1, MASK = 0xfffffffe };
	typedef	void (XMessage::*FUNCTION)();
	FUNCTION m_pFunction;

	XMessage() { m_pFunction = 0; }
	void	FgPostMessage(FUNCTION function, DWORD dwPriority = 0);
	void	BgPostMessage(FUNCTION function, DWORD dwPriority = 0);
	void	FgAddTimer(FUNCTION function, DWORD dwTime);
	void	BgAddTimer(FUNCTION function, DWORD dwTime);
	BOOL	FgCancelTimer();
	BOOL	BgCancelTimer();
	void	SetStatus(DWORD dwStatus);
	DWORD	GetStatus() { return *(DWORD *)&m_pFunction; }
	void	WaitMessage();
	virtual void	RaisePriority(DWORD dwPriority);

};

class XPriorityMessage : public XMessage
{
public:
	DWORD	m_dwPriority;

	XPriorityMessage() { m_dwPriority = 0; }
	DWORD	GetPriority() { return m_dwPriority; }
	void	SetPriority(DWORD dwPriority) { m_dwPriority = dwPriority; }
	void	FgPostMessage(FUNCTION function);
	void	BgPostMessage(FUNCTION function);
	void	SetStatus(DWORD dwStatus);
	virtual void	RaisePriority(DWORD dwPriority);
};

class XCaller
{
public:
	XCaller() { Clear(); }
	void	Clear() { m_pOwner = 0; }
	void	Call() { if (m_pOwner) (m_pOwner ->* m_pFunction)(); }
	void	Set(XCallee *pOwner, XCallee::FUNCTION pFunction) { m_pOwner = pOwner; m_pFunction = pFunction; }

	XCallee	*m_pOwner;
	XCallee::FUNCTION m_pFunction;
};

class XEventList
{
public:
	XEventList() { m_link.Initialize(); }
	void	FireEvent();
	BOOL	Empty() { return m_link.Empty(); }

	CLink	m_link;
};

class	XEvent
{
public:
	XEvent() { m_link.Initialize(); }
	~XEvent() { RemoveEvent(); }
	void	Initialize() { m_link.Initialize(); }
	void	Clear() { RemoveEvent(); }
	void	AddEvent(XCallee *pOwner, XCallee::FUNCTION pFunction, XEventList *pEventList);
	void	RemoveEvent();
	BOOL	Empty() { return m_link.Empty(); }

	CLink	m_link;
	XCallee*	m_pOwner;
	XCallee::FUNCTION	m_pFunction;
};

class XSystem : public XMessage
{
public:
	enum { CACHE_SIZE = 64 };
	enum { SHADOW_SIZE = 512 };

	static int		m_nResource;
	static int		m_nCache;
	static	CLink	m_linkCache;
	static int		m_nFrameNumber;
	static D3DXMATRIXA16 m_mIdentity;
	static D3DXVECTOR3	*m_pVertex;
	static D3DXVECTOR3	*m_pVertexBegin;
	static D3DXVECTOR3	*m_pVertexEnd;
	static	int VertexSize[VT_END];
	static	D3DVERTEXBLENDFLAGS	VertexBlendFlags[VT_END];
	static	LPDIRECT3DVERTEXSHADER9	VertexShader[8];
	static	D3DVERTEXELEMENT9 Decl[4][6];
	static	D3DVERTEXELEMENT9 DeclVS[4][6];
	static	IDirect3DVertexDeclaration9 *D3DDecl[4];
	static	IDirect3DVertexDeclaration9 *D3DDeclVS[4];
	static LPDIRECT3DTEXTURE9	m_pShadow;
	static LPDIRECT3DSURFACE9	m_pShadowSurface;
	static LPDIRECT3DSURFACE9	m_pBackBuffer;

	static void		AddRef() { ++m_nResource; }
	static void		Release() { if (--m_nResource == 0) Destroy(); }
	static void		Destroy();
	static void		WaitLoading();
	static IDirect3DTexture9 *CreateTexture(LPCVOID pSrcData, UINT nSrcDataSize, DWORD dwFlags);
	static void	LockVertex();
	static void	UnlockVertex();
	static	HRESULT LoadAndCreateVertexShader( LPCSTR strFileName, LPDIRECT3DVERTEXSHADER9 *pHandleVS);
	static void ResetShadow();
	static void	RenderShadow();
	static void PostRenderShadow();

	void	OnTimeout();
};

class CResource : public XPriorityMessage
{
public:

	enum {
#if defined( MODELVIEW)
		NAME_SIZE=MAX_PATH,
#else
		NAME_SIZE=63,
#endif
		CHAR_HEIGHT = 12,
		PATH_COUNT = 3,
	};

	enum {
		NO_CACHE = 0x10000,
	};

	CLink m_link;
	CLink	m_linkCache;
	int	m_nHash;
	int	m_nRef;
	DWORD	m_dwFlags;
	char	m_szName[NAME_SIZE+1];

	void	AddRef() { m_nRef++; }
	void	Release() { if (--m_nRef == 0) OnFinalRelease(); }
	void	Create(LPCSTR szName, DWORD dwFlags, int nHash);
	CResource*	Wait();
	void	Clear();
	void	OnFinalRelease();
	virtual void Destroy() = 0;
	virtual void ResetLOD(){}

	static IDirect3DDevice9* m_pDevice;
	static D3DCAPS9	m_caps;
	static DWORD	m_dwOSVersion;
	static HFONT	m_hFont;
	static	float	m_fFPS;
	static	CD3DFont*   m_pFont;          // Font for drawing text
	static D3DZBUFFERTYPE	m_nZBuffer;
	static BOOL	m_bLowest;
	static LPDIRECT3DVERTEXBUFFER9 m_pVB;
	static LPDIRECT3DVERTEXBUFFER9 m_pSW_VB;
	static unsigned m_nBgThreadId;
	static char  m_szPaths[ PATH_COUNT][ NAME_SIZE + 1];

	static CResource *Find(LPCSTR szName, DWORD dwFlags, int nHash);
	static void InitDevice(IDirect3DDevice9* dev);
	static void	DeleteDevice();
	static BOOL	RestoreDevice();
	static void	InvalidateDevice();
	static unsigned __stdcall ThreadProc(void *);
	static int GetHash(LPCSTR szName);
	static void SetState();
	static void ResetAllTextureLOD();

	static const char* GetPath( int order) { 
		ASSERT( 0 <= order && order < PATH_COUNT);
		return m_szPaths[ order];
	}
	static void SetPath( int order, const char* _path) {
		ASSERT( 0 <= order && order < PATH_COUNT);
		ASSERT( strlen( _path) <= NAME_SIZE);
		strcpy( m_szPaths[ order], _path);
	}
	static void	ProcessMessage();
	inline static BOOL IsBackground() { return (GetCurrentThreadId() == m_nBgThreadId); }
	inline static BOOL CanVolumeMap() { return (m_caps.TextureCaps & D3DPTEXTURECAPS_VOLUMEMAP); }


#ifdef _DEBUG
	static LPD3DXMESH m_pBox;
	static void DrawAxis( D3DXVECTOR3* pWorld);
#endif
};

class CTexture : public CResource
{
public:
	enum { 
		NO_MIPMAP = 1, 
		VOLUME = 2,
		SW_VOLUME = 4,
		NO_CACHE = 0x10000,	// CResource::NO_CACHE
		DECODE = 0x20000,
	};

	CTexture() : m_pTexture(0) {}
	static CTexture* Load(LPCSTR szName, DWORD dwFlags, DWORD dwPriority = 0);
	virtual void Destroy();

	IDirect3DTexture9		*m_pTexture;
	IDirect3DTexture9		**m_ppTexture;
	XFileEx					m_file;
	XEventList				m_xEventList;


	void ResetLOD();
#ifdef	_DEBUG
	static CTexture*	m_pSample;
#endif

	void	OnRead();
	void	OnLoad();
	void	OnFail();
	void	SetWCoord(float fWCoord);
	void	ResetWCoord();
};

extern bool g_bOverBright;

#endif // __TEXTURE_H__INCLUDED_