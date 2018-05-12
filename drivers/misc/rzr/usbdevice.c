/*
 * Copyright (C) 2014 Razer Inc.  All Right Reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 *
 * Razer provides this code “as is” and makes no warranties or give any
 * representation of its effectiveness, quality, fitness for any purpose,
 * satisfactory quality or that it is free from any defect or error.
 *
 * Razer shall in no event be liable for any lost profits, loss of information
 * or data, special, incidental, indirect, punitive or consequential or
 * incidental damages, arising in any way out of distribution of, sale of,
 * resale of, use of, or inability to use the code.
 */
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/of.h>

#include <linux/proc_fs.h>
#include <linux/version.h>

#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#define USBDBG_STR_VERSION "1.0"
#define USBDBG_MODULE "Razer USB device Mode"
#define USBDBG_DRIVER_NAME "rzr,usb_dbg_id"
#define USBDBG_GPIO_NAME "rzr,id-gpio"
#define USBDBG_GPIO_POWER_NAME "rzr,pw-gpio"
#define USBDBG_DEFAULT "none"
#define USBDBG_PROC_NAME "usb_device"

#define USBDBG_NO_DEFAULT -1

typedef struct {
  struct proc_dir_entry *pDBGEntry;
  int gpio;
  int gpio_power;
  int default_debug;
  spinlock_t sLock;
} PearlynData;

static char privTmpBuffer [4096];

static PearlynData privateData;

static int privProcDBGOutput(struct seq_file *pSeq,
			     void *pBuffer)
{
  PearlynData *pData = NULL;
  int rc;

  if (NULL != pSeq) {
    pData = (PearlynData *)pSeq->private;

    if (NULL != pData) {
      if ((0 != pData->gpio) && (0 != pData->gpio_power)) {
	rc = gpio_get_value(pData->gpio);
	seq_printf(pSeq, "%d\n", rc);
      } else {
	seq_printf(pSeq, "error\n");
      }
    }
  }

  return 0;
}

static void privProcSetHostMode(PearlynData * const pData, unsigned char hostMode) {

     spin_lock(&pData->sLock);
      gpio_set_value(pData->gpio, hostMode);
      pData->default_debug = hostMode;
      gpio_set_value(pData->gpio_power, 0); 
      printk("%s: Setting debug value %s\n", __FUNCTION__, "DEVICE MODE");
    spin_unlock(&pData->sLock);
     msleep(25);
       spin_lock(&pData->sLock);
        gpio_set_value(pData->gpio_power, hostMode); 
         spin_unlock(&pData->sLock);

}
static ssize_t privProcDBGInputSeq(struct file *pFile,
				   const char __user *pBuffer,
				   size_t iCount,
				   loff_t *pPos)
{
  PearlynData *pData = NULL;
  size_t rvalue = iCount;
  int loop = 0;
  bool berror = false;
  unsigned char hostMode = 0;
  bool bset = false;
  mm_segment_t cur_fs;
  int icSize;
  

  /* We could be called from user space */
  cur_fs = get_fs();

  set_fs(KERNEL_DS);

  icSize = iCount;
  if (icSize >= 4096) {
    icSize = 4096;
  }


  if (NULL != pFile) {
    if (NULL != pBuffer) {
      if ((!copy_from_user(privTmpBuffer, pBuffer, icSize)) && (iCount > 0)) {
	pData = (PearlynData *)&privateData;

	while ((loop < strlen(pBuffer)) && (false == bset) && (false == berror)) {
	  switch (privTmpBuffer [loop])
	    {
	    case ' ':
	    case '\r':
	    case '\n':
	      break;
	    case '0':
	      hostMode = 0;
	      bset = true;
	      break;
	    case '1':
	      hostMode = 1;
	      bset = true;
	      break;
	    default:
	      berror = true;
	      break;
	    }
	  loop++;
	}
	if ((false == berror) && (true == bset)) {
	  /* set device mode, let the gpio know */
     privProcSetHostMode(pData, hostMode); 
    
     /* Don't allow the system to draw power in device mode */
        msleep(15);
        spin_lock(&pData->sLock);
          gpio_set_value(pData->gpio_power, hostMode);
        spin_unlock(&pData->sLock); 

   
        }
      }
    }
  }

  set_fs(cur_fs);

  return rvalue;
}

