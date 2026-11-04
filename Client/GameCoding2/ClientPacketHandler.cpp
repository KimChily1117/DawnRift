// 서버 패킷 처리.
#include "pch.h"
#include "ClientPacketHandler.h"
#include "ServerCore/BufferReader.h"
#include "Model.h"
#include "Button.h"
#include "Material.h"

#include "HUDController.h"

#include "PlayerController.h"
#include "OtherPlayerController.h"
#include "CameraController.h"

#include "AnniePlayerController.h"
#include "AnnieOtherPlayerController.h"

#include "GarenPlayerController.h"
#include "GarenOtherPlayerController.h"

#include "ModelAnimator.h"
#include "SphereCollider.h"
#include "ProjectileScript.h"
#include "ChampionCombatStats.h"
#include "LolVfx.h"
#include "MapAutoTest.h"
#include "SkillVfx.h"
#include "UnitStatus.h"
#include "HealthBars.h"
#include "ProjectileVisualPolicy.h"
#include "AnnieWEffect.h"
#include "DamageText.h"
#include "OtherProjectileScript.h"
#include "ParticleRenderer.h"
#include "FogOfWar.h"
#include "MapUnits.h"
#include "NpcController.h"
#include "SummonerSpellsClient.h"
#include "UiStrings.h"
#include "GameSounds.h"
#include "Announcements.h"
#include "ChampionKits.h"
#include "KatarinaDaggers.h"
#include "ItemsClient.h"
#include "ItemVfx.h"
#include "Pings.h"
#include "Chat.h"
#include "GameFlow.h"
#include "UiStrings.h"
#include "KitPlayerController.h"

namespace
{
constexpr int32 TibbersAuraVfxKey = 1000;
const std::initializer_list<const wchar_t*> TibbersClips = {L"Idle", L"Run", L"Atk1", L"Atk2"};

const std::initializer_list<const wchar_t*> ChampionClips = {
    L"Idle", L"Run", L"Atk1", L"Atk2", L"Qspell", L"Wspell", L"Espell", L"Rspell", L"Idle", L"Death", L"Recall"};

const std::initializer_list<const wchar_t*> MapUnitClips = {L"Idle", L"Run",  L"Attack", L"Attack", L"Idle",
                                                            L"Idle", L"Idle", L"Idle",   L"Idle",   L"Death"};

shared_ptr<Model> LoadUnitModel(const wstring& folder, std::initializer_list<const wchar_t*> clips);

wstring SkinFolder(const wstring& champion, int32 skinId)
{
    const wstring folder = champion + L"Skin" + std::to_wstring(max(0, skinId));

    return std::filesystem::exists(L"../Resources/Models/" + folder + L"/" + folder + L".mesh") ? folder
                                                                                                : champion + L"Skin0";
}

void ApplySkinTransform(const shared_ptr<GameObject>& obj, const wstring&, int32)
{
    obj->GetTransform()->SetScale(Vec3(0.01f));
    obj->GetModelAnimator()->SetPass(11);
}

// The local or a remote unit's controller.
shared_ptr<BasePlayerController> ControllerOf(uint64 objectId)
{
    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == objectId)
        return me;

    if (auto object = CUR_SCENE->FindObjectById(objectId))
        return object->GetScript<BasePlayerController>();

    return nullptr;
}

template <typename T> bool Parse(BYTE* buffer, T& pkt)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);

    return pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader));
}

// S_MyPlayer / S_AddObject carry level and mana too.
void ReadUnitInfo(const Protocol::ObjectInfo& info)
{
    UnitStatus::State& state = UnitStatus::Of(info.objectid());
    state.level = max(1, info.level());
    state.mp = info.mp();
    state.maxMp = info.maxmp();
}

void PreloadUnit(Protocol::PLAYER_CHAMPION_TYPE champion, int32 skinId = 0)
{
    PreloadChampionVfx(champion, skinId);

    if (champion == Protocol::PLAYER_TYPE_ANNIE)
        LoadUnitModel(L"AnnieTibbers", TibbersClips);
}

const char* const StructureExplosions[] = {"SRU_Order_Turret_Explode",      "SRU_Chaos_Turret_Explode",
                                           "SRU_Order_Inhibitor_Explosion", "SRUAP_Chaos_Inhibitor_Explosion",
                                           "Nexus_Base_Explosion",          "Nexus_Skin03_Explosion"};

void PreloadMapUnits()
{
    for (int32 type : {Protocol::PLAYER_TYPE_MINION_MELEE,      Protocol::PLAYER_TYPE_MINION_RANGED,
                       Protocol::PLAYER_TYPE_MINION_SIEGE,      Protocol::PLAYER_TYPE_TURRET,
                       Protocol::PLAYER_TYPE_INHIBITOR,         Protocol::PLAYER_TYPE_NEXUS,
                       Protocol::PLAYER_TYPE_SRU_BLUE,          Protocol::PLAYER_TYPE_SRU_RED,
                       Protocol::PLAYER_TYPE_SRU_GROMP,         Protocol::PLAYER_TYPE_SRU_MURKWOLF,
                       Protocol::PLAYER_TYPE_SRU_RAZORBEAK,     Protocol::PLAYER_TYPE_SRU_KRUG,
                       Protocol::PLAYER_TYPE_SRU_DRAGON_FIRE,   Protocol::PLAYER_TYPE_SRU_DRAGON_EARTH,
                       Protocol::PLAYER_TYPE_SRU_DRAGON_WATER,  Protocol::PLAYER_TYPE_SRU_DRAGON_AIR,
                       Protocol::PLAYER_TYPE_SRU_BARON,         Protocol::PLAYER_TYPE_WARD,
                       Protocol::PLAYER_TYPE_SRU_DRAGON_ELDER,  Protocol::PLAYER_TYPE_SRU_RIFTHERALD,
                       Protocol::PLAYER_TYPE_MINION_SUPER,      Protocol::PLAYER_TYPE_SRU_KRUG_MINI,
                       Protocol::PLAYER_TYPE_SRU_KRUG_MINIMINI, Protocol::PLAYER_TYPE_SRU_MURKWOLF_MINI,
                       Protocol::PLAYER_TYPE_SRU_RAZORBEAK_MINI})
    {
        const MapUnitInfo* unit = FindMapUnit(static_cast<Protocol::PLAYER_CHAMPION_TYPE>(type));

        for (const wchar_t* folder : {unit->orderModel, unit->chaosModel})
            if (std::filesystem::exists(L"../Resources/Models/" + wstring(folder) + L"/" + folder + L".mesh"))
                LoadUnitModel(folder, MapUnitClips);

        for (const char* vfx : {unit->orderMissile, unit->chaosMissile, unit->orderHit, unit->chaosHit})
            if (vfx)
                LolVfx::Get()->Preload(vfx);
    }

    for (const char* vfx : StructureExplosions)
        LolVfx::Get()->Preload(vfx);

    for (const wchar_t* folder :
         {L"InhibitorOrderDestroyed", L"InhibitorChaosDestroyed", L"TurretOrderRubble", L"TurretChaosRubble"})
        if (std::filesystem::exists(L"../Resources/Models/" + wstring(folder) + L"/" + folder + L".mesh"))
            LoadUnitModel(folder, MapUnitClips);
}

const char* StructureExplosion(int32 type, bool chaos)
{
    switch (type)
    {
    case Protocol::PLAYER_TYPE_TURRET:
        return chaos ? "SRU_Chaos_Turret_Explode" : "SRU_Order_Turret_Explode";
    case Protocol::PLAYER_TYPE_INHIBITOR:
        return chaos ? "SRUAP_Chaos_Inhibitor_Explosion" : "SRU_Order_Inhibitor_Explosion";
    case Protocol::PLAYER_TYPE_NEXUS:
        return chaos ? "Nexus_Skin03_Explosion" : "Nexus_Base_Explosion";
    default:
        return nullptr;
    }
}

void LeaveRubble(const shared_ptr<GameObject>& turret)
{
    auto npc = turret ? turret->GetScript<NpcController>() : nullptr;

    if (!npc || !npc->_playerInfo || npc->_playerInfo->champtype() != Protocol::PLAYER_TYPE_TURRET)
        return;
    const wstring folder = MapUnitModel(*npc->_playerInfo) + L"Rubble";

    if (!std::filesystem::exists(L"../Resources/Models/" + folder + L"/" + folder + L".mesh"))
        return;
    auto rubble = make_shared<GameObject>();
    auto transform = rubble->GetOrAddTransform();
    transform->SetPosition(turret->GetTransform()->GetPosition());
    transform->SetRotation(turret->GetTransform()->GetLocalRotation());
    transform->SetScale(turret->GetTransform()->GetLocalScale());
    rubble->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));
    rubble->GetModelAnimator()->SetModel(LoadUnitModel(folder, MapUnitClips));
    rubble->GetModelAnimator()->SetPass(11);
    rubble->GetModelAnimator()->SetNeverCulled(true);
    CUR_SCENE->Add(rubble);
}

