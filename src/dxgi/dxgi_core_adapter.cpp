#include <cstring>
#include <cwchar>

#include "dxgi_adapter.h"
#include "dxgi_core_adapter.h"

// DEFINE_GUID inside the vendored dxcore_interface.h only declares
// these; emit definitions exactly once here. We avoid INITGUID at
// the top of the TU because that would also try to redefine GUIDs
// pulled in transitively from the DXGI / D3D headers.
extern "C" {
  const GUID DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS =
    { 0x8c47866b, 0x7583, 0x450d, { 0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b } };
  const GUID DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS =
    { 0x0c9ece4d, 0x2f6e, 0x4f01, { 0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1 } };
  const GUID DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE =
    { 0x248e2800, 0xa793, 0x4724, { 0xab, 0xaa, 0x23, 0xa6, 0xde, 0x1b, 0xe0, 0x90 } };
}

#include "../util/log/log.h"
#include "../util/util_string.h"

// Trace switch for the IDXCoreAdapter stub. Default ON while we are
// chasing the Witcher 3 next-gen "DX12 not supported" gate. Flip to
// 0 once the gate is open and the trace is no longer interesting.
#define DXVK_LOG_DXCORE 1

namespace dxvk {

  namespace {

#if DXVK_LOG_DXCORE
    // Well-known DXCore attribute GUIDs. We stringify these to symbolic
    // names so the trace is grep-friendly.
    static const GUID kAttrD3D12Graphics =
      { 0x0c9ece4d, 0x2f6e, 0x4f01, { 0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1 } };
    static const GUID kAttrD3D12CoreCompute =
      { 0x248e2800, 0xa793, 0x4724, { 0xab, 0xaa, 0x23, 0xa6, 0xde, 0x1b, 0xe0, 0x90 } };
    static const GUID kAttrD3D11Graphics =
      { 0x8c47866b, 0x7583, 0x450d, { 0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b } };
    static const GUID kAttrD3D12GenericMl =
      { 0xb71b0d41, 0x1088, 0x422f, { 0xa2, 0x7c, 0x02, 0x50, 0xb7, 0xf3, 0xa5, 0x1e } };
    static const GUID kAttrHwTypeGpu =
      { 0x8eda5d6b, 0xee2a, 0x4d9b, { 0xa9, 0x25, 0x2b, 0xff, 0x3a, 0x26, 0xa3, 0x7a } };

    static const char* DxcoreAttrName(REFGUID g) {
      if (g == kAttrD3D12Graphics)     return "DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS";
      if (g == kAttrD3D12CoreCompute)  return "DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE";
      if (g == kAttrD3D11Graphics)     return "DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS";
      if (g == kAttrD3D12GenericMl)    return "DXCORE_ADAPTER_ATTRIBUTE_D3D12_GENERIC_ML";
      if (g == kAttrHwTypeGpu)         return "DXCORE_HARDWARE_TYPE_ATTRIBUTE_GPU";
      return nullptr;
    }

    static std::string DxcoreAttrToString(REFGUID g) {
      if (const char* name = DxcoreAttrName(g))
        return std::string(name);
      return std::string("<unknown> ") + str::format(g);
    }

