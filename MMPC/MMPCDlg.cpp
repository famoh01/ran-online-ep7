// MMPCDlg.cpp : ���� ����
//

#include "stdafx.h"
#include "MMPC.h"
#include "MMPCDlg.h"
#include ".\mmpcdlg.h"

#include "BitmapMan.h"

inline int ROUND( float fValue)
{
	return static_cast<int>( fValue + ( fValue > 0 ? 0.5 : -0.5 ) );
}

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ���� ���α׷� ������ ���Ǵ� CAboutDlg ��ȭ �����Դϴ�.

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// ��ȭ ���� ������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ����

// ����
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CMMPCDlg ��ȭ ����



CMMPCDlg::CMMPCDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CMMPCDlg::IDD, pParent)
	, m_nLX(0)
	, m_nTY(0)
	, m_nRX(0)
	, m_nBY(0)
	, m_nSizeX(0)
	, m_nSizeY(0)
	, m_nCalcLX(0)
	, m_nCalcBY(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMMPCDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_FILENAME, m_csFileName);
	DDX_Text( pDX, IDC_LX, m_nLX );
	DDX_Text( pDX, IDC_TY, m_nTY );
	DDX_Text( pDX, IDC_RX, m_nRX );
	DDX_Text( pDX, IDC_BY, m_nBY );
	DDX_Control(pDX, IDCANCEL, m_ctrlButtonCancel);
	DDX_Control(pDX, IDC_VIEW, m_ctrlEditView);
	DDX_Control(pDX, IDC_HIDE, m_ctrlButtonHide);
	DDX_Control(pDX, IDC_FILENAME2, m_csDDSFileName);
}

BEGIN_MESSAGE_MAP(CMMPCDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton1)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
	ON_BN_CLICKED(IDC_HIDE, OnBnClickedHide)
//	ON_WM_TIMER()
ON_BN_CLICKED(IDC_BUTTON2, OnBnClickedButton2)
END_MESSAGE_MAP()


// CMMPCDlg �޽��� ó����

BOOL CMMPCDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// �ý��� �޴��� "����..." �޴� �׸��� �߰��մϴ�.

	// IDM_ABOUTBOX�� �ý��� ���� ������ �־�� �մϴ�.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// �� ��ȭ ������ �������� �����մϴ�. ���� ���α׷��� �� â�� ��ȭ ���ڰ� �ƴ� ��쿡��
	// �����ӿ�ũ�� �� �۾��� �ڵ����� �����մϴ�.
	SetIcon(m_hIcon, TRUE);			// ū �������� �����մϴ�.
	SetIcon(m_hIcon, FALSE);		// ���� �������� �����մϴ�.

	// TODO: ���⿡ �߰� �ʱ�ȭ �۾��� �߰��մϴ�.
	HideEditView(); // �Ʒ� ��Ʈ���� ����
	
	return TRUE;  // ��Ʈ�ѿ� ���� ��Ŀ���� �������� ���� ��� TRUE�� ��ȯ�մϴ�.
}

void CMMPCDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// ��ȭ ���ڿ� �ּ�ȭ ���߸� �߰��� ��� �������� �׸����� 
// �Ʒ� �ڵ尡 �ʿ��մϴ�. ����/�� ���� ����ϴ� MFC ���� ���α׷��� ��쿡��
// �����ӿ�ũ���� �� �۾��� �ڵ����� �����մϴ�.

void CMMPCDlg::OnPaint() 
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
HCURSOR CMMPCDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CMMPCDlg::OnBnClickedButton1()
{
	// TODO: BMP ���� ����
	CString szFilter = "��Ʈ�� ����(*.bmp)|*.bmp|";

	CFileDialog dlg( TRUE, ".bmp", NULL, OFN_HIDEREADONLY, szFilter );

	if ( dlg.DoModal() == IDOK )
	{
		CString strPathName( dlg.GetPathName() );
		CBitmapMan::m_cBitmapMan.LOAD( strPathName );

		m_csFileName.SetWindowText( dlg.GetPathName() );
	}
}

