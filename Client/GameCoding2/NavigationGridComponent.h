#pragma once

#include "MonoBehaviour.h"
#include "NavGridAssetWriter.h"

#include <filesystem>
#include <optional>
#include <string>

class NavigationGridComponent : public MonoBehaviour
{
public:
    NavigationGridComponent();

    bool CreateGrid(uint32_t width, uint32_t height, float cellSize, const Vec3& origin, std::string& outError);
    bool LoadNavGrid(const std::filesystem::path& path, std::string& outError);
    bool SaveNavGrid(const std::filesystem::path& path, std::string& outError);
    bool Reload(std::string& outError);
    bool ExportServerContent(const std::filesystem::path& path, std::string& outError);

    bool SaveSettings(const std::filesystem::path& path, std::string& outError) const;
    bool LoadSettings(const std::filesystem::path& path, std::string& outError);

    bool HasGrid() const noexcept
    {
        return _hasGrid;
    }

    bool IsDirty() const noexcept
    {
        return _dirty;
    }

    void MarkClean() noexcept
    {
        _dirty = false;
    }

    const EditableNavGridData& GetData() const noexcept
    {
        return _data;
    }

    const std::string& GetNavigationMapId() const noexcept
    {
        return _navigationMapId;
    }

    const std::string& GetAssetPath() const noexcept
    {
        return _assetPath;
    }

    const std::string& GetServerExportPath() const noexcept
    {
        return _serverExportPath;
    }

    const std::string& GetContentHashHex() const noexcept
    {
        return _contentHashHex;
    }

    const std::string& GetLastStatus() const noexcept
    {
        return _lastStatus;
    }

    void SetNavigationMapId(std::string value);
    bool SetDefaultAgentRadius(float value, std::string& outError);

    void SetAssetPath(std::string value)
    {
        _assetPath = std::move(value);
    }

    void SetServerExportPath(std::string value)
    {
        _serverExportPath = std::move(value);
    }

    void SetLastStatus(std::string value)
    {
        _lastStatus = std::move(value);
    }

    bool IsValidCell(int32_t x, int32_t z) const noexcept;
    NavCellType GetCellType(int32_t x, int32_t z) const;
    bool SetCellType(int32_t x, int32_t z, NavCellType type);
    std::optional<NavGridCoordinate> WorldToCell(const Vec3& worldPosition) const noexcept;
    std::optional<Vec3> CellToWorldCenter(const NavGridCoordinate& cell) const noexcept;

    bool showGrid = true;
    bool showWalkable = false;
    bool showBlocked = true;
    bool showSlow = true;
    bool showCellBorders = true;

private:
    static bool IsRelativeAssetPath(const std::filesystem::path& path);
    void UpdateSavedHash(const std::array<uint8_t, 32>& hash);

    EditableNavGridData _data;
    bool _hasGrid = false;
    bool _dirty = false;
    std::string _navigationMapId = "room-0-nav-v1";
    std::string _assetPath = "../Resources/Navigation/room-0-nav-v1.navgrid";
    std::string _serverExportPath;
    std::string _contentHashHex;
    std::string _lastStatus;
};
