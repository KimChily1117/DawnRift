#include "pch.h"
#include "Terrain.h"
#include "MeshRenderer.h"
#include "Camera.h"

Terrain::Terrain() : Component(ComponentType::Terrain) {}

Terrain::~Terrain() {}

void Terrain::Create(int32 sizeX, int32 sizeZ, shared_ptr<Material> material)
{
    _sizeX = sizeX;
    _sizeZ = sizeZ;

    auto go = _gameObject.lock();

    go->GetOrAddTransform();

    if (go->GetMeshRenderer() == nullptr)
        go->AddComponent(make_shared<MeshRenderer>());

    _mesh = make_shared<Mesh>();
    _mesh->CreateGrid(sizeX, sizeZ);

    go->GetMeshRenderer()->SetMesh(_mesh);
    go->GetMeshRenderer()->SetPass(0);
    go->GetMeshRenderer()->SetMaterial(material);

    // Create Tilemap
    if (!_tilemap)
    {
        _tilemap = make_shared<Tilemap>();
        _tilemap->SetMapSize(Vec3(sizeX, 0, sizeZ));
        _tilemap->SetTileSize(1);
        _tilemap->Save(L"..\\Resources\\MapData\\TilemapData.txt");
    }
}

bool Terrain::Pick(const Ray& worldRay, Vec3& pickPos, float& distance)
{
    if (!_tilemap || _sizeX <= 0 || _sizeZ <= 0)
        return false;

    const Matrix world = GetTransform()->GetWorldMatrix();
    const Matrix inverseWorld = world.Invert();

    const Vec3 localOrigin = XMVector3TransformCoord(worldRay.position, inverseWorld);
    Vec3 localDirection = XMVector3TransformNormal(worldRay.direction, inverseWorld);

    if (localDirection.LengthSquared() <= FLT_EPSILON)
        return false;

    localDirection.Normalize();
    const Ray localRay(localOrigin, localDirection);
    const DirectX::SimpleMath::Plane terrainPlane(Vec3::UnitY, 0.f);

    float localDistance = 0.f;

    if (!localRay.Intersects(terrainPlane, OUT localDistance))
        return false;

    const Vec3 localPickPosition = localRay.position + localRay.direction * localDistance;

    if (localPickPosition.x < 0.f || localPickPosition.z < 0.f || localPickPosition.x >= static_cast<float>(_sizeX) ||
        localPickPosition.z >= static_cast<float>(_sizeZ))
    {
        return false;
    }

    pickPos = XMVector3TransformCoord(localPickPosition, world);
    distance = (pickPos - worldRay.position).Length();

    return true;
}

Vec3 Terrain::GetTileCorrectedPosition(Vec3 pickPos)
{
    Vec3 tilePos = _tilemap->ConvertWorldToTile(pickPos);
    Tile* tile = _tilemap->GetTileAt(tilePos);

    if (!tile)
        return pickPos;

    // ?�� ?�당 ?�?�의 ??개의 �?��??좌표 가?�오�?
    int x = static_cast<int>(tilePos.x);
    int z = static_cast<int>(tilePos.z);

    Vec3 p0 = _tilemap->GetTileAt(Vec3(x, 0, z))->position;
    Vec3 p1 = _tilemap->GetTileAt(Vec3(x + 1, 0, z))->position;
    Vec3 p2 = _tilemap->GetTileAt(Vec3(x, 0, z + 1))->position;
    Vec3 p3 = _tilemap->GetTileAt(Vec3(x + 1, 0, z + 1))->position;

    // ?�� ?�각???�별 (Barycentric 좌표�??�용)
    Vec3 barycentric;

    if (IsPointInTriangle(pickPos, p0, p1, p2, barycentric))
    {
        return BarycentricInterpolation(p0, p1, p2, barycentric);
    }
    else if (IsPointInTriangle(pickPos, p3, p1, p2, barycentric))
    {
        return BarycentricInterpolation(p3, p1, p2, barycentric);
    }

    return pickPos;
}

bool Terrain::IsPointInTriangle(const Vec3& P, const Vec3& A, const Vec3& B, const Vec3& C, Vec3& outBarycentric)
{
    Vec3 v0 = B - A;
    Vec3 v1 = C - A;
    Vec3 v2 = P - A;

    float d00 = v0.Dot(v0);
    float d01 = v0.Dot(v1);
    float d11 = v1.Dot(v1);
    float d20 = v2.Dot(v0);
    float d21 = v2.Dot(v1);

    float denom = d00 * d11 - d01 * d01;

    if (denom == 0.0f)
        return false;

    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    outBarycentric = Vec3(u, v, w);

    return (u >= 0 && v >= 0 && w >= 0);
}

Vec3 Terrain::BarycentricInterpolation(const Vec3& A, const Vec3& B, const Vec3& C, const Vec3& barycentric)
{
    return A * barycentric.x + B * barycentric.y + C * barycentric.z;
}
