#include "pch.h"
#include "Scene.h"
#include "GameObject.h"
#include "BaseCollider.h"
#include "Camera.h"
#include "Terrain.h"
#include "ModelRenderer.h"
#include "Button.h"

void Scene::Awake()
{
    unordered_set<shared_ptr<GameObject>> objects = _objects;

    for (shared_ptr<GameObject> object : objects)
    {
        object->Awake();
    }
}

// Scene??배치 ?�어?�는 object??
void Scene::Start()
{
    unordered_set<shared_ptr<GameObject>> objects = _objects;

    for (shared_ptr<GameObject> object : objects)
    {
        object->Start();
    }
}

void Scene::Update()
{
    unordered_set<shared_ptr<GameObject>> objects = _objects;
    Profiler::Gauge("objects", static_cast<double>(objects.size()));

    for (shared_ptr<GameObject> object : objects)
    {
        object->Update();
    }

    PickUI();
}

void Scene::LateUpdate()
{
    unordered_set<shared_ptr<GameObject>> objects = _objects;

    for (shared_ptr<GameObject> object : objects)
    {
        object->LateUpdate();
    }
}

void Scene::Render()
{
    for (auto& camera : _cameras)
    {
        {
            PROFILE_SCOPE("  sort objects");
            camera->GetCamera()->SortGameObject();
        }

        camera->GetCamera()->Render_Forward();
    }

    unordered_set<shared_ptr<GameObject>> objects = _objects;
}

void Scene::GUIRender()
{
    unordered_set<shared_ptr<GameObject>> objects = _objects;

    for (shared_ptr<GameObject> object : objects)
    {
        if (object && object->_enableGUI == true)
        {
            object->GUIRender();
        }

        // UI Transform
        auto btnComponent = object->GetButton();

        if (btnComponent)
        {
            btnComponent->GUIRender();
        }
    }
}

void Scene::Add(shared_ptr<GameObject> object)
{
    if (_objects.find(object) != _objects.end())
        return;

    _objects.insert(object);

    if (object->GetCamera() != nullptr)
    {
        _cameras.insert(object);
    }

    if (object->GetLight() != nullptr)
    {
        _lights.insert(object);
    }

    object->Awake();
    object->Start();
}

void Scene::Remove(shared_ptr<GameObject> object)
{
    if (!object)
        return;

    // 부모�? ?�거?????�식???�께 ?�거
    auto children = object->GetTransform()->GetChildren();

    for (auto& child : children)
    {
        Remove(child->GetGameObject());
    }

    // 리스?�에???�거
    _objects.erase(object);
    _cameras.erase(object);
    _lights.erase(object);
}

shared_ptr<GameObject> Scene::GetMainCamera()
{
    for (auto& camera : _cameras)
    {
        if (camera->GetCamera()->GetProjectionType() == ProjectionType::Perspective)
            return camera;
    }

    return nullptr;
}

shared_ptr<GameObject> Scene::GetUICamera()
{
    for (auto& camera : _cameras)
    {
        if (camera->GetCamera()->GetProjectionType() == ProjectionType::Orthographic)
            return camera;
    }

    return nullptr;
}

void Scene::PickUI()
{
    if (_worldInputBlocked)
        return;

    if (INPUT->GetButtonDown(KEY_TYPE::LBUTTON) == false)
        return;

    if (GetUICamera() == nullptr)
        return;

    POINT screenPt = INPUT->GetMousePos();

    shared_ptr<Camera> camera = GetUICamera()->GetCamera();

    const auto gameObjects = GetObjects();

    for (auto& gameObject : gameObjects)
    {
        if (gameObject->GetButton() == nullptr)
            continue;

        if (gameObject->GetButton()->Picked(screenPt))
            gameObject->GetButton()->InvokeOnClicked();
    }
}

bool Scene::TryCreatePickingRay(int32 screenX, int32 screenY, Ray& ray)
{
    auto cameraObject = GetMainCamera();

    if (!cameraObject || !cameraObject->GetCamera())
        return false;

    const float width = GRAPHICS->GetViewport().GetWidth();
    const float height = GRAPHICS->GetViewport().GetHeight();

    if (width <= 0.f || height <= 0.f)
        return false;

    const Matrix projection = cameraObject->GetCamera()->GetProjectionMatrix();
    const float viewX = (+2.0f * screenX / width - 1.0f) / projection(0, 0);
    const float viewY = (-2.0f * screenY / height + 1.0f) / projection(1, 1);
    const Matrix inverseView = cameraObject->GetCamera()->GetViewMatrix().Invert();

    const Vec3 worldOrigin = XMVector3TransformCoord(Vec3::Zero, inverseView);
    Vec3 worldDirection = XMVector3TransformNormal(Vec3(viewX, viewY, 1.0f), inverseView);

    if (worldDirection.LengthSquared() <= FLT_EPSILON)
        return false;

    worldDirection.Normalize();
    ray = Ray(worldOrigin, worldDirection);

    return true;
}

shared_ptr<GameObject> Scene::Pick(int32 screenX, int32 screenY)
{
    Vec3 ignoredPickPosition;

    return Pick(screenX, screenY, ignoredPickPosition);
}

shared_ptr<GameObject> Scene::Pick(int32 screenX, int32 screenY, Vec3& pickPos)
{
    Ray ray;

    if (!TryCreatePickingRay(screenX, screenY, ray))
        return nullptr;

    float minDistance = FLT_MAX;
    shared_ptr<GameObject> picked;

    for (const auto& gameObject : _objects)
    {
        const auto collider = gameObject->GetCollider();

        if (!collider)
            continue;

        float distance = 0.f;

        if (!collider->Intersects(ray, OUT distance) || distance >= minDistance)
            continue;

        minDistance = distance;
        picked = gameObject;
        pickPos = ray.position + ray.direction * distance;
    }

    if (_terrain && _terrain->GetTerrain())
    {
        Vec3 terrainPickPosition;
        float terrainDistance = 0.f;

        if (_terrain->GetTerrain()->Pick(ray, OUT terrainPickPosition, OUT terrainDistance) &&
            terrainDistance < minDistance)
        {
            picked = _terrain;
            pickPos = terrainPickPosition;
        }
    }

    return picked;
}
