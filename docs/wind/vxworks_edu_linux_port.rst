===================================================
Porting a Linux PCI Driver to VxWorks: QEMU ``edu``
===================================================

This document walks through porting a Linux QEMU ``edu`` PCI driver to a
VxWorks VxBus driver. The Linux reference used for the port is Giovanni
Santini's open-source ``edu_driver.c`` implementation. The VxWorks result is the
``vxbPciEdu`` driver.

The port therefore follows the same order a driver normally initializes:

#. copy the device attributes into VxWorks header definitions;
#. create the VxWorks source, header, makefile, and CDF files;
#. define the VxBus driver object;
#. match the PCI vendor/device ID;
#. attach the device and create per-device state;
#. allocate BAR0 as a VxBus memory resource;
#. access registers through VxBus accessors;
#. allocate, connect, and enable the INTx interrupt resource;
#. replace Linux completions with a VxWorks synchronization object;
#. allocate DMA memory and program the edu DMA engine;
#. detach by releasing resources in the reverse order.


Device Attributes
=================

The driver must define the ``edu`` device interface and register mappings to
match the implementation of the ``edu`` device model. The values below come from
QEMU's ``edu`` documentation and ``hw/misc/edu.c``.

The ``edu`` device exposes PCI ID ``1234:11e8`` and one 1 MB MMIO BAR at BAR0.
Inside BAR0, the driver uses these registers:

=======================  ========  =============================================
Register                 Offset    Purpose
=======================  ========  =============================================
``EDU_REG_ID``           ``0x00``  Identification register, currently ``0x010000ed``
``EDU_REG_LIVE``         ``0x04``  Liveness register; read returns inverse of write
``EDU_REG_FACT``         ``0x08``  Factorial register
``EDU_REG_STATUS``       ``0x20``  Status and factorial interrupt enable
``EDU_REG_IRQ_STATUS``   ``0x24``  Interrupt status
``EDU_REG_IRQ_RAISE``    ``0x60``  Raise interrupt
``EDU_REG_IRQ_ACK``      ``0x64``  Acknowledge interrupt
``EDU_REG_DMA_SRC``      ``0x80``  DMA source address
``EDU_REG_DMA_DST``      ``0x88``  DMA destination address
``EDU_REG_DMA_CNT``      ``0x90``  DMA byte count
``EDU_REG_DMA_CMD``      ``0x98``  DMA command/status
=======================  ========  =============================================

The device-side DMA buffer starts at offset ``0x40000`` and is 4096 bytes long.
The DMA command register uses bit ``0x1`` to start a transfer, bit ``0x2`` for
direction, and bit ``0x4`` to request an interrupt when DMA completes. Direction
``0`` means RAM to EDU, and direction ``1`` means EDU to RAM.

These definitions belong in ``vxbPciEdu.h``:

.. code-block:: c

   #define EDU_PCI_VENDOR_ID       0x1234
   #define EDU_PCI_DEVICE_ID       0x11e8

   #define EDU_REG_ID              0x00
   #define EDU_REG_LIVE            0x04
   #define EDU_REG_FACT            0x08
   #define EDU_REG_STATUS          0x20
   #define EDU_REG_IRQ_STATUS      0x24
   #define EDU_REG_IRQ_RAISE       0x60
   #define EDU_REG_IRQ_ACK         0x64
   #define EDU_REG_DMA_SRC         0x80
   #define EDU_REG_DMA_DST         0x88
   #define EDU_REG_DMA_CNT         0x90
   #define EDU_REG_DMA_CMD         0x98

   #define EDU_ID_MAGIC            0x010000ed

   #define EDU_STATUS_COMPUTING    0x00000001
   #define EDU_STATUS_IRQ_ENABLE   0x00000080

   #define EDU_IRQ_FACT            0x00000001
   #define EDU_IRQ_DMA             0x00000100

   #define EDU_DMA_BUF_DEVADDR     0x40000
   #define EDU_DMA_BUF_SIZE        4096

   #define EDU_DMA_CMD_RUN         0x1
   #define EDU_DMA_CMD_DIR         0x2
   #define EDU_DMA_CMD_IRQ         0x4

In the Linux reference these same ideas appear as ``EDU_VENDOR_ID``,
``EDU_DEVICE_ID``, ``EDU_MMIO_*`` register offsets, ``EDU_DMA_BUFF_ADDR``, and
``EDU_DMA_BUFF_SIZE``. The names change, but the hardware values should not.


