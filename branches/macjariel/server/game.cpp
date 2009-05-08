/***************************************************************************
 *   Copyright (C) 2008 by MacJariel                                       *
 *   echo "badmailet@gbalt.dob" | tr "edibmlt" "ecrmjil"                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QXmlStreamWriter>
#include <QTimer>

#include <stdlib.h>

#include "game.h"
#include "gameserver.h"
#include "client.h"
#include "player.h"
#include "common.h"
#include "cardbeer.h"
#include "util.h"
#include "gameexceptions.h"


#include "gameinfo.h"
#include "gametable.h"
#include "gamecycle.h"
#include "gameeventbroadcaster.h"
#include "gamelogger.h"
#include "characterbase.h"
#include "characterlist.h"
#include "playerreaper.h"
#include "voidai.h"



Game::Game(GameServer* parent, int gameId, const CreateGameData& createGameData):
    QObject(parent),
    m_id(gameId),
    m_state(GAMESTATE_WAITINGFORPLAYERS),
    m_publicGameView(this),
    m_nextUnusedPlayerId(0),
    m_startable(0)
{
    mp_gameInfo = new GameInfo(createGameData);
    mp_gameTable = new GameTable(this);
    mp_gameCycle = new GameCycle(this);
    mp_gameEventBroadcaster = new GameEventBroadcaster();
    mp_beerRescue = new BeerRescue(this);
    mp_defaultPlayerReaper = new PlayerReaper(this);
    mp_playerReaper = mp_defaultPlayerReaper;
    mp_gameLogger = new GameLogger();
    mp_gameEventBroadcaster->registerHandler(mp_gameLogger, 0);
    createAI(mp_gameInfo->AIPlayers());
}

Game::~Game()
{
    delete mp_gameInfo;
    delete mp_gameTable;
    delete mp_gameCycle;
    delete mp_gameEventBroadcaster;
    delete mp_defaultPlayerReaper;
    delete mp_gameLogger;
}

int Game::alivePlayersCount() const
{
    return m_goodGuysCount + m_outlawsCount + m_renegadesCount;
}

Player* Game::player(int playerId)
{
    if (m_playerMap.contains(playerId))
        return m_playerMap[playerId];
    return 0;
}

Player* Game::nextPlayer(Player* currentPlayer) {
    QListIterator<Player*> it(m_playerList);
    if (!it.findNext(currentPlayer))
        return 0;
    // Now the it points right after the currentPlayer

    // If the currentPlayer is last in list, rewind
    if (!it.hasNext())
        it.toFront();

    while (!it.peekNext()->isAlive()) {
        // @invariant: the player after the iterator is dead
        it.next();
        if (!it.hasNext())
            it.toFront();
    }
    // The player after the iterator is the first alive player
    // after the currentPlayer
    return it.next();
}

int Game::getDistance(Player *fromPlayer, Player *toPlayer) const
{
    static int infiniteDistance = 99;
    if (!fromPlayer->isAlive() || !toPlayer->isAlive())
        return infiniteDistance;

    int fromIndex = m_playerList.indexOf(fromPlayer);
    int toIndex   = m_playerList.indexOf(toPlayer);
    if (fromIndex == -1 || toIndex == -1)
        return infiniteDistance;

    int upIndex   = fromIndex;
    int downIndex = fromIndex;
    int baseDistance = 0;
    while (upIndex != toIndex && downIndex != toIndex) {
        do {
            upIndex++;
            if (upIndex == m_playerList.size()) upIndex = 0;
        } while (!m_playerList[upIndex]->isAlive());
        do {
            downIndex--;
            if (downIndex == -1) downIndex = m_playerList.size() - 1;
        } while (!m_playerList[downIndex]->isAlive());
        baseDistance++;
    }

    baseDistance -= fromPlayer->distanceOut();
    baseDistance += toPlayer->distanceIn();
    return baseDistance;
}

Player* Game::createPlayer(const CreatePlayerData& createPlayerData, GameEventHandler* handler)
{
    if (m_state != GAMESTATE_WAITINGFORPLAYERS) {
        throw BadGameStateException();
    }
    while ((m_nextUnusedPlayerId == 0) || m_playerMap.contains(m_nextUnusedPlayerId))
    {
        m_nextUnusedPlayerId++;
    }
    Player* newPlayer = new Player(this, m_nextUnusedPlayerId, createPlayerData);
    m_playerMap[m_nextUnusedPlayerId] = newPlayer;
    m_playerList.append(newPlayer);
    m_publicPlayerList.append(&newPlayer->publicView());

    // The first connected nonAI player is the creator and can start the game
    if (mp_gameInfo->creatorId() == 0 && !handler->isAI()) {
        mp_gameInfo->setCreatorId(m_nextUnusedPlayerId);
    }

    gameEventBroadcaster().onPlayerJoinedGame(newPlayer);

    newPlayer->registerGameEventHandler(handler);
    checkStartable();
    return newPlayer;
}

void Game::createAI(int count)
{
    for(int i = 0; i < count; ++i) {
        VoidAI* ai = new VoidAI(this);
        createPlayer(ai->createPlayerData(), ai);
    }
}

void Game::replacePlayer(Player* player, const CreatePlayerData& createPlayerData,
                         GameEventHandler* gameEventHandler)
{
    Q_ASSERT(m_playerList.contains(player));
    if (player->gameEventHandler() != 0)
        throw BadTargetPlayerException();
    if (player->password() != createPlayerData.password)
        throw BadPlayerPasswordException();
    ///@todo: rename player
    player->registerGameEventHandler(gameEventHandler);
}

/**
 * @TODO: When the creator disconnects, cancel the game.
 */
