#pragma once
#include "NativeTypes.h"
#include "MessageIdentifer.h"
#include "CommonDefine.h"
#include <ctime>
#include "IRoom.h"
class IRoom ;
struct stMsg ;
class IRoomState
{
public:
	IRoomState(){ m_fStateDuring = 0 ; m_nState = eRoomState_Max ;}
	virtual ~IRoomState(){}
	virtual void enterState(IRoom* pRoom) = 0 ;
	virtual void leaveState(){}
	virtual void update(float fDeta);
	virtual bool onMessage( stMsg* prealMsg , eMsgPort eSenderPort , uint32_t nPlayerSessionID ){ return false ;}
	uint16_t getStateID(){ return m_nState ; } ;
	virtual void onStateDuringTimeUp(){}
	void setStateDuringTime( float fTime ){ m_fStateDuring = fTime ;} 
	float getStateDuring(){ return m_fStateDuring ;}
private:
	float m_fStateDuring ;
protected:
	uint16_t m_nState ;
};

// wait player join state 
class IRoomStateWaitPlayerJoin
	: public IRoomState
{
public:
	IRoomStateWaitPlayerJoin(){ m_nState = eRoomState_WaitJoin;}
	void enterState(IRoom* pRoom )override{m_pRoom = pRoom ;}
	void update(float)override
	{
		// check close 
		time_t nNow = time(nullptr) ;
		if ( m_pRoom->getCloseTime() && m_pRoom->getCloseTime() < nNow )
		{
			m_pRoom->onRoomClosed();
			m_pRoom->goToState(eRoomState_Close) ;
			return ;
		}

		if ( m_pRoom->canStartGame() )
		{
			m_pRoom->onGameWillBegin();
			m_pRoom->goToState(eRoomState_StartGame) ;
			return ;
		}

	}
protected:
	IRoom* m_pRoom ;
};

// close state 
class IRoomStateClosed
	:public IRoomState
{
public:
	IRoomStateClosed(){ m_nState = eRoomState_Close;}
	void enterState(IRoom* pRoom )override
	{ 
		m_pRoom = pRoom ;
	}

	void update(float)override
	{
		time_t nNow = time(nullptr) ;
		if ( m_pRoom->getDeadTime() && m_pRoom->getDeadTime() < nNow )
		{
			m_pRoom->goToState(eRoomState_Dead) ;
			return ;
		}

		if ( nNow > m_pRoom->getOpenTime() )  // reopen game ;
		{			
			m_pRoom->onRoomOpened();
			m_pRoom->goToState(eRoomState_WaitJoin) ;
			return ;
		}
	}
protected:
	IRoom* m_pRoom ;
};

// did game over 
class IRoomStateDidGameOver
	:public IRoomState
{
public:
	enum { eStateID = eRoomState_Dead };
public:
	IRoomStateDidGameOver(){ m_nState = eRoomState_DidGameOver; }
	virtual ~IRoomStateDidGameOver(){}
	virtual void enterState(IRoom* pRoom)
	{
		m_pRoom = pRoom ;
		pRoom->onGameDidEnd();
	}

	void update(float)override
	{
		// check close 
		time_t nNow = time(nullptr) ;
		if ( m_pRoom->getCloseTime() && m_pRoom->getCloseTime() < nNow )
		{
			m_pRoom->goToState(eRoomState_Close) ;
			return ;
		}
		m_pRoom->goToState(eRoomState_WaitJoin) ;
	}
protected:
	IRoom* m_pRoom ;
};


// dead state 
class IRoomStateDead
	:public IRoomState
{
public:
	enum { eStateID = eRoomState_Dead };
public:
	IRoomStateDead(){ m_nState = eRoomState_Dead; }
	virtual ~IRoomStateDead(){}
	virtual void enterState(IRoom* pRoom)
	{
		pRoom->deleteRoom() ;
	}
};