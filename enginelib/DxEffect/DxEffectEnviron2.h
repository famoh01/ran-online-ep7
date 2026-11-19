// DxEffectEnviron2t.h: interface for the DxEffectShadowHW class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_DXEFFECTENVIRON2_H__INCLUDED_)
#define AFX_DXEFFECTENVIRON2_H__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "DxEffectFrame.h"

struct DxFrame;

struct SHADOW_PROPERTY
{
	WORD			m_wEnvCase;					// ����

	float			m_fLoop;					// ���� ȿ��
	float			m_fLoopAdd;
	float			m_fDistance;
	float			m_fMultiply;
	char			m_szTexture[MAX_PATH];

	float			m_fDotPower;
	char			m_szTexDot3[MAX_PATH];		// ���� ������

	float			m_fWidth;					// ���� ����
	float			m_fHeight;
	char			m_szTexDetail[MAX_PATH];

	DWORD			m_dwBlendAlpha;				// �ݻ�
	char			m_szTexture1[MAX_PATH];
	char			m_szTexture2[MAX_PATH];
	char			m_szTexture3[MAX_PATH];
	char			m_szTexture4[MAX_PATH];
	char			m_szTexture5[MAX_PATH];
	char			m_szTexture6[MAX_PATH];
};


class DxEffectEnviron2 : public DxEffectBase
{
	//	Note : ����Ʈ Ÿ�� ����.
	//
public:
	const static DWORD	TYPEID;
	const static char	NAME[];

public:
	virtual DWORD GetTypeID ()		{ return TYPEID; }
	virtual const char* GetName ()	{ return NAME; }

	//	Note : ���̴�
	//
protected:	
	static DWORD	m_dwEffReflect;
	static DWORD	m_dwEffCloudReflect;

	static char		m_strEffReflect[];
	static char		m_strEffCloudReflect[];

	static DWORD	m_dwSavedStateBlock;
	static DWORD	m_dwEffectStateBlock;


	//	Note : �Ӽ�.
	//
protected:
	union
	{
		SHADOW_PROPERTY	m_Property;

		struct
		{
			WORD			m_wEnvCase;					// ����

			float			m_fLoop;					// ���� ȿ��
			float			m_fLoopAdd;
			float			m_fDistance;
			float			m_fMultiply;
			char			m_szTexture[MAX_PATH];

			float			m_fDotPower;
			char			m_szTexDot3[MAX_PATH];		// ���� ������

			float			m_fWidth;					// ���� ����
			float			m_fHeight;
			char			m_szTexDetail[MAX_PATH];

			DWORD			m_dwBlendAlpha;				// �ݻ�
			char			m_szTexture1[MAX_PATH];
			char			m_szTexture2[MAX_PATH];
			char			m_szTexture3[MAX_PATH];
			char			m_szTexture4[MAX_PATH];
			char			m_szTexture5[MAX_PATH];
			char			m_szTexture6[MAX_PATH];
		};
	};

public:
	void SetProperty ( SHADOW_PROPERTY& Property )
	{
		m_Property = Property;
	}
	SHADOW_PROPERTY& GetProperty () { return m_Property; }

protected:
	float				m_fTime;

	LPDIRECT3DTEXTURE9		m_pCloudTexture;	// ���� �׸���	 // ���� �ؽ���
	LPDIRECT3DTEXTURE9		m_pDot3Texture;		// ���� ������	 // Dot3 �ؽ���
	LPDIRECT3DTEXTURE9		m_pDetailTexture;		// ��ü �Ҹ� ������	 // Dot3 �ؽ���
	LPDIRECT3DCUBETEXTURE8	m_pCubeTexture;		// �ݻ�			// ť�� �ؽ���

//	WORD			m_wEnvCase;

protected:	

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
	DxEffectEnviron2();
	virtual ~DxEffectEnviron2();
};

#endif // !defined(AFX_DXEFFECTENVIRON2_H__INCLUDED_)
