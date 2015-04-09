//
//  ChipGroup.cpp
//  MyPoker
//
//  Created by Xu BILL on 14/12/4.
//
//

#include "ChipGroup.h"
#define TIME_ELASP_COIN 0.08
#define SPEED_COIN_MOVE 1300
#define OFFSET_CHIPGROUP 10
CChipGroup* CChipGroup::CreateGroup()
{
    CChipGroup* pG = new CChipGroup ;
    pG->init() ;
    pG->autorelease() ;
    return pG ;
}

bool CChipGroup::init()
{
    Node::init() ;
    m_funAniFinish = nullptr ;
    m_ptOrigPos = m_ptDestPos = Point::ZERO;
    SetGroupCoin(0);
    return true ;
}

void CChipGroup::SetOriginPosition(Point ptStart )
{
    m_ptOrigPos = ptStart;
}

void CChipGroup::SetDestPosition(Point ptEnd )
{
    m_ptDestPos = ptEnd ;   // ptEnd Defined in chipgroup parent coordinate ;
}

void CChipGroup::SetFinishCallBack(std::function<void (CChipGroup*)> pFunc)
{
    m_funAniFinish = pFunc ;
}

void CChipGroup::Start(eChipMoveType eType , float fAniTime )
{
    if ( m_vAllSprites.empty() )
    {
        CCLOG("sprite is null , cannot animate");
        return ;
    }
    assert(getParent() != nullptr && "no parent");
   
    // decide dest target;
    Point ptDest = m_ptDestPos ;
    Point ptOrigDest = m_ptOrigPos;
    m_ptOrigPos = m_ptDestPos ;
    m_ptDestPos = Point::ZERO ;
    
    ptOrigDest = getParent()->convertToWorldSpace(ptOrigDest);
    ptOrigDest = convertToNodeSpace(ptOrigDest);
    
    ptDest = getParent()->convertToWorldSpace(ptDest);
    ptDest = convertToNodeSpace(ptDest);

    float fDistance = ptDest.distance(ptOrigDest);
    float fTimeMove = fDistance / SPEED_COIN_MOVE ;
    switch (eType)
    {
        case eChipMove_Group2None:
        {
            for ( size_t nIdx = 0 ; nIdx < m_vAllSprites.size() ; ++nIdx )
            {
                DelayTime* pdelay = DelayTime::create( nIdx * TIME_ELASP_COIN );
                MoveTo* pMoveto = MoveTo::create(fTimeMove, ptDest);
                Hide* ph = Hide::create();
                CallFuncN* pFunc = nullptr;
                if ( nIdx == m_vAllSprites.size() -1 )
                {
                    pFunc = CallFuncN::create([this](Node*pnode){ pnode->setVisible(false); if (m_funAniFinish ) m_funAniFinish(this); } );
                }
                Sequence* pSeq = Sequence::create(pdelay,pMoveto,ph,pFunc,NULL);
                m_vAllSprites[m_vAllSprites.size() - 1 - nIdx]->runAction(pSeq);
            }
        }
            break;
        case eChipMove_Group2Group:
        {
            for ( size_t nIdx = 0 ; nIdx < m_vAllSprites.size() ; ++nIdx )
            {
                DelayTime* pdelay = DelayTime::create( nIdx * TIME_ELASP_COIN );
                MoveTo* pMoveto = MoveTo::create(fTimeMove, Point(ptDest.x,ptDest.y + nIdx * OFFSET_CHIPGROUP ));
                CallFuncN* changeOrder = CallFuncN::create([this](Node*pnode){ Node* parent = pnode->getParent(); pnode->retain(); pnode->removeFromParent(); parent->addChild(pnode);pnode->release(); });
                CallFuncN* pFunc = nullptr;
                if ( nIdx == m_vAllSprites.size() -1 )
                {
                    pFunc = CallFuncN::create([this](Node*pnode){ if (m_funAniFinish ) m_funAniFinish(this); } );
                }
                Sequence* pSeq = Sequence::create(pdelay,pMoveto,changeOrder,pFunc,NULL);
                Node* pTemp = m_vAllSprites[m_vAllSprites.size() - 1 - nIdx];
                pTemp->runAction(pSeq);
            }
        }
            break;
        case eChipMove_Origin2None:
        {
            m_ptDestPos = m_ptOrigPos;
            Start(eChipMove_Group2None,fAniTime );
        }
            break;
        case eChipMove_Origin2Group:
        {
            m_ptDestPos = m_ptOrigPos;
            Start(eChipMove_None2Group,fAniTime );
        }
            break;
        case eChipMove_None2Group:
        {
            for ( size_t nIdx = 0 ; nIdx < m_vAllSprites.size() ; ++nIdx )
            {
                DelayTime* pdelay = DelayTime::create( nIdx * TIME_ELASP_COIN );
                Show* pShow = Show::create();
                MoveTo* pMoveto = MoveTo::create(fTimeMove, Point(ptDest.x,ptDest.y + nIdx * OFFSET_CHIPGROUP ));
                Sequence* pSeq = nullptr;
                if ( nIdx == m_vAllSprites.size() -1 )
                {
                    CallFunc* pFunc = CallFunc::create([this](){ if (m_funAniFinish ) m_funAniFinish(this); });
                    pSeq = Sequence::create(pdelay,pShow,pMoveto,pFunc,NULL);
                }
                else
                {
                    pSeq = Sequence::create(pdelay,pShow,pMoveto,NULL);
                }
                m_vAllSprites[nIdx]->runAction(pSeq);
            }
        }
            break ;
        default:
            break;
    }
}

void CChipGroup::SetGroupCoin( uint64_t nAllCoin  , bool bVisible )
{
    m_nCoin = nAllCoin ;
    
    // clear all sprite
    m_vAllSprites.clear();
    removeAllChildren() ;
    m_ptOrigPos = getPosition();
    m_ptDestPos = getPosition() ;
    static int id = 0 ;
    ++id;
    // prepare sprite with new all coin count ;
    uint64_t nIdx = 0 ;
    while ( nIdx < nAllCoin )
    {
        Sprite* ps = Sprite::create(StringUtils::format("%d.png",id%2)) ; //Sprite::create(StringUtils::format("%lld.png",nIdx++%2)) ;
        addChild(ps);
        //ps->setRotation(random()%360);
        ps->setScale(0.5);
        m_vAllSprites.push_back(ps);
        ++nIdx;
    }
    
    // pack coin sprite ;
    for ( size_t nIdx = 0 ; nIdx < m_vAllSprites.size(); ++nIdx )
    {
        m_vAllSprites[nIdx]->setVisible(bVisible);
        m_vAllSprites[nIdx]->setPosition(Point(0,nIdx * OFFSET_CHIPGROUP ));
    }
}

uint64_t CChipGroup::GetGroupCoin()
{
    return m_nCoin ;
}