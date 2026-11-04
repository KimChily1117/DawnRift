#pragma once

#define WIN32_LEAN_AND_MEAN

#include "Types.h"
#include "Define.h"
#include "Utils.h"

// STL
#include <memory>
#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
using namespace std;

// WIN
#include <windows.h>
#include <assert.h>
#include <optional>
#include <sstream>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include <d3d11.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <d3d11.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/DirectXTex.inl>
using namespace DirectX;
using namespace Microsoft::WRL;

// FX관련
#include <FX11/d3dx11effect.h>

// Json Library (NlohmannJson)
#include "Nlohmann/json.hpp"
using json = nlohmann::json;

// Assimp
#include <Assimp/Importer.hpp>
#include <Assimp/scene.h>
#include <Assimp/postprocess.h>

//ImGUI
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// Libs
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#ifdef _DEBUG
#pragma comment(lib, "DirectXTex/DirectXTex_debug.lib")
#pragma comment(lib, "FX11/Effects11d.lib")
#pragma comment(lib, "Assimp/assimp-vc143-mtd.lib")
#pragma comment(lib, "Fmod/fmod_vc.lib")
#pragma comment(lib, "ServerCore/ServerCore.lib")
#else
#pragma comment(lib, "DirectXTex/DirectXTex.lib")
#pragma comment(lib, "FX11/Effects11.lib")
#pragma comment(lib, "Assimp/assimp-vc143-mt.lib")
#pragma comment(lib, "Fmod/fmod_vc.lib")
#pragma comment(lib, "ServerCore/Release/ServerCore.lib")
#endif

// Managers
#include "Game.h"
#include "Graphics.h"
#include "InputManager.h"
#include "TimeManager.h"
#include "ResourceManager.h"
#include "ImGuiManager.h"
#include "SceneManager.h"
#include "InstancingManager.h"
#include "NetworkManager.h"
#include "GameManager.h"
#include "UIManager.h"
#include "SoundManager.h"
#include "ParticleManager.h"
#include "Profiler.h"

// Engine
#include "VertexData.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "Shader.h"
#include "IExecute.h"

#include "GameObject.h"
#include "Transform.h"
#include "Texture.h"
#include "Mesh.h"
#include "Button.h"

// ServerCore
#include "ServerCore/Types.h"
#include "ServerCore/CoreMacro.h"
#include "ServerCore/CoreTLS.h"
#include "ServerCore/CoreGlobal.h"
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <winsock2.h>

#include "ServerCore/SocketUtils.h"
#include "ServerCore/SendBuffer.h"
#include "ServerCore/Session.h"

#pragma comment(lib, "ws2_32.lib")
