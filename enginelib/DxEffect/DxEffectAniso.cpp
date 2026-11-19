// DxEffectAniso.cpp: implementation of the DxEffectBlur class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "DxFrameMesh.h"
#include "DxEffectMan.h"

#include "DxEffectAniso.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define new DEBUG_NEW


const DWORD DxEffectAniso::TYPEID = DEF_EFFECT_ANISOTROPIC;
const DWORD	DxEffectAniso::VERSION = 0x00000100;
const char DxEffectAniso::NAME[] = "[2004]_[ �ֹ��⹰ü ]";
const DWORD DxEffectAniso::FLAG = NULL;

void DxEffectAniso::GetProperty ( PBYTE &pProp, DWORD &dwSize, DWORD &dwVer )
{
	pProp = (PBYTE) &m_Property;
	dwSize = sizeof(ANISO_PROPERTY);
	dwVer = VERSION;
}

void DxEffectAniso::SetProperty ( PBYTE &pProp, DWORD &dwSize, DWORD dwVer )
{
	if ( dwVer==VERSION && dwSize==sizeof(ANISO_PROPERTY) )
	{
		memcpy ( &m_Property, pProp, dwSize );
	}
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

DxEffectAniso::DxEffectAniso() :
	m_pddsTexture(NULL)
{
	strcpy ( m_szTexture, "_Ani_Aniso2.tga" );
}

DxEffectAniso::~DxEffectAniso()
{
	SAFE_DELETE_ARRAY(m_szAdaptFrame);
}

HRESULT DxEffectAniso::CreateDevice ( LPDIRECT3DDEVICE9 pd3dDevice )
{
	HRESULT hr;

	DWORD dwDecl[] =
	{
		D3DVSD_STREAM(0),
		D3DVSD_REG(0, D3DVSDT_FLOAT3 ),			//D3DVSDE_POSITION,  0  
		D3DVSD_REG(3, D3DVSDT_FLOAT3 ),		//D3DVSDE_DIFFUSE,   5  
		D3DVSD_END()
	};

	hr = pd3dDevice->CreateVertexShader ( dwDecl, (DWORD*)dwAnisoVertexShader, &m_dwEffect, 
											DxEffectMan::Instance.GetUseSwShader() );
	if( FAILED(hr) )	return E_FAIL;


	for( UINT which=0; which<2; which++ )
	{
		pd3dDevice->BeginStateBlock();

		//	Note : SetTextureStageState() ����
		//
		pd3dDevice->SetTextureStageState ( 0, D3DTSS_ADDRESSU,	D3DTADDRESS_MIRROR );
		pd3dDevice->SetTextureStageState ( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE );
		pd3dDevice->SetTextureStageState ( 0, D3DTSS_COLOROP,	D3DTOP_MODULATE4X );

		if( which==0 )	pd3dDevice->EndStateBlock( &m_dwSavedStateBlock );
		else			pd3dDevice->EndStateBlock( &m_dwEffectStateBlock );
	}


	return S_OK;
}

HRESULT DxEffectAniso::ReleaseDevice ( LPDIRECT3DDEVICE9 pd3dDevice )
{
	pd3dDevice->DeleteVertexShader ( m_dwEffect );

	if ( pd3dDevice )
	{
		if ( m_dwSavedStateBlock )
			pd3dDevice->DeleteStateBlock( m_dwSavedStateBlock );
		if ( m_dwEffectStateBlock )
			pd3dDevice->DeleteStateBlock( m_dwEffectStateBlock );
	}

	m_dwSavedStateBlock		= NULL;
	m_dwEffectStateBlock	= NULL;

	return S_OK;
}

HRESULT DxEffectAniso::AdaptToDxFrame ( DxFrame *pFrame, LPDIRECT3DDEVICE9 pd3dDevice )
{
	if ( !pFrame ) return S_OK;

	this->pLocalFrameEffNext = pFrame->pEffect;
	pFrame->pEffect = this;

	if ( pFrame->szName )
	{
		SAFE_DELETE_ARRAY(m_szAdaptFrame);
		m_szAdaptFrame = new char[strlen(pFrame->szName)+1];

		strcpy ( m_szAdaptFrame, pFrame->szName );
	}

	return S_OK;
}

HRESULT DxEffectAniso::InitDeviceObjects ( LPDIRECT3DDEVICE9 pd3dDevice )
{
	//	Note : �ؽ����� �б� ������ �����Ѵ�.
	//
	TextureManager::LoadTexture ( m_szTexture, pd3dDevice, m_pddsTexture, 0, 0 );

	if ( !m_pddsTexture )
	{
		char szMsg[256];
		sprintf ( szMsg, "�̹��� ���� '%s' �б⿡ ����", m_szTexture );
		MessageBox ( NULL, szMsg, "ERROR", MB_OK );
	}


	return S_OK;
}

HRESULT DxEffectAniso::DeleteDeviceObjects ()
{
	//	Note : ���� �ؽ��İ� �ε�Ǿ� ���� ��� ����.
	//
	if ( m_pddsTexture )
		TextureManager::ReleaseTexture ( m_pddsTexture );
	m_pddsTexture = NULL;

	return S_OK;
}

HRESULT DxEffectAniso::FrameMove ( float fTime, float fElapsedTime )
{

	return S_OK;
}

HRESULT DxEffectAniso::Render ( DxFrame *pframeCur, LPDIRECT3DDEVICE9 pd3dDevice )
{
	HRESULT hr = S_OK;
	DxMeshes *pmsMeshs;
	DxFrame *pframeChild;

	//	Note : Render 
	//
	if ( pframeCur->pmsMeshs != NULL )
	{
		//	Note : �޽� �׸���.
		//
		pmsMeshs = pframeCur->pmsMeshs;
		while ( pmsMeshs != NULL )
		{
			if ( pmsMeshs->m_pLocalMesh || pmsMeshs->m_pDxOctreeMesh )
			{
				DWORD dwFVFSize;
				DWORD dwVerts, dwFaces;
				LPDIRECT3DINDEXBUFFER9 pIB	= NULL;
				LPDIRECT3DVERTEXBUFFER9 pVB = NULL;

				//	Note : LocalMesh, OctreeMesh �� �� �ϳ��� ������.
				//
				if ( pmsMeshs->m_pLocalMesh )
				{				
					pmsMeshs->m_pLocalMesh->GetIndexBuffer ( &pIB );
					pmsMeshs->m_pLocalMesh->GetVertexBuffer ( &pVB );
					
					dwFVFSize = pmsMeshs->m_pLocalMesh->GetFVF ();
					dwFVFSize = D3DXGetFVFVertexSize ( dwFVFSize );

					dwVerts = pmsMeshs->m_pLocalMesh->GetNumVertices();
					dwFaces = pmsMeshs->m_pLocalMesh->GetNumFaces();
				}
				else
				{
					dwFVFSize = pmsMeshs->m_pDxOctreeMesh->m_dwFVF;
					dwFVFSize = D3DXGetFVFVertexSize ( dwFVFSize );

					dwVerts = pmsMeshs->m_pDxOctreeMesh->m_dwNumVertices;
					dwFaces = pmsMeshs->m_pDxOctreeMesh->m_dwNumFaces;
				}


				//	Note : Shader ��� ����
				//

				//	Note : SetVertexShader�� World�� WorldViewProjection ��� ���� �ֱ�
				//
				D3DXMATRIX	matWorld, matView, matProj, matWVP;

				matView = DxViewPort::Instance.GetMatView();
				matProj = DxViewPort::Instance.GetMatProj();

				D3DXMatrixTranspose( &matWorld, &(pframeCur->matCombined) );
				pd3dDevice->SetVertexShaderConstant ( VSC_MATWORLD,			&matWorld,	4 );

				D3DXMatrixMultiply ( &matWVP, &(pframeCur->matCombined), &matView );
				D3DXMatrixMultiply ( &matWVP, &matWVP, &matProj );
				D3DXMatrixTranspose( &matWVP, &matWVP );
				pd3dDevice->SetVertexShaderConstant ( VSC_MATWORLDVIEWPROJ, &matWVP,	4 );





				//	Note : SetTextureStageState() ����
				//
				pd3dDevice->SetTexture ( 0, m_pddsTexture );

				//	Note : ���� ���� ���.
				//
				pd3dDevice->CaptureStateBlock ( m_dwSavedStateBlock );

				//	Note : ������ ���� ����.
				//
				pd3dDevice->ApplyStateBlock ( m_dwEffectStateBlock );



				//	Note : ����Ʈ���� ���ؽ� ���μ���
				//
				if ( DxEffectMan::Instance.GetUseSwShader() )
					pd3dDevice->SetRenderState( D3DRS_SOFTWAREVERTEXPROCESSING, TRUE );


				//	Note : LocalMesh, OctreeMesh �� �� �ϳ��� ������.
				//
				if ( pmsMeshs->m_pLocalMesh )
				{
					pd3dDevice->SetIndices ( pIB, 0 );
					pd3dDevice->SetStreamSource ( 0, pVB, dwFVFSize );
				}
				else
				{
					pd3dDevice->SetIndices ( pmsMeshs->m_pDxOctreeMesh->m_pIB, 0 );
					pd3dDevice->SetStreamSource ( 0, pmsMeshs->m_pDxOctreeMesh->m_pVB, dwFVFSize );
				}


				pd3dDevice->SetVertexShader ( m_dwEffect );
				

				pd3dDevice->DrawIndexedPrimitive ( D3DPT_TRIANGLELIST, 0, dwVerts, 0, dwFaces );


				//	Note : ����Ʈ���� ���ؽ� ���μ���
				//
				if ( DxEffectMan::Instance.GetUseSwShader() )
					pd3dDevice->SetRenderState( D3DRS_SOFTWAREVERTEXPROCESSING, FALSE );

				pd3dDevice->SetIndices ( NULL, 0 );
				pd3dDevice->SetStreamSource ( 0, NULL, 0 );



				//	Note : �������·� ����.
				//
				pd3dDevice->ApplyStateBlock ( m_dwSavedStateBlock );

				pd3dDevice->SetTexture ( 0, NULL );

				if ( pmsMeshs->m_pLocalMesh )
				{
					if(pIB) pIB->Release ();
					if(pVB) pVB->Release ();
				}
			}
			pmsMeshs = pmsMeshs->pMeshNext;
		}
	}

	//	Note : �ڽ� ������ �׸���.
	//
	pframeChild = pframeCur->pframeFirstChild;
	while ( pframeChild != NULL )
	{
		hr = Render ( pframeChild, pd3dDevice );
		if (FAILED(hr))
			return hr;

		pframeChild = pframeChild->pframeSibling;
	}

	return S_OK;
}