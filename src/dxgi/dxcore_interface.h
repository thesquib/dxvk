/*
 * DXCore adapter interface declarations.
 *
 * Vendored subset adapted from Wine's include/dxcore_interface.h
 * (Copyright (C) 2023 Mohamad Al-Jaf, LGPL 2.1+). Trimmed to the
 * types DXVK needs to expose IDXCoreAdapter via QueryInterface on
 * IDXGIAdapter; we do not implement IDXCoreAdapterFactory or
 * IDXCoreAdapterList here, so their declarations are omitted.
 *
 * Reference: https://learn.microsoft.com/en-us/windows/win32/dxcore/dxcore-overview
 */

#ifndef DXVK_DXCORE_INTERFACE_H
#define DXVK_DXCORE_INTERFACE_H

#include <stdarg.h>
#include <stdint.h>
#include <ole2.h>

#ifdef __cplusplus
#define DXVK_DXCORE_DECLARE_ENUM(x) enum class x : uint32_t
#else
#define DXVK_DXCORE_DECLARE_ENUM(x) typedef enum x x; enum x
#endif

DXVK_DXCORE_DECLARE_ENUM(DXCoreAdapterProperty)
{
    InstanceLuid = 0,
    DriverVersion = 1,
    DriverDescription = 2,
    HardwareID = 3,
    KmdModelVersion = 4,
    ComputePreemptionGranularity = 5,
    GraphicsPreemptionGranularity = 6,
    DedicatedAdapterMemory = 7,
    DedicatedSystemMemory = 8,
    SharedSystemMemory = 9,
    AcgCompatible = 10,
    IsHardware = 11,
    IsIntegrated = 12,
    IsDetachable = 13,
    HardwareIDParts = 14,
};

DXVK_DXCORE_DECLARE_ENUM(DXCoreAdapterState)
{
    IsDriverUpdateInProgress = 0,
    AdapterMemoryBudget = 1,
};

DXVK_DXCORE_DECLARE_ENUM(DXCoreSegmentGroup)
{
    Local = 0,
    NonLocal = 1,
};

DXVK_DXCORE_DECLARE_ENUM(DXCoreComputePreemptionGranularity)
{
    DXCoreComputePreemptionGranularity_DmaBufferBoundary = 0,
    DXCoreComputePreemptionGranularity_DispatchBoundary = 1,
    DXCoreComputePreemptionGranularity_ThreadGroupBoundary = 2,
    DXCoreComputePreemptionGranularity_ThreadBoundary = 3,
    DXCoreComputePreemptionGranularity_InstructionBoundary = 4,
};

DXVK_DXCORE_DECLARE_ENUM(DXCoreGraphicsPreemptionGranularity)
{
    DXCoreGraphicsPreemptionGranularity_DmaBufferBoundary = 0,
    DXCoreGraphicsPreemptionGranularity_PrimitiveBoundary = 1,
    DXCoreGraphicsPreemptionGranularity_TriangleBoundary = 2,
    DXCoreGraphicsPreemptionGranularity_PixelBoundary = 3,
    DXCoreGraphicsPreemptionGranularity_InstructionBoundary = 4,
};

typedef struct DXCoreHardwareID
{
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t subSysID;
    uint32_t revision;
} DXCoreHardwareID;

typedef struct DXCoreHardwareIDParts
{
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t subSystemID;
    uint32_t subVendorID;
    uint32_t revisionID;
} DXCoreHardwareIDParts;

typedef struct DXCoreAdapterMemoryBudgetNodeSegmentGroup
{
    uint32_t nodeIndex;
    DXCoreSegmentGroup segmentGroup;
} DXCoreAdapterMemoryBudgetNodeSegmentGroup;

typedef struct DXCoreAdapterMemoryBudget
{
    uint64_t budget;
    uint64_t currentUsage;
    uint64_t availableForReservation;
    uint64_t currentReservation;
} DXCoreAdapterMemoryBudget;

DEFINE_GUID(IID_IDXCoreAdapter, 0xf0db4c7f, 0xfe5a, 0x42a2, 0xbd, 0x62, 0xf2, 0xa6, 0xcf, 0x6f, 0xc8, 0x3e);
DEFINE_GUID(DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS, 0x8c47866b, 0x7583, 0x450d, 0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b);
DEFINE_GUID(DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS, 0x0c9ece4d, 0x2f6e, 0x4f01, 0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1);
DEFINE_GUID(DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE, 0x248e2800, 0xa793, 0x4724, 0xab, 0xaa, 0x23, 0xa6, 0xde, 0x1b, 0xe0, 0x90);

#undef INTERFACE
#define INTERFACE IDXCoreAdapter
DECLARE_INTERFACE_IID_(IDXCoreAdapter, IUnknown, "f0db4c7f-fe5a-42a2-bd62-f2a6cf6fc83e")
{
    /* IUnknown methods */
    STDMETHOD(QueryInterface) (THIS_ REFIID riid, void **ppv) PURE;
    STDMETHOD_(ULONG, AddRef) (THIS) PURE;
    STDMETHOD_(ULONG, Release) (THIS) PURE;
    /* IDXCoreAdapter methods */
    STDMETHOD_(BOOL, IsValid) (THIS) PURE;
    STDMETHOD_(BOOL, IsAttributeSupported) (THIS_ REFGUID attribute) PURE;
    STDMETHOD_(BOOL, IsPropertySupported) (THIS_ DXCoreAdapterProperty property) PURE;
    STDMETHOD(GetProperty) (THIS_ DXCoreAdapterProperty property, size_t buffer_size, void *buffer) PURE;
    STDMETHOD(GetPropertySize) (THIS_ DXCoreAdapterProperty property, size_t *buffer_size) PURE;
    STDMETHOD_(BOOL, IsQueryStateSupported) (THIS_ DXCoreAdapterState property) PURE;
    STDMETHOD(QueryState) (THIS_ DXCoreAdapterState state, size_t state_details_size, const void *state_details, size_t buffer_size, void *buffer) PURE;
    STDMETHOD_(BOOL, IsSetStateSupported) (THIS_ DXCoreAdapterState property) PURE;
    STDMETHOD(SetState) (THIS_ DXCoreAdapterState state, size_t state_details_size, const void *state_details, size_t buffer_size, const void *buffer) PURE;
    STDMETHOD(GetFactory) (THIS_ REFIID riid, void **ppv) PURE;
};
#undef INTERFACE

#endif /* DXVK_DXCORE_INTERFACE_H */
