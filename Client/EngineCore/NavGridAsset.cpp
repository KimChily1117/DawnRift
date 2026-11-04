// 내비게이션 그리드 데이터.
#include "NavGridAsset.h"

#include <bcrypt.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

#pragma comment(lib, "bcrypt.lib")

namespace
{
constexpr uint8_t Magic[4] = {'N', 'V', 'G', '1'};
constexpr uint64_t FixedHeaderBytes = 84;
constexpr uint64_t MaximumFileBytes =
    FixedHeaderBytes + NavGridAssetLoader::MaximumMapIdBytes + NavGridAssetLoader::MaximumCellCount;

NavGridLoadStatus Fail(NavGridLoadError error, std::string message)
{
    return {error, std::move(message)};
}

bool ReadU32(const std::vector<uint8_t>& bytes, size_t& cursor, uint32_t& value)
{
    if (cursor > bytes.size() || bytes.size() - cursor < sizeof(uint32_t))
        return false;

    value = static_cast<uint32_t>(bytes[cursor]) | (static_cast<uint32_t>(bytes[cursor + 1]) << 8) |
            (static_cast<uint32_t>(bytes[cursor + 2]) << 16) | (static_cast<uint32_t>(bytes[cursor + 3]) << 24);
    cursor += sizeof(uint32_t);

    return true;
}

bool ReadF32(const std::vector<uint8_t>& bytes, size_t& cursor, float& value)
{
    uint32_t bits = 0;

    if (!ReadU32(bytes, cursor, bits))
        return false;
    value = std::bit_cast<float>(bits);

    return true;
}

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

bool IsValidUtf8(const uint8_t* bytes, size_t size)
{
    if (size == 0)
        return true;

    if (size > static_cast<size_t>((std::numeric_limits<int>::max)()))
        return false;

    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<const char*>(bytes),
                               static_cast<int>(size), nullptr, 0) > 0;
}

bool ConstantTimeEqual(const std::array<uint8_t, 32>& left, const std::array<uint8_t, 32>& right)
{
    uint8_t difference = 0;

    for (size_t i = 0; i < left.size(); ++i)
        difference |= left[i] ^ right[i];

    return difference == 0;
}
}

std::string NavGridAsset::GetContentHashHex() const
{
    return NavGridHash::ToHex(_contentHash);
}

bool NavGridAsset::IsValidCell(int32_t x, int32_t z) const noexcept
{
    return x >= 0 && z >= 0 && static_cast<uint32_t>(x) < _width && static_cast<uint32_t>(z) < _height;
}

NavCellType NavGridAsset::GetCell(int32_t x, int32_t z) const
{
    if (!IsValidCell(x, z))
        throw std::out_of_range("NavGrid cell coordinate is outside the grid.");

    const size_t index = static_cast<size_t>(z) * static_cast<size_t>(_width) + static_cast<size_t>(x);

    return static_cast<NavCellType>(_cells[index]);
}

std::optional<NavGridCoordinate> NavGridAsset::WorldToCell(const Vec3& worldPosition) const noexcept
{
    if (!std::isfinite(worldPosition.x) || !std::isfinite(worldPosition.y) || !std::isfinite(worldPosition.z))
    {
        return std::nullopt;
    }

    const double cellX = std::floor((static_cast<double>(worldPosition.x) - static_cast<double>(_origin.x)) /
                                    static_cast<double>(_cellSize));
    const double cellZ = std::floor((static_cast<double>(worldPosition.z) - static_cast<double>(_origin.z)) /
                                    static_cast<double>(_cellSize));

    if (cellX < 0.0 || cellZ < 0.0 || cellX >= static_cast<double>(_width) || cellZ >= static_cast<double>(_height))
    {
        return std::nullopt;
    }

    return NavGridCoordinate{static_cast<int32_t>(cellX), static_cast<int32_t>(cellZ)};
}

std::optional<Vec3> NavGridAsset::CellToWorldCenter(const NavGridCoordinate& cell) const noexcept
{
    if (!IsValidCell(cell.x, cell.z))
        return std::nullopt;

    return Vec3(_origin.x + (static_cast<float>(cell.x) + 0.5f) * _cellSize, _origin.y,
                _origin.z + (static_cast<float>(cell.z) + 0.5f) * _cellSize);
}

