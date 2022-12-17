#pragma once
#include <stages/foray_computestage.hpp>
#include "shaders/debug.glsl.h"

namespace foray::bmfr {
    class BmfrDenoiser;

    class RegressionStage : public stages::ComputeStageBase
    {
      public:
        void Init(BmfrDenoiser* bmfrStage);

        void UpdateDescriptorSet();

      protected:
        BmfrDenoiser* mBmfrStage = nullptr;

        struct PushConstant
        {
            uint32_t FrameIdx;
            uint32_t DispatchWidth;
            uint32_t ReadIdx;
            uint32_t DebugMode = DEBUG_NONE;
        } mPushC;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::bmfr