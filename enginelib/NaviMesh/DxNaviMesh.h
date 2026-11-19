#pragma once

#ifndef	__DX_NAVI_MESH__
#define	__DX_NAVI_MESH__

#include "actor.h"

struct	DxFrame;
class DxNaviMesh
{
public:
	DxNaviMesh() {};
	virtual	~DxNaviMesh() {};

	HRESULT InitDeviceObjects(LPDIRECT3DDEVICE9 pd3dDevice);	
	HRESULT	Update(void);
	HRESULT	Render(void);
	HRESULT DeleteDeviceObjects();	

	//	<--	�޽�� �ۼ�
	//		*�޽��� �ָ� �װ��� ������ ��� NavigationMesh Ŭ������
	//		����ϴ� �޽�� �ۼ�*
	//		*�̹� ���ϴ� DxFrame�� �Ѿ�´ٰ� ������*
	//		*���Ἲ �˻� - Ʈ���̾ޱ� �븻�� ���� 90���� �Ѿ���� üũ - �� �����Ѵ�.
	//		����, �ִٸ� E_FAIL, ���ٸ� S_OK�� ��ȯ�Ѵ�.*
	HRESULT	CreateNaviMesh ( DxFrame *pNaviFrame );
	//	-->	�޽�� �ۼ�

	//	<--	���콺�� �̿��� ĳ���� �̵�
	void	IsCollision ( const D3DXVECTOR3 &vP1, const D3DXVECTOR3 &vP2 );
	void	Follow ();
	void	GotoError ();// { NaviMesh.GotoErrorPosition(  ); }
	//	-->	���콺�� �̿��� ĳ���� �̵�

	void	SaveFile ( );
	void	LoadFile ( );


	//	<--	�ӽ� �޽�� �� ������
protected:	
	HRESULT	LoadDxFrameNaviMesh ();

private:
	LPDIRECT3DDEVICE9			m_pd3dDevice;
	Actor PathObject;		// red cone that does the pathfinding
	Actor ControlObject;	// blue cone controlled by the player	
	NavigationMesh NaviMesh;// our navigation mesh
	//	-->	�ӽ� �޽�� �� ������
};

#endif	//	__DX_NAVI_MESH__