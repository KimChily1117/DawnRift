// 스키닝 애니메이션 재생.
#include "pch.h"
#include "ModelAnimator.h"
#include "Material.h"
#include "ModelMesh.h"
#include "Model.h"
#include "ModelAnimation.h"
#include "InstancingBuffer.h"
#include "Camera.h"
#include "Light.h"
#include <ostream>

ModelAnimator::ModelAnimator(shared_ptr<Shader> shader) : Super(ComponentType::ModelAnimator), _shader(shader) {}

ModelAnimator::~ModelAnimator() {}

void ModelAnimator::SetModel(shared_ptr<Model> model)
{
    _model = model;

    const auto& materials = _model->GetMaterials();

    for (auto& material : materials)
    {
        material->SetShader(_shader);
    }
}

void ModelAnimator::Update()
{
    // Not Use
}

void ModelAnimator::BlendToAnimation(int animIndex, float blendTime)
{
    if (_tweenDesc.curr.animIndex == animIndex)
        return;

    _tweenDesc.next.animIndex = animIndex;
    _tweenDesc.tweenDuration = blendTime;
    _tweenDesc.tweenSumTime = 0.0f;
}

float ModelAnimator::GetAnimationDuration(int animIndex)
{
    auto animation = _model->GetAnimationByIndex(animIndex);

    if (animation)
        return animation->frameCount / animation->frameRate;

    return 1.0f;
}

void ModelAnimator::SetAnimation(int32 animIndex, bool loop)
{
    TweenDesc& desc = _tweenDesc;
    const uint32 requestedLoop = loop ? 1u : 0u;

    if (desc.curr.animIndex == animIndex && desc.curr.loop == requestedLoop)
        return;

    desc.curr.animIndex = animIndex;
    desc.curr.currFrame = 0;
    desc.curr.sumTime = 0;
    desc.curr.loop = requestedLoop;
}

void ModelAnimator::UpdateTweenData()
{
    TweenDesc& desc = _tweenDesc;
    desc.curr.sumTime += DT;

    // ✅ 현재 애니메이션 실행
    {
        shared_ptr<ModelAnimation> currentAnim = _model->GetAnimationByIndex(desc.curr.animIndex);

        if (currentAnim)
        {
            float timePerFrame = 1.0f / (currentAnim->frameRate * desc.curr.speed);

            if (desc.curr.sumTime >= timePerFrame)
            {
                desc.curr.sumTime = 0;
                int prevFrame = desc.curr.currFrame;
                desc.curr.currFrame++;

                // ✅ 애니메이션 종료 체크 (루프 여부 반영)
                if (desc.curr.currFrame >= currentAnim->frameCount)
                {
                    if (desc.curr.loop)
                    {
                        desc.curr.currFrame = 0;
                    }
                    else
                    {
                        desc.curr.currFrame = currentAnim->frameCount - 1;

                        if (_animationEndCallback)
                        {
                            _animationEndCallback();
                            _animationEndCallback = nullptr;
                        }
                    }
                }

                desc.curr.nextFrame = desc.curr.loop
                                          ? (desc.curr.currFrame + 1) % currentAnim->frameCount
                                          : min<uint32>(desc.curr.currFrame + 1, currentAnim->frameCount - 1);
            }

            desc.curr.ratio = (desc.curr.sumTime / timePerFrame);

            if (!desc.curr.loop && desc.curr.currFrame + 1 >= currentAnim->frameCount)
                desc.curr.ratio = 0.f;
        }
    }

    // ✅ 다음 애니메이션이 예약된 경우 블렌딩 처리
    if (desc.next.animIndex >= 0)
    {
        desc.tweenSumTime += DT;
        desc.tweenRatio = desc.tweenSumTime / desc.tweenDuration;

        if (desc.tweenRatio >= 1.0f)
        {
            // ✅ 블렌딩 완료 → 다음 애니메이션으로 변경
            desc.curr = desc.next;
            desc.ClearNextAnim();
        }
        else
        {
            shared_ptr<ModelAnimation> nextAnim = _model->GetAnimationByIndex(desc.next.animIndex);
            desc.next.sumTime += DT;

            float timePerFrame = 1.0f / (nextAnim->frameRate * desc.next.speed);

            if (desc.next.ratio >= 1.0f)
            {
                desc.next.sumTime = 0;
                desc.next.currFrame = (desc.next.currFrame + 1) % nextAnim->frameCount;
                desc.next.nextFrame = (desc.next.currFrame + 1) % nextAnim->frameCount;
            }

            desc.next.ratio = desc.next.sumTime / timePerFrame;
        }
    }
}

