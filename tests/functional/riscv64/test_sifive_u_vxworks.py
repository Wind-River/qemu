#!/usr/bin/env python3
#
# Functional test that boots a VxWorks kernel on a SiFive U machine
# and checks the console for a successful boot.
#
# Copyright (c) 2026 Wind River Systems, Inc.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os

from qemu_test import QemuSystemTest, wait_for_console_pattern


VXWORKS_SDK_PATH = os.environ.get(
    'QEMU_TEST_VXWORKS_SIFIVE_U_SDK',
    '/opt/wrsdk/vxworks-images'
)

BSP_DIR = os.path.join(VXWORKS_SDK_PATH, 'sifive_u')


class SiFiveUVxWorksTest(QemuSystemTest):
    """Boot VxWorks on SiFive U machine."""

    timeout = 30

    def test_vxworks_boot(self):
        """
        Boot VxWorks on SiFive U and wait for the shell prompt.

        The VxWorks image is expected at a path defined by the
        QEMU_TEST_VXWORKS_SIFIVE_U_SDK environment variable
        (default: /opt/wrsdk/vxworks-images).

        Expected layout:
          <SDK_PATH>/sifive_u/uVxWorks
        """

        kernel = os.path.join(BSP_DIR, 'uVxWorks')

        if not os.path.exists(kernel):
            self.skipTest(f'VxWorks kernel image not found: {kernel}')

        self.set_machine('sifive_u')
        self.vm.set_console()
        self.vm.add_args('-smp', '5')
        self.vm.add_args('-m', '2G')
        self.vm.add_args('-kernel', kernel)
        self.vm.launch()

        wait_for_console_pattern(self, '->')


if __name__ == '__main__':
    QemuSystemTest.main()
