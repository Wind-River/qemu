#include "qemu/osdep.h"
#include "hw/pci/pci_device.h"

#define TYPE_MY_DEVICE "wr-pci-leds"
OBJECT_DECLARE_SIMPLE_TYPE(MyDeviceState, MY_DEVICE)

typedef struct MyDeviceState {
    PCIDevice parent_obj;
    MemoryRegion bar;

    uint64_t led0,led1;
} MyDeviceState;
static void my_device_write(void *opaque, hwaddr addr, uint64_t data, unsigned size)
{
    printf(">>\t<QEMU> Write to %lu with %lu %u\n",addr, data, size);
    MyDeviceState* s = opaque;
    switch (addr) {
        case 0: s->led0=data; break;
        case 4: s->led1=data; break;
        default: return;
    }
}
static uint64_t my_device_read(void *opaque, hwaddr addr, unsigned size)
{
    printf(">>\t<QEMU> Read from %lu for %u\n",addr, size);
    MyDeviceState* s = opaque;
    switch (addr) {
        case 0: return s->led0;
        case 4: return s->led1;
        default: return ~0;
    }
}
static const MemoryRegionOps my_device_ops = {
        .write = my_device_write,
        .read = my_device_read,
};
static void pci_my_device_realize(PCIDevice *dev, Error **errp)
{
    printf(">>\t<QEMU> Hello this is my-device PCI device\n");
    MyDeviceState *s = MY_DEVICE(dev);
    memory_region_init_io(&s->bar, OBJECT(s), &my_device_ops, s, "my-device", 32);
    pci_register_bar(dev, 0 , PCI_BASE_ADDRESS_SPACE_MEMORY, &s->bar);

}
static void my_device_class_init(ObjectClass *klass, void *data)
{
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->vendor_id = 0xabcd;
    k->device_id = 0x0525;
    k->realize = pci_my_device_realize;
    k->class_id = PCI_CLASS_OTHERS;
}
static const TypeInfo my_device_info = {
    .name = TYPE_MY_DEVICE,
    .parent = TYPE_PCI_DEVICE,
    .instance_size = sizeof(MyDeviceState),
    .class_init = my_device_class_init,
    .interfaces = (InterfaceInfo[]) {
            { INTERFACE_CONVENTIONAL_PCI_DEVICE },
            { },
    },
};
static void my_device_register_types(void)
{
    type_register_static(&my_device_info);
}
type_init(my_device_register_types)
