#include "TaxasRoom.h"
#include "LogManager.h"
#include "TaxasRoomState.h"
#include "TaxasServerApp.h"
#include "TaxasPokerPeerCard.h"
CTaxasRoom::CTaxasRoom()
{
	nRoomID = 0 ;
	memset(&m_stRoomConfig,0,sizeof(m_stRoomConfig));
	memset(m_vAllState,0,sizeof(m_vAllState)) ;
	m_eCurRoomState = eRoomState_TP_MAX ;
}

CTaxasRoom::~CTaxasRoom()
{
	for ( uint8_t nIdx = 0 ; nIdx < eRoomState_TP_MAX; ++nIdx )
	{
		if ( m_vAllState[nIdx] )
		{
			delete m_vAllState[nIdx] ;
			m_vAllState[nIdx] = NULL ;
		}
	}
}

bool CTaxasRoom::Init( uint32_t nRoomID,stTaxasRoomConfig* pRoomConfig )
{
	GoToState(eRoomState_TP_WaitJoin) ;
	m_nBankerIdx = 0;
	m_nLittleBlindIdx = 0;
	m_nBigBlindIdx = 0;
	m_nCurWaitPlayerActionIdx = 0;
	m_nCurMainBetPool = 0;
	m_nMostBetCoinThisRound = 0;
	memset(m_vPublicCardNums,0,sizeof(m_vPublicCardNums)) ;
	m_nBetRound = 0 ;
	memset(m_vSitDownPlayers,0,sizeof(m_vSitDownPlayers)) ;
	for ( uint8_t nIdx = 0 ; nIdx < MAX_PEERS_IN_TAXAS_ROOM ; ++nIdx )
	{
		m_vAllVicePools[nIdx].nIdx = nIdx ;
		m_vAllVicePools[nIdx].Reset();
	}
	m_vAllPeers.clear();
	m_nLittleBlind = pRoomConfig->nBigBlind * 0.2 ;
	m_tPoker.InitTaxasPoker();
	if ( pRoomConfig->nMaxSeat > MAX_PEERS_IN_TAXAS_ROOM )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("config maxt is too big = %d",pRoomConfig->nMaxSeat ) ;
		pRoomConfig->nMaxSeat = MAX_PEERS_IN_TAXAS_ROOM ;
	}
	return true ;
}

void CTaxasRoom::GoToState( eRoomState eState )
{
	if ( eState >= eRoomState_TP_MAX )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("unknown target room state = %d",eState ) ;
		eState = eRoomState_TP_WaitJoin ;
	}

	if ( m_vAllState[m_eCurRoomState] )
	{
		m_vAllState[m_eCurRoomState]->LeaveState();
	}

	m_eCurRoomState = eState ;
	if ( m_vAllState[m_eCurRoomState] == NULL )
	{
		m_vAllState[m_eCurRoomState] = CreateRoomState(m_eCurRoomState) ;
	}

	if ( m_vAllState[m_eCurRoomState] )
	{
		m_vAllState[m_eCurRoomState]->EnterState(this) ;
	}
	else
	{
		CLogMgr::SharedLogMgr()->ErrorLog("why cur state = %d is null" , m_eCurRoomState) ;
	}

	// send msg to tell client ;
}

void CTaxasRoom::Update(float fDeta )
{
	if ( m_vAllState[m_eCurRoomState] )
	{
		m_vAllState[m_eCurRoomState]->Update(fDeta);
	}
}

CTaxasBaseRoomState* CTaxasRoom::CreateRoomState( eRoomState eState )
{
	CTaxasBaseRoomState* pState = NULL ;
	switch (eState)
	{
	case eRoomState_TP_WaitJoin:
		{
			pState = new CTaxasStateWaitJoin; 
		}
		break;
	case eRoomState_TP_BetBlind:
		{
			pState = new CTaxasStateBlindBet ;
		}
		break;
	case eRoomState_TP_PrivateCard:
		{
			pState = new CTaxasStatePrivateCard ;
		}
		break;
	case eRoomState_TP_Beting:
		{
			pState = new CTaxasStatePlayerBet ;
		}
		break;
	case eRoomState_TP_OneRoundBetEndResult:
		{
			pState = new CTaxasStateOneRoundBetEndResult ;
		}
		break;
	case eRoomState_TP_PublicCard:
		{
			pState = new CTaxasStatePublicCard ;
		}
		break;
	case eRoomState_TP_GameResult:
		{
			pState = new CTaxasStateGameResult ;
		}
		break;
	default:
		CLogMgr::SharedLogMgr()->ErrorLog("create null room state id = %d ",eState ) ;
		return NULL ;
	}
	return pState ;
}