void ModelAnimator::RenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
    if (_model == nullptr)
        return;

    if (_animation == nullptr)
        CreateTexture();

    if (_animation == nullptr)
        return;

    // GlobalData
    _shader->PushGlobalData(Camera::S_MatView, Camera::S_MatProjection);

    // Light
    auto lightObj = SCENE->GetCurrentScene()->GetLight();

    if (lightObj)
        _shader->PushLightData(lightObj->GetLight()->GetLightDesc());

    // SRV를 통해 정보 전달
    _shader->GetSRV("TransformMap")->SetResource(_animation->srv.Get());

    // Bones
    shared_ptr<BoneDesc> boneDesc = make_shared<BoneDesc>();

    const uint32 boneCount = _model->GetBoneCount();

    for (uint32 i = 0; i < boneCount; i++)
    {
        shared_ptr<ModelBone> bone = _model->GetBoneByIndex(i);
        boneDesc->transforms[i] = bone->transform;
    }

    _shader->PushBoneData(*boneDesc);

    const auto& meshes = _model->GetMeshes();

    for (auto& mesh : meshes)
    {
        if (mesh->material)
            mesh->material->Update();

        // BoneIndex
        _shader->GetScalar("BoneIndex")->SetInt(mesh->boneIndex);

        _shader->GetMatrix("MeshTransformMatrix")->SetMatrix(reinterpret_cast<const float*>(&mesh->transformMatrix));

        mesh->vertexBuffer->PushData();
        mesh->indexBuffer->PushData();

        buffer->PushData();
        DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        _shader->DrawIndexedInstanced(0, _pass, mesh->indexBuffer->GetCount(), buffer->GetCount());
    }
}

InstanceID ModelAnimator::GetInstanceID()
{
    return make_pair((uint64)_model.get(), (uint64)_shader.get());
}

bool ModelAnimator::GetBoneWorldPosition(const wstring& boneName, Vec3& position)
{
    if (!_model || _model->GetAnimationCount() == 0)
        return false;

    if (_animation == nullptr)
        CreateTexture();

    if (_animation == nullptr)
        return false;
    shared_ptr<ModelBone> bone = _model->GetBoneByName(boneName);

    if (!bone)
        return false;
    int32 boneIndex = -1;

    for (uint32 b = 0; b < _model->GetBoneCount(); ++b)
        if (_model->GetBoneByIndex(b) == bone)
            boneIndex = static_cast<int32>(b);

    if (boneIndex < 0)
        return false;

    auto pose = [&](const KeyframeDesc& key)
    {
        const uint32 clip =
            static_cast<uint32>(std::clamp(key.animIndex, 0, static_cast<int32>(_animation->clipCount) - 1));
        const Matrix& a = _animation->At(clip, min<uint32>(key.currFrame, _animation->frameCount - 1), boneIndex);
        const Matrix& b = _animation->At(clip, min<uint32>(key.nextFrame, _animation->frameCount - 1), boneIndex);

        return Matrix::Lerp(a, b, key.ratio);
    };
    // Same blend as the animation vertex shader.
    Matrix skin = pose(_tweenDesc.curr);

    if (_tweenDesc.next.animIndex >= 0)
        skin = Matrix::Lerp(skin, pose(_tweenDesc.next), _tweenDesc.tweenRatio);
    const Matrix meshTransform =
        _model->GetMeshCount() > 0 ? _model->GetMeshByIndex(0)->transformMatrix : Matrix::Identity;
    const Vec3 bind = bone->transform.Translation();
    position =
        Vec3::Transform(Vec3::Transform(Vec3::Transform(bind, skin), meshTransform), GetTransform()->GetWorldMatrix());

    return true;
}

