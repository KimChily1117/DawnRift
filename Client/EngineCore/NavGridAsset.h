// 내비게이션 그리드 데이터.
#pragma once

#include "Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class NavCellType : uint8_t
{
    Walkable = 0,
    Blocked = 1,
    Slow = 2,
};

struct NavGridCoordinate
{
    int32_t x = 0;
    int32_t z = 0;

    bool operator==(const NavGridCoordinate&) const = default;
};

enum class NavGridLoadError
{
    None,
    FileOpenFailed,
    FileTooLarge,
    TruncatedHeader,
    InvalidMagic,
    UnsupportedVersion,
    InvalidHeaderSize,
    MapIdTooLong,
    InvalidUtf8MapId,
    InvalidDimensions,
    CellCountTooLarge,
    InvalidCellSize,
    InvalidOrigin,
    InvalidAgentRadius,
    UnsupportedCellEncoding,
    InvalidCellDataLength,
    TruncatedCellData,
    TrailingData,
    UnknownCellValue,
    HashMismatch,
    HashFailure,
};

struct NavGridLoadStatus
{
    NavGridLoadError error = NavGridLoadError::None;
    std::string message;

    explicit operator bool() const noexcept
    {
        return error == NavGridLoadError::None;
    }
};

class NavGridAsset
{
public:
    const std::string& GetMapId() const noexcept
    {
        return _mapId;
    }

    uint32_t GetFormatVersion() const noexcept
    {
        return _formatVersion;
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

    uint32_t GetCellEncoding() const noexcept
    {
        return _cellEncoding;
    }

    const std::array<uint8_t, 32>& GetContentHash() const noexcept
    {
        return _contentHash;
    }

    std::string GetContentHashHex() const;

    bool IsValidCell(int32_t x, int32_t z) const noexcept;
    NavCellType GetCell(int32_t x, int32_t z) const;
    std::optional<NavGridCoordinate> WorldToCell(const Vec3& worldPosition) const noexcept;
    std::optional<Vec3> CellToWorldCenter(const NavGridCoordinate& cell) const noexcept;

private:
    friend class NavGridAssetLoader;

    uint32_t _formatVersion = 0;
    std::string _mapId;
    uint32_t _width = 0;
    uint32_t _height = 0;
    float _cellSize = 0.0f;
    Vec3 _origin = Vec3(0.0f, 0.0f, 0.0f);
    float _defaultAgentRadius = 0.0f;
    uint32_t _cellEncoding = 0;
    std::array<uint8_t, 32> _contentHash{};
    std::vector<uint8_t> _cells;
};

class NavGridHash
{
public:
    static bool ComputeSha256(const uint8_t* data, size_t size, std::array<uint8_t, 32>& outHash,
                              std::string& outError);
    static std::string ToHex(const std::array<uint8_t, 32>& hash);
};

class NavGridAssetLoader
{
public:
    static constexpr uint32_t FormatVersion = 1;
    static constexpr uint32_t CellEncodingBytePerCell = 1;
    static constexpr uint32_t MaximumMapIdBytes = 128;
    static constexpr uint32_t MaximumDimension = 4096;
    static constexpr uint64_t MaximumCellCount = 4'194'304;

    static NavGridLoadStatus Load(const std::wstring& path, NavGridAsset& outAsset);
};
