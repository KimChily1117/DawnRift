#pragma once
#include "MonoBehaviour.h"
#include "VertexData.h"
#include <random>

class Mesh;
class Shader;

class DamageText : public MonoBehaviour
{
public:
    enum class Type
    {
        Physical = 0,
        Magic = 1,
        True = 2,
        Gold = 3,
        Experience = 4,
        Level = 5
    };

    static shared_ptr<DamageText> Get();

    void Show(const shared_ptr<GameObject>& target, int32 damage, Type type, bool receivedByMe, bool crit = false);

    void ShowReward(const shared_ptr<GameObject>& target, const string& text, Type type);
    void LateUpdate() override;

    struct ScreenRect
    {
        float left, top, right, bottom;
    };

    vector<ScreenRect> GetLiveRects() const;

    size_t GetLiveCount() const
    {
        return _texts.size();
    }

private:
    struct Glyph
    {
        Vec2 uv0, uv1;
        Vec2 size;
        float advance = 0.f;
    };

    struct Style
    {
        Vec4 fill = Vec4(1.f), gradient = Vec4(1.f), outline = Vec4(0.f, 0.f, 0.f, 1.f);
        float fontSize = 27.f;
        size_t batch = 0;
    };

    struct Text
    {
        string glyphs;
        size_t style = 0;
        Vec3 anchor;
        Vec2 offset, velocity;
        float age = 0.f;
        bool crit = false;
    };

    struct Batch
    {
        shared_ptr<GameObject> object;
        shared_ptr<Mesh> mesh;
        vector<VertexTextureNormalTangentData> vertices;
        size_t used = 0;
        bool visible = false;
    };

    void Initialize();
    size_t StyleIndex(Type type, bool receivedByMe) const;
    bool Project(const Vec3& world, Vec2& screen) const;
    ScreenRect RectOf(const Text& text, const Vec2& screen) const;
    float Scale(const Text& text) const;
    float UiScale() const;
    void SeparateOverlaps();
    float TextWidth(const Text& text) const;
    float Random(float low, float high);

    unordered_map<char, Glyph> _glyphs;
    float _bakeSize = 64.f, _cellHeight = 85.f;
    vector<Style> _styles;
    vector<Batch> _batches;
    vector<Text> _texts;
    unordered_map<GameObject*, float> _nextDirection;
    shared_ptr<Shader> _shader;
    weak_ptr<Scene> _scene;
    std::mt19937 _random{0xDA3Au};
    bool _ready = false;
};