void CMMPCDlg::OnBnClickedOk()
{
	// TODO: MMP ���� ����
	CString szFilter = "MMP ����(*.mmp)|*.mmp|";

	CFileDialog dlg( FALSE, ".mmp", NULL, OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT, szFilter );

	if ( dlg.DoModal() == IDOK )
	{
		BeginWaitCursor();
		{
			// �����͹ڽ��� �ִ� ���� ���´�.
			UpdateData();

			// BMP�� �·Ḧ ���´�.
			RECT rect = GetBitmapPosition();

			// ��ǥ ���
			MiniMapPositionCalc( rect );
		}
		EndWaitCursor();

		CString strBUFFER;

		// ���� ����
		if( !MiniMapPositionSave( dlg.GetPathName(), strBUFFER ) )
		{
			// ���� ����
			AfxMessageBox( "MMP ���� ������ �����Ͽ����ϴ�." );
		}
		else
		{
			ShowEditView( strBUFFER );
		}
	}

	//OnOK();
}

void CMMPCDlg::ShowEditView( CString & strBUFFER )
{
	RECT rtCancel, rtMain, rtHide;

	SetDlgItemText( IDC_VIEW, strBUFFER );

	if( m_ctrlButtonHide.IsWindowVisible() == TRUE ) return; // �̹� �����ͺ䰡 ���̸� ũ������ �����ʴ´�.

	m_ctrlButtonCancel.GetWindowRect( &rtCancel );
	m_ctrlButtonHide.GetWindowRect( &rtHide );
	GetWindowRect( &rtMain );

	int nGap = rtHide.bottom - rtCancel.bottom;

	MoveWindow( rtMain.left, rtMain.top,
				rtMain.right - rtMain.left, rtMain.bottom - rtMain.top + nGap, TRUE );

	m_ctrlButtonHide.ShowWindow( SW_SHOW );
}

void CMMPCDlg::HideEditView()
{
	RECT rtCancel, rtMain, rtHide;

	m_ctrlButtonCancel.GetWindowRect( &rtCancel );
	m_ctrlButtonHide.GetWindowRect( &rtHide );
	GetWindowRect( &rtMain );

	int nGap = rtHide.bottom - rtCancel.bottom;

	MoveWindow( rtMain.left, rtMain.top,
				rtMain.right - rtMain.left, rtMain.bottom - rtMain.top - nGap, TRUE );

	m_ctrlButtonHide.ShowWindow( SW_HIDE );
}

void CMMPCDlg::InitEditView()
{
	RECT rect, destRect;

	GetWindowRect( &destRect );
	m_ctrlButtonCancel.GetWindowRect( &rect );
	ScreenToClient( &rect );

	destRect.bottom = rect.bottom + 18 + GetSystemMetrics(SM_CYCAPTION);
	MoveWindow( &destRect, FALSE );
}

BOOL CMMPCDlg::MiniMapPositionSave( CString & strPathName, CString & strBUFFER )
{
	CFile file;

	if( !file.Open( strPathName, CFile::modeCreate|CFile::modeWrite) )
		return FALSE;

	BITMAP sBitmap = CBitmapMan::m_cBitmapMan.GetBitmap();

	strBUFFER.Format( "MINIMAPNAME\t\t%s\r\n", m_strDDSFileName );
	strBUFFER.AppendFormat( "MAPSIZE_X\t\t%d %d\r\n", m_nSizeX, m_nCalcLX );
	strBUFFER.AppendFormat( "MAPSIZE_Y\t\t%d %d\r\n", m_nSizeY, m_nCalcBY );
	strBUFFER.AppendFormat( "TEXTURE_SIZE\t\t%d %d\r\n", sBitmap.bmWidth, sBitmap.bmHeight );
	strBUFFER.AppendFormat( "TEXTURE_POS\t\t%d %d %d %d", 0, 0, sBitmap.bmWidth, sBitmap.bmHeight ); 

	file.Write( strBUFFER, strBUFFER.GetLength() ); // ���� ����

	file.Close();

	return TRUE;
}

