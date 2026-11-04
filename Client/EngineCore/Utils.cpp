#include "pch.h"

bool Utils::StartsWith(string str, string comp)
{
    wstring::size_type index = str.find(comp);

    if (index != wstring::npos && index == 0)
        return true;

    return false;
}

bool Utils::StartsWith(wstring str, wstring comp)
{
    wstring::size_type index = str.find(comp);

    if (index != wstring::npos && index == 0)
        return true;

    return false;
}

void Utils::Replace(OUT string& str, string comp, string rep)
{
    string temp = str;

    size_t start_pos = 0;

    while ((start_pos = temp.find(comp, start_pos)) != wstring::npos)
    {
        temp.replace(start_pos, comp.length(), rep);
        start_pos += rep.length();
    }

    str = temp;
}

void Utils::Replace(OUT wstring& str, wstring comp, wstring rep)
{
    wstring temp = str;

    size_t start_pos = 0;

    while ((start_pos = temp.find(comp, start_pos)) != wstring::npos)
    {
        temp.replace(start_pos, comp.length(), rep);
        start_pos += rep.length();
    }

    str = temp;
}

std::wstring Utils::ToWString(string value)
{
    return wstring(value.begin(), value.end());
}

std::string Utils::ToString(wstring value)
{
    return string(value.begin(), value.end());
}

int Utils::GetRandomNumber(int min, int max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(min, max);

    return dist(gen);
}

int Utils::Random(const int& min, const int& max)
{
    return rand() % (max - min) + min;
}

float Utils::Random(const float& min, const float& max)
{
    float normal = rand() / (float)RAND_MAX;

    return (max - min) * normal + min;
}

Vec2 Utils::Random(const Vec2& min, const Vec2& max)
{
    return Vec2(Random(min.x, max.x), Random(min.y, max.y));
}

Vec3 Utils::Random(const Vec3& min, const Vec3& max)
{
    return Vec3(Random(min.x, max.x), Random(min.y, max.y), Random(min.z, max.z));
}

Color Utils::LerpColor(const Color& a, const Color& b, float t)
{
    XMVECTOR va = XMLoadFloat4(&a);
    XMVECTOR vb = XMLoadFloat4(&b);
    XMVECTOR result = XMVectorLerp(va, vb, t);

    Color outColor;
    XMStoreFloat4(&outColor, result);

    return outColor;
}

wstring Utils::GetFileNameWithoutExtension(const wstring& path)
{
    size_t lastSlash = path.find_last_of(L"\\/");
    size_t lastDot = path.find_last_of(L'.');

    if (lastDot == wstring::npos || lastDot < lastSlash)
        lastDot = path.length();

    return path.substr(lastSlash + 1, lastDot - lastSlash - 1);
}

float MathUtils::Random(float r1, float r2)
{
    float random = ((float)rand()) / (float)RAND_MAX;
    float diff = r2 - r1;
    float val = random * diff;

    return r1 + val;
}

Vec2 MathUtils::RandomVec2(float r1, float r2)
{
    Vec2 result;
    result.x = Random(r1, r2);
    result.y = Random(r1, r2);

    return result;
}

Vec3 MathUtils::RandomVec3(float r1, float r2)
{
    Vec3 result;
    result.x = Random(r1, r2);
    result.y = Random(r1, r2);
    result.z = Random(r1, r2);

    return result;
}