Create the VxWorks Driver Files
===============================

The VxWorks port uses four files:

.. code-block:: text

   os/drv/vxbus/drv/src/busCtlr/vxbPciEdu.c
   os/drv/vxbus/drv/src/busCtlr/vxbPciEdu.h
   os/drv/vxbus/drv/src/busCtlr/vxbPciEdu.mk
   os/drv/vxbus/drv/cdf/40vxbPciEdu.cdf

The Linux reference is a Linux module. It has Linux module entry/exit hooks,
``pci_register_driver()``, and a misc-device interface with ``read`` and
``ioctl``. Those Linux userspace pieces do not carry over directly. In VxWorks,
the source file defines a VxBus driver, the ``.mk`` file adds the object to the
vsb, and the CDF file makes the driver component selectable in the vip.

The makefile fragment is small:

.. code-block:: make

   OBJS_COMMON += vxbPciEdu.o
   DOC_FILES   += vxbPciEdu.c

The CDF component defines the vip component:

.. code-block:: c

   Component INCLUDE_PCI_EDU {
       NAME            QEMU edu PCI device driver
       SYNOPSIS        This component provides a VxBus driver for the QEMU "edu" \
                       PCI device. It maps BAR0, connects INTx interrupt support, \
                       and programs DMA transfers.
       MODULES         vxbPciEdu.o
       LINK_SYMS       vxbPciEduDrv
       REQUIRES        INCLUDE_PCI_BUS \
                       INCLUDE_CACHE_SUPPORT
       _CHILDREN       FOLDER_DRIVERS_PCI
   }

``MODULES`` names the object file. ``LINK_SYMS`` lists the symbols the module
defines that are exported from the module. ``REQUIRES`` lists the VxWorks
components that this component depends on. The CDF only defines the component as
a compilation module; the ``VXB_DRV_DEF`` defines the runtime structure that is
registered with the VxBus subsystem.


Define the VxBus Driver
=======================

The Linux driver centers around this object:

.. code-block:: c

   static struct pci_driver edu_driver = {
       .name     = MODULE_NAME,
       .id_table = edu_device_ids,
       .probe    = edu_driver_probe,
       .remove   = edu_driver_remove,
   };

That is the object Linux registers with ``pci_register_driver()``.

In VxWorks, the equivalent framework object is ``VXB_DRV``. Instead of named
fields like ``.probe`` and ``.remove``, VxBus uses a method table:

.. code-block:: c

   LOCAL VXB_DRV_METHOD eduMethodList[] =
       {
       { VXB_DEVMETHOD_CALL(vxbDevProbe),  (FUNCPTR)eduProbe },
       { VXB_DEVMETHOD_CALL(vxbDevAttach), (FUNCPTR)eduAttach },
       { VXB_DEVMETHOD_CALL(vxbDevDetach), (FUNCPTR)eduDetach },
       { 0, NULL }
       };

   VXB_DRV vxbPciEduDrv =
       {
       { NULL },
       "edu",
       "QEMU edu PCI device",
       VXB_BUSID_PCI,
       0,
       0,
       eduMethodList
       };

   VXB_DRV_DEF(vxbPciEduDrv)

``VXB_BUSID_PCI`` tells VxBus that this is a PCI driver. ``VXB_DRV_DEF`` makes
the driver object visible to the VxBus driver framework once the symbol is
linked into the image.


Match the PCI Device
====================

The Linux reference has a PCI ID table:

