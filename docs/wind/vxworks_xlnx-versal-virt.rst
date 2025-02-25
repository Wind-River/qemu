=========================================
VxWorks 24.03 on xlnx-versal-virt machine
=========================================

This document contains steps to build a basic VxWorks project for the QEMU xlnx-versal-virt machine.

Prerequisites
=============

* Build and install Wind River QEMU on your workstation. The source is publicly available at `<https://github.com/Wind-River/qemu/>`_.

* Install Wind River VxWorks on your workstation. For more information on VxWorks build and configuration, consult the VxWorks documentation.

Create VxWorks Source Build Project
===================================

From the workstation terminal, after setting up wrenv, run the following commands:

.. code::

 $ vxprj vsb create -lp64 -debug -bsp xlnx_versal vsb_xlnx_versal -S
 $ cd vsb_xlnx_versal
 $ make


Create VxWorks Image Project
============================

From the workstation terminal, after building the VxWorks Source Build project, run the following:

.. code::

 $ vxprj create llvm -smp -debug -vsb vsb_xlnx_versal -profile PROFILE_DEVELOPMENT xlnx_versal vip_xlnx_versal_smp
 $ cd vip_xlnx_versal_smp

For networking support using the built-in Cadence GEM network adapter, include the Generic PHY component:

.. code::

 $ vxprj component add INCLUDE_GENERICPHY

For PCIe support, add the following components:

.. code::

 $ vxprj component add DRV_PCI_ECAM
 $ vxprj component add INCLUDE_PCI_AUTOCONF

For Intel e1000/e1000e/igb PCIe pluggable NIC support, add the following components:

.. code::

 $ vxprj component add INCLUDE_GEI825XX_VXB_END

For virtio-net-pci support, add the following components:

.. code::

 $ vxprj component add INCLUDE_NET_VIRTIO
 $ vxprj component add INCLUDE_VIRTIO_LIB
 $ vxprj component add DRV_PCI_VIRTIO

Build the VxWorks Image Project

.. code::

 $ vxprj build


Boot and Run VxWorks on QEMU
============================

Basic Configuration
-------------------

To boot the most basic configuration use the following QEMU command:

.. code::

 $ qemu-system-aarch64 \
     -M xlnx-versal-virt \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -append "f=0x1"

With User Mode Networking
-------------------------

To boot and run VxWorks with basic networking support using the Cadence GEM network adapter
built into the xlnx-versal-virt machine and user-mode networking backend, run QEMU with the
following options:

.. code::

 $ qemu-system-aarch64 \
     -M xlnx-versal-virt \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -nic user,id=gem0
     -append "gem(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00 g=10.0.2.2 u=target pw=vxTarget f=0x1"

With Attached PCIe devices
--------------------------

The xlnx-versal-virt machine has a generic PCI express controller which can be used to
attach any of the PCI/PCIe devices that QEMU emulates.

e1000e PCIe NIC
```````````````

.. code::

 $ qemu-system-aarch64 \
     -M xlnx-versal-virt \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -netdev user,id=net0 \
     -device e1000e,netdev=net0,mac=00:00:e8:01:02:04 \
     -append "gei(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00 g=10.0.2.2 u=target pw=vxTarget f=0x1"

virtio-net-pci NIC
``````````````````

.. code::

 $ qemu-system-aarch64 \
     -M xlnx-versal-virt \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -netdev user,id=net0 \
     -device virtio-net-pci,disable-legacy=on,netdev=net0,mac=00:00:e8:01:02:04 \
     -append "virtioNet(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00 g=10.0.2.2 u=target pw=vxTarget f=0x1"

