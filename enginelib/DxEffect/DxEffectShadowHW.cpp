// DxEffectShadowHW.cpp: implementation of the DxEffectShadowHW class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "DxFrameMesh.h"
#include "DxEffectMan.h"
#include "DxSkinChar.h"

#include "DxEffectShadowHW.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define new DEBUG_NEW

const DWORD					DxEffectShadowHW::VERTEX::FVF			= D3DFVF_XYZ|D3DFVF_TEX1;
const float					DxEffectShadowHW::m_fRange				= 5.f;
LPDIRECT3DVERTEXBUFFER9		DxEffectShadowHW::m_pShadowVB			= NULL;
DWORD						DxEffectShadowHW::m_dwSavedStateBlock	= NULL;
DWORD						DxEffectShadowHW::m_dwEffectStateBlock	= NULL;


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DxEffectShadowHW::DxEffectShadowHW() :
	m_fDistanceRate(4.0f),
	m_fHeight(0.0f),			// �׸��ڰ� �ٴڿ� �帮�� ���� �ٴ� ��� ����
	m_vShadowPos(D3DXVECTOR3(0.f,0.f,0.f))
{
}

DxEffectShadowHW::~DxEffectShadowHW()
{
}
