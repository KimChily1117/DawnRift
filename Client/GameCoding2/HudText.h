#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"

class Mesh;
class Shader;
class MeshRenderer;
class InstancingBuffer;

class HudText : public MonoBehaviour
{
public:
    enum class Color
    {
        White,
        Gold,
        Gray,
        Red,
        Mana,
        Magic,
        Physical,
        Green,
        Count
    };
    enum class Align
    {
        Left,
        Center,
        Right
    };

    struct Item
    {
        string text;
        Vec2 position;
        float size = 16.f;
        Color color = Color::White;
        Align align = Align::Center;
        float alpha = 1.f;
    };

    static shared_ptr<HudText> Get();
    void Draw(const string& text, const Vec2& position, float size, Color color = Color::White,
              Align align = Align::Center, float alpha = 1.f);
    // Width in pixels of text drawn at size.
    float Measure(const string& text, float size) const;

    static uint32 NextCodePoint(const string& text, size_t& index);

    bool HasGlyph(uint32 codePoint) const
    {
        return _glyphs.count(codePoint) != 0;
    }

    void Render();

    void Clear()
    {
        _queue.clear();
    }

    static bool ScreenRectOf(const shared_ptr<GameObject>& uiObject, Vec4& rect);

    // Text drawn by the last Render, for tests.
    const vector<Item>& GetLastDrawn() const
    {
        return _lastDrawn;
    }

private:
    struct Glyph
    {
        Vec2 uv0, uv1;
        Vec2 size;
        float advance = 0.f;
    };

    struct Batch
    {
        shared_ptr<GameObject> object;
        shared_ptr<Mesh> mesh;
        vector<VertexTextureNormalTangentData> vertices;
        size_t used = 0;
    };

    void Initialize();

    bool _ready = false;
    float _bakeSize = 64.f;
    float _cellHeight = 85.f;
    float _padding = 0.f;
    unordered_map<uint32, Glyph> _glyphs;
    vector<Batch> _batches;
    shared_ptr<InstancingBuffer> _instance;
    vector<Item> _queue, _lastDrawn;
};
