#pragma once
#include "foray_bmfr_postprocessstage.hpp"
#include "foray_bmfr_preprocessstage.hpp"
#include "foray_bmfr_regressionstage.hpp"
#include <core/foray_managedimage.hpp>
#include <stages/foray_denoiserstage.hpp>
#include <util/foray_historyimage.hpp>
#include "shaders/debug.glsl.h"

namespace foray::bmfr {
    class PreProcessStage;
    class RegressionStage;
    class PostProcessStage;

    class BmfrDenoiser : public stages::DenoiserStage
    {
        friend PreProcessStage;
        friend RegressionStage;
        friend PostProcessStage;

      public:
        virtual void        Init(core::Context* context, const stages::DenoiserConfig& config) override;
        virtual void        RecordFrame(VkCommandBuffer cmdBuffer, base::FrameRenderInfo& renderInfo) override;
        virtual std::string GetUILabel() override;
        virtual void        DisplayImguiConfiguration() override;
        virtual void        IgnoreHistoryNextFrame() override;
        virtual void        OnShadersRecompiled() override;


        virtual void Resize(const VkExtent2D& size) override;

        virtual void Destroy() override;

      protected:
        struct
        {
            core::ManagedImage* Primary  = nullptr;
            core::ManagedImage* Position = nullptr;
            core::ManagedImage* Normal   = nullptr;
            core::ManagedImage* Motion   = nullptr;
        } mInputs;

        core::ManagedImage* mPrimaryOutput = nullptr;

        struct
        {
            core::ManagedImage Input;
            core::ManagedImage Filtered;
            core::ManagedImage AcceptBools;
            uint32_t LastInputArrayWriteIdx = 0;
            uint32_t LastFilteredArrayWriteIdx = 0;
        } mAccuImages;

        core::ManagedImage mFilterImage;

        struct
        {
            util::HistoryImage Position;
            util::HistoryImage Normal;
            bool               Valid = false;
        } mHistory;

        uint32_t mDebugMode = DEBUG_NONE;

        PreProcessStage  mPreProcessStage;
        RegressionStage  mRegressionStage;
        PostProcessStage mPostProcessStage;

        inline static const char* TIMESTAMP_PreProcess = "PreProcess";
        inline static const char* TIMESTAMP_Regression        = "Regression";
        inline static const char* TIMESTAMP_PostProcess  = "PostProcess";

        bench::DeviceBenchmark* mBenchmark = nullptr;

        bool mInitialized = false;
    };
}  // namespace foray::bmfr
