#pragma once

#include "MainPage.g.h"

namespace winrt::audioWinRT::implementation
{
    struct MainPage : MainPageT<MainPage>
    {
        using ValueVector = Windows::Foundation::Collections::IObservableVector<hstring>;

        MainPage();

        void limitUp_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void limitDown_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        winrt::event_token PropertyChanged(Windows::UI::Xaml::Data::PropertyChangedEventHandler const& handler);
        void PropertyChanged(winrt::event_token const& token) noexcept;

        int32_t LimitDB();
        void LimitDB(int32_t value);
        int32_t ReleaseDbPerSecond();
        void ReleaseDbPerSecond(int32_t value);
        ValueVector SoundDevices() { return m_soundDevices; }
        int DefaultOutputDeviceIndex() { return m_defaultOutputDeviceIndex; }
        void DefaultOutputDeviceIndex(int value) { m_defaultOutputDeviceIndex = value; }
        int OutputDeviceIndex() { return m_outputDeviceIndex; }
        void OutputDeviceIndex(int value) { m_outputDeviceIndex = value; }
        bool Playing() { return m_playing; }
        bool Stopped() { return !m_playing; }

        Windows::Foundation::IAsyncAction startButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void stopButton_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    private:
        Windows::Foundation::Collections::PropertySet m_loudnessProps;
        winrt::event<Windows::UI::Xaml::Data::PropertyChangedEventHandler> m_propertyChanged;
        ValueVector m_soundDevices;
        int m_defaultOutputDeviceIndex = 0;
        int m_outputDeviceIndex = 0;
        bool m_playing = false;

        void Playing(bool);
        Windows::Foundation::IAsyncAction enumSoundDevices();
    public:
        void dpsUp_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void dpsDown_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::audioWinRT::factory_implementation
{
    struct MainPage : MainPageT<MainPage, implementation::MainPage>
    {
    };
}