bool NavGridHash::ComputeSha256(const uint8_t* data, size_t size, std::array<uint8_t, 32>& outHash,
                                std::string& outError)
{
    if (size > static_cast<size_t>((std::numeric_limits<ULONG>::max)()))
    {
        outError = "SHA-256 input exceeds the Windows CNG single-call limit.";

        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<uint8_t> hashObject;

    NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);

    if (status < 0)
    {
        outError = "BCryptOpenAlgorithmProvider failed.";

        return false;
    }

    DWORD objectSize = 0;
    DWORD resultSize = 0;
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                               sizeof(objectSize), &resultSize, 0);

    if (status >= 0)
    {
        hashObject.resize(objectSize);
        status = BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0);
    }

    if (status >= 0 && size != 0)
    {
        status = BCryptHashData(hash, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0);
    }

    if (status >= 0)
    {
        status = BCryptFinishHash(hash, outHash.data(), static_cast<ULONG>(outHash.size()), 0);
    }

    if (hash != nullptr)
        BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (status < 0)
    {
        outError = "Windows CNG SHA-256 operation failed.";

        return false;
    }

    outError.clear();

    return true;
}

std::string NavGridHash::ToHex(const std::array<uint8_t, 32>& hash)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');

    for (uint8_t value : hash)
        stream << std::setw(2) << static_cast<unsigned int>(value);

    return stream.str();
}

