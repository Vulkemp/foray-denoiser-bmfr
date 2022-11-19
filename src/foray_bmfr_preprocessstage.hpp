#pragma once

#include <stages/foray_computestage.hpp>

namespace foray::bmfr {
    class BmfrDenoiser;

    class PreProcessStage : public foray::stages::ComputeStage
    {
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
            // Maximum position difference (Default 0.15)
            fp32_t MaxPositionDifference = 0.15f;
            // Maximum deviation of the sinus of previous and current normal (Default 0.05)
            fp32_t MaxNormalDeviation = 0.05f;
            // Combined Weight Threshhold (Default 0.01)
            fp32_t WeightThreshhold = 0.01f;
            // Minimum weight assigned to new data
            fp32_t   MinNewDataWeight = 0.1f;
            uint32_t EnableHistory;
            uint32_t DebugMode;
        };

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };


}  // namespace foray::bmfr