=====================================
HVP 24.03 on xlnx-versal-virt machine
=====================================

Configuring an HVP build can be a fairly complex process.
For specific needs, please contact your WR account representative, or
reach the WR QEMU development team at our contact information found at
`<https://github.com/Wind-River/qemu/>`_.

For a simple example, please see the Getting Started examples in your
HVP installation.

Prerequisites
=============

* Build and install Wind River QEMU on your workstation. The source is publicly available at `<https://github.com/Wind-River/qemu/>`_.

* Install Wind River Helix Virtualization Platform 24.03 on your workstation.

Build and Run HVP on QEMU xlnx-versal-virt
==========================================

HVP source modifications
------------------------

Some modifications to the device tree source may be required. Please contact your Wind River account representative
or the WR QEMU team for more information.

Build HVP Sample Project
------------------------

.. code::

 $ cd <HVP_INSTALL_DIR>
 $ export WIND_WRTOOL_WORKSPACE=<HVP_INSTALL_DIR>/helix/24.03/workspace
 $ cd helix/24.03/samples/examples/gettingStartedExample_Arm
 $ export USE_VXPRJ=y
 $ ./build.sh <project_name> VCK190 HV_SAFETY_PLUS

Run QEMU
--------

.. code::

 $ qemu-system-aarch64 \
     -display none \
     -machine xlnx-versal-virt \
     -m 16G \
     -kernel <HVP_INSTALL_DIR>/helix/24.03/workspace/<project_name>-hypervisor/uhypervisor \
     -serial mon:stdio \
     -nic user,id=gem0 \
     -nic user,id=gem
