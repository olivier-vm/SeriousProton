#include "multiplayer_client.h"
#include "multiplayer.h"
#include "multiplayer_internal.h"
#include "engine.h"

P<GameClient> gameClient;

GameClient::GameClient(sf::IpAddress server, int portNr)
{
    assert(!gameServer);
    assert(!gameClient);
    
    clientId = -1;
    gameClient = this;

    if (socket.connect(server, portNr) != sf::TcpSocket::Done)
        connected = false;
    else
        connected = true;
    socket.setBlocking(false);
}

P<MultiplayerObject> GameClient::getObjectById(int32_t id)
{
    if (objectMap.find(id) != objectMap.end())
        return objectMap[id];
    return NULL;
}

void GameClient::update(float delta)
{
    std::vector<int32_t> delList;
    for(std::map<int32_t, P<MultiplayerObject> >::iterator i=objectMap.begin(); i != objectMap.end(); i++)
    {
        int id = i->first;
        P<MultiplayerObject> obj = i->second;
        if (!obj)
            delList.push_back(id);
    }
    for(unsigned int n=0; n<delList.size(); n++)
        objectMap.erase(delList[n]);

    sf::Packet packet;
    sf::TcpSocket::Status status;
    while((status = socket.receive(packet)) == sf::TcpSocket::Done)
    {
        command_t command;
        packet >> command;
        switch(command)
        {
        case CMD_CREATE:
            {
                int32_t id;
                string name;
                packet >> id >> name;
                for(MultiplayerClassListItem* i = multiplayerClassListStart; i; i = i->next)
                {
                    if (i->name == name)
                    {
                        printf("Created %s from server replication\n", name.c_str());
                        MultiplayerObject* obj = i->func();
                        obj->multiplayerObjectId = id;
                        objectMap[id] = obj;
                        
                        int16_t idx;
                        while(packet >> idx)
                        {
                            if (idx < int16_t(obj->memberReplicationInfo.size()))
                                (obj->memberReplicationInfo[idx].receiveFunction)(obj->memberReplicationInfo[idx].ptr, packet);
                        }
                    }
                }
            }
            break;
        case CMD_DELETE:
            {
                int32_t id;
                packet >> id;
                if (objectMap.find(id) != objectMap.end() && objectMap[id])
                    objectMap[id]->destroy();
            }
            break;
        case CMD_UPDATE_VALUE:
            {
                int32_t id;
                int16_t idx;
                packet >> id;
                if (objectMap.find(id) != objectMap.end() && objectMap[id])
                {
                    P<MultiplayerObject> obj = objectMap[id];
                    while(packet >> idx)
                    {
                        if (idx < int32_t(obj->memberReplicationInfo.size()))
                            (obj->memberReplicationInfo[idx].receiveFunction)(obj->memberReplicationInfo[idx].ptr, packet);
                    }
                }
            }
            break;
        case CMD_SET_CLIENT_ID:
            packet >> clientId;
            break;
        case CMD_SET_GAME_SPEED:
            {
                float gamespeed;
                packet >> gamespeed;
                engine->setGameSpeed(gamespeed);
            }
            break;
        default:
            printf("Unknown command from server: %d\n", command);
        }
    }
    if (status == sf::TcpSocket::Disconnected)
    {
        connected = false;
    }
}