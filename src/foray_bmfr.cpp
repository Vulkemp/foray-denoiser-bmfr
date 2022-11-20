#include "foray_bmfr.hpp"
#include <bench/foray_devicebenchmark.hpp>
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
        mInputs.Albedo = config.GBufferOutputs[(size_t)stages::GBufferStage::EOutput::Albedo];
        Assert(!!mInputs.Albedo);
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
                core::ManagedImage::CreateInfo ci(usage, VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, size, "Bmfr.Regression.Out");
                mFilterImage.Create(mContext, ci);
            }
        }
        {  // Setup regression
            glm::uvec2 dispatch      = CalculateDispatchSize(size);
            mRegression.DispatchSize = dispatch;
            VkExtent2D regressionImageSize{BLOCK_EDGE * BLOCK_EDGE, dispatch.x * dispatch.y * 13};
            {  // TempData
                core::ManagedImage::CreateInfo ci(VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT, VkFormat::VK_FORMAT_R16_SFLOAT, regressionImageSize,
                                                  "Bmfr.Regression.TempData");
                mRegression.TempData.Create(mContext, ci);
            }
            {  // OutData
                core::ManagedImage::CreateInfo ci(VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT, VkFormat::VK_FORMAT_R16_SFLOAT, regressionImageSize, "Bmfr.Regression.OutData");
                mRegression.OutData.Create(mContext, ci);
            }
        }

        mPreProcessStage.Init(this);
        mRegressionStage.Init(this);
        mPostProcessStage.Init(this);

        mBenchmark = config.Benchmark;
        if(!!mBenchmark)
        {
            std::vector<const char*> queryNames(
                {bench::BenchmarkTimestamp::BEGIN, TIMESTAMP_PreProcess, TIMESTAMP_Regression, TIMESTAMP_PostProcess, bench::BenchmarkTimestamp::END});
            mBenchmark->Create(mContext, queryNames);
        }

        mInitialized = true;
    }

    glm::uvec2 BmfrDenoiser::CalculateDispatchSize(const VkExtent2D& renderSize)
    {
        glm::uvec2 size((renderSize.width + BLOCK_EDGE - 1) / BLOCK_EDGE, (renderSize.height + BLOCK_EDGE - 1) / BLOCK_EDGE);
        return size + glm::uvec2(1);
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
            "DEBUG_REGRESSION_OUT",
            "DEBUG_REGRESSION_BLOCKS",
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

        uint32_t                frameIdx = renderInfo.GetFrameNumber();
        VkPipelineStageFlagBits compute  = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

        if(!!mBenchmark)
        {
            mBenchmark->CmdResetQuery(cmdBuffer, frameIdx);
            mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, bench::BenchmarkTimestamp::BEGIN, compute);
        }
        mPreProcessStage.RecordFrame(cmdBuffer, renderInfo);
        if(!!mBenchmark)
        {
            mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_PreProcess, compute);
        }
        mRegressionStage.RecordFrame(cmdBuffer, renderInfo);
        if(!!mBenchmark)
        {
            mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_Regression, compute);
        }
        mPostProcessStage.RecordFrame(cmdBuffer, renderInfo);
        if(!!mBenchmark)
        {
            mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, TIMESTAMP_PostProcess, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT);
        }

        std::vector<util::HistoryImage*> historyImages({&mHistory.Position, &mHistory.Normal});
        util::HistoryImage::sMultiCopySourceToHistory(historyImages, cmdBuffer, renderInfo);
        mHistory.Valid = true;
        if(!!mBenchmark)
        {
            mBenchmark->CmdWriteTimestamp(cmdBuffer, frameIdx, bench::BenchmarkTimestamp::END, VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
        }
    }
    void BmfrDenoiser::OnShadersRecompiled()
    {
        mPreProcessStage.OnShadersRecompiled();
        mRegressionStage.OnShadersRecompiled();
        mPostProcessStage.OnShadersRecompiled();
    }
    void BmfrDenoiser::Resize(const VkExtent2D& size)
    {
        if(!mInitialized)
        {
            return;
        }

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

        {  // Setup regression
            glm::uvec2 dispatch      = CalculateDispatchSize(size);
            mRegression.DispatchSize = dispatch;
            VkExtent2D regressionImageSize{BLOCK_EDGE * BLOCK_EDGE, dispatch.x * dispatch.y * 13};
            mRegression.TempData.Resize(regressionImageSize);
            mRegression.OutData.Resize(regressionImageSize);
        }


        mPreProcessStage.UpdateDescriptorSet();
        mRegressionStage.UpdateDescriptorSet();
        mPostProcessStage.UpdateDescriptorSet();
        IgnoreHistoryNextFrame();
    }
    void BmfrDenoiser::Destroy()
    {
        mInitialized = false;

        mPostProcessStage.Destroy();
        mRegressionStage.Destroy();
        mPreProcessStage.Destroy();
        std::vector<core::ManagedImage*> images({&mAccuImages.Input, &mAccuImages.Filtered, &mAccuImages.AcceptBools, &mFilterImage, &mRegression.TempData, &mRegression.OutData});
        for(core::ManagedImage* image : images)
        {
            image->Destroy();
        }
        std::vector<util::HistoryImage*> historyImages({&mHistory.Position, &mHistory.Normal});
        for(util::HistoryImage* image : historyImages)
        {
            image->Destroy();
        }

        if(!!mBenchmark)
        {
            mBenchmark->Destroy();
            mBenchmark = nullptr;
        }
    }

}  // namespace foray::bmfr
