/** @file
  RISC-V IO Mapping Table (RIMT) ACPI table definition
  from the RISC-V(R) IO Mapping Table (RIMT) Specification.

  Copyright (c) 2025, 9elements GmbH. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Revision Reference:
    - RISC-V IO Mapping Table (RIMT) Specification v1.0, Dated March 2025.
      https://docs.riscv.org/reference/platform-software/acpi-rimt/_attachments/rimt-spec.pdf
**/

#ifndef _RISCV_IO_MAPPING_TABLE_H_
#define _RISCV_IO_MAPPING_TABLE_H_

#include <IndustryStandard/Acpi.h>

#pragma pack(1)

///
/// RISC-V IO Mapping Structure definitions from chapter 2.
///@{
#define RIMT_REVISION  0x01
///@}

//
// Chapter 2, RISC-V IO Mapping Structures
//
enum {
  RISCV_IOMMU_NODE_TYPE,
  PCIE_ROOT_COMPLEX_NODE_TYPE,
  PLATFORM_DEVICE_NODE_TYPE,
  RESERVED,
};

typedef struct {
  UINT8   Type;
  UINT8   Revision;
  UINT16  Length;
  UINT16  Reserved;

  /**
    Unique ID of this node in the RIMT that can be used
    to locate it in the RIMT node array. It can be simply
    the array index in the RIMT node array.
  **/
  UINT16  Id;
} RIMT_NODE_HEADER;

//
// Section 2.1.1, IOMMU Node.
//
#define IOMMU_NODE_FLAG_PCIE_DEVICE             BIT0
#define IOMMU_NODE_FLAG_PROXIMITY_DOMAIN_VALID  BIT1

/**
  Interrupt Wire Structure
**/
#define IOMMU_NODE_INTERRUPT_WIRE_FLAG_LEVEL_TRIGGERED  BIT0
#define IOMMU_NODE_INTERRUPT_WIRE_FLAG_ACTIVE_HIGH      BIT1

typedef struct {
  /**
    Interrupt number. This should be a Global System
    Interrupt (GSI) number. These are wired interrupts
    with GSI numbers mapping to a particular PLIC or
    APLIC. The OSPM determines the mapping of the
    Global System Interrupts by determining how many
    interrupt inputs each PLIC or APLIC supports and by
    determining the global system interrupt base for
    each PLIC / APLIC.
  **/
  UINT32  InterruptNumber;

  UINT32  Flags;
} RIMT_IOMMU_NODE_INTERRUPT_WIRE;

/**
  The IOMMU node reports the configuration and capabilities
  of each IOMMU in the system.
**/
typedef struct {
  RIMT_NODE_HEADER  Header;

  /**
    ACPI ID of the IOMMU when it is a platform device
    or PCIe ID (Vendor ID + Device ID) for the PCIe
    IOMMU device. This field adheres to the _HID
    format described by the ACPI specification.
  **/
  UINT64  HardwareId;

  /**
    Base address of the IOMMU registers. This field is
    valid for only an IOMMU that is a platform device. If
    IOMMU is a PCIe device, the base address of the
    IOMMU registers may be discovered from or
    programmed into the PCIe BAR of the IOMMU.
  **/
  UINT64  BaseAddress;

  UINT32  Flags;

  /**
    The Proximity Domain to which this IOMMU belongs.
    This is valid only when the "Proximity Domain Valid"
    flag is set. For optimal IOMMU performance, the in-
    memory data structures used by the IOMMU may be
    located in memory from this proximity domain.
  **/
  UINT32  ProximityDomain;

  /**
    If the IOMMU is implemented as a PCIe device (Bit
    0 of Flags is 1), then this field holds the PCIe
    segment where this IOMMU is located.
  **/
  UINT16  PcieSegment;

  /**
    If the IOMMU is implemented as a PCIe device (Bit
    0 of Flags is 1), then this field provides the
    Bus/Device/Function of the IOMMU.
  **/
  UINT16  PcieBdf;

  /**
    An IOMMU may signal IOMMU initiated interrupts by
    using wires or as message signaled interrupts (MSI).
    When the IOMMU supports signaling interrupts by
    using wires, this field provides the number of
    interrupt wires. This field must be 0 if the IOMMU
    does not support wire-based interrupt generation.
  **/
  UINT16  NumberOfInterruptWires;

  /**
    The offset from the start of this node entry to the
    first entry of the Interrupt Wire Array. This field is
    valid only if "Number of interrupt wires" is not 0.
  **/
  UINT16  InterruptWireArrayOffset;
} RIMT_IOMMU_NODE;

