#pragma once
#include "MonoBehaviour.h"

class CursorController : public MonoBehaviour
{
public:
    void Awake() override;
    void Update() override;

private:
    float _elapsedTime = 0.f;

    float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }
};
