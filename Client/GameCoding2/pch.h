#pragma once

#ifdef _DEBUG
#pragma comment(lib, "EngineCore/EngineCore.lib")
#else
#pragma comment(lib, "EngineCore/Release/EngineCore.lib")
#endif
#include "EngineCore/EnginePch.h"

// Network
#ifdef _DEBUG
#pragma comment(lib, "Protobuf\\Debug\\libprotobufd.lib")
#else
#pragma comment(lib, "Protobuf\\Release\\libprotobuf.lib")
#endif

#include "ServerSession.h"
#include "ClientPacketHandler.h"

// Proto
#include "Protocol.pb.h"
