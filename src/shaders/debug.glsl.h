#ifndef BMFRDEBUG_GLSL
#define BMFRDEBUG_GLSL
#ifdef __cplusplus
#pragma once

namespace foray::bmfr
{
    using uint = unsigned int;
#endif
    const uint DEBUG_NONE = 0U;
    const uint DEBUG_PREPROCESS_OUT = 1U;
    const uint DEBUG_PREPROCESS_ACCEPTS = 2U;
    const uint DEBUG_PREPROCESS_ALPHA = 3U;
    const uint DEBUG_REGRESSION_OUT = 4U;
    const uint DEBUG_REGRESSION_BLOCKS = 5U;
    const uint DEBUG_POSTPROCESS_ACCEPTS = 6U;
    const uint DEBUG_POSTPROCESS_ALPHA = 7U;
#ifdef __cplusplus
} // namespace foray::bmfr
#endif

#endif // BMFRDEBUG_GLSL