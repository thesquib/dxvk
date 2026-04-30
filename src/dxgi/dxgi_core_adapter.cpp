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

namespace dxvk {

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
    return TRUE;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsAttributeSupported(
          REFGUID                   attribute) {
    if (attribute == DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS
     || attribute == DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE
     || attribute == DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS)
      return TRUE;

    return FALSE;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsPropertySupported(
          DXCoreAdapterProperty     property) {
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
        return TRUE;
    }

    return FALSE;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::GetPropertySize(
          DXCoreAdapterProperty     property,
          size_t*                   bufferSize) {
    if (bufferSize == nullptr)
      return E_POINTER;

    switch (property) {
      case DXCoreAdapterProperty::InstanceLuid:
        *bufferSize = sizeof(LUID);
        return S_OK;

      case DXCoreAdapterProperty::DriverVersion:
        *bufferSize = sizeof(uint64_t);
        return S_OK;

      case DXCoreAdapterProperty::DriverDescription: {
        DXGI_ADAPTER_DESC1 desc;
        HRESULT hr = m_adapter->GetDesc1(&desc);
        if (FAILED(hr))
          return hr;

        // UTF-8-ish narrow conversion of the WIDE Description.
        // ASCII is the common case; high bytes get replaced with '?'.
        size_t length = 0;
        while (length < std::size(desc.Description) && desc.Description[length] != L'\0')
          length++;

        *bufferSize = length + 1;
        return S_OK;
      }

      case DXCoreAdapterProperty::HardwareID:
        *bufferSize = sizeof(DXCoreHardwareID);
        return S_OK;

      case DXCoreAdapterProperty::KmdModelVersion:
        *bufferSize = sizeof(uint64_t);
        return S_OK;

      case DXCoreAdapterProperty::ComputePreemptionGranularity:
      case DXCoreAdapterProperty::GraphicsPreemptionGranularity:
        *bufferSize = sizeof(uint32_t);
        return S_OK;

      case DXCoreAdapterProperty::DedicatedAdapterMemory:
      case DXCoreAdapterProperty::DedicatedSystemMemory:
      case DXCoreAdapterProperty::SharedSystemMemory:
        *bufferSize = sizeof(uint64_t);
        return S_OK;

      case DXCoreAdapterProperty::AcgCompatible:
      case DXCoreAdapterProperty::IsHardware:
      case DXCoreAdapterProperty::IsIntegrated:
      case DXCoreAdapterProperty::IsDetachable:
        *bufferSize = sizeof(BOOL);
        return S_OK;

      case DXCoreAdapterProperty::HardwareIDParts:
        *bufferSize = sizeof(DXCoreHardwareIDParts);
        return S_OK;
    }

    return E_INVALIDARG;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::GetProperty(
          DXCoreAdapterProperty     property,
          size_t                    bufferSize,
          void*                     buffer) {
    if (buffer == nullptr)
      return E_POINTER;

    DXGI_ADAPTER_DESC1 desc;
    HRESULT hr = m_adapter->GetDesc1(&desc);
    if (FAILED(hr))
      return hr;

    auto adapter = m_adapter->GetDXVKAdapter();
    const auto& deviceProp = adapter->deviceProperties();
    const bool isIntegrated = deviceProp.core.properties.deviceType
      == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;

    switch (property) {
      case DXCoreAdapterProperty::InstanceLuid: {
        if (bufferSize < sizeof(LUID))
          return E_INVALIDARG;
        std::memcpy(buffer, &desc.AdapterLuid, sizeof(LUID));
        return S_OK;
      }

      case DXCoreAdapterProperty::DriverVersion: {
        if (bufferSize < sizeof(uint64_t))
          return E_INVALIDARG;
        // Mirror the high "32.0.x.y" style version DXVK already uses
        // in CheckInterfaceSupport (HighPart=0x00200000, LowPart=~0u).
        uint64_t version = (uint64_t(0x00200000u) << 32) | uint64_t(0xffffffffu);
        std::memcpy(buffer, &version, sizeof(version));
        return S_OK;
      }

      case DXCoreAdapterProperty::DriverDescription: {
        size_t length = 0;
        while (length < std::size(desc.Description) && desc.Description[length] != L'\0')
          length++;

        if (bufferSize < length + 1)
          return E_INVALIDARG;

        char* out = static_cast<char*>(buffer);
        for (size_t i = 0; i < length; i++) {
          wchar_t wc = desc.Description[i];
          out[i] = (wc < 0x80) ? char(wc) : '?';
        }
        out[length] = '\0';
        return S_OK;
      }

      case DXCoreAdapterProperty::HardwareID: {
        if (bufferSize < sizeof(DXCoreHardwareID))
          return E_INVALIDARG;
        DXCoreHardwareID id = { };
        id.vendorID = desc.VendorId;
        id.deviceID = desc.DeviceId;
        id.subSysID = desc.SubSysId;
        id.revision = desc.Revision;
        std::memcpy(buffer, &id, sizeof(id));
        return S_OK;
      }

      case DXCoreAdapterProperty::KmdModelVersion: {
        if (bufferSize < sizeof(uint64_t))
          return E_INVALIDARG;
        // WDDM 3.0 (high=30, low=0)
        uint64_t kmd = (uint64_t(30u) << 32) | uint64_t(0u);
        std::memcpy(buffer, &kmd, sizeof(kmd));
        return S_OK;
      }

      case DXCoreAdapterProperty::ComputePreemptionGranularity: {
        if (bufferSize < sizeof(uint32_t))
          return E_INVALIDARG;
        uint32_t value = uint32_t(
          DXCoreComputePreemptionGranularity::DXCoreComputePreemptionGranularity_DispatchBoundary);
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::GraphicsPreemptionGranularity: {
        if (bufferSize < sizeof(uint32_t))
          return E_INVALIDARG;
        uint32_t value = uint32_t(
          DXCoreGraphicsPreemptionGranularity::DXCoreGraphicsPreemptionGranularity_PrimitiveBoundary);
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::DedicatedAdapterMemory: {
        if (bufferSize < sizeof(uint64_t))
          return E_INVALIDARG;
        uint64_t value = desc.DedicatedVideoMemory;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::DedicatedSystemMemory: {
        if (bufferSize < sizeof(uint64_t))
          return E_INVALIDARG;
        uint64_t value = desc.DedicatedSystemMemory;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::SharedSystemMemory: {
        if (bufferSize < sizeof(uint64_t))
          return E_INVALIDARG;
        uint64_t value = desc.SharedSystemMemory;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::AcgCompatible: {
        if (bufferSize < sizeof(BOOL))
          return E_INVALIDARG;
        BOOL value = TRUE;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::IsHardware: {
        if (bufferSize < sizeof(BOOL))
          return E_INVALIDARG;
        BOOL value = TRUE;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::IsIntegrated: {
        if (bufferSize < sizeof(BOOL))
          return E_INVALIDARG;
        BOOL value = isIntegrated ? TRUE : FALSE;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::IsDetachable: {
        if (bufferSize < sizeof(BOOL))
          return E_INVALIDARG;
        BOOL value = FALSE;
        std::memcpy(buffer, &value, sizeof(value));
        return S_OK;
      }

      case DXCoreAdapterProperty::HardwareIDParts: {
        if (bufferSize < sizeof(DXCoreHardwareIDParts))
          return E_INVALIDARG;
        DXCoreHardwareIDParts parts = { };
        parts.vendorID    = desc.VendorId;
        parts.deviceID    = desc.DeviceId;
        parts.subSystemID = desc.SubSysId;
        parts.subVendorID = desc.SubSysId >> 16;
        parts.revisionID  = desc.Revision;
        std::memcpy(buffer, &parts, sizeof(parts));
        return S_OK;
      }
    }

    return E_INVALIDARG;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsQueryStateSupported(
          DXCoreAdapterState        property) {
    return FALSE;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::QueryState(
          DXCoreAdapterState        state,
          size_t                    stateDetailsSize,
    const void*                     stateDetails,
          size_t                    bufferSize,
          void*                     buffer) {
    return DXGI_ERROR_UNSUPPORTED;
  }


  BOOL STDMETHODCALLTYPE DxgiCoreAdapter::IsSetStateSupported(
          DXCoreAdapterState        property) {
    return FALSE;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::SetState(
          DXCoreAdapterState        state,
          size_t                    stateDetailsSize,
    const void*                     stateDetails,
          size_t                    bufferSize,
    const void*                     buffer) {
    return E_INVALIDARG;
  }


  HRESULT STDMETHODCALLTYPE DxgiCoreAdapter::GetFactory(
          REFIID                    riid,
          void**                    ppv) {
    if (ppv != nullptr)
      *ppv = nullptr;

    // We do not provide an IDXCoreAdapterFactory; capability checks
    // we have observed (Witcher 3 next-gen, etc.) do not need one.
    Logger::warn("DxgiCoreAdapter::GetFactory: not implemented");
    return E_NOINTERFACE;
  }

}
