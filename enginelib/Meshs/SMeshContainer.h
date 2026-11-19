//	[struct SMeshContainer], (2002.12.05), JDH
//
//	NEED	: (2002.11.29), JDH, SMeshContainer, ���͸��� �κ� ���� �ʿ�.
//	UPDATE	: (2002.12.02), JDH, SMeshContainer, ���͸��� �κ� ����.
//	UPDATE	: (2002.12.05), JDH, ����� Ȯ��ʿ� ���� ���� �и��Ͽ� ����.
//
#pragma once

#include <vector>
#include <string>

#include "DxMaterial.h"
//#include "basestream.h"

class	basestream;
class	CSerialFile;
struct SMATERIAL_PIECE;

enum METHOD
{
	D3DNONINDEXED,
	D3DINDEXED,
	SOFTWARE,
	D3DINDEXEDVS,
	NONE
};


#define COL_ERR (-1)


struct SVERTEXINFLU
{
	enum { MAX_BONE = 6, };
	const static DWORD	VERSION;

	DWORD		m_dwIndex;
	D3DXVECTOR3	m_vVector;
	D3DXVECTOR3	m_vNormal;

	DWORD		m_dwNumBone;
	DWORD		m_pBone[MAX_BONE];
	float		m_pWeights[MAX_BONE];

	BOOL SaveFile ( CSerialFile &SFile );
	BOOL LoadFile ( basestream &SFile );

	SVERTEXINFLU ();
	void SetBoneNum ( DWORD _dwBone );
};

struct SBoneInfluence
{
	enum { MAX_BONE = 6, };
	DWORD	dwInfue;
	DWORD	pVertices[MAX_BONE];
	float	pWeights[MAX_BONE];

	SBoneInfluence ();
	void SetInfue ( DWORD _dwInfue );
};

struct D3DXSKINEFFVERTEX
{
	enum { FVF=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2|D3DFVF_TEXCOORDSIZE2(0)|D3DFVF_TEXCOORDSIZE3(1), };

	D3DXVECTOR3	vPos;
	D3DXVECTOR3	vNormal;

	float tu0, tv0;
	float tu1, tv1, tw1;
};

struct D3DXSKINVERTEX
{
	enum { FVF=D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1, };

	D3DXVECTOR3	vPos;
	D3DXVECTOR3	vNormal;

	float tu0, tv0;
};

struct SMeshContainer
{
public:
	char*						m_szName;

	LPD3DXMESH					m_pMesh;
	
	DWORD						m_dwEffNumVerts;
	LPDIRECT3DVERTEXBUFFER9		m_pEffVBuffer;

	SBoneInfluence*				m_pBoneInfluences;
	
	DWORD						m_cMaterials;		//	���͸����� ����.
	DXMATERIAL*					m_pMaterials;		//	���͸��� �迭.
	
	//	Bone Combination ������ ����. - ��Ų�޽��� �ƴ� ��� ���͸��� ����.
	DWORD						m_cpattr;
	DWORD						m_iAttrSplit;

	SMeshContainer				*m_pmcNext;			//	SFrame �� �޽� �����̳� ����Ʈ ��.
	SMeshContainer				*m_pmcGlobalNext;	//	DxSkinMesh �� ��ü �޽� �����̳� ����Ʈ ��.

	// Skin info
	LPD3DXSKINMESH				m_pSkinMesh;
	DWORD						m_dwAttribTableSize;
	D3DXATTRIBUTERANGE*			m_pAttrTable;
	
	DWORD						m_numBoneComb;
	D3DXMATRIX**				m_pBoneMatrix;			//	������ ��ġ�� �� ��Ʈ���� ����. ( matCombined )
	std::vector<std::string>	m_szBoneNames;			//	������ ��ġ�� ���� �̸���.
	
	LPD3DXBUFFER				m_pBoneOffsetBuf;
	D3DXMATRIX*					m_pBoneOffsetMat;

