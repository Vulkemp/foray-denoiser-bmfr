#include "foray_bmfr_regressionstage.hpp"
#include "foray_bmfr.hpp"

namespace foray::bmfr {
    void RegressionStage::Init(BmfrDenoiser* bmfrStage)
    {
        mBmfrStage = bmfrStage;
        stages::ComputeStage::Init(mBmfrStage->mContext);
    }
    void RegressionStage::UpdateDescriptorSet()
    {
        std::vector<core::ManagedImage*> images({mBmfrStage->mInputs.Position, mBmfrStage->mInputs.Normal, mBmfrStage->mInputs.Albedo, &mBmfrStage->mRegression.TempData,
                                                 &mBmfrStage->mRegression.OutData, &mBmfrStage->mAccuImages.Input, &mBmfrStage->mFilterImage, mBmfrStage->mPrimaryOutput});

        for(size_t i = 0; i < images.size(); i++)
        {
            mDescriptorSet.SetDescriptorAt(i, images[i], VkImageLayout::VK_IMAGE_LAYOUT_GENERAL, nullptr, VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                           VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        }

        if(mDescriptorSet.Exists())
        {
            mDescriptorSet.Update();
        }
        else
        {
            mDescriptorSet.Create(mContext, "Bmfr.Regression");
        }
    }
    void RegressionStage::ApiInitShader()
    {
        mShaderKeys.push_back(mShader.CompileFromSource(mContext, BMFR_SHADER_DIR "/regression.comp"));
    }
    void RegressionStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void RegressionStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }
    void RegressionStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> readOnlyImages({mBmfrStage->mInputs.Position, mBmfrStage->mInputs.Normal, mBmfrStage->mInputs.Albedo, mBmfrStage->mPrimaryOutput});

            for(core::ManagedImage* image : readOnlyImages)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                    .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                    .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                };
                vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, barrier));
            }
        }
        {  // Temp & OutData
            std::vector<core::ManagedImage*> readWriteImages({&mBmfrStage->mRegression.TempData, &mBmfrStage->mRegression.OutData});

            for(core::ManagedImage* image : readWriteImages)
            {
                core::ImageLayoutCache::Barrier2 barrier{
                    .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .SrcAccessMask = VK_ACCESS_2_NONE,
                    .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                    .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                    .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                };
                vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(image, barrier));
            }
        }
        {  // PreProcess Input
            uint32_t                         readIdx = mBmfrStage->mAccuImages.LastInputArrayWriteIdx;
            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange =
                    VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .baseArrayLayer = 0U, .layerCount = 2U}};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mAccuImages.Input, barrier));
        }
        {  // PostProcess Filtered Output
            core::ImageLayoutCache::Barrier2 barrier{.SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                     .SrcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                                                     .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                                                     .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mFilterImage, barrier));
        }
        {
            core::ImageLayoutCache::Barrier2 barrier{.SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                     .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                                                     .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                                                     .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mPrimaryOutput, barrier));
        }
        VkDependencyInfo depInfo{
            .sType = VkStructureType::VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .imageMemoryBarrierCount = (uint32_t)vkBarriers.size(), .pImageMemoryBarriers = vkBarriers.data()};

        vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
    }
    void RegressionStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        glm::uvec2 dispatch = mBmfrStage->mRegression.DispatchSize;

        mPushC.FrameIdx      = renderInfo.GetFrameNumber();
        mPushC.ReadIdx       = mBmfrStage->mAccuImages.LastInputArrayWriteIdx;
        mPushC.DispatchWidth = dispatch.x;
        mPushC.DebugMode     = mBmfrStage->mDebugMode;
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(mPushC), &mPushC);

        groupSize = glm::uvec3(dispatch.x * dispatch.y, 1, 1);
    }
}  // namespace foray::bmfr
