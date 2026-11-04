#include "pch.h"
#include "NavigationGridComponent.h"

#include <array>
#include <fstream>

NavigationGridComponent::NavigationGridComponent() {}

bool NavigationGridComponent::IsRelativeAssetPath(const std::filesystem::path& path)
{
    return !path.empty() && !path.is_absolute() && !path.has_root_name() && !path.has_root_directory();
}

bool NavigationGridComponent::CreateGrid(uint32_t width, uint32_t height, float cellSize, const Vec3& origin,
                                         std::string& outError)
{
    EditableNavGridData candidate;

    if (!EditableNavGridData::Create(_navigationMapId, width, height, cellSize, origin, 0.5f, candidate, outError))
    {
        return false;
    }

    _data = std::move(candidate);
    _hasGrid = true;
    _dirty = true;
    _contentHashHex.clear();
    _lastStatus = "Created an all-Walkable grid. Save is required.";

    return true;
}

bool NavigationGridComponent::LoadNavGrid(const std::filesystem::path& path, std::string& outError)
{
    if (!IsRelativeAssetPath(path))
    {
        outError = "Navigation Asset Path must be relative to the client working directory.";

        return false;
    }

    NavGridAsset loaded;
    const NavGridLoadStatus status = NavGridAssetLoader::Load(path.wstring(), loaded);

    if (!status)
    {
        outError = status.message;

        return false;
    }

    EditableNavGridData candidate = EditableNavGridData::FromAsset(loaded);
    _data = std::move(candidate);
    _hasGrid = true;
    _dirty = false;
    _navigationMapId = loaded.GetMapId();
    _assetPath = path.generic_string();
    _contentHashHex = loaded.GetContentHashHex();
    _lastStatus = "Loaded " + _assetPath;
    outError.clear();

    return true;
}

bool NavigationGridComponent::SaveNavGrid(const std::filesystem::path& path, std::string& outError)
{
    if (!_hasGrid)
    {
        outError = "No Navigation Grid is loaded.";

        return false;
    }

    if (!IsRelativeAssetPath(path))
    {
        outError = "Navigation Asset Path must be relative to the client working directory.";

        return false;
    }

    _data.SetMapId(_navigationMapId);
    std::array<uint8_t, 32> hash{};

    if (!NavGridAssetWriter::WriteAtomic(path.wstring(), _data, hash, outError))
        return false;

    _assetPath = path.generic_string();
    UpdateSavedHash(hash);
    _dirty = false;
    _lastStatus = "Saved and reloaded-verified " + _assetPath;

    std::string settingsError;
    SaveSettings(path.wstring() + L".component.json", settingsError);

    if (!settingsError.empty())
        _lastStatus += " (settings warning: " + settingsError + ")";

    return true;
}

bool NavigationGridComponent::Reload(std::string& outError)
{
    if (_assetPath.empty())
    {
        outError = "Navigation Asset Path is empty.";

        return false;
    }

    return LoadNavGrid(std::filesystem::path(_assetPath), outError);
}

bool NavigationGridComponent::ExportServerContent(const std::filesystem::path& path, std::string& outError)
{
    if (!_hasGrid)
    {
        outError = "No Navigation Grid is loaded.";

        return false;
    }

    if (!IsRelativeAssetPath(path))
    {
        outError = "Server export path must be a configured relative path.";

        return false;
    }

    std::array<uint8_t, 32> hash{};

    if (!NavGridAssetWriter::WriteAtomic(path.wstring(), _data, hash, outError))
        return false;
    _serverExportPath = path.generic_string();
    _lastStatus = "Exported server content: " + _serverExportPath + " (Hash: " + NavGridHash::ToHex(hash) + ")";

    return true;
}