void SwapInhibitorModel(BasePlayerController& controller, bool destroyed)
{
    if (!controller._playerInfo || controller._playerInfo->champtype() != Protocol::PLAYER_TYPE_INHIBITOR)
        return;
    auto animator = controller.GetGameObject()->GetModelAnimator();

    if (!animator)
        return;
    const wstring folder = MapUnitModel(*controller._playerInfo) + (destroyed ? L"Destroyed" : L"");

    if (!std::filesystem::exists(L"../Resources/Models/" + folder + L"/" + folder + L".mesh"))
        return;
    animator->SetModel(LoadUnitModel(folder, MapUnitClips));
}

void SetupMapUnit(const shared_ptr<GameObject>& obj, const Protocol::ObjectInfo& info)
{
    const MapUnitInfo* unit = FindMapUnit(info.champtype());
    auto controller = make_shared<NpcController>();
    controller->_playerInfo = make_shared<Protocol::ObjectInfo>(info);
    obj->AddComponent(controller);
    obj->GetOrAddTransform()->SetScale(Vec3(0.01f));
    auto collider = make_shared<SphereCollider>();
    collider->SetRadius(max(100.f, unit->radius * 150.f));
    obj->AddComponent(collider);
    obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));
    obj->GetModelAnimator()->SetModel(LoadUnitModel(MapUnitModel(info), MapUnitClips));
    obj->GetModelAnimator()->SetPass(11);
    obj->GetModelAnimator()->SetNeverCulled(unit->bar == MapUnitBar::Structure);
}

shared_ptr<Model> LoadUnitModel(const wstring& folder, std::initializer_list<const wchar_t*> clips)
{
    static unordered_map<wstring, shared_ptr<Model>> cache;
    wstring key = folder;

    for (const wchar_t* clip : clips)
        key += L"|" + wstring(clip);

    if (auto it = cache.find(key); it != cache.end())
        return it->second;
    PROFILE_SCOPE("  load unit model");
    auto model = make_shared<Model>();
    model->ReadModel(folder + L"/" + folder);
    model->ReadMaterial(folder + L"/" + folder);

    for (const wchar_t* clip : clips)
    {
        if (wstring(clip) == L"Recall" && !std::filesystem::exists(L"../Resources/Models/" + folder + L"/Recall.clip"))
            break;
        model->ReadAnimation(folder + L"/" + clip);
    }

    cache[key] = model;

    return model;
}

constexpr size_t MaxProjectileVisualPoolSize = 64;
vector<shared_ptr<GameObject>> g_projectileVisualPool;
weak_ptr<Scene> g_projectilePoolScene;

void EnsureProjectilePoolScene()
{
    auto pooledScene = g_projectilePoolScene.lock();

    if (pooledScene.get() == CUR_SCENE.get())
        return;
    g_projectileVisualPool.clear();
    g_projectilePoolScene = CUR_SCENE;
}

shared_ptr<GameObject> AcquireProjectileVisual()
{
    EnsureProjectilePoolScene();

    if (!g_projectileVisualPool.empty())
    {
        auto object = g_projectileVisualPool.back();
        g_projectileVisualPool.pop_back();

        return object;
    }

    auto object = make_shared<GameObject>("PooledProjectile");
    object->GetOrAddTransform();
    object->GetOrAddScript<ProjectileScript>();

    return object;
}

void ReleaseProjectileVisual(const shared_ptr<GameObject>& object)
{
    if (!object)
        return;

    if (auto script = object->GetScript<ProjectileScript>())
        script->ResetForPool();
    CUR_SCENE->Remove(object);
    EnsureProjectilePoolScene();

    if (g_projectileVisualPool.size() < MaxProjectileVisualPoolSize)
        g_projectileVisualPool.push_back(object);
}
}

std::unordered_map<uint64, int32> ClientPacketHandler::g_lastPlayedSkill;

void ClientPacketHandler::HandlePacket(ServerSessionRef session, BYTE* buffer, int32 len)
{
    BufferReader br(buffer, len);

    PacketHeader header;
    br >> header;

    Proto type = (Proto)header.id;

    switch (type)
    {
    case S_TEST_MSG:
        Handle_S_TEST(buffer, len);
        break;
    case S_OBJECT_UPDATE:
        break;
    case S_SKILL_RESULT:
        Handle_S_SkillResult(buffer, len);
        break;
    case S_UPDATE_MAP:
        break;
    case S_CHAT_MESSAGE:
        Handle_S_ChatMessage(buffer, len);
        break;
    case S_ENTER_GAME:
        Handle_S_EnterGame(buffer, len);
        break;
    case S_MY_PLAYER:
        Handle_S_MyPlayer(buffer, len);
        break;
    case S_ADD_OBJECT:
        Handle_S_AddObject(buffer, len);
        break;
    case S_REMOVE_OBJECT:
        Handle_S_RemoveObject(buffer, len);
        break;
    case S_MOVE:
        Handle_S_Move(buffer, len);
        break;
    case S_NAVIGATION_INFO:
        Handle_S_NavigationInfo(buffer, len);
        break;
    case S_MOVE_ACCEPTED:
        Handle_S_MoveAccepted(buffer, len);
        break;
    case S_MOVE_REJECTED:
        Handle_S_MoveRejected(buffer, len);
        break;
    case S_MOVEMENT_SNAPSHOT:
        Handle_S_MovementSnapshot(buffer, len);
        break;
    case S_PROJECTILE_SPAWN:
        Handle_S_ProjectileSpawn(buffer, len);
        break;
    case S_PROJECTILE_HIT:
        Handle_S_ProjectileHit(buffer, len);
        break;
    case S_DAMAGE:
        Handle_S_Damage(buffer, len);
        break;
    case S_BUFF:
        Handle_S_Buff(buffer, len);
        break;
    case S_HP_UPDATE:
        Handle_S_HpUpdate(buffer, len);
        break;
    case S_DEAD:
        Handle_S_Dead(buffer, len);
        break;
    case S_UNIT_STATS:
        Handle_S_UnitStats(buffer, len);
        break;
    case S_RESPAWN:
        Handle_S_Respawn(buffer, len);
        break;
    case S_SKILL_COOLDOWN:
        Handle_S_SkillCooldown(buffer, len);
        break;
    case S_SKILL_REJECTED:
        Handle_S_SkillRejected(buffer, len);
        break;
    case S_VISION:
        Handle_S_Vision(buffer, len);
        break;
    case S_GAME_OVER:
        Handle_S_GameOver(buffer, len);
        break;
    case S_SUMMONER_SPELL:
        Handle_S_SummonerSpell(buffer, len);
        break;
    case S_SUMMONER_COOLDOWN:
        Handle_S_SummonerCooldown(buffer, len);
        break;
    case S_SCOREBOARD:
        Handle_S_Scoreboard(buffer, len);
        break;
    case S_TRINKET:
        Handle_S_Trinket(buffer, len);
        break;
    case S_ANNOUNCEMENT:
        Handle_S_Announcement(buffer, len);
        break;
    case S_KATARINA_DAGGER:
        Handle_S_KatarinaDagger(buffer, len);
        break;
    case S_INVENTORY:
        Handle_S_Inventory(buffer, len);
        break;
    case S_ITEM_EFFECT:
        Handle_S_ItemEffect(buffer, len);
        break;
    case S_PING:
        Handle_S_Ping(buffer, len);
        break;
    case S_RECALL:
        Handle_S_Recall(buffer, len);
        break;
    case S_SURRENDER:
        Handle_S_Surrender(buffer, len);
        break;
    case S_DEATH_RECAP:
        Handle_S_DeathRecap(buffer, len);
        break;
    case S_GAME_PAUSE:
        Handle_S_GamePause(buffer, len);
        break;
    case S_ROOM_INFO:
        Handle_S_RoomInfo(buffer, len);
        break;
    case S_ROOM_CLOSED:
        Handle_S_RoomClosed(buffer, len);
        break;
    case S_MATCH_SUMMARY:
        Handle_S_MatchSummary(buffer, len);
        break;

    default:
        break;
    }
}

