#pragma once

#include <IvePluginAdapter.hpp>
#include <array>
#include <memory>
#include <set>
#include <whisper.h>

namespace Wvp
{
    class Plugin
    : public Vamp::Plugin
    , public Ive::PluginExtension
    {
    public:
        Plugin(float inputSampleRate);
        ~Plugin() override = default;

        // Vamp::Plugin
        bool initialise(size_t channels, size_t stepSize, size_t blockSize) override;

        InputDomain getInputDomain() const override;

        std::string getIdentifier() const override;
        std::string getName() const override;
        std::string getDescription() const override;
        std::string getMaker() const override;
        int getPluginVersion() const override;
        std::string getCopyright() const override;

        size_t getPreferredBlockSize() const override;
        size_t getPreferredStepSize() const override;
        OutputList getOutputDescriptors() const override;

        void reset() override;
        FeatureSet process(float const* const* inputBuffers, Vamp::RealTime timestamp) override;
        FeatureSet getRemainingFeatures() override;

        ParameterList getParameterDescriptors() const override;
        float getParameter(std::string paramid) const override;
        void setParameter(std::string paramid, float newval) override;

        // Ive::PluginExtension
        InputList getInputDescriptors() const override;
        void setPreComputingFeatures(FeatureSet const& fs) override;

        OutputExtraList getOutputExtraDescriptors(size_t outputDescriptorIndex) const override;

    private:
        FeatureList getCurrentFeatures(size_t timeOffset);

        static auto constexpr gModelSampleRate = 16000;

        class Resampler
        {
        public:
            Resampler() = default;
            ~Resampler() = default;

            void prepare(double sampleRate);
            std::tuple<size_t, size_t> process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer);
            void reset();

            void setTargetSampleRate(double sampleRate) noexcept;
            double getRatio() const noexcept;

        private:
            double mSourceSampleRate{48000.0};
            double mTargetSampleRate{static_cast<double>(gModelSampleRate)};
            std::array<float, 5> mLastInputSamples;
            double mSubSamplePos{1.0};
            size_t mIndexBuffer{0};
        };

        using handle_uptr = std::unique_ptr<whisper_context, void (*)(whisper_context*)>;
        handle_uptr mHandle{nullptr, nullptr};
        Resampler mResampler;
        std::vector<float> mBuffer;
        size_t mBufferPosition{0};
        size_t mAdvancement{0};
        size_t mBlockSize{0};
        size_t mModelIndex{0};
        size_t mSplitMode{2};
        bool mSuppressNonSpeechTokens{true};
        std::set<size_t> mRanges;
    };
} // namespace Wvp
