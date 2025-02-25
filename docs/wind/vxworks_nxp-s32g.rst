=========================================
VxWorks 24.03 on nxp-s32g machine
=========================================

This document contains steps to build a basic VxWorks project for the QEMU nxp-s32g machine.

Prerequisites
=============

* Build and install Wind River QEMU on your workstation. The source is publicly available at `<https://github.com/Wind-River/qemu/>`_.

* Install Wind River VxWorks on your workstation. For more information on VxWorks build and configuration, consult the VxWorks documentation.

Create VxWorks Source Build Project
===================================

From the workstation terminal, after setting up wrenv, run the following commands:

.. code::

 $ vxprj vsb create -lp64 -debug -bsp nxp_s32g3 vsb_nxp_s32g3 -S
 $ cd vsb_nxp_s32g3

The nxp-s32g machine has a limited implementation of the FlexCAN CAN controller. To build
support for this, add the following config to your VSB:

.. code::

 $ vxprj vsb add CAN

Build the VxWorks Source Build project

.. code::

 $ make


Create VxWorks Image Project
============================

From the workstation terminal, after building the VxWorks Source Build project, run the following:

.. code::

 $ vxprj create llvm -smp -debug -vsb vsb_nxp_s32g3 -profile PROFILE_DEVELOPMENT nxp_s32g3 vip_nxp_s32g3_smp/
 $ cd vip_nxp_s32g3_smp

For networking support on this model, you must use MMIO virtio-net adapter. To enable this,
add the following components to your VIP:

.. code::

 $ vxprj component add INCLUDE_NET_VIRTIO
 $ vxprj component add DRV_FDT_VIRTIO
 $ vxprj component add INCLUDE_VIRTIO_LIB

To enable support for the FlexCAN CAN controller device model, add the following components to your VIP:

.. code::

 $ vxprj vip component add INCLUDE_FSL_FLEXCAN
 $ vxprj vip component add INCLUDE_CANLIB
 $ vxprj vip component add INCLUDE_SOCKETCAN

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
     -M nxp-s32g \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -append "f=0x1"

With User Mode Networking
-------------------------

To boot and run VxWorks with basic networking support using the MMIO virtio-net adapter and
user mode networking backend, run QEMU with the following options:

.. code::

 $ qemu-system-aarch64 \
     -M nxp-s32g \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -netdev user,id=net0 \
     -device virtio-net-device,netdev=net0 \
     -append "virtioNet(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00 g=10.0.2.2 u=target pw=vxTarget f=0x1"

FlexCAN/SocketCAN Configuration
-------------------------------

To create a virtual CAN interface on your host

.. code::

 $ sudo ip link add dev vcan0 type vcan
 $ sudo ip link set up vcan0

To boot and run VxWorks with FlexCAN controller connected to the virtual CAN interface

.. code::

 $ qemu-system-aarch64 \
     -M nxp-s32g \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -object can-bus,id=canbus0-bus \
     -machine canbus0=canbus0-bus \
     -object can-host-socketcan,id=socketcan0,if=vcan0,canbus=canbus0-bus
