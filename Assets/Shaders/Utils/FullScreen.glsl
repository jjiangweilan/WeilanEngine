#ifndef FULL_SCREEN_INC
#define FULL_SCREEN_INC

#if VERT
layout(location = 0) out vec2 o_uv;

void main()
{
    vec4 c_Positions[6] =
    {
        { -1, -1, 0.5, 1 },
        { -1, 1, 0.5, 1 },
        { 1, 1, 0.5, 1 },

        { -1, -1, 0.5, 1 },
        { 1, 1, 0.5, 1 },
        { 1, -1, 0.5, 1 }
    };

    gl_Position = c_Positions[gl_VertexIndex];
    o_uv = gl_Position.xy * 0.5f + 0.5f;
}

#endif // VERT

#endif // FULL_SCREEN_INC
