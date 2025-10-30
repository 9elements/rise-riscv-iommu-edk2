/** @file
  RISC-V IOMMU driver.

  Copyright (c) 2025, 9elements GmbH. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/PciEnumerationComplete.h>
#include <Protocol/PciIo.h>
#include <Guid/Fdt.h>
#include <Guid/PlatformHasAcpi.h>
#include <Guid/PlatformHasDeviceTree.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/RiscVIoMappingTable.h>
#include "RiscVIoMmu.h"

#define EFI_ACPI_RISCV_IO_MAPPING_TABLE_SIGNATURE  SIGNATURE_32('R', 'I', 'M', 'T')

/**
  PciEnumerationComplete Protocol notification event handler.

  @param[in] Event    Event whose notification function is being invoked.
  @param[in] Context  Pointer to the notification function's context.
**/
STATIC
VOID
EFIAPI
OnPciEnumerationComplete (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  VOID                               *Interface;
  EFI_STATUS                         Status;
  UINTN                              HandleCount;
  EFI_HANDLE                         *HandleBuffer;
  UINTN                              Index;
  EFI_PCI_IO_PROTOCOL                *PciIo;
  PCI_TYPE00                         Pci;
  UINT16                             Data;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;

  //
  // Try to locate it because gEfiPciEnumerationCompleteProtocolGuid will trigger it once when registration.
  // Just return if it is not found.
  //
  Status = gBS->LocateProtocol (&gEfiPciEnumerationCompleteProtocolGuid, NULL, &Interface);
  if (EFI_ERROR (Status)) {
    return;
  }

  //
  // Iterate all PCI devices, looking for one with base class
  // PCI_CLASS_SYSTEM_PERIPHERAL; sub-class 06h; programming interface 00h.
  //
  Status = gBS->LocateHandleBuffer (ByProtocol, &gEfiPciIoProtocolGuid, NULL, &HandleCount, &HandleBuffer);
  ASSERT_EFI_ERROR (Status);

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], &gEfiPciIoProtocolGuid, (VOID **)&PciIo);
    ASSERT_EFI_ERROR (Status);

    //
    // Read the basics of the PCI config space.
    //
    Status = PciIo->Pci.Read (PciIo, EfiPciIoWidthUint32, 0, sizeof (Pci) / sizeof (UINT32), &Pci);
    ASSERT_EFI_ERROR (Status);

    if (IS_CLASS3 (&Pci, PCI_CLASS_SYSTEM_PERIPHERAL, 0x06, 0x00)) {
      mRiscVIoMmuGlobalDriverContext.DriverState  = STATE_AVAILABLE;

      // Also enable DMA to satisfy MSIs, etc.
      Data   = EFI_PCI_COMMAND_BUS_MASTER | EFI_PCI_COMMAND_MEMORY_SPACE;
      Status = PciIo->Pci.Write (PciIo, EfiPciIoWidthUint16, PCI_COMMAND_OFFSET, 1, (VOID *)&Data);
      ASSERT_EFI_ERROR (Status);

      Status = PciIo->GetBarAttributes (PciIo, 0, NULL, (VOID **)&Descriptor);
      ASSERT ((Status == EFI_SUCCESS) && (Descriptor->ResType == ACPI_ADDRESS_SPACE_TYPE_MEM));

      mRiscVIoMmuGlobalDriverContext.Address = Descriptor->AddrRangeMin;
      ASSERT (Descriptor->AddrLen == SIZE_4KB);
      FreePool (Descriptor);

      IoMmuCommonInitialise ();
      break;
    }
  }

  FreePool (HandleBuffer);

  gBS->CloseEvent (Event);
}

