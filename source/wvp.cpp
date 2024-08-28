#include "wvp.h"
#include "wvp_model.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <vamp-sdk/PluginAdapter.h>

#if defined(_MSC_VER)
#define forcedinline __forceinline
#else
#define forcedinline inline __attribute__((always_inline))
#endif

#if _WIN32
#include <Windows.h>
#include <shlobj.h>
static std::string getSpecialFolderPath(int type)
{
    char path[MAX_PATH + 256];
    if(SHGetSpecialFolderPathA(nullptr, path, type, FALSE))
    {
        return std::string(path);
    }
    return {};
}
#endif

namespace ResamplerUtils
{
    template <int k>
    struct LagrangeResampleHelper
    {
        static forcedinline void calc(float& a, float b) noexcept { a *= b * (1.0f / k); }
    };

    template <>
    struct LagrangeResampleHelper<0>
    {
        static forcedinline void calc(float&, float) noexcept {}
    };

    template <int k>
    static float calcCoefficient(float input, float offset) noexcept
    {
        LagrangeResampleHelper<0 - k>::calc(input, -2.0f - offset);
        LagrangeResampleHelper<1 - k>::calc(input, -1.0f - offset);
        LagrangeResampleHelper<2 - k>::calc(input, 0.0f - offset);
        LagrangeResampleHelper<3 - k>::calc(input, 1.0f - offset);
        LagrangeResampleHelper<4 - k>::calc(input, 2.0f - offset);
        return input;
    }

    static float valueAtOffset(const float* inputs, float offset, int index) noexcept
    {
        auto result = 0.0f;
        result += calcCoefficient<0>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<1>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<2>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<3>(inputs[index], offset);
        index = (++index % 5);
        result += calcCoefficient<4>(inputs[index], offset);
        return result;
    }
} // namespace ResamplerUtils

namespace Wvp
{
    static std::vector<std::filesystem::path> getModelPaths(std::filesystem::path const& path)
    {
        if(std::filesystem::exists(path))
        {
            std::vector<std::filesystem::path> paths;
            for(auto const& entry : std::filesystem::directory_iterator{path})
            {
                if(entry.path().extension().string() == ".bin")
                {
                    paths.push_back(entry.path());
                }
            }
            return paths;
        }
        return {};
    }

    static std::vector<std::filesystem::path> getModelPaths()
    {
        std::vector<std::filesystem::path> allPaths;
        if(auto const* envModelPath = std::getenv("WHISPERMODELSPATH"))
        {
            auto const path = getModelPaths(std::filesystem::path{envModelPath});
            allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
        }
#ifdef __APPLE__
        if(auto const* userPath = std::getenv("HOME"))
        {
            auto const path = getModelPaths({std::filesystem::path(userPath) / "Library/Application Support/Ircam/whispermodels"});
            allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
        }
        auto const path = getModelPaths({"/Library/Application Support/Ircam/whispermodels"});
        allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
#elif __linux__
        if(auto const* userPath = std::getenv("HOME"))
        {
            auto const path = getModelPaths({std::filesystem::path(userPath) / ".config/Ircam/whispermodels"});
            allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
        }
        auto const path = getModelPaths({"/opt/Ircam/whispermodels"});
        allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
#elif _WIN32
        auto const userPath = getSpecialFolderPath(CSIDL_APPDATA);
        if(!userPath.empty())
        {
            auto const path = getModelPaths({std::filesystem::path(userPath) / "Ircam/whispermodels"});
            allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
        }
        auto const commonPath = getSpecialFolderPath(CSIDL_COMMON_APPDATA);
        if(!commonPath.empty())
        {
            auto const path = getModelPaths({std::filesystem::path(commonPath) / "Ircam/whispermodels"});
            allPaths.insert(allPaths.cend(), path.cbegin(), path.cend());
        }
#endif
        return allPaths;
    }
} // namespace Wvp

void Wvp::Plugin::Resampler::prepare(double sampleRate)
{
    mSourceSampleRate = sampleRate;
    reset();
}

