#include "foray_bmfr_regressionstage.hpp"

namespace foray::bmfr
{
        void RegressionStage::Init(BmfrDenoiser* bmfrStage){}
        void RegressionStage::UpdateDescriptorSet(){}
        void RegressionStage::ApiInitShader() {}
        void RegressionStage::ApiCreateDescriptorSet() {}
        void RegressionStage::ApiCreatePipelineLayout() {}
        void RegressionStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) {}
        void RegressionStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize) {}
} // namespace foray::bmfr
