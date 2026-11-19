//
//	[struct DXMATERIAL], JDH, (2002.12.02)
//		���͸���� �ؽ���, ���͸���(�ؽ���) �Ӽ����� ������ �ִ� ����ü.
//		�̵� �����ʹ� ���� ���� ��찡 �����Ƿ� �̷��� ó����.
//
#pragma once
#include <string>

struct DXMATERIAL
{
	enum
	{
		FLAG_ALPHA_HARD		= 0x0001,
		FLAG_ALPHA_SOFT		= 0x0002,
	};

	DWORD				dwFlags;
	LPDIRECT3DTEXTUREQ	pTexture;
	std::string			strTexture;
	D3DMATERIALQ		d3dMaterial;	// legacy code (DxSkinMeshLoad) expects a material block

	DXMATERIAL () :
		dwFlags(NULL),
		pTexture(NULL)
	{
		strTexture = "";
		memset(&d3dMaterial, 0, sizeof(d3dMaterial));
	}

	void RELEASE ();
};

struct DXMATERIAL_SPECULAR
{
	BOOL				bSpecUse;
	LPDIRECT3DTEXTUREQ	pSpecTex;
	char				szTexture[MAX_PATH];
	char				szSpecTex[MAX_PATH];

	DXMATERIAL_SPECULAR () :
		bSpecUse(FALSE),
		pSpecTex(NULL)
	{
		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
		memset(szSpecTex, 0, sizeof(char) * (MAX_PATH));
	}

	~DXMATERIAL_SPECULAR();
};

struct DXMATERIAL_CHAR_EFF_100
{
	BOOL				bEffUse;
	float				fMaterial_Power;
	LPDIRECT3DTEXTUREQ	pEffTex;
	char				szTexture[MAX_PATH];
	char				szEffTex[MAX_PATH];

	DXMATERIAL_CHAR_EFF_100()
		: pEffTex(NULL)
	{
		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
		memset(szEffTex, 0, sizeof(char) * (MAX_PATH));
	}
};

struct DXMATERIAL_CHAR_EFF
{
	BOOL				bEffUse;
	D3DMATERIALQ		d3dMaterial;
	LPDIRECT3DTEXTUREQ	pEffTex;
	char				szTexture[MAX_PATH];
	char				szEffTex[MAX_PATH];

	DXMATERIAL_CHAR_EFF() :
		bEffUse(FALSE),
		pEffTex(NULL)
	{
		d3dMaterial.Diffuse.r	= 1.f;
		d3dMaterial.Diffuse.g	= 1.f;
		d3dMaterial.Diffuse.b	= 1.f;
		d3dMaterial.Diffuse.a	= 1.f;
		d3dMaterial.Ambient		= d3dMaterial.Diffuse;
		d3dMaterial.Specular	= d3dMaterial.Diffuse;
		d3dMaterial.Emissive	= d3dMaterial.Diffuse;
		d3dMaterial.Power		= 14.f;

		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
		memset(szEffTex, 0, sizeof(char) * (MAX_PATH));
	}

	~DXMATERIAL_CHAR_EFF();
};

//
//struct DXMATERIAL_CHAR_EFF
//{
//	BOOL				bEffUse;
//	D3DMATERIALQ		d3dMaterial;
//	DWORD				dwDiffuse;		// Optimize - ����ȭ ������ ���δ�.
//	LPDIRECT3DTEXTUREQ	pEffTex;
//	char				szTexture[MAX_PATH];
//	char				szEffTex[MAX_PATH];
//
//	DXMATERIAL_CHAR_EFF () :
//		bEffUse(FALSE),
//		pEffTex(NULL)
//	{
//		d3dMaterial.Diffuse.r	= 1.f;
//		d3dMaterial.Diffuse.g	= 1.f;
//		d3dMaterial.Diffuse.b	= 1.f;
//		d3dMaterial.Diffuse.a	= 1.f;
//		d3dMaterial.Ambient		= d3dMaterial.Diffuse;
//		d3dMaterial.Specular	= d3dMaterial.Diffuse;
//		d3dMaterial.Emissive	= d3dMaterial.Diffuse;
//		d3dMaterial.Power		= 14.f;
//
//		memset(szTexture, 0, sizeof(char) * (MAX_PATH));
//		memset(szEffTex, 0, sizeof(char) * (MAX_PATH));
//	}
//
//	~DXMATERIAL_CHAR_EFF();
//};

