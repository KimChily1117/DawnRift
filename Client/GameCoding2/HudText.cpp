#include "pch.h"
#include "HudText.h"
#include "Material.h"
#include "MeshRenderer.h"
#include "InstancingBuffer.h"
#include "Camera.h"
#include <fstream>

namespace
{
constexpr size_t QuadsPerColor = 2048;
constexpr size_t MaxQueued = 1024;
const wstring FontAtlas = L"..\\Resources\\Textures\\UI\\HudFont\\hud_font.png";
const wstring FontMetrics = L"..\\Resources\\Textures\\UI\\HudFont\\hud_font.json";

struct Palette
{
    Vec4 fill, gradient;
};

const Palette Palettes[] = {
    {Vec4(1.f, 1.f, 1.f, 1.f), Vec4(0.86f, 0.86f, 0.86f, 1.f)},
    {Vec4(0.96f, 0.84f, 0.45f, 1.f), Vec4(0.80f, 0.62f, 0.25f, 1.f)},
    {Vec4(0.72f, 0.72f, 0.72f, 1.f), Vec4(0.60f, 0.60f, 0.60f, 1.f)},
    {Vec4(1.f, 0.42f, 0.38f, 1.f), Vec4(0.86f, 0.25f, 0.22f, 1.f)},
    {Vec4(0.78f, 0.90f, 1.f, 1.f), Vec4(0.62f, 0.78f, 0.96f, 1.f)},

    {Vec4(0.55f, 0.62f, 1.f, 1.f), Vec4(0.45f, 0.52f, 0.92f, 1.f)},
    {Vec4(1.f, 0.62f, 0.32f, 1.f), Vec4(0.90f, 0.50f, 0.22f, 1.f)},
    {Vec4(0.52f, 0.92f, 0.42f, 1.f), Vec4(0.40f, 0.80f, 0.32f, 1.f)},
};
}

shared_ptr<HudText> HudText::Get()
{
    static weak_ptr<Scene> scene;
    static weak_ptr<HudText> instance;

    if (scene.lock() == CUR_SCENE)
        if (auto existing = instance.lock())
            return existing;
    auto object = make_shared<GameObject>("HudText");
    object->GetOrAddTransform();
    auto text = object->GetOrAddScript<HudText>();
    text->Initialize();
    CUR_SCENE->Add(object);
    scene = CUR_SCENE;
    instance = text;

    return text;
}

void HudText::Initialize()
{
    std::ifstream metricsFile(FontMetrics);
    json metrics = json::parse(metricsFile, nullptr, false);

    if (metrics.is_discarded() || !metrics.contains("glyphs"))
    {
        DEBUG_LOG("[HudText] missing font atlas metrics");

        return;
    }

    const float width = metrics.value("width", 1.f), height = metrics.value("height", 1.f);
    _bakeSize = metrics.value("bakeSize", 64.f);
    _padding = metrics.value("padding", 0.f);

    for (const auto& [key, glyph] : metrics["glyphs"].items())
    {
        if (key.empty())
            continue;
        size_t at = 0;
        const uint32 codePoint = NextCodePoint(key, at);
        const float x = glyph.value("x", 0.f), y = glyph.value("y", 0.f);
        const float w = glyph.value("w", 0.f), h = glyph.value("h", 0.f);
        _glyphs[codePoint] = {Vec2(x / width, y / height), Vec2((x + w) / width, (y + h) / height), Vec2(w, h),
                              glyph.value("advance", w)};
        _cellHeight = h;
    }

    auto shader = make_shared<Shader>(L"DamageText.fx");
    auto atlas = RESOURCES->Load<Texture>(L"HudFontAtlas", FontAtlas);

    for (const Palette& palette : Palettes)
    {
        Batch batch;
        batch.vertices.resize(QuadsPerColor * 4);
        vector<uint32> indices;

        for (uint32 index = 0; index < batch.vertices.size(); index += 4)
            indices.insert(indices.end(), {index, index + 1, index + 2, index + 2, index + 1, index + 3});
        batch.mesh = make_shared<Mesh>();
        batch.mesh->CreateDynamic(batch.vertices, indices);
        auto material = make_shared<Material>();
        material->SetShader(shader);
        material->SetDiffuseMap(atlas);
        material->SetNormalMap(atlas);
        material->SetSpecularMap(atlas);
        material->GetMaterialDesc().diffuse = palette.fill;
        material->GetMaterialDesc().specular = palette.gradient;
        material->GetMaterialDesc().emissive = Vec4(0.f, 0.f, 0.f, 1.f);
        batch.object = make_shared<GameObject>("HudTextBatch");
        batch.object->GetOrAddTransform();
        batch.object->AddComponent(make_shared<MeshRenderer>());
        batch.object->GetMeshRenderer()->SetMesh(batch.mesh);
        batch.object->GetMeshRenderer()->SetMaterial(material);
        batch.object->GetMeshRenderer()->SetPass(0);
        _batches.push_back(std::move(batch));
    }

    _instance = make_shared<InstancingBuffer>();
    _instance->AddData(InstancingData{Matrix::Identity});
    _ready = true;
}