void ClientPacketHandler::Handle_S_TEST(BYTE* buffer, int32 len)
{
    PacketHeader* header = (PacketHeader*)buffer;
    uint16 id = header->id;
    uint16 size = header->size;

    Protocol::S_TESTMsg pkt;
    pkt.ParseFromArray(&header[1], size - sizeof(PacketHeader));

    string msg = pkt.message();

    DEBUG_LOG("?쒕쾭?먯꽌 ?섏뼱 ???곗씠??: " << msg.c_str() << " ");
}

void ClientPacketHandler::Handle_S_EnterGame(BYTE* buffer, int32 len)
{
    PacketHeader* header = (PacketHeader*)buffer;
    uint16 id = header->id;
    uint16 size = header->size;

    Protocol::S_EnterGame pkt;
    pkt.ParseFromArray(&header[1], size - sizeof(PacketHeader));

    auto accountId = pkt.accountid();
    auto isSuccess = pkt.success();
    FogOfWar::SetEnabled(pkt.fogofwar());
    GameFlow::SetPracticeEnabled(pkt.practice());

    if (pkt.protocolversion() != kProtocolVersion)
    {
        GameFlow::SetVersionWarning(UiStrings::Format(
            "warn.version", {std::to_string(pkt.protocolversion()), std::to_string(kProtocolVersion)}));
        DEBUG_LOG("[Protocol] server version " << pkt.protocolversion() << " differs from the client's "
                                               << kProtocolVersion);
    }

    DEBUG_LOG("?쒕쾭?먯꽌 ?섏뼱 ???곗씠???⑦궥紐?: S_EnterGame : " << accountId << " , " << isSuccess);
}

void ClientPacketHandler::Handle_S_MyPlayer(BYTE* buffer, int32 len)
{
    // ???⑦궥 ?뚯떛
    PacketHeader* header = (PacketHeader*)buffer;
    uint16 id = header->id;
    uint16 size = header->size;

    Protocol::S_MyPlayer pkt;
    pkt.ParseFromArray(&header[1], size - sizeof(PacketHeader));

    const Protocol::ObjectInfo& info = pkt.info();

    // ???뚮젅?댁뼱 ?ㅻ툕?앺듃 ?앹꽦
    shared_ptr<GameObject> obj = make_shared<GameObject>(info.name());

    // ??Transform ?ㅼ젙 (?뚯쟾 諛??ш린 議곗젙)
    obj->GetOrAddTransform()->SetPosition(Vec3(info.position().x(), 2, info.position().z()));
    obj->GetOrAddTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), 0.f, 0.f));
    obj->GetOrAddTransform()->SetScale(Vec3(0.0001f));

    // ??紐⑤뜽 濡쒕뱶 諛??좊땲硫붿씠??異붽?
    shared_ptr<class Model> model;

    wstring champ;

    PreloadUnit(info.champtype(), info.skinid());
    PreloadMapUnits();
    SummonerSpellsClient::PreloadVfx();
    ReadUnitInfo(info);

    switch (info.champtype())
    {
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_ANNIE:
        champ = L"Annie";
        obj->AddComponent(make_shared<AnniePlayerController>());
        obj->GetScript<AnniePlayerController>()->_playerInfo = make_shared<Protocol::ObjectInfo>(pkt.info());
        GAMEMANAGER->_myPlayer = obj->GetScript<AnniePlayerController>();

        model = LoadUnitModel(SkinFolder(champ, info.skinid()), ChampionClips);
        {
            auto collider = make_shared<SphereCollider>();
            collider->SetRadius(20000.f);
            obj->AddComponent(collider);
        }

        obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));

        obj->GetModelAnimator()->SetModel(model);
        obj->GetModelAnimator()->SetPass(2);

        break;
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_ASHE:
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_AHRI:
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_SONA:
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_GRAVES:
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_KATARINA:
        champ = FindKit(info.champtype())->key;
        obj->AddComponent(make_shared<KitPlayerController>());
        obj->GetScript<KitPlayerController>()->_playerInfo = make_shared<Protocol::ObjectInfo>(pkt.info());
        GAMEMANAGER->_myPlayer = obj->GetScript<KitPlayerController>();
        obj->GetTransform()->SetScale(Vec3(0.01f));
        model = LoadUnitModel(SkinFolder(champ, info.skinid()), ChampionClips);
        obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));
        obj->GetModelAnimator()->SetModel(model);
        obj->GetModelAnimator()->SetPass(11);
        break;
    case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_GAREN:
        champ = L"Garen";
        obj->AddComponent(make_shared<GarenPlayerController>());
        obj->GetScript<GarenPlayerController>()->_playerInfo = make_shared<Protocol::ObjectInfo>(pkt.info());
        GAMEMANAGER->_myPlayer = obj->GetScript<GarenPlayerController>();
        obj->GetTransform()->SetScale(Vec3(0.01f));

        model = LoadUnitModel(SkinFolder(champ, info.skinid()), ChampionClips);
        obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));

        obj->GetModelAnimator()->SetModel(model);
        obj->GetModelAnimator()->SetPass(11);
    }

    ApplySkinTransform(obj, champ, info.skinid());

    // ???ъ뿉 異붽?
    CUR_SCENE->Add(obj);
    CUR_SCENE->RegisterObject(info.objectid(), obj);
    BasePlayerController::PreloadRecallModels(info.champtype(), info.skinid());

    CUR_SCENE->GetMainCamera()->GetScript<CameraController>()->_target = obj;
    CUR_SCENE->GetMainCamera()->GetScript<CameraController>()->_offset =
        CUR_SCENE->GetMainCamera()->GetTransform()->GetPosition() - obj->GetTransform()->GetPosition();

    {
        auto hud = make_shared<GameObject>("PlayerHUD");
        hud->GetOrAddTransform();
        hud->AddComponent(make_shared<HUDController>());
        UI->SetHUDControllerGameObject(hud);
        CUR_SCENE->Add(hud);
    }

    UI->SetTarget(info.champtype());
    HealthBars::Get();
}