static int privateProcDBGOpen(struct inode *pINode, struct file *pFile)
{
  return single_open(pFile, privProcDBGOutput, PDE_DATA(pINode));
}

static const struct file_operations private_proc_dbg_fops = {
  .owner   = THIS_MODULE,
  .open    = privateProcDBGOpen,
  .read    = seq_read,
  .write   = privProcDBGInputSeq,
  .llseek  = seq_lseek,
  .release = single_release,
};

/* Proc entry point */
void USBDBGProcInit(void *pPtr)
{
  PearlynData *pData = (PearlynData *)pPtr;

  if (NULL != pData) {
    pData->pDBGEntry = proc_create_data(USBDBG_PROC_NAME,
					0666,
					NULL,
					&private_proc_dbg_fops,
					pPtr);
  }
}

void USBDBGProcDeinit(void *pPtr)
{
  PearlynData *pData = (PearlynData *)pPtr;;

  if (NULL != pData) {
    if (NULL != pData->pDBGEntry) {
      /* Don't remove it, if we didn't have it. */
      remove_proc_entry(USBDBG_PROC_NAME, NULL);
      pData->pDBGEntry = NULL;
    }

  }
}

static struct of_device_id private_usbdbg_of_match[] = {
  {.compatible = USBDBG_DRIVER_NAME,},
  {},
};

static int privUSBDbgProbe(struct platform_device *pDev)
{
  int ret;
  privateData.gpio = of_get_named_gpio(pDev->dev.of_node,
				       USBDBG_GPIO_NAME, 0);
  privateData.gpio_power = of_get_named_gpio(pDev->dev.of_node,
				       USBDBG_GPIO_POWER_NAME, 0);
  if (gpio_is_valid(privateData.gpio)) {
    ret = gpio_request(privateData.gpio,"usb_id");
    if (ret)
      pr_err("%s: unable to request id gpio %d\n",__func__,privateData.gpio);
    else
      gpio_direction_output(privateData.gpio,0);
  }
  spin_lock(&privateData.sLock);
  if (USBDBG_NO_DEFAULT != privateData.default_debug) {
    gpio_set_value(privateData.gpio, privateData.default_debug);
    msleep(25);
    gpio_set_value(privateData.gpio_power, 0);
    msleep(25);
    gpio_set_value(privateData.gpio_power, privateData.default_debug);
    msleep(25);
  }
  spin_unlock(&privateData.sLock);
  return 0;
}

static int privUSBDbgRemove(struct platform_device *pDev)
{
  privateData.gpio = 0;
  privateData.gpio_power = 0;

  return 0;
}

static struct platform_driver private_usbdbg_driver = {
  .probe = privUSBDbgProbe,
  .remove = privUSBDbgRemove,
  .driver = {
    .name = USBDBG_DRIVER_NAME,
    .owner = THIS_MODULE,
    .of_match_table = private_usbdbg_of_match,
  }
};

/*
  The linux kernel module insmod entry point.
*/
static int __init privUsbDebugInit(void)
{
  /*
   * This is where the driver will call functions to install
   * its functions.  Keep this area simple.
   */
  /* Clear our ugle global data */
  spin_lock_init(&privateData.sLock);
  privateData.gpio = 0;
  privateData.gpio_power = 0;
  privateData.default_debug = USBDBG_NO_DEFAULT;

  platform_driver_register(&private_usbdbg_driver);
  USBDBGProcInit(&privateData);

  return 0;
}

/*
   Inform the linux kernel of the entry point.
*/
module_init(privUsbDebugInit);

/*
  The linux kernel module insmod exit point.
*/
static void __exit privUsbDebugExit(void)
{
  platform_driver_unregister(&private_usbdbg_driver);
  USBDBGProcDeinit(&privateData);
}

/*
   Inform the linux kernel of the exit point.
*/
module_exit(privUsbDebugExit);

MODULE_LICENSE("GPL");

/* Module Information for modinfo */
MODULE_VERSION(USBDBG_STR_VERSION);
MODULE_ALIAS(USBDBG_MODULE);

/* Author Information */
MODULE_AUTHOR("Stuart Wells <stuart.wells@razerzone.com>");
MODULE_DESCRIPTION("Razer Driver for USB Device/Host Switch");
