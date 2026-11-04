#pragma once
#include "ClientPacketHandler.h"

class ServerSession : public PacketSession
{
public:
    ~ServerSession() {}

    virtual void OnConnected() override
    {
        ClientPacketHandler::SendEnterGame(static_pointer_cast<ServerSession>(shared_from_this()));
    }

    virtual void OnRecvPacket(BYTE* buffer, int32 len) override
    {
        ClientPacketHandler::HandlePacket(static_pointer_cast<ServerSession>(shared_from_this()), buffer, len);
    }

    virtual void OnSend(int32 len) override {}

    virtual void OnDisconnected() override {}
};