void ClientPacketHandler::Handle_S_AddObject(BYTE* buffer, int32 len)
{
    // ???⑦궥 ?뚯떛
    PacketHeader* header = (PacketHeader*)buffer;
    uint16 id = header->id;
    uint16 size = header->size;

    Protocol::S_AddObject pkt;
    pkt.ParseFromArray(&header[1], size - sizeof(PacketHeader));

    // Objects Setting
    {
        uint64 myPlayerId = GAMEMANAGER->_myPlayer->_playerInfo->objectid();
        const int32 size = pkt.objects_size();

        for (int32 i = 0; i < size; i++)
        {
            const Protocol::ObjectInfo& info = pkt.objects(i);

            if (myPlayerId == info.objectid())
                continue;

            // ??以묐났 泥댄겕 (?대? 議댁옱?섎뒗 ObjectId?몄? ?뺤씤)
            if (CUR_SCENE->FindObjectById(info.objectid()) != nullptr)
            {
                continue;
            }

            // ???뚮젅?댁뼱 ?ㅻ툕?앺듃 ?앹꽦
            shared_ptr<GameObject> obj = make_shared<GameObject>(info.name());

            // ??Transform ?ㅼ젙 (?뚯쟾 諛??ш린 議곗젙)
            obj->GetOrAddTransform()->SetPosition(Vec3(info.position().x(), info.position().y(), info.position().z()));
            obj->GetOrAddTransform()->SetRotation(Vec3(XMConvertToRadians(90.f), 0.f, 0.f));
            obj->GetOrAddTransform()->SetScale(Vec3(0.0001f));

            // ??紐⑤뜽 濡쒕뱶 諛??좊땲硫붿씠??異붽?
            shared_ptr<class Model> model;

            wstring champ;

            PreloadUnit(info.champtype(), info.skinid());
            ReadUnitInfo(info);

            switch (info.champtype())
            {
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_ANNIE:
                champ = L"Annie";
                obj->AddComponent(make_shared<AnnieOtherPlayerController>());
                obj->GetScript<AnnieOtherPlayerController>()->_playerInfo =
                    make_shared<Protocol::ObjectInfo>(pkt.objects(i));

                model = LoadUnitModel(SkinFolder(champ, info.skinid()), ChampionClips);

                {
                    auto collider = make_shared<SphereCollider>();
                    collider->SetRadius(20000.f);
                    obj->AddComponent(collider);
                }

                obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));

                obj->GetModelAnimator()->SetModel(model);
                obj->GetModelAnimator()->SetPass(2);

                break;
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_TIBBERS:

                champ = L"AnnieTibbers";
                obj->AddComponent(make_shared<AnnieOtherPlayerController>());
                obj->GetScript<AnnieOtherPlayerController>()->_playerInfo =
                    make_shared<Protocol::ObjectInfo>(pkt.objects(i));
                obj->GetOrAddTransform()->SetScale(Vec3(0.01f));
                model = LoadUnitModel(champ, TibbersClips);
                {
                    auto collider = make_shared<SphereCollider>();
                    collider->SetRadius(155.f);
                    obj->AddComponent(collider);
                }

                obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));
                obj->GetModelAnimator()->SetModel(model);
                obj->GetModelAnimator()->SetPass(11);
                {
                    LolVfx::PlayOptions options;
                    options.follow = obj;
                    options.duration = 3600.f;
                    UnitStatus::Of(info.objectid()).vfx[TibbersAuraVfxKey] = LolVfx::Get()->Play(
                        "AnnieTibbers_Base_Idle_Ground", obj->GetTransform()->GetPosition(), Vec3::Forward, options);
                }

                break;
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_ASHE:
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_AHRI:
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_SONA:
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_GRAVES:
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_KATARINA:
                champ = FindKit(info.champtype())->key;
                obj->AddComponent(make_shared<KitOtherPlayerController>());
                obj->GetScript<KitOtherPlayerController>()->_playerInfo =
                    make_shared<Protocol::ObjectInfo>(pkt.objects(i));
                obj->GetOrAddTransform()->SetScale(Vec3(0.01f));
                model = LoadUnitModel(SkinFolder(champ, info.skinid()), ChampionClips);
                {
                    auto collider = make_shared<SphereCollider>();
                    collider->SetRadius(210.f);
                    obj->AddComponent(collider);
                }

                obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));
                obj->GetModelAnimator()->SetModel(model);
                obj->GetModelAnimator()->SetPass(11);
                break;
            case Protocol::PLAYER_CHAMPION_TYPE::PLAYER_TYPE_GAREN:
                champ = L"Garen";
                obj->AddComponent(make_shared<GarenOtherPlayerController>());

                obj->GetScript<GarenOtherPlayerController>()->_playerInfo =
                    make_shared<Protocol::ObjectInfo>(pkt.objects(i));
                obj->GetOrAddTransform()->SetScale(Vec3(0.01f));

                model = LoadUnitModel(SkinFolder(champ, info.skinid()), ChampionClips);

                {
                    auto collider = make_shared<SphereCollider>();
                    collider->SetRadius(210.f);
                    obj->AddComponent(collider);
                }

                obj->AddComponent(make_shared<ModelAnimator>(CUR_SCENE->_shader));

                obj->GetModelAnimator()->SetModel(model);
                obj->GetModelAnimator()->SetPass(11);

                break;
            default:
                if (!IsMapUnit(info.champtype()))
                    continue;
                SetupMapUnit(obj, info);
                break;
            }

            if (info.champtype() != Protocol::PLAYER_TYPE_TIBBERS && !IsMapUnit(info.champtype()))
                ApplySkinTransform(obj, champ, info.skinid());

            // ???ъ뿉 異붽?
            CUR_SCENE->Add(obj);
            CUR_SCENE->RegisterObject(info.objectid(), obj);

            if (!IsMapUnit(info.champtype()))
                BasePlayerController::PreloadRecallModels(info.champtype(), info.skinid());
            HealthBars::Get();

            if (IsMapUnit(info.champtype()))
                GameSounds::Spawn(obj);
        }
    }
}

wstring ChampionSkinFolder(int32 champType, int32 skinId)
{
    const string name = GameSounds::ChampionName(champType);

    return name.empty() ? wstring() : SkinFolder(Utils::ToWString(name), skinId);
}

shared_ptr<Model> LoadChampionModel(const wstring& folder)
{
    return LoadUnitModel(folder, ChampionClips);
}

void ClientPacketHandler::Handle_S_RemoveObject(BYTE* buffer, int32 len)
{
    // ???⑦궥 ?뚯떛
    PacketHeader* header = (PacketHeader*)buffer;
    uint16 id = header->id;
    uint16 size = header->size;

    Protocol::S_RemoveObject pkt;
    pkt.ParseFromArray(&header[1], size - sizeof(PacketHeader));

    const int32 Idssize = pkt.ids_size();

    for (int32 i = 0; i < Idssize; i++)
    {
        auto id = pkt.ids(i);
        LeaveRubble(CUR_SCENE->FindObjectById(id));
        UnitStatus::Remove(id);
        CUR_SCENE->RemoveObject(id);
    }
}

void ClientPacketHandler::Handle_S_Move(BYTE* buffer, int32 len)
{
    // ???⑦궥 ?뚯떛
    PacketHeader* header = (PacketHeader*)buffer;
    uint16 id = header->id;
    uint16 size = header->size;

    Protocol::S_Move pkt;
    pkt.ParseFromArray(&header[1], size - sizeof(PacketHeader));

    // ???뚮젅?댁뼱 李얘린
    const Protocol::ObjectInfo& info = pkt.info();
    auto targetPlayer = CUR_SCENE->FindObjectById(info.objectid());

    if (targetPlayer == nullptr)
        return;

    // ???먭린 ?먯떊?대㈃ 臾댁떆
    if (info.objectid() == GAMEMANAGER->_myPlayer->_playerInfo->objectid())
        return;

    // ??MovementController瑜?李얠븘 ?대룞 紐낅졊 ?ㅽ뻾
    auto movementController = targetPlayer->GetOrAddScript<OtherPlayerController>();

    if (movementController)
    {
        Vec3 newPos(info.position().x(), info.position().y(), info.position().z());
        movementController->SetTargetPosition(newPos);
    }
}

void ClientPacketHandler::Handle_S_NavigationInfo(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_NavigationInfo pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;

    if (pkt.formatversion() != 1 || pkt.navigationmapid().empty() || pkt.navigationcontenthash().empty())
    {
        NETWORK->ClearNavigationInfo();
        DEBUG_LOG("[Navigation] Invalid NavigationInfo received.");

        return;
    }

    NETWORK->SetNavigationInfo(pkt.roomid(), pkt.formatversion(), pkt.navigationmapid(), pkt.navigationcontenthash());
    DEBUG_LOG("[Navigation] Room=" << pkt.roomid() << " MapId=" << pkt.navigationmapid().c_str() << " Format="
                                   << pkt.formatversion() << " Hash=" << pkt.navigationcontenthash().c_str());
}

void ClientPacketHandler::Handle_S_MoveAccepted(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_MoveAccepted pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;

    if (!NETWORK->TryAcceptMoveResponse(pkt.clientmovesequence(), pkt.navigationmapid(), pkt.navigationcontenthash()))
    {
        DEBUG_LOG("[Movement] Ignored stale or mismatched MoveAccepted sequence=" << pkt.clientmovesequence());

        return;
    }

    if (!GAMEMANAGER->_myPlayer || !pkt.has_accepteddestination())
        return;

    const auto& destination = pkt.accepteddestination();
    GAMEMANAGER->_myPlayer->OnMoveAccepted(pkt.servermoveid(), Vec3(destination.x(), destination.y(), destination.z()),
                                           pkt.wasdestinationadjusted());
}

void ClientPacketHandler::Handle_S_MoveRejected(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_MoveRejected pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;

    if (!NETWORK->TryAcceptMoveResponse(pkt.clientmovesequence(), pkt.navigationmapid(), pkt.navigationcontenthash()))
    {
        DEBUG_LOG("[Movement] Ignored stale or mismatched MoveRejected sequence=" << pkt.clientmovesequence());

        return;
    }

    if (!GAMEMANAGER->_myPlayer || !pkt.has_serverposition())
        return;

    const auto& serverPosition = pkt.serverposition();
    GAMEMANAGER->_myPlayer->OnMoveRejected(Vec3(serverPosition.x(), serverPosition.y(), serverPosition.z()),
                                           pkt.rejectreason());
}

