#include "pch.h"
#include "UnitStatus.h"
#include "FogOfWar.h"
#include "BasePlayerController.h"
#include "PlayerController.h"
#include <fstream>

namespace
{
const wstring GridFile = L"..\\Resources\\Navigation\\sr-vision.grid";
constexpr uint8 BlocksSight = 2;
constexpr float ComputeInterval = 0.1f;
constexpr float FadeRate = 8.f;
constexpr float FadeInterval = 1.f / 30.f;
constexpr int32 StampsPerFrame = 4;
}

shared_ptr<FogOfWar> FogOfWar::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<FogOfWar> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("FogOfWar");
    object->GetOrAddTransform();
    auto fog = object->GetOrAddScript<FogOfWar>();
    fog->_scene = CUR_SCENE;

    if (fog->Load())
    {
        fog->CreateTexture();
        fog->Bind();
    }

    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = fog;

    return fog;
}

bool FogOfWar::Load()
{
    std::ifstream file(GridFile, std::ios::binary);
    char magic[4] = {};

    if (!file.read(magic, 4) || memcmp(magic, "VIS1", 4) != 0)
    {
        DEBUG_LOG("[FogOfWar] missing Resources/Navigation/sr-vision.grid");

        return false;
    }

    uint32 width = 0, height = 0;
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);
    file.read(reinterpret_cast<char*>(&_cellSize), 4);
    file.read(reinterpret_cast<char*>(&_originX), 4);
    file.read(reinterpret_cast<char*>(&_originZ), 4);

    if (!file || width == 0 || height == 0 || width > 4096 || height > 4096)
        return false;
    _cells.resize(static_cast<size_t>(width) * height);

    if (!file.read(reinterpret_cast<char*>(_cells.data()), _cells.size()))
        return false;
    _width = static_cast<int32>(width);
    _height = static_cast<int32>(height);

    _target.assign(_cells.size(), 255);
    _shown.assign(_cells.size(), 1.f);
    _pixels.assign(_cells.size(), 255);

    return true;
}

void FogOfWar::CreateTexture()
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = _width;
    desc.Height = _height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    CHECK(DEVICE->CreateTexture2D(&desc, nullptr, _texture.GetAddressOf()));
    CHECK(DEVICE->CreateShaderResourceView(_texture.Get(), nullptr, _srv.GetAddressOf()));
    Upload();
}

