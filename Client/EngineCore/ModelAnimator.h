// 스키닝 애니메이션 재생.
#pragma once
#include "Component.h"
#include "ModelAnimation.h"
#include "Model.h"

class Model;
class InstancingBuffer;

struct AnimationTextureData
{
    uint32 clipCount = 0;
    uint32 frameCount = 0;
    uint32 boneCount = 0;
    vector<Matrix> transforms;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;

    const Matrix& At(uint32 clip, uint32 frame, uint32 bone) const
    {
        return transforms[(static_cast<size_t>(clip) * frameCount + frame) * boneCount + bone];
    }
};

class ModelAnimator : public Component
{
    using Super = Component;

public:
    ModelAnimator(shared_ptr<Shader> shader);
    ~ModelAnimator();

    void SetModel(shared_ptr<Model> model);

    shared_ptr<Model> GetModel() const
    {
        return _model;
    }

    void SetNeverCulled(bool neverCulled)
    {
        _neverCulled = neverCulled;
    }

    bool IsNeverCulled() const
    {
        return _neverCulled;
    }

    const AnimationTextureData* GetAnimationTextureData() const
    {
        return _animation.get();
    }

    uint8 GetPass() const
    {
        return _pass;
    }

    void SetPass(uint8 pass)
    {
        _pass = pass;
    }

    virtual void Update() override;

    void UpdateTweenData();
    void RenderInstancing(shared_ptr<class InstancingBuffer>& buffer);
    InstanceID GetInstanceID();

    TweenDesc& GetTweenDesc()
    {
        return _tweenDesc;
    }

    shared_ptr<Shader> GetShader()
    {
        return _shader;
    }

public:
    void BlendToAnimation(int animIndex, float blendTime);
    float GetAnimationDuration(int animIndex);

    uint32 GetAnimationCount() const
    {
        return _model ? _model->GetAnimationCount() : 0;
    }

    void SetAnimation(int32 animIndex, bool loop);

    bool GetBoneWorldPosition(const wstring& boneName, Vec3& position);

private:
    void CreateTexture();
    static void CreateAnimationTransform(Model& model, AnimationTextureData& data, uint32 index);

private:
    function<void()> _animationEndCallback;

public:
public:
    void SetAnimationEndCallback(function<void()> callback)
    {
        _animationEndCallback = callback;
    }

    bool IsAnimationFinished()
    {
        shared_ptr<ModelAnimation> currentAnim = _model->GetAnimationByIndex(_tweenDesc.curr.animIndex);

        if (!currentAnim)
            return false;

        // ✅ 현재 프레임이 마지막 프레임인지 확인
        return _tweenDesc.curr.currFrame == currentAnim->frameCount - 1 &&
               _tweenDesc.curr.sumTime >= (1.0f / (currentAnim->frameRate * _tweenDesc.curr.speed));
    }

private:
    shared_ptr<AnimationTextureData> _animation;
    D3D11_PRIMITIVE_TOPOLOGY type = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

private:
    KeyframeDesc _keyframeDesc;
    TweenDesc _tweenDesc;

private:
    shared_ptr<Shader> _shader;
    uint8 _pass = 0;
    shared_ptr<Model> _model;
    bool _neverCulled = false;
};
