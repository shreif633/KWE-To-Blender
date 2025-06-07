
#ifndef __MODELHEADER_H__INCLUDED_
#define __MODELHEADER_H__INCLUDED_

/* Mesh
					Drawing		Pick		Camera		Collision	Example
Normal				O			O			X			O
Hidden(H_)			X			X			X			O			hidden Barricade
Camera(C_)			O			O			O			O			wall, floor, high roof
NoPick(N_)			O			X			O			O			upper pillar 
Transparent(T_)		O			X			X			X			leaf

Floor(F_)
TwoSide(S_)	 Transparent two side
*/

/* Material

Effect F_

*/

#define GB_HEADER_VERSION 12

enum VERTEX_TYPE
{
#if	GB_HEADER_VERSION < 11
	VT_STATIC,					
#endif
	VT_RIGID,					
	VT_BLEND1,					
	VT_BLEND2,					
	VT_BLEND3,					
	VT_BLEND4,					
#if GB_HEADER_VERSION >= 9 && GB_HEADER_VERSION < 11
	VT_LM_STATIC,				
#endif
#if	GB_HEADER_VERSION >= 11
	VT_RIGID_DOUBLE,
#endif
	VT_END,						
};

enum FACE_TYPE
{
	FT_LIST,
	FT_STRIP,
	FT_END
};

enum 
{
	MATERIAL_TWOSIDED = 1,
	MATERIAL_MAP_OPACITY = 2,
	MATERIAL_MAP_ALPHA_RGB = 4,
	MATERIAL_SPECULAR = 8,
	// MATERIAL_TEXTURE = 0x10,
	MATERIAL_LIGHT = 0x20,
	// MATERIAL_OPTION = 0x80,
	MATERIAL_LIGHTMAP = 0x100,
	MATERIAL_FX = 0x200,
};

enum {
	MODEL_MAX_BONE	= 200,
	MODEL_MAX_MESH	= 128,
	MODEL_MAX_ANIM	= 16,
	MODEL_MAX_BONEINDEX = 28,
};

enum {
	MODEL_BONE = 1,
	MODEL_FX = 2,
};
#pragma pack(push, 1)

#define MODEL_CRC_SEED 0x35bfd8a4L

struct GB_HEADER {
	BYTE	version;
	BYTE	bone_count;
	BYTE	flags;
	BYTE	mesh_count;
#if	GB_HEADER_VERSION >= 10
	DWORD	crc;
#endif
#if	GB_HEADER_VERSION >= 12
	char	name[64];
#endif
	DWORD   szoption;
#if	GB_HEADER_VERSION >= 9
	WORD	vertex_count[12];
#else
	WORD	vertex_count[6];
#endif

	WORD	index_count;
	WORD	bone_index_count;
	WORD	keyframe_count;
#if	GB_HEADER_VERSION >= 9
	WORD	reserved0;

	DWORD	string_size;
	DWORD	cls_size;
#else
	WORD	string_count;
	WORD	string_size;
	WORD	cls_size;
#endif

	WORD	anim_count;
	BYTE	anim_file_count;
#if	GB_HEADER_VERSION >= 9
	BYTE	reserved1;
#endif

	WORD	material_count;
	WORD	material_frame_count;

#if	GB_HEADER_VERSION >= 11
	D3DXVECTOR3	minimum;
	D3DXVECTOR3 maximum;
#endif
#if	GB_HEADER_VERSION >= 9
	DWORD	reserved2[4];
#endif
};

struct GB_BONE {
	D3DXMATRIX matrix;
	BYTE	parent;
};

struct GB_MESH_HEADER {
	DWORD	name;
	int		material_ref;
	BYTE	vertex_type;
	BYTE	face_type;
	WORD	vertex_count;
	WORD	index_count;
	BYTE	bone_index_count;
};

struct GB_ANIM_HEADER {
	DWORD   szoption;
	WORD	keyframe_count;
};

struct GB_KEYFRAME {
	WORD	time;
	DWORD	option;
};

struct GB_MATERIAL_KEY {
	DWORD		szTexture;
	WORD		mapoption;
	DWORD		szoption;
	float		m_power;
	DWORD		m_frame;
};

