#include "pch.h"
#include "TestScene.h"
#include "GameSounds.h"
#include "GameCursor.h"
#include "GeometryHelper.h"
#include "Camera.h"
#include "GameObject.h"
#include "EditCameraScript.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "Material.h"
#include "Model.h"
#include "ModelRenderer.h"
#include "FogOfWar.h"
#include "GroundHeight.h"
#include "ModelAnimator.h"
#include "Mesh.h"
#include "Transform.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Light.h"
#include "PlayerController.h"
#include "Terrain.h"
#include "SphereCollider.h"
#include "Button.h"
#include "CameraController.h"
#include "HUDController.h"
#include "SkillIndicatorController.h"
#include "CursorController.h"
#include "ParticleRenderer.h"
#include "NavigationGridComponent.h"
#include "NavigationEditorTool.h"
#include "AnnieQEffect.h"
#include "AnnieWEffect.h"

void TestScene::Start()
{
    Super::Start();
}

void TestScene::LateUpdate()
{
    Super::LateUpdate();
}

void TestScene::Update()
{
    if (_navigationEditor)
    {
        _navigationEditor->Update();
        SetWorldInputBlocked(_navigationEditor->IsActive());
    }

    Super::Update();
    GameSounds::Update();

    if (!IsWorldInputBlocked() && !ImGui::GetIO().WantCaptureMouse && INPUT->GetButtonDown(KEY_TYPE::LBUTTON))
    {
        int32 mouseX = INPUT->GetMousePos().x;
        int32 mouseY = INPUT->GetMousePos().y;

        Vec3 Pos;
        // Picking ( Test )
        auto pickObj = CUR_SCENE->Pick(mouseX, mouseY);

        if (pickObj)
        {
            DEBUG_LOG("PIck Obj : Attack Test : " << pickObj->_name.c_str());
        }
    }

    if (GameCursor::Current() == GameCursor::Kind::Count)
        GameCursor::Set(GameCursor::Kind::Normal);
}

void TestScene::GUIRender()
{
    Super::GUIRender();

    if (_navigationEditor)
    {
        _navigationEditor->RenderInspector();
        _navigationEditor->RenderEditorWindow();
        _navigationEditor->RenderOverlay();
    }
}

