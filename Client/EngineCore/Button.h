#pragma once

class Button : public Component
{
    using Super = Component;

public:
    Button();
    virtual ~Button();

    bool Picked(POINT screenPos);

    void Create(Vec2 screenPos, Vec2 size, shared_ptr<class Material> material);

    void AddOnClickedEvent(std::function<void(void)> func);
    void InvokeOnClicked();

    // Order 추가
    void SetOrder(int order)
    {
        _order = order;
    }

    int GetOrder() const
    {
        return _order;
    }

    void ChangeImageMatrial(shared_ptr<class Material> material);
    void GUIRender();

    shared_ptr<MeshRenderer> GetMatrial()
    {
        return _gameObject.lock()->GetMeshRenderer();
    }

private:
    std::function<void(void)> _onClicked;
    RECT _rect;

public:
    int32 _order;
};
