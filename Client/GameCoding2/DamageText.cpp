#include "pch.h"
#include "DamageText.h"
#include "BasePlayerController.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "Camera.h"
#include <fstream>

namespace
{
constexpr float HorizontalSpeedMin = 50.f, HorizontalSpeedMax = 65.f;
constexpr float VerticalSpeedMin = 150.f, VerticalSpeedMax = 180.f;
constexpr float Gravity = 500.f;
constexpr float HoldTime = 0.45f, FadeTime = 0.4f;
constexpr float PopTime = 0.15f, PopScale = 1.4f;
constexpr float ReferenceHeight = 1080.f;
constexpr float MinUiScale = 768.f / 1080.f;
constexpr float AnchorAboveCenter = 0.5f;
constexpr size_t QuadsPerStyle = 192;
constexpr int32 OverlayOrder = 100;
constexpr int32 MaxOverlapPushes = 6;

const char* StyleNames[] = {
    "FloatText_PhysicalDamage",
    "FloatText_MagicalDamage",
    "FloatText_TrueDamage",
    "FloatText_EnemyPhysicalDamage",
    "FloatText_EnemyMagicalDamage",
    "FloatText_EnemyTrueDamage",
    "FloatText_Gold",
    "FloatText_Experience",
    "FloatText_Level",
};
const wstring FontAtlas = L"..\\Resources\\Textures\\UI\\DamageFont\\damage_font.png";
const wstring FontMetrics = L"..\\Resources\\Textures\\UI\\DamageFont\\damage_font.json";
const wstring StyleFile = L"..\\Resources\\Extracted\\UI\\FloatingText\\damage_text.json";

Vec4 ReadColor(const json& value, const Vec4& fallback)
{
    if (!value.is_array() || value.size() < 4)
        return fallback;

    return Vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()) / 255.f;
}

bool Overlaps(const DamageText::ScreenRect& a, const DamageText::ScreenRect& b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}
}

shared_ptr<DamageText> DamageText::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<DamageText> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("DamageText");
    object->GetOrAddTransform();
    auto text = object->GetOrAddScript<DamageText>();
    text->_scene = CUR_SCENE;
    text->Initialize();
    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = text;

    return text;
}

void DamageText::Initialize()
{
    std::ifstream metricsFile(FontMetrics);
    json metrics = json::parse(metricsFile, nullptr, false);
    std::ifstream styleFile(StyleFile);
    json styles = json::parse(styleFile, nullptr, false);

    if (metrics.is_discarded() || styles.is_discarded() || !metrics.contains("glyphs") || !styles.contains("styles"))
    {
        DEBUG_LOG("[DamageText] missing font atlas metrics or damage_text.json");

        return;
    }

    const float width = metrics.value("width", 1.f), height = metrics.value("height", 1.f);
    _bakeSize = metrics.value("bakeSize", 64.f);

    for (const auto& [key, glyph] : metrics["glyphs"].items())
    {
        const float x = glyph.value("x", 0.f), y = glyph.value("y", 0.f);
        const float w = glyph.value("w", 0.f), h = glyph.value("h", 0.f);
        _glyphs[key[0]] = {Vec2(x / width, y / height), Vec2((x + w) / width, (y + h) / height), Vec2(w, h),
                           glyph.value("advance", w)};
        _cellHeight = h;
    }

    _shader = make_shared<Shader>(L"DamageText.fx");
    auto atlas = RESOURCES->Load<Texture>(L"DamageFontAtlas", FontAtlas);

    for (const char* name : StyleNames)
    {
        const json& source = styles["styles"].contains(name) ? styles["styles"][name] : json::object();
        Style style;
        style.fill = ReadColor(source.value("fill", json()), Vec4(1.f));
        style.gradient = ReadColor(source.value("gradient", json()), style.fill);
        style.outline = ReadColor(source.value("outline", json()), Vec4(0.f, 0.f, 0.f, 1.f));
        style.fontSize = source.value("size", 27.f);

        Batch batch;
        batch.vertices.resize(QuadsPerStyle * 4);
        vector<uint32> indices;

        for (uint32 index = 0; index < batch.vertices.size(); index += 4)
            indices.insert(indices.end(), {index, index + 1, index + 2, index + 2, index + 1, index + 3});
        batch.mesh = make_shared<Mesh>();
        batch.mesh->CreateDynamic(batch.vertices, indices);
        auto material = make_shared<Material>();
        material->SetShader(_shader);
        material->SetDiffuseMap(atlas);
        material->SetNormalMap(atlas);
        material->SetSpecularMap(atlas);
        material->GetMaterialDesc().diffuse = style.fill;
        material->GetMaterialDesc().specular = style.gradient;
        material->GetMaterialDesc().emissive = style.outline;
        batch.object = make_shared<GameObject>("DamageTextBatch");
        batch.object->GetOrAddTransform();
        batch.object->AddComponent(make_shared<MeshRenderer>());
        batch.object->GetMeshRenderer()->SetMesh(batch.mesh);
        batch.object->GetMeshRenderer()->SetMaterial(material);
        batch.object->GetMeshRenderer()->SetPass(0);
        batch.object->GetMeshRenderer()->SetWorldOverlay(OverlayOrder);
        style.batch = _batches.size();
        _batches.push_back(std::move(batch));
        _styles.push_back(style);
    }

    _ready = true;
}

