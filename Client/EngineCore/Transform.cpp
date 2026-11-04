#include "pch.h"
#include "Transform.h"

Transform::Transform() : Super(ComponentType::Transform) {}

Transform::~Transform() {}

void Transform::Awake() {}

void Transform::Update() {}

Vec3 Transform::ToEulerAngles(Quaternion q)
{
    Vec3 angles;

    // Roll (X축 회전)
    double sinr_cosp = 2 * (q.w * q.x + q.y * q.z);

    double cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
    angles.x = XMConvertToDegrees(std::atan2(sinr_cosp, cosr_cosp));

    // Pitch (Y축 회전)
    double sinp = 2 * (q.w * q.y - q.z * q.x);

    if (std::abs(sinp) >= 1)
        angles.y = XMConvertToDegrees(std::copysign(XM_PIDIV2, sinp));
    else
        angles.y = XMConvertToDegrees(std::asin(sinp));

    // Yaw (Z축 회전)
    double siny_cosp = 2 * (q.w * q.z + q.x * q.y);

    double cosy_cosp = 1 - 2 * (q.y * q.y + q.z * q.z);
    angles.z = XMConvertToDegrees(std::atan2(siny_cosp, cosy_cosp));

    if (angles.x > 90.0f)
    {
        angles.x = 180.0f - angles.x;
    }
    else if (angles.x < -90.0f)
    {
        angles.x = -180.0f - angles.x;
    }

    return angles;
}

void Transform::UpdateTransform()
{
    Matrix matScale = Matrix::CreateScale(_localScale);
    Matrix matRotation = Matrix::CreateRotationX(_localRotation.x);
    matRotation *= Matrix::CreateRotationY(_localRotation.y);
    matRotation *= Matrix::CreateRotationZ(_localRotation.z);
    Matrix matTranslation = Matrix::CreateTranslation(_localPosition);

    _matLocal = matScale * matRotation * matTranslation;

    if (HasParent())
    {
        _matWorld = _matLocal * _parent->GetWorldMatrix();
    }
    else
    {
        _matWorld = _matLocal;
    }

    Quaternion quat;
    _matWorld.Decompose(_scale, quat, _position);
    _rotation = ToEulerAngles(quat);

    // Children
    for (const shared_ptr<Transform>& child : _children)
        child->UpdateTransform();
}

void Transform::SetScale(const Vec3& worldScale)
{
    if (HasParent())
    {
        Vec3 parentScale = _parent->GetScale();
        Vec3 scale = worldScale;
        scale.x /= parentScale.x;
        scale.y /= parentScale.y;
        scale.z /= parentScale.z;
        SetLocalScale(scale);
    }
    else
    {
        SetLocalScale(worldScale);
    }
}

void Transform::SetRotation(const Vec3& worldRotation)
{
    if (HasParent())
    {
        Matrix inverseMatrix = _parent->GetWorldMatrix().Invert();

        Vec3 rotation;
        rotation.TransformNormal(worldRotation, inverseMatrix);

        SetLocalRotation(rotation);
    }
    else
        SetLocalRotation(worldRotation);
}

void Transform::SetPosition(const Vec3& worldPosition)
{
    if (HasParent())
    {
        Matrix worldToParentLocalMatrix = _parent->GetWorldMatrix().Invert();

        Vec3 position;
        position.Transform(worldPosition, worldToParentLocalMatrix);

        SetLocalPosition(position);
    }
    else
    {
        SetLocalPosition(worldPosition);
    }
}

Matrix Transform::GetFinalWorldMatrix()
{
    if (!HasParent())
        return _matWorld;

    return _matWorld * _parent->GetFinalWorldMatrix();
}
