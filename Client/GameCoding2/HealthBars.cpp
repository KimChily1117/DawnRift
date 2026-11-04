#include "pch.h"
#include "ModelAnimator.h"
#include "HealthBars.h"
#include "GameFlow.h"
#include "BasePlayerController.h"
#include "ChampionCombatStats.h"
#include "UnitStatus.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include "HudText.h"
#include "FogOfWar.h"
#include "MapUnits.h"
#include "GroundHeight.h"

namespace
{
constexpr float ChampionWidth = 104.f, ChampionHeight = 12.f;
constexpr float SummonWidth = 76.f, SummonHeight = 8.f;
constexpr float MinionWidth = 62.f, MinionHeight = 6.f;
constexpr float MonsterWidth = 92.f, MonsterHeight = 8.f;
constexpr float StructureWidth = 150.f, StructureHeight = 10.f;
constexpr float Border = 2.f;
constexpr float CrowdControlHeight = 4.f;
constexpr float LevelBoxWidth = 18.f;
constexpr float HealthPerTick = 100.f;
constexpr float AboveHead = 0.75f;

constexpr float ReferenceHeight = 1080.f;
constexpr float MinUiScale = 768.f / 1080.f;
constexpr size_t MaxQuads = 2048;
constexpr int32 OverlayOrder = 90;

const Vec4 Frame(0.02f, 0.02f, 0.03f, 0.9f);
const Vec4 Missing(0.12f, 0.1f, 0.1f, 0.85f);
const Vec4 Mine(0.33f, 0.78f, 0.22f, 1.f);
const Vec4 Ally(0.24f, 0.55f, 0.95f, 1.f);
const Vec4 Enemy(0.86f, 0.2f, 0.18f, 1.f);
const Vec4 Shield(0.92f, 0.92f, 0.92f, 1.f);
const Vec4 Tick(0.f, 0.f, 0.f, 0.55f);
const Vec4 CrowdControl(0.95f, 0.95f, 0.95f, 1.f);
const Vec4 LevelBox(0.05f, 0.06f, 0.08f, 0.95f);
}

shared_ptr<HealthBars> HealthBars::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<HealthBars> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("HealthBars");
    object->GetOrAddTransform();
    auto bars = object->GetOrAddScript<HealthBars>();
    bars->_scene = CUR_SCENE;
    bars->Initialize();
    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = bars;

    return bars;
}

void HealthBars::Initialize()
{
    _vertices.resize(MaxQuads * 4);
    vector<uint32> indices;

    for (uint32 index = 0; index < _vertices.size(); index += 4)
        indices.insert(indices.end(), {index, index + 1, index + 2, index + 2, index + 1, index + 3});
    _mesh = make_shared<Mesh>();
    _mesh->CreateDynamic(_vertices, indices);
    auto material = make_shared<Material>();
    material->SetShader(make_shared<Shader>(L"HealthBar.fx"));
    _object = make_shared<GameObject>("HealthBarsBatch");
    _object->GetOrAddTransform();
    _object->AddComponent(make_shared<MeshRenderer>());
    _object->GetMeshRenderer()->SetMesh(_mesh);
    _object->GetMeshRenderer()->SetMaterial(material);
    _object->GetMeshRenderer()->SetPass(0);
    _object->GetMeshRenderer()->SetWorldOverlay(OverlayOrder);
}

void HealthBars::Quad(float x0, float y0, float x1, float y1, const Vec4& color)
{
    if (_used + 4 > _vertices.size() || x1 <= x0 || y1 <= y0)
        return;
    const auto& desc = GAME->GetGameDesc();
    auto ndc = [&](float x, float y)
    {
        return Vec3(x / desc.width * 2.f - 1.f, 1.f - y / desc.height * 2.f, 0.f);
    };
    const Vec3 corners[4] = {ndc(x0, y0), ndc(x1, y0), ndc(x0, y1), ndc(x1, y1)};

    for (const Vec3& corner : corners)
    {
        auto& vertex = _vertices[_used++];
        vertex.position = corner;
        vertex.normal = Vec3(color.x, color.y, color.z);
        vertex.tangent = Vec3(color.w, 0.f, 0.f);
    }
}

