// RanOnlienInstallerDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "RanOnlienInstaller.h"
#include "RanOnlienInstallerDlg.h"
#include "HttpPatchThread.h"
#include "s_CHttpPatch.h"
#include "LogControl.h"

#include <knownfolders.h>
// FOLDERID_LocalAppDataLow now comes from shell32.lib/knownfolders.h
// const CLSID FOLDERID_LocalAppDataLow = { 0xA520A1A4, 0x1780, 0x4FF6 ,{ 0xBD, 0x18, 0x16, 0x73, 0x43, 0xC5, 0xAF, 0x16}};

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CRanOnlienInstallerDlg ��ȭ ����



CRanOnlienInstallerDlg::CRanOnlienInstallerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CRanOnlienInstallerDlg::IDD, pParent)
	, m_bAutoExecute(FALSE)
	, m_bForceTerminate ( FALSE )
	, m_pThread ( NULL )
	, m_ULBefore ( 0UL )
	, m_dwBps ( 0 )
	, m_dwBeforeTime ( 0 )
	, m_dwCntTime ( 0 )
	, m_pHttpPatch ( NULL )
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRanOnlienInstallerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EXECUTE_BUTTON, m_ctrlExecute);
	DDX_Control(pDX, IDC_START_BUTTON, m_ctrlStart);
	DDX_Control(pDX, IDC_PROGRESS_ALL, m_ctrlProgressAll);
	DDX_Check(pDX, IDC_READYTOSTART, m_bAutoExecute);
}

BEGIN_MESSAGE_MAP(CRanOnlienInstallerDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_EXECUTE_BUTTON, OnBnClickedExecuteButton)
	ON_BN_CLICKED(IDC_START_BUTTON, OnBnClickedStartButton)
	ON_BN_CLICKED(IDC_CLOSE_BUTTON, OnBnClickedCloseButton)
	ON_WM_TIMER()
END_MESSAGE_MAP()


// CRanOnlienInstallerDlg �޽��� ó����

BOOL CRanOnlienInstallerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// �� ��ȭ ������ �������� �����մϴ�. ���� ���α׷��� �� â�� ��ȭ ���ڰ� �ƴ� ��쿡��
	// �����ӿ�ũ�� �� �۾��� �ڵ����� �����մϴ�.
	SetIcon(m_hIcon, TRUE);			// ū �������� �����մϴ�.
	SetIcon(m_hIcon, FALSE);		// ���� �������� �����մϴ�.

	// TODO: ���⿡ �߰� �ʱ�ȭ �۾��� �߰��մϴ�.

	CWinApp *pApp = AfxGetApp();
	if( !pApp )	return FALSE;

	m_DownloadURL = pApp->m_lpCmdLine;

	if ( m_DownloadURL.IsEmpty() )
	{
		OnCancel();
		return FALSE;
	}

	m_ctrlExecute.EnableWindow( FALSE );

	TCHAR szPROFILE[MAX_PATH] = {0};

	SHGetSpecialFolderPath( NULL, szPROFILE, CSIDL_PERSONAL, FALSE );
	
	m_strFileName = m_DownloadURL;

	int index = m_strFileName.ReverseFind ( '/' );
	index = m_strFileName.GetLength() - index - 1 ;

	m_strFileName = m_strFileName.Right( index );

	m_strSavePath += szPROFILE;
	m_strSavePath += "\\";

	m_pHttpPatch = new CHttpPatch;
	//	�������� ��� ����
	m_pHttpPatch->SetForceTerminateToggle ( &m_bForceTerminate );

	//	�����Ȳ ����
	ULONGLONG* pCurPos = NULL;
	ULONGLONG* pCurEnd = NULL;
	NS_LOG_CONTROL::GetProcessCurPositionAddr ( &pCurPos, &pCurEnd );	
	m_pHttpPatch->SetProgressValue ( pCurPos, pCurEnd );

	SetDlgItemText( IDC_DOWNPLACE_STATIC, m_strSavePath + m_strFileName );
	
	return TRUE;  // ��Ʈ�ѿ� ���� ��Ŀ���� �������� ���� ��� TRUE�� ��ȯ�մϴ�.
}