	DWORD*						m_rgiAdjacency;			//	���̽� �ε��� ����
	DWORD						m_maxFaceInfl;			//	������ �� ��Ʈ���� ����.
	LPD3DXBUFFER				m_pBoneCombinationBuf;
	METHOD						m_Method;				//	�޽� ����Ÿ�� �Ӽ�.
	DWORD						m_paletteSize;			//	������ ����Ʈ ũ��.
	BOOL						m_bUseSW;				//	����Ʈ���� ���ؽ� ���μ���.


public:
	HRESULT				SetMaterial ( LPDIRECT3DDEVICE9 pd3dDevice, DWORD nIndex, SMATERIAL_PIECE *pmtrlPiece=NULL, DXMATERIAL_CHAR_EFF* pmtrlSpecular=NULL );
	HRESULT				ReSetMaterial ( LPDIRECT3DDEVICE9 pd3dDevice );

	HRESULT				GenerateMesh ( METHOD method );					// ��Ų �޽�
	HRESULT				GenerateMesh ( LPD3DXBUFFER pAdjacency );		// �Ϲ� �޽�
	HRESULT				UpdateSkinnedMesh ( PBYTE pbData, DWORD dwFVF, LPD3DXMATRIX pBoneMatrices=NULL );

//	Note : ����Ʈ �޽� ���� �κ�.
//
public:
	BOOL				IsEffMesh ()	{	return (m_pEffVBuffer!=NULL); }
	HRESULT				GenerateEffMesh ( LPDIRECT3DDEVICE9 pd3dDevice );		//	Effect �޽� ��ü�� ����.
	HRESULT				UpdateEffMesh ( LPD3DXMATRIX pBoneMatrices );
	LPDIRECT3DVERTEXBUFFER9 GetEffVertexBuffer ()	{ return m_pEffVBuffer; }

public:
	BOOL				IsAlphaTex_HARD ( DWORD nMtIndex );
	BOOL				IsAlphaTex_SOFT ( DWORD nMtIndex );
	
	BOOL				IsCollision ( const D3DXVECTOR3 &vPoint1, const D3DXVECTOR3 &vPoint2, D3DXVECTOR3 &vDetectPos, DWORD &dwDetectFace );
	BOOL				GetClosedPoint ( const D3DXVECTOR3 &vPoint1, const D3DXVECTOR3 &vPoint2, D3DXVECTOR3 &vDetectPos, DWORD &dwDetectIndex );
	HRESULT				GetVertexInfluences ( const DWORD dwIndex, SVERTEXINFLU *pVertInflu );
	HRESULT				CalculateVertexInfluences ( SVERTEXINFLU *pVertInflu, D3DXVECTOR3 &vVecOutput, D3DXVECTOR3 &vNorOutput, LPD3DXMATRIX pBoneMatrice=NULL );

	HRESULT				DebugRender ( LPDIRECT3DDEVICE9 pd3dDevice );

	HRESULT				CalculateBoundingBox ( LPDIRECT3DDEVICE9 pd3dDevice, LPD3DXMATRIX pBoneMatrices, D3DXVECTOR3 &vMax, D3DXVECTOR3 &vMin );

public:
	SMeshContainer () :
			m_pMesh(NULL),
			m_pBoneInfluences(NULL),
			m_dwEffNumVerts(0),
			m_pEffVBuffer(NULL),
			m_szName(NULL),
			m_cMaterials(0),
			m_pMaterials(NULL),
			m_cpattr(0),
			m_iAttrSplit(0),
			m_pmcNext(NULL),
			m_pmcGlobalNext(NULL),
			m_pSkinMesh(NULL),
			m_pAttrTable(NULL),
			m_dwAttribTableSize(0),
			m_pBoneMatrix(NULL),
			m_pBoneOffsetBuf(NULL),
			m_pBoneOffsetMat(NULL),
			m_rgiAdjacency(NULL),
			m_numBoneComb(0),
			m_maxFaceInfl(0),
			m_pBoneCombinationBuf(NULL),
			m_Method(NONE),
			m_paletteSize(0),
			m_bUseSW(FALSE)
	{
	}

	~SMeshContainer ();
};
typedef SMeshContainer* PSMESHCONTAINER;
