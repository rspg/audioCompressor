#pragma once

#include "MyAudioEffect.g.h"

namespace winrt::audioEffect::implementation
{
    struct MyAudioEffect : MyAudioEffectT<MyAudioEffect>
    {
        MyAudioEffect() = default;

        virtual Windows::Foundation::Collections::IVectorView<Windows::Media::MediaProperties::AudioEncodingProperties> SupportedEncodingProperties() const;
        virtual void SetEncodingProperties(Windows::Media::MediaProperties::AudioEncodingProperties encodingProperties);
        virtual void SetProperties(Windows::Foundation::Collections::IPropertySet configuration);
        virtual void ProcessFrame(Windows::Media::Effects::ProcessAudioFrameContext context);
        virtual void Close(Windows::Media::Effects::MediaEffectClosedReason reason);
        virtual void DiscardQueuedFrames();
        virtual bool TimeIndependent() const { return false; }
        virtual bool UseInputFrameForOutput() const { return false; }

    private:
        using ebu_t = kfr::ebu_r128<float>;

        Windows::Media::MediaProperties::AudioEncodingProperties m_currentEncodingProperties;
        Windows::Foundation::Collections::IPropertySet m_propertySet;
        Windows::Foundation::Collections::IPropertySet::MapChanged_revoker m_mapChangedRevoker;
        kfr::univector<float>  m_delayBuffer;
        kfr::univector<float>  m_loudnessBuffer;
        std::unique_ptr<ebu_t> m_ebu;
        concurrency::task<void> m_dbTask;
        double      m_maxDb = 0;
        double      m_limitDb = 0;
        double      m_correctDb = 0;
        double      m_releaseDbPerSecond = 0;
    };
}

namespace winrt::audioEffect::factory_implementation
{
    struct MyAudioEffect : MyAudioEffectT<MyAudioEffect, implementation::MyAudioEffect>
    {
    };
}
