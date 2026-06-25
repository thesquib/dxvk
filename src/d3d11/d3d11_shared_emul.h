#pragma once

#include <mutex>
#include <unordered_map>

// d3d11_texture.h provides D3D11_COMMON_TEXTURE_DESC, Rc<DxvkImage> (via
// dxvk_device.h) and the Win32 HANDLE type (via d3d11_include.h).
#include "d3d11_texture.h"

namespace dxvk {

  /**
   * \brief Intra-process shared-resource emulation
   *
   * Drivers without VK_KHR_external_memory_win32 (e.g. KosmicKrisp on Metal)
   * cannot create real shareable images, so a normal D3D11 GetSharedHandle
   * fails and a title that shares a texture - the common UE5 media / D3D11
   * interop case during level streaming - hangs forever waiting for a handle
   * it never receives.
   *
   * Almost all D3D11 resource sharing in practice happens WITHIN one process:
   * producer and consumer live in the same address space and use the same
   * VkDevice, so they can simply reference the same DxvkImage. No OS handle and
   * no external memory are needed at all. This registry maps a minted
   * process-local handle to the live image plus its texture desc, so the
   * consumer's OpenSharedResource adopts that same image instead of importing
   * memory through an external handle the driver cannot export.
   *
   * Scope: same-process sharing only. Cross-process sharing (a real OS handle
   * handed to another process) still requires true external memory and is not
   * covered here - that would need a Metal IOSurface / MTLSharedEvent path in
   * the driver.
   */
  class D3D11SharedResourceEmulation {

  public:

    struct Entry {
      Rc<DxvkImage>             image;
      D3D11_COMMON_TEXTURE_DESC desc;
    };

    static D3D11SharedResourceEmulation& get() {
      static D3D11SharedResourceEmulation s_instance;
      return s_instance;
    }

    HANDLE registerImage(
      const Rc<DxvkImage>&             image,
      const D3D11_COMMON_TEXTURE_DESC& desc) {
      std::lock_guard<std::mutex> lock(m_mutex);
      HANDLE handle = reinterpret_cast<HANDLE>(m_next);
      m_next += 0x10;
      m_map.insert({ handle, Entry { image, desc } });
      return handle;
    }

    bool lookup(HANDLE handle, Entry& entry) {
      if (!handle)
        return false;
      std::lock_guard<std::mutex> lock(m_mutex);
      auto it = m_map.find(handle);
      if (it == m_map.end())
        return false;
      entry = it->second;
      return true;
    }

    void unregisterHandle(HANDLE handle) {
      if (!handle)
        return;
      std::lock_guard<std::mutex> lock(m_mutex);
      m_map.erase(handle);
    }

  private:

    std::mutex                        m_mutex;
    std::unordered_map<HANDLE, Entry> m_map;
    // Distinctive non-zero base so a synthetic handle is obvious in logs and
    // unlikely to collide with a real handle; registry membership is what
    // actually disambiguates, so the exact value only needs to be unique.
    uintptr_t                         m_next = 0xca5e0000u;

  };

}
