#include "lsdev_app.hpp"

LsDevApp::LsDevApp(const CommandLineOptions& options,
         IDeviceEnumerator& enumerator,
         const DeviceFilter& filter,
         const IDeviceRenderer& renderer)
    : m_options(options),
      m_enumerator(enumerator),
      m_filter(filter),
      m_renderer(renderer) {}

int LsDevApp::Run() {
    if (m_options.showHelp) {
        m_renderer.ShowHelp();
        return 0;
    }

    auto allDevices = m_enumerator.EnumerateDevices(m_filter.IncludesNonPresentDevices());
    auto filtered = m_filter.Apply(allDevices);

    m_renderer.Render(filtered);

    return 0;
}
