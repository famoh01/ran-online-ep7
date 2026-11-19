// DxEffectFishEye.h: interface for the DxEffectFishEye class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTFISHEYE_H__D67CBC08_57AA_4968_8904_4D3533B7E0A0__INCLUDED_)
#define AFX_DXEFFECTFISHEYE_H__D67CBC08_57AA_4968_8904_4D3533B7E0A0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DxEffectFrame.h"

struct DxFrame;

struct FISHEYE_PROPERTY
{
	float	m_fXZRatio;	
	float	m_fDistortion;
};


class DxEffectFishEye : public DxEffectBase
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
	static DWORD		m_dwEffect;
	static DWORD		dwFisheyeVertexShader[];

	static DWORD		m_dwSavedStateBlock;
	static DWORD		m_dwEffectStateBlock;

	//	Note : �Ӽ�.
	//
protected:
	union
	{
		FISHEYE_PROPERTY	m_Property;

		struct
		{
			float	m_fXZRatio;	
			float	m_fDistortion;
		};
	};

public:
	void SetProperty ( FISHEYE_PROPERTY& Property )
	{
		m_Property = Property;
	}
	FISHEYE_PROPERTY& GetProperty () { return m_Property; }

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

public:
	DxEffectFishEye();
	virtual ~DxEffectFishEye();
};

#endif // !defined(AFX_DXEFFECTFISHEYE_H__D67CBC08_57AA_4968_8904_4D3533B7E0A0__INCLUDED_)