void ClientPacketHandler::Handle_S_MovementSnapshot(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_MovementSnapshot pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)) || !pkt.has_position())
        return;

    if (!NETWORK->HasNavigationInfo() || pkt.roomid() != NETWORK->GetNavigationRoomId())
        return;

    shared_ptr<BasePlayerController> controller;
    const bool isLocalPlayer = GAMEMANAGER->_myPlayer && GAMEMANAGER->_myPlayer->_playerInfo &&
                               GAMEMANAGER->_myPlayer->_playerInfo->objectid() == pkt.objectid();

    if (isLocalPlayer)
    {
        controller = GAMEMANAGER->_myPlayer;
    }
    else
    {
        auto object = CUR_SCENE->FindObjectById(pkt.objectid());

        if (object)
            controller = object->GetScript<BasePlayerController>();
    }

    if (!controller)
        return;

    const auto& position = pkt.position();

    if (!controller->ApplyAuthoritativeSnapshot(pkt.servermoveid(), pkt.servertick(),
                                                Vec3(position.x(), position.y(), position.z()), pkt.movementstate()))
    {
        return;
    }

    if (isLocalPlayer)
        GAMEMANAGER->_myPlayer->OnMovementSnapshotState(pkt.movementstate());
}

void ClientPacketHandler::Handle_S_SkillResult(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_SkillResult pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;

    const uint64 casterId = pkt.casterid();
    const int32 skillId = pkt.skillid();
    const bool isLocal = GAMEMANAGER->_myPlayer && GAMEMANAGER->_myPlayer->_playerInfo &&
                         GAMEMANAGER->_myPlayer->_playerInfo->objectid() == casterId;

    const bool localAnnieW = isLocal && skillId == (int32)SkillType::WSpell &&
                             GAMEMANAGER->_myPlayer->_playerInfo->champtype() == Protocol::PLAYER_TYPE_ANNIE;

    if (pkt.followup())
    {
        if (auto casterObject = isLocal ? GAMEMANAGER->_myPlayer->GetGameObject() : CUR_SCENE->FindObjectById(casterId))
            if (auto casterController = casterObject->GetScript<BasePlayerController>();
                casterController && casterController->_playerInfo)
                if (const ChampionKit* kit = FindKit(casterController->_playerInfo->champtype());
                    kit && skillId >= 1 && skillId <= 4)
                {
                    const Vec3 center(pkt.centerpos().x(), pkt.centerpos().y(), pkt.centerpos().z());
                    Vec3 forward(pkt.castdirection().x(), 0.f, pkt.castdirection().z());

                    if (forward.LengthSquared() < 1e-4f)
                        forward = Vec3(0.f, 0.f, 1.f);
                    const int32 skinId = casterController->_playerInfo->skinid();

                    if (kit->followUpVfx[skillId - 1])
                    {
                        LolVfx::PlayOptions options;
                        options.duration = skillId == (int32)SkillType::WSpell ? 4.f : 0.f;
                        LolVfx::Get()->Play(SkinVfx(kit->followUpVfx[skillId - 1], skinId), center, forward, options);
                    }

                    if (kit->followUpBurstVfx[skillId - 1])
                        LolVfx::Get()->Play(SkinVfx(kit->followUpBurstVfx[skillId - 1], skinId), center, forward,
                                            LolVfx::PlayOptions());

                    if (kit->followUpSounds[skillId - 1])
                        SOUND->PlayCueAt(kit->followUpSounds[skillId - 1], center);
                }
        return;
    }

    const ChampionKit* casterKit = isLocal ? FindKit(GAMEMANAGER->_myPlayer->_playerInfo->champtype()) : nullptr;
    const bool localKitSpell = casterKit && casterKit->serverCast && skillId != (int32)SkillType::GeneralAtk;

    if (isLocal && !localAnnieW && !localKitSpell && !MapAutoTest::IsDemo())
        return;

    auto duplicate = g_lastPlayedSkill.find(casterId);

    const ChampionKit* anyKit = nullptr;

    if (auto casterObject = CUR_SCENE->FindObjectById(casterId))
        if (auto casterController = casterObject->GetScript<BasePlayerController>();
            casterController && casterController->_playerInfo)
            anyKit = FindKit(casterController->_playerInfo->champtype());

    if (skillId != (int32)SkillType::GeneralAtk && !(anyKit && anyKit->serverCast) &&
        duplicate != g_lastPlayedSkill.end() && duplicate->second == skillId)
        return;

    shared_ptr<BasePlayerController> controller;

    if (isLocal)
    {
        controller = GAMEMANAGER->_myPlayer;
    }
    else
    {
        auto caster = CUR_SCENE->FindObjectById(casterId);

        if (caster)
            controller = caster->GetScript<BasePlayerController>();
    }

    if (!controller)
        return;

    if (pkt.hitobjects_size() > 0)
    {
        auto target = CUR_SCENE->FindObjectById(pkt.hitobjects(0));

        if (target)
            controller->SetTarget(target);
    }

    Vec3 castOrigin = controller->GetTransform()->GetPosition();
    Vec3 castDirection = controller->GetTransform()->GetLook();

    if (pkt.has_castorigin())
        castOrigin = Vec3(pkt.castorigin().x(), pkt.castorigin().y(), pkt.castorigin().z());

    if (pkt.has_castdirection())
        castDirection = Vec3(pkt.castdirection().x(), pkt.castdirection().y(), pkt.castdirection().z());

    g_lastPlayedSkill[casterId] = skillId;

    if (skillId == (int32)SkillType::WSpell && controller->_playerInfo &&
        controller->_playerInfo->champtype() == Protocol::PLAYER_TYPE_ANNIE)
    {
        LolVfx::Get()->Play("Annie_Base_W_cas_Right", castOrigin, castDirection);
    }

    if (controller->_playerInfo)
        if (const ChampionKit* kit = FindKit(controller->_playerInfo->champtype());
            kit && kit->blinkSkill == skillId && pkt.has_centerpos())
            controller->SnapTo(Vec3(pkt.centerpos().x(), pkt.centerpos().y(), pkt.centerpos().z()));
    GameSounds::Cast(controller->GetGameObject(), skillId);

    if (pkt.isareaskill() && pkt.arearadius() > 0.f && controller->_playerInfo &&
        controller->_playerInfo->champtype() == Protocol::PLAYER_TYPE_ASHE && skillId == (int32)SkillType::ESpell)
        if (auto me = GAMEMANAGER->_myPlayer;
            me && me->_playerInfo && me->_playerInfo->teamid() == controller->_playerInfo->teamid())
            FogOfWar::Get()->AddTemporarySight(
                Vec3(pkt.centerpos().x(), pkt.centerpos().y(), pkt.centerpos().z()), pkt.arearadius(),
                5.f + Vec3::Distance(castOrigin, Vec3(pkt.centerpos().x(), pkt.centerpos().y(), pkt.centerpos().z())) /
                          14.f);
    controller->PlayServerSkillResult(skillId, castOrigin, castDirection);
}

