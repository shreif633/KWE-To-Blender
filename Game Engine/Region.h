//////////////////////////////////////////////////////////////////////////
// Brief: 지역 환경값 정보와 지역 인디케이터 처리 
// Desc: 보통 툴에서 처리하고 바로 적용을 할 수 있어야 하는데 툴과 클라간의
//		 환경이 많이 틀리고 따로 맘대로(?) 작업되어 일반 상황에서는 필요하지
//		 않는 작업들이 많이 생겼다.		
// ToDo: GameEnv라는 환경에 대한 클래스들 모은 파일이 있는데 이거 분리 정리 하자
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "roam.h"

class Region
{
public:
	static const int CELL_COUNT = 32;
	static const int PATCH_COUNT = 16;
	static const int REGION_SIZE = CELL_SIZE * CELL_COUNT * PATCH_COUNT;
	static const int BETWEEN_TOOL_AND_CLIENT_GAP_X = -8192;		// 따로 노는 툴과 클라 -_-;
	static const int BETWEEN_TOOL_AND_CLIENT_GAP_Y = -8192;
	static const int BASE_PATCH_X = 31;	// 미치겠다. -_-;
	static const int BASE_PATCH_Y = 31;
	static const float ENV_BLENDING_TIME;
	static const float INDICATOR_BLENDING_TIME;

	enum eState
	{
		S_STOP = 0,
		S_BEGIN,
		S_RENDER,
		S_END,
	};

	// 환경값과 지역인디케이터를 같이 가다 보니 지저분해졌다.
	struct sRegionEnv
	{
		// 공용
		int nIndex;			// 환경값말고도 지역 인디케이션과 매칭될 인덱스 번호 = 중복 번호를 가질 수 있다.
		D3DXCOLOR clr;		// 색상
		SIZE sizeStandard;	// 기준 지역
 		bool bSelect;

		// 툴용 - 디버깅을 위해 몇 가지 정보 남겨 둠
		D3DXVECTOR3 vLF;	// 좌앞
		D3DXVECTOR3 vRB;	// 후뒤
// 		int nID;			// 에디팅에 필요한 유니크 ID

		// 게임용
		eState m_eIndicator;
		float fCurrentTime;

		sRegionEnv::sRegionEnv()
			: nIndex( -1 ), bSelect( false ), fCurrentTime( 0.f ), m_eIndicator( S_STOP )
		{}
	};

public:
	Region();
	~Region();

	bool Init();
	void Delete();
	void Update();
	void RenderForDebug();

private:
	bool _LoadRegionInfo();
//	bool _LoadRegionIndicator();
	BYTE _UpdateARGB( BYTE byGap, BYTE bySource, float fTime );
	void _BeginEnv( int nIndex );
	void _EndEnv( int nIndex );
	void _RenderIndicator( int nIndex, int nTexIndex );

private:
	std::vector< sRegionEnv > m_RegionEnv;
	bool m_bRestoreColor;
	CTexture* m_pTexture;
};