#pragma once
#include <stages/foray_computestage.hpp>

namespace foray::bmfr {
    class BmfrDenoiser;

    class RegressionStage : public stages::ComputeStage
    {
      public:
        void Init(BmfrDenoiser* bmfrStage);

        void UpdateDescriptorSet();

      protected:
        BmfrDenoiser* mBmfrStage = nullptr;

        struct PushConstant
        {
            /* some data */
        };

        virtual void ApiInitShader() override;
        virtual void ApiCreateDescriptorSet() override;
        virtual void ApiCreatePipelineLayout() override;
        virtual void ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual void ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) override;
    };
}  // namespace foray::bmfr