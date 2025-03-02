#ifndef VIRTUAL_TEXTURE_INCLUDE
#define VIRTUAL_TEXTURE_INCLUDE
// vec3 SampleVT(vec2 uv)
// {
//     vec4 scaleBias = texture(vtIndir, uv);
//     vec3 vtCacheData = texture(vtCache, uv * scaleBias.xy + scaleBias.zw).xyz;
//
//     return vtCacheData;
// }
#endif
