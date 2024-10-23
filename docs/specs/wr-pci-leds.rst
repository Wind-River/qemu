=================
Wind River PCI LEDs example device
=================
-------------------------
Sharing how to create a barebone QEMU custom device
-------------------------

Standalone run
============
Run
  ::

    ./qemu-system-x86_64 -device wr-pci-leds
    >>      <QEMU> Hello this is my-device PCI device
  
Linux run
============
Integrate the example device driver into Kernel image, and during the boot
  ::
  
    >>      <DRIVER> Hello, this is your leds QEMU driver
    >>      <DRIVER> Found device vid: 0xABCD pid: 0x525

On command shell,
  ::

    # echo 11 > /sys/class/leds/qemu:led0/brightness 
    >>      <QEMU> Write to 0 with 11 4
    >>      <QEMU> Read from 0 for 4
    >>      <DRIVER> leds_qemu regs is 11
  
Linux, drivers/leds/leds-qemu.c
============
.. code:: C
  :linenos:
  
  #include <linux/module.h>
  #include <linux/pci.h>
  #include <linux/leds.h>
  
  #define DRV_NAME "leds_qemu"
  #define PCI_VENDOR_ID_QEMU 0xabcd
  #define PCI_DEVICE_ID_QEMU 0x0525
  
  static const struct pci_device_id pci_table[] = {
          { PCI_DEVICE(PCI_VENDOR_ID_QEMU, PCI_DEVICE_ID_QEMU), },
          { 0, },
  };
  MODULE_DEVICE_TABLE(pci, pci_table);
  
  struct leds_qemu_priv {
          uint32_t* regs;
  	struct led_classdev led0_qemu_cdev;
  };
  
  static int leds_qemu_set(struct led_classdev *led_cdev, enum led_brightness value)
  {
  	struct leds_qemu_priv* s= container_of(led_cdev, struct leds_qemu_priv, led0_qemu_cdev);
  	writel((u8)value, s->regs);
  	int v = readl(s->regs);
  	printk(KERN_INFO ">>\t<DRIVER> leds_qemu regs is %d\n",v);
  	return 0;
  }
  
  static int leds_qemu_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
  {
          int rc = 0;
          printk(KERN_INFO ">>\t<DRIVER> Hello, this is your leds QEMU driver\n");
  
          struct leds_qemu_priv* priv = devm_kzalloc(&pdev->dev,sizeof(struct leds_qemu_priv), GFP_KERNEL);
          pci_set_drvdata(pdev, priv);
  
          uint16_t vendor, device;
          pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor);
          pci_read_config_word(pdev, PCI_DEVICE_ID, &device);
          printk(KERN_INFO ">>\t<DRIVER> Found device vid: 0x%X pid: 0x%X\n", vendor, device);
  
          rc = pci_enable_device(pdev);
          if (rc) return rc;
          rc = pci_request_regions(pdev,DRV_NAME);
          if (rc) {
          	pci_disable_device(pdev);
  		return rc;
  	}
  
          resource_size_t pciaddr = pci_resource_start(pdev,0);
          uint32_t* regs = ioremap(pciaddr,32);
          priv->regs = regs;
          priv->led0_qemu_cdev.name="qemu:led0";
          priv->led0_qemu_cdev.max_brightness=255;
          priv->led0_qemu_cdev.brightness_set_blocking=leds_qemu_set;
          rc = led_classdev_register(&pdev->dev, &priv->led0_qemu_cdev);
  
          return 0;
  }
  static void leds_qemu_remove (struct pci_dev *pdev)
  {
  }
  
  static struct pci_driver leds_qemu_driver = {
          .name = DRV_NAME,
          .id_table = pci_table,
          .probe = leds_qemu_probe,
          .remove = leds_qemu_remove,
  };

  module_pci_driver(leds_qemu_driver);
  MODULE_LICENSE("GPL");


Linux, drivers/leds/KConfig
=================
.. code::

  config LEDS_QEMU
        tristate "LED support for my-device in QEMU"
        depends on LEDS_CLASS
        depends on PCI


Linux, drivers/leds/Makefile
=================
.. code::

    obj-$(CONFIG_LEDS_QEMU)                 += leds-qemu.o
