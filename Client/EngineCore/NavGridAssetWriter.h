#pragma once

#include "NavGridAsset.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class EditableNavGridData
{
public:
    static bool Create(const std::string& mapId, uint32_t width, uint32_t height, float cellSize, const Vec3& origin,
                       float defaultAgentRadius, EditableNavGridData& outData, std::string& outError);

    static EditableNavGridData FromAsset(const NavGridAsset& asset);

    const std::string& GetMapId() const noexcept
    {
        return _mapId;
    }

    uint32_t GetWidth() const noexcept
    {
        return _width;
    }

    uint32_t GetHeight() const noexcept
    {
        return _height;
    }

    float GetCellSize() const noexcept
    {
        return _cellSize;
    }

    const Vec3& GetOrigin() const noexcept
    {
        return _origin;
    }

    float GetDefaultAgentRadius() const noexcept
    {
        return _defaultAgentRadius;
    }

    const std::vector<uint8_t>& GetCells() const noexcept
    {
        return _cells;
    }

    void SetMapId(std::string value)
    {
        _mapId = std::move(value);
    }

    void SetDefaultAgentRadius(float value) noexcept
    {
        _defaultAgentRadius = value;
    }

    bool IsValidCell(int32_t x, int32_t z) const noexcept;
    NavCellType GetCell(int32_t x, int32_t z) const;
    bool SetCell(int32_t x, int32_t z, NavCellType type) noexcept;
    std::optional<NavGridCoordinate> WorldToCell(const Vec3& worldPosition) const noexcept;
    std::optional<Vec3> CellToWorldCenter(const NavGridCoordinate& cell) const noexcept;

private:
    std::string _mapId;
    uint32_t _width = 0;
    uint32_t _height = 0;
    float _cellSize = 0.0f;
    Vec3 _origin = Vec3(0.0f, 0.0f, 0.0f);
    float _defaultAgentRadius = 0.0f;
    std::vector<uint8_t> _cells;
};

class NavGridAssetWriter
{
public:
    static bool ComputeContentHash(const EditableNavGridData& data, std::array<uint8_t, 32>& outHash,
                                   std::string& outError);

    static bool WriteAtomic(const std::wstring& path, const EditableNavGridData& data, std::array<uint8_t, 32>& outHash,
                            std::string& outError);
};