void TestScene::InitializeObject()
{
    _navigationEditor = make_shared<NavigationEditorTool>();
    _shader = make_shared<Shader>(L"23. RenderDemo.fx");

    {
        GameSounds::Init();
        HUDController::LoadAudioSettings();
        SOUND->LoadSound("BGM", "..\\Resources\\Musics\\Sr_BGM.mp3", true);
        SOUND->PlaySound("BGM", 0.5f);
    }

    // VFX
    AnnieQEffect::Get();
    AnnieWEffect::Get();
    PARTICLE->Add(L"ProjectileImpact", L"..\\Resources\\Particles\\Annie_QSpellImpact_V2.fx", 8);
    PARTICLE->Add(L"ProjectileTrailSparks", L"..\\Resources\\Particles\\Annie_QSpellTrailSparks_V3.fx", 16);
    PARTICLE->Add(L"ProjectileExplosion", L"..\\Resources\\Particles\\Annie_QSpellExplosion_V4.fx", 8);

    // Main Camera
    {
        auto camera = make_shared<GameObject>("Main_Camera");

        camera->GetOrAddTransform()->SetPosition(Vec3{0.f, 0.f, -5.f});
        camera->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));

        camera->AddComponent(make_shared<Camera>());
        camera->GetCamera()->SetProjectionType(ProjectionType::Perspective);
        camera->GetCamera()->SetCullingMaskLayerOnOff(Layer_UI, true);

        camera->AddComponent(make_shared<CameraController>());
        CUR_SCENE->Add(camera);
        camera->LoadTrasnformData();
    }

    // UI_Camera
    {
        auto camera = make_shared<GameObject>("UI_Camera");
        camera->GetOrAddTransform()->SetPosition(Vec3{0.f, 0.f, -5.f});
        camera->AddComponent(make_shared<Camera>());
        camera->GetCamera()->SetProjectionType(ProjectionType::Orthographic);
        camera->GetCamera()->SetNear(1.f);
        camera->GetCamera()->SetFar(100.f);

        camera->GetCamera()->SetCullingMaskAll();
        camera->GetCamera()->SetCullingMaskLayerOnOff(Layer_UI, false);
        CUR_SCENE->Add(camera);
    }

    // Light
    {
        auto light = make_shared<GameObject>("Light");
        light->AddComponent(make_shared<Light>());
        LightDesc lightDesc;
        lightDesc.ambient = Vec4(0.4f);
        lightDesc.diffuse = Vec4(1.f);
        lightDesc.specular = Vec4(0.1f);
        lightDesc.direction = Vec3(1.f, 0.f, 1.f);
        light->GetLight()->SetLightDesc(lightDesc);
        Add(light);
    }

    // Material
    {{shared_ptr<Material> material = make_shared<Material>();
    material->SetShader(_shader);
    auto texture = RESOURCES->Load<Texture>(L"Veigar", L"..\\Resources\\Textures\\veigar.jpg");
    material->SetDiffuseMap(texture);
    MaterialDesc& desc = material->GetMaterialDesc();
    desc.ambient = Vec4(1.f);
    desc.diffuse = Vec4(1.f);
    desc.specular = Vec4(1.f);
    RESOURCES->Add(L"Veigar", material);
}

{
    shared_ptr<Material> material = make_shared<Material>();
    material->SetShader(_shader);
    auto texture =
        RESOURCES->Load<Texture>(L"Projectile", L"..\\Resources\\Textures\\Annie\\Particles\\V2\\fireball-core-v2.dds");
    material->SetDiffuseMap(texture);
    MaterialDesc& desc = material->GetMaterialDesc();
    desc.ambient = Vec4(1.f);
    desc.diffuse = Vec4(1.f);
    desc.specular = Vec4(1.f);
    RESOURCES->Add(L"Projectile", material);
}

{
    shared_ptr<Material> material = make_shared<Material>();
    material->SetShader(_shader);
    auto texture =
        RESOURCES->Load<Texture>(L"Trail", L"..\\Resources\\Textures\\Annie\\Particles\\V2\\fireball-trail-v2.dds");
    material->SetDiffuseMap(texture);
    MaterialDesc& desc = material->GetMaterialDesc();
    desc.ambient = Vec4(1.f);
    desc.diffuse = Vec4(1.f);
    desc.specular = Vec4(1.f);
    RESOURCES->Add(L"Trail", material);
}
}

{
    auto indicator = make_shared<GameObject>("AnnieSkillRangeIndicator");
    indicator->GetOrAddTransform();
    indicator->GetOrAddScript<SkillIndicatorController>();
    UI->SetSkillIndicatorControllerGameObject(indicator);
    CUR_SCENE->Add(indicator);
}

{
}

{
    shared_ptr<class Model> m2 = make_shared<Model>();
    m2->ReadModel(L"Rift/Rift");
    m2->ReadMaterial(L"Rift/Rift");

    auto obj = make_shared<GameObject>("Rift");
    obj->GetOrAddTransform();

    obj->AddComponent(make_shared<ModelRenderer>(_shader));
    obj->GetModelRenderer()->SetModel(m2);
    obj->GetModelRenderer()->SetPass(19);

    Add(obj);
    FogOfWar::Get();
    GroundHeight::Get();
}

// Terrain
{
    auto obj = make_shared<GameObject>("Terrain");
    obj->AddComponent(make_shared<Terrain>());
    obj->GetOrAddTransform()->SetLocalPosition(Vec3(0.f, 2.f, 0.f));
    obj->GetOrAddTransform()->SetRotation(Vec3(0.f, 0.f, 0.f));

    obj->GetTerrain()->Create(145, 145, RESOURCES->Get<Material>(L"Veigar"));
    obj->GetMeshRenderer()->SetPass(3);

    auto navigation = make_shared<NavigationGridComponent>();
    obj->AddComponent(navigation);
    std::string navigationError;
    navigation->LoadSettings("../Resources/Navigation/room-0-nav-v1.navgrid.component.json", navigationError);
    navigationError.clear();

    if (!navigation->LoadNavGrid(navigation->GetAssetPath(), navigationError))
    {
        navigationError.clear();
        navigation->CreateGrid(145, 145, 1.0f, Vec3::Zero, navigationError);
        navigation->SetLastStatus("Room 0 asset was not found; created an unsaved all-Walkable editor grid. " +
                                  navigationError);
    }

    _navigationEditor->SetTarget(navigation);
    _terrain = obj;
    CUR_SCENE->Add(obj);
}

#pragma region LegacyCode(Katsujin)

#pragma endregion
}
