//////////////////////////////////////////////////////////////////////////
// Brief: ���� ȯ�氪 ������ ���� �ε������� ó�� 
// Desc: ���� ������ ó���ϰ� �ٷ� ������ �� �� �־�� �ϴµ� ���� Ŭ����
//		 ȯ���� ���� Ʋ���� ���� �����(?) �۾��Ǿ� �Ϲ� ��Ȳ������ �ʿ�����
//		 �ʴ� �۾����� ���� �����.		
// ToDo: GameEnv��� ȯ�濡 ���� Ŭ������ ���� ������ �ִµ� �̰� �и� ���� ����
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "roam.h"

class Region
{
public:
	static const int CELL_COUNT = 32;
	static const int PATCH_COUNT = 16;
	static const int REGION_SIZE = CELL_SIZE * CELL_COUNT * PATCH_COUNT;
	static const int BETWEEN_TOOL_AND_CLIENT_GAP_X = -8192;		// ���� ��� ���� Ŭ�� -_-;
	static const int BETWEEN_TOOL_AND_CLIENT_GAP_Y = -8192;
	static const int BASE_PATCH_X = 31;	// ��ġ�ڴ�. -_-;
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

	// ȯ�氪�� �����ε������͸� ���� ���� ���� ������������.
	struct sRegionEnv
	{
		// ����
		int nIndex;			// ȯ�氪���� ���� �ε����̼ǰ� ��Ī�� �ε��� ��ȣ = �ߺ� ��ȣ�� ���� �� �ִ�.
		D3DXCOLOR clr;		// ����
		SIZE sizeStandard;	// ���� ����
 		bool bSelect;

		// ���� - ������� ���� �� ���� ���� ���� ��
		D3DXVECTOR3 vLF;	// �¾�
		D3DXVECTOR3 vRB;	// �ĵ�
// 		int nID;			// �����ÿ� �ʿ��� ����ũ ID

		// ���ӿ�
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