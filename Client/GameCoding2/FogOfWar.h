#pragma once
#include "MonoBehaviour.h"

class FogOfWar : public MonoBehaviour
{
public:
    static shared_ptr<FogOfWar> Get();
    void LateUpdate() override;

    void SetHidden(uint64 objectId, bool hidden, const Vec3& position);

    bool IsHidden(uint64 objectId) const
    {
        return _hiddenScale.count(objectId) != 0;
    }

    void AddTemporarySight(const Vec3& position, float radius, float seconds);

    float SightAt(float x, float z) const;

    bool BrushAt(float x, float z) const;

    bool IsLoaded() const
    {
        return _width > 0;
    }

    static void SetEnabled(bool enabled)
    {
        s_enabled = enabled;
    }

    static bool IsEnabled()
    {
        return s_enabled;
    }

    static constexpr float SightRange = 13.5f;
    static constexpr float WardSightRange = 9.f;
    static constexpr float NearsightRange = 2.5f;

private:
    bool Load();
    void CreateTexture();
    void ComputeSight();

    struct TemporarySight
    {
        Vec3 position;
        float radius;
        float until;
    };

    vector<TemporarySight> _temporarySight;
    void ComputeStamp(int32 cx, int32 cz, float sightRange, vector<uint32>& cells) const;
    bool ClearLine(int32 x, int32 z, int32 tx, int32 tz) const;
    void Upload();
    void Bind();

    int32 _width = 0, _height = 0;
    float _cellSize = 0.5f, _originX = 0.f, _originZ = 0.f;
    vector<uint8> _cells;
    vector<uint8> _target;
    vector<float> _shown;
    vector<uint8> _pixels;
    float _computeTimer = 0.f;
    float _fadeTimer = 1.f;

    struct SightStamp
    {
        int32 cx = INT32_MIN, cz = INT32_MIN;
        vector<uint32> cells;
        bool nearsighted = false;
    };

    unordered_map<uint64, SightStamp> _stamps;
    ComPtr<ID3D11Texture2D> _texture;
    ComPtr<ID3D11ShaderResourceView> _srv;
    unordered_map<uint64, Vec3> _hiddenScale;
    static inline bool s_enabled = true;
    weak_ptr<Scene> _scene;
};