//
// Section 2.1.2, PCIe Root Complex Node.
//
#define PCIE_NODE_FLAG_ATS_REQUIRED  BIT0
#define PCIE_NODE_FLAG_PRI_REQUIRED  BIT1

/**
  ID Mapping Structure
**/
typedef struct {
  /**
    The base of a range of source IDs mapped by this
    entry to a range of device IDs that will be used at
    input to the IOMMU.
  **/
  UINT32  SourceIdBase;

  /**
    Number of IDs in the range. The range must include
    the IDs of devices that may be enumerated later
    during OS boot (For example, SR-IOV Virtual
    Functions).
  **/
  UINT32  NumberOfIds;

  /**
    The base of the destination ID range as mapped by
    this entry. This is the device_id as defined by the
    RISC-V IOMMU specification.
  **/
  UINT32  DestinationDeviceIdBase;

  /**
    The destination IOMMU that is associated with
    these IDs. This field is the offset of the RISC-V
    IOMMU node from the start of the RIMT table.
  **/
  UINT32  DestinationIoMmuOffset;

  UINT32 Flags;
} RIMT_PCIE_NODE_ID_MAPPING;

#define PCIE_NODE_FLAG_ATS_SUPPORT  BIT0
#define PCIE_NODE_FLAG_PRI_SUPPORT  BIT1

/**
  The PCIe root complex node is a logical PCIe root complex.
  It can be used to represent an entire physical root complex,
  an RCiEP/set of RCiEPs, a standalone PCIe device,
  or the hierarchy following a PCIe host bridge.
**/
typedef struct {
  RIMT_NODE_HEADER  Header;

  UINT32  Flags;

  UINT16  Reserved;

  /**
    The PCIe segment number, as in MCFG and as
    returned by _SEG method in the ACPI namespace.
  **/
  UINT16  PcieSegment;

  /**
    The offset from the start of this node to the start of
    the ID mapping array.
  **/
  UINT16  IdMappingArrayOffset;

  /**
    Number of elements in the ID mapping array.
  **/
  UINT16  NumberOfIdMappings;
} RIMT_PCIE_NODE;

//
// Section 2.1.3, Platform Device Node
//

/**
  The platform device node describes non-PCIe platform devices
  that should be discovered in the DSDT. They can have one or more source IDs
  in the mapping table, but have their own scheme to define the source IDs.
**/
typedef struct {
  RIMT_NODE_HEADER  Header;

  /**
    The offset from the start of this node to the start of
    the ID mapping array.
  **/
  UINT16  IdMappingArrayOffset;

  /**
    Number of elements in the ID mapping array.
  **/
  UINT16  NumberOfIdMappings;

  /**
    Null terminated ASCII string. Full path to the device
    object in the ACPI namespace.
  **/
  CHAR8   DeviceObjectName[0];

  /**
    Pad with zeros to align the ID mapping array at 4-
    byte offset.
  **/
  //UINT8  Padding[0];
} RIMT_PLATFORM_DEVICE_NODE;

//
// RISC-V IO Mapping Table header, as defined in chapter 2.
// This header will be followed by list of RIMT nodes.
//
typedef struct {
  EFI_ACPI_DESCRIPTION_HEADER  Header;

  /**
    Number of nodes in the RIMT node array.
  **/
  UINT32  NumberOfNodes;

  /**
    The offset from the start of this table to the first
    node in RIMT node array.
  **/
  UINT32  OffsetToNodeArray;

  UINT32  Reserved;
} EFI_ACPI_RIMT_HEADER;

#pragma pack()

#endif
