// DxEffectFire.h: interface for the DxEffectFire class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTFIRE_H__INCLUDED_)
#define AFX_DXEFFECTTOON_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DxEffectFrame.h"

struct DxFrame;

struct FIRE_PROPERTY
{
	DWORD		m_dwNumFaces;

	float		m_fPosX;		// �� ��Ÿ�� ��ġ
	float		m_fPosY;
	float		m_fPosZ;

	int			m_iCol;
	int			m_iRow;
	float		m_fAniTime;

	float		m_fUpLong;		// ���κ� ����
	float		m_fDownLong;	// �Ʒ� �κ� ����
	float		m_fHeight;		// ����
	
	char		m_szTexture[MAX_PATH];
};


class DxEffectFire : public DxEffectBase
{
	//	Note : ����Ʈ Ÿ�� ����.
	//
public:
	const static DWORD	TYPEID;
	const static DWORD	VERSION;
	const static DWORD	FLAG;
	const static char	NAME[];

public:
	virtual DWORD GetTypeID ()		{ return TYPEID; }
	virtual DWORD GetFlag ()		{ return FLAG; }
	virtual const char* GetName ()	{ return NAME; }

	virtual void GetProperty ( PBYTE &pProp, DWORD &dwSize, DWORD &dwVer );
	virtual void SetProperty ( PBYTE &pProp, DWORD &dwSize, DWORD dwVer );
	
	//	Note : ���̴�.
	//
protected:	
	static DWORD			m_dwSavedStateBlock;
	static DWORD			m_dwEffectStateBlock;

	//	Note : �Ӽ�.
	//
protected:
	union
	{
		FIRE_PROPERTY	m_Property;

		struct
		{
			DWORD		m_dwNumFaces;

			float		m_fPosX;
			float		m_fPosY;
			float		m_fPosZ;

			int			m_iCol;
			int			m_iRow;
			float		m_fAniTime;

			float		m_fUpLong;
			float		m_fDownLong;
			float		m_fHeight;

			char		m_szTexture[MAX_PATH];
		};
	};

public:
	void SetProperty ( FIRE_PROPERTY& Property )
	{
		m_Property = Property;
	}
	FIRE_PROPERTY& GetProperty () { return m_Property; }

protected:
	float	m_fTime;

	DWORD	m_dwVertices;
	DWORD	m_dwIndices;

	int		m_iNowSprite;

	LPDIRECT3DVERTEXBUFFER9		m_pVB;
	LPDIRECT3DINDEXBUFFER9		m_pIB;

	LPDIRECT3DTEXTURE9	m_pddsTexture;

	struct FIREVERTEX
	{
		D3DXVECTOR3	p;
		D3DCOLOR	d;
		D3DXVECTOR2	t;
		const static DWORD FVF;
	};

	struct PARTICLE
	{
		D3DXVECTOR3   vPosition;
		D3DXVECTOR3   vVelocity;
		D3DXVECTOR3   vAccelerate;
		D3DXVECTOR3   vForce;
	};

public:
	static HRESULT CreateDevice ( LPDIRECT3DDEVICE9 pd3dDevice );
	static HRESULT ReleaseDevice ( LPDIRECT3DDEVICE9 pd3dDevice );

protected:
	HRESULT AdaptToDxFrameChild ( DxFrame *pframeCur, LPDIRECT3DDEVICE9 pd3dDevice );

public:
	HRESULT AdaptToDxFrame ( DxFrame *pframeCur, LPDIRECT3DDEVICE9 pd3dDevice );

	//	Note : 
	//
public:
	virtual HRESULT InitDeviceObjects ( LPDIRECT3DDEVICE9 pd3dDevice );
	virtual HRESULT DeleteDeviceObjects ();

public:
	HRESULT FrameMove ( float fTime, float fElapsedTime );	
	HRESULT Render ( DxFrame *pFrame, LPDIRECT3DDEVICE9 pd3dDevice );

protected:
	HRESULT CreateFireMesh ( LPDIRECT3DDEVICE9 pd3dDevice, LPD3DXMESH m_pLocalMesh );

public:
	DxEffectFire();
	virtual ~DxEffectFire();
};

#endif // !defined(AFX_DXEFFECTTOON_H__INCLUDED_)
