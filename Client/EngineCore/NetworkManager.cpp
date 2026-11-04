// 서버 연결과 패킷 전달.
#include "pch.h"
#include "NetworkManager.h"
#include "ServerCore/Service.h"
#include "ServerCore/ThreadManager.h"
#include "../GameCoding2/ServerSession.h"

void NetworkManager::Init()
{
    SocketUtils::Init();

    _service = make_shared<ClientService>(
        NetAddress(_host, _port), make_shared<IocpCore>(), [=]() { return CreateSession(); }, 1);

    const bool started = _service->Start();
    assert(started);
    (void)started;
}

void NetworkManager::Update()
{
    constexpr int32 MaxCompletionsPerFrame = 64;

    for (int32 i = 0; i < MaxCompletionsPerFrame; ++i)
    {
        if (!_service->GetIocpCore()->Dispatch(0))
            break;
    }
}

ServerSessionRef NetworkManager::CreateSession()
{
    return _session = make_shared<ServerSession>();
}

void NetworkManager::SendPacket(SendBufferRef sendBuffer)
{
    if (_session)
    {
        _session->Send(sendBuffer);
    }
}

void NetworkManager::SetNavigationInfo(int32 roomId, uint32 formatVersion, const string& mapId,
                                       const string& contentHash)
{
    _navigationRoomId = roomId;
    _navigationFormatVersion = formatVersion;
    _navigationMapId = mapId;
    _navigationContentHash = contentHash;
    _clientMoveSequence = 0;
    _lastMoveResponseSequence = 0;
    _hasNavigationInfo = !_navigationMapId.empty() && !_navigationContentHash.empty();
}

void NetworkManager::ClearNavigationInfo()
{
    _hasNavigationInfo = false;
    _navigationRoomId = -1;
    _navigationFormatVersion = 0;
    _clientMoveSequence = 0;
    _lastMoveResponseSequence = 0;
    _navigationMapId.clear();
    _navigationContentHash.clear();
}

bool NetworkManager::TryGetNextMoveSequence(uint32& sequence)
{
    if (!_hasNavigationInfo || _clientMoveSequence == UINT32_MAX)
        return false;

    sequence = ++_clientMoveSequence;

    return true;
}

bool NetworkManager::TryAcceptMoveResponse(uint32 sequence, const string& mapId, const string& contentHash)
{
    if (!_hasNavigationInfo || sequence == 0 || sequence > _clientMoveSequence || sequence <= _lastMoveResponseSequence)
        return false;

    if (mapId != _navigationMapId || contentHash != _navigationContentHash)
        return false;

    _lastMoveResponseSequence = sequence;

    return true;
}
