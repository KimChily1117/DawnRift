#include "pch.h"
#include "ModelRenderer.h"
#include "Material.h"
#include "ModelMesh.h"
#include "Model.h"
#include "InstancingBuffer.h"
#include "Camera.h"
#include "Light.h"

ModelRenderer::ModelRenderer(shared_ptr<Shader> shader) : Super(ComponentType::ModelRenderer), _shader(shader) {}

ModelRenderer::~ModelRenderer() {}

void ModelRenderer::SetModel(shared_ptr<Model> model)
{
    _model = model;

    const auto& materials = _model->GetMaterials();

    for (auto& material : materials)
    {
        material->SetShader(_shader);
    }
}

bool ModelRenderer::PickMesh(int32 screenX, int32 screenY, Vec3& pickPos, float& distance)
{
    if (!_model)
        return false;

    Matrix W = Matrix::Identity;
    Matrix V = Camera::S_MatView;
    Matrix P = Camera::S_MatProjection;

    Viewport& vp = GRAPHICS->GetViewport();

    // Unproject를 이용해 스크린 좌표를 월드 공간으로 변환
    Vec3 start = vp.Unproject(Vec3(screenX, screenY, 0), W, V, P);
    Vec3 end = vp.Unproject(Vec3(screenX, screenY, 1), W, V, P);

    // Ray 생성
    Vec3 rayDir = end - start;
    rayDir.Normalize();
    Ray ray(start, rayDir);

    // Step 2: 모델의 메쉬 가져오기
    const auto& meshes = _model->GetMeshes();
    float minDistance = FLT_MAX;
    bool hit = false;

    // Step 3: 모든 삼각형 검사
    for (const auto& mesh : meshes)
    {
        const auto& vertices = mesh->geometry->GetVertices();
        const auto& indices = mesh->geometry->GetIndices();

        for (size_t i = 0; i < indices.size(); i += 3)
        {
            Vec3 v0 = vertices[indices[i + 0]].position;
            Vec3 v1 = vertices[indices[i + 1]].position;
            Vec3 v2 = vertices[indices[i + 2]].position;

            float dist;

            if (ray.Intersects(v0, v1, v2, OUT dist))
            {
                if (dist < minDistance)
                {
                    minDistance = dist;
                    hit = true;
                    pickPos = ray.position + ray.direction * minDistance;
                }
            }
        }
    }

    // 최종적으로 픽킹이 되었는지 확인
    if (hit)
    {
        distance = minDistance;

        return true;
    }

    return false;
}

void ModelRenderer::RenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
    if (_model == nullptr)
        return;

    // GlobalData
    _shader->PushGlobalData(Camera::S_MatView, Camera::S_MatProjection);

    // Light
    auto lightObj = SCENE->GetCurrentScene()->GetLight();

    if (lightObj)
        _shader->PushLightData(lightObj->GetLight()->GetLightDesc());

    // Bones
    BoneDesc boneDesc;

    const uint32 boneCount = _model->GetBoneCount();

    for (uint32 i = 0; i < boneCount; i++)
    {
        shared_ptr<ModelBone> bone = _model->GetBoneByIndex(i);
        boneDesc.transforms[i] = bone->transform;
    }

    _shader->PushBoneData(boneDesc);

    const bool cull = buffer->GetCount() == 1 && Camera::S_MatProjection._44 == 0.f;
    BoundingFrustum frustum;
    Matrix world;

    if (cull)
    {
        BoundingFrustum::CreateFromMatrix(frustum, Camera::S_MatProjection);
        frustum.Transform(frustum, Camera::S_MatView.Invert());
        world = GetTransform()->GetWorldMatrix();
    }

    uint32 drawn = 0;

    const auto& meshes = _model->GetMeshes();

    for (auto& mesh : meshes)
    {
        if (cull && mesh->bone)
        {
            BoundingBox bounds;
            mesh->bounds.Transform(bounds, mesh->bone->transform * world);
            constexpr float MapCullMargin = 4.f;
            bounds.Extents = Vec3(bounds.Extents) * 1.5f + Vec3(MapCullMargin);

            if (!frustum.Intersects(bounds))
                continue;
        }

        ++drawn;

        if (mesh->material)
            mesh->material->Update();

        // BoneIndex
        _shader->GetScalar("BoneIndex")->SetInt(mesh->boneIndex);

        mesh->vertexBuffer->PushData();
        mesh->indexBuffer->PushData();

        buffer->PushData();
        DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        _shader->DrawIndexedInstanced(0, _pass, mesh->indexBuffer->GetCount(), buffer->GetCount());
    }

    if (cull && meshes.size() > 50)
        Profiler::Gauge("map submeshes drawn", static_cast<double>(drawn));
}

InstanceID ModelRenderer::GetInstanceID()
{
    return make_pair((uint64)_model.get(), (uint64)_shader.get());
}
