#include "stdafx.h"

#include "DxNaviMesh.h"
#include "DxFrameMesh.h"
#include "DxCustomTypes.h"
#include "DxInputDevice.h"
#include "SerialFile.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define new DEBUG_NEW

DxNaviMesh	g_DxNaviMesh;

HRESULT DxNaviMesh::InitDeviceObjects( LPDIRECT3DDEVICE9 pd3dDevice )
{
	m_pd3dDevice = pd3dDevice;

	return LoadDxFrameNaviMesh();
}


HRESULT	DxNaviMesh::Update(void)
{
	static	DWORD	Frame = 0;

	if ( !(Frame % 330) )
	{
        Follow();		
	}
	Frame++;


	PathObject.Update();
	ControlObject.Update();

	return S_OK;
}

HRESULT	DxNaviMesh::Render ( void )
{
	PathObject.Render(m_pd3dDevice);
	ControlObject.Render(m_pd3dDevice);
	NaviMesh.Render(m_pd3dDevice);

	return S_OK;
}

HRESULT	DxNaviMesh::DeleteDeviceObjects(void)
{
	NaviMesh.Clear();
	PathObject.Create ( NULL, D3DXVECTOR3(0.0f, 0.0f, 0.0f), -1 );
	ControlObject.Create ( NULL, D3DXVECTOR3(0.0f, 0.0f, 0.0f), -1 );	

	return S_OK;
}


HRESULT	DxNaviMesh::LoadDxFrameNaviMesh ()
{
	HRESULT hr = E_FAIL;

//	<-- DxFrameMesh�� NavigationMesh ��������
    DxFrameMesh* pRawFrameMesh = new DxFrameMesh();
	
	//	Note : ���� ������ ����.
	//
	pRawFrameMesh->CleanUp ();	

	//	Note : ���� x���ϰ� ���ؽ� ���� ���� ( FVF ) �� �ش�.
	//
//	pRawFrameMesh->SetFile ( "e:/smtm_c/_dbin/xFiles/neschool.x" );
	pRawFrameMesh->SetFile ( "e:/smtm_c/_dbin/xFiles/nav.x" );
//	pRawFrameMesh->SetFile ( "e:/smtm_c/_dbin/xFiles/under.x" );

	pRawFrameMesh->SetFVF ( D3DFVF_XYZ );	

	//	Note : �ʱ�ȭ �Ѵ�.
	//
	if ( FAILED ( pRawFrameMesh->Create ( m_pd3dDevice ) ) )
	{
		return E_FAIL;
	}
	pRawFrameMesh->UpdateFrames();
//	--> DxFrameMesh�� NavigationMesh ��������
//	DxFrame* pFrameMesh = pRawFrameMesh->FindFrame ( "Frame514_endnevi_Layer1" );
	DxFrame* pFrameMesh = pRawFrameMesh->FindFrame ( "Line01s[Mesh]" );
//	DxFrame* pFrameMesh = pRawFrameMesh->FindFrame ( "nav[Mesh]" );
	pRawFrameMesh->SetCollisionDetectedFrame ( pFrameMesh );

	//	<--	Navigation Mesh ����Ÿ ����
	//		*�׺���̼� �޽� ������ ���� DxFrame�� �Ķ���ͷ� �ѱ��.*
	//		*������ �߻��ϸ� �ٷ� ����...*
//	hr = NaviMesh.CreateNaviMesh( pFrameMesh );
//	if ( FAILED ( hr ) )
//	{
//		SAFE_DELETE ( pRawFrameMesh );
//		return E_FAIL;
//	}
	//	-->	Navigation Mesh ����Ÿ ����
	SAFE_DELETE ( pRawFrameMesh );

	//	<--	ĳ����, ���� �����.
	PathObject.Create(&NaviMesh, D3DXVECTOR3(0.0f, 0.0f, 0.0f),-1);
	ControlObject.Create(&NaviMesh, D3DXVECTOR3(0.0f, 0.0f, 0.0f),-1);
	//	-->	ĳ����, ���� �����.
	
	//	<--	ĳ����, ���� �ӵ��� 
	ControlObject.SetMaxSpeed ( 1.0f );
	PathObject.SetMaxSpeed ( 0.9f );
	//	-->	ĳ����, ���� �����.

	return S_OK;
}


void DxNaviMesh::IsCollision ( const D3DXVECTOR3 &vP1, const D3DXVECTOR3 &vP2 )
{
	BOOL bCollision = FALSE;

	D3DXVECTOR3		vDest;
	DWORD			CollisionID;

	D3DXVECTOR3 vFromPt = vP1;
	D3DXVECTOR3	vTargetPt = vP2;
	
	NavigationMesh*	pNaviMesh = ControlObject.GetParentMesh ();
	pNaviMesh->IsCollision ( vFromPt, vTargetPt, vDest, CollisionID, bCollision );
	
	if ( bCollision )
	{		
		ControlObject.GotoLocation ( vDest, CollisionID );
		CDebugSet::ToView ( "Ȯ����  �� - %d", CollisionID );
	}
	//	<--	������ �κ��� ����� ���
	else
	{	
//		vDest = ( vFromPt + vTargetPt * 2 )  / 3;
//		vDest.y = ControlObject.Position().y;
//		DWORD	CellID;
//		vDest = pNaviMesh->SnapPointToMesh ( &CellID,vDest );		
//		ControlObject.GotoLocation ( vDest, CellID );
//		CDebugSet::ToView ( "������� - %d", CollisionID );
	}
	//	-->
}

void	DxNaviMesh::Follow ()
{
	PathObject.GotoLocation ( ControlObject.Position(), ControlObject.CurrentCellID() );
}


void	DxNaviMesh::SaveFile ( )
{
	CSerialFile	SFile;
	if ( !SFile.OpenFile ( FOT_WRITE, "TestNaviSaveLoad.set" ) )
	{
		return;
	}
	NaviMesh.SaveFile ( SFile );
	SFile.CloseFile ();
}

void	DxNaviMesh::LoadFile (  )
{
	NaviMesh.Clear ();

	PathObject.Create ( NULL, D3DXVECTOR3(0.0f, 0.0f, 0.0f), -1 );
	ControlObject.Create ( NULL, D3DXVECTOR3(0.0f, 0.0f, 0.0f), -1 );	
	
	CSerialFile SFile;
	if ( !SFile.OpenFile ( FOT_READ, "TestNaviSaveLoad.set" ) )
	{
		return;
	}
//	NaviMesh.LoadFile ( SFile );
	SFile.CloseFile();

	//	<--	ĳ����, ���� �����.
	PathObject.Create(&NaviMesh, D3DXVECTOR3(0.0f, 0.0f, 0.0f),-1);
	ControlObject.Create(&NaviMesh, D3DXVECTOR3(0.0f, 0.0f, 0.0f),-1);
	//	-->	ĳ����, ���� �����.
	
	//	<--	ĳ����, ���� �ӵ��� 
	ControlObject.SetMaxSpeed ( 1.0f );
	PathObject.SetMaxSpeed ( 0.9f );
	//	-->	ĳ����, ���� �����.
}