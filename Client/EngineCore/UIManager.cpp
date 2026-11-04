#include "pch.h"
#include "UIManager.h"
#include "Material.h"
#include "Texture.h"
#include "MeshRenderer.h"
#include "../GameCoding2/HUDController.h"
#include "../GameCoding2/SkillIndicatorController.h"
#include "../GameCoding2/CursorController.h"

void UIManager::Init()
{
    _shader = make_shared<Shader>(L"23. RenderDemo.fx");

    InitCursor();
    InitHUD();
    InitIndicator();
}

void UIManager::Update() {}

void UIManager::SetTarget(Protocol::PLAYER_CHAMPION_TYPE type)
{
    (void)type;
}

void UIManager::InitHUD()
{
    {
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture = RESOURCES->Load<Texture>(L"PlayerHUD", L"..\\Resources\\Textures\\UI\\HUD\\PlayerHUD.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"PlayerHUD", material);
        }

        // Material
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"PlayerHUD_Hp", L"..\\Resources\\Textures\\UI\\HUD\\PlayerHUD_Hp.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"PlayerHUD_Hp", material);
        }

        // Material
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"PlayerHUD_Mp", L"..\\Resources\\Textures\\UI\\HUD\\PlayerHUD_Mp.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"PlayerHUD_Mp", material);
        }

        // Material
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture = RESOURCES->Load<Texture>(L"empty_circle",
                                                    L"..\\Resources\\Textures\\UI\\HUD\\ChampMark\\empty_circle.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"empty_circle", material);
        }

        // Material
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture = RESOURCES->Load<Texture>(L"empty_square	",
                                                    L"..\\Resources\\Textures\\UI\\HUD\\ChampMark\\empty_square.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"empty_square", material);
        }

        // Material
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture = RESOURCES->Load<Texture>(L"annie_circle",
                                                    L"..\\Resources\\Textures\\UI\\HUD\\ChampMark\\annie_circle.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"annie_circle", material);
        }

        // Material
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture = RESOURCES->Load<Texture>(L"garen_circle",
                                                    L"..\\Resources\\Textures\\UI\\HUD\\ChampMark\\garen_circle.png");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"garen_circle", material);
        }

        // Annie
        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"annie_q", L"..\\Resources\\Textures\\UI\\Spell\\Annie\\annie_q.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"annie_q", material);
        }

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"annie_w", L"..\\Resources\\Textures\\UI\\Spell\\Annie\\annie_w.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"annie_w", material);
        }

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"annie_e", L"..\\Resources\\Textures\\UI\\Spell\\Annie\\annie_e.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"annie_e", material);
        }

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"annie_r", L"..\\Resources\\Textures\\UI\\Spell\\Annie\\annie_r.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"annie_r", material);
        }

        // garen

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"garen_q", L"..\\Resources\\Textures\\UI\\Spell\\Garen\\garen_q.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"garen_q", material);
        }

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"garen_w", L"..\\Resources\\Textures\\UI\\Spell\\Garen\\garen_w.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"garen_w", material);
        }

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"garen_e", L"..\\Resources\\Textures\\UI\\Spell\\Garen\\garen_e.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"garen_e", material);
        }

        {
            shared_ptr<Material> material = make_shared<Material>();
            material->SetShader(_shader);
            auto texture =
                RESOURCES->Load<Texture>(L"garen_r", L"..\\Resources\\Textures\\UI\\Spell\\Garen\\garen_r.dds");
            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"garen_r", material);
        }
    }
}

void UIManager::InitCursor()
{
    {
        shared_ptr<Material> material = make_shared<Material>();
        material->SetShader(_shader);
        auto texture =
            RESOURCES->Load<Texture>(L"hover_precise", L"..\\Resources\\Textures\\UI\\Cursor\\hover_precise.tga");
        material->SetDiffuseMap(texture);
        MaterialDesc& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f);
        desc.specular = Vec4(1.f);
        RESOURCES->Add(L"hover_precise", material);
    }

    {
        shared_ptr<Material> material = make_shared<Material>();
        material->SetShader(_shader);
        auto texture = RESOURCES->Load<Texture>(L"Cursor", L"..\\Resources\\Textures\\UI\\Cursor\\Cursor.png");
        material->SetDiffuseMap(texture);
        MaterialDesc& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f);
        desc.specular = Vec4(1.f);
        RESOURCES->Add(L"Cursor", material);
    }

    {
        shared_ptr<Material> material = make_shared<Material>();
        material->SetShader(_shader);
        auto texture =
            RESOURCES->Load<Texture>(L"singletarget", L"..\\Resources\\Textures\\UI\\Cursor\\singletarget.tga");
        material->SetDiffuseMap(texture);
        MaterialDesc& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f);
        desc.specular = Vec4(1.f);
        RESOURCES->Add(L"singletarget", material);
    }
}

void UIManager::InitIndicator()
{
    {
        shared_ptr<Material> material = make_shared<Material>();
        material->SetShader(_shader);
        auto texture = RESOURCES->Load<Texture>(L"aoe", L"..\\Resources\\Textures\\UI\\indicator\\aoe.dds");
        material->SetDiffuseMap(texture);
        MaterialDesc& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f);
        desc.specular = Vec4(1.f);
        RESOURCES->Add(L"aoe", material);
    }

    {
        shared_ptr<Material> material = make_shared<Material>();
        material->SetShader(_shader);
        auto texture = RESOURCES->Load<Texture>(L"conicrangeindicator",
                                                L"..\\Resources\\Textures\\UI\\indicator\\conicrangeindicator.dds");
        material->SetDiffuseMap(texture);
        MaterialDesc& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f, 1.f, 1.f, 0.55f);
        desc.specular = Vec4(1.f);
        RESOURCES->Add(L"conicrangeindicator", material);
    }

    {
        shared_ptr<Material> material = make_shared<Material>();
        material->SetShader(_shader);
        auto texture = RESOURCES->Load<Texture>(L"aoe_brand", L"..\\Resources\\Textures\\UI\\indicator\\aoe_brand.dds");
        material->SetDiffuseMap(texture);
        MaterialDesc& desc = material->GetMaterialDesc();
        desc.ambient = Vec4(1.f);
        desc.diffuse = Vec4(1.f);
        desc.specular = Vec4(1.f);
        RESOURCES->Add(L"aoe_brand", material);
    }
}
