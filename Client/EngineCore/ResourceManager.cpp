#include "pch.h"
#include "ResourceManager.h"
#include "Texture.h"
#include "Shader.h"
#include "Mesh.h"
#include "Material.h"
#include "BinaryReader.h"
#include "BindShaderDesc.h"

ResourceManager::~ResourceManager()
{
    for (auto& resourceMap : _resources)
    {
        resourceMap.clear();
    }
}

void ResourceManager::Init()
{
    CreateDefaultMesh();
}

std::shared_ptr<Texture> ResourceManager::GetOrAddTexture(const wstring& key, const wstring& path)
{
    shared_ptr<Texture> texture = Get<Texture>(key);

    if (filesystem::exists(filesystem::path(path)) == false)
        return nullptr;

    texture = Load<Texture>(key, path);

    if (texture == nullptr)
    {
        texture = make_shared<Texture>();
        texture->Load(path);
        Add(key, texture);
    }

    return texture;
}

std::shared_ptr<ParticleData> ResourceManager::GetEffectParticle(const wstring& key) const
{
    auto findIt = _effectParticles.find(key);

    if (findIt != _effectParticles.end())
        return findIt->second;

    return nullptr;
}

void ResourceManager::CreateDefaultMesh()
{
    {
        shared_ptr<Mesh> mesh = make_shared<Mesh>();
        mesh->CreateQuad();
        Add(L"Quad", mesh);
    }

    {
        shared_ptr<Mesh> mesh = make_shared<Mesh>();
        mesh->CreateCube();
        Add(L"Cube", mesh);
    }

    {
        shared_ptr<Mesh> mesh = make_shared<Mesh>();
        mesh->CreateSphere();
        Add(L"Sphere", mesh);
    }

    {
        shared_ptr<Mesh> mesh = make_shared<Mesh>();
        mesh->CreateParticleQuad();
        Add(L"ParticleQuad", mesh);
    }
}

void ResourceManager::CreateParticle(const wstring key, const wstring& path)
{
    if (_effectParticles.contains(key))
        return;
    BinaryReader reader(path);

    // texture path + data
    wstring texFile = reader.WString();

    // Parse ParticleData
    shared_ptr<ParticleData> data = make_shared<ParticleData>();

    reader.Byte(&(*data), sizeof(ParticleData));

    // Create mat
    auto material = make_shared<Material>();
    material->SetShader(CUR_SCENE->_shader);
    auto texture = Load<Texture>(key, L"..\\" + texFile);
    material->SetDiffuseMap(texture);
    MaterialDesc& desc = material->GetMaterialDesc();
    desc.ambient = Vec4(1.f);
    desc.diffuse = Vec4(1.f);
    desc.specular = Vec4(1.f);
    RESOURCES->Add(key, material);

    // key pair로 필요한 파티클 정보를 받아오기 위해서?
    _effectParticles[key] = data;
}