std::tuple<size_t, size_t> Wvp::Plugin::Resampler::process(size_t numInputSamples, float const* inputBuffer, size_t numOutputSamples, float* outputBuffer)
{
    double const speedRatio = getRatio();
    size_t numGeneratedSamples = 0;
    size_t numUsedSamples = 0;
    auto subSamplePos = mSubSamplePos;
    while(numUsedSamples < numInputSamples && numGeneratedSamples < numOutputSamples)
    {
        while(subSamplePos >= 1.0 && numUsedSamples < numInputSamples)
        {
            mLastInputSamples[mIndexBuffer] = inputBuffer[numUsedSamples++];
            if(++mIndexBuffer == mLastInputSamples.size())
            {
                mIndexBuffer = 0;
            }
            subSamplePos -= 1.0;
        }
        if(subSamplePos < 1.0)
        {
            outputBuffer[numGeneratedSamples++] = ResamplerUtils::valueAtOffset(mLastInputSamples.data(), static_cast<float>(subSamplePos), static_cast<int>(mIndexBuffer));
            subSamplePos += speedRatio;
        }
    }
    while(subSamplePos >= 1.0 && numUsedSamples < numInputSamples)
    {
        mLastInputSamples[mIndexBuffer] = inputBuffer[numUsedSamples++];
        if(++mIndexBuffer == mLastInputSamples.size())
        {
            mIndexBuffer = 0;
        }
        subSamplePos -= 1.0;
    }
    mSubSamplePos = subSamplePos;
    return std::make_tuple(numUsedSamples, numGeneratedSamples);
}

void Wvp::Plugin::Resampler::reset()
{
    mIndexBuffer = 0;
    mSubSamplePos = 1.0;
    std::fill(mLastInputSamples.begin(), mLastInputSamples.end(), 0.0f);
}

void Wvp::Plugin::Resampler::setTargetSampleRate(double sampleRate) noexcept
{
    mTargetSampleRate = sampleRate;
}

double Wvp::Plugin::Resampler::getRatio() const noexcept
{
    return mSourceSampleRate / mTargetSampleRate;
}

Wvp::Plugin::Plugin(float inputSampleRate)
: Vamp::Plugin(inputSampleRate)
{
    whisper_log_set([](enum ggml_log_level level, const char* text, void* user_data)
                    {
                        if(level <= GGML_LOG_LEVEL_WARN)
                        {
                            std::cerr << text << "\n";
                        }
                    },
                    nullptr);
    mResampler.prepare(static_cast<double>(inputSampleRate));
}

bool Wvp::Plugin::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if(channels != static_cast<size_t>(1) || stepSize != blockSize)
    {
        return false;
    }
    reset();
    mBuffer.reserve(gModelSampleRate * 2);
    mBlockSize = blockSize;
    return mHandle != nullptr;
}

std::string Wvp::Plugin::getIdentifier() const
{
    return "whisper";
}

std::string Wvp::Plugin::getName() const
{
    return "Whisper";
}

std::string Wvp::Plugin::getDescription() const
{
    return "Automatic speech recognition using OpenAI's Whisper model.";
}

std::string Wvp::Plugin::getMaker() const
{
    return "Ircam";
}

int Wvp::Plugin::getPluginVersion() const
{
    return WVP_PLUGIN_VERSION;
}

std::string Wvp::Plugin::getCopyright() const
{
    return "Whisper models by OpenAI. Whisper.cpp by Georgi Gerganov. Whisper Vamp Plugin by Pierre Guillot. Copyright 2024 Ircam. All rights reserved.";
}

Wvp::Plugin::InputDomain Wvp::Plugin::getInputDomain() const
{
    return TimeDomain;
}

size_t Wvp::Plugin::getPreferredBlockSize() const
{
    return static_cast<size_t>(1024);
}

size_t Wvp::Plugin::getPreferredStepSize() const
{
    return static_cast<size_t>(0);
}

