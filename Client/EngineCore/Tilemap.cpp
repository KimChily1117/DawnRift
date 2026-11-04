#include "pch.h"
#include "Tilemap.h"
#include <fstream>
#include <cmath>

Tilemap::Tilemap() : Super(ResourceType::Tilemap) {}

Tilemap::~Tilemap()
{
    _tiles.clear();
}

void Tilemap::Load(const wstring& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file.is_open())
        return;

    file.read(reinterpret_cast<char*>(&_mapSize), sizeof(Vec3));
    file.read(reinterpret_cast<char*>(&_tileSize), sizeof(int32));

    _tiles.resize(_mapSize.x, std::vector<Tile>(_mapSize.z));

    for (int z = 0; z < _mapSize.z; z++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            file.read(reinterpret_cast<char*>(&_tiles[x][z]), sizeof(Tile));
        }
    }

    file.close();
}

void Tilemap::Save(const std::wstring& path)
{
    std::wofstream file(path, std::ios::out);

    if (!file.is_open())
    {
        std::wcerr << L"❌ Failed to open file for writing: " << path << std::endl;

        return;
    }

    std::wcout << L"✅ File opened successfully: " << path << std::endl;

    // 맵 크기 저장
    file << _mapSize.x << L" " << _mapSize.z << std::endl;

    // 타일 정보 저장 (x, y, z, value, isWalkable)
    for (int z = 0; z < _mapSize.z; z++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            Tile* tile = GetTileAt(Vec3(x, 0, z));

            int height = tile ? tile->position.y : 0;
            int value = tile ? tile->value : 0;
            int isWalkable = 1;

            file << x << L"\t" << height << L"\t" << z << L"\t" << value << L"\t" << isWalkable << std::endl;
        }
    }

    file.close();
    std::wcout << L"✅ Tilemap saved to: " << path << std::endl;
}

Tile* Tilemap::GetTileAt(Vec3 pos)
{
    Vec3 tilePos = ConvertWorldToTile(pos);

    if (tilePos.x < 0 || tilePos.z < 0 || tilePos.x >= _mapSize.x || tilePos.z >= _mapSize.z)
    {
        return nullptr;
    }

    return &_tiles[static_cast<int>(tilePos.x)][static_cast<int>(tilePos.z)];
}

void Tilemap::SetMapSize(Vec3 size)
{
    _mapSize = size;
    _tiles.resize(size.x, std::vector<Tile>(size.z));

    for (int z = 0; z < size.z; z++)
    {
        for (int x = 0; x < size.x; x++)
        {
            _tiles[x][z].position = Vec3(x * _tileSize, 0, z * _tileSize);
            _tiles[x][z].height = 0;
            _tiles[x][z].isWalkable = true;
        }
    }
}

void Tilemap::SetTileSize(int32 size)
{
    _tileSize = size;
}

Vec3 Tilemap::ConvertWorldToTile(Vec3 worldPos)
{
    int tileX = static_cast<int>(floor(worldPos.x / _tileSize));
    int tileZ = static_cast<int>(floor(worldPos.z / _tileSize));

    return Vec3(tileX, 0, tileZ);
}

bool Tilemap::PickTile(Vec3 worldPos, Vec3& outTilePos)
{
    return false;
}

bool Tilemap::CheckCollision(Vec3 position)
{
    Tile* tile = GetTileAt(position);

    if (!tile || !tile->isWalkable)
        return true;

    return false;
}

void Tilemap::ApplySkillRange(Vec3 skillPos, float skillRange)
{
    for (int z = 0; z < _mapSize.z; z++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            Tile* tile = GetTileAt(Vec3(x, 0, z));

            if (!tile)
                continue;

            float dist = sqrt(pow(tile->position.x - skillPos.x, 2) + pow(tile->position.z - skillPos.z, 2));

            if (dist <= skillRange)
            {
                tile->value = 1;
            }
        }
    }
}

void Tilemap::UpdateFogOfWar(Vec3 playerPos, float visionRange)
{
    for (int z = 0; z < _mapSize.z; z++)
    {
        for (int x = 0; x < _mapSize.x; x++)
        {
            Tile* tile = GetTileAt(Vec3(x, 0, z));

            if (!tile)
                continue;

            float dist = sqrt(pow(tile->position.x - playerPos.x, 2) + pow(tile->position.z - playerPos.z, 2));

            if (dist <= visionRange)
            {
                tile->isWalkable = true;
            }
        }
    }
}
