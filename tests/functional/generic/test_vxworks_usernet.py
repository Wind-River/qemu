#!/usr/bin/env python3
#
# Functional test that boots VxWorks on multiple machines and verifies
# user-mode (SLIRP) networking works by performing a TFTP transfer.
#
# QEMU's SLIRP backend includes a built-in TFTP server. This test
# places a known file in a temporary directory, configures SLIRP to
# serve it, boots VxWorks, and uses the VxWorks shell to fetch the
# file via TFTP -- proving that the full network data path works
# (ARP, IP, UDP, TFTP).
#
# Copyright (c) 2026 Wind River Systems, Inc.
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import tempfile

from qemu_test import QemuSystemTest, wait_for_console_pattern, \
    exec_command, exec_command_and_wait_for_pattern


# ---------------------------------------------------------------------------
# Shell type configuration
# ---------------------------------------------------------------------------
# VxWorks supports two shell types with different command syntax:
#
#   'cmd' - Command shell (default)
#           Commands use CLI syntax: tftp <host> get <file>
#           Success indicated by: "bytes transferred"
#
#   'c'   - C interpreter shell (prompt: "-> ")
#           Commands use function-call syntax: iptftp_client_get ...
#           Success indicated by: "value = 0"
#
# The shell type can be overridden via QEMU_TEST_VXWORKS_SHELL_TYPE.

SHELL_CONFIGS = {
    'cmd': {
        'prompt': ']#',
        'tftp_cmd_fmt': 'tftp {host} get {file} /ram0/{file}',
        'tftp_success': 'Transfer completed',
    },
    'c': {
        'prompt': '->',
        'tftp_cmd_fmt': 'iptftp_client_get "{host}", "{file}"',
        'tftp_success': 'value = 0',
    },
}

# ---------------------------------------------------------------------------
# Per-machine configuration
# ---------------------------------------------------------------------------
# Each entry describes how to launch QEMU and interact with VxWorks for
# a specific machine/architecture combination.
#
# Fields:
#   qemu_bin:     QEMU system binary name (resolved from build dir)
#   machine:      QEMU -M value
#   extra_args:   additional QEMU arguments (SMP, memory, etc.)
#   nic_args:     arguments to attach the user netdev to a NIC
#                 (empty list if the machine's built-in NIC is used)
#   netdev_extra: extra options appended to the -netdev user,... string
#   console_index: which serial port is the VxWorks console
#   kernel_env:   environment variable holding the VxWorks kernel path
#   dtb_env:      environment variable holding an optional DTB path (or None)
#   net_setup_cmd: VxWorks command to bring up the network (or None if DHCP)
#

