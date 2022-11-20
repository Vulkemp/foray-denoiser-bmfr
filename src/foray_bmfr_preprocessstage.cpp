#include "foray_bmfr_preprocessstage.hpp"
#include "foray_bmfr.hpp"

namespace foray::bmfr {
    void PreProcessStage::Init(BmfrDenoiser* bmfrStage)
    {
        Destroy();
        mBmfrStage = bmfrStage;
        stages::ComputeStage::Init(mBmfrStage->mContext);
    }

    void PreProcessStage::ApiInitShader()
    {
        mShader.LoadFromSource(mContext, BMFR_SHADER_DIR "/preprocess.comp");
        mShaderSourcePaths.push_back(BMFR_SHADER_DIR "/preprocess.comp");
    }
    void PreProcessStage::ApiCreateDescriptorSet()
    {
        UpdateDescriptorSet();
    }
    void PreProcessStage::UpdateDescriptorSet()
    {
        std::vector<core::ManagedImage*> images({mBmfrStage->mInputs.Primary, mBmfrStage->mInputs.Position, &mBmfrStage->mHistory.Position.GetHistoryImage(),
                                                 mBmfrStage->mInputs.Normal, &mBmfrStage->mHistory.Normal.GetHistoryImage(), mBmfrStage->mInputs.Motion,
                                                 &mBmfrStage->mAccuImages.Input, &mBmfrStage->mAccuImages.AcceptBools, mBmfrStage->mPrimaryOutput});

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
            mDescriptorSet.Create(mContext, "Bmfr.PreProcess");
        }
    }

    void PreProcessStage::ApiCreatePipelineLayout()
    {
        mPipelineLayout.AddDescriptorSetLayout(mDescriptorSet.GetDescriptorSetLayout());
        mPipelineLayout.AddPushConstantRange<PushConstant>(VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT);
        mPipelineLayout.Build(mContext);
    }

    void PreProcessStage::ApiBeforeFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        std::vector<VkImageMemoryBarrier2> vkBarriers;

        {  // Read Only Images
            std::vector<core::ManagedImage*> readOnlyImages({mBmfrStage->mInputs.Primary, mBmfrStage->mInputs.Position, &mBmfrStage->mHistory.Position.GetHistoryImage(),
                                                             mBmfrStage->mInputs.Normal, &mBmfrStage->mHistory.Normal.GetHistoryImage(), mBmfrStage->mInputs.Motion});

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
            VkImageLayout                    oldLayout = renderInfo.GetImageLayoutCache().Get(mBmfrStage->mAccuImages.Input);
            uint32_t                         readIdx   = renderInfo.GetFrameNumber() % 2;
            uint32_t                         writeIdx  = (renderInfo.GetFrameNumber() + 1) % 2;
            core::ImageLayoutCache::Barrier2 readBarrier{
                .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
                .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
                .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange =
                    VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .baseArrayLayer = readIdx, .layerCount = 1U}};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mAccuImages.Input, readBarrier));
            core::ImageLayoutCache::Barrier2 writeBarrier{
                .SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL,
                .SubresourceRange =
                    VkImageSubresourceRange{.aspectMask = VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1U, .baseArrayLayer = writeIdx, .layerCount = 1U}};
            auto vkBarrier      = renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mAccuImages.Input, writeBarrier);
            vkBarrier.oldLayout = oldLayout;
            vkBarriers.push_back(vkBarrier);
        }
        {
            core::ImageLayoutCache::Barrier2 barrier{.SrcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                                                     .SrcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
                                                     .DstStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                                     .DstAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
                                                     .NewLayout     = VkImageLayout::VK_IMAGE_LAYOUT_GENERAL};
            vkBarriers.push_back(renderInfo.GetImageLayoutCache().MakeBarrier(mBmfrStage->mAccuImages.AcceptBools, barrier));
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

    void PreProcessStage::ApiBeforeDispatch(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo, glm::uvec3& groupSize)
    {
        PushConstant pushC                             = PushConstant();
        pushC.ReadIdx                                  = renderInfo.GetFrameNumber() % 2;
        pushC.WriteIdx                                 = (renderInfo.GetFrameNumber() + 1) % 2;
        pushC.EnableHistory                            = mBmfrStage->mHistory.Valid;
        pushC.DebugMode                                = mBmfrStage->mDebugMode;
        mBmfrStage->mAccuImages.LastInputArrayWriteIdx = pushC.WriteIdx;
        vkCmdPushConstants(cmdBuffer, mPipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushC), &pushC);

        VkExtent2D size = renderInfo.GetRenderSize();

        glm::uvec2 localSize(16, 16);
        glm::uvec2 FrameSize(size.width, size.height);

        groupSize = glm::uvec3((FrameSize.x + localSize.x - 1) / localSize.x, (FrameSize.y + localSize.y - 1) / localSize.y, 1);
    }
}  // namespace foray::bmfr