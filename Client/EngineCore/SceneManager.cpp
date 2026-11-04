#include "pch.h"
#include "SceneManager.h"

void SceneManager::Update()
{
    if (_currentScene == nullptr)
        return;

    {
        PROFILE_SCOPE("scene update");
        _currentScene->Update();
    }

    {
        PROFILE_SCOPE("scene late update");
        _currentScene->LateUpdate();
    }

    {
        PROFILE_SCOPE("scene render");
        _currentScene->Render();
    }
}

void SceneManager::GUIRender()
{
    if (_currentScene == nullptr)
        return;

    _currentScene->GUIRender();
}