.. code-block:: c

   static const struct pci_device_id edu_device_ids[] = {
       { EDU_VENDOR_ID, EDU_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
       { 0 }
   };

The VxWorks port uses a VxBus PCI match table:

.. code-block:: c

   LOCAL const VXB_PCI_DEV_MATCH_ENTRY eduMatch[] =
       {
           {
           EDU_PCI_DEVICE_ID,
           EDU_PCI_VENDOR_ID,
           NULL
           },
           {}
       };

One detail matters here: this VxWorks match entry uses device ID first and
vendor ID second. The Linux reference lists vendor first and device second.

The Linux driver performs matching through the PCI core before calling
``edu_driver_probe()``. The VxWorks driver makes that match explicit in
``eduProbe()``:

.. code-block:: c

   LOCAL STATUS eduProbe
       (
       VXB_DEV_ID pDev
       )
       {
       return vxbPciDevMatch (pDev, eduMatch, NULL);
       }

``VXB_DEV_ID`` is the VxBus handle for the device instance, similar in role to
the ``struct pci_dev *`` passed into the Linux ``probe()`` callback.


Attach and Create Driver State
==============================

The Linux reference does most initialization inside ``edu_driver_probe()``. It
enables the PCI device, requests the BAR, maps MMIO, sets up DMA, initializes a
completion, and requests the IRQ.

VxWorks splits this lifecycle into two methods:

* ``eduProbe()`` checks whether the driver matches the device.
* ``eduAttach()`` allocates state and prepares the device for use.

The first thing attach needs is per-device state. The Linux reference keeps its
state in one file-scope object:

.. code-block:: c

   struct edu_device {
       struct pci_dev *pci_dev;
       void __iomem *pci_mem;
       void *dma_virt_addr;
       dma_addr_t dma_phys_addr;
       size_t dma_size;
       struct completion dma_done;
   };

   static struct edu_device edu = {0};

That is acceptable for a small Linux example, but a VxBus driver should carry
state per attached device. In VxWorks this state is usually called the softc:

.. code-block:: c

   typedef struct eduDrvCtrl
       {
       VXB_DEV_ID      pDev;
       VXB_RESOURCE *  pBar0Res;
       VXB_RESOURCE *  pIrqRes;
       void *          regBase;
       void *          regHandle;
       SEM_ID          irqSem;
       volatile UINT32 irqCount;
       volatile UINT32 lastIrqStatus;
       void *          dmaVirt;
       PHYS_ADDR       dmaAddr;
       size_t          dmaSize;
       } EDU_DRV_CTRL;

Attach allocates and clears this state:

.. code-block:: c

   pCtrl = (EDU_DRV_CTRL *)vxbMemAlloc (sizeof (EDU_DRV_CTRL));
   if (pCtrl == NULL)
       return ERROR;

   memset (pCtrl, 0, sizeof (*pCtrl));
   pCtrl->pDev   = pDev;
   pCtrl->irqSem = SEM_ID_NULL;

Then the softc is attached to the VxBus device:

.. code-block:: c

   vxbDevSoftcSet (pDev, (void *)pCtrl);

Later code retrieves it with:

.. code-block:: c

   pCtrl = (EDU_DRV_CTRL *)vxbDevSoftcGet (pDev);

This is the VxWorks equivalent of keeping per-device private state. In Linux,
many PCI drivers would use ``pci_set_drvdata()`` and ``pci_get_drvdata()`` for
that pattern. The Santini reference uses a global object instead, so the VxWorks
port is slightly cleaner here.


Map BAR0 With a VxBus Memory Resource
=====================================

In the Linux reference, BAR0 is handled in two steps:

.. code-block:: c

   pci_request_region(pci_dev, EDU_BAR, MODULE_NAME);
   edu.pci_mem = pci_iomap(pci_dev, EDU_BAR, EDU_BAR_MAX_LEN);

``pci_request_region()`` reserves the BAR so another driver does not claim the
same range. ``pci_iomap()`` returns the CPU-visible MMIO pointer used by
``ioread32()`` and ``iowrite32()``.

In VxWorks, BAR0 comes through the VxBus resource system:

.. code-block:: c

   pRes = vxbResourceAlloc (pDev, VXB_RES_MEMORY, 0);
   if (pRes == NULL)
       {
       vxbMemFree (pCtrl);
       return ERROR;
       }

   pResAdr = (VXB_RESOURCE_ADR *)pRes->pRes;
   if (pResAdr == NULL)
       {
       vxbMemFree (pCtrl);
       return ERROR;
       }

   pCtrl->pBar0Res  = pRes;
   pCtrl->regBase   = (void *)pResAdr->virtAddr;
   pCtrl->regHandle = pResAdr->pHandle;

``vxbResourceAlloc(pDev, VXB_RES_MEMORY, 0)`` asks VxBus for the first memory
resource owned by the PCI device. For this device, that is BAR0. The returned
``VXB_RESOURCE`` contains a ``VXB_RESOURCE_ADR`` structure:

* ``virtAddr`` is the mapped CPU address of the BAR, similar to what Linux gets
  from ``pci_iomap()``.
* ``pHandle`` is the VxBus access handle used by ``vxbRead32()`` and
  ``vxbWrite32()``.

The driver keeps ``pBar0Res`` in the softc so detach can release it later with
``vxbResourceFree()``.

After BAR0 is mapped, the driver reads the ID register:

.. code-block:: c

   id = EDU_REG_READ (pCtrl, EDU_REG_ID);
   if (id != EDU_ID_MAGIC)
       return ERROR;

That read confirms that the mapped BAR is really the expected ``edu`` register
space.


Use VxBus Register Accessors
============================

The Linux reference uses:

.. code-block:: c

   ioread32(edu.pci_mem + offset);
   iowrite32(value, edu.pci_mem + offset);

The VxWorks port uses ``vxbRead32()`` and ``vxbWrite32()``. The driver wraps
them so every access includes both the BAR base address and the VxBus access
handle:

.. code-block:: c

   #define EDU_REG_READ(pCtrl, offset)                         \
       vxbRead32 ((pCtrl)->regHandle,                          \
                  (UINT32 *)((unsigned long)(pCtrl)->regBase + \
                             (unsigned long)(offset)))

   #define EDU_REG_WRITE(pCtrl, offset, value)                  \
       vxbWrite32 ((pCtrl)->regHandle,                         \
                   (UINT32 *)((unsigned long)(pCtrl)->regBase +\
                              (unsigned long)(offset)),        \
                   (value))

This is not just cosmetic. Device registers should go through bus-aware access
helpers, not plain pointer dereferences. The VxBus handle carries the bus access
information needed by the target architecture.


Connect INTx Interrupts
=======================

The Linux reference asks Linux for the IRQ like this:

.. code-block:: c

   request_irq(pci_dev->irq, edu_driver_irq_handler, 0, MODULE_NAME, pci_dev);

Linux gives the driver an IRQ number in ``pci_dev->irq``. ``request_irq()``
registers the handler and enables the interrupt path.

In VxWorks, interrupts are also resources. The driver first creates a semaphore
for interrupt synchronization:

.. code-block:: c

   pCtrl->irqSem = semBCreate (SEM_Q_PRIORITY, SEM_EMPTY);
   if (pCtrl->irqSem == SEM_ID_NULL)
       return ERROR;

Then it allocates the interrupt resource, connects the ISR, and enables the
interrupt:

.. code-block:: c

   pCtrl->pIrqRes = vxbResourceAlloc (pDev, VXB_RES_IRQ, 0);
   if (pCtrl->pIrqRes == NULL)
       return ERROR;

   if (vxbIntConnect (pDev, pCtrl->pIrqRes, eduIsr, (void *)pCtrl) != OK)
       return ERROR;

   if (vxbIntEnable (pDev, pCtrl->pIrqRes) != OK)
       return ERROR;

The Linux ``request_irq()`` call roughly turns into three VxWorks steps:

=========================  ====================================================
Linux                      VxWorks
=========================  ====================================================
``pci_dev->irq``           ``vxbResourceAlloc(pDev, VXB_RES_IRQ, 0)``
handler registration       ``vxbIntConnect()``
interrupt enable           ``vxbIntEnable()``
=========================  ====================================================

The ISR reads the ``edu`` interrupt status register, acknowledges the bits it
saw, records the status, and wakes the waiting task:

.. code-block:: c

   LOCAL void eduIsr
       (
       void * arg
       )
       {
       EDU_DRV_CTRL * pCtrl = (EDU_DRV_CTRL *)arg;
       UINT32         status;

       status = EDU_REG_READ (pCtrl, EDU_REG_IRQ_STATUS);

       if (status == 0)
           return;

       EDU_REG_WRITE (pCtrl, EDU_REG_IRQ_ACK, status);

       pCtrl->lastIrqStatus = status;
       pCtrl->irqCount++;

       if (pCtrl->irqSem != SEM_ID_NULL)
           semGive (pCtrl->irqSem);
       }

Writing ``EDU_REG_IRQ_ACK`` is what clears the device interrupt. The semaphore
does not clear hardware state; it only wakes the task waiting for completion.


Replace Linux Completions With a Semaphore
==========================================

The Linux reference uses ``struct completion dma_done`` for DMA completion:

.. code-block:: c

   reinit_completion(&edu.dma_done);
   complete(&edu.dma_done);
   wait_for_completion_timeout(&edu.dma_done, msecs_to_jiffies(100));

VxWorks does not use Linux completions. The VxWorks port uses a binary
semaphore instead:

.. code-block:: c

   pCtrl->irqSem = semBCreate (SEM_Q_PRIORITY, SEM_EMPTY);

Before starting an interrupt-driven operation, stale semaphore tokens are
drained:

.. code-block:: c

   LOCAL void eduSemDrain
       (
       EDU_DRV_CTRL * pCtrl
       )
       {
       if ((pCtrl == NULL) || (pCtrl->irqSem == SEM_ID_NULL))
           return;

       while (semTake (pCtrl->irqSem, NO_WAIT) == OK)
           ;
       }

The task waits with ``semTake()``, and the ISR wakes it with ``semGive()``. That
is the VxWorks version of the same driver-level pattern: start work in task
context, finish it from interrupt context.

A production driver would also mask the device interrupt inside the ISR when it
signals the semaphore, and re-enable it once the device has been serviced.
Otherwise a device that raises interrupts rapidly can continuously preempt the
consumer task and keep draining the semaphore in the ``semTake()`` loop.

A binary semaphore is only one option for replacing a Linux completion. VxWorks
also provides deferred-interrupt mechanisms such as ``jobDefer()`` and the
``tIsr`` task, which can be a better fit for this pattern.


Allocate DMA Memory
===================

The Linux reference sets a DMA mask and allocates coherent DMA memory:

.. code-block:: c

   dma_set_mask_and_coherent(&pci_dev->dev, dma_mask);
   edu.dma_virt_addr = dma_alloc_coherent(&pci_dev->dev,
                                          EDU_DMA_BUFF_SIZE,
                                          &edu.dma_phys_addr,
                                          GFP_KERNEL);

It keeps two addresses for the same buffer:

* ``dma_virt_addr`` is the CPU pointer;
* ``dma_phys_addr`` is the address programmed into the device.

The VxWorks port follows the same idea with VxWorks cache-DMA APIs:

.. code-block:: c

   pCtrl->dmaVirt = cacheDmaMalloc (EDU_DMA_BUF_SIZE);
   if (pCtrl->dmaVirt == NULL)
       return ERROR;

   pCtrl->dmaAddr = (PHYS_ADDR)CACHE_DMA_VIRT_TO_PHYS (pCtrl->dmaVirt);
   pCtrl->dmaSize = EDU_DMA_BUF_SIZE;

``dmaVirt`` is the CPU-visible pointer. ``dmaAddr`` is the device-side address
used when programming ``EDU_REG_DMA_SRC`` or ``EDU_REG_DMA_DST``.

The current driver writes ``dmaAddr`` to the ``edu`` DMA registers as a
``UINT32``. That matches the QEMU ``edu`` device's default 28-bit DMA mask, but
it is not a general 64-bit DMA pattern. If the device is configured with a wider
DMA mask, or if the platform can allocate DMA memory above 4 GB, the driver
should program the full address in the form expected by the device.


Program DMA Transfers
=====================

The Linux reference programs DMA by writing the ``edu`` DMA registers with
``iowrite32()``. The VxWorks port writes the same registers through
``EDU_REG_WRITE()``.

For RAM-to-device DMA, the CPU first copies data into the DMA buffer. The driver
then flushes the cache so the device sees the latest bytes in RAM:

.. code-block:: c

   memcpy (pCtrl->dmaVirt, src, count);
   CACHE_DMA_FLUSH (pCtrl->dmaVirt, count);
   eduSemDrain (pCtrl);

   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_SRC, (UINT32)pCtrl->dmaAddr);
   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_DST, EDU_DMA_BUF_DEVADDR);
   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_CNT, count);
   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_CMD,
                  EDU_DMA_CMD_RUN | EDU_DMA_CMD_IRQ);

   if (semTake (pCtrl->irqSem, sysClkRateGet ()) != OK)
       return ERROR;

