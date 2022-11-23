#include "foray_bmfr_postprocessstage.hpp"
#include "foray_bmfr.hpp"

namespace foray::bmfr {
    void PostProcessStage::Init(BmfrDenoiser* bmfrStage)
    {
        Destroy();
        mBmfrStage = bmfrStage;
        stages::ComputeStage::Init(mBmfrStage->mContext);
    }

    void PostProcessStage::ApiInitShader()
    {
        mShader.LoadFromSource(mContext, BMFR_SHADER_DIR "/postprocess.comp");
        mShaderSourcePaths.push_back(BMFR_SHADER_DIR "/postprocess.comp");
    }
    void PostProcessStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void PostProcessStage::UpdateDescriptorSet()
    {
        std::vector<core::ManagedImage*> images(
            {&mBmfrStage->mFilterImage, &mBmfrStage->mAccuImages.Filtered, mBmfrStage->mInputs.Motion, &mBmfrStage->mAccuImages.AcceptBools, mBmfrStage->mPrimaryOutput});

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
            mDescriptorSet.Create(mContext, "Bmfr.PostProcess");
        }
    }

    void PostProcessStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void PostProcessStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> readOnlyImages(
                {&mBmfrStage->mFilterImage, mBmfrStage->mInputs.Motion, &mBmfrStage->mAccuImages.AcceptBools, mBmfrStage->mPrimaryOutput});

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
        {
            core::ImageLayoutCache::Barrier2 barrier{
                .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange =
                    VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .baseArrayLayer = 0U, .layerCount = 2U}};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mAccuImages.Filtered, barrier));
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

    void PostProcessStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        mPushC.ReadIdx                                 = renderInfo.GetFrameNumber() % 2;
        mPushC.WriteIdx                                = (renderInfo.GetFrameNumber() + 1) % 2;
        mPushC.EnableHistory                           = mBmfrStage->mHistory.Valid;
        mPushC.DebugMode                               = mBmfrStage->mDebugMode;
        mBmfrStage->mAccuImages.LastInputArrayWriteIdx = mPushC.WriteIdx;
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(mPushC), &mPushC);

        VkExtent2D size = renderInfo.GetRenderSize();

        glm::uvec2 localSize(16, 16);
        glm::uvec2 FrameSize(size.width, size.height);

        groupSize = glm::uvec3((FrameSize.x + localSize.x - 1) / localSize.x, (FrameSize.y + localSize.y - 1) / localSize.y, 1);
    }
}  // namespace foray::bmfr