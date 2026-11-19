// SoundSourceManagerDoc.cpp : CSoundSourceManagerDoc Ŭ������ ����
//

#include "stdafx.h"
#include "SoundSourceManager.h"

#include "SoundSourceManagerDoc.h"
#include "MainFrm.h"

#include <d3d9types.h>
#include "SerialFile.h"
#include "SoundSourceMan.h"
#include ".\soundsourcemanagerdoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// CSoundSourceManagerDoc

IMPLEMENT_DYNCREATE(CSoundSourceManagerDoc, CDocument)

BEGIN_MESSAGE_MAP(CSoundSourceManagerDoc, CDocument)
END_MESSAGE_MAP()


// CSoundSourceManagerDoc ����/�Ҹ�

CSoundSourceManagerDoc::CSoundSourceManagerDoc()
{
	// TODO: ���⿡ ��ȸ�� ���� �ڵ带 �߰��մϴ�.
}

CSoundSourceManagerDoc::~CSoundSourceManagerDoc()
{
}

BOOL CSoundSourceManagerDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: ���⿡ �ٽ� �ʱ�ȭ �ڵ带 �߰��մϴ�.
	// SDI ������ �� ������ �ٽ� ����մϴ�.
	return TRUE;
}

// CSoundSourceManagerDoc serialization

void CSoundSourceManagerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: ���⿡ ���� �ڵ带 �߰��մϴ�.
	}
	else
	{
		// TODO: ���⿡ �ε� �ڵ带 �߰��մϴ�.
	}
}


// CSoundSourceManagerDoc ����

#ifdef _DEBUG
void CSoundSourceManagerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CSoundSourceManagerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CSoundSourceManagerDoc ����
//
//BOOL	CSoundSourceManagerDoc::DefaultSaveFile ()
//{
//	CString	FullPath = m_pSoundSourceMan->GetDataDirectory() + g_szFileName;
//	return m_pSoundSourceMan->SaveFile ( FullPath.GetString() );
//}
//
//BOOL	CSoundSourceManagerDoc::DefaultLoadFile ()
//{
//	CString	FullPath = m_pSoundSourceMan->GetDataDirectory() + g_szFileName;
//	return m_pSoundSourceMan->LoadFile ( FullPath.GetString() );
//}



