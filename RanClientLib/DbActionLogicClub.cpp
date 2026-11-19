#include "stdafx.h"
#include "./DbActionLogic.h"

#include "../NetServerLib/s_CAgentServer.h"
#include "../NetServerLib/s_CFieldServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/**
* ���ο� Ŭ���� �����Ѵ�.
* \param strClubName : Ŭ���̸�
* \param dwChaNum : Ŭ���� �����Ϸ��� ĳ���� ��ȣ (�� ĳ���Ͱ� Ŭ���� �����Ͱ� �ȴ�)
* \return -1 : �̹� ��帶���ͷ� ��ϵǾ� ���� �����Ұ�.
*         -2 : �������� �����߻� (�ߺ��ȱ���̸�)
*         0 ���� Ŭ�� : ������ ����ȣ��
*/
CCreateClub::CCreateClub(
	DWORD dwClientID,
	CString strClubName,
	DWORD dwChaNum )
{
	m_dwClient = dwClientID;
    m_strClubName = strClubName;
    m_dwChaNum = dwChaNum;
}

int CCreateClub::Execute(
	CServer* pServer )
{
    int nRETCODE = m_pDbManager->CreateClub(m_strClubName, m_dwChaNum);    
	
	GLMSG::SNET_CLUB_NEW_DB2AGT NetMsgDb2Agt;
	NetMsgDb2Agt.dwMasterID = m_dwChaNum;
	NetMsgDb2Agt.dwClubID = 0;
	StringCchCopy ( NetMsgDb2Agt.szClubName, CHAR_SZNAME, m_strClubName.GetString() );

	switch ( nRETCODE )
	{
	case -1:
		NetMsgDb2Agt.emFB = EMCLUB_NEW_FB_ALREADY;
		break;

	case -2:
		NetMsgDb2Agt.emFB = EMCLUB_NEW_FB_NAME;
		break;

	case CLUB_NULL:
		NetMsgDb2Agt.emFB = EMCLUB_NEW_FB_FAIL;
		break;

	default:
		NetMsgDb2Agt.emFB = EMCLUB_NEW_FB_OK;
		NetMsgDb2Agt.dwClubID = nRETCODE;
		break;
	};

	CAgentServer* pTemp = reinterpret_cast<CAgentServer*> (pServer);
	pTemp->InsertMsg ( m_dwClient, (char*) &NetMsgDb2Agt );

	return nRETCODE;
}

/**
* Ŭ���� �����Ѵ�.
* \param dwClub : Ŭ����ȣ
* \param dwChaNum : Ŭ���� �����Ϸ��� ĳ���͹�ȣ(ĳ���� ��ȣ�� Ŭ�� �����Ͱ� �ƴϸ� ������ ���� �ʴ´�)
* \return 0 : ��� ���� ����
*         1 : ��� ���� ����
*/
CDeleteClub::CDeleteClub(
	DWORD dwClub, 
	DWORD dwChaNum)
	: m_dwClub( dwClub )
	, m_dwChaNum( dwChaNum )
{
}

int CDeleteClub::Execute(
	CServer* pServer )
{
    return m_pDbManager->DeleteClub(m_dwClub, m_dwChaNum);
}

/**
* Ŭ����ũ�� �����Ѵ�.
* \param dwClub : Ŭ����ȣ
* \param dwRank : ��ŷ
*/
CSetClubRank::CSetClubRank(
	DWORD dwClub,
	DWORD dwRank )
	: m_dwClub( dwClub )
	, m_dwRank( dwRank )
{   
}
 
int CSetClubRank::Execute(CServer* pServer)
{
    return m_pDbManager->SetClubRank(m_dwClub, m_dwRank);
}