void Game::removePlayer(Player* player)
{
    Q_ASSERT(player->game() == this);
    int playerId = player->id();
    Q_ASSERT(m_playerMap.contains(playerId));
    Q_ASSERT(m_playerMap[playerId] == player);
    qDebug(qPrintable(QString("Removing player #%1.").arg(playerId)));

    if (player->isCreator() && m_state == GAMESTATE_WAITINGFORPLAYERS) {
        foreach(Player* p, m_playerList) {
            p->unregisterGameEventHandler();
        }
        GameServer::instance().removeGame(this);
        return;
    }
    m_publicPlayerList.removeAll(&player->publicView());
    m_playerList.removeAll(player);
    m_playerMap.remove(playerId);

    player->unregisterGameEventHandler();
    gameEventBroadcaster().onPlayerLeavedGame(player);
    if (m_state == GAMESTATE_WAITINGFORPLAYERS)
        checkStartable();
    player->deleteLater();
}

void Game::buryPlayer(Player* player, Player* causedBy)
{
    Q_ASSERT(player->lifePoints() <= 0);
    Q_ASSERT(player->isAlive());
    player->setAlive(0);

    mp_playerReaper->cleanUpCards(player);

    gameEventBroadcaster().onPlayerDied(player, causedBy);

    switch(player->role()) {
        case ROLE_SHERIFF:
        case ROLE_DEPUTY:
            m_goodGuysCount--;
            break;
        case ROLE_OUTLAW:
            m_outlawsCount--;
            break;
        case ROLE_RENEGADE:
            m_renegadesCount--;
            break;
        default:
            NOT_REACHED();
    }

    /// game winning condition check
    if (player->role() == ROLE_SHERIFF) {
        if (m_outlawsCount > 0 || m_goodGuysCount > 0)
            winningSituation(ROLE_OUTLAW);
        else
            winningSituation(ROLE_RENEGADE);
    } else if (m_outlawsCount == 0 && m_renegadesCount == 0) {
        winningSituation(ROLE_SHERIFF);
    } else if (player->role() == ROLE_OUTLAW && causedBy != 0) {
        /// killer draws 3 cards for killing an outlaw
        mp_gameTable->playerDrawFromDeck(causedBy, 3);
    } else if (player->role() == ROLE_DEPUTY && causedBy->role() == ROLE_SHERIFF) {
        /// sheriff killed his deputy and has to cancel all his cards
        foreach(PlayingCard* card, causedBy->hand())
            gameTable().cancelCard(card);
        foreach(PlayingCard* card, causedBy->table())
            gameTable().cancelCard(card);
    }
}

