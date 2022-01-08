#include "pch.h"
#include "MyAudioEffect.h"
#include "MyAudioEffect.g.cpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Media;
using namespace winrt::Windows::Media::MediaProperties;
using namespace kfr;
//using namespace concurrency;

namespace winrt::audioEffect::implementation
{
    struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
    {
        virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
    };

    IVectorView<AudioEncodingProperties> MyAudioEffect::SupportedEncodingProperties() const
    {
        IVector<AudioEncodingProperties> properties{ winrt::single_threaded_vector<AudioEncodingProperties>() };
        AudioEncodingProperties encodingProps1 = AudioEncodingProperties::CreatePcm(44100, 2, 32);
        encodingProps1.Subtype(MediaEncodingSubtypes::Float());
        AudioEncodingProperties encodingProps2 = AudioEncodingProperties::CreatePcm(48000, 2, 32);
        encodingProps2.Subtype(MediaEncodingSubtypes::Float());
        properties.Append(encodingProps1);
        properties.Append(encodingProps2);
        return properties.GetView();
    }

    void MyAudioEffect::SetEncodingProperties(AudioEncodingProperties encodingProperties)
    {
        m_currentEncodingProperties = encodingProperties;
        m_ebu = std::make_unique<ebu_t>(encodingProperties.SampleRate(), std::vector<Speaker> { Speaker::Mono });

        m_delayBuffer.resize((size_t)floor(encodingProperties.SampleRate()*encodingProperties.ChannelCount()*0.2));
    }

    void MyAudioEffect::SetProperties(Windows::Foundation::Collections::IPropertySet configuration)
    {
        auto updateLimitDb = [this]()
        {
            auto p = m_propertySet.TryLookup(L"LimitDB");
            if (p)
            {
                m_limitDb = (double)unbox_value<int32_t>(p);
            }
        };
        auto updateReleaseDbPerSecond = [this]()
        {
            auto p = m_propertySet.TryLookup(L"ReleaseDbPerSecond");
            if (p)
            {
                m_releaseDbPerSecond = (double)unbox_value<int32_t>(p);
            }
        };

        m_propertySet = configuration;
        m_mapChangedRevoker = m_propertySet.MapChanged(auto_revoke, [this, updateLimitDb, updateReleaseDbPerSecond](auto sender, auto value)
        {
                if (value.Key() == L"LimitDB")
                {
                    updateLimitDb();
                }
                if (value.Key() == L"ReleaseDbPerSecond")
                {
                    updateReleaseDbPerSecond();
                }
        });
        updateLimitDb();
        updateReleaseDbPerSecond();
    }

    void MyAudioEffect::ProcessFrame(Windows::Media::Effects::ProcessAudioFrameContext context)
    {
        auto inputLockedBuffer = context.InputFrame().LockBuffer(AudioBufferAccessMode::Read);
        auto inputBufferReference = inputLockedBuffer.CreateReference();
        auto inputBuffer = inputBufferReference.as<IMemoryBufferByteAccess>();

        uint8_t* inputBufferPtr = nullptr;
        uint32_t inputBufferLength = 0;
        inputBuffer->GetBuffer(&inputBufferPtr, &inputBufferLength);
        
        auto outputLockBuffer = context.OutputFrame().LockBuffer(AudioBufferAccessMode::Write);
        auto outputBufferReference = outputLockBuffer.CreateReference();
        auto outputBuffer = outputBufferReference.as<IMemoryBufferByteAccess>();

        uint8_t* outputBufferPtr = nullptr;
        uint32_t outputBufferLength = 0;
        outputBuffer->GetBuffer(&outputBufferPtr, &outputBufferLength);

        {
            const auto channles = m_currentEncodingProperties.ChannelCount();
            const auto samples = outputBufferLength / sizeof(float);
            const auto samplesMono = samples / channles;
           
            float* inputBufferFloatPtr = (float*)inputBufferPtr;

            {
                for (size_t i = 0; i < samplesMono; ++i)
                {
                    auto l = inputBufferFloatPtr[i * channles + 0];
                    auto r = inputBufferFloatPtr[i * channles + 1];
                    m_loudnessBuffer.push_back((l + r) * 0.5f);
                }

                if (m_dbTask == concurrency::task<void>() || m_dbTask.is_done())
                {
                    if (m_loudnessBuffer.size() >= m_ebu->packet_size())
                    {
                        m_dbTask = concurrency::create_task([this, in = std::move(m_loudnessBuffer)]() mutable
                            {
                                float M, S, I, RL, RH;
                                float maxM = -HUGE_VALF, maxS = -HUGE_VALF;
                                for (size_t i = 0; i < in.size() / m_ebu->packet_size(); ++i)
                                {
                                    std::vector<univector_ref<float>> channels;
                                    channels.push_back(in.slice(i * m_ebu->packet_size(), m_ebu->packet_size()));
                                    m_ebu->process_packet(channels);
                                    m_ebu->get_values(M, S, I, RL, RH);
                                    maxM = std::max(maxM, M);
                                    maxS = std::max(maxS, S);
                                }

                                m_maxDb = maxM;

                                //char str[256];;
                                //sprintf_s(str, "maxM : %f, maxS : %f\n", (float)maxM, (float)maxS);
                                //OutputDebugStringA(str);
                            });
                        m_loudnessBuffer.clear();
                    }
                }
            }

            const auto difference = m_limitDb - m_maxDb;
            if (!isinf(difference))
            {
                if (m_correctDb > difference)
                    m_correctDb = difference;
                else
                {
                    const auto elapsedTime = samplesMono / (double)m_currentEncodingProperties.SampleRate();
                    m_correctDb += std::min(difference - m_correctDb, m_releaseDbPerSecond) * elapsedTime;
                    m_correctDb = std::min(m_correctDb, difference);
                }
            }
            const auto power = std::min((float)dB_to_amp(m_correctDb), 1.0f);

            std::copy(
                m_delayBuffer.begin() + samples, m_delayBuffer.end(), m_delayBuffer.begin());
            std::copy(
                inputBufferFloatPtr, inputBufferFloatPtr + samples, m_delayBuffer.end() - samples);

            float* outputBufferFloatPtr = (float*)outputBufferPtr;
            for (uint32_t i = 0; i < samples; ++i)
            {
                outputBufferFloatPtr[i] = m_delayBuffer[i] * power;
            }
        }

        inputBufferReference.Close();
        inputLockedBuffer.Close();

        outputBufferReference.Close();
        outputLockBuffer.Close();
    }

    void MyAudioEffect::Close(Windows::Media::Effects::MediaEffectClosedReason reason)
    {
        if(m_dbTask != concurrency::task<void>())
            m_dbTask.wait();
        if (m_mapChangedRevoker)
            m_mapChangedRevoker.revoke();
    }

    void MyAudioEffect::DiscardQueuedFrames()
    {
        if (m_dbTask != concurrency::task<void>())
            m_dbTask.wait();
    }
}
