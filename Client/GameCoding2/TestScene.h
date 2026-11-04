#pragma once
class NavigationEditorTool;

class TestScene : public Scene
{
    using Super = Scene;

public:
    void Start() override;
    void Update() override;
    void LateUpdate() override;
    void GUIRender() override;

    void InitializeObject();

    shared_ptr<Shader> GetShader()
    {
        return _shader;
    }

    shared_ptr<GameObject> _cursor;
    shared_ptr<NavigationEditorTool> _navigationEditor;
};