MACHINE_CONFIGS = {
    'riscv64_virt': {
        'qemu_bin': 'qemu-system-riscv64',
        'machine': 'virt',
        'extra_args': ['-smp', '4', '-m', '2G',
                       '-append',
                       'gei(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget f=0x1'],
        'nic_args': ['-device', 'e1000,netdev=net0'],
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_RISCV64_VIRT_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_RISCV64_VIRT_DTB',
        'net_setup_cmd': None,
    },
    'riscv64_sifive_u': {
        'qemu_bin': 'qemu-system-riscv64',
        'machine': 'sifive_u',
        'extra_args': ['-smp', '5', '-m', '2G',
                       '-append',
                       'gem(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget f=0x1'],
        'nic_args': [],  # built-in cadence_gem
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_RISCV64_SIFIVE_U_KERNEL',
        'dtb_env': None,
        'net_setup_cmd': None,
    },
    'aarch64_amd_versal': {
        'qemu_bin': 'qemu-system-aarch64',
        'machine': 'amd-versal-virt',
        'extra_args': ['-smp', '4', '-m', '4096',
                       '-append',
                       'gem(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget f=0x1'],
        'nic_args': [],  # built-in cadence_gem, use -nic instead of -netdev
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_AARCH64_VERSAL_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_AARCH64_VERSAL_DTB',
        'net_setup_cmd': None,
    },
    'aarch64_amd_versal2': {
        'qemu_bin': 'qemu-system-aarch64',
        'machine': 'amd-versal2-virt',
        'extra_args': ['-m', '4096',
                       '-append',
                       'gem(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget f=0x1'],
        'nic_args': [],  # built-in cadence_gem, use -nic instead of -netdev
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_AARCH64_VERSAL2_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_AARCH64_VERSAL2_DTB',
        'net_setup_cmd': None,
    },
    'aarch64_xlnx_zcu102': {
        'qemu_bin': 'qemu-system-aarch64',
        'machine': 'xlnx-zcu102',
        'extra_args': ['-smp', '4', '-m', '4096',
                       '-nic', 'user', '-nic', 'user', '-nic', 'user',
                       '-append',
                       'gem(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget'],
        'nic_args': [],  # VxWorks uses GEM3 (4th cadence_gem); the 3 extra
                         # -nic user in extra_args fill GEM0-2 so that the
                         # test's -nic user,tftp=... lands on GEM3.
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_AARCH64_ZCU102_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_AARCH64_ZCU102_DTB',
        'net_setup_cmd': None,
    },
    'aarch64_nxp_s32g': {
        'qemu_bin': 'qemu-system-aarch64',
        'machine': 'nxp-s32g',
        'extra_args': ['-smp', '4', '-m', '4096',
                       '-append',
                       'virtioNet(0,0)host:vxWorks h=10.0.2.2'
                       ' e=10.0.2.15:ffffff00 g=10.0.2.2 u=target'
                       ' pw=vxTarget f=0x1'],
        'nic_args': ['-device', 'virtio-net-device,netdev=net0'],
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_AARCH64_S32G_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_AARCH64_S32G_DTB',
        'net_setup_cmd': None,
    },
    'aarch64_nxp_imx8mp': {
        'qemu_bin': 'qemu-system-aarch64',
        'machine': 'imx8mp-evk',
        'extra_args': ['-smp', '4', '-m', '6G',
                       '-append',
                       'enet(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget f=0x1'],
        'nic_args': [],  # built-in i.MX FEC/ENET
        'netdev_extra': '',
        'console_index': 1,
        'kernel_env': 'QEMU_TEST_VXWORKS_AARCH64_IMX8MP_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_AARCH64_IMX8MP_DTB',
        'net_setup_cmd': None,
    },
    'arm_imx6_sabrelite': {
        'qemu_bin': 'qemu-system-arm',
        'machine': 'sabrelite',
        'extra_args': ['-smp', '4', '-m', '1G',
                       '-serial', 'null',
                       '-append',
                       'enet(0,0)host:vxWorks h=10.0.2.2 e=10.0.2.15:ffffff00'
                       ' g=10.0.2.2 u=target pw=vxTarget'],
        'nic_args': [],  # built-in i.MX FEC/ENET
        'netdev_extra': '',
        'console_index': 1,
        'kernel_env': 'QEMU_TEST_VXWORKS_ARM_SABRELITE_KERNEL',
        'dtb_env': 'QEMU_TEST_VXWORKS_ARM_SABRELITE_DTB',
        'net_setup_cmd': None,
    },
    'x86_64_q35': {
        'qemu_bin': 'qemu-system-x86_64',
        'machine': 'q35',
        'extra_args': ['-smp', '2', '-m', '2G'],
        'nic_args': ['-device', 'e1000e,netdev=net0'],
        'netdev_extra': '',
        'console_index': 0,
        'kernel_env': 'QEMU_TEST_VXWORKS_X86_64_Q35_KERNEL',
        'dtb_env': None,
        'net_setup_cmd': None,
    },
}

# SLIRP default gateway address (hosts the built-in TFTP server)
SLIRP_GATEWAY = '10.0.2.2'

# Name of the test file placed in the TFTP directory
TFTP_TEST_FILENAME = 'qemu_net_test.txt'

# Content written to the test file; VxWorks will fetch this via TFTP
TFTP_TEST_CONTENT = 'QEMU_VXWORKS_NET_OK'


