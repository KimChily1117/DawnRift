#include "pch.h"
#include "Mesh.h"
#include "GeometryHelper.h"
#include <fstream>
#include <filesystem>

bool Mesh::LoadVfxMesh(const wstring& path)
{
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    uint32 header[5] = {};
    file.read(reinterpret_cast<char*>(header), sizeof(header));

    if (!file || header[0] != 0x4d584656 || header[1] != 1 || header[2] != sizeof(VertexTextureNormalTangentData) ||
        header[3] == 0 || header[3] > 65536 || header[4] == 0 || header[4] > 196608 || header[4] % 3 != 0)
        return false;
    vector<VertexTextureNormalTangentData> vertices(header[3]);
    vector<uint32> indices(header[4]);
    file.read(reinterpret_cast<char*>(vertices.data()), vertices.size() * sizeof(vertices[0]));
    file.read(reinterpret_cast<char*>(indices.data()), indices.size() * sizeof(indices[0]));

    if (!file || file.peek() != std::char_traits<char>::eof())
        return false;

    for (const auto& vertex : vertices)
        if (!std::isfinite(vertex.position.LengthSquared()) || !std::isfinite(vertex.uv.LengthSquared()))
            return false;

    for (uint32 index : indices)
        if (index >= vertices.size())
            return false;
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    _geometry->SetVertices(vertices);
    _geometry->SetIndices(indices);
    CreateBuffers();

    return true;
}

Mesh::Mesh() : Super(ResourceType::Mesh) {}

Mesh::~Mesh() {}

void Mesh::CreateQuad()
{
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    GeometryHelper::CreateQuad(_geometry);
    CreateBuffers();
}

void Mesh::CreateParticleQuad()
{
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    GeometryHelper::CreateQuadFacingZ(_geometry);
    CreateBuffers();
}

void Mesh::CreateCube()
{
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    GeometryHelper::CreateCube(_geometry);
    CreateBuffers();
}

void Mesh::CreateGrid(int32 sizeX, int32 sizeZ)
{
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    GeometryHelper::CreateGrid(_geometry, sizeX, sizeZ);
    CreateBuffers();
}

void Mesh::CreateSphere()
{
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    GeometryHelper::CreateSphere(_geometry);
    CreateBuffers();
}

void Mesh::CreateDynamic(const vector<VertexTextureNormalTangentData>& vertices, const vector<uint32>& indices)
{
    _geometry = make_shared<Geometry<VertexTextureNormalTangentData>>();
    _geometry->SetVertices(vertices);
    _geometry->SetIndices(indices);
    _vertexBuffer = make_shared<VertexBuffer>();
    _vertexBuffer->Create(vertices, 0, true);
    _indexBuffer = make_shared<IndexBuffer>();
    _indexBuffer->Create(indices);
}

bool Mesh::UpdateDynamicVertices(const vector<VertexTextureNormalTangentData>& vertices)
{
    if (!_vertexBuffer || vertices.size() != _vertexBuffer->GetCount())
        return false;
    _vertexBuffer->Update(vertices);

    return true;
}

bool Mesh::UpdateDynamicVertices(const vector<VertexTextureNormalTangentData>& vertices, uint32 vertexCount)
{
    if (!_vertexBuffer || vertices.size() != _vertexBuffer->GetCount())
        return false;
    vertexCount = min<uint32>(vertexCount, static_cast<uint32>(vertices.size()));
    _vertexBuffer->Update(vertices, vertexCount);
    _drawIndexCount = min<uint32>(vertexCount / 4 * 6, _indexBuffer->GetCount());

    return true;
}

uint32 Mesh::GetDrawIndexCount() const
{
    return _drawIndexCount == UINT32_MAX ? _indexBuffer->GetCount() : _drawIndexCount;
}

void Mesh::CreateBuffers()
{
    _vertexBuffer = make_shared<VertexBuffer>();
    _vertexBuffer->Create(_geometry->GetVertices());
    _indexBuffer = make_shared<IndexBuffer>();
    _indexBuffer->Create(_geometry->GetIndices());
}
