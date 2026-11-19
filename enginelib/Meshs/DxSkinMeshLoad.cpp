#include "stdafx.h"

#include "DxMethods.h"
#include "../DxCommon/TextureManager.h"
#include "DxBoneCollector.h"
#include "DxSkinMeshMan.h"

#include "DxSkinMesh.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


//	Note : ��� �������� ���ؽ� ���� ������ �ջ�.
//		����� ȣ��� ��� �������� �ջ�.
//		( �� ��ü�� �������� ã�� ���� ����. )
//
HRESULT CalculateSum (
					SFrame *pframe,			//	����� ���� ������.
					D3DXMATRIX *pmatCur,	//	�θ� �������� Ʈ������ ��Ʈ����.
					D3DXVECTOR3 *pvCenter,	//	���⿡ ���� ���� �ջ�.
					UINT *pcVertices )		//	�ջ�� ������ ���� ����.
{
	HRESULT hr = S_OK;
	PBYTE pbPoints = NULL;
	UINT cVerticesLocal = 0;
	PBYTE pbCur;
	D3DXVECTOR3 *pvCur;
	D3DXVECTOR3 vTransformedCur;
	UINT iPoint;
	SMeshContainer *pmcCur;
	SFrame *pframeCur;
	UINT cVertices;
	D3DXMATRIXA16 matLocal;
    
	//	Note : ���� �������� Ʈ������ ��Ʈ����.
	//
	//D3DXMatrixMultiply ( &matLocal, &pframe->matRot, pmatCur );
	D3DXMatrixMultiply_MM ( matLocal, pframe->matRot, *pmatCur );
    
	pmcCur = pframe->pmcMesh;
	while (pmcCur != NULL)
	{
		//	Note : ���ؽ��� ������ �м��� �޸� �Ҵ緮�� ���.
		//
		DWORD fvfsize = D3DXGetFVFVertexSize ( pmcCur->m_pMesh->GetFVF() );
        
		cVertices = pmcCur->m_pMesh->GetNumVertices();
        
		hr = pmcCur->m_pMesh->LockVertexBuffer(0, &pbPoints);
		if (FAILED(hr))
			goto e_Exit;
        
		for ( iPoint=0, pbCur = pbPoints; iPoint < cVertices; iPoint++, pbCur += fvfsize)
		{
			pvCur = (D3DXVECTOR3*)pbCur;
            
			if ( (pvCur->x != 0.0) || (pvCur->y != 0.0) || (pvCur->z != 0.0) )
			{
				cVerticesLocal++;
                
				//	Note : ���͸� Ʈ������ ���Ŀ� �ջ�.
				//
				D3DXVec3TransformCoord ( &vTransformedCur, pvCur, &matLocal );
                
				pvCenter->x += vTransformedCur.x;
				pvCenter->y += vTransformedCur.y;
				pvCenter->z += vTransformedCur.z;
			}
		}

		pmcCur->m_pMesh->UnlockVertexBuffer();
		pbPoints = NULL;

		pmcCur = pmcCur->m_pmcNext;
	}
    
	//	Note : �� �����ӿ��� �ջ�� ���ͼ��� ����.
	//
	*pcVertices += cVerticesLocal;
    
	//	Note : �ڽ� �������� ����ȣ��� ���� ���� ���� �ջ�.
	//
	pframeCur = pframe->pframeFirstChild;
	while ( pframeCur != NULL )
	{
		hr = CalculateSum ( pframeCur, &matLocal, pvCenter, pcVertices );
		if (FAILED(hr))
			goto e_Exit;
        
		pframeCur = pframeCur->pframeSibling;
	}
    
e_Exit:
	//	Note : ���� �߻��ÿ� ���ؽ� ���� lock�� �Ǿ� ���� ���� ���.
	//
	if ( pbPoints != NULL )
	{
		pmcCur->m_pMesh->UnlockVertexBuffer();
	}
    
	return hr;
}