class VxWorksUsernet(QemuSystemTest):
    """
    Test user-mode networking on VxWorks across multiple architectures.

    For each machine configuration, this test:
      1. Creates a temporary directory with a known test file.
      2. Launches QEMU with SLIRP networking and the built-in TFTP server
         pointing at that directory.
      3. Boots VxWorks and waits for the shell prompt.
      4. Optionally runs a network setup command.
      5. Executes a TFTP get to transfer the file into the VxWorks guest.
      6. Verifies the transfer succeeded by checking console output.
    """

    timeout = 120

    def _run_usernet_test(self, config_name):
        """Run the TFTP transfer test for the given machine configuration."""

        cfg = MACHINE_CONFIGS[config_name]

        # --- Resolve kernel path from environment ---
        kernel = os.environ.get(cfg['kernel_env'], '')
        if not kernel or not os.path.exists(kernel):
            self.skipTest(
                f"VxWorks kernel not found. Set {cfg['kernel_env']} to the "
                f"path of the VxWorks image for {config_name}."
            )

        # --- Resolve optional DTB ---
        dtb = None
        if cfg['dtb_env']:
            dtb = os.environ.get(cfg['dtb_env'], '')
            if dtb and not os.path.exists(dtb):
                dtb = None  # ignore if set but not present

        # --- Create TFTP directory and test file ---
        tftp_dir = tempfile.mkdtemp(prefix='qemu-vxworks-tftp-')
        test_file_path = os.path.join(tftp_dir, TFTP_TEST_FILENAME)
        with open(test_file_path, 'w') as f:
            f.write(TFTP_TEST_CONTENT)

        try:
            self._launch_and_test(cfg, kernel, dtb, tftp_dir)
        finally:
            # Cleanup temp files
            if os.path.exists(test_file_path):
                os.unlink(test_file_path)
            if os.path.exists(tftp_dir):
                os.rmdir(tftp_dir)

    def _launch_and_test(self, cfg, kernel, dtb, tftp_dir):
        """Configure QEMU, boot VxWorks, and perform the TFTP test."""

        # --- Determine shell type ---
        shell_type = os.environ.get('QEMU_TEST_VXWORKS_SHELL_TYPE', 'cmd')
        if shell_type not in SHELL_CONFIGS:
            self.fail(
                f"Unknown shell type '{shell_type}'. "
                f"Set QEMU_TEST_VXWORKS_SHELL_TYPE to 'c' or 'cmd'."
            )
        shell_cfg = SHELL_CONFIGS[shell_type]

        self.set_machine(cfg['machine'])
        self.vm.set_console(console_index=cfg['console_index'])

        # Base QEMU args
        self.vm.add_args('-kernel', kernel)

        if dtb:
            self.vm.add_args('-dtb', dtb)

        for arg in cfg['extra_args']:
            self.vm.add_args(arg)

        # Network: user-mode netdev with built-in TFTP server
        if cfg['nic_args']:
            # Machines where we attach an explicit NIC device
            netdev_opts = (
                f"user,id=net0,tftp={tftp_dir}{cfg['netdev_extra']}"
            )
            self.vm.add_args('-netdev', netdev_opts)
            for arg in cfg['nic_args']:
                self.vm.add_args(arg)
        else:
            # Machines with a built-in NIC: use -nic to combine netdev+device
            nic_opts = (
                f"user,id=gem0,tftp={tftp_dir}{cfg['netdev_extra']}"
            )
            self.vm.add_args('-nic', nic_opts)

        # Launch
        self.vm.launch()

        # VxWorks boots into the C shell first
        wait_for_console_pattern(self, '->')

        # Format the RAM disk so TFTP has somewhere to write
        exec_command_and_wait_for_pattern(
            self, 'hrfsFormat "/ram0"', 'value = 0')

        # Wait for the C shell prompt to return after hrfsFormat
        wait_for_console_pattern(self, '->')

        # Switch to the Cmd shell
        exec_command_and_wait_for_pattern(self, 'cmd', shell_cfg['prompt'])

        # Optional network setup command
        net_setup = os.environ.get('QEMU_TEST_VXWORKS_NET_SETUP_CMD',
                                   cfg['net_setup_cmd'])
        if net_setup:
            exec_command_and_wait_for_pattern(self, net_setup,
                                             shell_cfg['prompt'])

        # Perform TFTP transfer
        tftp_cmd_fmt = os.environ.get('QEMU_TEST_VXWORKS_TFTP_CMD',
                                      shell_cfg['tftp_cmd_fmt'])
        tftp_cmd = tftp_cmd_fmt.format(
            host=SLIRP_GATEWAY,
            file=TFTP_TEST_FILENAME,
        )

        tftp_success = os.environ.get('QEMU_TEST_VXWORKS_TFTP_SUCCESS',
                                      shell_cfg['tftp_success'])

        exec_command_and_wait_for_pattern(
            self, tftp_cmd, tftp_success,
            failure_message='error'
        )

    # ------------------------------------------------------------------
    # One test method per machine configuration
    # ------------------------------------------------------------------

    def test_riscv64_virt(self):
        """Test user-mode networking on RISC-V 64 virt machine."""
        self._run_usernet_test('riscv64_virt')

    def test_riscv64_sifive_u(self):
        """Test user-mode networking on RISC-V 64 SiFive U machine."""
        self._run_usernet_test('riscv64_sifive_u')

    def test_aarch64_amd_versal(self):
        """Test user-mode networking on AArch64 AMD (formely Xilinx) Versal machine."""
        self._run_usernet_test('aarch64_amd_versal')

    def test_aarch64_amd_versal2(self):
        """Test user-mode networking on AArch64 AMD Versal Gen 2 machine."""
        self._run_usernet_test('aarch64_amd_versal2')

    def test_aarch64_xlnx_zcu102(self):
        """Test user-mode networking on AArch64 Xilinx ZCU102 machine."""
        self._run_usernet_test('aarch64_xlnx_zcu102')

    def test_aarch64_nxp_s32g(self):
        """Test user-mode networking on AArch64 NXP S32G machine."""
        self._run_usernet_test('aarch64_nxp_s32g')

    def test_aarch64_nxp_imx8mp(self):
        """Test user-mode networking on AArch64 NXP i.MX8MP EVK machine."""
        self._run_usernet_test('aarch64_nxp_imx8mp')

    def test_arm_imx6_sabrelite(self):
        """Test user-mode networking on ARM i.MX6 SABRE Lite machine."""
        self._run_usernet_test('arm_imx6_sabrelite')

    def test_x86_64_q35(self):
        """Test user-mode networking on x86-64 Q35 machine."""
        self._run_usernet_test('x86_64_q35')


if __name__ == '__main__':
    QemuSystemTest.main()
