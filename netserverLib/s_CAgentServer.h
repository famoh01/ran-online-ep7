///////////////////////////////////////////////////////////////////////////////
// s_CAgentServer.h
//
#ifndef S_CAgentServer_H_
#define S_CAgentServer_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "s_CServer.h"
#include "s_CClientAgent.h"
#include "s_CMinMd5.h"
#include "s_Util.h"
#include "s_CDbAction.h"
#include "minTea.h"
#include "ApexCloseList.h"
#include "s_CWhiteRock.h"

#include "../LegacyInclude/ApexProxy.h"
#include "s_CAgentServerApex.h"

#include "../MfcExLib/ExceptionHandler.h"

#include "../EngineLib/G-Logic/DxMsgServer.h"

#include "../RanClientLib/DbActionLogic.h"
#include "../RanClientLib/DxServerInstance.h"
#include "../RanClientLib/G-Logic/GLChar.h"
#include "../RanClientLib/G-Logic/GLContrlMsg.h"
#include "../RanClientLib/G-Logic/GLAgentServer.h"

#ifndef ERROR_MSG_LENGTH 
#define ERROR_MSG_LENGTH 256
#endif

#if defined ( PH_PARAM ) || defined ( VN_PARAM )
	#import "EGameEncrypt.dll" raw_interfaces_only, raw_native_types, no_namespace, named_guids
#else
	#import "MyRossoEncrypt.dll" raw_interfaces_only, raw_native_types, no_namespace, named_guids
#endif

/**
 * \ingroup NetServerLib
 *
 *
 * \par requirements 
 * win2k or later\n
 * MFC\n
 *
 * \version 1.0
 * first version
 *
 * \date 2002-05-30
 *
 * \author Jgkim
 *
 * \par license
 * Copyright (c) Min Communications. All rights reserved.
 *
 * \todo 
 *
 * \bug 
 * 
 */
#define MAX_RANDOM_PASSWORD_NUM 30
#define	HTTP_STATUS_ALREADY_LOGIN 533

#if defined ( PH_PARAM ) || defined ( VN_PARAM )
	extern IEGameEncryptPtr    m_pMyRossoEncrypto; //! Terra Decrypt Module
#else
	extern IMyRossoEncryptPtr    m_pMyRossoEncrypto; //! Terra Decrypt Module
#endif

class CAgentServer : public CServer, public DxMsgServer
{
public :
	CAgentServer(HWND hWnd, HWND hEditBox, HWND hEditBoxInfo);
	~CAgentServer();

protected :
	// SOCKET m_sSession; ///< Socket for session server
	CClientAgent* m_pClientManager; ///< Client ������
	int m_ChannelUser[MAX_CHANNEL_NUMBER]; ///< ä�κ� ����� ���� ����
	
	CDaumGameAuth m_DaumAuth; ///< Daum Game Decrypt Module
	SERVER_UTIL::CMinMd5 m_MinMd5; ///< Excite Japan MD5
	minTea m_Tea; ///< Encrypt algorithm
	bool m_bHeartBeat;

	HMODULE m_hCheckStrDLL;

	BOOL (_stdcall *m_pCheckString)(CString);

	// typedef	std::pair<DWORD,DWORD>		APEX_ID_TIME;
	// typedef	std::list<APEX_ID_TIME>		APEX_CLOSE_LIST;
	// typedef	APEX_CLOSE_LIST::iterator	APEX_CLOSE_LIST_ITER;

	ApexCloseList m_ApexClose;
	// APEX_CLOSE_LIST m_listApexClose;

	// INFO:Reached Max Client Number �޽����� �㶧 UpdateProc�� ����� �۵��ϴ����� �˾ƺ��� ���� ����
	bool m_bReachedMAX;


	CTime m_VietnamPresetTime;
public:
	//! -----------------------------------------------------------------------
	//! I/O operation memory
	//virtual int StopIOCPList();
	//virtual int StartIOCPList() { return NET_OK; }
	//virtual PER_IO_OPERATION_DATA* getSendIO( DWORD dwClient );
	//virtual PER_IO_OPERATION_DATA* getRecvIO( DWORD dwClient );	
	//virtual void releaseSendIO( DWORD dwClient, PER_IO_OPERATION_DATA* pData );
	//virtual void releaseRecvIO( DWORD dwClient, PER_IO_OPERATION_DATA* pData );
	//virtual void releaseIO( DWORD dwClient, PER_IO_OPERATION_DATA* pData );

public:
	virtual int  GetServerGroup ()		{ return m_nServerGroup; }
	virtual int  GetServerNum ()		{ return m_nServerNum; }