size_t DamageText::StyleIndex(Type type, bool receivedByMe) const
{
    if (type >= Type::Gold)
        return 6 + static_cast<size_t>(type) - static_cast<size_t>(Type::Gold);

    return static_cast<size_t>(type) + (receivedByMe ? 3 : 0);
}

float DamageText::Random(float low, float high)
{
    return std::uniform_real_distribution<float>(low, high)(_random);
}

bool DamageText::Project(const Vec3& world, Vec2& screen) const
{
    auto cameraObject = CUR_SCENE ? CUR_SCENE->GetMainCamera() : nullptr;

    if (!cameraObject || !cameraObject->GetCamera())
        return false;
    auto camera = cameraObject->GetCamera();
    const Vec4 clip =
        Vec4::Transform(Vec4(world.x, world.y, world.z, 1.f), camera->GetViewMatrix() * camera->GetProjectionMatrix());

    if (clip.w <= 0.0001f)
        return false;
    const auto& desc = GAME->GetGameDesc();
    screen = Vec2((clip.x / clip.w * 0.5f + 0.5f) * desc.width, (0.5f - clip.y / clip.w * 0.5f) * desc.height);

    return true;
}

float DamageText::Scale(const Text& text) const
{
    const float pop = text.age < PopTime ? PopScale + (1.f - PopScale) * (text.age / PopTime) : 1.f;

    return _styles[text.style].fontSize / _bakeSize * UiScale() * pop * (text.crit ? 1.4f : 1.f);
}

float DamageText::TextWidth(const Text& text) const
{
    float width = 0.f;

    for (char c : text.glyphs)
        if (auto it = _glyphs.find(c); it != _glyphs.end())
            width += it->second.advance;

    return width * Scale(text);
}

float DamageText::UiScale() const
{
    return max(MinUiScale, GAME->GetGameDesc().height / ReferenceHeight);
}

void DamageText::SeparateOverlaps()
{
    for (size_t newer = 1; newer < _texts.size(); ++newer)
    {
        Vec2 newerScreen;

        if (!Project(_texts[newer].anchor, newerScreen))
            continue;

        for (size_t older = 0; older < newer; ++older)
        {
            Vec2 olderScreen;

            if (!Project(_texts[older].anchor, olderScreen))
                continue;
            const ScreenRect a = RectOf(_texts[older], olderScreen);
            const ScreenRect b = RectOf(_texts[newer], newerScreen);

            if (Overlaps(a, b))
                _texts[newer].offset.y += b.bottom - a.top + 1.f;
        }
    }
}

DamageText::ScreenRect DamageText::RectOf(const Text& text, const Vec2& screen) const
{
    const Vec2 center(screen.x + text.offset.x, screen.y - text.offset.y);
    const float halfWidth = TextWidth(text) * 0.5f;
    const float halfHeight = _cellHeight * Scale(text) * 0.5f;

    return {center.x - halfWidth, center.y - halfHeight, center.x + halfWidth, center.y + halfHeight};
}

vector<DamageText::ScreenRect> DamageText::GetLiveRects() const
{
    vector<ScreenRect> rects;

    for (const auto& text : _texts)
    {
        Vec2 screen;

        if (Project(text.anchor, screen))
            rects.push_back(RectOf(text, screen));
    }

    return rects;
}

void DamageText::ShowReward(const shared_ptr<GameObject>& target, const string& glyphs, Type type)
{
    if (!_ready || !target || glyphs.empty() || type < Type::Gold)
        return;
    Text text;
    text.glyphs = glyphs;
    text.style = StyleIndex(type, false);
    text.anchor = BasePlayerController::GetHitVfxPosition(target) + Vec3(0.f, AnchorAboveCenter, 0.f);

    text.velocity = Vec2(0.f, Random(VerticalSpeedMin, VerticalSpeedMax) * 0.8f) * UiScale();
    _texts.push_back(text);
}

