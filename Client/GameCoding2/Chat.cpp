#include "pch.h"
#include "Chat.h"
#include "ClientPacketHandler.h"
#include "UiStrings.h"
#include "GameFlow.h"

namespace
{
std::deque<Chat::Line> g_lines;
bool g_open = false;
Protocol::CHAT_CHANNEL g_channel = Protocol::CHAT_TEAM;
std::u32string g_input;
size_t g_sent = 0;

string ToUtf8(const std::u32string& text)
{
    string out;

    for (char32_t c : text)
    {
        if (c < 0x80)
            out += static_cast<char>(c);
        else if (c < 0x800)
        {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
        else
        {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }

    return out;
}

string ToUtf8(const wstring& text)
{
    std::u32string wide;

    for (size_t i = 0; i < text.size(); ++i)
    {
        const wchar_t c = text[i];

        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < text.size())
        {
            wide.push_back(0x10000 + ((static_cast<char32_t>(c) - 0xD800) << 10) + (text[i + 1] - 0xDC00));
            ++i;
        }
        else
            wide.push_back(c);
    }

    return ToUtf8(wide);
}

constexpr size_t MaxInput = 150;
}

namespace Chat
{
void Open(Protocol::CHAT_CHANNEL channel)
{
    g_open = true;
    g_channel = channel;
    g_input.clear();
    INPUT->SetTextCapture(true);
}

void Close()
{
    g_open = false;
    g_input.clear();
    INPUT->SetTextCapture(false);
}

bool IsOpen()
{
    return g_open;
}

Protocol::CHAT_CHANNEL Channel()
{
    return g_channel;
}

void Update()
{
    const float now = TIME->GetGameTime();

    if (!g_open)
        return;

    for (uint32 c : INPUT->TakeTyped())
    {
        if (c == 0x08)
        {
            if (!g_input.empty())
                g_input.pop_back();
        }
        else if (c >= 0x20 && c != 0x7F && g_input.size() < MaxInput)
            g_input.push_back(c);
    }

    if (INPUT->GetButtonDown(KEY_TYPE::ESCAPE))
        Close();
    else if (INPUT->GetButtonDown(KEY_TYPE::ENTER))
        Submit();
    (void)now;
}

void Submit()
{
    if (!g_open)
        return;
    const string typed = ToUtf8(g_input);

    if (typed == "/ff" || typed == "/surrender" || typed == "/\xed\x95\xad\xeb\xb3\xb5")
    {
        GameFlow::SendSurrender(true);
        Close();

        return;
    }

    if (!g_input.empty())
    {
        Protocol::C_ChatMessage packet;
        packet.set_message(ToUtf8(g_input));
        packet.set_channel(g_channel);
        NETWORK->SendPacket(ClientPacketHandler::MakeSendBuffer(packet, C_CHAT_MESSAGE));
        ++g_sent;
    }

    Close();
}

void OnPacket(const Protocol::S_ChatMessage& packet)
{
    Line line;
    line.channel = packet.channel();
    line.sender = packet.sendername();
    line.text = packet.message();
    line.team = packet.team();
    line.at = TIME->GetGameTime();
    g_lines.push_back(line);

    if (g_lines.size() > MaxLines)
        g_lines.pop_front();
    auto me = GAMEMANAGER->_myPlayer;

    if (!(me && me->_playerInfo && me->_playerInfo->objectid() == packet.senderid()))
        SOUND->PlayCue("Play_sfx_hud_Generic_Button_Click");
}

void AddSystem(const string& text)
{
    if (text.empty())
        return;
    Line line;
    line.channel = Protocol::CHAT_SYSTEM;
    line.text = text;
    line.at = TIME->GetGameTime();
    g_lines.push_back(line);

    if (g_lines.size() > MaxLines)
        g_lines.pop_front();
}

const std::deque<Line>& Lines()
{
    return g_lines;
}

string Input()
{
    return ToUtf8(g_input);
}

string Composition()
{
    return ToUtf8(INPUT->GetComposition());
}

void TypeForTests(const string& utf8)
{
    for (size_t i = 0; i < utf8.size();)
    {
        const unsigned char c = utf8[i];
        char32_t code;
        size_t len;

        if (c < 0x80)
        {
            code = c;
            len = 1;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            code = c & 0x1F;
            len = 2;
        }
        else if ((c & 0xF0) == 0xE0)
        {
            code = c & 0x0F;
            len = 3;
        }
        else
        {
            code = c & 0x07;
            len = 4;
        }

        for (size_t k = 1; k < len && i + k < utf8.size(); ++k)
            code = (code << 6) | (utf8[i + k] & 0x3F);
        i += len;

        if (code == 0x08)
        {
            if (!g_input.empty())
                g_input.pop_back();
        }
        else if (g_input.size() < MaxInput)
            g_input.push_back(code);
    }
}

size_t Sent()
{
    return g_sent;
}

void Clear()
{
    g_lines.clear();
    Close();
}
}