void ModelAnimator::CreateTexture()
{
    const uint32 clipCount = _model->GetAnimationCount();

    if (clipCount == 0)
        return;

    if (auto shared = _model->GetAnimationTexture(); shared && shared->clipCount == clipCount)
    {
        _animation = shared;

        return;
    }

    PROFILE_SCOPE("  build animation texture");

    auto data = make_shared<AnimationTextureData>();
    data->clipCount = clipCount;
    data->boneCount = max<uint32>(1, min<uint32>(_model->GetBoneCount(), MAX_MODEL_TRANSFORMS));

    for (uint32 i = 0; i < clipCount; i++)
        data->frameCount = max<uint32>(data->frameCount, _model->GetAnimationByIndex(i)->frameCount);
    data->frameCount = max<uint32>(1, min<uint32>(data->frameCount, MAX_MODEL_KEYFRAMES));
    data->transforms.assign(static_cast<size_t>(clipCount) * data->frameCount * data->boneCount, Matrix::Identity);

    for (uint32 i = 0; i < clipCount; i++)
        CreateAnimationTransform(*_model, *data, i);

    {
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(D3D11_TEXTURE2D_DESC));
        desc.Width = data->boneCount * 4;
        desc.Height = data->frameCount;
        desc.ArraySize = clipCount;
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;

        const uint32 rowPitch = data->boneCount * sizeof(Matrix);
        vector<D3D11_SUBRESOURCE_DATA> subResources(clipCount);

        for (uint32 c = 0; c < clipCount; c++)
        {
            subResources[c].pSysMem = &data->At(c, 0, 0);
            subResources[c].SysMemPitch = rowPitch;
            subResources[c].SysMemSlicePitch = rowPitch * data->frameCount;
        }

        HRESULT hr = DEVICE->CreateTexture2D(&desc, subResources.data(), data->texture.GetAddressOf());
        CHECK(hr);
    }

    // Create SRV
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        desc.Texture2DArray.MipLevels = 1;
        desc.Texture2DArray.ArraySize = clipCount;

        HRESULT hr = DEVICE->CreateShaderResourceView(data->texture.Get(), &desc, data->srv.GetAddressOf());
        CHECK(hr);
    }

    _model->SetAnimationTexture(data);
    _animation = data;
}

void ModelAnimator::CreateAnimationTransform(Model& model, AnimationTextureData& out, uint32 index)
{
    vector<Matrix> tempAnimBoneTransforms(out.boneCount, Matrix::Identity);

    shared_ptr<ModelAnimation> animation = model.GetAnimationByIndex(index);

    vector<shared_ptr<ModelKeyframe>> tracks(out.boneCount);

    for (uint32 b = 0; b < out.boneCount; b++)
        tracks[b] = animation->GetKeyframe(model.GetBoneByIndex(b)->name);

    const uint32 frameCount = min<uint32>(animation->frameCount, out.frameCount);

    for (uint32 f = 0; f < frameCount; f++)
    {
        for (uint32 b = 0; b < out.boneCount; b++)
        {
            shared_ptr<ModelBone> bone = model.GetBoneByIndex(b);

            Matrix matAnimation;

            const shared_ptr<ModelKeyframe>& frame = tracks[b];

            if (frame != nullptr)
            {
                ModelKeyframeData& data = frame->transforms[f];

                Matrix S, R, T;
                S = Matrix::CreateScale(data.scale.x, data.scale.y, data.scale.z);
                R = Matrix::CreateFromQuaternion(data.rotation);
                T = Matrix::CreateTranslation(data.translation.x, data.translation.y, data.translation.z);

                matAnimation = S * R * T;
            }
            else
            {
                matAnimation = Matrix::Identity;
            }

            // [ !!!!!!! ]
            Matrix toRootMatrix = bone->transform;
            Matrix invGlobal = toRootMatrix.Invert();

            int32 parentIndex = bone->parentIndex;

            Matrix matParent = Matrix::Identity;

            if (parentIndex >= 0)
                matParent = tempAnimBoneTransforms[parentIndex];

            tempAnimBoneTransforms[b] = matAnimation * matParent;

            out.transforms[(static_cast<size_t>(index) * out.frameCount + f) * out.boneCount + b] =
                invGlobal * tempAnimBoneTransforms[b];
        }
    }
}
