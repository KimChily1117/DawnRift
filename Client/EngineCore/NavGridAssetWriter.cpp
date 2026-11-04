#include "NavGridAssetWriter.h"

#include <Windows.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace
{
constexpr uint8_t Magic[] = {'N', 'V', 'G', '1'};
constexpr uint32_t FixedHeaderBytes = 84;

void AppendU32(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 24));
}

void AppendF32(std::vector<uint8_t>& bytes, float value)
{
    AppendU32(bytes, std::bit_cast<uint32_t>(value));
}

bool IsValidUtf8(const std::string& value)
{
    if (value.empty())
        return true;

    if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
        return false;

    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr,
                               0) > 0;
}

bool Validate(const EditableNavGridData& data, std::string& error)
{
    if (data.GetMapId().size() > NavGridAssetLoader::MaximumMapIdBytes || !IsValidUtf8(data.GetMapId()))
    {
        error = "NavigationMapId must be valid UTF-8 and at most 128 bytes.";

        return false;
    }

    if (data.GetWidth() == 0 || data.GetHeight() == 0 || data.GetWidth() > NavGridAssetLoader::MaximumDimension ||
        data.GetHeight() > NavGridAssetLoader::MaximumDimension)
    {
        error = "NavGrid dimensions are zero or exceed 4096.";

        return false;
    }

    const uint64_t count = static_cast<uint64_t>(data.GetWidth()) * data.GetHeight();

    if (count > NavGridAssetLoader::MaximumCellCount || count != data.GetCells().size())
    {
        error = "NavGrid cell count is invalid.";

        return false;
    }

    if (!std::isfinite(data.GetCellSize()) || data.GetCellSize() <= 0.0f)
    {
        error = "CellSize must be finite and positive.";

        return false;
    }

    const Vec3& origin = data.GetOrigin();

    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
    {
        error = "Origin must contain only finite values.";

        return false;
    }

    if (!std::isfinite(data.GetDefaultAgentRadius()) || data.GetDefaultAgentRadius() < 0.0f)
    {
        error = "DefaultAgentRadius must be finite and non-negative.";

        return false;
    }

    for (uint8_t cell : data.GetCells())
    {
        if (cell > static_cast<uint8_t>(NavCellType::Slow))
        {
            error = "CellData contains an unknown V1 cell value.";

            return false;
        }
    }

    error.clear();

    return true;
}

std::vector<uint8_t> BuildCanonical(const EditableNavGridData& data)
{
    std::vector<uint8_t> bytes;
    bytes.reserve(40 + data.GetMapId().size() + data.GetCells().size());
    AppendU32(bytes, NavGridAssetLoader::FormatVersion);
    AppendU32(bytes, static_cast<uint32_t>(data.GetMapId().size()));
    bytes.insert(bytes.end(), data.GetMapId().begin(), data.GetMapId().end());
    AppendU32(bytes, data.GetWidth());
    AppendU32(bytes, data.GetHeight());
    AppendF32(bytes, data.GetCellSize());
    AppendF32(bytes, data.GetOrigin().x);
    AppendF32(bytes, data.GetOrigin().y);
    AppendF32(bytes, data.GetOrigin().z);
    AppendF32(bytes, data.GetDefaultAgentRadius());
    AppendU32(bytes, NavGridAssetLoader::CellEncodingBytePerCell);
    AppendU32(bytes, static_cast<uint32_t>(data.GetCells().size()));
    bytes.insert(bytes.end(), data.GetCells().begin(), data.GetCells().end());

    return bytes;
}
}

bool EditableNavGridData::Create(const std::string& mapId, uint32_t width, uint32_t height, float cellSize,
                                 const Vec3& origin, float defaultAgentRadius, EditableNavGridData& outData,
                                 std::string& outError)
{
    EditableNavGridData candidate;
    candidate._mapId = mapId;
    candidate._width = width;
    candidate._height = height;
    candidate._cellSize = cellSize;
    candidate._origin = origin;
    candidate._defaultAgentRadius = defaultAgentRadius;

    const uint64_t count = static_cast<uint64_t>(width) * height;

    if (count <= NavGridAssetLoader::MaximumCellCount)
        candidate._cells.assign(static_cast<size_t>(count), static_cast<uint8_t>(NavCellType::Walkable));

    if (!Validate(candidate, outError))
        return false;

    outData = std::move(candidate);

    return true;
}

EditableNavGridData EditableNavGridData::FromAsset(const NavGridAsset& asset)
{
    EditableNavGridData data;
    data._mapId = asset.GetMapId();
    data._width = asset.GetWidth();
    data._height = asset.GetHeight();
    data._cellSize = asset.GetCellSize();
    data._origin = asset.GetOrigin();
    data._defaultAgentRadius = asset.GetDefaultAgentRadius();
    data._cells.reserve(static_cast<size_t>(data._width) * data._height);

    for (uint32_t z = 0; z < data._height; ++z)
        for (uint32_t x = 0; x < data._width; ++x)
            data._cells.push_back(static_cast<uint8_t>(asset.GetCell(x, z)));

    return data;
}

bool EditableNavGridData::IsValidCell(int32_t x, int32_t z) const noexcept
{
    return x >= 0 && z >= 0 && static_cast<uint32_t>(x) < _width && static_cast<uint32_t>(z) < _height;
}

NavCellType EditableNavGridData::GetCell(int32_t x, int32_t z) const
{
    if (!IsValidCell(x, z))
        throw std::out_of_range("NavGrid cell coordinate is outside the grid.");

    return static_cast<NavCellType>(_cells[static_cast<size_t>(z) * _width + x]);
}