void Game::winningSituation(PlayerRole winners)
{
    m_state = GAMESTATE_FINISHED;

    /// @todo: announce game over
}

void Game::setPlayerReaper(PlayerReaper* playerReaper)
{
    mp_playerReaper = playerReaper;
}

void Game::unsetPlayerReaper()
{
    mp_playerReaper = mp_defaultPlayerReaper;
}


void Game::startGame(Player* player)
{
    if (player->id() != mp_gameInfo->creatorId()) {
        throw BadPlayerException(player->id());
    }
    if (m_state != GAMESTATE_WAITINGFORPLAYERS) {
        throw BadGameStateException();
    }
    if (!m_startable) {
        throw BadGameStateException();
    }
    m_state = GAMESTATE_PLAYING;
    if (mp_gameInfo->hasShufflePlayers())
        shufflePlayers();

    setRolesAndCharacters();

    gameEventBroadcaster().onGameStarted();
    mp_gameTable->prepareGame(GameServer::instance().cardFactory());
    mp_gameCycle->start();
}


void Game::sendMessage(Player* player, const QString& message)
{
    gameEventBroadcaster().onChatMessage(player, message);
}

void Game::checkStartable()
{
    bool newStartable;
    if (m_playerList.count() >= mp_gameInfo->minPlayers() &&
            m_playerList.count() <= mp_gameInfo->maxPlayers())
        newStartable = 1;
    else
        newStartable = 0;
    if (m_startable != newStartable)
        m_playerMap[mp_gameInfo->creatorId()]->gameEventHandler()->onGameStartabilityChanged(newStartable);
    m_startable = newStartable;
}

void Game::shufflePlayers()
{
    shuffleList(m_playerList);
    m_publicPlayerList.clear();
    foreach(Player* player, m_playerList) {
        m_publicPlayerList.append(&player->publicView());
    }
}

void Game::setRolesAndCharacters()
{
    QList<PlayerRole> roles = getRoleList();
    CharacterList characters = CharacterList(this, m_playerList.size());
    shuffleList(roles);
    QListIterator<Player*> pIt(m_playerList);
    QListIterator<PlayerRole> rIt(roles);
    QListIterator<CharacterBase*> cIt(characters);
    int i = 0;
    m_goodGuysCount = m_outlawsCount = m_renegadesCount = 0;
    while(pIt.hasNext() && rIt.hasNext() && cIt.hasNext())
    {
        // TODO
        pIt.peekNext()->setRoleAndCharacter(rIt.peekNext(), cIt.peekNext());
        switch(rIt.peekNext()) {
            case ROLE_SHERIFF:
            case ROLE_DEPUTY:
                m_goodGuysCount++;
                break;
            case ROLE_OUTLAW:
                m_outlawsCount++;
                break;
            case ROLE_RENEGADE:
                m_renegadesCount++;
                break;
            default:
                NOT_REACHED();
        }
        i++;
        pIt.next(); rIt.next(); cIt.next();
    }
}

QList<PlayerRole> Game::getRoleList()
{
    static char* roleSets[] = {"", "S", "SB", "SRB", "SBBR", "SVBBR", "SVVBBR", "SVVBBBR"};
    QList<PlayerRole> res;
    char* i = roleSets[m_playerList.count()];
    while(*i != '\0')
    {
        switch(*i)
        {
            case 'S': res.append(ROLE_SHERIFF); break;
            case 'B': res.append(ROLE_OUTLAW); break;
            case 'V': res.append(ROLE_DEPUTY); break;
            case 'R': res.append(ROLE_RENEGADE); break;
        }
        ++i;
    }
    return res;
}
