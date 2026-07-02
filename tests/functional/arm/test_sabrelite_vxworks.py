#!/usr/bin/env python3
#
# Functional test that boots VxWorks on the QEMU Sabrelite (i.MX6Q) machine
# and checks the console for a successful boot.
#
# Copyright (c) 2026 Wind River Systems, Inc.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os

from qemu_test import QemuSystemTest, wait_for_console_pattern

VXWORKS_SDK_PATH = os.environ.get(
    'QEMU_TEST_VXWORKS_SABRELITE_SDK',
    '/opt/wrsdk/vxworks-images'
)

BSP_DIR = os.path.join(VXWORKS_SDK_PATH, 'sabrelite')


class SabreliteVxWorksTest(QemuSystemTest):
    """Boot VxWorks on the QEMU sabrelite (i.MX6Q) machine."""

    def test_vxworks_boot(self):
        """
        Boot VxWorks on sabrelite and wait for the shell prompt.

        The VxWorks image and DTB are expected at a path defined by the
        QEMU_TEST_VXWORKS_SABRELITE_SDK environment variable
        (default: /opt/wrsdk/vxworks-images).

        Expected layout:
          <SDK_PATH>/sabrelite/uVxWorks
          <SDK_PATH>/sabrelite/imx6q-sabrelite.dtb
        """
        kernel = os.path.join(BSP_DIR, 'uVxWorks')
        dtb = os.path.join(BSP_DIR, 'imx6q-sabrelite.dtb')

        if not os.path.exists(kernel):
            self.skipTest(f'VxWorks kernel image not found: {kernel}')
        if not os.path.exists(dtb):
            self.skipTest(f'VxWorks DTB not found: {dtb}')

        self.set_machine('sabrelite')
        # The sabrelite board uses the second UART as serial console,
        # so we set console_index=1 and add -serial null for the first.
        self.vm.set_console(console_index=1)
        self.vm.add_args('-smp', '4')
        self.vm.add_args('-m', '1G')
        self.vm.add_args('-serial', 'null')
        self.vm.add_args('-kernel', kernel)
        self.vm.add_args('-dtb', dtb)
        self.vm.launch()

        wait_for_console_pattern(self, '->')


if __name__ == '__main__':
    QemuSystemTest.main()
