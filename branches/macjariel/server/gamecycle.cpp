#include "gamecycle.h"
#include "gametable.h"
#include "gameexceptions.h"
#include "game.h"
#include "player.h"
#include "gameeventhandler.h"
#include "gameeventbroadcaster.h"
#include "reactioncard.h"
#include "characterbase.h"
#include <QDebug>

GameCycle::GameCycle(Game* game):
        mp_game(game),
        m_state(GAMEPLAYSTATE_INVALID),
        mp_currentPlayer(0),
        mp_requestedPlayer(0)
{
}

void GameCycle::assertDraw() const
{
    if (!isDraw())
        throw BadGamePlayStateException(m_state, GAMEPLAYSTATE_DRAW);
}

void GameCycle::assertTurn() const
{
    if (!isTurn())
        throw BadGamePlayStateException(m_state, GAMEPLAYSTATE_TURN);
}

void GameCycle::assertResponse() const
{
    if (!isResponse())
        throw BadGamePlayStateException(m_state, GAMEPLAYSTATE_RESPONSE);
}

 void GameCycle::assertDiscard() const
 {
     if (!isDiscard())
         throw BadGamePlayStateException(m_state, GAMEPLAYSTATE_DISCARD);
 }


GameContextData GameCycle::gameContextData() const
{
    GameContextData res;
    res.currentPlayerId   = currentPlayer()->id();
    res.requestedPlayerId = requestedPlayer()->id();
    res.turnNumber        = turnNumber();
    res.gamePlayState     = gamePlayState();
    return res;
}

void GameCycle::start()
{
    Player* player;
    foreach (player, mp_game->playerList())
        if (player->role() == ROLE_SHERIFF)
            break;
    Q_ASSERT(player->role() == ROLE_SHERIFF);
    qDebug("Starting game cycle.");
    m_turnNum = 0;
    startTurn(player);
    sendRequest();
}

void GameCycle::startTurn(Player* player)
{

    qDebug(qPrintable(QString("GameCycle: startTurn(%1)").arg(player->id())));
    if (player->role() == ROLE_SHERIFF)
        m_turnNum++;
    mp_currentPlayer = mp_requestedPlayer = player;
    m_state = GAMEPLAYSTATE_DRAW;
    mp_currentPlayer->onTurnStart();
    announceContextChange();
    m_drawCardCount = 0;
    m_drawCardMax = 2;
}

void GameCycle::draw(Player* player, bool specialDraw)
{
    checkPlayerAndState(player, GAMEPLAYSTATE_DRAW);
    player->predrawCheck(0);
    player->character()->draw(specialDraw);
    m_state = GAMEPLAYSTATE_TURN;
    sendRequest();
}


void GameCycle::skipPlayersTurn()
{
    startTurn(mp_game->nextPlayer(mp_currentPlayer));
}

void GameCycle::finishTurn(Player* player)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if ((m_state != GAMEPLAYSTATE_TURN) && (m_state != GAMEPLAYSTATE_DISCARD))
        throw BadGameStateException();

    if (needDiscard(player))
        throw TooManyCardsInHandException();

    startTurn(mp_game->nextPlayer(mp_currentPlayer));
    sendRequest();
}

void GameCycle::discardCard(Player* player, PlayingCard* card)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if (m_state != GAMEPLAYSTATE_TURN && m_state != GAMEPLAYSTATE_DISCARD)
        throw BadGameStateException();

    if (needDiscard(player) == 0)
        throw BadGameStateException();

    mp_game->gameTable().playerDiscardCard(card);
    m_state = GAMEPLAYSTATE_DISCARD;

    if (needDiscard(player) == 0)
        startTurn(mp_game->nextPlayer(mp_currentPlayer));
    sendRequest();
}

void GameCycle::playCard(Player* player, PlayingCard* card)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if (card->owner() !=  0 && card->owner() != player) {
        throw BadCardException();
    }

    if (isResponse()) {
        player->character()->respondCard(m_reactionHandlers.head(), card);
    } else {
        player->character()->playCard(card);
    }
    sendRequest();
}

void GameCycle::playCard(Player* player, PlayingCard* card, Player* targetPlayer)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if (card->owner() !=  0 && card->owner() != player) {
        throw BadCardException();
    }

    if (!targetPlayer->isAlive())
        throw BadTargetPlayerException();

    if (isResponse())
        throw BadGameStateException();

    player->character()->playCard(card, targetPlayer);
    sendRequest();
}

void GameCycle::playCard(Player* player, PlayingCard* card, PlayingCard* targetCard)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if (card->owner() !=  0 && card->owner() != player) {
        throw BadCardException();
    }

    if (isResponse())
        throw BadGameStateException();

    player->character()->playCard(card, targetCard);
    sendRequest();
}


void GameCycle::pass(Player* player)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if (m_state != GAMEPLAYSTATE_RESPONSE)
        throw BadGameStateException();

    player->character()->respondPass(m_reactionHandlers.head());
    sendRequest();
}


void GameCycle::setResponseMode(ReactionHandler* reactionHandler, Player* player)
{
    if (m_reactionHandlers.isEmpty()) {
        Q_ASSERT(m_state != GAMEPLAYSTATE_RESPONSE);
        m_lastState = m_state;
    }

    m_reactionHandlers.enqueue(reactionHandler);
    m_reactionPlayers.enqueue(player);

    if (m_reactionHandlers.size() == 1) {
        mp_requestedPlayer = player;
        m_state = GAMEPLAYSTATE_RESPONSE;
        announceContextChange();
    }
}

void GameCycle::unsetResponseMode()
{
    Q_ASSERT(m_state == GAMEPLAYSTATE_RESPONSE);
    m_reactionHandlers.dequeue();
    m_reactionPlayers.dequeue();
    if (!m_reactionHandlers.isEmpty()) {
        mp_requestedPlayer = m_reactionPlayers.head();
    } else {
        mp_requestedPlayer = mp_currentPlayer;
        m_state = m_lastState;
    }
    announceContextChange();
}

void GameCycle::sendRequest()
{
    if (mp_game->isFinished())
        return;

    if (!mp_requestedPlayer->isAlive()) {
        Q_ASSERT(m_state != GAMEPLAYSTATE_RESPONSE);
        startTurn(mp_game->nextPlayer(mp_currentPlayer));
    }

    ActionRequestType requestType;
    switch(m_state) {
        case GAMEPLAYSTATE_DRAW:
            requestType = REQUEST_DRAW;
            break;
        case GAMEPLAYSTATE_TURN:
            requestType = REQUEST_PLAY;
            break;
        case GAMEPLAYSTATE_RESPONSE:
            requestType = REQUEST_RESPOND;
            break;
        case GAMEPLAYSTATE_DISCARD:
            requestType = REQUEST_DISCARD;
            break;
        default:
            NOT_REACHED();
    }
    qDebug(qPrintable(QString("GameCycle: sendRequest to #%1 (%2)").arg(mp_requestedPlayer->id()).arg(mp_requestedPlayer->name())));

    mp_requestedPlayer->gameEventHandler()->onActionRequest(requestType);
}

void GameCycle::checkPlayerAndState(Player* player, GamePlayState state)
{
    if (player != mp_requestedPlayer)
        throw BadPlayerException(mp_currentPlayer->id());

    if (state != m_state)
        throw BadGameStateException();
}

int GameCycle::needDiscard(Player* player)
{
    int lifePoints = player->lifePoints();
    int handSize = player->handSize();
    return lifePoints > handSize ? 0 : handSize - lifePoints;
}

void GameCycle::announceContextChange()
{
    mp_game->gameEventBroadcaster().onGameContextChange(gameContextData());
}