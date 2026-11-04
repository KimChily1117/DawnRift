#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"

class Mesh;
class Shader;

class HealthBars : public MonoBehaviour
{
public:
    static shared_ptr<HealthBars> Get();
    void LateUpdate() override;

    struct Bar
    {
        uint64 objectId = 0;
        float left = 0.f, top = 0.f, width = 0.f, height = 0.f;
        float health = 0.f, shield = 0.f;
        bool crowdControlled = false;
    };

    // Bars drawn last frame, for tests.
    const vector<Bar>& GetBars() const
    {
        return _bars;
    }

private:
    void Initialize();
    void Quad(float x0, float y0, float x1, float y1, const Vec4& color);

    shared_ptr<GameObject> _object;
    shared_ptr<Mesh> _mesh;
    vector<VertexTextureNormalTangentData> _vertices;
    size_t _used = 0;
    bool _visible = false;
    vector<Bar> _bars;

    unordered_map<uint64, float> _barHeights;
    weak_ptr<Scene> _scene;
};