    static const char* DxcorePropName(DXCoreAdapterProperty p) {
      switch (p) {
        case DXCoreAdapterProperty::InstanceLuid:                 return "InstanceLuid";
        case DXCoreAdapterProperty::DriverVersion:                return "DriverVersion";
        case DXCoreAdapterProperty::DriverDescription:            return "DriverDescription";
        case DXCoreAdapterProperty::HardwareID:                   return "HardwareID";
        case DXCoreAdapterProperty::KmdModelVersion:              return "KmdModelVersion";
        case DXCoreAdapterProperty::ComputePreemptionGranularity: return "ComputePreemptionGranularity";
        case DXCoreAdapterProperty::GraphicsPreemptionGranularity:return "GraphicsPreemptionGranularity";
        case DXCoreAdapterProperty::DedicatedAdapterMemory:       return "DedicatedAdapterMemory";
        case DXCoreAdapterProperty::DedicatedSystemMemory:        return "DedicatedSystemMemory";
        case DXCoreAdapterProperty::SharedSystemMemory:           return "SharedSystemMemory";
        case DXCoreAdapterProperty::AcgCompatible:                return "AcgCompatible";
        case DXCoreAdapterProperty::IsHardware:                   return "IsHardware";
        case DXCoreAdapterProperty::IsIntegrated:                 return "IsIntegrated";
        case DXCoreAdapterProperty::IsDetachable:                 return "IsDetachable";
        case DXCoreAdapterProperty::HardwareIDParts:              return "HardwareIDParts";
      }
      return "<unknown property>";
    }

    static std::string DxcorePropToString(DXCoreAdapterProperty p) {
      return std::string(DxcorePropName(p))
        + " (" + std::to_string(uint32_t(p)) + ")";
    }

    static std::string HexDump16(const void* buffer, size_t bufferSize) {
      static const char hex[] = "0123456789abcdef";
      const uint8_t* b = static_cast<const uint8_t*>(buffer);
      size_t n = bufferSize < 16 ? bufferSize : 16;
      std::string s;
      s.reserve(n * 3);
      for (size_t i = 0; i < n; i++) {
        if (i) s.push_back(' ');
        s.push_back(hex[(b[i] >> 4) & 0xf]);
        s.push_back(hex[b[i] & 0xf]);
      }
      return s;
    }

    static const char* HrName(HRESULT hr) {
      if (hr == S_OK)            return "S_OK";
      if (hr == E_POINTER)       return "E_POINTER";
      if (hr == E_INVALIDARG)    return "E_INVALIDARG";
      if (hr == E_NOINTERFACE)   return "E_NOINTERFACE";
      if (hr == DXGI_ERROR_UNSUPPORTED) return "DXGI_ERROR_UNSUPPORTED";
      return "HRESULT";
    }
#endif

  }


  DxgiCoreAdapter::DxgiCoreAdapter(DxgiAdapter* pAdapter)
  : m_adapter(pAdapter) {

  }


  ULONG STDMETHODCALLTYPE DxgiCoreAdapter::AddRef() {
    return m_adapter->AddRef();
  }


