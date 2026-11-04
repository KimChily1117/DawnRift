#pragma once

struct ModelBone
{
    wstring name;
    int32 index;
    int32 parentIndex;
    shared_ptr<ModelBone> parent;

    Matrix transform;
    vector<shared_ptr<ModelBone>> children;
};

struct ModelMesh
{
    void CreateBuffers();

    wstring name;

    // Mesh
    shared_ptr<Geometry<ModelVertexType>> geometry = make_shared<Geometry<ModelVertexType>>();
    shared_ptr<VertexBuffer> vertexBuffer;
    shared_ptr<IndexBuffer> indexBuffer;
    BoundingBox bounds;

    // Material
    wstring materialName = L"";
    shared_ptr<Material> material;

    // Bones
    int32 boneIndex;
    shared_ptr<ModelBone> bone;

    // Added by yeop
    Matrix transformMatrix;
};
