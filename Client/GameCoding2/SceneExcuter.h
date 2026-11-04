#pragma once

class SceneExcuter : public IExecute
{
public:
    SceneExcuter();
    ~SceneExcuter();

public:
    void Init() override;
    void Update() override;
    void Render() override;

public:
    shared_ptr<Scene> testScene = nullptr;

private:
    shared_ptr<Shader> _shader;
};
