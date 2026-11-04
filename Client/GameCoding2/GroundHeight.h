#pragma once
#include "MonoBehaviour.h"

class GroundHeight : public MonoBehaviour
{
public:
    static shared_ptr<GroundHeight> Get();

    static bool IsActive();
    // Bilinear height at a world position (clamped to the grid).
    static float At(float x, float z);
    void LateUpdate() override;

    static constexpr float FlatHeight = 2.f;

private:
    static bool Load();
};
