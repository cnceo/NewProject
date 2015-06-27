#include "ClientApp.h"
#include "cocos2d.h"
CClientApp* CClientApp::s_ClientApp = NULL ;
CClientApp* CClientApp::getInstance()
{
	if ( s_ClientApp == nullptr )
	{
		s_ClientApp = new CClientApp ;
	}
	return s_ClientApp ;
}

void CClientApp::destroyInstance()
{
	if ( s_ClientApp )
	{
		delete s_ClientApp ;
		s_ClientApp = nullptr ;
	}
}

CClientApp::CClientApp()
{
	m_pNetwork = NULL ;
	m_pConnectID = INVALID_CONNECT_ID ;
}

CClientApp::~CClientApp()
{
	if ( m_pNetwork )
	{
		m_pNetwork->ShutDown() ;
		delete m_pNetwork ;
		m_pNetwork = nullptr ;
	}
}

bool CClientApp::init()
{
	if ( m_pNetwork == nullptr )
	{
		m_pNetwork = new CNetWorkMgr ;
		m_pNetwork->AddMessageDelegate(this);
	}

	m_eNetState = CNetWorkMgr::eConnectType_None ;
	return true ;
}

void CClientApp::enterForground()
{
	connectToSvr();
	cocos2d::Director::getInstance()->getScheduler()->schedule([=](float fTime){ update(fTime);},this,0,false,"clientUpdate");
}

void CClientApp::enterBackground()
{
	disconnectFromSvr();
	cocos2d::Director::getInstance()->getScheduler()->unscheduleAllForTarget(this);
}

void CClientApp::connectToSvr()
{
	if ( m_eNetState == CNetWorkMgr::eConnectType_Connected || m_eNetState == CNetWorkMgr::eConnectType_Connecting )
	{
		printf("connecting or connected so do not need connect to ");
		return ;
	}

	if ( m_pNetwork )
	{
		m_pNetwork->SetupNetwork(1) ;
		m_pNetwork->ConnectToServer("203.186.75.136",50001);
	}
	m_eNetState = CNetWorkMgr::eConnectType_Connecting ;
}

void CClientApp::disconnectFromSvr()
{
	if ( m_pNetwork )
	{
		m_pNetwork->DisconnectServer(m_pConnectID);
		m_pNetwork->ShutDown();
	}
	m_eNetState = CNetWorkMgr::eConnectType_None ;
}

// net delegate 
bool CClientApp::OnMessage( Packet* pMsg )
{
	return false ;
}

bool CClientApp::OnLostSever(Packet* pMsg)
{
	m_pConnectID = INVALID_CONNECT_ID ;
	m_eNetState = CNetWorkMgr::eConnectType_Disconnectd ;
	return false ;
}

bool CClientApp::OnConnectStateChanged( eConnectState eSate, Packet* pMsg)
{
	if ( eSate == CNetMessageDelegate::eConnect_Accepted )
	{
		m_pConnectID = pMsg->_connectID ;
		m_eNetState = CNetWorkMgr::eConnectType_Connected ;
		stMsg msgVerify ;
		msgVerify.cSysIdentifer = ID_MSG_VERIFY ;
		msgVerify.usMsgType = MSG_VERIFY_CLIENT ;
		sendMsg(&msgVerify,sizeof(msgVerify));
	}
	else
	{
		m_eNetState = CNetWorkMgr::eConnectType_None ;
	}
	return false ;
}

bool CClientApp::sendMsg(stMsg* pMsg , uint16_t nLen )
{
	if ( isConnecting() )
	{
		return m_pNetwork->SendMsg((char*)pMsg,nLen,m_pConnectID ) ;
	}
	else
	{
		printf("server is disconnected , cann't send msg\n");
	}
	return false ;
}

bool CClientApp::isConnecting()
{
	return (m_pConnectID != INVALID_CONNECT_ID ) && m_eNetState == CNetWorkMgr::eConnectType_Connected ;
}


void CClientApp::update(float fDeta )
{
	if ( m_pNetwork )
	{
		m_pNetwork->ReciveMessage() ;
	}
}

void CClientApp::addMsgDelegate(CNetMessageDelegate* pDelegate )
{
	if ( m_pNetwork )
	{
		m_pNetwork->AddMessageDelegate(pDelegate) ;
	}
}

void CClientApp::removeMsgDelegate( CNetMessageDelegate* pDelegate )
{
	if ( m_pNetwork )
	{
		m_pNetwork->RemoveMessageDelegate(pDelegate);
	}
}