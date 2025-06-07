/************************************************************************/
/* 파일명 : ContentBase.h												*/
/* 내  용 : 컨텐츠 선참조 헤더											*/
/************************************************************************/
#ifndef _ContentBase_h_
#define _ContentBase_h_


#pragma once

#include "Utility.h"
#include "KGameSys.h"
#include "kgamesysex.h"

#include "GameData/GameUser.h"
#include "GameData/GameMonster.h"
#include "GameData/GameCharacter.h"
#include "GameData/GameItem.h"
#include "GameData/GameNPC.h"
#include "GameData/GameEffect.h"

#include "lisp.h"
#include "HyperElement.h"
#include "CControlEx.h"
#include "gamedata/GameUser.h"
#include "gamedata/GameCharacter.h"
#include "KSocket.h"

using namespace Control;
using namespace CControlEx;
using namespace KGameSys;
using namespace CGame_Character;
using HyperElement::CElement;
using HyperElement::CPosition;
using HyperElement::CList;
using HyperElement::CPage;
using HyperElement::CWord;
using HyperElement::CLabel;
using HyperElement::CBackground;
#endif