//	Note : ��ü�� �ݰ��� ����.
//
HRESULT CalculateRadius ( 
						SFrame *pframe,			//	����� ������.
						D3DXMATRIX *pmatCur,	//	�θ� �������� Ʈ������ ��Ʈ����.
						D3DXVECTOR3 *pvCenter,	//	��ü�� �߽���.
                        float *pfRadiusSq)		//	�ݰ�.
{
	HRESULT hr = S_OK;
	PBYTE pbPoints = NULL;
	PBYTE pbCur;
	D3DXVECTOR3 *pvCur;
	D3DXVECTOR3 vDist;;
	UINT iPoint;
	UINT cVertices;
	SMeshContainer *pmcCur;
	SFrame *pframeCur;
	float fRadiusLocalSq;
	float fDistSq;
	D3DXMATRIXA16 matLocal;
    
	//	Note : ���� �������� Ʈ������ ��Ʈ����.
	//
	//D3DXMatrixMultiply ( &matLocal, &pframe->matRot, pmatCur );
	D3DXMatrixMultiply_MM ( matLocal, pframe->matRot, *pmatCur );
    
	pmcCur = pframe->pmcMesh;
	fRadiusLocalSq = *pfRadiusSq;	//	���� �ݰ����� ����.
	while ( pmcCur != NULL )
	{
		//	Note : ���ؽ��� ������ �м��� �޸� �Ҵ緮�� ���.
		//
		DWORD fvfsize = D3DXGetFVFVertexSize ( pmcCur->m_pMesh->GetFVF() );
        
		cVertices = pmcCur->m_pMesh->GetNumVertices();
        
		hr = pmcCur->m_pMesh->LockVertexBuffer ( 0, &pbPoints );
		if (FAILED(hr))
			goto e_Exit;
        
		for ( iPoint=0, pbCur = pbPoints; iPoint < cVertices; iPoint++, pbCur += fvfsize )
		{
			pvCur = (D3DXVECTOR3*)pbCur;
            
			if ( (pvCur->x == 0.0) && (pvCur->y == 0.0) && (pvCur->z == 0.0) )
				continue;
            
			//	Note : ���͸� Ʈ������ ���Ŀ� ��ü �߽��������� �ָ� ������ �ִٸ� �ݰ氪�� ����.
			//
			D3DXVec3TransformCoord ( &vDist, pvCur, &matLocal );
            
			vDist -= *pvCenter;
            
			fDistSq = D3DXVec3LengthSq ( &vDist );
            
			if ( fDistSq > fRadiusLocalSq )
			fRadiusLocalSq = fDistSq;
		}
        
        
		pmcCur->m_pMesh->UnlockVertexBuffer();
		pbPoints = NULL;
        
		pmcCur = pmcCur->m_pmcNext;
	}
    
	//	Note : ������ ã�� �ݰ�����.
	//
	*pfRadiusSq = fRadiusLocalSq;
    
	//	Note : �ڽ� �������� ����ȣ��� �ݰ氪�� ã��.
	//
	pframeCur = pframe->pframeFirstChild;
	while ( pframeCur != NULL )
	{
		hr = CalculateRadius ( pframeCur, &matLocal, pvCenter, pfRadiusSq );
		if (FAILED(hr))
			goto e_Exit;
        
		pframeCur = pframeCur->pframeSibling;
	}
    
e_Exit:
	//	Note : ���� �߻��ÿ� ���ؽ� ���� lock�� �Ǿ� ���� ���� ���.
	//
	if (pbPoints != NULL)
	{
		pmcCur->m_pMesh->UnlockVertexBuffer();
	}
    
	return hr;
}

//	Note : ��ü�� ��豸 ���.
//
HRESULT DxSkinMesh::CalculateBoundingSphere ()
{
	HRESULT hr = S_OK;
	D3DXVECTOR3 vCenter(0,0,0);
	UINT cVertices = 0;
	float fRadiusSq = 0;
	D3DXMATRIXA16 matCur;
    
	//	Note : �߽����� ã�� ���� ��� ���͸� �ջ�. 
	//
	D3DXMatrixIdentity (&matCur);
	hr = CalculateSum ( pframeRoot, &matCur, &vCenter, &cVertices );
	if (FAILED(hr))				goto e_Exit;
    
	if (cVertices > 0)
	{
		//	Note : �߽��� ������.
		//
		vCenter /= (float)cVertices;
        
		//	Note : �ܽ������� �ִ� �Ÿ��� �ִ� ���͸� ã�� �ݰ� ���� �����´�.
		//
		D3DXMatrixIdentity (&matCur);
		hr = CalculateRadius ( pframeRoot, &matCur, &vCenter, &fRadiusSq );
		if (FAILED(hr))			goto e_Exit;
	}
    
	fRadius = (float)sqrt((double)fRadiusSq); // square ���̹Ƿ�.
	vCenter = vCenter;

e_Exit:
	return hr;
}