void CTaxasRoom::SendRoomMsg(stMsg* pMsg, uint16_t nLen )
{
	VEC_IN_ROOM_PEERS::iterator iter = m_vAllPeers.begin();
	for ( ; iter != m_vAllPeers.end(); ++iter )
	{
		stTaxasInRoomPeerData* pPeer = *iter ;
		if ( pPeer )
		{
			SendMsgToPlayer(pPeer->nSessionID,pMsg,nLen) ;
		}
		else
		{
			CLogMgr::SharedLogMgr()->ErrorLog("why have null peer in m_vAllPeers") ;
		}
	}
}

void CTaxasRoom::SendMsgToPlayer( uint32_t nSessionID, stMsg* pMsg, uint16_t nLen  )
{
	CTaxasServerApp::SharedGameServerApp()->SendMsg(nSessionID,(char*)pMsg,nLen) ;
}

void CTaxasRoom::OnMessage( stMsg* prealMsg , eMsgPort eSenderPort , uint32_t nPlayerSessionID )
{
 
	if ( m_vAllState[m_eCurRoomState] )
	{
		m_vAllState[m_eCurRoomState]->OnMessage(prealMsg,eSenderPort,nPlayerSessionID);
	}
}

// logic function 
uint8_t CTaxasRoom::GetPlayerCntWithState(eRoomPeerState eState )
{
	uint8_t nCnt = 0 ;
	for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx)
	{
		if ( m_vSitDownPlayers[nIdx].IsInvalid() )
		{
			continue; 
		}

		if ( m_vSitDownPlayers[nIdx].IsHaveState(eState) )
		{
			++nCnt ;
		}
	}
	return nCnt ;
}

void CTaxasRoom::StartGame()
{
	// parepare all players ;
	for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx)
	{
		if ( m_vSitDownPlayers[nIdx].IsInvalid() )
		{
			continue; 
		}

		stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
		pData.nAllBetCoin = 0 ;
		pData.nBetCoinThisRound = 0 ;
		pData.nWinCoinThisGame = 0 ;
		pData.nStateFlag = eRoomPeer_CanAct ;
		pData.eCurAct = eRoomPeerAction_None ;
		memset(pData.vHoldCard,0,sizeof(pData.vHoldCard)) ;
	}

	// prepare running data 
	m_nCurWaitPlayerActionIdx = -1;
	m_nCurMainBetPool = 0;
	m_nMostBetCoinThisRound = 0;
	memset(m_vPublicCardNums,0,sizeof(m_vPublicCardNums)) ;
	m_nBetRound = 0 ;
	for ( uint8_t nIdx = 0 ; nIdx < MAX_PEERS_IN_TAXAS_ROOM ; ++nIdx )
	{
		m_vAllVicePools[nIdx].nIdx = nIdx ;
		m_vAllVicePools[nIdx].Reset();
	}
	m_tPoker.RestAllPoker();

	// init running data 
	m_nBankerIdx = GetFirstInvalidIdxWithState(m_nBankerIdx + 1 , eRoomPeer_CanAct) ;
	m_nLittleBlindIdx = GetFirstInvalidIdxWithState(m_nBankerIdx + 1 , eRoomPeer_CanAct) ;
	m_nBigBlindIdx = GetFirstInvalidIdxWithState(m_nLittleBlindIdx + 1 , eRoomPeer_CanAct) ;

	// bet coin this 
	m_vSitDownPlayers[m_nLittleBlindIdx].BetCoin( m_nLittleBlind ) ;
	m_vSitDownPlayers[m_nBigBlindIdx].BetCoin( m_nLittleBlind * 2 ) ;
	m_nMostBetCoinThisRound = m_nLittleBlind * 2 ;
}

