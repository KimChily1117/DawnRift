// 인스턴싱 드로우 묶음 관리.
#pragma once
#include "InstancingBuffer.h"

class GameObject;

class InstancingManager
{
    DECLARE_SINGLE(InstancingManager);

public:
    void Render(vector<shared_ptr<GameObject>>& gameObjects);

    void Clear()
    {
        _buffers.clear();
    }

    void ClearData();

    const vector<const GameObject*>& GetLastCulled() const
    {
        return _lastCulled;
    }

    // Instances the last perspective pass drew, by model (tests).
    uint32 GetLastDrawn(const void* model) const
    {
        auto it = _lastDrawn.find(model);

        return it == _lastDrawn.end() ? 0 : it->second;
    }

private:
    void RenderMeshRenderer(vector<shared_ptr<GameObject>>& gameObjects, bool overlayOnly);
    void RenderModelRenderer(vector<shared_ptr<GameObject>>& gameObjects);
    void RenderAnimRenderer(vector<shared_ptr<GameObject>>& gameObjects);
    ;
    void RenderParticleRenderer(vector<shared_ptr<GameObject>>& gameObjects);
    ;

private:
    void AddData(InstanceID instanceId, InstancingData& data);

private:
    map<InstanceID, shared_ptr<InstancingBuffer>> _buffers;
    vector<const GameObject*> _lastCulled;
    unordered_map<const void*, uint32> _lastDrawn;
};
