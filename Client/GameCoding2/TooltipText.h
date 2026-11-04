#pragma once
#include "HudText.h"

namespace TooltipText
{
struct Run
{
    string text;
    HudText::Color color;
};

using Line = vector<Run>;

struct Entry
{
    string name;
    vector<string> ranks;
    vector<float> cooldown, cost;
    float range = 0.f;
};

const Entry* Find(const string& champion, const string& slot);

vector<Line> Layout(const string& text, float size, float maxWidth);
}
