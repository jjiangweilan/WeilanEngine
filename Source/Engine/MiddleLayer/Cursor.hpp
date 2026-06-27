#pragma once

class CursorAtlas;

class Cursor
{
public:
    static bool SetCursorAppearance(CursorAtlas* atlas, int textureIndex);
    static void ResetCursorAppearance();
};