bool EditableNavGridData::SetCell(int32_t x, int32_t z, NavCellType type) noexcept
{
    if (!IsValidCell(x, z) || type > NavCellType::Slow)
        return false;
    _cells[static_cast<size_t>(z) * _width + x] = static_cast<uint8_t>(type);

    return true;
}

std::optional<NavGridCoordinate> EditableNavGridData::WorldToCell(const Vec3& worldPosition) const noexcept
{
    if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y) || !std::isfinite(worldPosition.z))
        return std::nullopt;
    const double x = std::floor((static_cast<double>(worldPosition.x) - _origin.x) / _cellSize);
    const double z = std::floor((static_cast<double>(worldPosition.z) - _origin.z) / _cellSize);

    if (x < 0.0 || z < 0.0 || x >= _width || z >= _height)
        return std::nullopt;

    return NavGridCoordinate{static_cast<int32_t>(x), static_cast<int32_t>(z)};
}

std::optional<Vec3> EditableNavGridData::CellToWorldCenter(const NavGridCoordinate& cell) const noexcept
{
    if (!IsValidCell(cell.x, cell.z))
        return std::nullopt;

    return Vec3(_origin.x + (cell.x + 0.5f) * _cellSize, _origin.y, _origin.z + (cell.z + 0.5f) * _cellSize);
}

bool NavGridAssetWriter::ComputeContentHash(const EditableNavGridData& data, std::array<uint8_t, 32>& outHash,
                                            std::string& outError)
{
    if (!Validate(data, outError))
        return false;
    const std::vector<uint8_t> canonical = BuildCanonical(data);

    return NavGridHash::ComputeSha256(canonical.data(), canonical.size(), outHash, outError);
}

bool NavGridAssetWriter::WriteAtomic(const std::wstring& path, const EditableNavGridData& data,
                                     std::array<uint8_t, 32>& outHash, std::string& outError)
{
    if (path.empty())
    {
        outError = "NavGrid output path is empty.";

        return false;
    }

    if (!ComputeContentHash(data, outHash, outError))
        return false;

    std::vector<uint8_t> bytes;
    bytes.reserve(FixedHeaderBytes + data.GetMapId().size() + data.GetCells().size());
    bytes.insert(bytes.end(), std::begin(Magic), std::end(Magic));
    AppendU32(bytes, NavGridAssetLoader::FormatVersion);
    AppendU32(bytes, FixedHeaderBytes + static_cast<uint32_t>(data.GetMapId().size()));
    AppendU32(bytes, static_cast<uint32_t>(data.GetMapId().size()));
    bytes.insert(bytes.end(), data.GetMapId().begin(), data.GetMapId().end());
    AppendU32(bytes, data.GetWidth());
    AppendU32(bytes, data.GetHeight());
    AppendF32(bytes, data.GetCellSize());
    AppendF32(bytes, data.GetOrigin().x);
    AppendF32(bytes, data.GetOrigin().y);
    AppendF32(bytes, data.GetOrigin().z);
    AppendF32(bytes, data.GetDefaultAgentRadius());
    AppendU32(bytes, NavGridAssetLoader::CellEncodingBytePerCell);
    AppendU32(bytes, static_cast<uint32_t>(data.GetCells().size()));
    bytes.insert(bytes.end(), outHash.begin(), outHash.end());
    bytes.insert(bytes.end(), data.GetCells().begin(), data.GetCells().end());

    const std::filesystem::path target(path);
    std::error_code ec;

    if (!target.parent_path().empty())
        std::filesystem::create_directories(target.parent_path(), ec);

    if (ec)
    {
        outError = "Cannot create the NavGrid output directory.";

        return false;
    }

    const std::filesystem::path temporary = target.wstring() + L".tmp." + std::to_wstring(GetCurrentProcessId());
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);

        if (!output)
        {
            outError = "Cannot create the temporary NavGrid file.";

            return false;
        }

        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.flush();

        if (!output)
        {
            output.close();
            std::filesystem::remove(temporary, ec);
            outError = "Cannot flush the temporary NavGrid file.";

            return false;
        }
    }

    NavGridAsset verified;
    const NavGridLoadStatus status = NavGridAssetLoader::Load(temporary.wstring(), verified);
    bool matches = status && verified.GetMapId() == data.GetMapId() && verified.GetWidth() == data.GetWidth() &&
                   verified.GetHeight() == data.GetHeight() && verified.GetCellSize() == data.GetCellSize() &&
                   verified.GetOrigin() == data.GetOrigin() &&
                   verified.GetDefaultAgentRadius() == data.GetDefaultAgentRadius() &&
                   verified.GetContentHash() == outHash;

    if (matches)
    {
        for (uint32_t z = 0; z < data.GetHeight() && matches; ++z)
            for (uint32_t x = 0; x < data.GetWidth(); ++x)
                if (verified.GetCell(x, z) != data.GetCell(x, z))
                {
                    matches = false;
                    break;
                }
    }

    if (!matches)
    {
        std::filesystem::remove(temporary, ec);
        outError = status ? "Saved NavGrid verification did not match the editable data." : status.message;

        return false;
    }

    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::filesystem::remove(temporary, ec);
        outError = "Cannot atomically replace the NavGrid asset (Win32 error " + std::to_string(GetLastError()) + ").";

        return false;
    }

    outError.clear();

    return true;
}