For device-to-RAM DMA, the driver starts the transfer, waits for completion, and
then invalidates the cache before reading the buffer from the CPU:

.. code-block:: c

   eduSemDrain (pCtrl);

   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_SRC, EDU_DMA_BUF_DEVADDR);
   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_DST, (UINT32)pCtrl->dmaAddr);
   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_CNT, count);
   EDU_REG_WRITE (pCtrl, EDU_REG_DMA_CMD,
                  EDU_DMA_CMD_RUN | EDU_DMA_CMD_DIR | EDU_DMA_CMD_IRQ);

   if (semTake (pCtrl->irqSem, sysClkRateGet ()) != OK)
       return ERROR;

   CACHE_DMA_INVALIDATE (pCtrl->dmaVirt, count);
   memcpy (dest, pCtrl->dmaVirt, count);

The cache operations are the VxWorks side of the same problem the Linux driver
handles with coherent DMA memory and memory barriers: the CPU and the device
must agree on the contents of memory at the point ownership effectively changes.


Detach and Release Resources
============================

Detach should undo attach in a safe order. The Linux remove path releases the
IRQ, frees coherent DMA memory, releases the PCI region, and disables the
device. The VxWorks path releases the VxBus resources and softc state:

.. code-block:: c

   if (pCtrl->regBase != NULL)
       EDU_REG_WRITE (pCtrl, EDU_REG_STATUS, 0);

   if (pCtrl->pIrqRes != NULL)
       {
       (void) vxbIntDisable    (pDev, pCtrl->pIrqRes);
       (void) vxbIntDisconnect (pDev, pCtrl->pIrqRes);
       (void) vxbResourceFree  (pDev, pCtrl->pIrqRes);
       pCtrl->pIrqRes = NULL;
       }

   if (pCtrl->irqSem != SEM_ID_NULL)
       {
       semDelete (pCtrl->irqSem);
       pCtrl->irqSem = SEM_ID_NULL;
       }

   if (pCtrl->pBar0Res != NULL)
       {
       (void) vxbResourceFree (pDev, pCtrl->pBar0Res);
       pCtrl->pBar0Res = NULL;
       }

   if (pCtrl->dmaVirt != NULL)
       {
       cacheDmaFree (pCtrl->dmaVirt);
       pCtrl->dmaVirt = NULL;
       }

   vxbDevSoftcSet (pDev, NULL);
   vxbMemFree (pCtrl);

