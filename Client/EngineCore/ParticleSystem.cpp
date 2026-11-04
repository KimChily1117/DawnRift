#include "pch.h"
#include "ParticleSystem.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "BinaryReader.h"
#include "FileUtils.h"
#include "Camera.h"

namespace
{
constexpr UINT MaxSupportedParticleCount = 4096;

bool IsValidParticleCount(UINT count)
{
    return count > 0 && count <= MaxSupportedParticleCount;
}
}

ParticleSystem::ParticleSystem(wstring file)
{
    LoadData(file);
    instancingBuffer = make_shared<InstancingBuffer>();

    blendState[0] = new BlendState();
    blendState[1] = new BlendState();
    blendState[1]->Alpha(true);

    depthState[0] = new DepthStencilState();
    depthState[1] = new DepthStencilState();
    depthState[1]->DepthWriteMask(D3D11_DEPTH_WRITE_MASK_ZERO);
}

ParticleSystem::~ParticleSystem()
{
    delete blendState[0];
    delete blendState[1];

    delete depthState[0];
    delete depthState[1];
}

void ParticleSystem::Update()
{
    instancingBuffer->ClearData();

    lifeTime += DT;
    UpdatePhysical();
    UpdateColor();
    // quad�� ���� Ʈ������ ������Ʈ ����
    if (lifeTime > data.duration)
    {
        if (data.isLoop)

            Stop();
        else
            Stop();
    }
}

void ParticleSystem::Render()
{
    TransformDesc desc;

    desc.W = _quad->GetOrAddTransform()->GetWorldMatrix();
    _quad->GetMeshRenderer()->GetShader()->PushTransformData(desc);

    _quad->GetMeshRenderer()->GetMesh()->GetVertexBuffer()->PushData();
    // IdxBUffer
    _quad->GetMeshRenderer()->GetMesh()->GetIndexBuffer()->PushData();

    DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // InstanceBuffer

    instancingBuffer->PushData();

    blendState[1]->SetState();
    depthState[1]->SetState();

    _quad->GetMeshRenderer()->GetShader()->DrawIndexedInstanced(
        0, 12, _quad->GetMeshRenderer()->GetMesh()->GetIndexBuffer()->GetCount(), drawCount);

    blendState[0]->SetState();
    depthState[0]->SetState();
}

void ParticleSystem::Play(Vec3 pos, Vec3 rot)
{
    initialized = false;
    isActive = true;

    _quad->GetOrAddTransform()->SetPosition(pos);
    _quad->GetOrAddTransform()->SetRotation(rot);

    Init();
}

void ParticleSystem::Stop()
{
    isActive = false;
}

void ParticleSystem::UpdatePhysical()
{
    // ���̴� ���ڵ鿡���� Ʈ������ ������Ʈ

    drawCount = 0;

    for (int i = 0; i < data.count; i++)
    {
        if (particleInfos[i].startTime > lifeTime)
            continue;

        particleInfos[i].velocity += particleInfos[i].accelation * DT;
        particleInfos[i].transform._localPosition += particleInfos[i].velocity * particleInfos[i].speed * DT;
        particleInfos[i].transform._localRotation.z += particleInfos[i].angularVelocity * DT;

        if (data.isBillboard)
        {
            particleInfos[i].transform._localRotation.x = CUR_SCENE->GetMainCamera()->GetTransform()->_localRotation.x;
            particleInfos[i].transform._localRotation.y = CUR_SCENE->GetMainCamera()->GetTransform()->_localRotation.y;
        }

        float t = (lifeTime - particleInfos[i].startTime) / (data.duration - particleInfos[i].startTime);

        particleInfos[i].transform._localScale.x =
            std::lerp(particleInfos[i].startScale.x, particleInfos[i].endScale.x, t);
        particleInfos[i].transform._localScale.y =
            std::lerp(particleInfos[i].startScale.y, particleInfos[i].endScale.y, t);
        particleInfos[i].transform._localScale.z =
            std::lerp(particleInfos[i].startScale.z, particleInfos[i].endScale.z, t);

        particleInfos[i].transform.UpdateTransform();
        instances[drawCount].world = particleInfos[i].transform.GetWorldMatrix();
        instancingBuffer->AddData(instances[drawCount]);
        drawCount++;
    }

    // ���̴� ���ڵ鿡���� Ʈ������ ������Ʈ
}

void ParticleSystem::UpdateColor()
{
    // Mat desc ����ü ���� Update ��

    float t = lifeTime / data.duration;

    _quad->GetMeshRenderer()->GetMaterial()->GetMaterialDesc().diffuse.x =
        std::lerp(data.startColor.x, data.endColor.x, t);
    _quad->GetMeshRenderer()->GetMaterial()->GetMaterialDesc().diffuse.y =
        std::lerp(data.startColor.y, data.endColor.y, t);
    _quad->GetMeshRenderer()->GetMaterial()->GetMaterialDesc().diffuse.z =
        std::lerp(data.startColor.z, data.endColor.z, t);
    _quad->GetMeshRenderer()->GetMaterial()->GetMaterialDesc().diffuse.w =
        std::lerp(data.startColor.w, data.endColor.w, t);

    _quad->GetMeshRenderer()->GetMaterial()->Update();
}