  ULONG STDMETHODCALLTYPE DxgiCoreAdapter::Release() {
    return m_adapter->Release();
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::QueryInterface(
          REFIID                    riid,
          void**                    ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(IDXCoreAdapter)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    // Cross-cast back to IDXGIAdapter and friends so callers can
    // retrieve the original DXGI view from the IDXCoreAdapter.
    return m_adapter->QueryInterface(riid, ppvObject);
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsValid() {
#if DXVK_LOG_DXCORE
    Logger::info("DxgiCoreAdapter::IsValid -> TRUE");
#endif
    return TRUE;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsAttributeSupported(
          REFGUID                   attribute) {
    BOOL result = FALSE;
    if (attribute == DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS
     || attribute == DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE
     || attribute == DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS)
      result = TRUE;

#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::IsAttributeSupported(",
      DxcoreAttrToString(attribute),
      ") -> ", result ? "TRUE" : "FALSE"));
#endif
    return result;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsPropertySupported(
          DXCoreAdapterProperty     property) {
    BOOL result = FALSE;
    switch (property) {
      case DXCoreAdapterProperty::InstanceLuid:
      case DXCoreAdapterProperty::DriverVersion:
      case DXCoreAdapterProperty::DriverDescription:
      case DXCoreAdapterProperty::HardwareID:
      case DXCoreAdapterProperty::KmdModelVersion:
      case DXCoreAdapterProperty::ComputePreemptionGranularity:
      case DXCoreAdapterProperty::GraphicsPreemptionGranularity:
      case DXCoreAdapterProperty::DedicatedAdapterMemory:
      case DXCoreAdapterProperty::DedicatedSystemMemory:
      case DXCoreAdapterProperty::SharedSystemMemory:
      case DXCoreAdapterProperty::AcgCompatible:
      case DXCoreAdapterProperty::IsHardware:
      case DXCoreAdapterProperty::IsIntegrated:
      case DXCoreAdapterProperty::IsDetachable:
      case DXCoreAdapterProperty::HardwareIDParts:
        result = TRUE;
        break;
    }

#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::IsPropertySupported(",
      DxcorePropToString(property),
      ") -> ", result ? "TRUE" : "FALSE"));
#endif
    return result;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::GetPropertySize(
          DXCoreAdapterProperty     property,
          size_t*                   bufferSize) {
    if (bufferSize == nullptr) {
#if DXVK_LOG_DXCORE
      Logger::info(str::format(
        "DxgiCoreAdapter::GetPropertySize(",
        DxcorePropToString(property),
        ") -> E_POINTER"));
#endif
      return E_POINTER;
    }

    HRESULT hr = S_OK;
    switch (property) {
      case DXCoreAdapterProperty::InstanceLuid:
        *bufferSize = sizeof(LUID);
        break;

      case DXCoreAdapterProperty::DriverVersion:
        *bufferSize = sizeof(uint64_t);
        break;

      case DXCoreAdapterProperty::DriverDescription: {
        DXGI_ADAPTER_DESC1 desc;
        hr = m_adapter->GetDesc1(&desc);
        if (FAILED(hr)) {
#if DXVK_LOG_DXCORE
          Logger::info(str::format(
            "DxgiCoreAdapter::GetPropertySize(",
            DxcorePropToString(property),
            ") -> hr=0x", std::hex, uint32_t(hr)));
#endif
          return hr;
        }

        size_t length = 0;
        while (length < std::size(desc.Description) && desc.Description[length] != L'\0')
          length++;

        *bufferSize = length + 1;
        break;
      }

      case DXCoreAdapterProperty::HardwareID:
        *bufferSize = sizeof(DXCoreHardwareID);
        break;

      case DXCoreAdapterProperty::KmdModelVersion:
        *bufferSize = sizeof(uint64_t);
        break;

      case DXCoreAdapterProperty::ComputePreemptionGranularity:
      case DXCoreAdapterProperty::GraphicsPreemptionGranularity:
        *bufferSize = sizeof(uint32_t);
        break;

      case DXCoreAdapterProperty::DedicatedAdapterMemory:
      case DXCoreAdapterProperty::DedicatedSystemMemory:
      case DXCoreAdapterProperty::SharedSystemMemory:
        *bufferSize = sizeof(uint64_t);
        break;

      case DXCoreAdapterProperty::AcgCompatible:
      case DXCoreAdapterProperty::IsHardware:
      case DXCoreAdapterProperty::IsIntegrated:
      case DXCoreAdapterProperty::IsDetachable:
        *bufferSize = sizeof(BOOL);
        break;

      case DXCoreAdapterProperty::HardwareIDParts:
        *bufferSize = sizeof(DXCoreHardwareIDParts);
        break;

      default:
#if DXVK_LOG_DXCORE
        Logger::info(str::format(
          "DxgiCoreAdapter::GetPropertySize(",
          DxcorePropToString(property),
          ") -> E_INVALIDARG"));
#endif
        return E_INVALIDARG;
    }

#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::GetPropertySize(",
      DxcorePropToString(property),
      ") -> S_OK, size=", *bufferSize));
#endif
    return S_OK;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::GetProperty(
          DXCoreAdapterProperty     property,
          size_t                    bufferSize,
          void*                     buffer) {
    if (buffer == nullptr) {
#if DXVK_LOG_DXCORE
      Logger::info(str::format(
        "DxgiCoreAdapter::GetProperty(",
        DxcorePropToString(property),
        ", size=", bufferSize, ") -> E_POINTER"));
#endif
      return E_POINTER;
    }

    DXGI_ADAPTER_DESC1 desc;
    HRESULT hr = m_adapter->GetDesc1(&desc);
    if (FAILED(hr)) {
#if DXVK_LOG_DXCORE
      Logger::info(str::format(
        "DxgiCoreAdapter::GetProperty(",
        DxcorePropToString(property),
        ", size=", bufferSize, ") -> GetDesc1 failed hr=0x",
        std::hex, uint32_t(hr)));
#endif
      return hr;
    }

    auto adapter = m_adapter->GetDXVKAdapter();
    const auto& deviceProp = adapter->deviceProperties();
    const bool isIntegrated = deviceProp.core.properties.deviceType
      == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    HRESULT result = E_INVALIDARG;

    switch (property) {
      case DXCoreAdapterProperty::InstanceLuid: {
        if (bufferSize < sizeof(LUID)) { result = E_INVALIDARG; break; }
        std::memcpy(buffer, &desc.AdapterLuid, sizeof(LUID));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::DriverVersion: {
        if (bufferSize < sizeof(uint64_t)) { result = E_INVALIDARG; break; }
        // Mirror the high "32.0.x.y" style version DXVK already uses
        // in CheckInterfaceSupport (HighPart=0x00200000, LowPart=~0u).
        uint64_t version = (uint64_t(0x00200000u) << 32) | uint64_t(0xffffffffu);
        std::memcpy(buffer, &version, sizeof(version));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::DriverDescription: {
        size_t length = 0;
        while (length < std::size(desc.Description) && desc.Description[length] != L'\0')
          length++;

        if (bufferSize < length + 1) { result = E_INVALIDARG; break; }

        char* out = static_cast<char*>(buffer);
        for (size_t i = 0; i < length; i++) {
          wchar_t wc = desc.Description[i];
          out[i] = (wc < 0x80) ? char(wc) : '?';
        }
        out[length] = '\0';
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::HardwareID: {
        if (bufferSize < sizeof(DXCoreHardwareID)) { result = E_INVALIDARG; break; }
        DXCoreHardwareID id = { };
        id.vendorID = desc.VendorId;
        id.deviceID = desc.DeviceId;
        id.subSysID = desc.SubSysId;
        id.revision = desc.Revision;
        std::memcpy(buffer, &id, sizeof(id));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::KmdModelVersion: {
        if (bufferSize < sizeof(uint64_t)) { result = E_INVALIDARG; break; }
        // WDDM 3.0 (high=30, low=0)
        uint64_t kmd = (uint64_t(30u) << 32) | uint64_t(0u);
        std::memcpy(buffer, &kmd, sizeof(kmd));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::ComputePreemptionGranularity: {
        if (bufferSize < sizeof(uint32_t)) { result = E_INVALIDARG; break; }
        uint32_t value = uint32_t(
          DXCoreComputePreemptionGranularity::DXCoreComputePreemptionGranularity_DispatchBoundary);
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::GraphicsPreemptionGranularity: {
        if (bufferSize < sizeof(uint32_t)) { result = E_INVALIDARG; break; }
        uint32_t value = uint32_t(
          DXCoreGraphicsPreemptionGranularity::DXCoreGraphicsPreemptionGranularity_PrimitiveBoundary);
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::DedicatedAdapterMemory: {
        if (bufferSize < sizeof(uint64_t)) { result = E_INVALIDARG; break; }
        uint64_t value = desc.DedicatedVideoMemory;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::DedicatedSystemMemory: {
        if (bufferSize < sizeof(uint64_t)) { result = E_INVALIDARG; break; }
        uint64_t value = desc.DedicatedSystemMemory;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::SharedSystemMemory: {
        if (bufferSize < sizeof(uint64_t)) { result = E_INVALIDARG; break; }
        uint64_t value = desc.SharedSystemMemory;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::AcgCompatible: {
        if (bufferSize < sizeof(BOOL)) { result = E_INVALIDARG; break; }
        BOOL value = TRUE;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::IsHardware: {
        if (bufferSize < sizeof(BOOL)) { result = E_INVALIDARG; break; }
        BOOL value = TRUE;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::IsIntegrated: {
        if (bufferSize < sizeof(BOOL)) { result = E_INVALIDARG; break; }
        BOOL value = isIntegrated ? TRUE : FALSE;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::IsDetachable: {
        if (bufferSize < sizeof(BOOL)) { result = E_INVALIDARG; break; }
        BOOL value = FALSE;
        std::memcpy(buffer, &value, sizeof(value));
        result = S_OK;
        break;
      }

      case DXCoreAdapterProperty::HardwareIDParts: {
        if (bufferSize < sizeof(DXCoreHardwareIDParts)) { result = E_INVALIDARG; break; }
        DXCoreHardwareIDParts parts = { };
        parts.vendorID    = desc.VendorId;
        parts.deviceID    = desc.DeviceId;
        parts.subSystemID = desc.SubSysId;
        parts.subVendorID = desc.SubSysId >> 16;
        parts.revisionID  = desc.Revision;
        std::memcpy(buffer, &parts, sizeof(parts));
        result = S_OK;
        break;
      }

      default:
        result = E_INVALIDARG;
        break;
    }

#if DXVK_LOG_DXCORE
    if (result == S_OK && bufferSize <= 32) {
      Logger::info(str::format(
        "DxgiCoreAdapter::GetProperty(",
        DxcorePropToString(property),
        ", size=", bufferSize, ") -> ", HrName(result),
        " bytes=[", HexDump16(buffer, bufferSize), "]"));
    } else {
      Logger::info(str::format(
        "DxgiCoreAdapter::GetProperty(",
        DxcorePropToString(property),
        ", size=", bufferSize, ") -> ", HrName(result)));
    }
#endif
    return result;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsQueryStateSupported(
          DXCoreAdapterState        property) {
#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::IsQueryStateSupported(state=",
      uint32_t(property), ") -> FALSE"));
#endif
    return FALSE;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::QueryState(
          DXCoreAdapterState        state,
          size_t                    stateDetailsSize,
    const void*                     stateDetails,
          size_t                    bufferSize,
          void*                     buffer) {
#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::QueryState(state=", uint32_t(state),
      ", detailsSize=", stateDetailsSize,
      ", bufferSize=", bufferSize, ") -> DXGI_ERROR_UNSUPPORTED"));
#endif
    return DXGI_ERROR_UNSUPPORTED;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsSetStateSupported(
          DXCoreAdapterState        property) {
#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::IsSetStateSupported(state=",
      uint32_t(property), ") -> FALSE"));
#endif
    return FALSE;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::SetState(
          DXCoreAdapterState        state,
          size_t                    stateDetailsSize,
    const void*                     stateDetails,
          size_t                    bufferSize,
    const void*                     buffer) {
#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::SetState(state=", uint32_t(state),
      ", detailsSize=", stateDetailsSize,
      ", bufferSize=", bufferSize, ") -> E_INVALIDARG"));
#endif
    return E_INVALIDARG;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::GetFactory(
          REFIID                    riid,
          void**                    ppv) {
    if (ppv != nullptr)
      *ppv = nullptr;

    // We do not provide an IDXCoreAdapterFactory; capability checks
    // we have observed (Witcher 3 next-gen, etc.) do not need one.
#if DXVK_LOG_DXCORE
    Logger::info(str::format(
      "DxgiCoreAdapter::GetFactory(", str::format(riid),
      ") -> E_NOINTERFACE"));
#else
    Logger::warn("DxgiCoreAdapter::GetFactory: not implemented");
#endif
    return E_NOINTERFACE;
  }

}