// ��ȭ ���ڿ� �ּ�ȭ ���߸� �߰��� ��� �������� �׸����� 
// �Ʒ� �ڵ尡 �ʿ��մϴ�. ����/�� ���� ����ϴ� MFC ���� ���α׷��� ��쿡��
// �����ӿ�ũ���� �� �۾��� �ڵ����� �����մϴ�.

void CRanOnlienInstallerDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // �׸��⸦ ���� ����̽� ���ؽ�Ʈ

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Ŭ���̾�Ʈ �簢������ �������� ����� ����ϴ�.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// �������� �׸��ϴ�.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// ����ڰ� �ּ�ȭ�� â�� ���� ���ȿ� Ŀ���� ǥ�õǵ��� �ý��ۿ���
//  �� �Լ��� ȣ���մϴ�. 
HCURSOR CRanOnlienInstallerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CRanOnlienInstallerDlg::OnBnClickedExecuteButton()
{
	// TODO: ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
	EXECUTE_INSTALL();
}

void CRanOnlienInstallerDlg::OnBnClickedStartButton()
{
	// TODO: ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
	CFileDialog dlg( FALSE, (LPCTSTR)".*", m_strFileName.GetString(), OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,
		NULL, this );

	dlg.m_ofn.lpstrInitialDir = m_strSavePath;
	if ( dlg.DoModal() != IDOK )
	{
		return;
	}

	m_strSavePath = dlg.GetPathName();

	m_strSavePath = m_strSavePath.Left ( m_strSavePath.ReverseFind ( '\\' ) + 1 );


	m_strFileName = dlg.GetFileName();	

	// �ٿ�ε� ����

	SetDlgItemText( IDC_DOWNPLACE_STATIC, m_strSavePath + m_strFileName );

	m_ULBefore = 0UL;
	m_dwBps = 0;
	m_dwBeforeTime = 0;
	m_dwCntTime = 0;


	BEGIN_DOWNLOAD();

	return;
}

void CRanOnlienInstallerDlg::OnBnClickedCloseButton()
{
	// TODO: ���⿡ ��Ʈ�� �˸� ó���� �ڵ带 �߰��մϴ�.
	m_bForceTerminate = TRUE; // ���� �ߴ�
	if ( m_pThread )	m_pThread->KillThread(); // ������ �ߴ�

	OnOK();
}

void CRanOnlienInstallerDlg::OnTimer(UINT nIDEvent)
{
	// TODO: ���⿡ �޽��� ó���� �ڵ带 �߰� ��/�Ǵ� �⺻���� ȣ���մϴ�.
	switch( nIDEvent )
	{
		case E_START_PATCH:
		{
			UpdateControls(); // ���α׷����� ���� ������Ʈ
			
			if ( m_pThread->IsForceTerminate() )	//	Canceled
			{
				KillTimer ( E_START_PATCH );
			}
			else if ( !m_pThread->IsRunning() )	//	Not Running
			{
				KillTimer ( E_START_PATCH );

				if ( !m_pThread->IsFail () )	//	Failed
				{
					END_DOWNLOAD ();
				}
			}			
		}
		break;

	}

	CDialog::OnTimer(nIDEvent);
}

void CRanOnlienInstallerDlg::UpdateControls()
{
	static const ULONGLONG UL100 = 100UL;
	static const ULONGLONG UL1 = 1UL;
    ULONGLONG Pos = 0;
	ULONGLONG End = 0;
	
	int	AllPercent= 0;	
	
	NS_LOG_CONTROL::GetProcessCurPosition ( &Pos, &End );	
	End = ( !End )? UL1 : End;
	AllPercent = int( (Pos*UL100) / End);


	// ������� ���� �뷮 
	CString strTemp;	
	float dwPos = static_cast<float>((double)Pos / ( 1024 * 1024 ));
	float dwEnd = static_cast<float>((double)End / ( 1024 * 1024 ));
	strTemp.Format( "%0.2f MB / %0.2f MB ", dwPos, dwEnd );

	SetDlgItemText ( IDC_STATE_STATIC, strTemp );


	// �ٿ�ε� �ӵ�
	if ( m_dwBeforeTime == 0 ) 
	{
		m_dwBeforeTime = GetTickCount();
	}
	else
	{
		DWORD dwCurrentTime = GetTickCount();
		m_dwCntTime += dwCurrentTime - m_dwBeforeTime;
		m_dwBeforeTime = dwCurrentTime;
       
		if ( m_dwCntTime > 1000 )
		{
			ULONGLONG ULBps = Pos - m_ULBefore;
			
			CString strTemp2;
			
			if ( ULBps > (1024 * 1024 )) // MB
			{
				m_dwBps = static_cast<float>( (double)ULBps / (1024*1024) );
				strTemp2.Format( "%0.2f MB / �� ", m_dwBps );
			}
			else // KB
			{
				m_dwBps = static_cast<float>( (double)ULBps / (1024) );
				strTemp2.Format( "%0.2f KB / �� ", m_dwBps );
			}	
			
			
			m_dwCntTime = 0;
			m_ULBefore = Pos;

			SetDlgItemText ( IDC_SPEED_STATIC, strTemp2 );
		}

	}

    
	m_ctrlProgressAll.SetPos ( AllPercent );
}

