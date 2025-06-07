#pragma once

#define MAP_VERSION 2
#define MAP_VERSION_NO(x) x
#define MAP_CRC_SEED 0x35bfd8a5L

#pragma pack(push, 1)

enum LIGHT_DECAY_TYPE
{
	DECAY_NONE = 0,
	DECAY_INVERSE = 1,
	DECAY_INVERSE_SQUARE = 2
};

struct MAP_HEADER {
	BYTE	version;
	BYTE	region_count;
	WORD	node_count;
#if	MAP_VERSION >= 2
	DWORD	crc;
#endif
	WORD	leaf_count;
	WORD	part_count;
	DWORD	string_size;
	D3DXVECTOR3	maximum;
	D3DXVECTOR3	minimum;
	DWORD	option;
	WORD	light_count;
	WORD	object_count;
	DWORD	reserved2[3];
};

//! 말단
struct	MAP_LEAF {
	DWORD	leaf_list;
};

//! 데이터 노드
struct	MAP_NODE {
	D3DXPLANE	plane;
	WORD	leaf;
	WORD	side[2];
};

//! 지역
struct MAP_REGION {
	DWORD	name;
	DWORD	part_count;
	D3DXVECTOR3	maximum;
	D3DXVECTOR3	minimum;
	DWORD	leaf_list;
	DWORD	reserved[2];
};

//! 지역 내의 부분 영역
struct MAP_PART {
	DWORD	name;
	D3DXVECTOR3	maximum;
	D3DXVECTOR3	minimum;
	DWORD	reserved[2];
};

struct MAP_LIGHT {
	DWORD	name;
	BYTE	type;
	BYTE	decay_type;
	WORD	reserverd;
	D3DCOLOR color;	
	float	intensity;
	float	decay_radius;
	float	atten_start;
	float	atten_end;
	D3DXVECTOR3	position;
	D3DXVECTOR3	direction;
	float	theta;
	float	phi;
};

struct MAP_OBJECT {
	DWORD	path;
	float	matrix[12];
};

#pragma pack(pop)