void CTaxasRoom::DistributePrivateCard()
{
	for ( uint8_t nCardIdx = 0 ; nCardIdx < TAXAS_PEER_CARD ; ++nCardIdx )
	{
		for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx)
		{
			if ( m_vSitDownPlayers[nIdx].IsInvalid() || ( m_vSitDownPlayers[nIdx].IsHaveState(eRoomPeer_CanAct) == false ) )
			{
				continue; 
			}
			m_vSitDownPlayers[nIdx].vHoldCard[nCardIdx] = m_tPoker.GetCardWithCompositeNum() ;
		}
	}
}

void CTaxasRoom::PreparePlayersForThisRoundBet()
{
	for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx)
	{
		if ( m_vSitDownPlayers[nIdx].IsInvalid() || ( m_vSitDownPlayers[nIdx].IsHaveState(eRoomPeer_StayThisRound) == false ) )
		{
			continue; 
		}

		stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
		pData.nBetCoinThisRound = 0 ;
		pData.eCurAct = eRoomPeerAction_None ;
	}
	m_nMostBetCoinThisRound = 0 ;

	if ( m_nCurWaitPlayerActionIdx >= 0 )  // means not first round 
	{
		m_nCurWaitPlayerActionIdx = m_nLittleBlindIdx - 1 ; //  little blid begin act   ps: m_nCurWaitPlayerActionIdx = GetFirstInvalidIdxWithState( m_nCurWaitPlayerActionIdx + 1 ,eRoomPeer_CanAct) ;
	}
}

uint8_t CTaxasRoom::InformPlayerAct()
{
	if ( m_nCurWaitPlayerActionIdx < 0 ) // first round 
	{
		m_nCurWaitPlayerActionIdx = GetFirstInvalidIdxWithState(m_nBigBlindIdx + 1 ,eRoomPeer_CanAct) ;
	}
	else
	{
		m_nCurWaitPlayerActionIdx = GetFirstInvalidIdxWithState( m_nCurWaitPlayerActionIdx + 1 ,eRoomPeer_CanAct) ;
	}
	return m_nCurWaitPlayerActionIdx ;
}

void CTaxasRoom::OnPlayerActTimeOut()
{
	if ( m_nMostBetCoinThisRound == 0 )
	{
		m_vSitDownPlayers[m_nCurWaitPlayerActionIdx].eCurAct = eRoomPeerAction_Pass ;
	}
	else
	{
		m_vSitDownPlayers[m_nCurWaitPlayerActionIdx].eCurAct = eRoomPeerAction_GiveUp ;
	}
	// send msg to tell client ;
}

bool CTaxasRoom::IsThisRoundBetOK()
{
	if ( GetPlayerCntWithState(eRoomPeer_CanAct) <= 1 )
	{
		return true ;
	}

	for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx)
	{
		if ( m_vSitDownPlayers[nIdx].IsInvalid() )
		{
			continue; 
		}

		stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
		
		if ( pData.IsHaveState(eRoomPeer_CanAct) && (pData.eCurAct == eRoomPeerAction_None || pData.nBetCoinThisRound != m_nMostBetCoinThisRound ) )
		{
			return false ;
		}
	}

	return true ;
}

 // return produced vice pool cunt this round ;
