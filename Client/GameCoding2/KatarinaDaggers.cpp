#include "pch.h"
#include "KatarinaDaggers.h"
#include "LolVfx.h"
#include "BasePlayerController.h"
#include "ChampionKits.h"

namespace
{
struct Dagger
{
    Vec3 position;
    float landsAt = 0.f, expiresAt = 0.f;
    uint64 groundVfx = 0, areaVfx = 0;
    bool landed = false;

    Vec3 from;
    float thrownAt = 0.f, arcHeight = 0.f;
    shared_ptr<GameObject> flight;
    uint64 flightVfx = 0;
};

constexpr float TossHeight = 3.5f, ArcHeight = 2.f, BladeHeight = 0.3f;

Vec3 ArcPoint(const Dagger& dagger, float t)
{
    Vec3 point = dagger.from + (dagger.position - dagger.from) * t;
    point.y += BladeHeight + 4.f * dagger.arcHeight * t * (1.f - t);

    return point;
}

int32 SkinOf(uint64 ownerId)
{
    const auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == ownerId)
        return me->_playerInfo->skinid();
    const auto owner = CUR_SCENE->FindObjectById(ownerId);
    const auto controller = owner ? owner->GetScript<BasePlayerController>() : nullptr;

    return controller && controller->_playerInfo ? controller->_playerInfo->skinid() : 0;
}

string AreaVfx(uint64 ownerId)
{
    const auto me = GAMEMANAGER->_myPlayer;
    const auto owner = CUR_SCENE->FindObjectById(ownerId);
    const auto controller = owner ? owner->GetScript<BasePlayerController>() : nullptr;
    const bool enemy = me && me->_playerInfo && controller && controller->_playerInfo &&
                       controller->_playerInfo->teamid() != me->_playerInfo->teamid();

    return SkinVfx(enemy ? "Katarina_Base_W_Indicator_Enemy" : "Katarina_Base_W_Indicator_Ally", SkinOf(ownerId));
}

void Land(Dagger& dagger)
{
    if (dagger.flight)
        LolVfx::Get()->Stop(dagger.flightVfx, true);
    dagger.flight.reset();
    dagger.landed = true;
    SOUND->PlayCueAt("Play_sfx_Katarina_Q_dagger_land_dirt", dagger.position);
}

unordered_map<uint64, Dagger> g_daggers;

Vec3 ToVec3(const Protocol::Vector3& v)
{
    return Vec3(v.x(), v.y(), v.z());
}
}

namespace KatarinaDaggers
{
void OnPacket(const Protocol::S_KatarinaDagger& packet)
{
    auto vfx = LolVfx::Get();
    const Vec3 at = ToVec3(packet.position());
    const Vec3 forward(0.f, 0.f, 1.f);
    const float now = TIME->GetGameTime();
    const int32 skin = SkinOf(packet.ownerid());

    if (packet.state() == 0)
    {
        const Vec3 from = ToVec3(packet.origin());
        const bool toss = Vec3::Distance(from, at) < 0.5f;
        LolVfx::PlayOptions ground;
        ground.delay = packet.landin();
        ground.duration = packet.lifetime();
        Dagger& dagger = g_daggers[packet.daggerid()];

        if (dagger.groundVfx)
            vfx->Stop(dagger.groundVfx, true);

        if (dagger.flight)
            vfx->Stop(dagger.flightVfx, true);
        vfx->Stop(dagger.areaVfx, true);
        dagger = {at, now + packet.landin(), now + packet.landin() + packet.lifetime(),
                  vfx->Play(SkinVfx("Katarina_Base_Dagger_Ground_Indicator", skin), at, forward, ground), false};
        LolVfx::PlayOptions area;
        area.duration = packet.landin() + packet.lifetime();
        dagger.areaVfx = vfx->Play(AreaVfx(packet.ownerid()), at, forward, area);
        dagger.from = from;
        dagger.thrownAt = now;
        dagger.arcHeight = toss ? TossHeight : ArcHeight;

        if (packet.landin() > 0.05f)
        {
            dagger.flight = make_shared<GameObject>("KatarinaDaggerFlight");
            dagger.flight->GetOrAddTransform()->SetPosition(ArcPoint(dagger, 0.f));
            LolVfx::PlayOptions flight;
            flight.follow = dagger.flight;
            flight.duration = packet.landin();
            flight.orientToMotion = !toss;
            Vec3 direction = at - from;
            direction.y = 0.f;
            dagger.flightVfx =
                vfx->Play(SkinVfx("Katarina_base_Q_mis", skin), ArcPoint(dagger, 0.f),
                          direction.LengthSquared() > 1e-4f ? direction / direction.Length() : forward, flight);

            if (!toss)
                SOUND->PlayCueAt("Play_sfx_Katarina_KatarinaQdaggerarc_OnMissileLaunch", from);
        }

        return;
    }

    if (auto it = g_daggers.find(packet.daggerid()); it != g_daggers.end())
    {
        if (it->second.flight)
            vfx->Stop(it->second.flightVfx, true);
        vfx->Stop(it->second.groundVfx, true);
        vfx->Stop(it->second.areaVfx, true);
        g_daggers.erase(it);
    }

    if (packet.state() == 1)
    {
        vfx->Play(SkinVfx("Katarina_Base_Dagger_PickUp_Cas", skin), at, forward);
        vfx->Play(SkinVfx("Katarina_Base_Dagger_PickUp_Tar", skin), at, forward);
        SOUND->PlayCueAt("Play_sfx_Katarina_Dagger_PickUpPBAoE_Cast", at);
    }
    else
    {
        vfx->Play(SkinVfx("Katarina_Base_Dagger_Ground_Indicator_Death", skin), at, forward);
        SOUND->PlayCueAt("Play_sfx_Katarina_Dagger_GroundIndicator_death", at);
    }
}

void Update()
{
    const float now = TIME->GetGameTime();

    for (auto it = g_daggers.begin(); it != g_daggers.end();)
    {
        Dagger& dagger = it->second;

        if (!dagger.landed && now >= dagger.landsAt)
            Land(dagger);
        else if (dagger.flight)
        {
            const float span = max(0.01f, dagger.landsAt - dagger.thrownAt);
            dagger.flight->GetTransform()->SetPosition(
                ArcPoint(dagger, std::clamp((now - dagger.thrownAt) / span, 0.f, 1.f)));
        }

        if (now > dagger.expiresAt + 1.f)
        {
            if (dagger.flight)
                LolVfx::Get()->Stop(dagger.flightVfx, true);
            LolVfx::Get()->Stop(dagger.groundVfx, true);
            LolVfx::Get()->Stop(dagger.areaVfx, true);
            it = g_daggers.erase(it);
        }
        else
            ++it;
    }
}

size_t Count()
{
    return g_daggers.size();
}

bool Nearest(const Vec3& from, Vec3& position)
{
    float best = FLT_MAX;

    for (const auto& [id, dagger] : g_daggers)
        if (dagger.landed && Vec3::Distance(from, dagger.position) < best)
        {
            best = Vec3::Distance(from, dagger.position);
            position = dagger.position;
        }
    return best < FLT_MAX;
}
}
