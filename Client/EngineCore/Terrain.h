#pragma once

#include "Component.h"
#include "Mesh.h"
#include "Tilemap.h"

class Terrain : public Component
{
public:
    Terrain();
    virtual ~Terrain();

    void Create(int32 sizeX, int32 sizeZ, shared_ptr<Material> material);
    bool Pick(const Ray& worldRay, Vec3& pickPos, float& distance);

    Vec3 GetTileCorrectedPosition(Vec3 pickPos);

private:
    bool IsPointInTriangle(const Vec3& point, const Vec3& a, const Vec3& b, const Vec3& c, Vec3& outBarycentric);
    Vec3 BarycentricInterpolation(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& barycentric);

private:
    int32 _sizeX = 0;
    int32 _sizeZ = 0;
    shared_ptr<Mesh> _mesh;

public:
    shared_ptr<Tilemap> _tilemap;
};
