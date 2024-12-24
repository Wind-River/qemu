===========
QEMU README
===========

Wind River QEMU has improved support for
`VxWorks <https://www.windriver.com/products/vxworks>`_ and
`Wind River Helix Virtualization Platform <https://www.windriver.com/products/helix>`_.
This is a fork of the community QEMU project, rebased on the
latest stable upstream branch. For more information
about Wind River QEMU, please `contact us`_.

For more information about the upstream community QEMU project, please 
visit  `<https://github.com/qemu/qemu/>`_ or 
`<https://www.qemu.org/>`_.

QEMU as a whole is released under the GNU General Public License,
version 2. For full licensing details, consult the LICENSE file.


Legal Notices
=============

All product names, logos, and brands are property of their respective
owners. All company, product and service names used in this software
are for identification purposes only. Wind River and VxWorks are
registered trademarks of Wind River Systems, Inc. UNIX is a registered
trademark of The Open Group.

Disclaimer of Warranty / No Support: Wind River does not provide
support and maintenance services for this software, under Wind River’s
standard Software Support and Maintenance Agreement or otherwise.
Unless required by applicable law, Wind River provides the software
(and each contributor provides its contribution) on an “AS IS” BASIS,
WITHOUT WARRANTIES OF ANY KIND, either express or implied, including,
without limitation, any warranties of TITLE, NONINFRINGEMENT,
MERCHANTABILITY, or FITNESS FOR A PARTICULAR PURPOSE. You are solely
responsible for determining the appropriateness of using or
redistributing the software and assume any risks associated with your
exercise of permissions under the license.


Documentation
=============

Documentation can be found hosted online at
`<https://www.qemu.org/documentation/>`_. The documentation for the
current development version that is available at
`<https://www.qemu.org/docs/master/>`_ is generated from the ``docs/``
folder in the source tree, and is built by `Sphinx
<https://www.sphinx-doc.org/en/master/>`_.


Building
========

QEMU is multi-platform software intended to be buildable on all modern
Linux platforms, OS-X, Win32 (via the Mingw64 toolchain) and a variety
of other UNIX targets. The simple steps to build QEMU are:


.. code-block:: shell

  mkdir build
  cd build
  ../configure
  make

Additional information can also be found online via the QEMU website:

* `<https://wiki.qemu.org/Hosts/Linux>`_
* `<https://wiki.qemu.org/Hosts/Mac>`_
* `<https://wiki.qemu.org/Hosts/W32>`_


Submitting patches
==================

Please submit a pull request.


Bug reporting
=============

Please file an issue.


ChangeLog
=========

For version history and release notes, please look at the git history.


.. _contact us:

Contact
=======

The Wind River QEMU team can be reached through

James.Hui@windriver.com

Nelson.Ho@windriver.com