Ive::PluginExtension::InputList Wvp::Plugin::getInputDescriptors() const
{
    OutputDescriptor d;
    d.identifier = "token";
    d.name = "Region";
    d.description = "Time regions used to separate token generation sequences";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = 0;
    d.hasKnownExtents = false;
    d.minValue = 0.0f;
    d.maxValue = 0.0f;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::SampleType::VariableSampleRate;
    d.hasDuration = false;
    return {d};
}

void Wvp::Plugin::setPreComputingFeatures(FeatureSet const& fs)
{
    mRanges.clear();
    auto const sampleRate = static_cast<int>(getInputSampleRate());
    auto const it = fs.find(0);
    if(it != fs.cend())
    {
        for(auto const& feature : it->second)
        {
            mRanges.insert(static_cast<size_t>(Vamp::RealTime::realTime2Frame(feature.timestamp, sampleRate)));
        }
    }
}

Wvp::Plugin::OutputList Wvp::Plugin::getOutputDescriptors() const
{
    OutputDescriptor d;
    d.identifier = "token";
    d.name = "Token";
    d.description = "Tokens generated by text-to-speech transcription";
    d.unit = "";
    d.hasFixedBinCount = true;
    d.binCount = static_cast<size_t>(0);
    d.hasKnownExtents = false;
    d.minValue = 0.0f;
    d.maxValue = 0.0f;
    d.isQuantized = false;
    d.sampleType = OutputDescriptor::SampleType::VariableSampleRate;
    d.hasDuration = true;
    return {d};
}

void Wvp::Plugin::reset()
{
    auto const createContext = [this]() -> struct whisper_context*
    {
        auto params = whisper_context_default_params();
        if(mModelIndex == 0)
        {
            return whisper_init_from_buffer_with_params(const_cast<void*>(Wvp::model), Wvp::model_size, params);
        }
        auto const models = getModelPaths();
        if(mModelIndex <= models.size())
        {
            auto const path = models.at(mModelIndex - 1).string();
            return whisper_init_from_file_with_params(path.c_str(), params);
        }
        return nullptr;
    };
    mHandle = handle_uptr(createContext(), [](whisper_context* ctx)
                          {
                              if(ctx != nullptr)
                              {
                                  whisper_free(ctx);
                              }
                          });
    mBuffer.clear();
    mRanges.clear();
    mBufferPosition = 0;
    mAdvancement = 0;
    mResampler.reset();
}

