#include "foray_bmfr.hpp"
#include <imgui/imgui.h>

namespace foray::bmfr {
    void BmfrDenoiser::Init(core::Context* context, const stages::DenoiserConfig& config)
    {
        Destroy();
        mContext       = context;
        mInputs.Normal = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Normal];
        Assert(!!mInputs.Normal);
        mInputs.Motion = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Motion];
        Assert(!!mInputs.Motion);
        mInputs.Position = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Position];
        Assert(!!mInputs.Position);
        mInputs.Primary = config.PrimaryInput;
        Assert(!!mInputs.Primary);
        mPrimaryOutput = config.PrimaryOutput;
        Assert(!!mPrimaryOutput);

        VkExtent2D size = mInputs.Primary->GetExtent2D();

        {  // Setup history images
            mHistory.Position.Create(mContext, mInputs.Position);
            mHistory.Normal.Create(mContext, mInputs.Normal);
        }
        {  // Setup Accumulation images
            VkImageUsageFlags usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
            {  // Input
                core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, size, "Bmfr.AccuInput");
                ci.ImageCI.arrayLayers                     = 2;
                ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                ci.ImageViewCI.subresourceRange.layerCount = 2;
                mAccuImages.Input.Create(mContext, ci);
            }
            {  // Filtered
                core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, size, "Bmfr.AccuFiltered");
                ci.ImageCI.arrayLayers                     = 2;
                ci.ImageViewCI.viewType                    = VkImageViewType::VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                ci.ImageViewCI.subresourceRange.layerCount = 2;
                mAccuImages.Filtered.Create(mContext, ci);
            }
            {  // AcceptBools
                core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R8_UINT, size, "Bmfr.AcceptBools");
                mAccuImages.AcceptBools.Create(mContext, ci);
            }
        }
        {  // Setup temporary filtered image working target
            VkImageUsageFlags usage = VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT;
            {  // Input
                core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, size, "Bmfr.RegressionTmp");
                mFilterImage.Create(mContext, ci);
            }
        }

        mPreProcessStage.Init(this);
        // mRegressionStage.Init(this);
        mPostProcessStage.Init(this);
    }
    std::string BmfrDenoiser::GetUILabel()
    {
        return "BMFR Denoiser";
    }
    void BmfrDenoiser::DisplayImguiConfiguration()
    {
        const char* debugModes[] = {
            "DEBUG_NONE",
            "DEBUG_PREPROCESS_OUT",
            "DEBUG_PREPROCESS_ACCEPTS",
            "DEBUG_PREPROCESS_ALPHA",
            "DEBUG_POSTPROCESS_OUT",
            "DEBUG_POSTPROCESS_ACCEPTS",
            "DEBUG_POSTPROCESS_ALPHA",
        };
        int debugMode = (int)mDebugMode;
        if(ImGui::Combo("Debug Mode", &debugMode, debugModes, sizeof(debugModes) / sizeof(const char*)))
        {
            mDebugMode = (uint32_t)debugMode;
        }
    }
    void BmfrDenoiser::IgnoreHistoryNextFrame()
    {
        mHistory.Valid = false;
    }

    void BmfrDenoiser::RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo)
    {
        if(mHistory.Valid)
        {
            renderInfo.GetImageLayoutCache().Set(mAccuImages.Input, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
            renderInfo.GetImageLayoutCache().Set(mAccuImages.Filtered, VkImageLayout::VK_IMAGE_LAYOUT_GENERAL);
        }
        mHistory.Position.ApplyToLayoutCache(renderInfo.GetImageLayoutCache());
        mHistory.Normal.ApplyToLayoutCache(renderInfo.GetImageLayoutCache());

        mPreProcessStage.RecordFrame(cmdBuffer, renderInfo);
        // mRegressionStage.RecordFrame(cmdBuffer, renderInfo);
        mPostProcessStage.RecordFrame(cmdBuffer, renderInfo);

        std::vector<util::HistoryImage*> historyImages({&mHistory.Position, &mHistory.Normal});
        util::HistoryImage::sMultiCopySourceToHistory(historyImages, cmdBuffer, renderInfo);
        mHistory.Valid = true;
    }
    void BmfrDenoiser::OnShadersRecompiled()
    {
        mPreProcessStage.OnShadersRecompiled();
        // mRegressionStage.OnShadersRecompiled();
        mPostProcessStage.OnShadersRecompiled();
    }
    void BmfrDenoiser::Resize(const VkExtent2D& size)
    {
        std::vector<core::ManagedImage*> images({&mAccuImages.Input, &mAccuImages.Filtered, &mAccuImages.AcceptBools, &mFilterImage});
        for(core::ManagedImage* image : images)
        {
            image->Resize(size);
        }
        std::vector<util::HistoryImage*> historyImages({&mHistory.Position, &mHistory.Normal});
        for(util::HistoryImage* image : historyImages)
        {
            image->Resize(size);
        }

        mPreProcessStage.UpdateDescriptorSet();
        // mRegressionStage.UpdateDescriptorSet();
        mPostProcessStage.UpdateDescriptorSet();
        IgnoreHistoryNextFrame();
    }
    void BmfrDenoiser::Destroy()
    {
        mPostProcessStage.Destroy();
        // mRegressionStage.Destroy();
        mPreProcessStage.Destroy();
        std::vector<core::ManagedImage*> images({&mAccuImages.Input, &mAccuImages.Filtered, &mAccuImages.AcceptBools, &mFilterImage});
        for(core::ManagedImage* image : images)
        {
            image->Destroy();
        }
        std::vector<util::HistoryImage*> historyImages({&mHistory.Position, &mHistory.Normal});
        for(util::HistoryImage* image : historyImages)
        {
            image->Destroy();
        }
    }

}  // namespace foray::bmfr
