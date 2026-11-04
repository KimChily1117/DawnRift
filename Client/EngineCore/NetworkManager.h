// 서버 연결과 패킷 전달.
#pragma once

using ServerSessionRef = shared_ptr<class ServerSession>;
using SendBufferRef = shared_ptr<class SendBuffer>;
using ClientServiceRef = shared_ptr<class ClientService>;

class NetworkManager
{
    DECLARE_SINGLE(NetworkManager);

public:
    void Configure(const wstring& host, uint16 port, const string& token)
    {
        _host = host;
        _port = port;
        _joinToken = token;
    }

    const string& GetJoinToken() const
    {
        return _joinToken;
    }

    void Init();
    void Update();
    void SendPacket(SendBufferRef sendBuffer);

    void SetNavigationInfo(int32 roomId, uint32 formatVersion, const string& mapId, const string& contentHash);
    void ClearNavigationInfo();

    bool HasNavigationInfo() const
    {
        return _hasNavigationInfo;
    }

    int32 GetNavigationRoomId() const
    {
        return _navigationRoomId;
    }

    uint32 GetNavigationFormatVersion() const
    {
        return _navigationFormatVersion;
    }

    const string& GetNavigationMapId() const
    {
        return _navigationMapId;
    }

    const string& GetNavigationContentHash() const
    {
        return _navigationContentHash;
    }

    bool TryGetNextMoveSequence(uint32& sequence);
    bool TryAcceptMoveResponse(uint32 sequence, const string& mapId, const string& contentHash);

    bool IsAuthoritativeMoveRequestsEnabled() const
    {
        return _authoritativeMoveRequestsEnabled;
    }

    void SetAuthoritativeMoveRequestsEnabled(bool enabled)
    {
        _authoritativeMoveRequestsEnabled = enabled;
    }

private:
    ServerSessionRef CreateSession();
    wstring _host = L"127.0.0.1";
    uint16 _port = 8080;
    string _joinToken;
    ClientServiceRef _service;
    ServerSessionRef _session;

    bool _authoritativeMoveRequestsEnabled = true;
    bool _hasNavigationInfo = false;
    int32 _navigationRoomId = -1;
    uint32 _navigationFormatVersion = 0;
    uint32 _clientMoveSequence = 0;
    uint32 _lastMoveResponseSequence = 0;
    string _navigationMapId;
    string _navigationContentHash;
};