bool NavigationGridComponent::SaveSettings(const std::filesystem::path& path, std::string& outError) const
{
    if (!IsRelativeAssetPath(path))
    {
        outError = "Component settings path must be relative.";

        return false;
    }

    std::error_code ec;

    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path(), ec);

    if (ec)
    {
        outError = "Cannot create component settings directory.";

        return false;
    }

    json value;
    value["navigationMapId"] = _navigationMapId;
    value["assetPath"] = _assetPath;
    value["serverExportPath"] = _serverExportPath;
    value["showGrid"] = showGrid;
    value["showWalkable"] = showWalkable;
    value["showBlocked"] = showBlocked;
    value["showSlow"] = showSlow;
    value["showCellBorders"] = showCellBorders;

    const std::filesystem::path temporary = path.wstring() + L".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);

        if (!output)
        {
            outError = "Cannot write Navigation component settings.";

            return false;
        }

        output << value.dump(2);
        output.flush();

        if (!output)
        {
            std::filesystem::remove(temporary, ec);
            outError = "Cannot flush Navigation component settings.";

            return false;
        }
    }

    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);

    if (ec)
    {
        std::filesystem::remove(temporary, ec);
        outError = "Cannot replace Navigation component settings.";

        return false;
    }

    outError.clear();

    return true;
}

bool NavigationGridComponent::LoadSettings(const std::filesystem::path& path, std::string& outError)
{
    if (!IsRelativeAssetPath(path))
    {
        outError = "Component settings path must be relative.";

        return false;
    }

    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        outError = "Navigation component settings do not exist.";

        return false;
    }

    try
    {
        json value;
        input >> value;
        const std::string assetPath = value.value("assetPath", _assetPath);

        if (!IsRelativeAssetPath(std::filesystem::path(assetPath)))
        {
            outError = "Stored Navigation Asset Path is not relative.";

            return false;
        }

        _navigationMapId = value.value("navigationMapId", _navigationMapId);
        _assetPath = assetPath;
        _serverExportPath = value.value("serverExportPath", std::string());
        showGrid = value.value("showGrid", true);
        showWalkable = value.value("showWalkable", false);
        showBlocked = value.value("showBlocked", true);
        showSlow = value.value("showSlow", true);
        showCellBorders = value.value("showCellBorders", true);
    }
    catch (const std::exception& exception)
    {
        outError = std::string("Invalid Navigation component settings: ") + exception.what();

        return false;
    }

    outError.clear();

    return true;
}

void NavigationGridComponent::SetNavigationMapId(std::string value)
{
    if (_navigationMapId == value)
        return;
    _navigationMapId = std::move(value);

    if (_hasGrid)
    {
        _data.SetMapId(_navigationMapId);
        _dirty = true;
    }
}

bool NavigationGridComponent::SetDefaultAgentRadius(float value, std::string& outError)
{
    if (!std::isfinite(value) || value < 0.0f)
    {
        outError = "DefaultAgentRadius must be finite and non-negative.";

        return false;
    }

    if (!_hasGrid)
    {
        outError = "No Navigation Grid is loaded.";

        return false;
    }

    if (_data.GetDefaultAgentRadius() != value)
    {
        _data.SetDefaultAgentRadius(value);
        _dirty = true;
    }

    outError.clear();

    return true;
}

bool NavigationGridComponent::IsValidCell(int32_t x, int32_t z) const noexcept
{
    return _hasGrid && _data.IsValidCell(x, z);
}

NavCellType NavigationGridComponent::GetCellType(int32_t x, int32_t z) const
{
    return _data.GetCell(x, z);
}

bool NavigationGridComponent::SetCellType(int32_t x, int32_t z, NavCellType type)
{
    if (!_hasGrid || !_data.IsValidCell(x, z) || _data.GetCell(x, z) == type)
        return false;

    if (!_data.SetCell(x, z, type))
        return false;
    _dirty = true;

    return true;
}

std::optional<NavGridCoordinate> NavigationGridComponent::WorldToCell(const Vec3& worldPosition) const noexcept
{
    return _hasGrid ? _data.WorldToCell(worldPosition) : std::nullopt;
}

std::optional<Vec3> NavigationGridComponent::CellToWorldCenter(const NavGridCoordinate& cell) const noexcept
{
    return _hasGrid ? _data.CellToWorldCenter(cell) : std::nullopt;
}

void NavigationGridComponent::UpdateSavedHash(const std::array<uint8_t, 32>& hash)
{
    _contentHashHex = NavGridHash::ToHex(hash);
}