void HealthBars::LateUpdate()
{
    _used = 0;
    _bars.clear();
    auto cameraObject = CUR_SCENE ? CUR_SCENE->GetMainCamera() : nullptr;

    if (!cameraObject || !cameraObject->GetCamera())
        return;
    const Matrix viewProjection =
        cameraObject->GetCamera()->GetViewMatrix() * cameraObject->GetCamera()->GetProjectionMatrix();
    const auto& desc = GAME->GetGameDesc();
    const float scale = max(MinUiScale, desc.height / ReferenceHeight);

    auto me = GAMEMANAGER->_myPlayer;
    const uint64 myId = me && me->_playerInfo ? me->_playerInfo->objectid() : 0;
    const int32 myTeam = me && me->_playerInfo ? me->_playerInfo->teamid() : 0;
    vector<shared_ptr<BasePlayerController>> units;

    if (me && me->_playerInfo)
        units.push_back(me);

    for (const auto& [id, object] : CUR_SCENE->_players)
        if (id != myId && object)
            if (auto controller = object->GetScript<BasePlayerController>(); controller && controller->_playerInfo)
                units.push_back(controller);

    const float now = TIME->GetGameTime();

    if (me && me->_playerInfo)
        if (const UnitStatus::State* mine = UnitStatus::Find(me->_playerInfo->objectid());
            mine && mine->respawnAt > now)
            Quad(0.f, 0.f, static_cast<float>(desc.width), static_cast<float>(desc.height),
                 Vec4(0.16f, 0.17f, 0.2f, 0.42f));

    if (GameFlow::Room().paused)
        Quad(0.f, 0.f, static_cast<float>(desc.width), static_cast<float>(desc.height),
             Vec4(0.36f, 0.36f, 0.38f, 0.66f));

    for (const auto& unit : units)
    {
        const auto& info = *unit->_playerInfo;

        if (info.hp() <= 0 || info.maxhp() <= 0 || FogOfWar::Get()->IsHidden(info.objectid()))
            continue;
        const MapUnitInfo* mapUnit = FindMapUnit(info.champtype());
        const bool summon = info.champtype() == Protocol::PLAYER_TYPE_TIBBERS;
        const bool champion = !summon && !mapUnit;
        float barWidth = summon ? SummonWidth : ChampionWidth, barHeight = summon ? SummonHeight : ChampionHeight;

        if (mapUnit)
        {
            barWidth = mapUnit->bar == MapUnitBar::Minion    ? MinionWidth
                       : mapUnit->bar == MapUnitBar::Monster ? MonsterWidth
                                                             : StructureWidth;
            barHeight = mapUnit->bar == MapUnitBar::Minion    ? MinionHeight
                        : mapUnit->bar == MapUnitBar::Monster ? MonsterHeight
                                                              : StructureHeight;
        }

        Vec3 feet = unit->GetTransform()->GetPosition();

        if (GroundHeight::IsActive())
            feet.y = GroundHeight::At(feet.x, feet.z);
        float headHeight = mapUnit && mapUnit->barHeight > 0.f
                               ? mapUnit->barHeight
                               : GetChampionCombatStats(info.champtype()).centerHeight * 2.f + AboveHead;

        if (champion)
            if (auto animator = unit->GetGameObject()->GetModelAnimator())
            {
                Vec3 overhead;

                if (animator->GetBoneWorldPosition(L"C_Buffbone_Glb_Overhead_Loc", overhead) ||
                    animator->GetBoneWorldPosition(L"C_BUFFBONE_GLB_OVERHEAD_LOC", overhead))
                    headHeight = max(headHeight, overhead.y - feet.y + 0.35f);
                float& eased = _barHeights[info.objectid()];
                eased = eased <= 0.f ? headHeight
                                     : eased + (headHeight - eased) * std::clamp(TIME->GetDeltaTime() * 4.f, 0.f, 1.f);
                headHeight = eased;
            }
        const Vec3 anchor = feet + Vec3(0.f, headHeight, 0.f);
        const Vec4 clip = Vec4::Transform(Vec4(anchor.x, anchor.y, anchor.z, 1.f), viewProjection);

        if (clip.w <= 0.0001f)
            continue;
        const float cx = (clip.x / clip.w * 0.5f + 0.5f) * desc.width;
        const float cy = (0.5f - clip.y / clip.w * 0.5f) * desc.height;

        const float width = barWidth * scale;
        const float height = barHeight * scale;
        const float border = Border * scale;
        const float x0 = cx - width * 0.5f, y0 = cy - height;
        const UnitStatus::State* status = UnitStatus::Find(info.objectid());
        const float shieldHp = status ? static_cast<float>(status->shield) : 0.f;

        const float total = max(static_cast<float>(info.maxhp()), info.hp() + shieldHp);
        const float healthFraction = info.hp() / total, shieldFraction = shieldHp / total;

        Vec4 color = info.objectid() == myId || (info.ownerid() != 0 && info.ownerid() == myId) ? Mine
                     : info.teamid() == myTeam                                                  ? Ally
                                                                                                : Enemy;

        if (status && status->grievousEndsAt > TIME->GetGameTime())
            color = Vec4(color.x * 0.55f, color.y * 0.55f, color.z * 0.55f, color.w);
        Quad(x0 - border, y0 - border, x0 + width + border, y0 + height + border, Frame);
        Quad(x0, y0, x0 + width, y0 + height, Missing);
        const float healthEnd = x0 + width * healthFraction;
        Quad(x0, y0, healthEnd, y0 + height, color);
        Quad(healthEnd, y0, healthEnd + width * shieldFraction, y0 + height, Shield);

        if (champion)
            for (float hp = HealthPerTick; hp < info.hp(); hp += HealthPerTick)
            {
                const float x = x0 + width * (hp / total);
                Quad(x - 0.5f * scale, y0, x + 0.5f * scale, y0 + height * 0.55f, Tick);
            }

        if (champion)
        {
            const UnitStatus::State* levelStatus = UnitStatus::Find(info.objectid());
            const int32 level = levelStatus ? levelStatus->level : max(1, info.level());
            const float boxWidth = LevelBoxWidth * scale;
            Quad(x0 - border - boxWidth, y0 - border, x0 - border, y0 + height + border, LevelBox);
            HudText::Get()->Draw(std::to_string(level), Vec2(x0 - border - boxWidth * 0.5f, y0 + height * 0.5f),
                                 (height + border * 2.f) * 1.05f);
        }

        Bar bar{info.objectid(), x0, y0, width, height, healthFraction, shieldFraction, false};

        if (status)
        {
            float remaining = 0.f;

            if (now < status->stunEndsAt && status->stunDuration > 0.f)
                remaining = max(remaining, (status->stunEndsAt - now) / status->stunDuration);

            if (now < status->silenceEndsAt && status->silenceDuration > 0.f)
                remaining = max(remaining, (status->silenceEndsAt - now) / status->silenceDuration);

            if (remaining > 0.f)
            {
                const float top = y0 + height + border * 2.f;
                Quad(x0 - border, top - border, x0 + width + border, top + CrowdControlHeight * scale + border, Frame);
                Quad(x0, top, x0 + width * remaining, top + CrowdControlHeight * scale, CrowdControl);
                bar.crowdControlled = true;
            }
        }

        _bars.push_back(bar);
    }

    auto scene = _scene.lock();

    if (!scene)
        return;

    if (_used == 0)
    {
        if (_visible)
            scene->Remove(_object);
        _visible = false;

        return;
    }

    _mesh->UpdateDynamicVertices(_vertices, static_cast<uint32>(_used));

    if (!_visible)
        scene->Add(_object);
    _visible = true;
}
