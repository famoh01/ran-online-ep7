// DxEffectBlur.h: interface for the DxEffectBlur class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTBLUR_H__INCLUDED_)
#define AFX_DXEFFECTBLUR_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DxEffectFrame.h"

struct DxFrame;
struct DxOctreeMesh;

struct BLUR_PROPERTY
{
	D3DXVECTOR4		m_vCurrentColor;
	D3DXVECTOR4		m_vPrevColor;
	float			m_fFudge;
	float			m_fFraction;
};

class DxEffectBlur : public DxEffectBase
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
	static DWORD	m_dwEffect;
	static DWORD	dwBlurDVertexShader[];

	static DWORD	m_dwSavedStateBlock;
	static DWORD	m_dwEffectStateBlock;

	//	Note : �Ӽ�.
	//
protected:
	union
	{
		struct
		{
			BLUR_PROPERTY		m_Property;
		};

		struct
		{
			D3DXVECTOR4		m_vCurrentColor;
			D3DXVECTOR4		m_vPrevColor;
			float			m_fFudge;
			float			m_fFraction;
		};
	};

public:
	void SetProperty ( BLUR_PROPERTY& Property )
	{
		m_Property = Property;
	}
	BLUR_PROPERTY& GetProperty () { return m_Property; }

protected:
	float			m_fTime;

	D3DXMATRIX		m_matOldWorld;

public:
	static HRESULT CreateDevice ( LPDIRECT3DDEVICE9 pd3dDevice );
	static HRESULT ReleaseDevice ( LPDIRECT3DDEVICE9 pd3dDevice );

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

//private:
//	HRESULT MakeEffNormalMeshs ( LPD3DXMESH *pEffectMesh, DxOctreeMesh *pOctreeMesh );

public:
	DxEffectBlur();
	virtual ~DxEffectBlur();
};

#endif // !defined(AFX_DXEFFECTBLUR_H__INCLUDED_)
