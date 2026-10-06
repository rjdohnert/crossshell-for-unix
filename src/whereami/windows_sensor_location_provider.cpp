#include "location_report.hpp"
#include "windows_sensor_location_provider.hpp"

std::string WindowsSensorLocationProvider::get_name() const  { return "Windows Sensor / Location API"; }

bool WindowsSensorLocationProvider::query(LocationReport& out_report, int /*timeout_seconds*/, bool verbose)  {
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        bool co_initialized = SUCCEEDED(hr);

        ILocation* pLocation = nullptr;
        hr = CoCreateInstance(CLSID_Location, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pLocation));
        if (FAILED(hr) || !pLocation) {
            if (verbose) std::cerr << "[whereami:sensor] Windows Location API not available on this device.\n";
            if (co_initialized) CoUninitialize();
            return false;
        }

        IID reportTypes[] = { IID_ILatLongReport };
        pLocation->RequestPermissions(NULL, reportTypes, 1, FALSE);

        ILocationReport* pReport = nullptr;
        hr = pLocation->GetReport(IID_ILatLongReport, &pReport);
        if (FAILED(hr) || !pReport) {
            if (verbose) std::cerr << "[whereami:sensor] No active GPS/Wi-Fi sensor lock.\n";
            pLocation->Release();
            if (co_initialized) CoUninitialize();
            return false;
        }

        ILatLongReport* pLatLong = nullptr;
        hr = pReport->QueryInterface(IID_PPV_ARGS(&pLatLong));
        if (SUCCEEDED(hr) && pLatLong) {
            DOUBLE lat = 0, lon = 0, err = 0, alt = 0;
            pLatLong->GetLatitude(&lat);
            pLatLong->GetLongitude(&lon);
            pLatLong->GetErrorRadius(&err);

            out_report.coords.latitude = lat;
            out_report.coords.longitude = lon;
            out_report.coords.accuracy = err;
            out_report.coords.has_accuracy = (err > 0.0);

            if (SUCCEEDED(pLatLong->GetAltitude(&alt))) {
                out_report.coords.altitude = alt;
                out_report.coords.has_altitude = true;
            }

            out_report.provider_name = get_name();
            out_report.success = true;
            pLatLong->Release();
        }

        pReport->Release();
        pLocation->Release();
        if (co_initialized) CoUninitialize();
        return out_report.success;
    }
