#include "pch.h"
#include "Button.h"
#include "MeshRenderer.h"
#include "Material.h"

Button::Button() : Super(ComponentType::Button) {}

Button::~Button() {}

bool Button::Picked(POINT screenPos)
{
    return ::PtInRect(&_rect, screenPos);
}

void Button::Create(Vec2 screenPos, Vec2 size, shared_ptr<class Material> material)
{
    auto go = _gameObject.lock();

    float height = GRAPHICS->GetViewport().GetHeight();
    float width = GRAPHICS->GetViewport().GetWidth();

    float x = screenPos.x - width / 2;
    float y = height / 2 - screenPos.y;
    Vec3 position = Vec3(x, y, 0);

    go->GetOrAddTransform()->SetPosition(position);
    go->GetOrAddTransform()->SetScale(Vec3(size.x, size.y, 1));

    go->SetLayerIndex(Layer_UI);

    if (go->GetMeshRenderer() == nullptr)
        go->AddComponent(make_shared<MeshRenderer>());

    go->GetMeshRenderer()->SetMaterial(material);

    auto mesh = RESOURCES->Get<Mesh>(L"Quad");
    go->GetMeshRenderer()->SetMesh(mesh);
    go->GetMeshRenderer()->SetPass(0);

    // Picking
    _rect.left = screenPos.x - size.x / 2;
    _rect.right = screenPos.x + size.x / 2;
    _rect.top = screenPos.y - size.y / 2;
    _rect.bottom = screenPos.y + size.y / 2;
}

void Button::AddOnClickedEvent(std::function<void(void)> func)
{
    _onClicked = func;
}

void Button::InvokeOnClicked()
{
    if (_onClicked)
        _onClicked();
}

void Button::ChangeImageMatrial(shared_ptr<Material> material)
{
    _gameObject.lock()->GetMeshRenderer()->SetMaterial(material);
}

void Button::GUIRender()
{
    if (!_gameObject.expired())
    {
        auto go = _gameObject.lock();
        auto transform = go->GetOrAddTransform();

        Vec3 pos = transform->GetLocalPosition();
        Vec3 scale = transform->GetLocalScale();

        // 회전 값을 라디안에서 도 단위로 변환
        XMFLOAT3 currentRot;
        currentRot.x = XMConvertToDegrees(transform->GetLocalRotation().x);
        currentRot.y = XMConvertToDegrees(transform->GetLocalRotation().y);
        currentRot.z = XMConvertToDegrees(transform->GetLocalRotation().z);

        ImGui::Begin("UI Transform Editor");

        // UI 이름 표시
        ImGui::Text(go->_name.c_str());

        // Position 조절
        std::string temp = go->_name + "_Position";

        if (ImGui::DragFloat3(temp.c_str(), (float*)&pos, 0.1f, -1000.0f, 1000.0f))
        {
            transform->SetLocalPosition(pos);
        }

        temp = go->_name + "_Rotation";
        XMFLOAT3 newRot = currentRot;

        if (ImGui::DragFloat3(temp.c_str(), (float*)&newRot, 1.0f, -180, 180))
        {
            newRot.x = fmod(newRot.x, 360.0f);
            newRot.y = fmod(newRot.y, 360.0f);
            newRot.z = fmod(newRot.z, 360.0f);

            transform->SetLocalRotation(
                Vec3(XMConvertToRadians(newRot.x), XMConvertToRadians(newRot.y), XMConvertToRadians(newRot.z)));
        }

        // Scale 조절
        temp = go->_name + "_Scale";

        if (ImGui::DragFloat3(temp.c_str(), (float*)&scale, 0.1f, 0.1f, 10.0f))
        {
            transform->SetLocalScale(scale);
        }

        ImGui::End();
    }
}