/**
  Search the devicetree for an IOMMU.

  @retval  EFI_SUCCESS    A system IOMMU was detected.
  @retval  EFI_NOT_FOUND  No system IOMMUs were detected.

**/
STATIC
EFI_STATUS
IoMmuDeviceTreeDiscovery (
  VOID
  )
{
  VOID        *Fdt;
  EFI_STATUS  Status;
  INT32       IoMmuNode;
  INT32       TempLen;
  UINT64      *Data64;
  UINT64      StartAddress;
  UINT64      NumberOfBytes;
  VOID        *Registration;
  EFI_EVENT   ProtocolNotifyEvent;

  Status = EfiGetSystemConfigurationTable (&gFdtTableGuid, &Fdt);
  ASSERT_EFI_ERROR (Status);

  //
  // Search for a system IOMMU-compatible node and get its address.
  //
  IoMmuNode = FdtNodeOffsetByCompatible (Fdt, -1, "riscv,iommu");
  if (IoMmuNode != -FDT_ERR_NOTFOUND) {
    // TODO: `TempLen` is great enough. Effectively, an #address-cells and #size-cells check.
    Data64 = (UINT64 *)FdtGetProp (Fdt, IoMmuNode, "reg", &TempLen);
    ASSERT (Data64 != NULL);

    StartAddress  = Fdt64ToCpu (ReadUnaligned64 (Data64));
    NumberOfBytes = Fdt64ToCpu (ReadUnaligned64 (Data64 + 1));

    mRiscVIoMmuGlobalDriverContext.DriverState      = STATE_AVAILABLE;
    mRiscVIoMmuGlobalDriverContext.IoMmuIsPciDevice = FALSE;
    mRiscVIoMmuGlobalDriverContext.Address          = StartAddress;
    ASSERT (NumberOfBytes == SIZE_4KB);

    return EFI_SUCCESS;
  }

  //
  // Search for a PCI IOMMU-compatible node and get its BDF, then register a callback for enumeration.
  //
  IoMmuNode = FdtNodeOffsetByCompatible (Fdt, -1, "riscv,pci-iommu");
  if (IoMmuNode != -FDT_ERR_NOTFOUND) {
    // TODO: `TempLen` is great enough. Effectively, an #address-cells and #size-cells check.
    Data64 = (UINT64 *)FdtGetProp (Fdt, IoMmuNode, "reg", &TempLen);
    ASSERT (Data64 != NULL);

    mRiscVIoMmuGlobalDriverContext.DriverState      = STATE_DETECTED;
    mRiscVIoMmuGlobalDriverContext.IoMmuIsPciDevice = TRUE;
    mRiscVIoMmuGlobalDriverContext.Address          = Fdt32ToCpu (ReadUnaligned32 ((UINT32 *)Data64));

    //
    // The FDT merely provides the BDF (and device references to the IOMMU), now scan for the device.
    //
    ProtocolNotifyEvent = EfiCreateProtocolNotifyEvent (
                            &gEfiPciEnumerationCompleteProtocolGuid,
                            TPL_CALLBACK,
                            OnPciEnumerationComplete,
                            NULL,
                            &Registration
                            );
    ASSERT (ProtocolNotifyEvent != NULL);

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

/**
  Search ACPI's RIMT for an IOMMU.

  @retval  EFI_SUCCESS    A system IOMMU was detected.
  @retval  EFI_NOT_FOUND  No system IOMMUs were detected.

**/
STATIC
EFI_STATUS
IoMmuAcpiRimtDiscovery (
  VOID
  )
{
  EFI_ACPI_RIMT_HEADER  *AcpiRimtTable;
  RIMT_NODE_HEADER      *RimtNodeHeader;
  UINTN                 Index;
  RIMT_IOMMU_NODE       *RimtIoMmuNode;

  AcpiRimtTable = (VOID *)EfiLocateFirstAcpiTable (EFI_ACPI_RISCV_IO_MAPPING_TABLE_SIGNATURE);
  if (AcpiRimtTable == NULL) {
    return EFI_NOT_FOUND;
  }

  RimtNodeHeader = (VOID *)((UINT8 *)AcpiRimtTable + AcpiRimtTable->OffsetToNodeArray);
  for (Index = 0; Index < AcpiRimtTable->NumberOfNodes; Index++) {
    if (RimtNodeHeader->Type == RISCV_IOMMU_NODE_TYPE) {
      RimtIoMmuNode = (VOID *)RimtNodeHeader;
      if (RimtIoMmuNode->Flags & IOMMU_NODE_FLAG_PCIE_DEVICE) {
        mRiscVIoMmuGlobalDriverContext.DriverState      = STATE_DETECTED;
        mRiscVIoMmuGlobalDriverContext.IoMmuIsPciDevice = TRUE;
        mRiscVIoMmuGlobalDriverContext.Address          = RimtIoMmuNode->PcieBdf;
      } else {
        mRiscVIoMmuGlobalDriverContext.DriverState      = STATE_AVAILABLE;
        mRiscVIoMmuGlobalDriverContext.IoMmuIsPciDevice = FALSE;
        mRiscVIoMmuGlobalDriverContext.Address          = RimtIoMmuNode->BaseAddress;
      }
    }

    RimtNodeHeader = (VOID *)((UINT8 *)RimtNodeHeader + RimtNodeHeader->Length);
  }

  return EFI_NOT_FOUND;
}

/**
  Detect a RISC-V IOMMU device.

  @retval  EFI_SUCCESS    An IOMMU was detected.
  @retval  EFI_NOT_FOUND  No IOMMU could be detected.

**/
VOID
DetectRiscVIoMmus (
  VOID
  )
{
  EFI_STATUS  Status;
  VOID        *Registration;

  //
  // Search the devicetree for an IOMMU.
  //
  Status = gBS->LocateProtocol (&gEdkiiPlatformHasDeviceTreeGuid, NULL, (VOID **)&Registration);
  if (!EFI_ERROR (Status)) {
    Status = IoMmuDeviceTreeDiscovery ();
    if (!EFI_ERROR (Status)) {
      return;
    }
  }

  //
  // Search ACPI's RIMT for an IOMMU.
  //
  Status = gBS->LocateProtocol (&gEdkiiPlatformHasAcpiGuid, NULL, (VOID **)&Registration);
  if (!EFI_ERROR (Status)) {
    Status = IoMmuAcpiRimtDiscovery ();
    if (!EFI_ERROR (Status)) {
      return;
    }
  }
}
