#include <WinSock2.h>
#include "ClientNetworkImp.h"
CClientNetworkImp::CClientNetworkImp()
	:m_pReadIngBuffer(new CInternalBuffer())
{
	m_socket = nullptr ;
	m_pIOThread = nullptr ;
	m_pEndpoint = nullptr ;
}

CClientNetworkImp::~CClientNetworkImp()
{
	if ( m_pIOThread )
	{
		m_io_service.stop() ;
		m_pIOThread->detach() ;
		delete m_pIOThread ;
		m_pIOThread = nullptr ;
	}

	if ( m_socket )
	{
		m_socket->close() ;
		delete m_socket ;
		m_socket = nullptr ;
	}

	if ( m_pEndpoint )
	{
		delete m_pEndpoint ;
		m_pEndpoint = nullptr ;
	}
}

bool CClientNetworkImp::init()
{
	if ( nullptr == m_socket )
	{
		m_socket = new tcp::socket(m_io_service);	
	}
	m_nState = eState_None ;
	
	return true ;
}

void CClientNetworkImp::shutdown()
{
	m_io_service.post(boost::bind(&CClientNetworkImp::doClose, this)); //这个close函数是客户端要主动终止时调用  do_close函数是从服务器端  
	//读数据失败时调用  
}

bool CClientNetworkImp::connectToServer(const char* pIP, unsigned short nPort )
{
	if ( m_nState == eState_Connecting || m_nState == eState_Connected )
	{
		printf("connecting or connected , don't do twice\n") ;
		return false;
	}

	if ( nullptr == m_socket )
	{
		m_socket = new tcp::socket(m_io_service);	
	}

	if ( m_pEndpoint )
	{
		delete m_pEndpoint ;
		m_pEndpoint = nullptr ;
	}
	m_io_service.reset();
	m_nState = eState_Connecting ;
	printf("connect ting \n");

	m_pEndpoint =  new boost::asio::ip::tcp::endpoint( boost::asio::ip::address::from_string(pIP),nPort );
	m_socket->async_connect(*m_pEndpoint,boost::bind(&CClientNetworkImp::handleConnect, this,  
		boost::asio::placeholders::error));
	//boost::asio::async_connect(*m_socket, m_pEndpoint,pNull,  
// 		boost::bind(&CClientNetworkImp::handleConnect, this,  
// 		boost::asio::placeholders::error)); //所有的操作都采用异步的方式 

	if ( m_pIOThread )
	{
		delete m_pIOThread ;
		m_pIOThread = nullptr ;
	}

	if ( m_pIOThread == nullptr )
	{
		m_pIOThread = new  boost::thread(boost::bind(&boost::asio::io_service::run, &m_io_service)); 
		m_pIOThread->detach();
	}
	

	return true ;
}

bool CClientNetworkImp::getAllPacket(LIST_PACKET& vOutPackets ) // must delete out side ;
{
	WriteLock wLock(m_PacketMutex) ;
	vOutPackets.swap(m_vRecivedPackets) ;
	return !vOutPackets.empty();
}

bool CClientNetworkImp::getFirstPacket(Packet** ppPacket ) // must delete out side ;
{
	WriteLock wLock(m_PacketMutex) ;
	if ( m_vRecivedPackets.empty() )
	{
		return false ;
	}

	LIST_PACKET::iterator iter = m_vRecivedPackets.begin() ;
	Packet* p = m_vRecivedPackets.front() ;
	*ppPacket = p ;
	m_vRecivedPackets.erase(iter) ;
	return true ;
}

void CClientNetworkImp::addPacket(Packet* pPacket ) 
{
	WriteLock wLock(m_PacketMutex) ;
	m_vRecivedPackets.push_back(pPacket) ;
}

bool CClientNetworkImp::sendMsg(const char* pData , size_t nLen ) 
{
	if ( nLen >= CInternalBuffer::max_body_length )
	{
		return false ;
	}
	InternalBuffer_ptr msg (new CInternalBuffer());
	msg->setData(pData,nLen) ;
	m_io_service.post(boost::bind(&CClientNetworkImp::doWrite, this, msg)); //将消息主动投递给io_service  
	return true ;
}