/**
* Ŭ����ũ �̹����� �����Ѵ�.
* \param dwClub : Ŭ����ȣ
* \param dwMarkVer : ��ũ����
* \param pData : ������ ������
* \param nSize : ������ size
*/
CWriteClubMarkImage::CWriteClubMarkImage(
	DWORD dwClub,
	DWORD dwMarkVer,
	BYTE* pData,
	int nSize) 
	: m_nSize(0)
	, m_dwMarkVer(0)
	, m_dwClub(0)
{
	if (pData != NULL)
	{
		int nMaxSize = (int) (sizeof(DWORD)*EMCLUB_MARK_SX*EMCLUB_MARK_SY);

		m_dwClub = dwClub;
		m_dwMarkVer = dwMarkVer;
	    
		if ( nSize <= 0 || nSize > nMaxSize )
		{
			m_nSize = 0;
		}
		else
		{
			m_nSize = nSize;
			memcpy ( m_aryMark, pData, nSize );
		}
	}
}

CWriteClubMarkImage::~CWriteClubMarkImage()
{    
}

int CWriteClubMarkImage::Execute(CServer* pServer)
{
    if (m_nSize > 0)
        return m_pDbManager->WriteClubMarkImage(m_dwClub, m_dwMarkVer, m_aryMark, m_nSize);
    else
        return DB_ERROR;
}

/**
* Ŭ�� ��ü�ð��� �����Ѵ�.
* dwClub : Ŭ����ȣ
* tDiss : ��ü�ð�
*/
CSetClubDissolutionTime::CSetClubDissolutionTime(
	DWORD dwClub,
	__time64_t tDiss )
	: m_dwClub( dwClub )
{    
    m_tDiss = tDiss;
}

int CSetClubDissolutionTime::Execute(
	CServer* pServer )
{
    return m_pDbManager->SetClubDissolutionTime(m_dwClub, m_tDiss);
}

/**
* Ŭ���� ����Ż�� Ȥ�� �����ð��� ���Ѵ�.
* \param dwClub Ŭ����ȣ
* \param tSec �ð�
* \return 
*/
CSetClubAllianceSec::CSetClubAllianceSec(
	DWORD dwClub,
	__time64_t tSec )
	: m_dwClub( dwClub )
{
	m_tSec   = tSec;
}

int CSetClubAllianceSec::Execute(
	CServer* pServer )
{
	return m_pDbManager->SetClubAllianceSec( m_dwClub, m_tSec );
}

/**
* Ŭ���� �����ػ� �ð��� ���Ѵ�.
* \param dwClub Ŭ����ȣ
* \param tDis �ػ�ð�
* \return 
*/
CSetClubAllianceDis::CSetClubAllianceDis(
	DWORD dwClub,
	__time64_t tDis )
	: m_dwClub( dwClub )
{
	m_tDis   = tDis;
}
    
int CSetClubAllianceDis::Execute(
	CServer* pServer )
{
	return m_pDbManager->SetClubAllianceDis( m_dwClub, m_tDis );
}


/**
* Ŭ���� ������ ���� �ð��� �����Ѵ�.
* \param dwClub Ŭ����ȣ
* \param tDis ������ ���� �ð�
* \return 
*/
CSetClubAuthorityTime::CSetClubAuthorityTime(
	DWORD dwClub,
	__time64_t tAuthority )
	: m_dwClub( dwClub )
{
	m_tAuthority   = tAuthority;
}
    
int CSetClubAuthorityTime::Execute(
	CServer* pServer )
{
	return m_pDbManager->SetClubAuthorityTime( m_dwClub, m_tAuthority );
}



/**
* ���ο� ĳ���͸� Ŭ���� ���Խ�Ų��
* \param dwClub : Ŭ����ȣ
* \param dwChaNum : ���Խ�Ű���� ĳ���͹�ȣ
*/
CAddClubMember::CAddClubMember(
	DWORD dwClub,
	DWORD dwChaNum )
	: m_dwClub( dwClub )
	, m_dwChaNum( dwChaNum )
{    
}

int CAddClubMember::Execute(CServer* pServer)
{
    return m_pDbManager->AddClubMember(m_dwClub, m_dwChaNum);
}