Wvp::Plugin::ParameterList Wvp::Plugin::getParameterDescriptors() const
{
    ParameterList list;
    auto const models = getModelPaths();
    if(!models.empty())
    {
        ParameterDescriptor param;
        param.identifier = "model";
        param.name = "Model";
        param.description = "The model used to generate the tokens";
        param.unit = "";
        param.valueNames.push_back("embedded");
        for(auto const& model : models)
        {
            param.valueNames.push_back(model.filename().replace_extension().string());
        }
        param.minValue = 0.0f;
        param.maxValue = static_cast<float>(models.size());
        param.defaultValue = 0.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    {
        ParameterDescriptor param;
        param.identifier = "splitmode";
        param.name = "Split Mode";
        param.description = "The model splits the text on sentences, words or tokens";
        param.unit = "";
        param.valueNames = {"Sentences", "Words", "Tokens"};
        param.minValue = 0.0f;
        param.maxValue = 2.0f;
        param.defaultValue = 2.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    {
        ParameterDescriptor param;
        param.identifier = "suppressnonspeechtokens";
        param.name = "Suppress Non-Speech Tokens";
        param.description = "The model suppresses non-speech tokens";
        param.unit = "";
        param.minValue = 0.0f;
        param.maxValue = 1.0f;
        param.defaultValue = 1.0f;
        param.isQuantized = true;
        param.quantizeStep = 1.0f;
        list.push_back(std::move(param));
    }
    return list;
}

void Wvp::Plugin::setParameter(std::string paramid, float newval)
{
    if(paramid == "model")
    {
        auto const max = static_cast<float>(getModelPaths().size());
        mModelIndex = static_cast<size_t>(std::floor(std::clamp(newval, 0.0f, max)));
    }
    else if(paramid == "splitmode")
    {
        mSplitMode = static_cast<size_t>(std::floor(std::clamp(newval, 0.0f, 2.0f)));
    }
    else if(paramid == "suppressnonspeechtokens")
    {
        mSuppressNonSpeechTokens = newval > 0.5f;
    }
    else
    {
        std::cerr << "Invalid parameter : " << paramid << "\n";
    }
}

float Wvp::Plugin::getParameter(std::string paramid) const
{
    if(paramid == "model")
    {
        return static_cast<float>(mModelIndex);
    }
    if(paramid == "splitmode")
    {
        return static_cast<float>(mSplitMode);
    }
    if(paramid == "suppressnonspeechtokens")
    {
        return mSuppressNonSpeechTokens ? 1.0f : 0.0f;
    }
    std::cerr << "Invalid parameter : " << paramid << "\n";
    return 0.0f;
}

Wvp::Plugin::OutputExtraList Wvp::Plugin::getOutputExtraDescriptors(size_t outputDescriptorIndex) const
{
    OutputExtraList list;
    if(outputDescriptorIndex == 0)
    {
        OutputExtraDescriptor d;
        d.identifier = "probability";
        d.name = "Probability";
        d.description = "The probability of the generated token";
        d.unit = "";
        d.hasKnownExtents = true;
        d.minValue = 0.0f;
        d.maxValue = 1.0f;
        d.isQuantized = false;
        d.quantizeStep = 0.0f;
        list.push_back(std::move(d));
    }
    return list;
}

Wvp::Plugin::FeatureList Wvp::Plugin::getCurrentFeatures(size_t timeOffset)
{
    auto params = whisper_full_default_params(whisper_sampling_strategy::WHISPER_SAMPLING_GREEDY);
    params.no_context = true;
    params.print_progress = false;
    params.print_timestamps = false;
    params.print_special = false;
    params.translate = false;
    params.suppress_non_speech_tokens = mSuppressNonSpeechTokens;
    params.language = nullptr;
    params.token_timestamps = mSplitMode >= 1;
    params.max_len = mSplitMode == 1;
    params.split_on_word = mSplitMode == 1;
    static auto const minSize = gModelSampleRate + gModelSampleRate / 10;
    if(mBufferPosition < minSize)
    {
        mBuffer.resize(minSize, 0.0f);
        mBufferPosition = minSize;
    }
    if(whisper_full(mHandle.get(), params, mBuffer.data(), static_cast<int>(mBufferPosition)) != 0)
    {
        std::cerr << "Failed to process\n";
    }
    mBuffer.clear();
    mBufferPosition = 0;
    FeatureList fl;
    auto const offset = Vamp::RealTime::frame2RealTime(static_cast<long>(timeOffset), static_cast<int>(getInputSampleRate()));
    auto const nsegments = whisper_full_n_segments(mHandle.get());
    for(int i = 0; i < nsegments; ++i)
    {
        if(mSplitMode < 2)
        {
            auto const* text = whisper_full_get_segment_text(mHandle.get(), i);
            auto const t0 = whisper_full_get_segment_t0(mHandle.get(), i);
            auto const t1 = whisper_full_get_segment_t1(mHandle.get(), i);
            Feature feature;
            feature.hasTimestamp = true;
            auto const time = Vamp::RealTime::fromSeconds(static_cast<double>(t0) / 100.0);
            feature.timestamp = time + offset;
            feature.hasDuration = true;
            feature.duration = Vamp::RealTime::fromSeconds(static_cast<double>(t1) / 100.0) - time;
            feature.label = text;
            feature.values.push_back(1.0);
            fl.push_back(std::move(feature));
        }
        else
        {
            auto const ntokens = whisper_full_n_tokens(mHandle.get(), i);
            for(int j = 0; j < ntokens; ++j)
            {
                auto const data = whisper_full_get_token_data(mHandle.get(), i, j);
                if(!mSuppressNonSpeechTokens || data.id < whisper_token_eot(mHandle.get()))
                {
                    Feature feature;
                    feature.hasTimestamp = true;
                    auto const time = Vamp::RealTime::fromSeconds(static_cast<double>(data.t0) / 100.0);
                    feature.timestamp = time + offset;
                    feature.hasDuration = true;
                    feature.duration = Vamp::RealTime::fromSeconds(static_cast<double>(data.t1) / 100.0) - time;
                    feature.label = whisper_full_get_token_text(mHandle.get(), i, j);
                    feature.values.push_back(data.p);
                    fl.push_back(std::move(feature));
                }
            }
        }
    }
    return fl;
}

Wvp::Plugin::FeatureSet Wvp::Plugin::process(float const* const* inputBuffers, [[maybe_unused]] Vamp::RealTime timestamp)
{
    auto blockSize = mBlockSize;
    auto const* inputBuffer = inputBuffers[0];
    size_t inputPosition = 0;
    auto const scaleRatio = static_cast<double>(gModelSampleRate) / static_cast<double>(getInputSampleRate());
    auto const pushSamples = [&](size_t subBlockSize)
    {
        auto const remaining = mBuffer.size() - mBufferPosition;
        auto const scaleSize = static_cast<size_t>(std::ceil(static_cast<double>(subBlockSize) * scaleRatio));
        if(remaining < scaleSize)
        {
            mBuffer.resize(mBuffer.size() + (scaleSize - remaining), 0.0f);
        }
        auto const result = mResampler.process(subBlockSize, inputBuffer + inputPosition, scaleSize, mBuffer.data() + mBufferPosition);
        if(std::get<0>(result) != subBlockSize)
        {
            std::cerr << "Missing input samples\n";
        }
        mBufferPosition += std::get<1>(result);
        mAdvancement += subBlockSize;
        inputPosition += subBlockSize;
        blockSize -= subBlockSize;
    };

    FeatureList fl;
    while(blockSize > 0)
    {
        auto const nextTime = mRanges.upper_bound(mAdvancement);
        if(nextTime != mRanges.cend())
        {
            auto const diffSamples = *nextTime - mAdvancement;
            if(diffSamples > blockSize)
            {
                pushSamples(blockSize);
            }
            else
            {
                pushSamples(diffSamples);
                auto const result = getCurrentFeatures(nextTime == mRanges.cbegin() ? 0.0 : *std::prev(nextTime));
                fl.insert(fl.end(), result.cbegin(), result.cend());
            }
        }
        else
        {
            pushSamples(blockSize);
        }
    }
    return {{0, fl}};
}

Wvp::Plugin::FeatureSet Wvp::Plugin::getRemainingFeatures()
{
    if(mRanges.empty())
    {
        return {{0, getCurrentFeatures(0.0)}};
    }
    auto const nextTime = mRanges.upper_bound(mAdvancement);
    return {{0, getCurrentFeatures(nextTime == mRanges.cbegin() ? 0.0 : *std::prev(nextTime))}};
}

#ifdef __cplusplus
extern "C"
{
#endif
    VampPluginDescriptor const* vampGetPluginDescriptor(unsigned int version, unsigned int index)
    {
        if(version < 1)
        {
            return nullptr;
        }
        switch(index)
        {
            case 0:
            {
                static Vamp::PluginAdapter<Wvp::Plugin> adaptater;
                return adaptater.getDescriptor();
            }
            default:
            {
                return nullptr;
            }
        }
    }

    IVE_EXTERN IvePluginDescriptor const* iveGetPluginDescriptor(unsigned int version, unsigned int index)
    {
        if(version < 1)
        {
            return nullptr;
        }
        switch(index)
        {
            case 0:
            {
                return Ive::PluginAdapter::getDescriptor<Wvp::Plugin>();
            }
            default:
            {
                return nullptr;
            }
        }
    }
#ifdef __cplusplus
}
#endif