uint8_t CTaxasRoom::CaculateOneRoundPool()
{
	// check build vice pool
	uint8_t nBeforeVicePoolIdx = GetFirstCanUseVicePool().nIdx ;
	uint64_t nVicePool = 0 ;
	while ( true )
	{
		// find maybe pool 
		nVicePool = 0 ;
		for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx )
		{
			if ( m_vSitDownPlayers[nIdx].IsInvalid() || ( m_vSitDownPlayers[nIdx].IsHaveState(eRoomPeer_WaitCaculate) == false ) )
			{
				continue;
			}

			stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
			if ( pData.eCurAct == eRoomPeerAction_AllIn && pData.nBetCoinThisRound > 0 )
			{
				if ( pData.nBetCoinThisRound < nVicePool || nVicePool == 0 )
				{
					nVicePool = pData.nBetCoinThisRound ;
				}
			}
		}

		if ( nVicePool == 0 )
		{
			break;
		}

		// real build pool;
		stVicePool& pPool = GetFirstCanUseVicePool();
		pPool.bUsed = true ;
		pPool.nCoin = m_nCurMainBetPool ;
		m_nCurMainBetPool = 0 ;
		
		// put player idx in pool ;
		CLogMgr::SharedLogMgr()->PrintLog("build pool pool idx = %d",pPool.nIdx ) ;
		for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx )
		{
			if ( m_vSitDownPlayers[nIdx].IsInvalid() || ( m_vSitDownPlayers[nIdx].IsHaveState(eRoomPeer_WaitCaculate) == false ) )
			{
				continue;
			}

			stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
			if ( pData.nBetCoinThisRound > 0 )
			{
				pPool.nCoin += nVicePool ;
				pData.nBetCoinThisRound -= nVicePool ;
				pPool.vInPoolPlayerIdx.push_back(nIdx) ;
				CLogMgr::SharedLogMgr()->PrintLog("put player into pool player Idx = %d, UID = %d",nIdx,pData.nUserUID ) ;
			}
		}
		CLogMgr::SharedLogMgr()->PrintLog("pool idx = %d : coin = %I64d",pPool.nIdx,pPool.nCoin) ;
	}

	// build mian pool ;
	CLogMgr::SharedLogMgr()->PrintLog("build main pool: " ) ;
	for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx )
	{
		if ( m_vSitDownPlayers[nIdx].IsInvalid() || ( m_vSitDownPlayers[nIdx].IsHaveState(eRoomPeer_CanAct) == false ) )
		{
			continue;
		}

		stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
		if ( pData.nBetCoinThisRound > 0 )
		{
			m_nCurMainBetPool += pData.nBetCoinThisRound ;
			CLogMgr::SharedLogMgr()->PrintLog("put player into Main pool player Idx = %d, UID = %d",nIdx,pData.nUserUID ) ;
		}
	}

	uint8_t nProducedVicePoolCnt = GetFirstCanUseVicePool().nIdx - nBeforeVicePoolIdx;
	CLogMgr::SharedLogMgr()->SystemLog("oneRound Caculate over, mainPool = %I64d, newVicePool = %d",m_nCurMainBetPool,nProducedVicePoolCnt );

	// send msg tell client [ nBeforeVicePoolIdx, GetFirstCanUseVicePoolIdx() ); this set of pool idx are new produced ; not include the last 
	return nProducedVicePoolCnt ;
}

// return dis card cnt ;
uint8_t CTaxasRoom::DistributePublicCard()
{
	// distr 3 
	if ( m_vPublicCardNums[0] == 0 )
	{
		for ( uint8_t nIdx = 0 ; nIdx < 3 ; ++nIdx )
		{
			m_vPublicCardNums[nIdx] = m_tPoker.GetCardWithCompositeNum() ;
		}
		// send msg to tell client ;
		return 3 ;
	}

	if ( m_vPublicCardNums[3] == 0 )
	{
		m_vPublicCardNums[3] = m_tPoker.GetCardWithCompositeNum() ;
		// send msg to tell client ;
		return 1 ;
	}

	if ( m_vPublicCardNums[4] == 0 )
	{
		m_vPublicCardNums[4] = m_tPoker.GetCardWithCompositeNum() ;
		// send msg to tell client ;
		return 1 ;
	}

	CLogMgr::SharedLogMgr()->ErrorLog("already finish public card why one more time ") ;
	return 0 ;
}

//return pool cnt ;
uint8_t CTaxasRoom::CaculateGameResult()
{
	// build a main pool;
	if ( m_nCurMainBetPool > 0 )
	{
		stVicePool& pPool = GetFirstCanUseVicePool();
		pPool.nCoin = m_nCurMainBetPool ;
		pPool.bUsed = true ;
		for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx )
		{
			stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
			if ( pData.IsInvalid() )
			{
				continue;
			}

			if ( pData.IsHaveState(eRoomPeer_CanAct) )
			{
				pPool.vInPoolPlayerIdx.push_back(nIdx) ;
			}
		}
	}

	// cacluate a main pool ;
	for ( uint8_t nIdx = 0 ; nIdx < MAX_PEERS_IN_TAXAS_ROOM; ++nIdx )
	{
		if ( m_vAllVicePools[nIdx].bUsed )
		{
			CaculateVicePool(m_vAllVicePools[nIdx]) ;
		}
	}

	// send msg tell client ;
	if ( GetFirstCanUseVicePool().nIdx == 0 )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("why this game have no pool ? at least should have one room id = %d",nRoomID ) ;
	}
	return GetFirstCanUseVicePool().nIdx ;
}

