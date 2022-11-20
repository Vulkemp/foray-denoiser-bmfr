#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::bmfr {
    class BmfrDenoiser;

    class PostProcessStage : public foray::stages::ComputeStage
    {
      friend BmfrDenoiser;
      public:
        void Init(BmfrDenoiser* bmfrStage);

        void UpdateDescriptorSet();

      protected:
        BmfrDenoiser* mBmfrStage = nullptr;

        struct PushConstant
        {
            // Read array index
            uint32_t ReadIdx;
            // Write array index
            uint32_t WriteIdx;
            // Combined Weight Threshhold (Default 0.01)
            fp32_t WeightThreshhold = 0.01f;
            // Minimum weight assigned to new data
            fp32_t MinNewDataWeight = 0.166666667f;
            uint32_t  EnableHistory;
            uint32_t  DebugMode;
        } mPushC;

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };


}  // namespace foray::bmfr