void ClientPacketHandler::Handle_S_ProjectileSpawn(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_ProjectileSpawn pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;

    auto casterIt = CUR_SCENE->_players.find(pkt.casterid());

    if (casterIt == CUR_SCENE->_players.end())
        return;
    shared_ptr<GameObject> caster = casterIt->second;
    shared_ptr<GameObject> target;
    auto targetIt = CUR_SCENE->_players.find(pkt.targetid());

    if (targetIt != CUR_SCENE->_players.end())
        target = targetIt->second;

    auto controller = caster->GetScript<BasePlayerController>();
    const MapUnitInfo* mapUnit =
        controller && controller->_playerInfo ? FindMapUnit(controller->_playerInfo->champtype()) : nullptr;
    Vec3 startPos = caster->GetTransform()->GetPosition();
    startPos.y += mapUnit && mapUnit->missileHeight > 0.f ? mapUnit->missileHeight : 1.4f;

    if (controller && controller->_playerInfo &&
        controller->_playerInfo->champtype() == Protocol::PLAYER_TYPE_KATARINA && pkt.has_startpos() &&
        Vec3::Distance(Vec3(pkt.startpos().x(), 0.f, pkt.startpos().z()), Vec3(startPos.x, 0.f, startPos.z)) > 1.f)
        startPos = Vec3(pkt.startpos().x(), pkt.startpos().y() + 1.2f, pkt.startpos().z());
    Vec3 endPos(pkt.endpos().x(), pkt.endpos().y() + 1.4f, pkt.endpos().z());

    if (target)
    {
        endPos = target->GetTransform()->GetPosition();
        endPos.y += 1.4f;
    }

    auto duplicateIt = CUR_SCENE->_projectiles.find(pkt.projectileid());

    if (duplicateIt != CUR_SCENE->_projectiles.end())
    {
        DEBUG_LOG("[Projectile] Duplicate spawn replaced. id=" << pkt.projectileid());
        ReleaseProjectileVisual(duplicateIt->second);
        CUR_SCENE->_projectiles.erase(duplicateIt);
    }

    auto projectileObj = AcquireProjectileVisual();
    projectileObj->GetTransform()->SetPosition(startPos);
    CUR_SCENE->Add(projectileObj);
    const int championType = controller && controller->_playerInfo ? controller->_playerInfo->champtype() : -1;
    const bool annieQ = UsesAnnieQEffect(championType, pkt);
    const bool basicAttack = !annieQ && pkt.skillid() == (int32)SkillType::GeneralAtk && championType >= 0;

    const char* kitMissile =
        championType >= 0 && pkt.has_skillid() ? KitMissileVfx(championType, pkt.skillid(), pkt.variant()) : "";
    const int32 skinId = controller && controller->_playerInfo ? controller->_playerInfo->skinid() : 0;
    const string missileVfx = SkinVfx(
        annieQ         ? "Annie_Base_Q_mis"
        : *kitMissile  ? kitMissile
        : !basicAttack ? ""
        : mapUnit
            ? MapUnitMissileVfx(*controller->_playerInfo)
            : GetChampionCombatStats(static_cast<Protocol::PLAYER_CHAMPION_TYPE>(championType)).basicAttackMissileVfx,
        skinId);
    projectileObj->GetScript<ProjectileScript>()->SetTarget(endPos, pkt.speed(), target, annieQ, missileVfx);

    if (basicAttack)
        GameSounds::MissileLaunch(caster, target);
    else if (pkt.has_skillid() && pkt.skillid() > 0 && *kitMissile)
        GameSounds::MissileLaunch(caster, target, pkt.skillid(), pkt.variant());

    if (basicAttack)
        projectileObj->GetScript<ProjectileScript>()->SetHitVfx(SkinVfx(
            mapUnit
                ? MapUnitHitVfx(*controller->_playerInfo)
                : GetChampionCombatStats(static_cast<Protocol::PLAYER_CHAMPION_TYPE>(championType)).basicAttackHitVfx,
            skinId));

    if (championType == Protocol::PLAYER_TYPE_ANNIE && !pkt.has_skillid())
    {
        static bool warned = false;

        if (!warned)
            DEBUG_LOG(
                "[AnnieQ] Server has no projectile skillId; using legacy VFX. Rebuild/restart the updated server.");
        warned = true;
    }

    CUR_SCENE->_projectiles[pkt.projectileid()] = projectileObj;
}

void ClientPacketHandler::Handle_S_ProjectileHit(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_ProjectileHit pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;

    auto projectileIt = CUR_SCENE->_projectiles.find(pkt.projectileid());

    if (projectileIt == CUR_SCENE->_projectiles.end())
        return;

    auto projectileObject = projectileIt->second;

    if (auto projectileScript = projectileObject->GetScript<ProjectileScript>())
        projectileScript->RemoveTrail();

    Vec3 impactPosition = projectileObject->GetTransform()->GetPosition();
    auto targetIt = CUR_SCENE->_players.find(pkt.targetid());

    if (targetIt != CUR_SCENE->_players.end())
    {
        impactPosition = targetIt->second->GetTransform()->GetPosition();
        impactPosition.y += 1.2f;
    }

    if (auto projectileScript = projectileObject->GetScript<ProjectileScript>())
        projectileScript->OnHit(impactPosition);
    ReleaseProjectileVisual(projectileObject);
    CUR_SCENE->_projectiles.erase(projectileIt);
}

void ClientPacketHandler::Handle_S_Damage(BYTE* buffer, int32 len)
{
    Protocol::S_Damage pkt;

    if (!pkt.ParseFromArray(&buffer[sizeof(PacketHeader)], len - sizeof(PacketHeader)))
        return;

    const uint64 targetId = pkt.targetid();
    const int32 damage = pkt.damage();
    const int32 remainHp = pkt.remainhp();
    shared_ptr<GameObject> targetObject;

    if (GAMEMANAGER->_myPlayer && GAMEMANAGER->_myPlayer->_playerInfo &&
        GAMEMANAGER->_myPlayer->_playerInfo->objectid() == targetId)
    {
        GAMEMANAGER->_myPlayer->_playerInfo->set_hp(remainHp);
        targetObject = GAMEMANAGER->_myPlayer->GetGameObject();
    }
    else
    {
        auto targetIt = CUR_SCENE->_players.find(targetId);

        if (targetIt != CUR_SCENE->_players.end())
        {
            targetObject = targetIt->second;

            if (auto controller = targetObject->GetScript<BasePlayerController>())
                if (controller->_playerInfo)
                    controller->_playerInfo->set_hp(remainHp);
        }
    }

    if (pkt.has_skillid() && targetObject)
    {
        shared_ptr<GameObject> sourceObject;

        if (auto sourceIt = CUR_SCENE->_players.find(pkt.sourceid()); sourceIt != CUR_SCENE->_players.end())
            sourceObject = sourceIt->second;
        else if (GAMEMANAGER->_myPlayer && GAMEMANAGER->_myPlayer->_playerInfo &&
                 GAMEMANAGER->_myPlayer->_playerInfo->objectid() == pkt.sourceid())
            sourceObject = GAMEMANAGER->_myPlayer->GetGameObject();
        auto sourceController = sourceObject ? sourceObject->GetScript<BasePlayerController>() : nullptr;

        if (sourceController && sourceController->_playerInfo)
            PlaySkillHitVfx(sourceController->_playerInfo->champtype(), pkt.skillid(), sourceObject, targetObject);

        if (damage > 0)
            GameSounds::Hit(sourceObject, pkt.skillid(), targetObject);
    }

    const uint64 myId = GAMEMANAGER->_myPlayer && GAMEMANAGER->_myPlayer->_playerInfo
                            ? GAMEMANAGER->_myPlayer->_playerInfo->objectid()
                            : 0;
    const bool receivedByMe = targetId == myId;
    bool dealtByMe = pkt.sourceid() == myId || pkt.sourceid() == 0;

    if (!dealtByMe)
        if (auto source = CUR_SCENE->FindObjectById(pkt.sourceid()))
            if (auto controller = source->GetScript<BasePlayerController>(); controller && controller->_playerInfo)
                dealtByMe = myId != 0 && controller->_playerInfo->ownerid() == myId;

    if (targetObject && damage > 0 && (dealtByMe || receivedByMe))
        DamageText::Get()->Show(targetObject, damage, static_cast<DamageText::Type>(std::clamp(pkt.damagetype(), 0, 2)),
                                receivedByMe && pkt.sourceid() != myId, pkt.crit());
    else if (!targetObject)
        DEBUG_LOG("[Combat] Damage target object was not found: " << targetId);
}

void ClientPacketHandler::Handle_S_Buff(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_Buff pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;
    const int32 soulBefore = UnitStatus::Find(pkt.targetid()) ? UnitStatus::Find(pkt.targetid())->dragonSoul : -1;
    UnitStatus::ApplyBuff(pkt);

    if (pkt.bufftype() == Protocol::BUFF_DRAGON_SOUL && soulBefore < 0 && pkt.duration() > 0.f)
        if (auto self = GAMEMANAGER->_myPlayer;
            self && self->_playerInfo && self->_playerInfo->objectid() == pkt.targetid())
            if (auto hud = HUDController::Find())
            {
                static const char* const Souls[4] = {"buff.soul_infernal", "buff.soul_mountain", "buff.soul_ocean",
                                                     "buff.soul_cloud"};
                const int32 element = std::clamp(static_cast<int32>(pkt.amount()), 0, 3);
                hud->ShowAnnouncement(UiStrings::Format("announce.soul", {UiStrings::Get(Souls[element])}), true);
            }

    auto me = GAMEMANAGER->_myPlayer;

    if (pkt.bufftype() == Protocol::BUFF_STUN && pkt.duration() > 0.f && me && me->_playerInfo &&
        me->_playerInfo->objectid() == pkt.targetid())
        me->InterruptForCrowdControl();
}

void ClientPacketHandler::Handle_S_HpUpdate(BYTE* buffer, int32 len)
{
    PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
    Protocol::S_HpUpdate pkt;

    if (!pkt.ParseFromArray(&header[1], header->size - sizeof(PacketHeader)))
        return;
    UnitStatus::SetShield(pkt.targetid(), pkt.shield());
    shared_ptr<BasePlayerController> controller;
    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == pkt.targetid())
        controller = me;
    else if (auto object = CUR_SCENE->FindObjectById(pkt.targetid()))
        controller = object->GetScript<BasePlayerController>();

    if (controller && controller->_playerInfo)
    {
        controller->_playerInfo->set_hp(pkt.hp());
        controller->_playerInfo->set_maxhp(pkt.maxhp());
    }
}