/**
* ���ԵǾ� �ִ� ĳ���͸� Ŭ������ Ż���Ų��
* \param dwChaNum : ĳ���͹�ȣ
*/
CDeleteClubMember::CDeleteClubMember(
	DWORD dwChaNum )
	: m_dwChaNum( dwChaNum )
{
}

int CDeleteClubMember::Execute(
	CServer* pServer )
{
    return m_pDbManager->DeleteClubMember(m_dwChaNum);
}

/**
* �ش�Ŭ���� �����ݾ��� �����Ѵ�. (Storage)
* \param dwClub : Ŭ����ȣ
* \param llMoney : �����ݾ� (�ݾ��� >= 0)
*/
CSetClubMoney::CSetClubMoney(
	DWORD dwClub,
	LONGLONG llMoney )
	: m_dwClub( dwClub )
	, m_llMoney( llMoney )
{	
}

int CSetClubMoney::Execute(
	CServer* pServer )
{
	return m_pDbManager->SetClubMoney( m_dwClub, m_llMoney );
}

/**
* �ش�Ŭ���� ���Աݾ��� �����Ѵ�. (Income money)
* \param dwClub : Ŭ����ȣ
* \param llMoney : ���Աݾ� (�ݾ��� >= 0)
*/
CSetClubIncomeMoney::CSetClubIncomeMoney(
	DWORD dwClub,
	LONGLONG llMoney )
	: m_dwClub( dwClub )
	, m_llMoney( llMoney )
{	
}

int CSetClubIncomeMoney::Execute(
	CServer* pServer )
{
	return m_pDbManager->SetClubIncomeMoney( m_dwClub, m_llMoney );
}

/**
* �ش�Ŭ���� Ŭ��â���� �����Ѵ�.
* \param dwClub : Ŭ����ȣ
* \param pData : â����ġ
* \param nSize : ���� ��üũ��
*/
CWriteClubStorage::CWriteClubStorage(
	DWORD dwClub,
	const BYTE* pData,
	int nSize )
	: m_dwClub(0)
	, m_pData(NULL)
	, m_nSize(0)
{	
	if ((pData != NULL) && (nSize > 0) && (dwClub > 0))
	{
		m_dwClub = dwClub;
		m_nSize  = nSize;
		m_pData  = new BYTE[nSize];
		memcpy(m_pData, pData, nSize);
	}	
}

CWriteClubStorage::~CWriteClubStorage()
{
	SAFE_DELETE_ARRAY(m_pData);
}
	
int CWriteClubStorage::Execute(CServer* pServer)
{
	if ( (m_pData!=NULL) && (m_nSize>0) )
		return m_pDbManager->WriteClubStorage(m_dwClub, m_pData, m_nSize);
	else
		return DB_ERROR;
}

CGetClubStorage::CGetClubStorage(
	DWORD _dwClientID,
	DWORD _dwCharID,
	DWORD _dwClubID )
	: m_dwClientID( _dwClientID )
	, m_dwCharID( _dwCharID )
	, m_dwClubID( _dwClubID )
{
}

int CGetClubStorage::Execute(
	CServer* pServer )
{
	int nRetCode;

	LONGLONG lnMoney = m_pDbManager->GetClubMoney ( m_dwClubID );
	if ( lnMoney < 0 )
	{
		//CConsoleMessage::GetInstance()->Write(C_MSG_FILE_CONSOLE, "ERROR:Club Storage Money DB Read Fail.");
		return NET_OK;
	}

	CByteStream *pSTREAM = new CByteStream;
	nRetCode = m_pDbManager->ReadClubStorage ( m_dwClubID, *pSTREAM );
	if (nRetCode == DB_ERROR)
	{
		//CConsoleMessage::GetInstance()->Write(C_MSG_FILE_CONSOLE, "ERROR:Club Storate Stream DB Read Fail.");

		SAFE_DELETE(pSTREAM);
		return NET_OK;
	}

	GLMSG::SNET_CLUB_STORAGE_GET_DB NetMsg;
	NetMsg.m_dwCharID = m_dwCharID;
	NetMsg.m_dwClubID = m_dwClubID;
	NetMsg.m_lnStorageMoney = lnMoney;
	NetMsg.m_pStream = pSTREAM;
	CFieldServer* pFieldServer = reinterpret_cast<CFieldServer*> (pServer);
	pFieldServer->InsertMsg ( m_dwClientID, (char*) &NetMsg );

	return NET_OK;
}