void CMMPCDlg::MiniMapPositionCalc( RECT & rect )
{
	float fRealSizeX = static_cast<float>(m_nRX - m_nLX);
	float fRealSizeY = static_cast<float>(m_nTY - m_nBY);

	BITMAP sBitmap = CBitmapMan::m_cBitmapMan.GetBitmap();

	m_nSizeX = ROUND( ( fRealSizeX * sBitmap.bmWidth ) / ( rect.right - rect.left ) );
	m_nCalcLX = m_nLX - ROUND( (float)m_nSizeX * rect.left / sBitmap.bmWidth );

	m_nSizeY = ROUND( ( fRealSizeY * sBitmap.bmHeight ) / ( rect.bottom - rect.top ) );
	m_nCalcBY = m_nBY - ROUND( (float)m_nSizeY * rect.top / sBitmap.bmHeight );
}

RECT CMMPCDlg::GetBitmapPosition()
{
	CClientDC WinDC(this);

	CDC MemDC;
	MemDC.CreateCompatibleDC( &WinDC );

	BITMAP sBitmap = CBitmapMan::m_cBitmapMan.GetBitmap();
	HBITMAP hBitmap = CBitmapMan::m_cBitmapMan.GetHBitmap();

	MemDC.SelectObject( (HBITMAP)hBitmap ); // ���� ������Ʈ�� �����ߴٰ� ���߿� �����ϴ°� ������.

	///////////////////////////////
	INT nLEFT(0);
	INT nTOP(0);
	INT nRIGHT(sBitmap.bmWidth-1);
	INT nBOTTOM(sBitmap.bmHeight-1);

	BOOL bOut(FALSE);
	///////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////
	COLORREF colorRef = GetPixel(MemDC, nLEFT, nTOP);
	for(int i=nTOP; i<=nBOTTOM && !bOut; ++i) // ���� ������ �˻��Ѵ�.
	{
		for(int j=nLEFT; j<=nRIGHT; ++j)
		{
			COLORREF color = GetPixel(MemDC, j, i);
			if(colorRef != color )
			{
				nTOP = i;
				bOut = TRUE;
				break;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	bOut = FALSE;
	colorRef = GetPixel(MemDC, nLEFT, nBOTTOM);
	for(int i=nBOTTOM; i>=nTOP && !bOut; --i) // �Ʒ��� ������ �˻��Ѵ�.
	{
		for(int j=nLEFT; j<=nRIGHT; ++j)
		{
			COLORREF color = GetPixel(MemDC, j, i);
			if(colorRef != color)
			{
				nBOTTOM = i;
				bOut = TRUE;
				break;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	bOut = FALSE;
	colorRef = GetPixel(MemDC, nLEFT, nTOP);
	for(int i=nLEFT; i<=nRIGHT && !bOut; ++i) // ���� ������ �˻��Ѵ�.
	{
		for(int j=nTOP; j<=nBOTTOM; ++j)
		{
			COLORREF color = GetPixel(MemDC, i, j);
			if(colorRef != color)
			{
				nLEFT = i;
				bOut = TRUE;
				break;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	bOut = FALSE;
	colorRef = GetPixel(MemDC, nRIGHT, nTOP);
	for(int i=nRIGHT; i>=nLEFT && !bOut; --i) // ������ ������ �˻��Ѵ�.
	{
		for(int j=nTOP; j<=nBOTTOM; ++j)
		{
			COLORREF color = GetPixel(MemDC, i, j);
			if(colorRef != color)
			{
				nRIGHT = i;
				bOut = TRUE;
				break;
			}
		}
	}

	RECT rect;

	rect.left = nLEFT;
	rect.top = nTOP;
	rect.right = nRIGHT;
	rect.bottom = nBOTTOM;

	return rect;
}

void CMMPCDlg::OnBnClickedHide()
{
	HideEditView();
}

void CMMPCDlg::OnBnClickedButton2()
{
	// TODO: DDS ���� ����
	CString szFilter = "DDS ����(*.dds)|*.dds|";

	CFileDialog dlg( TRUE, ".dds", NULL, OFN_HIDEREADONLY, szFilter );

	if ( dlg.DoModal() == IDOK )
	{
		CString strPathName( dlg.GetPathName() );
		m_csDDSFileName.SetWindowText( dlg.GetPathName() );

		m_strDDSFileName = dlg.GetFileName();
	}
}