static void AnnounceEpicKill(Protocol::PLAYER_CHAMPION_TYPE type, uint64 killerId)
{
    const char* name = nullptr;

    switch (type)
    {
    case Protocol::PLAYER_TYPE_SRU_DRAGON_FIRE:
        name = "epic.infernal";
        break;
    case Protocol::PLAYER_TYPE_SRU_DRAGON_EARTH:
        name = "epic.mountain";
        break;
    case Protocol::PLAYER_TYPE_SRU_DRAGON_WATER:
        name = "epic.ocean";
        break;
    case Protocol::PLAYER_TYPE_SRU_DRAGON_AIR:
        name = "epic.cloud";
        break;
    case Protocol::PLAYER_TYPE_SRU_BARON:
        name = "epic.baron";
        break;
    case Protocol::PLAYER_TYPE_SRU_DRAGON_ELDER:
        name = "epic.elder";
        break;
    case Protocol::PLAYER_TYPE_SRU_RIFTHERALD:
        name = "epic.herald";
        break;
    default:
        return;
    }

    auto killer = ControllerOf(killerId);
    auto me = GAMEMANAGER->_myPlayer;

    if (!killer || !killer->_playerInfo || !me || !me->_playerInfo)
        return;
    const bool ally = killer->_playerInfo->teamid() == me->_playerInfo->teamid();

    if (auto hud = UI->GetHUD())
        if (auto controller = hud->GetScript<HUDController>())
            controller->ShowAnnouncement(
                UiStrings::Format(ally ? "announce.ally" : "announce.enemy", {UiStrings::Get(name)}), ally);
}

void ClientPacketHandler::Handle_S_Dead(BYTE* buffer, int32 len)
{
    Protocol::S_Dead pkt;

    if (!Parse(buffer, pkt))
        return;
    auto controller = ControllerOf(pkt.targetid());

    if (!controller || !controller->_playerInfo)
        return;
    controller->_playerInfo->set_hp(0);
    UnitStatus::State& state = UnitStatus::Of(pkt.targetid());
    state.shield = 0;
    GameSounds::Death(controller->GetGameObject(), controller == GAMEMANAGER->_myPlayer);

    if (const char* explosion =
            StructureExplosion(controller->_playerInfo->champtype(), controller->_playerInfo->teamid() == 2))
        LolVfx::Get()->Play(explosion, controller->GetGameObject()->GetTransform()->GetPosition(), Vec3::Forward);

    if (auto me = GAMEMANAGER->_myPlayer;
        me && me->_playerInfo && pkt.gold() > 0 && pkt.killerid() == me->_playerInfo->objectid())
    {
        DamageText::Get()->ShowReward(controller->GetGameObject(), "+" + std::to_string(pkt.gold()),
                                      DamageText::Type::Gold);
        SOUND->PlayCue("Play_sfx_GoldAquisition_P_cast", 0.8f);
    }

    if (IsMapUnit(controller->_playerInfo->champtype()))
    {
        SwapInhibitorModel(*controller, true);
        controller->PlayDeath();
        AnnounceEpicKill(controller->_playerInfo->champtype(), pkt.killerid());

        return;
    }

    if (pkt.respawnseconds() <= 0.f)
        return;
    state.respawnAt = TIME->GetGameTime() + pkt.respawnseconds();
    controller->PlayDeath();

    if (auto me = GAMEMANAGER->_myPlayer; me && controller == me)
        me->InterruptForCrowdControl();
}

void ClientPacketHandler::Handle_S_Respawn(BYTE* buffer, int32 len)
{
    Protocol::S_Respawn pkt;

    if (!Parse(buffer, pkt))
        return;
    auto controller = ControllerOf(pkt.targetid());

    if (!controller || !controller->_playerInfo)
        return;
    controller->_playerInfo->set_hp(pkt.hp());
    controller->_playerInfo->set_maxhp(pkt.maxhp());
    controller->_playerInfo->set_mp(pkt.mp());
    controller->_playerInfo->set_maxmp(pkt.maxmp());
    UnitStatus::State& state = UnitStatus::Of(pkt.targetid());
    state.respawnAt = 0.f;
    state.mp = pkt.mp();
    state.maxMp = pkt.maxmp();
    SwapInhibitorModel(*controller, false);
    controller->Respawn(Vec3(pkt.position().x(), pkt.position().y(), pkt.position().z()));
}

void ClientPacketHandler::Handle_S_UnitStats(BYTE* buffer, int32 len)
{
    Protocol::S_UnitStats pkt;

    if (!Parse(buffer, pkt))
        return;
    UnitStatus::State& state = UnitStatus::Of(pkt.targetid());
    state.level = max(1, pkt.level());
    state.mp = pkt.mp();
    state.maxMp = pkt.maxmp();
    state.itemAttackSpeed = pkt.attackspeed();
    auto controller = ControllerOf(pkt.targetid());

    if (controller && controller->_playerInfo)
    {
        controller->_playerInfo->set_level(pkt.level());
        controller->_playerInfo->set_mp(pkt.mp());
        controller->_playerInfo->set_maxmp(pkt.maxmp());
        // A dead champion stays at 0 until S_Respawn.
        if (controller->_playerInfo->hp() > 0 || state.respawnAt == 0.f)
            controller->_playerInfo->set_hp(pkt.hp());
        controller->_playerInfo->set_maxhp(pkt.maxhp());
    }

    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == pkt.targetid())
    {
        UnitStatus::Progress& progress = UnitStatus::MyProgress();

        if (progress.level > 0 && pkt.level() > progress.level)
        {
            SOUND->PlayCue("Play_sfx_levelup_levelup");

            if (auto hud = HUDController::Find())
                hud->ShowLevelUp();
        }

        progress.level = pkt.level();
        progress.skillPoints = pkt.skillpoints();
        progress.gold = pkt.gold();
        progress.xp = pkt.xp();
        progress.xpToNext = pkt.xptonext();

        for (int32 i = 0; i < 4 && i < pkt.skillranks_size(); ++i)
            progress.ranks[i] = pkt.skillranks(i);
        // Totals with items for the shop's stats page.
        auto& s = progress.stats;
        s.attackDamage = pkt.attackdamage();
        s.abilityPower = pkt.abilitypower();
        s.armor = pkt.armor();
        s.magicResist = pkt.magicresist();
        s.attackSpeed = pkt.attackspeed();
        s.moveSpeed = pkt.movespeed();
        s.critChance = pkt.critchance();
        s.abilityHaste = pkt.abilityhaste();
        s.lethality = pkt.lethality();
        s.armorPenPercent = pkt.armorpenpercent();
        s.magicPen = pkt.magicpen();
        s.magicPenPercent = pkt.magicpenpercent();
        s.lifeSteal = pkt.lifesteal();
        s.omnivamp = pkt.omnivamp();
        s.healShieldPower = pkt.healshieldpower();
        s.tenacity = pkt.tenacity();
        s.hpRegen = pkt.hpregen();
        s.manaRegen = pkt.manaregen();
    }
}

void ClientPacketHandler::Handle_S_Inventory(BYTE* buffer, int32 len)
{
    Protocol::S_Inventory pkt;

    if (!Parse(buffer, pkt))
        return;
    UnitStatus::State& state = UnitStatus::Of(pkt.targetid());

    for (int32 i = 0; i < 6; ++i)
        state.items[i] = i < pkt.slots_size() ? pkt.slots(i).itemid() : 0;
    auto me = GAMEMANAGER->_myPlayer;

    if (!me || !me->_playerInfo || me->_playerInfo->objectid() != pkt.targetid())
        return;
    auto& progress = UnitStatus::MyProgress();
    const float now = TIME->GetGameTime();
    auto hud = HUDController::Find();

    for (int32 i = 0; i < 6; ++i)
    {
        auto& slot = progress.items[i];
        const int32 before = slot.itemId, stacksBefore = slot.stacks;

        if (i < pkt.slots_size())
        {
            const auto& s = pkt.slots(i);
            slot.itemId = s.itemid();
            slot.stacks = s.stacks();
            slot.charges = s.charges();
            slot.cooldownDuration = s.cooldownduration();
            slot.cooldownAt = s.cooldown() > 0.f ? now + s.cooldown() : 0.f;
        }
        else
            slot = {};

        if (hud && slot.itemId != 0 && (slot.itemId != before || slot.stacks > stacksBefore))
            hud->FlashItemSlot(i);
    }

    progress.inShop = pkt.inshop();
    progress.canUndo = pkt.canundo();
    progress.gold = pkt.gold();
}

