================================
Callstack TCG plugin for VxWorks
================================

This document explains usage and additional details of
``contrib/plugins/callstack.c`` plugin. The purpose of this plugin
is to maintain shadow callstacks for each process/thread running in
a VxWorks guest.

The work serves as a demonstration of the power of the TCG plugin
API and its ability to be leveraged for OS introspection.
It is able to see into the data structures within
the operating system and extract dynamic state allowing for
non-intrusive runtime analysis.
In the case of the callstack plugin, we extract thread
state from the operating system data structures by reading
system memory. This allows us to track function calls
and returns on a finer grained per-thread basis rather than just
per-process.
There are other potential uses like process lifecycle tracking and
system call tracing that can be implemented using OS introspection.

This information we can gather via OS introspection allows us to
perform deeper and more meaningful analysis of a running system
without source modification. Maintaining a shadow
call stack is just one example of how to leverage these capabilities.

The inspiration for this work comes from the PANDA open source
project, the Platform for Architecture-Neutral Dynamic Analysis
`<https://panda.re/>`_.
PANDA is built on a modified version of QEMU and implements an API to
hook callbacks into various points of system execution. It also
supports operating system introspection through a set of configuration
files that describe properties and layout of different guest operating
systems. This gives us the information we need to reach into the guest
memory at specific offsets, thus giving the plugin the ability to
parse and extract meaningful information from the guest at runtime.

The novelty of this plugin is in the demonstration of the QEMU TCG
plugin interface and how it has evolved into a powerful API to enable
system introspection and analysis activities that previously were
solely in the realm of specialized downstream tools and frameworks.

With the addition of the register reading API combined with the guest
memory reading API we are able to achieve functionality similar to what
some of the downstream system analysis tools like PANDA are able to
achieve.

Additionally, we demonstrate this capability with a VxWorks guest
operating system, which other open source projects have not previously
done.

Building the plugin
-------------------

See ``docs/about/emulation.rst`` for general plugin build and usage
instructions.


Usage
-----

The callstack plugin currently only supports aarch64 system emulation.
Basic usage is as follows::

  $ qemu-system-aarch64 $(QEMU_ARGS) \
    -plugin contrib/plugins/libcallstack.so \
    -d plugin

With no additional options specified, the plugin will maintain a shadow
callstack per-process as identified by the translation table base
address, and the plugin will output all callstacks in their current
state at plugin exit.

.. list-table:: Callstack Plugin arguments
  :widths: 30 70
  :header-rows: 1

  * - Option
    - Description
  * - stack=[vxworks|heuristic]
    - By default the plugin tracks callstack per-process. This option allows
      you to track callstacks by task or thread. In the case of stack=vxworks
      we use OS introspection to determine which callstack a function call or
      return belongs to. In the case of stack=heuristic we guess based on the
      current stack pointer.
  * - pc=<addr>
    - Specify a point in execution where you want to observe the callstack of
      the current thread or process. Whenever the current Program Counter
      matches the provided <addr> the current callstack will be output.
  * - kernel_elf=PATH
    - The path to the OS ELF file. Symbol information will be extracted from
      here. When specifying stack=vxworks, kernel_elf must point to a vxWorks
      elf image.
  * - user_elf=PATH
    - The path to an application elf file. Symbol information will be extracted
      from here to resolve addresses in callstacks to function symbols.

Example::

  $ qemu-system-aarch64 $(QEMU_ARGS) \
    -plugin contrib/plugins/libcallstack.so,kernel_elf=$(WIND_HOME)/vsb/vip/default/vxworks,user_elf=$(APPS)/demo.vxe,stack=vxworks,pc=0xffffffff803f3568 \
    -d plugin

Example output::

  PC 0xffffffff803f3568
  TTBR 0x1897000 TCB 0xffff800000023ac0 callstack depth: 13
    #0  0xffffffff80226d24 in windLoadContext
    #1  0xffffffff80103434 in ipcomNetTask
    #2  0xffffffff8046e524 in jobQueueProcess
    #3  0xffffffff8012cbb4 in ipcom_singleton_job_action
    #4  0xffffffff80135370 in ipnet_process_pending_timeouts
    #5  0xffffffff801406b4 in ipnet_dst_cache_delete_tmo
    #6  0xffffffff80140884 in ipnet_dst_cache_delete
    #7  0xffffffff80199e0c in ipnet_peer_info_release
    #8  0xffffffff801998a0 in ipnet_peer_info_free
    #9  0xffffffff8012dfe0 in ipcom_slab_free
    #10 0xffffffff8012de08 in ipcom_slab_free_nogc
    #11 0xffffffff8012c2bc in ipcom_spinlock_lock
    #12 0xffffffff803f3568 in semTake

  PC 0xffffffff803f3568
  TTBR 0x1897000 TCB 0xffff800000037010 callstack depth: 3
    #0  0xffffffff80226d24 in windLoadContext
    #1  0xffffffff803c4b48 in miiBusMonitorTask
    #2  0xffffffff803f3568 in semTake

  PC 0xffffffff803f3568
  TTBR 0x1897000 TCB 0xffff800000037010 callstack depth: 6
    #0  0xffffffff80226d24 in windLoadContext
    #1  0xffffffff803c4b48 in miiBusMonitorTask
    #2  0xffffffff803c53c8 in miiBusRead
    #3  0xffffffff803c74a8 in MII_READ
    #4  0xffffffff80297594 in zynqGemPhyRead
    #5  0xffffffff803f3568 in semTake