HRESULT DxSkinMesh::CalculateBoundingBox ( LPDIRECT3DDEVICE9 pd3dDevice, SMeshContainer* pmcMesh, D3DXVECTOR3 &vMax, D3DXVECTOR3 &vMin )
{
	HRESULT hr(S_OK);
	if ( !pmcMesh->m_pSkinMesh )	return E_FAIL;

	DWORD cBones  = pmcMesh->m_pSkinMesh->GetNumBones();

	// set up bone transforms
	for ( DWORD iBone = 0; iBone < cBones; ++iBone )
	{
		//D3DXMatrixMultiply
		//(
		//	&m_pBoneMatrices[iBone],                 // output
		//	&pmcMesh->m_pBoneOffsetMat[iBone], 
		//	pmcMesh->m_pBoneMatrix[iBone]
		//);
		D3DXMatrixMultiply_MM( m_pBoneMatrices[iBone], pmcMesh->m_pBoneOffsetMat[iBone], *pmcMesh->m_pBoneMatrix[iBone] );
	}

	hr = pmcMesh->CalculateBoundingBox ( pd3dDevice, m_pBoneMatrices, vMax, vMin );

	return hr;
}

//	Note : ��ü �����ӵ鿡�� ��Ų�޽��� ������ �ִٸ�
//		�ش� �޽��� �ۿ��ϴ� ��Ʈ���� �����͸� ����.
//
HRESULT DxSkinMesh::FindBones ( SFrame *pframeCur )
{
	HRESULT hr = S_OK;
	SFrame *pframeChild;
	SMeshContainer *pmcMesh;
    
	pmcMesh = pframeCur->pmcMesh;
	while (pmcMesh != NULL)
    {
		if ( pmcMesh->m_pSkinMesh && !pmcMesh->m_szBoneNames.empty() )
		{
			//	Note : �� �޽� �����̳ʿ� ����� '�� �̸�' ������.
			//

			//	NEED : ����� ���� ã�Ƽ� �ش� ��Ʈ���� �����͸� ����.
			//
			for ( DWORD i = 0; i < pmcMesh->m_pSkinMesh->GetNumBones(); ++i )
			{
				DxBoneTrans* pBone = pSkeleton->FindBone ( pmcMesh->m_szBoneNames[i].c_str() );
				if ( !pBone )	return E_FAIL;

				pmcMesh->m_pBoneMatrix[i] = &(pBone->matCombined);
			}
		}
		pmcMesh = pmcMesh->m_pmcNext;
	}
    
	//	Note : �ڽĵ��� ��������� ��� Ž��.
	//
	pframeChild = pframeCur->pframeFirstChild;
	while (pframeChild != NULL)
	{
		hr = FindBones ( pframeChild );
		if (FAILED(hr))
			return hr;
        
		pframeChild = pframeChild->pframeSibling;
	}
    
	return S_OK;
}

