================================
HVP 24.03 on xlnx-zcu102 machine
================================

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

Build and Run HVP on QEMU xlnx-zcu102
=====================================

Build the HVP Getting Started example for the ZynqMP

.. code::
 
 $ cd <HVP_INSTALL_DIR>
 $ export WIND_WRTOOL_WORKSPACE=<HVP_INSTALL_DIR>/helix/24.03/workspace
 $ cd helix/24.03/samples/examples/gettingStartedExample_Arm
 $ export USE_VXPRJ=y
 $ ./build.sh <name_of_project> ZynqMP HV_SAFETY_PLUS 

Run QEMU

.. code::

 $ qemu-system-aarch64 \
     -display none \
     -machine xlnx-zcu102,virtualization=on \
     -smp 4 \
     -m 4G \
     -kernel <HVP_INSTALL_DIR>/helix/24.03/workspace/<name_of_project>-hypervisor/uhypervisor \
     -dtb <HVP_INSTALL_DIR>/helix/24.03/workspace/<name_of_project>-hypervisor/xlnx-zcu102-rev-1.1.dtb \
     -serial mon:stdio