void DamageText::Show(const shared_ptr<GameObject>& target, int32 damage, Type type, bool receivedByMe, bool crit)
{
    if (!_ready || !target || damage <= 0)
        return;
    Text text;
    text.glyphs = std::to_string(damage);
    text.style = StyleIndex(type, receivedByMe);
    text.crit = crit;
    text.anchor = BasePlayerController::GetHitVfxPosition(target) + Vec3(0.f, AnchorAboveCenter, 0.f);

    float& direction = _nextDirection[target.get()];
    direction = direction > 0.f ? -1.f : 1.f;
    const float scale = UiScale();
    text.velocity =
        Vec2(direction * Random(HorizontalSpeedMin, HorizontalSpeedMax), Random(VerticalSpeedMin, VerticalSpeedMax)) *
        scale;

    // Push the new number up until it clears every live one.
    Vec2 screen;

    if (Project(text.anchor, screen))
    {
        const auto live = GetLiveRects();

        for (int32 push = 0; push < MaxOverlapPushes; ++push)
        {
            const ScreenRect rect = RectOf(text, screen);

            if (std::none_of(live.begin(), live.end(), [&](const ScreenRect& other) { return Overlaps(rect, other); }))
                break;
            text.offset.y += (rect.bottom - rect.top) * 0.8f;
        }
    }

    _texts.push_back(text);
}

void DamageText::LateUpdate()
{
    const float dt = TIME->GetDeltaTime();

    if (!_ready || !std::isfinite(dt) || dt <= 0.f)
        return;
    const float scale = UiScale();

    for (auto& text : _texts)
    {
        text.age += dt;

        if (text.style < StyleIndex(Type::Gold, false))
            text.velocity.y -= Gravity * scale * dt;
        text.offset += text.velocity * dt;
    }

    std::erase_if(_texts, [](const Text& t) { return t.age >= HoldTime + FadeTime; });
    SeparateOverlaps();

    for (auto& batch : _batches)
        batch.used = 0;
    const auto& desc = GAME->GetGameDesc();
    auto toNdc = [&](float x, float y)
    {
        return Vec3(x / desc.width * 2.f - 1.f, 1.f - y / desc.height * 2.f, 0.f);
    };

    for (const auto& text : _texts)
    {
        Vec2 screen;

        if (!Project(text.anchor, screen))
            continue;
        Batch& batch = _batches[_styles[text.style].batch];
        const float alpha = text.age < HoldTime ? 1.f : max(0.f, 1.f - (text.age - HoldTime) / FadeTime);
        const float k = Scale(text);
        const ScreenRect rect = RectOf(text, screen);
        float cursor = rect.left;

        for (char c : text.glyphs)
        {
            auto it = _glyphs.find(c);

            if (it == _glyphs.end() || batch.used + 4 > batch.vertices.size())
                continue;
            const Glyph& glyph = it->second;

            const float x0 = cursor + (glyph.advance - glyph.size.x) * k * 0.5f;
            const float x1 = x0 + glyph.size.x * k;
            const float y0 = rect.top, y1 = rect.top + glyph.size.y * k;
            const Vec3 corners[4] = {toNdc(x0, y0), toNdc(x1, y0), toNdc(x0, y1), toNdc(x1, y1)};
            const Vec2 uvs[4] = {glyph.uv0, Vec2(glyph.uv1.x, glyph.uv0.y), Vec2(glyph.uv0.x, glyph.uv1.y), glyph.uv1};
            const float gradientV[4] = {0.f, 0.f, 1.f, 1.f};

            for (int32 i = 0; i < 4; ++i)
            {
                auto& vertex = batch.vertices[batch.used++];
                vertex.position = corners[i];
                vertex.uv = uvs[i];
                vertex.normal = Vec3(1.f);
                vertex.tangent = Vec3(alpha, gradientV[i], 0.f);
            }

            cursor += glyph.advance * k;
        }
    }

    auto scene = _scene.lock();

    if (!scene)
        return;

    for (auto& batch : _batches)
    {
        if (batch.used == 0)
        {
            if (batch.visible)
                scene->Remove(batch.object);
            batch.visible = false;
            continue;
        }

        batch.mesh->UpdateDynamicVertices(batch.vertices, static_cast<uint32>(batch.used));

        if (!batch.visible)
            scene->Add(batch.object);
        batch.visible = true;
    }
}