void ClientPacketHandler::Handle_S_Recall(BYTE* buffer, int32 len)
{
    Protocol::S_Recall pkt;

    if (!Parse(buffer, pkt))
        return;
    auto controller = ControllerOf(pkt.targetid());

    if (controller)
        controller->PlayRecall(pkt.state() == 0, pkt.duration());

    if (pkt.state() == 1 && controller && pkt.has_position())
        controller->SnapTo(Vec3(pkt.position().x(), pkt.position().y(), pkt.position().z()));
    auto me = GAMEMANAGER->_myPlayer;

    if (me && me->_playerInfo && me->_playerInfo->objectid() == pkt.targetid() && pkt.state() == 2)
        if (auto hud = HUDController::Find())
            hud->ShowRejection(Protocol::SKILL_REJECT_INVALID);
}

void ClientPacketHandler::Handle_S_Surrender(BYTE* buffer, int32 len)
{
    Protocol::S_Surrender pkt;

    if (Parse(buffer, pkt))
        GameFlow::OnSurrender(pkt);
}

void ClientPacketHandler::Handle_S_DeathRecap(BYTE* buffer, int32 len)
{
    Protocol::S_DeathRecap pkt;

    if (Parse(buffer, pkt))
        GameFlow::OnDeathRecap(pkt);
}

void ClientPacketHandler::Handle_S_MatchSummary(BYTE* buffer, int32 len)
{
    Protocol::S_MatchSummary pkt;

    if (Parse(buffer, pkt))
        GameFlow::OnMatchSummary(pkt);
}

void ClientPacketHandler::Handle_S_GamePause(BYTE* buffer, int32 len)
{
    Protocol::S_GamePause pkt;

    if (Parse(buffer, pkt))
        GameFlow::OnGamePause(pkt);
}

void ClientPacketHandler::Handle_S_RoomInfo(BYTE* buffer, int32 len)
{
    Protocol::S_RoomInfo pkt;

    if (Parse(buffer, pkt))
        GameFlow::OnRoomInfo(pkt);
}

void ClientPacketHandler::Handle_S_RoomClosed(BYTE* buffer, int32 len)
{
    Protocol::S_RoomClosed pkt;

    if (Parse(buffer, pkt))
        GameFlow::OnRoomClosed(pkt);
}

void ClientPacketHandler::Handle_S_ChatMessage(BYTE* buffer, int32 len)
{
    Protocol::S_ChatMessage pkt;

    if (Parse(buffer, pkt))
        Chat::OnPacket(pkt);
}

void ClientPacketHandler::Handle_S_Ping(BYTE* buffer, int32 len)
{
    Protocol::S_Ping pkt;

    if (Parse(buffer, pkt))
        Pings::OnPacket(pkt);
}

void ClientPacketHandler::Handle_S_ItemEffect(BYTE* buffer, int32 len)
{
    Protocol::S_ItemEffect pkt;

    if (!Parse(buffer, pkt))
        return;
    const Items::Item* item = Items::Find(pkt.itemid());
    auto me = GAMEMANAGER->_myPlayer;
    const bool mine = me && me->_playerInfo && me->_playerInfo->objectid() == pkt.casterid();
    shared_ptr<GameObject> caster = mine ? me->GetGameObject() : CUR_SCENE->FindObjectById(pkt.casterid());

    ItemVfx::Play(pkt);

    if (item && caster && item->active && pkt.hitobjects_size() == 0)
        DamageText::Get()->ShowReward(caster, item->name, DamageText::Type::Level);

    if (mine)
    {
        SOUND->PlayCue("Play_sfx_hud_Generic_Button_Click");

        if (auto hud = HUDController::Find())
            for (int32 i = 0; i < 6; ++i)
                if (UnitStatus::MyProgress().items[i].itemId == pkt.itemid())
                    hud->FlashItemSlot(i);
    }
}

void ClientPacketHandler::Handle_S_SkillCooldown(BYTE* buffer, int32 len)
{
    Protocol::S_SkillCooldown pkt;

    if (!Parse(buffer, pkt))
        return;

    if (auto me = GAMEMANAGER->_myPlayer)
        me->ApplyServerCooldown(pkt.skillid(), pkt.duration());
}

void ClientPacketHandler::Handle_S_SummonerSpell(BYTE* buffer, int32 len)
{
    Protocol::S_SummonerSpell pkt;

    if (Parse(buffer, pkt))
        SummonerSpellsClient::OnCast(pkt);
}

void ClientPacketHandler::Handle_S_SummonerCooldown(BYTE* buffer, int32 len)
{
    Protocol::S_SummonerCooldown pkt;

    if (Parse(buffer, pkt))
        SummonerSpellsClient::OnCooldown(pkt);
}

void ClientPacketHandler::Handle_S_Scoreboard(BYTE* buffer, int32 len)
{
    Protocol::S_Scoreboard pkt;

    if (!Parse(buffer, pkt))
        return;
    UnitStatus::SetGameTime(pkt.gametime());

    for (const auto& entry : pkt.entries())
    {
        UnitStatus::State& state = UnitStatus::Of(entry.objectid());
        state.kills = entry.kills();
        state.deaths = entry.deaths();
        state.assists = entry.assists();
        state.cs = entry.cs();
    }
}

void ClientPacketHandler::Handle_S_Trinket(BYTE* buffer, int32 len)
{
    Protocol::S_Trinket pkt;

    if (!Parse(buffer, pkt))
        return;
    auto& progress = UnitStatus::MyProgress();
    progress.trinketCharges = pkt.charges();
    progress.wards = pkt.wards();
    progress.trinketRechargeDuration = pkt.rechargeduration();
    progress.trinketRechargeAt = pkt.recharge() > 0.f ? TIME->GetGameTime() + pkt.recharge() : 0.f;
}

void ClientPacketHandler::Handle_S_KatarinaDagger(BYTE* buffer, int32 len)
{
    Protocol::S_KatarinaDagger pkt;

    if (Parse(buffer, pkt))
        KatarinaDaggers::OnPacket(pkt);
}

void ClientPacketHandler::Handle_S_Announcement(BYTE* buffer, int32 len)
{
    Protocol::S_Announcement pkt;

    if (Parse(buffer, pkt))
        Announcements::OnPacket(pkt);
}

void ClientPacketHandler::Handle_S_SkillRejected(BYTE* buffer, int32 len)
{
    Protocol::S_SkillRejected pkt;

    if (!Parse(buffer, pkt))
        return;
    auto me = GAMEMANAGER->_myPlayer;

    if (!me)
        return;
    me->OnSkillRejected(pkt.skillid(), pkt.reason());

    if (auto hud = UI->GetHUD())
        if (auto controller = hud->GetScript<HUDController>())
            controller->ShowRejection(pkt.reason());
}

void ClientPacketHandler::Handle_S_Vision(BYTE* buffer, int32 len)
{
    Protocol::S_Vision pkt;

    if (!Parse(buffer, pkt))
        return;

    if (auto controller = ControllerOf(pkt.objectid()); controller && controller->_playerInfo && pkt.visible())
    {
        controller->_playerInfo->set_hp(pkt.hp());
        controller->_playerInfo->set_maxhp(pkt.maxhp());
    }

    FogOfWar::Get()->SetHidden(pkt.objectid(), !pkt.visible(),
                               Vec3(pkt.position().x(), pkt.position().y(), pkt.position().z()));
}

void ClientPacketHandler::Handle_S_GameOver(BYTE* buffer, int32 len)
{
    Protocol::S_GameOver pkt;

    if (!Parse(buffer, pkt))
        return;
    auto me = GAMEMANAGER->_myPlayer;
    const bool won = me && me->_playerInfo && me->_playerInfo->teamid() == pkt.winningteam();

    if (auto hud = UI->GetHUD())
        if (auto controller = hud->GetScript<HUDController>())
            controller->ShowGameOver(won);
    Announcements::OnGameOver(won);
    DEBUG_LOG("[Game] Over: team " << pkt.winningteam() << (won ? " (victory)" : " (defeat)"));
}

void ClientPacketHandler::SendEnterGame(ServerSessionRef session)
{
    Protocol::C_EnterGame enter;
    enter.set_token(NETWORK->GetJoinToken());
    session->Send(MakeSendBuffer(enter, C_ENTER_GAME));
}
