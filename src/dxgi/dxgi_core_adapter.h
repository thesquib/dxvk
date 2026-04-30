#pragma once

#include "dxcore_interface.h"
#include "dxgi_interfaces.h"

#ifndef _MSC_VER
__CRT_UUID_DECL(IDXCoreAdapter, 0xf0db4c7f, 0xfe5a, 0x42a2, 0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e);
#endif

namespace dxvk {

  class DxgiAdapter;


  /**
   * \brief DXCore adapter stub
   *
   * Provides a minimal IDXCoreAdapter implementation backed by an
   * existing DxgiAdapter, so that QueryInterface(IID_IDXCoreAdapter)
   * on an IDXGIAdapter returns a usable object. Capability checks in
   * games such as Witcher 3 next-gen DX12 use this to confirm that
   * the adapter is hardware-class and DX12 capable.
   *
   * Lifetime is tied to the parent DxgiAdapter; AddRef/Release are
   * forwarded so the parent stays alive while the cross-cast pointer
   * is held.
   */
  class DxgiCoreAdapter : public IDXCoreAdapter {

  public:

    DxgiCoreAdapter(DxgiAdapter* pAdapter);

    ULONG STDMETHODCALLTYPE AddRef();

    ULONG STDMETHODCALLTYPE Release();

    HRESULT STDMETHODCALLTYPE QueryInterface(
            REFIID                    riid,
            void**                    ppvObject);

    BOOL STDMETHODCALLTYPE IsValid();

    BOOL STDMETHODCALLTYPE IsAttributeSupported(
            REFGUID                   attribute);

    BOOL STDMETHODCALLTYPE IsPropertySupported(
            DXCoreAdapterProperty     property);

    HRESULT STDMETHODCALLTYPE GetProperty(
            DXCoreAdapterProperty     property,
            size_t                    bufferSize,
            void*                     buffer);

    HRESULT STDMETHODCALLTYPE GetPropertySize(
            DXCoreAdapterProperty     property,
            size_t*                   bufferSize);

    BOOL STDMETHODCALLTYPE IsQueryStateSupported(
            DXCoreAdapterState        property);

    HRESULT STDMETHODCALLTYPE QueryState(
            DXCoreAdapterState        state,
            size_t                    stateDetailsSize,
      const void*                     stateDetails,
            size_t                    bufferSize,
            void*                     buffer);

    BOOL STDMETHODCALLTYPE IsSetStateSupported(
            DXCoreAdapterState        property);

    HRESULT STDMETHODCALLTYPE SetState(
            DXCoreAdapterState        state,
            size_t                    stateDetailsSize,
      const void*                     stateDetails,
            size_t                    bufferSize,
      const void*                     buffer);

    HRESULT STDMETHODCALLTYPE GetFactory(
            REFIID                    riid,
            void**                    ppv);

  private:

    DxgiAdapter* m_adapter;

  };

}