//	Note : xFile�� ��� ��Ų�޽��� ����.
//
HRESULT DxSkinMesh::LoadMeshHierarchy ( char *szFile, char *szSkeletonFile, LPDIRECT3DDEVICE9 pd3dDevice, const BOOL bThread )
{
	GASSERT(szFile);
	GASSERT(szSkeletonFile);

	StringCchCopy( m_szName, MAX_PATH, szFile );
	StringCchCopy( m_szSkeleton, MAX_PATH, szSkeletonFile );
	TCHAR* pszFile = m_szName;


	pSkeleton = DxBoneCollector::GetInstance().Load ( m_szSkeleton, pd3dDevice );
	if ( !pSkeleton )	return E_FAIL;


	HRESULT hr = S_OK;
	LPDIRECTXFILE pxofapi = NULL;
	LPDIRECTXFILEENUMOBJECT pxofenum = NULL;
	LPDIRECTXFILEDATA pxofobjCur = NULL;
	DWORD dwOptions;
	int cchFileName;

	if (pszFile == NULL)	return E_INVALIDARG;
    
	//	Note : ��Ʈ ����.
	//
	pframeRoot = new SFrame();
	if ( pframeRoot == NULL )
	{
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	int nStrLen = strlen(__FRAMEROOT)+1;
	pframeRoot->szName = new char[nStrLen];
	StringCchCopy( pframeRoot->szName, nStrLen, __FRAMEROOT );

	dwOptions = 0;
  
	//	Note : xFile ����.
	//
	cchFileName = strlen(pszFile);
	if (cchFileName < 2)
	{
		hr = E_FAIL;
		goto e_Exit;
	}
    
	//	xFile ��ü�� ����.
	hr = DirectXFileCreate(&pxofapi);
	if (FAILED(hr))	goto e_Exit;
    
    //	xFile ���ø�Ʈ ���.
	hr = pxofapi->RegisterTemplates((LPVOID)D3DRM_XTEMPLATES,
		D3DRM_XTEMPLATE_BYTES);
	if (FAILED(hr))	goto e_Exit;
    
	char szPathName[MAX_PATH] = "";
	StringCchCopy( szPathName, MAX_PATH, DxSkinMeshMan::GetInstance().GetPath() );
	StringCchCat( szPathName, MAX_PATH, pszFile );

    //	���� xFile�� ����.
	hr = pxofapi->CreateEnumObject((LPVOID)szPathName,
		DXFILELOAD_FROMFILE,
		&pxofenum);
	if (FAILED(hr))	goto e_Exit;
    
    
	//	Note : �ֻ��� ������Ʈ�� �о��.
	//
	while (SUCCEEDED(pxofenum->GetNextDataObject(&pxofobjCur)))
	{
		hr = LoadFrames ( pxofobjCur, dwOptions, m_dwFVF, pd3dDevice, pframeRoot, bThread );
        
		GXRELEASE(pxofobjCur);
        
		if (FAILED(hr))	goto e_Exit;
	}

    //	Note : ��Ų�޽��� ����� ������ ã��.
	//
	hr = FindBones ( pframeRoot );
	if (FAILED(hr))	goto e_Exit;
    
	//	Note : ��ü�� ��豸 ����.
	//
    hr = CalculateBoundingSphere();
    if (FAILED(hr))	goto e_Exit;
    
	//	Note : ���� Ʈ������ ��Ʈ���� ���� ����� �迭 ����.
	//		( METHOD::SOFTWARE ), �� ��쿡.
	//
	SAFE_DELETE_ARRAY(m_pBoneMatrices);
	m_pBoneMatrices = new D3DXMATRIX[m_maxBones];

e_Exit:
    GXRELEASE(pxofobjCur);
    GXRELEASE(pxofenum);
    GXRELEASE(pxofapi);
    
    if (FAILED(hr))
    {
        SAFE_DELETE(pframeRoot);
    }
    
    return hr;
}

//	Note : �޽��� ����.
//
HRESULT DxSkinMesh::LoadMesh ( LPDIRECTXFILEDATA pxofobjCur,
								DWORD options,
								DWORD fvf,
								LPDIRECT3DDEVICE9 pD3DDevice,
                                SFrame *pframeParent, const BOOL bThread )
{
	HRESULT hr = S_OK;
	SMeshContainer *pmcMesh = NULL;
	LPD3DXBUFFER pbufMaterials = NULL;
	LPD3DXBUFFER pbufAdjacency = NULL;
	DWORD cchName;
	UINT cFaces;
	UINT iMaterial;
	LPDIRECT3DDEVICE9 m_pDevice = pD3DDevice;
	LPDWORD pAdjacencyIn;

	//	Note : �޽� �ܵ��̳� ����.
	//	
	pmcMesh = new SMeshContainer();
	if (pmcMesh == NULL)
	{
		hr = E_OUTOFMEMORY;
		goto e_Exit;
	}

	//	Note : �̸� ������.
	//
	hr = pxofobjCur->GetName(NULL, &cchName);
	if (FAILED(hr))
		goto e_Exit;

	if (cchName > 0)
	{
		pmcMesh->m_szName = new char[cchName];
		if (pmcMesh->m_szName == NULL)
		{
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}
	    
		hr = pxofobjCur->GetName(pmcMesh->m_szName, &cchName);
		if (FAILED(hr))
			goto e_Exit;
	}
	else if( pframeParent->szName )
	{
		cchName = strlen ( pframeParent->szName ) + 1;
		pmcMesh->m_szName = new char[cchName];
		StringCchCopy( pmcMesh->m_szName, cchName, pframeParent->szName );
	}

	//	Note : ��Ų�޽� ����.
	//
	LPD3DXBUFFER pBoneNamesBuf;

	hr = D3DXLoadSkinMeshFromXof ( pxofobjCur, options, pD3DDevice, &pbufAdjacency, &pbufMaterials, &pmcMesh->m_cMaterials,
		&pBoneNamesBuf, &pmcMesh->m_pBoneOffsetBuf, &pmcMesh->m_pSkinMesh );
	if (FAILED(hr))
	{
		if (hr == D3DXERR_LOADEDMESHASNODATA)
			hr = S_OK;
		goto e_Exit;
	}

	if ( pBoneNamesBuf )
	{
		if ( pmcMesh->m_pSkinMesh )
		{
			char** pBoneName = static_cast<char**>(pBoneNamesBuf->GetBufferPointer());

			DWORD dwNum = pmcMesh->m_pSkinMesh->GetNumBones();
			for ( DWORD i=0; i<dwNum; ++i )
			{
				pmcMesh->m_szBoneNames.push_back ( std::string(pBoneName[i]) );
			}
		}

		GXRELEASE(pBoneNamesBuf);
	}

	//	Note : ���̽� ����.
	//
	cFaces = pmcMesh->m_pSkinMesh->GetNumFaces();

	//	Note : ���̽� �ε���.
	//
	pAdjacencyIn = static_cast<LPDWORD> ( pbufAdjacency->GetBufferPointer() );

	pmcMesh->m_rgiAdjacency = new DWORD[cFaces * 3];

	if (pmcMesh->m_rgiAdjacency == NULL)
	{
		hr = E_OUTOFMEMORY;

		goto e_Exit;
	}

	memcpy(pmcMesh->m_rgiAdjacency, pAdjacencyIn, cFaces * 3 * sizeof(DWORD));


	//	Note : �� ��Ų �޽��� ���� ���� ����.
	//
	DWORD dwMeshNumBones = pmcMesh->m_pSkinMesh->GetNumBones();
	if ( dwMeshNumBones )
	{
		//	Note : �޽��� ���� ������ ���� ū���� ����.
		//
		m_maxBones = max(dwMeshNumBones, m_maxBones);
	    
		//	Note : �� ��Ʈ���� ����. �޸� �迭�� �Ҵ��ѵ� ��� �ε��� ���� FindBones ()���� ��Ʈ����
		//		�����͸� ã�Ƴ���.
		//
		pmcMesh->m_pBoneMatrix = new D3DXMATRIX*[dwMeshNumBones];
		if (pmcMesh->m_pBoneMatrix == NULL)		goto e_Exit;

		pmcMesh->m_pBoneOffsetMat = reinterpret_cast<D3DXMATRIX*>(pmcMesh->m_pBoneOffsetBuf->GetBufferPointer());
	    
		//// Normal ���� �������ͷ� ������Ű�� ���� �۾�
		//float fLength	= 1.f;
		//D3DXVECTOR3	vDir(1.f,0.f,0.f);
		//D3DXVECTOR3	vScale(1.f,0.f,0.f);
		//D3DXMATRIX matTemp;
		//for( DWORD i=0; i<dwMeshNumBones; ++i )
		//{
		//	D3DXMatrixInverse( &matTemp, NULL, &pmcMesh->m_pBoneOffsetMat[i] );
		//	D3DXMatrixTranspose( &matTemp, &matTemp );
		//	D3DXVec3TransformNormal( &vScale, &vDir, &matTemp );
		//	fLength = D3DXVec3Length( &vScale );
		//	fLength = 1.f/fLength;
		//	pmcMesh->m_pBoneOffsetMat[i] *= fLength;
		//}

		//	Note : GenerateMesh
		//
		hr = pmcMesh->GenerateMesh ( DxSkinMesh::m_method );
		if (FAILED(hr))		goto e_Exit;
	}
	else
	{
		//	Note : ��Ų�޽��� �ƴ� ��� �Ϲ� �޽���.
		//
		pmcMesh->m_pSkinMesh->GetOriginalMesh(&(pmcMesh->m_pMesh));
		pmcMesh->m_pSkinMesh->Release();
		pmcMesh->m_pSkinMesh = NULL;
		pmcMesh->m_cpattr = pmcMesh->m_cMaterials;

		//	Note : GenerateMesh
		//
		hr = pmcMesh->GenerateMesh ( pbufAdjacency );
		if (FAILED(hr))		goto e_Exit;
	}

	//	Note : ���͸����� ���ٸ� �⺻ ���͸��� ����.
	//
	if ((pbufMaterials == NULL) || (pmcMesh->m_cMaterials == 0))
	{
		pmcMesh->m_pMaterials = new DXMATERIAL[1];

		if ( pmcMesh->m_pMaterials == NULL )
		{
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}
	    
		memset ( &pmcMesh->m_pMaterials[0].d3dMaterial, 0, sizeof(D3DMATERIAL8) );
		pmcMesh->m_pMaterials[0].d3dMaterial.Diffuse.r = 0.5f;
		pmcMesh->m_pMaterials[0].d3dMaterial.Diffuse.g = 0.5f;
		pmcMesh->m_pMaterials[0].d3dMaterial.Diffuse.b = 0.5f;
		pmcMesh->m_pMaterials[0].d3dMaterial.Specular = pmcMesh->m_pMaterials[0].d3dMaterial.Diffuse;
		pmcMesh->m_pMaterials[0].d3dMaterial.Power = 14.f;	// 30.f - ����Ʈ

		pmcMesh->m_pMaterials[0].pTexture = NULL;
	}
	//	Note : ���͸��� ���� ���� ���.
	//
	else
	{
		pmcMesh->m_pMaterials = new DXMATERIAL[pmcMesh->m_cMaterials];

		if ( pmcMesh->m_pMaterials == NULL )
		{
			hr = E_OUTOFMEMORY;
			goto e_Exit;
		}

		LPD3DXMATERIAL pMaterials = (LPD3DXMATERIAL)pbufMaterials->GetBufferPointer();
	    
		for (iMaterial = 0; iMaterial < pmcMesh->m_cMaterials; ++iMaterial)
		{
			//	Note : ���͸��� �� ����.
			//
			pmcMesh->m_pMaterials[iMaterial].d3dMaterial = pMaterials[iMaterial].MatD3D;
			pmcMesh->m_pMaterials[iMaterial].d3dMaterial.Power = 14.f;	// 30.f - ����Ʈ

			//	Note : Ambient ���� Diffuse ���� ���� ���ش�.
			pmcMesh->m_pMaterials[iMaterial].d3dMaterial.Ambient = pmcMesh->m_pMaterials[iMaterial].d3dMaterial.Diffuse;
	        
			//	Note : �ؽ��� �ε�.
			//
			pmcMesh->m_pMaterials[iMaterial].pTexture = NULL;
			if (pMaterials[iMaterial].pTextureFilename != NULL)
			{
				pmcMesh->m_pMaterials[iMaterial].strTexture = std::string(pMaterials[iMaterial].pTextureFilename);

				hr = TextureManager::LoadTexture( pMaterials[iMaterial].pTextureFilename,
										m_pDevice, pmcMesh->m_pMaterials[iMaterial].pTexture, 0, 2, bThread );
				if (FAILED(hr))
				{
					hr = S_OK;
					pmcMesh->m_pMaterials[iMaterial].pTexture = NULL;
				}

				if( pmcMesh->m_pMaterials[iMaterial].pTexture )
				{
					//	Note : �ؽ��� �б⿡ �����ÿ� �� ���͸����� �ʱ� ���� ����.
					//
					TextureManager::EM_TEXTYPE emType = TextureManager::GetTexType( pMaterials[iMaterial].pTextureFilename );

					if( emType==TextureManager::EMTT_ALPHA_HARD )			pmcMesh->m_pMaterials[iMaterial].dwFlags = DXMATERIAL::FLAG_ALPHA_HARD;
					else if( emType==TextureManager::EMTT_ALPHA_SOFT )		pmcMesh->m_pMaterials[iMaterial].dwFlags = DXMATERIAL::FLAG_ALPHA_SOFT;
					else if( emType==TextureManager::EMTT_ALPHA_SOFT01 )	pmcMesh->m_pMaterials[iMaterial].dwFlags = DXMATERIAL::FLAG_ALPHA_SOFT;
					else if( emType==TextureManager::EMTT_ALPHA_SOFT02 )	pmcMesh->m_pMaterials[iMaterial].dwFlags = DXMATERIAL::FLAG_ALPHA_SOFT;
				}
			}
		}
	}

	//	Note : ���ο� �޽� �����̳ʸ� ����.
	//
	pframeParent->AddMesh (pmcMesh );	//	SFrame�� ���.
	AddMeshGlobal ( pmcMesh );			//	DxSkinMesh�� ���.
	pmcMesh = NULL;
   
e_Exit:
    SAFE_DELETE(pmcMesh);
    
    GXRELEASE(pbufAdjacency);
    GXRELEASE(pbufMaterials);

    return hr;
}


//	Note : Frame ( �޽�, Ʈ������, ���ϸ��̼� ) �� ����.
//
HRESULT DxSkinMesh::LoadFrames ( LPDIRECTXFILEDATA pxofobjCur,
								DWORD options,
								DWORD fvf,
								LPDIRECT3DDEVICE9 pD3DDevice,
                                SFrame *pframeParent, const BOOL bThread )
{
    HRESULT hr = S_OK;
    LPDIRECTXFILEDATA pxofobjChild = NULL;
    LPDIRECTXFILEOBJECT pxofChild = NULL;
    const GUID *type;
    DWORD cbSize;
    D3DXMATRIX *pmatNew;
    SFrame *pframeCur;
    DWORD cchName;
    
    //	Note : ������Ʈ�� Ÿ��.
	//
    hr = pxofobjCur->GetType(&type);
    if (FAILED(hr))
        goto e_Exit;
    
    //	Note : �޽�.
	//
    if (*type == TID_D3DRMMesh)
    {
		SFrame* pframe = NULL;
		if ( pframeParent->szName == NULL )
		{
			pframe = FindParentFrame ( pframeParent );
			if ( pframe && pframe->szName != NULL )
			{
				char *szMesh = "[Mesh]";
				size_t nStrLen = strlen(pframe->szName)+strlen(szMesh)+1;
				pframeParent->szName = new char[ nStrLen ];
				memset( pframeParent->szName, 0, sizeof(char)*nStrLen );

				StringCchCat( pframeParent->szName, nStrLen, pframe->szName );
				StringCchCat( pframeParent->szName, nStrLen, szMesh );
			}
		}

		hr = LoadMesh( pxofobjCur, options, fvf, pD3DDevice, pframeParent, bThread );
        if (FAILED(hr))		goto e_Exit;

		if ( pframe )
		{
			//	Note : �̰��� MAX ���� ó��.
			//		Ʈ������ �����Ӱ� �޽��� ��� �ִ� �������� ������ �������� ��ȿ�ϰ� �������
			//		�ڵ��̴�.
			//		����, �ٸ��� �����Ǿ� �ִ°� ���Ǹ� ������ �߻��� �� �ִ�.
			//
			//	m_pmcMesh->m_szBoneNames[0];	//	[LOCAL]
			//	m_pmcMesh->m_szBoneNames[1];	//	[LOCAL]
			//	m_pmcMesh->m_szBoneNames[2];	//	[COMBINED] <-- �̰͸� ������ BONE���� ������ �޴°�.

			GASSERT(pframeParent);
			if ( pframeParent->pmcMesh && !pframeParent->pmcMesh->m_pSkinMesh )
			{
				SFrame* pBoneFrame = FindParentFrame ( pframe );
				if ( pBoneFrame )
				{
					DxBoneTrans* pBoneTrans = pSkeleton->FindBone ( pBoneFrame->szName );

					if ( pBoneTrans )
					{
						pframeParent->pmcMesh->m_pBoneMatrix = new LPD3DXMATRIX[3];
						pframeParent->pmcMesh->m_numBoneComb = 3;

						pframeParent->pmcMesh->m_szBoneNames.push_back ( std::string("[LOCAL]") );
						pframeParent->pmcMesh->m_pBoneMatrix[0] = &pframeParent->matRot;

						pframeParent->pmcMesh->m_szBoneNames.push_back ( std::string("[LOCAL]") );
						pframeParent->pmcMesh->m_pBoneMatrix[1] = &pframe->matRot;

						pframeParent->pmcMesh->m_szBoneNames.push_back ( std::string(pBoneFrame->szName) );
						pframeParent->pmcMesh->m_pBoneMatrix[2] = &pBoneTrans->matCombined;
					}
				}
			}
		}
	}
    //	Note : Ʈ������.
	//
    else if (*type == TID_D3DRMFrameTransformMatrix)
    {
        hr = pxofobjCur->GetData ( NULL, &cbSize, (PVOID*)&pmatNew );
        if (FAILED(hr))
            goto e_Exit;
        
        // update the parents matrix with the new one
        pframeParent->matRot = *pmatNew;
        pframeParent->matRotOrig = *pmatNew;
    }
	//	Note : ���� ������.
	//
    else if (*type == TID_D3DRMFrame)
    {
		char *szframeName = NULL;
        
		//	Note : �������� �̸� ������.
		//
        hr = pxofobjCur->GetName(NULL, &cchName);
        if (FAILED(hr))
            goto e_Exit;
        
        if (cchName > 0)
        {
            szframeName = new char[cchName];
            if (szframeName == NULL)
            {
                hr = E_OUTOFMEMORY;
                goto e_Exit;
            }
            
            hr = pxofobjCur->GetName(szframeName, &cchName);
            if (FAILED(hr))
                goto e_Exit;
        }

		if ( !strcmp(pframeParent->szName,__FRAMEROOT) )
		{
			pframeCur = pframeParent;

			//	Note : __FRAMEROOT �̸��� �����ϰ� ���� ������ �̸��� ����.
			//
			delete[] pframeCur->szName;
			pframeCur->szName = szframeName;
		}
		else
		{
       		//	Note : �� ������ ����.
			//
			pframeCur = new SFrame();
			if (pframeCur == NULL)
			{
				hr = E_OUTOFMEMORY;
				goto e_Exit;
			}

			pframeCur->szName = szframeName;

			pframeParent->AddFrame(pframeCur);
		}
        
		//	Note : ���������� Ž���ϸ鼭 ���ϸ��̼��� �о�´�.
		//		QueryInterface()�� ����Ͽ� ������Ʈ�� Ÿ���� Ȯ����.
		//
        while ( SUCCEEDED(pxofobjCur->GetNextObject(&pxofChild)) )
        {
            hr = pxofChild->QueryInterface(IID_IDirectXFileData,
                (LPVOID *)&pxofobjChild);

            if (SUCCEEDED(hr))
            {
                hr = LoadFrames( pxofobjChild, options, fvf, pD3DDevice, pframeCur, bThread );
                if (FAILED(hr))
                    goto e_Exit;
                
                GXRELEASE(pxofobjChild);
            }
            
            GXRELEASE(pxofChild);
        }
        
    }
    
e_Exit:
    GXRELEASE(pxofobjChild);
    GXRELEASE(pxofChild);
    return hr;
}