struct GB_MATERIAL_FRAME {
	D3DCOLOR	m_ambient;
	D3DCOLOR	m_diffuse;
	D3DCOLOR	m_specular;
	float		m_opacity;
	D3DXVECTOR2 m_offset;
	D3DXVECTOR3 m_angle;
};

struct GB_CLS_HEADER {
	WORD	vertex_count;
	WORD	face_count;
#if	GB_HEADER_VERSION >= 11
	DWORD	reserved[6];
#else
	D3DXVECTOR3	minimum;
	D3DXVECTOR3 maximum;
#endif
};




struct GB_CLS_NODE {
	enum {
		L_LEAF = 1,
		R_LEAF = 2,
		X_MIN = 4,
		X_MAX = 8,
		Y_MIN = 0x10,
		Y_MAX = 0x20,
		Z_MIN = 0x40,
		Z_MAX = 0x80,
		L_HIDDEN = 0x100,
		R_HIDDEN = 0x200,
		L_CAMERA = 0x400,
		R_CAMERA = 0x800,
		L_NOPICK = 0x1000,
		R_NOPICK = 0x2000,
		L_FLOOR = 0x4000,
		R_FLOOR = 0x8000
	};
	WORD	flag;
	BYTE	x_min, y_min, z_min;
	BYTE	x_max, y_max, z_max;
	WORD	left, right;

	void MinMax(D3DXVECTOR3 *pLMin, D3DXVECTOR3 *pRMin, D3DXVECTOR3 *pLMax, D3DXVECTOR3 *pRMax, 
		const D3DXVECTOR3 &vMin, const D3DXVECTOR3& vMax)
	{
		D3DXVECTOR3 e = (1 / 255.0f) * ( vMax - vMin);
		*pLMin = *pRMin = vMin;
		if (flag & X_MIN)
			pLMin->x += x_min * e.x;
		else
			pRMin->x += x_min * e.x;
		if (flag & Y_MIN)
			pLMin->y += y_min * e.y;
		else
			pRMin->y += y_min * e.y;
		if (flag & Z_MIN)
			pLMin->z += z_min * e.z;
		else
			pRMin->z += z_min * e.z;
		*pLMax = *pRMax = vMax;
		if (flag & X_MAX)
			pLMax->x -= x_max * e.x;
		else
			pRMax->x -= x_max * e.x;
		if (flag & Y_MAX)
			pLMax->y -= y_max * e.y;
		else
			pRMax->y -= y_max * e.y;
		if (flag & Z_MAX)
			pLMax->z += z_max * e.z;
		else
			pRMax->z += z_max * e.z;
	}

};

#pragma pack(pop)

struct GB_VERTEX_RIGID
{		// sea 2006-02-28
	D3DXVECTOR3 v;			// vertex point 
	D3DXVECTOR3 n;			// 법선 벡터 
	D3DXVECTOR2	t;			// uv 좌표 
};

struct GB_VERTEX_BLEND1
{
	D3DXVECTOR3 v;
	DWORD	indices;
	D3DXVECTOR3 n;
	D3DXVECTOR2	t;
};

struct GB_VERTEX_BLEND2
{
	D3DXVECTOR3 v;
	FLOAT	blend[1];
	DWORD	indices;
	D3DXVECTOR3 n;
	D3DXVECTOR2	t;
};

struct GB_VERTEX_BLEND3
{
	D3DXVECTOR3 v;
	FLOAT	blend[2];
	DWORD	indices;
	D3DXVECTOR3 n;
	D3DXVECTOR2	t;
};

struct GB_VERTEX_BLEND4 {
	D3DXVECTOR3 v;
	FLOAT	blend[3];
	DWORD	indices;
	D3DXVECTOR3 n;
	D3DXVECTOR2	t;
};

struct GB_VERTEX_RIGID_DOUBLE {
	D3DXVECTOR3 v;
	D3DXVECTOR3 n;
	D3DXVECTOR2	t0;
	D3DXVECTOR2	t1;
};

struct GB_ANIM {	        // 하나의 키프레임 본의 정보  (GB 애니메이션 보간)
	D3DXVECTOR3 pos;	    // 이동
	D3DXQUATERNION quat;	// 회전
	D3DXVECTOR3 scale;	    // 스케일
};

#endif //__MODELHEADER_H__INCLUDED_