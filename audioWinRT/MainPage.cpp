#include "pch.h"
#include "MainPage.h"
#include "MainPage.g.cpp"


using namespace winrt;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::ViewManagement;
using namespace Windows::Media::Capture;
using namespace Windows::Media::Audio;
using namespace Windows::Media::Effects;
using namespace Windows::Media::Render;
using namespace Windows::Media::Core;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Media::Devices;

namespace winrt::audioWinRT::implementation
{
    MainPage::MainPage()
    {
        InitializeComponent();
        ApplicationView::PreferredLaunchViewSize({ 400,400 });
        ApplicationView::PreferredLaunchWindowingMode(ApplicationViewWindowingMode::PreferredLaunchViewSize);
        ApplicationView::GetForCurrentView().TryResizeView({400, 400});

        LimitDB(-35);
        ReleaseDbPerSecond(10);

        enumSoundDevices();
    }

    int32_t MainPage::LimitDB()
    {
        auto p = m_loudnessProps.TryLookup(L"LimitDB");
        return unbox_value<int32_t>(p);
    }

    void MainPage::LimitDB(int32_t value)
    {
        m_loudnessProps.Insert(L"LimitDB", box_value(value));
        m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"LimitDB" });
    }

    int32_t MainPage::ReleaseDbPerSecond()
    {
        return unbox_value<int32_t>(m_loudnessProps.TryLookup(L"ReleaseDbPerSecond"));
    }

    void MainPage::ReleaseDbPerSecond(int32_t value)
    {
        value = max(value, 0);
        m_loudnessProps.Insert(L"ReleaseDbPerSecond", box_value(value));
        m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"ReleaseDbPerSecond" });
    }

    void MainPage::Playing(bool value)
    {
        m_playing = value;
        m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Playing" });
        m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"Stopped" });
    }

    void ChangeDefaultOutputDevice(winrt::impl::com_ref<IPolicyConfig> policyConfig, hstring id)
    {
        std::wregex mmdeviceRx(LR"(\{[a-f0-9]\.[a-f0-9]\.[a-f0-9]\.[a-f0-9]{8}\}\.\{[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}\})");
        std::wcmatch match;
        if (std::regex_search(id.c_str(), match, mmdeviceRx))
        {
            policyConfig->SetDefaultEndpoint(match[0].str().c_str(), ERole::eMultimedia);
        }
    }

    IAsyncAction MainPage::startButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        Playing(true);

        auto policyConfig = create_instance<IPolicyConfig>(__uuidof(CPolicyConfigClient));

        AudioGraphSettings settings(AudioRenderCategory::Media);

        auto defaultOutputDevice = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Default);

        auto devices = co_await DeviceInformation::FindAllAsync(Windows::Media::Devices::MediaDevice::GetAudioRenderSelector());
        for (uint32_t i = 0; i < devices.Size(); ++i)
        {
            auto device = devices.GetAt(i);

            auto x = m_soundDevices.GetAt(m_outputDeviceIndex);
            if (device.Name() == m_soundDevices.GetAt(m_outputDeviceIndex))
            {
                settings.PrimaryRenderDevice(device);
            }
            auto y = m_soundDevices.GetAt(m_defaultOutputDeviceIndex);
            if (device.Name() == m_soundDevices.GetAt(m_defaultOutputDeviceIndex))
            {
                ChangeDefaultOutputDevice(policyConfig, device.Id());
            }
        }

        auto result = co_await AudioGraph::CreateAsync(settings);
        auto graph = result.Graph();


        AudioDeviceInputNode inputNode{ nullptr };
        {
            auto input = co_await graph.CreateDeviceInputNodeAsync(MediaCategory::Media);
            inputNode = input.DeviceInputNode();
        }
        AudioDeviceOutputNode outputNode{ nullptr };
        {
            auto output = co_await graph.CreateDeviceOutputNodeAsync();
            outputNode = output.DeviceOutputNode();
        }

        inputNode.AddOutgoingConnection(outputNode);
        inputNode.EffectDefinitions().Append(AudioEffectDefinition(L"audioEffect.MyAudioEffect", m_loudnessProps));

        graph.Start();

        while(m_playing)
            co_await std::chrono::milliseconds(100);

        ChangeDefaultOutputDevice(policyConfig, defaultOutputDevice);
        policyConfig->Release();
    }


    void MainPage::stopButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        Playing(false);
    }


    void MainPage::limitUp_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        LimitDB(LimitDB() + 1);
    }

    void MainPage::limitDown_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        LimitDB(LimitDB() - 1);
    }

    void MainPage::dpsUp_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        ReleaseDbPerSecond(ReleaseDbPerSecond() + 1);
    }

    void MainPage::dpsDown_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        ReleaseDbPerSecond(ReleaseDbPerSecond() - 1);
    }


    //std::wregex mmdeviceRx(LR"(\{[a-f0-9]\.[a-f0-9]\.[a-f0-9]\}\.\{[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}})");

    Windows::Foundation::IAsyncAction MainPage::enumSoundDevices()
    {
        m_soundDevices = single_threaded_observable_vector<hstring>();

        auto devices = co_await DeviceInformation::FindAllAsync(Windows::Media::Devices::MediaDevice::GetAudioRenderSelector());
        for (auto device : devices)
        {
            m_soundDevices.Append(device.Name());
        }
        m_propertyChanged(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"SoundDevices" });
    }

    winrt::event_token MainPage::PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler)
    {
        return m_propertyChanged.add(handler);
    }

    void MainPage::PropertyChanged(winrt::event_token const& token) noexcept
    {
        m_propertyChanged.remove(token);
    }

}