uint64_t CTaxasRoom::GetAllBetCoinThisRound()
{
	uint64_t nCoinThis = 0 ;
	for ( uint8_t nIdx = 0 ; nIdx < m_stRoomConfig.nMaxSeat ; ++nIdx)
	{
		if ( m_vSitDownPlayers[nIdx].IsInvalid() || ( m_vSitDownPlayers[nIdx].IsHaveState(eRoomPeer_StayThisRound) == false ) )
		{
			continue; 
		}

		stTaxasPeerData& pData = m_vSitDownPlayers[nIdx] ;
		nCoinThis += pData.nBetCoinThisRound;
	}
	return nCoinThis ;
}

bool CTaxasRoom::IsPublicDistributeFinish()
{
	return (m_vPublicCardNums[4] != 0 );
}

uint8_t CTaxasRoom::GetFirstInvalidIdxWithState( uint8_t nIdxFromInclude , eRoomPeerState estate )
{
	for ( uint8_t nIdx = nIdxFromInclude ; nIdx < m_stRoomConfig.nMaxSeat * 2 ; ++nIdx )
	{
		uint8_t nRealIdx = nIdx % m_stRoomConfig.nMaxSeat ;
		if ( m_vSitDownPlayers[nRealIdx].IsInvalid() )
		{
			continue;
		}

		if ( m_vSitDownPlayers[nRealIdx].IsHaveState(estate) )
		{
			return nRealIdx ;
		}
	}
	CLogMgr::SharedLogMgr()->ErrorLog("why don't have peer with state = %d",estate ) ;
	return 0 ;
}

stVicePool& CTaxasRoom::GetFirstCanUseVicePool()
{
	for ( uint8_t nIdx = 0 ; nIdx < MAX_PEERS_IN_TAXAS_ROOM; ++nIdx )
	{
		if ( !m_vAllVicePools[nIdx].bUsed )
		{
			 return m_vAllVicePools[nIdx] ;
		}
	}
	CLogMgr::SharedLogMgr()->ErrorLog("why all vice pool was used ? error ") ;
	return m_vAllVicePools[MAX_PEERS_IN_TAXAS_ROOM-1] ;
}

