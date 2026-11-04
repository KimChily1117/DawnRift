#pragma once

class TimeManager
{
    DECLARE_SINGLE(TimeManager);

public:
    void Init();
    void Update();

    float GetDeltaTime() const
    {
        return _deltaTime;
    }

    float GetGameTime() const
    {
        return _gameTime;
    }

    uint32 GetFps() const
    {
        return _fps;
    }

    void SetFrozen(bool frozen)
    {
        _frozen = frozen;
    }

    bool IsFrozen() const
    {
        return _frozen;
    }

    float GetRealDeltaTime() const
    {
        return _realDeltaTime;
    }

private:
    uint64 _frequency = 0;
    uint64 _prevCount = 0;
    float _deltaTime = 0.f;
    float _gameTime = 0.f;
    float _realDeltaTime = 0.f;
    bool _frozen = false;

    uint32 _frameCount = 0;
    float _frameTime = 0.f;
    uint32 _fps = 0;
};
