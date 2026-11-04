// 메시 렌더링 컴포넌트.
#include "pch.h"
#include "MeshRenderer.h"
#include "Game.h"
#include "Mesh.h"
#include "Shader.h"
#include "Material.h"
#include "InstancingBuffer.h"
#include "Camera.h"
#include "Light.h"

MeshRenderer::MeshRenderer() : Super(ComponentType::MeshRenderer) {}

MeshRenderer::~MeshRenderer() {}

void MeshRenderer::RenderInstancing(shared_ptr<class InstancingBuffer>& buffer)
{
    if (_mesh == nullptr || _material == nullptr)
        return;

    auto shader = _material->GetShader();

    if (shader == nullptr)
        return;

    // GlobalData
    shader->PushGlobalData(Camera::S_MatView, Camera::S_MatProjection);

    // Light
    auto lightObj = SCENE->GetCurrentScene()->GetLight();

    if (lightObj)
        shader->PushLightData(lightObj->GetLight()->GetLightDesc());

    // Light
    _material->Update();

    _mesh->GetVertexBuffer()->PushData();
    _mesh->GetIndexBuffer()->PushData();
    DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    buffer->PushData();

    shader->DrawIndexedInstanced(0, _pass, _mesh->GetDrawIndexCount(), buffer->GetCount());
}

InstanceID MeshRenderer::GetInstanceID()
{
    return make_pair((uint64)_mesh.get(), (uint64)_material.get());
}

void MeshRenderer::RenderSingle()
{
    if (_mesh == nullptr || _material == nullptr)
        return;

    auto shader = _material->GetShader();

    if (shader == nullptr)
        return;

    // ✅ UI 전용 카메라의 직교 투영 행렬 적용
    Matrix projection = CUR_SCENE->GetUICamera()->GetCamera()->GetProjectionMatrix();
    Matrix view = CUR_SCENE->GetUICamera()->GetCamera()->GetViewMatrix();

    DEBUG_LOG("GET NAME : " << GetGameObject()->_name.c_str());

    Matrix world = GetTransform()->GetWorldMatrix();

    // ✅ 쉐이더 데이터 업데이트
    shader->PushGlobalData(view, projection);

    // ✅ UI Transform 업데이트
    TransformDesc desc = {};
    desc.W = world;
    shader->PushTransformData(desc);

    // ✅ 인덱스 및 버퍼 설정
    _mesh->GetVertexBuffer()->PushData();
    _mesh->GetIndexBuffer()->PushData();
    DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ✅ 단일 렌더링 실행
    shader->DrawIndexed(0, 0, _mesh->GetDrawIndexCount(), 0, 0);
}