void CTaxasRoom::CaculateVicePool(stVicePool& pPool )
{
	if ( pPool.nCoin == 0 )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("why this pool coin is 0 ? players = %d room id = %d ",pPool.vInPoolPlayerIdx.size(),nRoomID ) ;
		return ;
	}

	if ( pPool.vInPoolPlayerIdx.empty() )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("why pool coin = %I64d , peers is 0 room id = %d  ",pPool.nCoin,nRoomID ) ;
	}

	// find winner ;
	if ( pPool.vInPoolPlayerIdx.size() == 1 )
	{
		uint8_t nPeerIdx = pPool.vInPoolPlayerIdx[0] ;
		if ( m_vSitDownPlayers[nPeerIdx].IsInvalid() )
		{
			CLogMgr::SharedLogMgr()->ErrorLog("why this winner idx is invalid = %d, system got coin = %I64d",nPeerIdx,pPool.nCoin ) ;
			return ;
		}

		if ( m_vSitDownPlayers[nPeerIdx].IsHaveState(eRoomPeer_WaitCaculate) == false )
		{
			CLogMgr::SharedLogMgr()->ErrorLog("why this winner idx state is invalid = %d, system got coin = %I64d",nPeerIdx,pPool.nCoin ) ;
			return ;
		}
		pPool.vWinnerIdxs.push_back( nPeerIdx ) ;
		m_vSitDownPlayers[nPeerIdx].nTakeInMoney += pPool.nCoin ;
		m_vSitDownPlayers[nPeerIdx].nWinCoinThisGame += pPool.nCoin ;
		return ;
	}

	// pk card
	if ( IsPublicDistributeFinish() == false )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("public is not finish how to pk card ? error room id = %d",nRoomID);
		return ;
	}

	CTaxasPokerPeerCard cardWinner ;
	for ( uint8_t nIdx = 0 ; nIdx < pPool.vInPoolPlayerIdx.size(); ++nIdx )
	{
		uint8_t nPeerIdx = pPool.vInPoolPlayerIdx[0] ;
		stTaxasPeerData& pData = m_vSitDownPlayers[nPeerIdx];
		if ( pData.IsInvalid() )
		{
			CLogMgr::SharedLogMgr()->ErrorLog("why this player in pool idx is invalid = %d, pool idx  = %I64d",nPeerIdx,pPool.nIdx ) ;
			continue; ;
		}

		if ( pData.IsHaveState(eRoomPeer_WaitCaculate) == false )
		{
			CLogMgr::SharedLogMgr()->ErrorLog("why this player in pool state is invalid = %d, pool idx  = %I64d",nPeerIdx,pPool.nIdx ) ;
			continue ;
		}

		if ( pData.vHoldCard[0] == 0 || 0 == pData.vHoldCard[1] )
		{
			CLogMgr::SharedLogMgr()->ErrorLog("why peer idx = %d , uid = %d in pool hold card is invalid ",nPeerIdx,pData.nUserUID ) ;
			continue;
		}

		if ( pPool.vWinnerIdxs.empty() )
		{
			cardWinner.AddCardByCompsiteNum(pData.vHoldCard[0]);
			cardWinner.AddCardByCompsiteNum(pData.vHoldCard[1]);
			for ( uint8_t nPcardIdx = 0 ; nPcardIdx < TAXAS_PUBLIC_CARD ; ++nPcardIdx )
			{
				cardWinner.AddCardByCompsiteNum(m_vPublicCardNums[nPcardIdx]) ;
			}
			pPool.vWinnerIdxs.push_back(nPeerIdx) ;
			continue;
		}

		CTaxasPokerPeerCard curPeer ;
		curPeer.AddCardByCompsiteNum(pData.vHoldCard[0]);
		curPeer.AddCardByCompsiteNum(pData.vHoldCard[1]);
		for ( uint8_t nPcardIdx = 0 ; nPcardIdx < TAXAS_PUBLIC_CARD ; ++nPcardIdx )
		{
			curPeer.AddCardByCompsiteNum(m_vPublicCardNums[nPcardIdx]) ;
		}
		
		int8_t nRet = cardWinner.PK(&curPeer) ;
		if ( nRet < 0 )
		{
			pPool.vWinnerIdxs.clear();
			pPool.vWinnerIdxs.push_back(nPeerIdx) ;

			// switch winner 
			cardWinner.Reset();
			cardWinner.AddCardByCompsiteNum(pData.vHoldCard[0]);
			cardWinner.AddCardByCompsiteNum(pData.vHoldCard[1]);
			for ( uint8_t nPcardIdx = 0 ; nPcardIdx < TAXAS_PUBLIC_CARD ; ++nPcardIdx )
			{
				cardWinner.AddCardByCompsiteNum(m_vPublicCardNums[nPcardIdx]) ;
			}
		}
		else if ( nRet == 0 ) 
		{
			pPool.vWinnerIdxs.push_back(nPeerIdx) ;
		}
	}

	// give coin 
	if ( pPool.vWinnerIdxs.empty() )
	{
		CLogMgr::SharedLogMgr()->ErrorLog("why room id = %d pool idx = %d winner is empty , system got coin = %I64d ",nRoomID,pPool.nIdx,pPool.nCoin ) ;
		return ;
	}

	uint8_t nElasCoin = pPool.nCoin % pPool.vWinnerIdxs.size() ;
	pPool.nCoin -= nElasCoin ;
	if ( nElasCoin > 0 )
	{
		CLogMgr::SharedLogMgr()->PrintLog("system got the elaps coin = %d, room id = %d , pool idx = %d ",nElasCoin,nRoomID,pPool.nIdx ) ;
	}
	uint64_t nCoinPerWinner = pPool.nCoin / pPool.vWinnerIdxs.size() ;
	for ( uint8_t nIdx = 0 ; nIdx < pPool.vWinnerIdxs.size(); ++nIdx )
	{
		stTaxasPeerData& pData = m_vSitDownPlayers[pPool.vWinnerIdxs[nIdx]];
		pData.nTakeInMoney += nCoinPerWinner ;
		pData.nWinCoinThisGame += nCoinPerWinner ;
		CLogMgr::SharedLogMgr()->PrintLog("player use uid = %d win coin = %I64d , from pool idx = %d, room id = %d",pData.nUserUID,nCoinPerWinner,pPool.nIdx,nRoomID) ;
	}
}
