#pragma once
#include "MonoBehaviour.h"

class Shader;

class ClickEffectController : public MonoBehaviour
{
public:
    void Update() override;

    void Play(const Vec3& position, shared_ptr<GameObject> follow, float startScale);
    void SetPosition(const Vec3& position);
    void Hide();

    bool IsPlaying() const
    {
        return _playing;
    }

private:
    bool _playing = false;
    float _elapsed = 0.f;
    float _startScale = 0.f;
    weak_ptr<GameObject> _follow;
};

class ClickEffectPool
{
public:
    enum class Kind
    {
        Move,
        Attack
    };

    void Init(shared_ptr<Shader> shader);
    void PlayAt(Kind kind, const Vec3& position);
    void PlayOnTarget(shared_ptr<GameObject> target);

    void MoveLastMoveEffect(const Vec3& position);

private:
    shared_ptr<ClickEffectController> Acquire(Kind kind);

    vector<shared_ptr<ClickEffectController>> _moveEffects;
    vector<shared_ptr<ClickEffectController>> _attackEffects;
    size_t _nextMove = 0;
    size_t _nextAttack = 0;
    weak_ptr<ClickEffectController> _lastMoveEffect;
};
