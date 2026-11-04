#include "pch.h"
#include "Viewport.h"

Viewport::Viewport()
{
    Set(800, 600);
}

Viewport::Viewport(float width, float height, float x, float y, float minDepth, float maxDepth)
{
    Set(width, height, x, y, minDepth, maxDepth);
}

Viewport::~Viewport() {}

void Viewport::RSSetViewport()
{
    DC->RSSetViewports(1, &_vp);
}

void Viewport::Set(float width, float height, float x, float y, float minDepth, float maxDepth)
{
    _vp.TopLeftX = x;
    _vp.TopLeftY = y;
    _vp.Width = width;
    _vp.Height = height;
    _vp.MinDepth = minDepth;
    _vp.MaxDepth = maxDepth;
}

Vec3 Viewport::Project(const Vec3& pos, const Matrix& W, const Matrix& V, const Matrix& P)
{
    Matrix wvp = W * V * P;
    Vec3 p = Vec3::Transform(pos, wvp);

    p.x = (p.x + 1.0f) * (_vp.Width / 2) + _vp.TopLeftX;
    p.y = (-p.y + 1.0f) * (_vp.Height / 2) + _vp.TopLeftY;
    p.z = p.z * (_vp.MaxDepth - _vp.MinDepth) + _vp.MinDepth;

    return p;
}

Vec3 Viewport::Unproject(const Vec3& pos, const Matrix& W, const Matrix& V, const Matrix& P)
{
    Vec3 p = pos;

    p.x = 2.f * (p.x - _vp.TopLeftX) / _vp.Width - 1.f;
    p.y = -2.f * (p.y - _vp.TopLeftY) / _vp.Height + 1.f;
    p.z = ((p.z - _vp.MinDepth) / (_vp.MaxDepth - _vp.MinDepth));

    Matrix wvp = W * V * P;
    Matrix wvpInv = wvp.Invert();

    p = Vec3::Transform(p, wvpInv);

    return p;
}