void FogOfWar::Upload()
{
    if (!_texture)
        return;
    D3D11_MAPPED_SUBRESOURCE mapped = {};

    if (FAILED(DC->Map(_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
        return;

    for (int32 z = 0; z < _height; ++z)
        memcpy(static_cast<uint8*>(mapped.pData) + static_cast<size_t>(z) * mapped.RowPitch,
               &_pixels[static_cast<size_t>(z) * _width], _width);
    DC->Unmap(_texture.Get(), 0);
}

void FogOfWar::Bind()
{
    auto scene = _scene.lock();

    if (!scene || !scene->_shader || !_srv)
        return;
    scene->_shader->GetSRV("FogMap")->SetResource(_srv.Get());
    const float params[4] = {_originX, _originZ, 1.f / (_width * _cellSize), 1.f / (_height * _cellSize)};
    scene->_shader->GetVector("FogParams")->SetFloatVector(params);
}

void FogOfWar::ComputeStamp(int32 cx, int32 cz, float sightRange, vector<uint32>& cells) const
{
    cells.clear();

    if (cx < 0 || cz < 0 || cx >= _width || cz >= _height)
        return;
    const int32 radius = static_cast<int32>(std::ceil(sightRange / _cellSize));
    const float radiusSquared = (sightRange / _cellSize) * (sightRange / _cellSize);

    for (int32 tz = max(0, cz - radius); tz <= min(_height - 1, cz + radius); ++tz)
        for (int32 tx = max(0, cx - radius); tx <= min(_width - 1, cx + radius); ++tx)
        {
            const float ddx = static_cast<float>(tx - cx), ddz = static_cast<float>(tz - cz);

            if (ddx * ddx + ddz * ddz <= radiusSquared && ClearLine(cx, cz, tx, tz))
                cells.push_back(static_cast<uint32>(tz) * _width + tx);
        }
}

bool FogOfWar::ClearLine(int32 x, int32 z, int32 tx, int32 tz) const
{
    auto blocks = [&](int32 cellX, int32 cellZ)
    {
        return (_cells[static_cast<size_t>(cellZ) * _width + cellX] & BlocksSight) != 0;
    };

    const float dx = static_cast<float>(tx - x), dz = static_cast<float>(tz - z);
    const int32 stepX = (dx > 0.f) - (dx < 0.f), stepZ = (dz > 0.f) - (dz < 0.f);
    const float tDeltaX = stepX ? 1.f / fabsf(dx) : FLT_MAX, tDeltaZ = stepZ ? 1.f / fabsf(dz) : FLT_MAX;
    float tMaxX = stepX ? 0.5f / fabsf(dx) : FLT_MAX, tMaxZ = stepZ ? 0.5f / fabsf(dz) : FLT_MAX;

    enum
    {
        EyeWalls,
        Between,
        TargetWalls
    } state = blocks(x, z) ? EyeWalls : Between;

    for (int32 guard = 0; guard < _width + _height && (x != tx || z != tz); ++guard)
    {
        if (tMaxX < tMaxZ)
        {
            x += stepX;
            tMaxX += tDeltaX;
        }
        else
        {
            z += stepZ;
            tMaxZ += tDeltaZ;
        }

        const bool wall = blocks(x, z);

        if (state == EyeWalls && !wall)
            state = Between;
        else if (state == Between && wall)
            state = TargetWalls;
        else if (state == TargetWalls && !wall)
            return false;
    }

    return true;
}

void FogOfWar::AddTemporarySight(const Vec3& position, float radius, float seconds)
{
    _temporarySight.push_back({position, radius, TIME->GetGameTime() + seconds});
}

void FogOfWar::ComputeSight()
{
    auto me = GAMEMANAGER->_myPlayer;

    if (!me || !me->_playerInfo || !s_enabled)
    {
        std::fill(_target.begin(), _target.end(), 255);

        return;
    }

    const int32 team = me->_playerInfo->teamid();
    vector<pair<uint64, shared_ptr<BasePlayerController>>> allies = {{me->_playerInfo->objectid(), me}};

    for (const auto& [id, object] : CUR_SCENE->_players)
        if (object && id != me->_playerInfo->objectid())
            if (auto controller = object->GetScript<BasePlayerController>();
                controller && controller->_playerInfo && controller->_playerInfo->teamid() == team &&
                controller->_playerInfo->hp() > 0)
                allies.emplace_back(id, controller);

    unordered_set<uint64> present;
    int32 budget = StampsPerFrame;

    for (const auto& [id, ally] : allies)
    {
        if (ally->_playerInfo->hp() <= 0)
            continue;
        present.insert(id);
        const Vec3 eye = ally->GetTransform()->GetPosition();
        const int32 cx = static_cast<int32>(std::floor((eye.x - _originX) / _cellSize));
        const int32 cz = static_cast<int32>(std::floor((eye.z - _originZ) / _cellSize));
        SightStamp& stamp = _stamps[id];

        const bool sightChanged = stamp.nearsighted != UnitStatus::IsNearsighted(id);

        if ((stamp.cx != cx || stamp.cz != cz || sightChanged) &&
            (id == me->_playerInfo->objectid() || stamp.cells.empty() || budget > 0))
        {
            if (id != me->_playerInfo->objectid())
                --budget;
            stamp.cx = cx;
            stamp.cz = cz;
            const bool ward = ally->_playerInfo->champtype() == Protocol::PLAYER_TYPE_WARD;
            const bool nearsighted = UnitStatus::IsNearsighted(id);
            ComputeStamp(cx, cz, ward ? WardSightRange : nearsighted ? NearsightRange : SightRange, stamp.cells);
            stamp.nearsighted = nearsighted;
        }
    }

    // Temporary sight (Ashe's hawk) stamps under ids no unit has.
    const float now = TIME->GetGameTime();
    std::erase_if(_temporarySight, [now](const TemporarySight& sight) { return sight.until <= now; });

    for (size_t i = 0; i < _temporarySight.size(); ++i)
    {
        const uint64 id = 0xF000000000000000ull + i;
        const TemporarySight& sight = _temporarySight[i];
        const int32 cx = static_cast<int32>(std::floor((sight.position.x - _originX) / _cellSize));
        const int32 cz = static_cast<int32>(std::floor((sight.position.z - _originZ) / _cellSize));
        present.insert(id);
        SightStamp& stamp = _stamps[id];

        if (stamp.cx != cx || stamp.cz != cz || stamp.cells.empty())
        {
            stamp.cx = cx;
            stamp.cz = cz;
            ComputeStamp(cx, cz, sight.radius, stamp.cells);
        }
    }

    for (auto it = _stamps.begin(); it != _stamps.end();)
        it = present.count(it->first) ? std::next(it) : _stamps.erase(it);

    if (_computeTimer > 0.f)
        return;
    _computeTimer = ComputeInterval;
    uint8* target = _target.data();
    memset(target, 0, _target.size());

    for (const auto& [id, stamp] : _stamps)
    {
        const uint32* cells = stamp.cells.data();

        for (size_t i = 0, count = stamp.cells.size(); i < count; ++i)
            target[cells[i]] = 255;
    }
}

void FogOfWar::LateUpdate()
{
    if (_width == 0)
        return;
    const float dt = TIME->GetDeltaTime();
    _computeTimer -= dt;
    ComputeSight();

    _fadeTimer += dt;

    if (_fadeTimer >= FadeInterval)
    {
        const float step = std::clamp(_fadeTimer * FadeRate, 0.f, 1.f);
        _fadeTimer = 0.f;
        const uint8* target = _target.data();
        float* shown = _shown.data();
        uint8* pixels = _pixels.data();
        bool changed = false;

        for (size_t i = 0, count = _shown.size(); i < count; ++i)
        {
            if (pixels[i] == target[i])
                continue;
            shown[i] += (target[i] * (1.f / 255.f) - shown[i]) * step;
            int32 value = static_cast<int32>(std::clamp(shown[i], 0.f, 1.f) * 255.f + 0.5f);

            if (abs(value - target[i]) <= 2)
            {
                value = target[i];
                shown[i] = target[i] * (1.f / 255.f);
            }

            pixels[i] = static_cast<uint8>(value);
            changed = true;
        }

        if (changed)
            Upload();
    }

    Bind();

    for (const auto& [id, scale] : _hiddenScale)
        if (auto object = CUR_SCENE->FindObjectById(id))
            object->GetTransform()->SetScale(Vec3(0.f));
}

void FogOfWar::SetHidden(uint64 objectId, bool hidden, const Vec3& position)
{
    auto object = CUR_SCENE->FindObjectById(objectId);

    if (!object)
    {
        _hiddenScale.erase(objectId);

        return;
    }

    auto it = _hiddenScale.find(objectId);

    if (hidden && s_enabled)
    {
        if (it == _hiddenScale.end())
            _hiddenScale[objectId] = object->GetTransform()->GetScale();
        object->GetTransform()->SetScale(Vec3(0.f));

        return;
    }

    if (it != _hiddenScale.end())
    {
        object->GetTransform()->SetScale(it->second);
        _hiddenScale.erase(it);
    }

    if (auto controller = object->GetScript<BasePlayerController>())
        controller->SnapTo(position);
}

bool FogOfWar::BrushAt(float x, float z) const
{
    const int32 cx = static_cast<int32>(std::floor((x - _originX) / _cellSize));
    const int32 cz = static_cast<int32>(std::floor((z - _originZ) / _cellSize));

    if (cx < 0 || cz < 0 || cx >= _width || cz >= _height)
        return false;

    return (_cells[static_cast<size_t>(cz) * _width + cx] & 1) != 0;
}

float FogOfWar::SightAt(float x, float z) const
{
    const int32 cx = static_cast<int32>(std::floor((x - _originX) / _cellSize));
    const int32 cz = static_cast<int32>(std::floor((z - _originZ) / _cellSize));

    if (cx < 0 || cz < 0 || cx >= _width || cz >= _height)
        return 0.f;

    return _pixels[static_cast<size_t>(cz) * _width + cx] / 255.f;
}
