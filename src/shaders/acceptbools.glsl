#ifndef ACCEPTBOOLS_GLSL
#define ACCEPTBOOLS_GLSL

void writeAcceptBool(inout uint acceptBools, ivec2 bilinearSample, bool accept)
{
    uint offset = bilinearSample.x * 2 + bilinearSample.y;
    acceptBools = acceptBools | (uint(accept) << offset);
}

bool readAcceptBool(uint acceptBools, ivec2 bilinearSample)
{
    uint offset = bilinearSample.x * 2 + bilinearSample.y;
    return bool((acceptBools >> offset) & 1);
}

#endif // ACCEPTBOOLS_GLSL