///////////////////////////////////////////////////////////////////////////
// �� Ŭ������ �����Ѵ�
// dwClub : Ŭ����ȣ
// dwChaNum : ĳ���͹�ȣ
// dwSubMasterFlags : �÷��� (0 ���� ������ �Ϲݱ����� �ȴ�)
CSetClubMasterFlags::CSetClubMasterFlags(DWORD dwClub, DWORD dwChaNum, DWORD dwSubMasterFlags)
{
	m_dwClub           = dwClub;
	m_dwChaNum         = dwChaNum;
	m_dwSubMasterFlags = dwSubMasterFlags;
}

int CSetClubMasterFlags::Execute(CServer* pServer)
{
	return m_pDbManager->SetClubMasterFlags(m_dwClub, m_dwChaNum, m_dwSubMasterFlags);
}

///////////////////////////////////////////////////////////////////////////
// ���� Ŭ���� �Ἲ�Ѵ�.
// dwClubP : �� Ŭ����ȣ (���� Ŭ���� ��)
// dwClubS : ���� Ŭ����ȣ (�� Ŭ�� ������ ���� ���� Ŭ��)
// * ���� *
// db �� Ŭ���� ���Ἲ�� üũ���� �ʴ´�.
// A �� B �� ������ ���¿��� 
// B �� C �� ������ ����
// C �� A �� ������ ���� ������ ü�� ������ �߻��Ѵ�.
CSetClubAlliance::CSetClubAlliance(DWORD dwClubP, DWORD dwClubS)
{
	m_dwClubP = dwClubP;
	m_dwClubS = dwClubS;
}

int CSetClubAlliance::Execute(CServer* pServer)
{
	return m_pDbManager->SetClubAlliance(m_dwClubP, m_dwClubS);
}

///////////////////////////////////////////////////////////////////////////
// ���� Ŭ���� ��ü�Ѵ�.
// dwClubP : �� Ŭ����ȣ (���� Ŭ���� ��)
// dwClubS : ���� Ŭ����ȣ (�� Ŭ�� ������ ���� ���� Ŭ��)
CDelClubAlliance::CDelClubAlliance(DWORD dwClubP, DWORD dwClubS)
{	
	m_dwClubP = dwClubP;
	m_dwClubS = dwClubS;
}

int CDelClubAlliance::Execute(CServer* pServer)
{
	return m_pDbManager->DelClubAlliance(m_dwClubP, m_dwClubS);
}


///////////////////////////////////////////////////////////////////////////
// Ŭ����Ʋ�� �����Ѵ�.
// dwClubP : �� Ŭ����ȣ (�ڱ�Ŭ��)
// dwClubS : ���� Ŭ����ȣ (���Ŭ��)
CSetClubBattle::CSetClubBattle(DWORD dwClubP, DWORD dwClubS, DWORD dwEndTime, bool bAlliance )
	: m_dwClubP ( dwClubP )
	, m_dwClubS ( dwClubS )
	, m_dwEndTime ( dwEndTime )
	, m_bAlliance ( bAlliance )
{	
}

int CSetClubBattle::Execute(CServer* pServer)
{
	return m_pDbManager->SetClubBattle(m_dwClubP, m_dwClubS, m_dwEndTime, (int)m_bAlliance );
}

