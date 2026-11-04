#include "pch.h"
#include "GroundHeight.h"
#include <fstream>

namespace
{
const wstring HeightFile = L"..\\Resources\\Navigation\\sr-height.grid";

struct Grid
{
    bool tried = false;
    int32 width = 0, height = 0;
    float originX = 0.f, originZ = 0.f, spacingX = 1.f, spacingZ = 1.f;
    vector<float> heights;
};

Grid g_grid;
weak_ptr<Scene> g_scene;
weak_ptr<GroundHeight> g_instance;
}

bool GroundHeight::Load()
{
    if (g_grid.tried)
        return !g_grid.heights.empty();
    g_grid.tried = true;
    std::ifstream file(HeightFile, std::ios::binary);
    char magic[4] = {};

    if (!file.read(magic, 4) || memcmp(magic, "HGT1", 4) != 0)
    {
        DEBUG_LOG("[GroundHeight] missing Resources/Navigation/sr-height.grid");

        return false;
    }

    uint32 width = 0, height = 0;
    file.read(reinterpret_cast<char*>(&width), 4);
    file.read(reinterpret_cast<char*>(&height), 4);
    file.read(reinterpret_cast<char*>(&g_grid.originX), 4);
    file.read(reinterpret_cast<char*>(&g_grid.originZ), 4);
    file.read(reinterpret_cast<char*>(&g_grid.spacingX), 4);
    file.read(reinterpret_cast<char*>(&g_grid.spacingZ), 4);

    if (!file || width < 2 || height < 2 || width > 8192 || height > 8192 || g_grid.spacingX <= 0.f ||
        g_grid.spacingZ <= 0.f)
        return false;
    g_grid.heights.resize(static_cast<size_t>(width) * height);

    if (!file.read(reinterpret_cast<char*>(g_grid.heights.data()), g_grid.heights.size() * sizeof(float)))
    {
        g_grid.heights.clear();

        return false;
    }

    g_grid.width = static_cast<int32>(width);
    g_grid.height = static_cast<int32>(height);

    return true;
}

float GroundHeight::At(float x, float z)
{
    if (!Load())
        return FlatHeight;
    const float gx = std::clamp((x - g_grid.originX) / g_grid.spacingX, 0.f, static_cast<float>(g_grid.width - 1));
    const float gz = std::clamp((z - g_grid.originZ) / g_grid.spacingZ, 0.f, static_cast<float>(g_grid.height - 1));
    const int32 x0 = min(static_cast<int32>(gx), g_grid.width - 2), z0 = min(static_cast<int32>(gz), g_grid.height - 2);
    const float fx = gx - x0, fz = gz - z0;
    auto h = [&](int32 cx, int32 cz)
    {
        return g_grid.heights[static_cast<size_t>(cz) * g_grid.width + cx];
    };
    const float bottom = h(x0, z0) + (h(x0 + 1, z0) - h(x0, z0)) * fx;
    const float top = h(x0, z0 + 1) + (h(x0 + 1, z0 + 1) - h(x0, z0 + 1)) * fx;

    return bottom + (top - bottom) * fz;
}

bool GroundHeight::IsActive()
{
    return g_scene.lock() == CUR_SCENE && !g_instance.expired();
}

shared_ptr<GroundHeight> GroundHeight::Get()
{
    if (IsActive())
        return g_instance.lock();
    auto object = make_shared<GameObject>("GroundHeight");
    object->GetOrAddTransform();
    auto ground = object->GetOrAddScript<GroundHeight>();
    CUR_SCENE->Add(object);
    g_scene = CUR_SCENE;
    g_instance = ground;

    return ground;
}

void GroundHeight::LateUpdate()
{
    for (const auto& [id, object] : CUR_SCENE->_players)
    {
        if (!object)
            continue;
        auto transform = object->GetTransform();
        Vec3 position = transform->GetPosition();
        const float ground = At(position.x, position.z);

        if (fabsf(position.y - ground) > 0.0001f)
        {
            position.y = ground;
            transform->SetPosition(position);
        }
    }
}
