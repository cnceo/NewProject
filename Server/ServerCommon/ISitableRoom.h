#pragma once
#include "IRoom.h"
#include <vector>
struct stSitableRoomConfig ;
class ISitableRoomPlayer ;
class ISitableRoom
	:public IRoom
{
public:
	typedef std::list<ISitableRoomPlayer*> LIST_SITDOWN_PLAYERS ;
	typedef std::vector<ISitableRoomPlayer*> VEC_SITDOWN_PLAYERS;
public:
	~ISitableRoom();
	bool onFirstBeCreated(IRoomManager* pRoomMgr,stBaseRoomConfig* pConfig, uint32_t nRoomID , Json::Value& vJsValue) override;
	void serializationFromDB(IRoomManager* pRoomMgr,stBaseRoomConfig* pConfig,uint32_t nRoomID , Json::Value& vJsValue )override;
	void willSerializtionToDB(Json::Value& vOutJsValue)override;
	void roomItemDetailVisitor(Json::Value& vOutJsValue)override;
	bool canStartGame()override ;
	// event function 
	virtual void onPlayerSitDown( ISitableRoomPlayer* pPlayer ){}
	virtual void onPlayerWillStandUp(ISitableRoomPlayer* pPlayer );
	void playerDoStandUp( ISitableRoomPlayer* pPlayer );

	void onPlayerWillLeaveRoom(stStandPlayer* pPlayer )final;
	virtual uint32_t getLeastCoinNeedForCurrentGameRound(ISitableRoomPlayer* pp) = 0 ;
	uint16_t getEmptySeatCount();
	ISitableRoomPlayer* getPlayerByIdx(uint16_t nIdx );
	bool isSeatIdxEmpty( uint8_t nIdx );
	uint16_t getSitDownPlayerCount();
	uint16_t getSeatCount();
	ISitableRoomPlayer* getReuseSitableRoomPlayerObject();
	virtual ISitableRoomPlayer* doCreateSitableRoomPlayer() = 0 ;
	uint16_t getPlayerCntWithState( uint32_t nState );
	ISitableRoomPlayer* getSitdownPlayerBySessionID(uint32_t nSessionID);
	ISitableRoomPlayer* getSitdownPlayerByUID(uint32_t nUserUID );
	bool onMessage( stMsg* prealMsg , eMsgPort eSenderPort , uint32_t nPlayerSessionID )override;
	virtual  uint32_t coinNeededToSitDown() = 0;
	virtual void prepareCards() = 0 ;
	void onGameDidEnd()override ;
	void onGameWillBegin()override ;
	void doProcessNewPlayerHalo();
private:
	time_t m_tTimeCheckRank ;
	uint16_t m_nSeatCnt ;
	ISitableRoomPlayer** m_vSitdownPlayers ;
private:
	LIST_SITDOWN_PLAYERS m_vReserveSitDownObject ;
protected:
	VEC_SITDOWN_PLAYERS m_vSortByPeerCardsAsc ;
};