///////////////////////////////////////////////////////////////////////////
// Ŭ����Ʋ�� �����Ѵ�.
// dwClubP : �� Ŭ����ȣ (�ڱ�Ŭ��)
// dwClubS : ���� Ŭ����ȣ (���Ŭ��)
// nFlag :	���� Flag
// nGuKillNum : ų��
// nGuDeathNum : ������
CEndClubBattle::CEndClubBattle(
	DWORD dwClubP, 
	DWORD dwClubS, 
	int nFlag, 
	int nGuKillNum, 
	int nGuDeathNum, 
	bool bAlliance )
{	
	m_dwClubP = dwClubP;
	m_dwClubS = dwClubS;
	m_nFlag = nFlag;
	m_nGuKillNum = nGuKillNum;
	m_nGuDeathNum = nGuDeathNum;
	m_bAlliance = bAlliance;
}

int CEndClubBattle::Execute(CServer* pServer)
{
	return m_pDbManager->EndClubBattle(m_dwClubP, m_dwClubS, m_nFlag, m_nGuKillNum, 
									m_nGuDeathNum, m_bAlliance);
}


///////////////////////////////////////////////////////////////////////////
// Ŭ����Ʋ�� �����Ѵ�.
// dwClubP : �� Ŭ����ȣ (�ڱ�Ŭ��)
// dwClubS : ���� Ŭ����ȣ (���Ŭ��)
// nFlag :	���� Flag
// nGuKillNum : ų��
// nGuDeathNum : ������
CSaveClubBattle::CSaveClubBattle(
	DWORD dwClubP, 
	DWORD dwClubS, 
	int nGuKillNum, 
	int nGuDeathNum )
{	
	m_dwClubP = dwClubP;
	m_dwClubS = dwClubS;
	m_nGuKillNum = nGuKillNum;
	m_nGuDeathNum = nGuDeathNum;
}

int CSaveClubBattle::Execute(CServer* pServer)
{
	return m_pDbManager->SaveClubBattle(m_dwClubP, m_dwClubS, m_nGuKillNum, m_nGuDeathNum);
}

CReSetAllianceBattle:: CReSetAllianceBattle( DWORD dwClub )
	: m_dwClub ( dwClub )
{
}

int CReSetAllianceBattle::Execute(CServer* pServer)
{
	return m_pDbManager->ReSetAllianceBattle( m_dwClub );
}


///////////////////////////////////////////////////////////////////////////
// Ŭ���� ���������� �����Ѵ�.
CSetClubNotice::CSetClubNotice(DWORD dwClub, const char* szClubNotice)
{
	memset(m_szNotice, 0, sizeof(char) * (EMCLUB_NOTICE_LEN+1));
	
	m_dwClub = dwClub;

	CString strTemp;
	strTemp = szClubNotice;
    strTemp.Replace(_T("'"), _T("''"));	
	StringCchCopy(m_szNotice, EMCLUB_NOTICE_LEN+1, strTemp.GetString());
}

int CSetClubNotice::Execute(CServer* pServer)
{
	return m_pDbManager->SetClubNotice(m_dwClub, (const char*) m_szNotice);
}

///////////////////////////////////////////////////////////////////////////
// Ŭ���� ����Ŭ�� ���� �븮�ڸ� �����Ѵ�.
CSetClubDeputy::CSetClubDeputy(DWORD dwClub, DWORD dwDeputy)
{
	m_dwClub   = dwClub;
	m_dwDeputy = dwDeputy;
}

int CSetClubDeputy::Execute(CServer* pServer)
{
	return m_pDbManager->SetClubDeputy(m_dwClub, m_dwDeputy);
}

CSetClubAuthority::CSetClubAuthority(DWORD dwClub, DWORD dwMasterID)
{
	m_dwClub   = dwClub;
	m_dwMasterID = dwMasterID;
}

int CSetClubAuthority::Execute(CServer* pServer)
{
	return m_pDbManager->SetClubAuthority(m_dwClub, m_dwMasterID);
}
