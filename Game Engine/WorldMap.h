/************************************************************************/
/* 파일명 : WorldMap.h													*/
/* 내  용 : 전체 맵 클래스												*/
/************************************************************************/
#ifndef _WorldMap_h_
#define _WorldMap_h_

#include "../ContentBase.h"

#ifdef DEF_UI_CONTENTS_KUMADORI_090227

//////////////////////////////////////////////////////////////////////
///	class name : WorldMap - 전체 맵 클래스							//
//////////////////////////////////////////////////////////////////////
class WorldMap : public KHyperView
{
public:
	enum {
		BORDERX				= 0,
		BORDERY				= 0,
		PATCHBORDER			= 1,
		PATCHSIZE			= 128,
		TEXTUREHALFSIZE		= 4,
	};

	CTexture* m_pHero;
	CTexture* m_pCamera;

public:
	void OnInit();
	void OnCreate();
	void OnDelete();
	void OnPaint();
	void DrawTexture( int x, int y, DWORD dwColor, CTexture* pTexture, float fHalfsize, float fRotation = 0.0f); 
	void MoveButton();

public:
	WorldMap() { OnCreate();}
	virtual ~WorldMap() { OnDelete();}
	static KHyperView* CreateObject() { return new WorldMap(); }
};

#endif // DEF_UI_CONTENTS_KUMADORI_090227

#endif