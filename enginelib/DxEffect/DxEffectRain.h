// DxEffectRain.h: interface for the DxEffectRain class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTRAIN_H__E41B1A51_5B95_4F90_8CA9_EE2F3C02A92E__INCLUDED_)
#define AFX_DXEFFECTRAIN_H__E41B1A51_5B95_4F90_8CA9_EE2F3C02A92E__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
 
#include "DxEffectFrame.h"

struct DxFrame;

struct RAIN_PROPERTY
{
	float		m_fAlpha;		//	����� ���� ������ ��
	float		m_fScale;		//	����� ũ��
	float		m_fRainRate;	//	������ ��Ÿ���� ����� ����   ( 0.f - 0��, 0.5f - 0.5��, 1.f - 1�� )
	char		m_szTexture[MAX_PATH];
};


class DxEffectRain : public DxEffectBase
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
	static char				m_strEffect[];

	static DWORD			m_dwSavedStateBlock;
	static DWORD			m_dwEffectStateBlock;

	//	Note : �Ӽ�.
	//
protected:
	union
	{
		RAIN_PROPERTY	m_Property;

		struct
		{
			float		m_fAlpha;		//	����� ���� ������ ��
			float		m_fScale;		//	����� ũ��
			float		m_fRainRate;	//	������ ��Ÿ���� ����� ����   ( 0.f - 0��, 0.5f - 0.5��, 1.f - 1�� )
			char		m_szTexture[MAX_PATH];
		};
	};

public:
	void SetProperty ( RAIN_PROPERTY& Property )
	{
		m_Property = Property;
	}
	RAIN_PROPERTY& GetProperty () { return m_Property; }

protected:
	float		m_fTime;

	LPDIRECT3DTEXTURE9	m_pddsTexture;

	struct VERTEX
	{
		D3DXVECTOR3 p;       // vertex position
		D3DXVECTOR3 n;       // vertex normal
	};

	struct RAINVERTEX
	{
		D3DXVECTOR3 p;
		float		loop; // Loop point
		D3DXVECTOR3 n;
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
private:
	HRESULT MakeEffRainMeshs ( LPDIRECT3DDEVICE9 pd3dDevice, LPD3DXMESH *pEffectMesh );
	float	CalcSurfaceArea(VERTEX* pFV[3]);

public:
	DxEffectRain();
	virtual ~DxEffectRain();
};

#endif // !defined(AFX_DXEFFECTRAIN_H__E41B1A51_5B95_4F90_8CA9_EE2F3C02A92E__INCLUDED_)
