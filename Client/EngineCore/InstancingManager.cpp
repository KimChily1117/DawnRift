// 인스턴싱 드로우 묶음 관리.
#include "pch.h"
#include "InstancingManager.h"
#include "InstancingBuffer.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "ModelRenderer.h"
#include "ModelAnimator.h"
#include "ParticleRenderer.h"
#include "Transform.h"
#include "Camera.h"
#include "Model.h"
#include "ModelMesh.h"

namespace
{
void ResetPipelineState()
{
    DC->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    DC->OMSetDepthStencilState(nullptr, 0);
    DC->RSSetState(nullptr);
}
}

void InstancingManager::Render(vector<shared_ptr<GameObject>>& gameObjects)
{
    ClearData();
    ResetPipelineState();

    {
        PROFILE_SCOPE("  render meshes");
        RenderMeshRenderer(gameObjects, false);
    }

    {
        PROFILE_SCOPE("  render models");
        RenderModelRenderer(gameObjects);
    }

    ResetPipelineState();
    {
        PROFILE_SCOPE("  render animated");
        RenderAnimRenderer(gameObjects);
    }

    RenderParticleRenderer(gameObjects);
    {
        PROFILE_SCOPE("  render overlays");
        RenderMeshRenderer(gameObjects, true);
    }

    ResetPipelineState();
}

void InstancingManager::RenderMeshRenderer(vector<shared_ptr<GameObject>>& gameObjects, bool overlayOnly)
{
    map<pair<int32, InstanceID>, vector<shared_ptr<GameObject>>> cache;

    for (shared_ptr<GameObject>& gameObject : gameObjects)
    {
        auto renderer = gameObject->GetMeshRenderer();

        if (renderer == nullptr)
            continue;
        const bool isWorldOverlay = renderer->IsWorldOverlay();

        if (isWorldOverlay != overlayOnly)
            continue;

        const InstanceID instanceId = renderer->GetInstanceID();
        cache[{renderer->GetOverlayOrder(), instanceId}].push_back(gameObject);
    }

    for (auto& pair : cache)
    {
        const vector<shared_ptr<GameObject>>& vec = pair.second;
        const InstanceID instanceId = pair.first.second;

        for (int32 i = 0; i < vec.size(); i++)
        {
            const shared_ptr<GameObject>& gameObject = vec[i];
            InstancingData data;
            data.world = gameObject->GetTransform()->GetWorldMatrix();
            AddData(instanceId, data);
        }

        shared_ptr<InstancingBuffer>& buffer = _buffers[instanceId];
        vec[0]->GetMeshRenderer()->RenderInstancing(buffer);
    }
}

void InstancingManager::RenderModelRenderer(vector<shared_ptr<GameObject>>& gameObjects)
{
    map<InstanceID, vector<shared_ptr<GameObject>>> cache;

    for (shared_ptr<GameObject>& gameObject : gameObjects)
    {
        if (gameObject->GetModelRenderer() == nullptr)
            continue;

        const InstanceID instanceId = gameObject->GetModelRenderer()->GetInstanceID();
        cache[instanceId].push_back(gameObject);
    }

    for (auto& pair : cache)
    {
        const vector<shared_ptr<GameObject>>& vec = pair.second;

        {
            const InstanceID instanceId = pair.first;

            for (int32 i = 0; i < vec.size(); i++)
            {
                const shared_ptr<GameObject>& gameObject = vec[i];
                InstancingData data;
                data.world = gameObject->GetTransform()->GetWorldMatrix();
                AddData(instanceId, data);
            }

            shared_ptr<InstancingBuffer>& buffer = _buffers[instanceId];
            vec[0]->GetModelRenderer()->RenderInstancing(buffer);
        }
    }
}

void InstancingManager::RenderAnimRenderer(vector<shared_ptr<GameObject>>& gameObjects)
{
    map<InstanceID, vector<shared_ptr<GameObject>>> cache;

    for (shared_ptr<GameObject>& gameObject : gameObjects)
    {
        if (gameObject->GetModelAnimator() == nullptr)
            continue;

        const InstanceID instanceId = gameObject->GetModelAnimator()->GetInstanceID();
        cache[instanceId].push_back(gameObject);
    }

    const bool cull = Camera::S_MatProjection._44 == 0.f;
    BoundingFrustum frustum;

    if (cull)
    {
        BoundingFrustum::CreateFromMatrix(frustum, Camera::S_MatProjection);
        frustum.Transform(frustum, Camera::S_MatView.Invert());
    }

    constexpr float CullMargin = 4.f;
    static unordered_map<const Model*, BoundingSphere> modelBounds;
    auto boundsOf = [](const shared_ptr<Model>& model)
    {
        auto it = modelBounds.find(model.get());

        if (it != modelBounds.end())
            return it->second;
        BoundingBox box;
        bool first = true;

        for (const auto& mesh : model->GetMeshes())
        {
            if (first)
                box = mesh->bounds;
            else
                BoundingBox::CreateMerged(box, box, mesh->bounds);
            first = false;
        }

        BoundingSphere sphere;
        BoundingSphere::CreateFromBoundingBox(sphere, box);
        sphere.Radius = sphere.Radius * 3.f + CullMargin;

        return modelBounds[model.get()] = sphere;
    };
    uint32 culled = 0;

    if (cull)
    {
        _lastCulled.clear();
        _lastDrawn.clear();
    }

    static InstancedTweenDesc tweenDesc;

    for (auto& pair : cache)
    {
        const vector<shared_ptr<GameObject>>& vec = pair.second;
        const InstanceID instanceId = pair.first;
        uint32 count = 0;

        for (const shared_ptr<GameObject>& gameObject : vec)
        {
            auto animator = gameObject->GetModelAnimator();
            animator->UpdateTweenData();
            auto transform = gameObject->GetTransform();
            bool visible = transform->GetScale().LengthSquared() > 0.f;

            if (visible && cull && animator->GetModel() && !animator->IsNeverCulled())
            {
                BoundingSphere world;
                boundsOf(animator->GetModel()).Transform(world, transform->GetWorldMatrix());
                visible = frustum.Intersects(world);
            }

            if (!visible)
            {
                ++culled;

                if (cull)
                    _lastCulled.push_back(gameObject.get());
                continue;
            }

            if (count >= MAX_MODEL_INSTANCE)
                break;
            InstancingData data;
            data.world = transform->GetWorldMatrix();
            AddData(instanceId, data);
            tweenDesc.tweens[count++] = GpuTweenDesc(animator->GetTweenDesc());
        }

        if (cull)
            _lastDrawn[vec[0]->GetModelAnimator()->GetModel().get()] += count;

        if (count == 0)
            continue;

        vec[0]->GetModelAnimator()->GetShader()->PushTweenData(tweenDesc);
        shared_ptr<InstancingBuffer>& buffer = _buffers[instanceId];
        vec[0]->GetModelAnimator()->RenderInstancing(buffer);
    }

    Profiler::Gauge("animated culled", culled);
}

void InstancingManager::RenderParticleRenderer(vector<shared_ptr<GameObject>>& gameObjects) {}

void InstancingManager::AddData(InstanceID instanceId, InstancingData& data)
{
    if (_buffers.find(instanceId) == _buffers.end())
        _buffers[instanceId] = make_shared<InstancingBuffer>();

    _buffers[instanceId]->AddData(data);
}

void InstancingManager::ClearData()
{
    for (auto& pair : _buffers)
    {
        shared_ptr<InstancingBuffer>& buffer = pair.second;
        buffer->ClearData();
    }
}