	virtual int	 Start();
	virtual int	 Stop();	
	virtual	int	 Pause();
	virtual	int  Resume();
	virtual	int	 ReStart();
	
	        int	 StartClientManager();
	        int	 StartDbManager();

			void WriteNprotectLog( DWORD dwClient, DWORD dwResult );
	
	virtual int	 WorkProc();
	virtual int	 UpdateProc();	
	virtual int  ListenProc();	

	virtual	int  DatabaseProc();     ///< Database thread function
	virtual	int  LogDatabaseProc();  ///< Log Database thread function
	virtual	int  UserDatabaseProc(); ///< User Database thread function
	virtual int	 WebDatabaseProc(); ///< Web Database thread function

	virtual int ConnectFieldSvr( DWORD dwClient, int nFieldServer, DWORD dwGaeaID, int nServerChannel=0 );	
	
	virtual int SendAgent ( DWORD dwClient, LPVOID pBuffer )
				{ 
					assert(0&&"������Ʈ������ SendAgent()�� ȣ���Ҽ� �����ϴ�."); 
					return 0; 
				}
	virtual int SendAgent ( LPVOID pBuffer )		                 
				{ 
					assert(0&&"������Ʈ������ SendAgent()�� ȣ���Ҽ� �����ϴ�."); 
					return 0; 
				}	
	virtual int SendClient ( DWORD dwClient, LPVOID pBuffer );

			void SendAllClient ( LPVOID pBuffer );
			void SendAllClientChannel ( LPVOID pBuffer, int nServerChannel=0 );

	virtual void SendAllField ( LPVOID pBuffer, int nServerChannel=0 ) 
	             { 
					 BroadcastToField(pBuffer, nServerChannel); 
				 }	
	virtual int SendField ( DWORD dwClient, LPVOID pBuffer );
	virtual int SendFieldSvr ( int nSvrNum, 
		                       LPVOID pBuffer, 
							   int nServerChannel=0 )
				{ 
					return BroadcastToField ( nSvrNum, pBuffer, nServerChannel ); 
				}

	virtual void CloseClient(DWORD dwClient);  // Close Client Connection
            void CloseField (DWORD dwClient);
            void CloseAllClient(); 	// close all client connections
    
	virtual bool IsFieldOnline ( DWORD dwClient );
	virtual bool IsSlotOnline  ( DWORD dwClient );

	void	MsgVersion			(MSG_LIST* pMsg);	
	
	void	MsgCreateCharacter	(MSG_LIST* pMsg);    
    void    MsgCreateCharacterBack(int nChaNum, DWORD dwClient, DWORD dwUserNum, const char* szUserIP, USHORT uPort=0);	
	
	void    MsgSndPing			(DWORD dwClient);
	void	MsgSndPingAnswer	(MSG_LIST* pMsg);

	void	MsgSndChatGlobal	(char* szChatMsg);
	void	MsgSndChatNormal	(DWORD dwClient, const char* szName, const char* szMsg);
	void	MsgSndCryptKey		(DWORD dwClient);
	
	void	MsgSndLoginInfoToField(DWORD dwClient);

	void	MsgSndChaBasicBAInfo(MSG_LIST* pMsg);
	void	MsgSndChaBasicBAInfoBack(NET_CHA_BBA_INFO* pStruct, DWORD dwClient, const char* szUserIP, USHORT uPort=0);
	void	MsgSndChaBasicInfo  (MSG_LIST* pMsg);
	void	MsgSndChaBasicInfo  (DWORD dwClient, int nChaNum);
	void	MsgSndChaBasicInfoBack(GLMSG::SNETLOBBY_CHARINFO* snci, DWORD dwClient, const char* szUserIP, USHORT uPort=0);	
	
	void	MsgSndChaDelInfo( MSG_LIST* pMsg ); // Agent->Client:ĳ���� ���� ��� ����	
	void	MsgSndChaDelInfoBack( int nChaNum, 
		                          int nUserNum, 
								  int nResult, 
								  DWORD dwClient, 
								  const TCHAR* szUserIP, 
								  USHORT uPort=0 ); // ���Ӽ���->Ŭ���̾�Ʈ : ĳ���� ���� ��� ����	

	//! Agent->Client:ĳ���� ����
	void DaumMsgSndChaDelInfo(MSG_LIST* pMsg);