The interrupt is disabled and disconnected before freeing anything the ISR can
touch. The DMA buffer is freed before the softc because the DMA pointer is
stored inside the softc.


Build the VSB and VIP
=====================

For the Versal 2 setup used during this port, the build flow was:

.. code-block:: console

   vxprj vsb create -lp64 -bsp amd_versal2 myVSB
   cd myVSB
   vxprj build

   vxprj vip create llvm -smp -vsb myVSB -profile PROFILE_DEVELOPMENT amd_versal2 myVIP
   cd myVIP
   vxprj component add INCLUDE_PCI_EDU
   vxprj component add DRV_PCI_ECAM
   vxprj component add INCLUDE_PCI_AUTOCONF
   vxprj build

The exact BSP and PCI host-controller components depend on the target. The key
point is that the driver object is built into the VSB, and the VIP links the
selected components into the final VxWorks image.


What Carries Over, and What Does Not
====================================

The hardware behavior carries over directly:

* PCI vendor/device ID;
* BAR0 register layout;
* interrupt status and acknowledge behavior;
* factorial and DMA register programming;
* DMA buffer offset and size.

The Linux framework code does not carry over directly:

* ``struct pci_driver`` becomes ``VXB_DRV`` plus a VxBus method table.
* ``pci_register_driver()`` becomes ``VXB_DRV_DEF`` plus CDF/image linkage.
* ``struct pci_device_id`` becomes ``VXB_PCI_DEV_MATCH_ENTRY``.
* Linux ``probe()`` is split into VxBus probe and attach.
* Linux's global device state or ``drvdata`` pattern becomes a VxBus softc.
* ``pci_request_region()`` and ``pci_iomap()`` become memory resource
  allocation through ``vxbResourceAlloc(VXB_RES_MEMORY)``.
* ``ioread32()`` and ``iowrite32()`` become ``vxbRead32()`` and
  ``vxbWrite32()``.
* ``request_irq()`` becomes IRQ resource allocation plus ``vxbIntConnect()``
  and ``vxbIntEnable()``.
* Linux completions become a VxWorks synchronization primitive, here a binary
  semaphore.
* Linux DMA allocation becomes VxWorks cache-DMA allocation and address
  conversion.
* Linux ``miscdevice``, ``read``, and ``ioctl`` are Linux userspace interfaces;
  the VxWorks driver should expose the interface required by the VxWorks
  subsystem or application.


Evidence Used
=============

* Linux reference driver used as the porting model:
  https://github.com/San7o/edu-driver/blob/main/edu/edu_driver.c
* QEMU ``edu`` device documentation:
  https://www.qemu.org/docs/master/specs/edu.html
* QEMU ``edu`` source model:
  https://github.com/qemu/qemu/blob/master/hw/misc/edu.c
* Linux PCI driver documentation:
  https://docs.kernel.org/PCI/pci.html
* Working VxWorks driver provided for this port:
  ``vxbPciEdu.c``, ``vxbPciEdu.h``, ``vxbPciEdu.mk``, and
  ``40vxbPciEdu.cdf``.
