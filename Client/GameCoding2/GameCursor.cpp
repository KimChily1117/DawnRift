#include "pch.h"
#include "GameCursor.h"

namespace
{
const char* const Files[] = {"hand1",     "hand2",        "hoverenemy",       "hoverfriendly",
                             "hovershop", "singletarget", "singletargetally", "singletargetenemy"};
static_assert(sizeof(Files) / sizeof(Files[0]) == static_cast<size_t>(GameCursor::Kind::Count), "one file per kind");

HCURSOR g_cursors[static_cast<size_t>(GameCursor::Kind::Count)] = {};
GameCursor::Kind g_current = GameCursor::Kind::Count;

HCURSOR Load(GameCursor::Kind kind)
{
    HCURSOR& cursor = g_cursors[static_cast<size_t>(kind)];

    if (!cursor)
    {
        const wstring path =
            L"..\\Resources\\Textures\\UI\\Cursors\\" + Utils::ToWString(Files[static_cast<size_t>(kind)]) + L".cur";
        cursor = static_cast<HCURSOR>(::LoadImageW(nullptr, path.c_str(), IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE));
    }

    return cursor;
}
}

namespace GameCursor
{
void Set(Kind kind)
{
    if (kind == g_current || kind >= Kind::Count)
        return;
    HCURSOR cursor = Load(kind);

    if (!cursor)
        return;
    g_current = kind;

    if (HWND window = GAME->GetGameDesc().hWnd)
        ::SetClassLongPtrW(window, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
    ::SetCursor(cursor);
    static bool shown = false;

    if (!shown)
    {
        shown = true;

        while (::ShowCursor(TRUE) < 0)
        {
        }
    }
}

Kind Current()
{
    return g_current;
}

const char* Name(Kind kind)
{
    return kind < Kind::Count ? Files[static_cast<size_t>(kind)] : "";
}
}