NavGridLoadStatus NavGridAssetLoader::Load(const std::wstring& path, NavGridAsset& outAsset)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file)
        return Fail(NavGridLoadError::FileOpenFailed, "Cannot open NavGrid asset.");

    const std::streamoff streamSize = file.tellg();

    if (streamSize < 0 || static_cast<uint64_t>(streamSize) > MaximumFileBytes)
        return Fail(NavGridLoadError::FileTooLarge, "NavGrid asset exceeds the V1 size limit.");

    std::vector<uint8_t> bytes(static_cast<size_t>(streamSize));
    file.seekg(0, std::ios::beg);

    if (!bytes.empty() && !file.read(reinterpret_cast<char*>(bytes.data()), streamSize))
    {
        return Fail(NavGridLoadError::TruncatedCellData, "Cannot read the complete NavGrid asset.");
    }

    if (bytes.size() < 16)
        return Fail(NavGridLoadError::TruncatedHeader, "NavGrid header is truncated.");

    if (!std::equal(std::begin(Magic), std::end(Magic), bytes.begin()))
        return Fail(NavGridLoadError::InvalidMagic, "NavGrid magic must be NVG1.");

    size_t cursor = 4;
    uint32_t version = 0;
    uint32_t headerSize = 0;
    uint32_t mapIdLength = 0;

    if (!ReadU32(bytes, cursor, version) || !ReadU32(bytes, cursor, headerSize) || !ReadU32(bytes, cursor, mapIdLength))
    {
        return Fail(NavGridLoadError::TruncatedHeader, "NavGrid fixed header is truncated.");
    }

    if (version != FormatVersion)
        return Fail(NavGridLoadError::UnsupportedVersion, "Unsupported NavGrid format version.");

    if (mapIdLength > MaximumMapIdBytes)
        return Fail(NavGridLoadError::MapIdTooLong, "NavigationMapId exceeds 128 UTF-8 bytes.");

    const uint64_t expectedHeaderSize = FixedHeaderBytes + mapIdLength;

    if (headerSize != expectedHeaderSize)
        return Fail(NavGridLoadError::InvalidHeaderSize, "HeaderSize does not match the V1 layout.");

    if (bytes.size() < expectedHeaderSize)
        return Fail(NavGridLoadError::TruncatedHeader, "NavGrid variable header is truncated.");

    if (!IsValidUtf8(bytes.data() + cursor, mapIdLength))
        return Fail(NavGridLoadError::InvalidUtf8MapId, "NavigationMapId is not valid UTF-8.");

    std::string mapId(reinterpret_cast<const char*>(bytes.data() + cursor), mapIdLength);
    cursor += mapIdLength;

    uint32_t width = 0;
    uint32_t height = 0;
    float cellSize = 0.0f;
    Vec3 origin = Vec3(0.0f, 0.0f, 0.0f);
    float agentRadius = 0.0f;
    uint32_t cellEncoding = 0;
    uint32_t cellDataLength = 0;

    if (!ReadU32(bytes, cursor, width) || !ReadU32(bytes, cursor, height) || !ReadF32(bytes, cursor, cellSize) ||
        !ReadF32(bytes, cursor, origin.x) || !ReadF32(bytes, cursor, origin.y) || !ReadF32(bytes, cursor, origin.z) ||
        !ReadF32(bytes, cursor, agentRadius) || !ReadU32(bytes, cursor, cellEncoding) ||
        !ReadU32(bytes, cursor, cellDataLength))
    {
        return Fail(NavGridLoadError::TruncatedHeader, "NavGrid metadata is truncated.");
    }

    if (width == 0 || height == 0 || width > MaximumDimension || height > MaximumDimension)
    {
        return Fail(NavGridLoadError::InvalidDimensions, "NavGrid dimensions are zero or exceed 4096.");
    }

    const uint64_t cellCount = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);

    if (cellCount > MaximumCellCount)
        return Fail(NavGridLoadError::CellCountTooLarge, "NavGrid cell count exceeds the V1 limit.");

    if (!std::isfinite(cellSize) || cellSize <= 0.0f)
        return Fail(NavGridLoadError::InvalidCellSize, "CellSize must be finite and positive.");

    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
    {
        return Fail(NavGridLoadError::InvalidOrigin, "Origin must contain only finite values.");
    }

    if (!std::isfinite(agentRadius) || agentRadius < 0.0f)
        return Fail(NavGridLoadError::InvalidAgentRadius, "DefaultAgentRadius must be finite and non-negative.");

    if (cellEncoding != CellEncodingBytePerCell)
        return Fail(NavGridLoadError::UnsupportedCellEncoding, "Unsupported NavGrid cell encoding.");

    if (cellDataLength != cellCount)
        return Fail(NavGridLoadError::InvalidCellDataLength,
                    "CellDataLength must equal Width * Height for encoding 1.");

    std::array<uint8_t, 32> storedHash{};

    if (cursor > bytes.size() || bytes.size() - cursor < storedHash.size())
        return Fail(NavGridLoadError::TruncatedHeader, "ContentHash is truncated.");
    std::copy_n(bytes.data() + cursor, storedHash.size(), storedHash.begin());
    cursor += storedHash.size();

    if (cursor != headerSize)
        return Fail(NavGridLoadError::InvalidHeaderSize, "HeaderSize does not end after ContentHash.");

    if (bytes.size() < cursor + cellDataLength)
        return Fail(NavGridLoadError::TruncatedCellData, "CellData is truncated.");

    if (bytes.size() > cursor + cellDataLength)
        return Fail(NavGridLoadError::TrailingData, "V1 rejects bytes after CellData.");

    std::vector<uint8_t> cells(bytes.begin() + static_cast<ptrdiff_t>(cursor), bytes.end());

    for (uint8_t cell : cells)
    {
        if (cell > static_cast<uint8_t>(NavCellType::Slow))
            return Fail(NavGridLoadError::UnknownCellValue, "CellData contains an unknown V1 cell value.");
    }

    std::vector<uint8_t> canonical;
    canonical.reserve(40 + mapId.size() + cells.size());
    AppendU32(canonical, version);
    AppendU32(canonical, mapIdLength);
    canonical.insert(canonical.end(), mapId.begin(), mapId.end());
    AppendU32(canonical, width);
    AppendU32(canonical, height);
    AppendF32(canonical, cellSize);
    AppendF32(canonical, origin.x);
    AppendF32(canonical, origin.y);
    AppendF32(canonical, origin.z);
    AppendF32(canonical, agentRadius);
    AppendU32(canonical, cellEncoding);
    AppendU32(canonical, cellDataLength);
    canonical.insert(canonical.end(), cells.begin(), cells.end());

    std::array<uint8_t, 32> computedHash{};
    std::string hashError;

    if (!NavGridHash::ComputeSha256(canonical.data(), canonical.size(), computedHash, hashError))
    {
        return Fail(NavGridLoadError::HashFailure, std::move(hashError));
    }

    if (!ConstantTimeEqual(storedHash, computedHash))
        return Fail(NavGridLoadError::HashMismatch, "NavGrid content SHA-256 does not match.");

    NavGridAsset loaded;
    loaded._formatVersion = version;
    loaded._mapId = std::move(mapId);
    loaded._width = width;
    loaded._height = height;
    loaded._cellSize = cellSize;
    loaded._origin = origin;
    loaded._defaultAgentRadius = agentRadius;
    loaded._cellEncoding = cellEncoding;
    loaded._contentHash = storedHash;
    loaded._cells = std::move(cells);
    outAsset = std::move(loaded);

    return {};
}
