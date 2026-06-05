=================================================
VxWorks 26.03 on the QEMU NXP i.MX8MP EVK Machine
=================================================

This document describes the basic steps used to build a VxWorks Source Build
(VSB), create a VxWorks Image Project (VIP), and boot the resulting image with
Wind River QEMU on the NXP i.MX8MP EVK machine.

The VxWorks BSP used for this flow is ``nxp_imx8`` with the
``INCLUDE_NXP_IMX8MP_EVK`` VIP component enabled.

Prerequisites
=============

* Build and install Wind River QEMU on the workstation. The QEMU source is
  available from the Wind River QEMU repository.

* Install Wind River VxWorks on the workstation.

* Configure the workstation terminal for VxWorks command-line development by
  entering the VxWorks environment before running ``vxprj`` commands.

Create the VxWorks Source Build Project
=======================================

From the workstation terminal, after setting up the VxWorks environment, create
the VSB for the ``nxp_imx8`` BSP:

.. code::

   $ cd ~/WindRiver/workspace
   $ vxprj vsb create -bsp nxp_imx8 vsb_nxp_imx8mp -S
   $ cd vsb_nxp_imx8mp
   $ make
   $ cd ..

Create the VxWorks Image Project
================================

Create the VIP from the VSB:

.. code::

   $ cd ~/WindRiver/workspace
   $ vxprj create -vsb vsb_nxp_imx8mp -profile PROFILE_DEVELOPMENT nxp_imx8 vip_nxp_imx8mp_evk
   $ cd vip_nxp_imx8mp_evk

Enable the i.MX8MP EVK board component:

.. code::

   $ vxprj component add INCLUDE_NXP_IMX8MP_EVK

Build the VxWorks Image Project
===============================

Build a compatible VxWorks image:

.. code::

   $ vxprj build

Boot and Run VxWorks on QEMU
============================

Basic Configuration
-------------------

Use the local Wind River QEMU binary and the generated VxWorks image. Replace
the paths below with the paths from your workstation.

.. code::

   $ /path/to/qemu-system-aarch64 \
       -machine imx8mp-evk \
       -m 6G \
       -smp 4 \
       -display none \
       -kernel /path/to/vip_imx/default/uVxWorks \
       -dtb /path/to/vip_imx/default/imx8mp-evk.dtb \
       -serial none \
       -serial stdio

The -display none option disables the graphical display window. This boot
flow uses the serial console, so no display device is required.

The two serial options are intentional and order-dependent. The i.MX8MP EVK
machine exposes more than one serial device, and the VxWorks console is on the
second serial device in this configuration. Therefore, -serial none disables
the first serial device and -serial stdio connects the second serial device
to the terminal.