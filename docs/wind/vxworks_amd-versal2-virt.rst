==========================================
VxWorks 26.03 on amd-versal2-virt machine
==========================================

This document contains steps to build a basic VxWorks project for the QEMU amd-versal2-virt machine.

Prerequisites
=============

* Build and install Wind River QEMU on your workstation. The source is publicly available at `<https://github.com/Wind-River/qemu/>`_.

* Install Wind River VxWorks on your workstation. For more information on VxWorks build and configuration, consult the VxWorks documentation.

Create VxWorks Source Build Project
===================================

From the workstation terminal, after setting up wrenv, run the following commands:

.. code::

 $ vxprj vsb create -lp64 -debug -bsp amd_versal2 vsb_amd_versal2 -S
 $ cd vsb_amd_versal2
 $ make


Create VxWorks Image Project
============================

From the workstation terminal, after building the VxWorks Source Build project, run the following:

.. code::

 $ vxprj create llvm -smp -debug -vsb vsb_amd_versal2 -profile PROFILE_DEVELOPMENT amd_versal2 vip_amd_versal2_smp
 $ cd vip_amd_versal2_smp


Build the VxWorks Image Project
===============================

.. code::

 $ vxprj build


Boot and Run VxWorks on QEMU
============================

Basic Configuration
-------------------

To boot the most basic configuration use the following QEMU command:

.. code::

 $ qemu-system-aarch64 \
     -M amd-versal2-virt \
     -m 4096 \
     -nographic \
     -kernel /path/to/VIP/default/uVxWorks \
     -append "f=0x1"
