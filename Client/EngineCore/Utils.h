#pragma once

#include <string>
#include <random>
#include "Types.h"

#ifndef OUT
#define OUT
#endif

using std::string;
using std::wstring;

class Utils
{
public:
    static bool StartsWith(string str, string comp);
    static bool StartsWith(wstring str, wstring comp);

    static void Replace(OUT string& str, string comp, string rep);
    static void Replace(OUT wstring& str, wstring comp, wstring rep);

    static wstring ToWString(string value);
    static string ToString(wstring value);

    static int GetRandomNumber(int min, int max);
    static int Random(const int& min, const int& max);
    static float Random(const float& min, const float& max);
    static Vec2 Random(const Vec2& min, const Vec2& max);
    static Vec3 Random(const Vec3& min, const Vec3& max);

    static Color LerpColor(const Color& a, const Color& b, float t);

    static wstring GetFileNameWithoutExtension(const wstring& path);
};

struct MathUtils
{
    static float Random(float r1, float r2);
    static Vec2 RandomVec2(float r1, float r2);
    static Vec3 RandomVec3(float r1, float r2);
};
