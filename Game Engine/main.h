#pragma once 

#include <list>

#include "D3DFramework/DXUtil.h"
#include "D3DFramework/D3DEnumeration.h"
#include "D3DFramework/D3DSettings.h"
#include "D3DFramework/D3DApp.h"
#include "D3DFramework/D3DFont.h"
#include "D3DFramework/D3DUtil.h"

#include "gamedata/GameEnv.h"

#ifdef DEF_REGION_ENV_LETSBE_090203
#include "Region.h"
#endif // DEF_REGION_ENV_LETSBE_090203

#ifdef DEF_UI_RENEWALL_081016
#   include "gamestate/CGameStateFSM.h"
#endif

#ifdef DEF_PRIEST_TOWER_OEN_081124
#   include "GroupSystem/CGroupMgr.h"
#endif

#ifdef DEF_TRIGGERSYSTE_OEN_090203
#include "CTriggerMgr.h"
#include "CSequenceMgr.h"
#endif


// earth quake intensity
#define EQ_INTENSITY_0		0.10f
#define EQ_INTENSITY_1		0.28f
#define EQ_INTENSITY_2		0.47f
#define EQ_INTENSITY_3		0.60f

//#ifdef DEF_CANNON_TEST02_KENSHIN_2006_01_06 // 대포 시스템
	#define CANNON_HEIGHT 60.0f
//#endif

typedef struct evQuake_s {
	float		intensity;
	float		timer;
	float		frametime;
	D3DXVECTOR3	origin;
} evQuake_t;

class CMyD3DApplication : public CD3DApplication
{
private:
	enum { 
		LEFTWARD = 1, 
		RIGHTWARD, 
		UPWARD, 
		DOWNWARD, 
		FOREWARD, 
		BACKWARD, 
		UPTURN,
		DOWNTURN,
		LEFTTURN,
		RIGHTTURN,
		ZOOMIN,
		ZOOMOUT,
		VAR_INC,
		VAR_DEC,
	};

	enum {
		WV_REFLECTION = 1,
		WV_REFRACTION,
	};

    CD3DFont*               m_pFont;            // Font for drawing text
	
	//D3DXVECTOR3           m_vCamera;          //place of the camera		
	D3DXVECTOR3	            m_vPick;
	BOOL	                m_nRender;          //!<  렌더링 상태 변수(게임 상태와 동일)
	BOOL	                m_bDialogBoxMode;

	int                     m_nMap;         	//name of the map

	//angles
	//float                 m_fYaw;
	//float                 m_fPitch;
	//float                 m_NearZ;
	//float                 m_FarZ;

	D3DXVECTOR3             m_vUp, m_vEye, m_vAt;
	D3DXMATRIXA16           m_mTmpView;
	BOOL                    m_bEarthQuake;
	BOOL                    m_bGEarthQuake;
	float                   earthtime;
	
	float                   m_fEarthQuakeIntensity;
	float                   m_fEarthQuakeEventIntensity;
	std::list<evQuake_t *>  m_eventQuakeList;

	//move flags
	unsigned                m_nMoveFlag;

#ifdef DEF_UI_RENEWALL_081016
    GameState::CGameStateFSM* m_pGameFSM;
#endif

#ifdef DEF_PRIEST_TOWER_OEN_081124
    GroupSystem::CGroupMgr* m_pGroupMgr;
#endif

#ifdef DEF_TRIGGERSYSTE_OEN_090203
    TriggerSystem::CTriggerMgr* m_pTriggerMgr;
    SequenceSystem::CSequenceMgr* m_pSequenceMgr;
#endif

    // Internal member functions
    virtual HRESULT ConfirmDevice(D3DCAPS9*,DWORD,D3DFORMAT,D3DFORMAT);

protected:
	
    HRESULT OneTimeSceneInit();
    HRESULT InitDeviceObjects();
    HRESULT RestoreDeviceObjects();
    HRESULT InvalidateDeviceObjects();
    HRESULT DeleteDeviceObjects();	

    HRESULT Render();
	void	RenderView();
	void	RenderPostProcess();
    HRESULT FrameMove();
    HRESULT FinalCleanup();
#ifdef DEF_EFFECT_TOOL_CAMERA_OEN_080916
public:
#endif
	void	SetView();
	void	SetProjection( float fNearZ );
#ifdef DEF_EFFECT_TOOL_CAMERA_OEN_080916
protected:
#endif
	
	void BeginWaterView(int waterview);
	void EndWaterView();
	
	IDirect3DSurface9 *GetBackBufferImageSurface(D3DFORMAT *format);
	bool TakeScreenShotJpeg(const char *filename);

public:
	int	                    m_nCursorPrev;
	int	                    m_nCursor;
	DWORD	                m_dwStartTick;

	CSkyBox				    m_SkyBox;
	CWaterSystem		    m_Water;
	D3DXPLANE			    m_waterClipPlane;

	LPDIRECT3DSURFACE9	    m_pBackBuffer;

	bool				    m_bBackBufferTextureUpdated;

	int					    m_bbtWidth;				        // back buffer texture width
	int					    m_bbtHeight;			        // back buffer texture height
	LPDIRECT3DTEXTURE9	    m_pBackBufferTexture;	        // back buffer texture	
	LPDIRECT3DTEXTURE9	    m_pBlurTexture[2];
	LPDIRECT3DTEXTURE9	    m_pLuminanceTexture;
	LPDIRECT3DVERTEXSHADER9	m_pBlurVertexShader;
	LPDIRECT3DPIXELSHADER9	m_pBlurPixelShader;

	void InitPostProcess();
	void ShutdownPostProcess();
	void RestartPostProcess();

	bool UpdateBackBufferTexture();
		
	void SetClipPlane(const D3DXPLANE *pPlane);

	LRESULT MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
	CMyD3DApplication();
	virtual ~CMyD3DApplication();

	virtual INT     Run() {
		INT ret = CD3DApplication::Run();
		return ret;
	}

	void SetDialogBoxMode( BOOL bEnableDialogs);
	void SetCursor(int nCursor);	

	BOOL IsFullScreen() { return !m_bWindowed; }
	void BeginEarthQuake(float intensity) { m_bEarthQuake = TRUE; m_fEarthQuakeIntensity = intensity; }
	void EndEarthQuake() { m_bEarthQuake = FALSE; m_fEarthQuakeIntensity = 0.0f; }	
	
	void SetFrameEarthQuake( BOOL setearth, float intensity, float frametime );							
	//void ProcessEarthQuake(float frametime) ;
	void CalEarthQuakeTime ( float Frametime );

	void EventEarthQuake(const D3DXVECTOR3 &origin, float intensity, float time);
	void ProcessEarthQuakeEvent(float frametime);	
#ifdef DEF_SCENARIO3
	void SetAngle(float frametime);  //엔딩 이벤트용
#endif

#ifdef DEF_REGION_ENV_LETSBE_090203
	Region m_Region;
#endif // DEF_REGION_ENV_LETSBE_090203
};

extern CMyD3DApplication* g_pMyD3DApp;