	//! Agent->Client:ĳ���� ���� ��� ����
	void DaumMsgSndChaDelInfoBack(int nChaNum, 
		                          int nUserNum, 
		                          int nResult, 
								  DWORD dwClient, 
								  const char* szUserIP, 
								  USHORT uPort=0);

	/**
	* Client->Agent:ĳ���� ������û
	* \param pMsg 
	*/
	void TerraMsgSndChaDelInfo( MSG_LIST* pMsg );

	/**
	* Client->Agent:ĳ���� ������û
	* \param pMsg 
	*/
	void GspMsgSndChaDelInfo( MSG_LIST* pMsg );

	/**
	* Client->Agent:ĳ���� ������û
	* \param pMsg 
	*/
	void ExciteMsgSndChaDelInfo(MSG_LIST* pMsg);

	/**
	*
	* \param nChaNum 
	* \param nUserNum 
	* \param nResult 
	* \param dwClient 
	* \param szUserIP 
	* \param uPort 
	*/
	void ExciteMsgSndChaDelInfoBack(int nChaNum, 
		                            int nUserNum, 
		                            int nResult, 
								    DWORD dwClient, 
								    const char* szUserIP, 
								    USHORT uPort=0);

	/**
	* Client->Agent:ĳ���� ������û
	* \param pMsg 
	*/
	void JapanMsgSndChaDelInfo(MSG_LIST* pMsg);

	/**
	*
	* \param nChaNum 
	* \param nUserNum 
	* \param nResult 
	* \param dwClient 
	* \param szUserIP 
	* \param uPort 
	*/
	void JapanMsgSndChaDelInfoBack(int nChaNum, 
		                            int nUserNum, 
		                            int nResult, 
								    DWORD dwClient, 
								    const char* szUserIP, 
								    USHORT uPort=0);

	/**
	* Agent->Client:ĳ���� �����������
	* \param nChaNum ĳ���͹�ȣ
	* \param nUserNum ������ȣ
	* \param nResult ���
	* \param dwClient Ŭ���̾�Ʈ��ȣ
	* \param szUserIP IP
	* \param uPort PORT
	*/
	void TerraMsgSndChaDelInfoBack(int nChaNum, 
	                               int nUserNum, 
								   int nResult, 
								   DWORD dwClient, 
								   const char* szUserIP, 
								   USHORT uPort=0);

	/**
	* Agent->Client:ĳ���� �����������
	* \param nChaNum ĳ���͹�ȣ
	* \param nUserNum ������ȣ
	* \param nResult ���
	* \param dwClient Ŭ���̾�Ʈ��ȣ
	* \param szUserIP IP
	* \param uPort PORT
	*/
	void GspMsgSndChaDelInfoBack(
			int nChaNum, 
	        int nUserNum, 
			int nResult, 
			DWORD dwClient, 
			const TCHAR* szUserIP, 
			USHORT uPort=0 );


	/**
	* Client->Agent:ĳ���� ������û
	* \param pMsg 
	*/
	void	GsMsgSndChaDelInfo( MSG_LIST* pMsg ); // Agent->Client:ĳ���� ���� ��� ����	

	/**
	* Agent->Client:ĳ���� �����������
	* \param nChaNum ĳ���͹�ȣ
	* \param nUserNum ������ȣ
	* \param nResult ���
	* \param dwClient Ŭ���̾�Ʈ��ȣ
	* \param szUserIP IP
	* \param uPort PORT
	*/
	void	GsMsgSndChaDelInfoBack( int nChaNum, 
		                          int nUserNum, 
								  int nResult, 
								  DWORD dwClient, 
								  const TCHAR* szUserIP, 
								  USHORT uPort=0 ); // ���Ӽ���->Ŭ���̾�Ʈ : ĳ���� ���� ��� ����	
	
	//! Ŭ���̾�Ʈ->������Ʈ���� : ĳ���� ������ DB���� �о���� ��û.
	void MsgGameJoin(MSG_LIST* pMsg); 
	//! DB ������->������Ʈ���� : DB���� �о��� ������ �ɸ��� ���� �õ�.
	void MsgAgentReqJoin(MSG_LIST* pMsg);
	//void MsgAgentReqJoin( DWORD dwClientID, GLCHARAG_DATA * pCHAR_DATA );
	//! �ʵ弭��->������Ʈ���� : ĳ���Ͱ� ���ӿ� ��������
	int MsgLobbyCharJoinField(MSG_LIST* pMsg);