void ParticleSystem::Init()
{
    // _quad�� ���� �޸� �Ҵ� �� Component ���� �ʿ���

    if (data.isAdditive)
        blendState[1]->Additive();
    else
        blendState[1]->Alpha(true);

    lifeTime = 0.0f;
    drawCount = 0;

    // Particle ���� ��ġ
    if (_quad->GetTransform() == nullptr)
        return;
    Vec3 originPos = _quad->GetTransform()->GetPosition();

    if (initialized == false)
    {
        _originPos = originPos;
        initialized = true;
    }

    DEBUG_LOG("????" << _originPos.x << " , " << _originPos.y << " , " << _originPos.z);

    Vec3 rotation = _quad->GetTransform()->GetRotation();
    Matrix rotMatrix = XMMatrixRotationY(rotation.y);

    for (ParticleInfo& info : particleInfos)
    {
        info.transform.SetPosition(_originPos);

        Vec3 baseVel = Utils::Random(data.minVelocity, data.maxVelocity);
        XMStoreFloat3(&info.velocity, XMVector3Transform(XMLoadFloat3(&baseVel), rotMatrix));
        info.velocity.Normalize();

        info.accelation = Utils::Random(data.minAccelation, data.maxAccelation);
        info.angularVelocity = Utils::Random(data.minAngularVelocity, data.maxAngularVelocity);
        info.speed = Utils::Random(data.minSpeed, data.maxSpeed);
        info.startTime = Utils::Random(data.minStartTime, data.maxStartTime);
        info.startScale = Utils::Random(data.minStartScale, data.maxStartScale);
        info.endScale = Utils::Random(data.minEndScale, data.maxEndScale);
        info.velocity.Normalize();
    }
}

void ParticleSystem::LoadData(wstring file)
{
    BinaryReader* reader = new BinaryReader(file);

    wstring textureFile = reader->WString();
    {
        _quad = make_shared<GameObject>();
        _quad->AddComponent(make_shared<MeshRenderer>());

        // Setting Quad Mesh
        auto mesh = RESOURCES->Get<Mesh>(L"Quad");
        _quad->GetMeshRenderer()->SetMesh(mesh);

        // �ʿ��ϴٸ� Quad mesh�̿��� shader pass�� ���⼭ Setting
        _quad->GetMeshRenderer()->SetPass(12);

        // ���⼭ �ϴ�����? mat�̳� �̷��κе��� ���⼭ �����ؼ� tex�� �־������
        {
            // Setting Matrial
            shared_ptr<Material> material = make_shared<Material>();

            // �ϴ��� ������� ����ϴ� General Shader �����
            material->SetShader(CUR_SCENE->_shader);

            auto texture = RESOURCES->Load<Texture>(L"TEX_" + Utils::GetFileNameWithoutExtension(textureFile),
                                                    L"..\\" + textureFile);

            material->SetDiffuseMap(texture);
            MaterialDesc& desc = material->GetMaterialDesc();
            desc.ambient = Vec4(1.f);
            desc.diffuse = Vec4(1.f);
            desc.specular = Vec4(1.f);
            RESOURCES->Add(L"MAT_" + Utils::GetFileNameWithoutExtension(textureFile), material);
            _quad->GetMeshRenderer()->SetMaterial(material);
        }
    }

    ParticleData particleData = {};

    particleData.isLoop = reader->Bool();
    particleData.isAdditive = reader->Bool();
    particleData.isBillboard = reader->Bool();

    particleData.count = reader->UInt();

    if (!IsValidParticleCount(particleData.count))
    {
        uint8 legacyCountHighByte = 0;
        reader->Byte(&legacyCountHighByte, sizeof(legacyCountHighByte));

        const UINT alignedCount = (particleData.count >> 8) | (static_cast<UINT>(legacyCountHighByte) << 24);

        if (IsValidParticleCount(alignedCount))
        {
            particleData.count = alignedCount;
        }
        else
        {
            DEBUG_LOG(L"[Particle] Invalid particle count in " << file << L": raw=" << particleData.count
                                                               << L", aligned=" << alignedCount);
            particleData.count = 1;
        }
    }

    particleData.duration = reader->Float();

    particleData.minVelocity.x = reader->Float();
    particleData.minVelocity.y = reader->Float();
    particleData.minVelocity.z = reader->Float();

    particleData.maxVelocity.x = reader->Float();
    particleData.maxVelocity.y = reader->Float();
    particleData.maxVelocity.z = reader->Float();

    particleData.minAccelation.x = reader->Float();
    particleData.minAccelation.y = reader->Float();
    particleData.minAccelation.z = reader->Float();

    particleData.maxAccelation.x = reader->Float();
    particleData.maxAccelation.y = reader->Float();
    particleData.maxAccelation.z = reader->Float();

    particleData.minStartScale.x = reader->Float();
    particleData.minStartScale.y = reader->Float();
    particleData.minStartScale.z = reader->Float();

    particleData.maxStartScale.x = reader->Float();
    particleData.maxStartScale.y = reader->Float();
    particleData.maxStartScale.z = reader->Float();

    particleData.minEndScale.x = reader->Float();
    particleData.minEndScale.y = reader->Float();
    particleData.minEndScale.z = reader->Float();

    particleData.maxEndScale.x = reader->Float();
    particleData.maxEndScale.y = reader->Float();
    particleData.maxEndScale.z = reader->Float();

    particleData.minSpeed = reader->Float();
    particleData.maxSpeed = reader->Float();
    particleData.minAngularVelocity = reader->Float();
    particleData.maxAngularVelocity = reader->Float();
    particleData.minStartTime = reader->Float();
    particleData.maxStartTime = reader->Float();

    particleData.startColor.x = reader->Float();
    particleData.startColor.y = reader->Float();
    particleData.startColor.z = reader->Float();
    particleData.startColor.w = reader->Float();

    particleData.endColor.x = reader->Float();
    particleData.endColor.y = reader->Float();
    particleData.endColor.z = reader->Float();
    particleData.endColor.w = reader->Float();

    // ����ü ����
    data = particleData;

    // ��ƼŬ ���� �ʱ�ȭ
    instances.resize(data.count);
    particleInfos.resize(data.count);

    delete reader;
}
