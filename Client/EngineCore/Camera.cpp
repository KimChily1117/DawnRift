// 카메라 컴포넌트.
#include "pch.h"
#include "Camera.h"
#include "Scene.h"
#include "Button.h"
#include "MeshRenderer.h"

Matrix Camera::S_MatView = Matrix::Identity;
Matrix Camera::S_MatProjection = Matrix::Identity;

void Camera::SortGameObject()
{
    shared_ptr<Scene> scene = CUR_SCENE;
    unordered_set<shared_ptr<GameObject>>& gameObjects = scene->GetObjects();

    _vecForward.clear();
    _vecUI.clear();

    for (auto& gameObject : gameObjects)
    {
        if (IsCulled(gameObject->GetLayerIndex()))
            continue;

        if (gameObject->GetButton() != nullptr)
        {
            _vecUI.push_back(gameObject);
            continue;
        }

        if (gameObject->GetMeshRenderer() != nullptr || gameObject->GetModelRenderer() != nullptr ||
            gameObject->GetModelAnimator() != nullptr || gameObject->GetParticleRenderer() != nullptr)
        {
            _vecForward.push_back(gameObject);
        }
    }

    // UI 렌더링 순서 정렬 (Order 낮은 UI가 먼저 렌더링됨)
    std::sort(_vecUI.begin(), _vecUI.end(),
              [](const shared_ptr<GameObject>& a, const shared_ptr<GameObject>& b)
              { return a->GetButton()->GetOrder() < b->GetButton()->GetOrder(); });
}

void Camera::Render_Forward()
{
    S_MatView = _matView;
    S_MatProjection = _matProjection;

    // UI 렌더링 전에 Orthographic Projection 적용
    if (!_vecUI.empty())
    {
        S_MatProjection = ::XMMatrixOrthographicLH(_width, _height, _near, _far);
    }

    GET_SINGLE(InstancingManager)->Render(_vecForward);
    GET_SINGLE(InstancingManager)->Render(_vecUI);
}

Camera::Camera() : Super(ComponentType::Camera)
{
    _width = static_cast<float>(GAME->GetGameDesc().width);
    _height = static_cast<float>(GAME->GetGameDesc().height);
}

Camera::~Camera() {}

void Camera::Update()
{
    UpdateMatrix();
}

void Camera::UpdateMatrix()
{
    Vec3 eyePosition = GetTransform()->GetPosition();
    Vec3 focusPosition = eyePosition + GetTransform()->GetLook();
    Vec3 upDirection = GetTransform()->GetUp();

    _matView = ::XMMatrixLookAtLH(eyePosition, focusPosition, upDirection);

    if (_type == ProjectionType::Perspective)
    {
        _matProjection = ::XMMatrixPerspectiveFovLH(_fov, _width / _height, _near, _far);
    }
    else
    {
        _matProjection = ::XMMatrixOrthographicLH(_width, _height, _near, _far);
    }
}