	void MsgFieldSvrOutFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );
	void MsgReBirthOutFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );
	void MsgReCallFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );
	void MsgPremiumSetFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );    
	void MsgAttackClientCheck ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );    
	void MsgTeleportFb ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );
	
	void SetUserInfo(DWORD nClient, const char* strUserId, const char* strPassword); 	

	//! Execute Command Text
	//! @param strCmd �ĸ��
	int	ExecuteCommand(char* strCmd);

	///////////////////////////////////////////////////////////////////////////
	/// Field Server Communication Member Function
	/// If you do not use Field Server, this functions are unnecessary
	///////////////////////////////////////////////////////////////////////////
	void FieldConnectAll();
	
	int FieldConnect(
			int nFieldNum = 0,
			int nChannel = 0 );

	/*
	int	FieldConnect(
			const char* szServerIP,
			int nPort,
			int nFieldNum = 0,
			int nChannel = 0 );
	*/

	int	FieldConnect(
			unsigned long ulServerIP,
			int nPort,
			int nFieldNum=0,
			int nChannel=0 );

	int	FieldCloseConnect(
			int nSvrNum );

	void FieldCloseConnectAll();

	// bool CheckFieldConnection();

	///////////////////////////////////////////////////////////////////////////
	/// �޽��� ó�� ���� �Լ���
	///////////////////////////////////////////////////////////////////////////	
	//! Local Message process function ( gaeaserver�� ���� ���� �޽��� ����. )
	int	InsertMsg(int nClient, void* pMsg);
	//! ��� �޽����� �ϴ� �̰��� ��ģ��.
	int	MsgProcess(MSG_LIST* pMsg);
	//! ���������� ����϶��� �̰��� ��ģ��.
	void ServerMsgProcess(MSG_LIST* pMsg);
	//! ���������� ����� session ������ �ƴҶ��� �̰��� ��ģ��.
	void OtherMsgProcess(MSG_LIST* pMsg);	

	// ���� �޽������� ó���Ѵ�.
	void RecvMsgProcess();
	// ��Ʈ��Ʈ üũ�� �Ѵ�.
	void AgentSrvHeartBeatCheck( DWORD dwUdpateTime );
	// ���� ������Ʈ 
	void UpdateClientState();
	// ���� ������ ���¸� ����Ʈ �Ѵ�.
	void PrintDebugMsg( DWORD dwUdpateTime );
	// UpdateThread ����
	void EndUpdateThread();

	///////////////////////////////////////////////////////////////////////////
	/// ä�� ���� �Լ���
	///////////////////////////////////////////////////////////////////////////
	int  IncreaseChannelUser(int nChannel);
	int  DecreaseChannelUser(int nChannel);
	int  GetChannelUser     (int nChannel);
	bool IsChannelFull      (int nChannel);
	virtual void SendChannel(LPVOID pBuffer, int nServerChannel=0 ) { BroadcastToField(pBuffer, nServerChannel); }
	virtual void SendAllChannel( LPVOID pBuffer );

	///////////////////////////////////////////////////////////////////////////
	/// �α��� ���� �Լ���
	///////////////////////////////////////////////////////////////////////////	
	//! �α��� message ( �븸, ȫ��, ���� ) 
	void MsgLogIn( MSG_LIST* pMsg );
	void MsgLogInBack( NET_LOGIN_FEEDBACK_DATA2* nlfd2 );

	//! China �α��� �޽��� ( �߱� ) 
	void ChinaMsgLogin( MSG_LIST* pMsg );
	void ChinaMsgLoginBack( NET_LOGIN_FEEDBACK_DATA2* nlfd2 );

	//! Daum �α��� �޽��� ( �ѱ� ) 
	void DaumMsgLogin(MSG_LIST* pMsg);
	void DaumMsgLoginBack(DAUM_NET_LOGIN_FEEDBACK_DATA2* nlfd2);

	//! Daum �н����� üũ �޽���
	void DaumMsgPassCheck(MSG_LIST* pMsg);
	void DaumMsgPassCheckBack(DAUM_NET_PASSCHECK_FEEDBACK_DATA2* nlfd2);

	//! GSP �α��� �޽���
	void GspMsgLogin( MSG_LIST* pMsg );
	void GspMsgLoginBack( GSP_NET_LOGIN_FEEDBACK_DATA2* nlfd2 );

	//! Terra �α��� �޽��� ( �����̽þ�, �ʸ���, ��Ʈ�� )
	void TerraMsgLogin( MSG_LIST* pMsg );
	void TerraMsgLoginBack( TERRA_NET_LOGIN_FEEDBACK_DATA2* nlfd2 );

	//! Terra �н����� üũ �޽���
	void TerraMsgPassCheck( MSG_LIST* pMsg );
	void TerraMsgPassCheckBack( TERRA_NET_PASSCHECK_FEEDBACK_DATA2* nlfd2 );

	//! Excite Japan �α��� �޽��� ( �Ϻ� )
	void ExciteMsgLogin(MSG_LIST* pMsg);
	void ExciteMsgLoginBack(EXCITE_NET_LOGIN_FEEDBACK_DATA2* nlfd2);

	//! Excite Japna �н����� üũ �޽���
	void ExciteMsgPassCheck( MSG_LIST* pMsg );
	void ExciteMsgPassCheckBack( EXCITE_NET_PASSCHECK_FEEDBACK_DATA2* nlfd2 );

	//! �Ϻ� Gonzo Japan �α��� �޽���
	void JapanMsgLogin(MSG_LIST* pMsg);
	void JapanMsgLoginBack(JAPAN_NET_LOGIN_FEEDBACK_DATA2* nlfd2);

	// �Ϻ� Gonzo Japan �α��� ���� �Լ�
	void JapanWRFailed( DWORD dwClient, int nResult );
	void JapanWRSuccess( DWORD dwClient, CString strUserID,
									int nServerGroup, int nServerNum, CString strUUID, int nUserNum );

	//! �α��� �޽��� (Thailand) ( �±� ) 
	//! \param pMsg 
	void ThaiMsgLogin(MSG_LIST* pMsg);
	void ThaiMsgLogInBack(NET_LOGIN_FEEDBACK_DATA2* nlfd2);	

    //! Encoding �� TID ���� Decoding �� TID �� ��´�.
	//! @param szEncodedTID Encoding �� TID
	//! @param szDecodedTID Decoding �� TID
	//! @return	
	int TerraGetDecodedTID(char* szEncodedTID, char* szDecodedTID);

	//! �α��� message ( �۷ι� ���� ) 
	void GsMsgLogin( MSG_LIST* pMsg );
	void GsMsgLoginBack( GS_NET_LOGIN_FEEDBACK_DATA2* nlfd2 );

	//! ���� ��ȣ�� ���� ��ȣ�� ������.
	void MsgSndRandomNumber(DWORD dwClient);

	//! Apex Data �޼���	// Apex ����
	void MsgApexData( MSG_LIST* pMsg );
	void MsgApexReturn( MSG_LIST* pMsg );	// Apex ����( ȫ�� )

	//! Apex �� ����ڸ� ���� ��ų�� ȣ��� Block
	void ApexUserBlock( DWORD dwClient, WORD dwHour );

	//! Apex �� ����� ä���� ���� ��ų�� ȣ��� 
	void ApexUserChatBlock( DWORD dwClient, WORD dwHour );

	//! Apex �� ����� �α׸� ���涧 ȣ���
	void ApexUserHackLog( DWORD dwClient, TCHAR* szComment );

	//! Apex �� ����޽����� Ŭ���̾�Ʈ���� ������.
	void ApexSendMessageToClient( DWORD dwClient, WORD wAction, WORD wHour );

	void ApexCloseClient( DWORD dwClient );

	void ApexCloseClientFinal();

	///////////////////////////////////////////////////////////////////////////
	/// nPortect ���� �Լ���
	///////////////////////////////////////////////////////////////////////////	
	
	//! nProtect ������ �޽��� ����
	void MsgSndGameGuard( DWORD dwClient, NET_GAMEGUARD_AUTH& nga );

	//! nProtect Ŭ���̾�Ʈ ���� First
	void MsgSndGameGuardFirstCheck( DWORD dwClient );

	//! nProtect Ŭ���̾�Ʈ ���� Second
	void MsgSndGameGuardSecondCheck( DWORD dwClient );

	//! nProtect ������ �޼��� ���
	void MsgGameGuardAnswer( MSG_LIST* pMsg );

	//! nProtect ������ �޼��� ���
	void MsgGameGuardAnswerFirst( MSG_LIST* pMsg );

	//! nProtect ������ �޼��� ���
	void MsgGameGuardAnswerSecond( MSG_LIST* pMsg );

	//! �ʵ弭���� Ŭ���̾�Ʈ���� ���� ���� �α׸� ��´�.
	void MsgTracingLogPrint( MSG_LIST* pMsg );


	// ���� ID���� �ƴ��� üũ
	int CheckTracingUser( const char *pUserAccount, const int nUserNum );


	///////////////////////////////////////////////////////////////////////////
	/// ���ӳ� �̺�Ʈ�� �Լ���
	///////////////////////////////////////////////////////////////////////////	
	//! �������� PC �� ���θ�� ������ '����' �̺�Ʈ
	void MsgLogPungPungCheckBack(int nResult, DWORD dwClient, const char* szUserIP, USHORT uPort=0);
    //! �� ��������!! �̺�Ʈ 
	void MsgLogLotteryBack(int nResult, DWORD dwClient, const char* szUserIP, USHORT uPort=0);

	///////////////////////////////////////////////////////////////////////////
	/// GM ���ɿ� �޽��� �Լ���
	///////////////////////////////////////////////////////////////////////////	
	void MsgGmPkMode ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );
	void MsgGmwherePcMap ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID );

	// ��Ʈ�� Ÿ�� ���� �ӽ� �Լ�
	void MsgVietTimeReset ( NET_MSG_GENERIC* nmg, DWORD dwClientID, DWORD dwGaeaID ); //vietnamtest%%% && vietnamtest2

    ///////////////////////////////////////////////////////////////////////////
	/// Session Server Communication Member Function	
	///////////////////////////////////////////////////////////////////////////	
	int	SessionConnect(
			unsigned long ulServerIP,
			int nPort);

	int SessionConnect(
			const char* szServerIP,
			int nPort );

	/**
	* Session ������ ����� üũ�ϰ� �̻��� ������ �ٽ� �����Ѵ�.
	*/
	int SessionReconnect();
	
	//! Session ������ �����ϰ� ������ ������ �����Ѵ�.
	int SessionConnectSndSvrInfo(const char* szServerIP, int nPort);
	int SessionConnectSndSvrInfo();

	int SessionCloseConnect();
	int	SendSession(LPVOID pBuffer);
	void SessionMsgProcess(MSG_LIST* pMsg);	

	void SessionMsgHeartbeatServerAns();
	
	//! ���Ǽ���->���Ӽ��� : ä�ø޽���
	void SessionMsgChatProcess(NET_MSG_GENERIC* nmg);
	void SessionMsgSvrCmd(NET_MSG_GENERIC* nmg);

	//  �α��� Ű
	void SessionMsgEncryptKey( NET_MSG_GENERIC* nmg );
	
	//! Ping Answer	
	void SessionMsgPingAnswer(NET_MSG_GENERIC* nmg);
	//! Send ping
	// void SessionSndPing();
	//! Agent->Session:������ ��� ������ ������.
	int SessionSndSvrInfo();
	//! Send Heartbeat
    void SessionSndHeartbeat(); 
	//! Agent->Session:������ ���� ���¸� ������.
	int SessionSndSvrCurState();
	//! Agent->Session:������ ���� ä�� ���¸� ������.
	int SessionSndChannelState(int nChannel);	

	//! ���� �ٿ�� UserLoginState���� ����� �ʱ�ȭ ���� �ʴ� ������ �ִ�.
	//! Agent�� ó�� ������ �� UserDB�� UserLoginState���� 0���� �ʱ�ȭ �����ش�.
	void InitUserLoginState();

	int SessionSndCreateCharacter(int nUserNum);
	int	DaumSessionSndCreateCharacter(int nUserNum);

	int	SessionSndTestCreateCharacter(int nUserNum);
	int	DaumSessionSndTestCreateCharacter(int nUserNum);

	int	SessionSndDelCharacter     (int nUserNum);
	int	DaumSessionSndDelCharacter (int nUserNum);

	int SessionSndTestDelCharacter (int nUserNum);
	int DaumSessionSndTestDelCharacter(int nUserNum);

	BOOL UpdateTracingCharacter(NET_MSG_GENERIC* nmg );

protected:
	void BroadcastToField(LPVOID pBuffer, int nServerChannel=0);
	int	BroadcastToField(int nSvrNum, LPVOID pBuffer, int nServerChannel=0);
	int	BroadcastToField(DWORD dwClient, LPVOID pBuffer);
};

extern CAgentServer* g_pAgentServer;

#endif // S_CAgentServer_H_