void HudText::Draw(const string& text, const Vec2& position, float size, Color color, Align align, float alpha)
{
    if (text.empty() || size <= 0.f || alpha <= 0.f || color >= Color::Count || _queue.size() >= MaxQueued)
        return;
    _queue.push_back({text, position, size, color, align, alpha});
}

uint32 HudText::NextCodePoint(const string& text, size_t& index)
{
    const auto byte = [&](size_t i)
    {
        return static_cast<uint8>(text[i]);
    };
    const uint8 lead = byte(index++);

    if (lead < 0x80)
        return lead;
    int32 extra = lead >= 0xF0 ? 3 : lead >= 0xE0 ? 2 : lead >= 0xC0 ? 1 : -1;

    if (extra < 0 || index + extra > text.size())
        return 0xFFFD;
    uint32 codePoint = lead & (0x3F >> extra);

    for (int32 i = 0; i < extra; ++i)
    {
        const uint8 next = byte(index);

        if ((next & 0xC0) != 0x80)
            return 0xFFFD;
        codePoint = (codePoint << 6) | (next & 0x3F);
        ++index;
    }

    return codePoint;
}

float HudText::Measure(const string& text, float size) const
{
    const float k = size / _bakeSize;
    float width = 0.f;

    for (size_t i = 0; i < text.size();)
        if (auto it = _glyphs.find(NextCodePoint(text, i)); it != _glyphs.end())
            width += it->second.advance * k;

    return width;
}

void HudText::Render()
{
    _lastDrawn = std::move(_queue);
    _queue.clear();

    if (!_ready)
        return;

    for (auto& batch : _batches)
        batch.used = 0;

    const auto& desc = GAME->GetGameDesc();
    auto toNdc = [&](float x, float y)
    {
        return Vec3(x / desc.width * 2.f - 1.f, 1.f - y / desc.height * 2.f, 0.f);
    };

    for (const Item& item : _lastDrawn)
    {
        Batch& batch = _batches[static_cast<size_t>(item.color)];
        const float k = item.size / _bakeSize;
        const float width = Measure(item.text, item.size);
        float cursor = item.align == Align::Left    ? item.position.x
                       : item.align == Align::Right ? item.position.x - width
                                                    : item.position.x - width * 0.5f;
        const float top = item.position.y - _cellHeight * k * 0.5f;

        for (size_t index = 0; index < item.text.size();)
        {
            const uint32 c = NextCodePoint(item.text, index);
            auto it = _glyphs.find(c);

            if (it == _glyphs.end())
                continue;
            const Glyph& glyph = it->second;

            if (c != ' ' && batch.used + 4 <= batch.vertices.size())
            {
                const float x0 = cursor - _padding * k;
                const float x1 = x0 + glyph.size.x * k;
                const float y0 = top, y1 = top + glyph.size.y * k;
                const Vec3 corners[4] = {toNdc(x0, y0), toNdc(x1, y0), toNdc(x0, y1), toNdc(x1, y1)};
                const Vec2 uvs[4] = {glyph.uv0, Vec2(glyph.uv1.x, glyph.uv0.y), Vec2(glyph.uv0.x, glyph.uv1.y),
                                     glyph.uv1};
                const float gradientV[4] = {0.f, 0.f, 1.f, 1.f};

                for (int32 i = 0; i < 4; ++i)
                {
                    auto& vertex = batch.vertices[batch.used++];
                    vertex.position = corners[i];
                    vertex.uv = uvs[i];
                    vertex.normal = Vec3(1.f);
                    vertex.tangent = Vec3(item.alpha, gradientV[i], 0.f);
                }
            }

            cursor += glyph.advance * k;
        }
    }

    for (auto& batch : _batches)
    {
        if (batch.used == 0)
            continue;
        batch.mesh->UpdateDynamicVertices(batch.vertices, static_cast<uint32>(batch.used));
        batch.object->GetMeshRenderer()->RenderInstancing(_instance);
    }

    // The text pass leaves its blend and depth states bound.
    DC->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    DC->OMSetDepthStencilState(nullptr, 0);
    DC->RSSetState(nullptr);
}

bool HudText::ScreenRectOf(const shared_ptr<GameObject>& uiObject, Vec4& rect)
{
    auto cameraObject = CUR_SCENE ? CUR_SCENE->GetUICamera() : nullptr;

    if (!uiObject || !cameraObject || !cameraObject->GetCamera())
        return false;
    auto camera = cameraObject->GetCamera();
    const Matrix transform =
        uiObject->GetTransform()->GetWorldMatrix() * camera->GetViewMatrix() * camera->GetProjectionMatrix();
    const auto& desc = GAME->GetGameDesc();
    rect = Vec4(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const Vec3& corner :
         {Vec3(-0.5f, -0.5f, 0.f), Vec3(0.5f, -0.5f, 0.f), Vec3(-0.5f, 0.5f, 0.f), Vec3(0.5f, 0.5f, 0.f)})
    {
        const Vec3 ndc = Vec3::Transform(corner, transform);
        const float x = (ndc.x * 0.5f + 0.5f) * desc.width, y = (0.5f - ndc.y * 0.5f) * desc.height;
        rect.x = min(rect.x, x);
        rect.y = min(rect.y, y);
        rect.z = max(rect.z, x);
        rect.w = max(rect.w, y);
    }

    return std::isfinite(rect.x) && rect.z > rect.x && rect.w > rect.y;
}
