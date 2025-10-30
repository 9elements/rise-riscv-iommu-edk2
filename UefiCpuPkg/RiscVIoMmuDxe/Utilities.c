/** @file
  Primitive helper functions for working with the IOMMU.

  Copyright (c) 2025, 9elements GmbH. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include "RiscVIoMmu.h"

/**
  Read a 32-bit IOMMU register.

  @param[in]  Offset  The register offset to read.
  @ret        The value read from the register.

**/
UINT32
IoMmuRead32 (
  IN UINTN  Offset
  )
{
  return MmioRead32 (mRiscVIoMmuGlobalDriverContext.Address + Offset);
}

/**
  Write a 32-bit IOMMU register.

  @param[in]  Offset  The register offset to write.
  @param[in]  Value   The value to write to the register.

**/
VOID
IoMmuWrite32 (
  IN UINTN   Offset,
  IN UINT32  Value
  )
{
  MmioWrite32 (mRiscVIoMmuGlobalDriverContext.Address + Offset, Value);
}

/**
  Write a 32-bit IOMMU register and wait for a mask to be set/unset.

  @param[in]  Offset  The register offset to write.
  @param[in]  Value   The value to write to the register.
  @param[in]  Mask    The bitmask to wait for.
  @param[in]  Set     Whether the mask should be set or unset.

**/
VOID
IoMmuWriteAndWait32 (
  IN UINTN    Offset,
  IN UINT32   Value,
  IN UINT32   Mask,
  IN BOOLEAN  Set
  )
{
  UINT32  RegValue;

  RegValue = Value;

  MmioWrite32 (mRiscVIoMmuGlobalDriverContext.Address + Offset, RegValue);
  while ((Set && !(RegValue & Mask)) || (!Set && (RegValue & Mask))) {
    MicroSecondDelay (5000);
    RegValue = MmioRead32 (mRiscVIoMmuGlobalDriverContext.Address + Offset);
  }
}

/**
  Read a 64-bit IOMMU register.

  @param[in]  Offset  The register offset to read.
  @ret        The value read from the register.

**/
UINT64
IoMmuRead64 (
  IN UINTN  Offset
  )
{
  return MmioRead64 (mRiscVIoMmuGlobalDriverContext.Address + Offset);
}

/**
  Write a 64-bit IOMMU register.

  @param[in]  Offset  The register offset to write.
  @param[in]  Value   The value to write to the register.

**/
VOID
IoMmuWrite64 (
  IN UINTN   Offset,
  IN UINT64  Value
  )
{
  MmioWrite64 (mRiscVIoMmuGlobalDriverContext.Address + Offset, Value);
}

/**
  Write a 64-bit IOMMU register and wait for a mask to be set/unset.

  @param[in]  Offset  The register offset to write.
  @param[in]  Value   The value to write to the register.
  @param[in]  Mask    The bitmask to wait for.
  @param[in]  Set     Whether the mask should be set or unset.

**/
VOID
IoMmuWriteAndWait64 (
  IN UINTN    Offset,
  IN UINT64   Value,
  IN UINT64   Mask,
  IN BOOLEAN  Set
  )
{
  UINT64  RegValue;

  RegValue = Value;

  MmioWrite64 (mRiscVIoMmuGlobalDriverContext.Address + Offset, RegValue);
  while ((Set && !(RegValue & Mask)) || (!Set && (RegValue & Mask))) {
    MicroSecondDelay (5000);
    RegValue = MmioRead64 (mRiscVIoMmuGlobalDriverContext.Address + Offset);
  }
}