void CClientNetworkImp::handleConnect(const boost::system::error_code& error)  
{  
	bool bSucce = !error;
	Packet* pack = new Packet ;
	pack->_brocast = false ;
	pack->_packetType = bSucce ?_PACKET_TYPE_CONNECTED : _PACKET_TYPE_CONNECT_FAILED ;
	pack->_connectID = 0 ;
	if (bSucce)  
	{  
		boost::asio::async_read(*m_socket,  
			boost::asio::buffer(m_pReadIngBuffer->data(), CInternalBuffer::header_length), //读取数据报头  
			boost::bind(&CClientNetworkImp::handleReadHeader, this,  
			boost::asio::placeholders::error));  
		printf("connected success\n");
	} 
	else
	{
		printf("connected Failed\n");
	}
	m_nState = bSucce ? eState_Connected : eState_ConnectedFailed ;
	addPacket(pack);
}  

void CClientNetworkImp::handleReadHeader(const boost::system::error_code& error)  
{  
	if (!error && m_pReadIngBuffer->decodeHeader()) //分别处理数据报头和数据部分  
	{  
		boost::asio::async_read(*m_socket,  
			boost::asio::buffer(m_pReadIngBuffer->body(),m_pReadIngBuffer->bodyLength()),//读取数据包数据部分  
			boost::bind(&CClientNetworkImp::handleReadBody, this,  
			boost::asio::placeholders::error));  
	}  
	else  
	{  
		doClose();  
	}  
}  

void CClientNetworkImp::handleReadBody(const boost::system::error_code& error)  
{  
	if (!error)  
	{  
		if ( m_pReadIngBuffer->bodyLength() )
		{
			Packet* pack = new Packet ;
			pack->_brocast = false ;
			pack->_packetType = _PACKET_TYPE_MSG ;
			pack->_connectID = 0 ;
			pack->_len = m_pReadIngBuffer->bodyLength() ;
			memcpy(pack->_orgdata,m_pReadIngBuffer->body(),m_pReadIngBuffer->bodyLength()) ;
			addPacket(pack) ;
		}

		boost::asio::async_read(*m_socket,   
			boost::asio::buffer(m_pReadIngBuffer->data(), CInternalBuffer::header_length), //在这里读取下一个数据包头  
			boost::bind(&CClientNetworkImp::handleReadHeader, this,    
			boost::asio::placeholders::error)); //完成一次读操作（处理完一个数据包）  进行下一次读操作  
		
		
	}  
	else  
	{  
		doClose();  
	}  
}  

void CClientNetworkImp::doWrite(InternalBuffer_ptr msg)  
{  
	bool write_in_progress = !m_vWillSendBuffers.empty(); //空的话变量为false  
	m_vWillSendBuffers.push_back(msg); //把要写的数据push至写队列  
	if (!write_in_progress)//队列初始为空 push一个msg后就有一个元素了  
	{  
		boost::asio::async_write(*m_socket,  
			boost::asio::buffer(m_vWillSendBuffers.front()->data(),  
			m_vWillSendBuffers.front()->length()),  
			boost::bind(&CClientNetworkImp::handleWrite, this,   
			boost::asio::placeholders::error));  
	}  
}  

void CClientNetworkImp::handleWrite(const boost::system::error_code& error)//第一个消息单独处理，剩下的才更好操作  
{  
	if (!error)  
	{  
		m_vWillSendBuffers.pop_front();//刚才处理完一个数据 所以要pop一个  
		if (!m_vWillSendBuffers.empty())    
		{  
			boost::asio::async_write(*m_socket,  
				boost::asio::buffer(m_vWillSendBuffers.front()->data(),  
				m_vWillSendBuffers.front()->length()),  
				boost::bind(&CClientNetworkImp::handleWrite, this,  
				boost::asio::placeholders::error)); //循环处理剩余的消息  
		}  
	}  
	else  
	{  
		doClose();  
	}  
}  

void CClientNetworkImp::doClose()  
{  
	if ( m_nState != eState_Connected && m_nState != eState_Connecting )
	{
		printf("socket already closed") ;
		return ;
	}
	m_nState = eState_None ;
	m_socket->close();

	Packet* pack = new Packet ;
	pack->_brocast = false ;
	pack->_packetType = _PACKET_TYPE_DISCONNECT ;
	pack->_connectID = 0 ;
	pack->_len = 0 ;
	addPacket(pack) ;
	printf("server dis connected\n");
}  