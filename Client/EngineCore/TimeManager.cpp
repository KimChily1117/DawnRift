#include "pch.h"
#include "TimeManager.h"

void TimeManager::Init()
{
    ::QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&_frequency));
    ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&_prevCount));

    _gameTime = 0.0f;
}

void TimeManager::Update()
{
    uint64 currentCount;
    ::QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&currentCount));

    _realDeltaTime = (currentCount - _prevCount) / static_cast<float>(_frequency);
    _prevCount = currentCount;
    _deltaTime = _frozen ? 0.f : _realDeltaTime;

    _gameTime += _deltaTime;

    _frameCount++;
    _frameTime += _realDeltaTime;

    if (_frameTime > 1.f)
    {
        _fps = static_cast<uint32>(_frameCount / _frameTime);
        _frameTime = 0.f;
        _frameCount = 0;
    }
}
