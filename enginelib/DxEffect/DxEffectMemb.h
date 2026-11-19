// DxEffectMemb.h: interface for the DxEffectMemb class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTMEMB_H__80FC7B1D_5770_4561_B55D_331E7D2CA0A8__INCLUDED_)
#define AFX_DXEFFECTMEMB_H__80FC7B1D_5770_4561_B55D_331E7D2CA0A8__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DxEffectFrame.h"

struct DxFrame;

struct MEMBRANE_PROPERTY
{
	bool	m_bBase;
	bool	m_bMemb;
	float	m_fSpeed;
	char	m_szTexture[MAX_PATH];
};

class DxEffectMemb : public DxEffectBase
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
	static DWORD			m_dwEffect;
	static DWORD			dwMemVertexShader[];

	static DWORD			m_dwEffect2;
	static DWORD			dwMem2VertexShader[];

	static DWORD			m_dwSavedStateBlock;
	static DWORD			m_dwEffectStateBlock;

	//	Note : �Ӽ�.
	//
protected:
	union
	{
		MEMBRANE_PROPERTY	m_Property;

		struct
		{
			bool	m_bBase;
			bool	m_bMemb;
			float	m_fSpeed;
			char	m_szTexture[MAX_PATH];
		};
	};

public:
	void SetProperty ( MEMBRANE_PROPERTY& Property )
	{
		m_Property = Property;
	}
	MEMBRANE_PROPERTY& GetProperty () { return m_Property; }

protected:	
	float				m_fTime;

	float				m_fLoop;
	LPDIRECT3DTEXTURE9	m_pddsTexture;

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
	HRESULT Render ( DxFrame *pFrame, LPDIRECT3DDEVICE9 pd3dDevice, DxFrameMesh *pFrameMesh, DxLandMan* pLandMan );

public:
	DxEffectMemb();
	virtual ~DxEffectMemb();
};

#endif // !defined(AFX_DXEFFECTMEMB_H__80FC7B1D_5770_4561_B55D_331E7D2CA0A8__INCLUDED_)