void CRanOnlienInstallerDlg::BEGIN_DOWNLOAD()
{
	m_ctrlStart.EnableWindow( FALSE );

	CString strTemp = m_strSavePath + m_strFileName;
	m_pThread = new CHttpPatchThread( m_pHttpPatch, m_DownloadURL.GetString(), strTemp.GetString() , AfxGetThread()->m_nThreadID );

	if (m_pThread == NULL)
		return;

	ASSERT_VALID(m_pThread);
	m_pThread->m_pThreadParams = NULL;

	if ( !m_pThread->CreateThread(CREATE_SUSPENDED) )
	{
		delete m_pThread;
		return;
	}

	VERIFY(m_pThread->SetThreadPriority(THREAD_PRIORITY_IDLE));
	m_pThread->ResumeThread();

	SetTimer ( E_START_PATCH, 30, NULL ); // ������ ������ Ÿ�̸� �۵��� ������...?
}

void CRanOnlienInstallerDlg::END_DOWNLOAD()
{
	m_ctrlProgressAll.SetPos ( 100 );
	m_ctrlExecute.EnableWindow( TRUE );

	if ( !m_pThread->IsFail() ) // ���а� �ƴϸ�...
	{
		UpdateData( TRUE );
		if ( m_bAutoExecute )
		{
			EXECUTE_INSTALL();
		}
	}

}

void CRanOnlienInstallerDlg::EXECUTE_INSTALL()
{
	CString strTemp;

	strTemp = m_strSavePath;
	strTemp += m_strFileName;

	int result = (int)ShellExecute( NULL , "open", strTemp.GetBuffer() ,NULL, NULL, SW_SHOW);

	if( 32 > result )
	{
		MessageBox ( "CreateProcess InstallProgram", "ERROR", MB_OK|MB_ICONEXCLAMATION );
		return;
	}

	OnOK();
}

BOOL CRanOnlienInstallerDlg::IsVista()
{
	OSVERSIONINFO osver;

	osver.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	
	if (	::GetVersionEx( &osver ) && 
			osver.dwPlatformId == VER_PLATFORM_WIN32_NT && 
			(osver.dwMajorVersion >= 6 ) )
		return TRUE;

	return FALSE;
}

void CRanOnlienInstallerDlg::OnKillThread()
{
	if ( !m_pThread ) return;
		
	VERIFY(SetEvent(m_pThread->m_hEventKill));
	
	// wait for all threads to finish shutdown
	BOOL bThreadsLeft(TRUE);

	while (bThreadsLeft)
	{
		Sleep( 1 );// 200ms for every 100 threads
		bThreadsLeft = FALSE;
		
		if (WaitForSingleObject(m_pThread->m_hEventDead, 0) == WAIT_TIMEOUT)
			bThreadsLeft = TRUE;
	}

	VERIFY(WaitForSingleObject(m_pThread->m_hThread, INFINITE) == WAIT_OBJECT_0);
	delete m_pThread;
}

void CRanOnlienInstallerDlg::PostNcDestroy()
{
	// TODO: ���⿡ Ư��ȭ�� �ڵ带 �߰� ��/�Ǵ� �⺻ Ŭ������ ȣ���մϴ�.
	if( !m_bForceTerminate )
	{
		OnKillThread(); // �����ư�� �������� �ʾҴٸ� ������ ����
	}

	SAFE_DELETE ( m_pHttpPatch );

	CDialog::PostNcDestroy();
}
