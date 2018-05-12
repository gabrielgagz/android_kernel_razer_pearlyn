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
#include <linux/uaccess.h>
#include <linux/leds.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <linux/fs.h>
#include <linux/seq_file.h>

#define LEDDBG_STR_VERSION "1.0"
#define LEDDBG_MODULE "Razer LED device Mode"
#define LEDDBG_DRIVER_NAME "rzr,led_dbg_id"
#define LEDDBG_DEFAULT "none"
#define LEDDBG_PROC_NAME "led_device"

typedef struct {
  struct proc_dir_entry *pDBGEntry;
  long value;
  spinlock_t sLock;
  struct led_classdev *pDev;
} PearlynData;

static PearlynData privateData = {
  .pDBGEntry = NULL,
  .pDev = NULL,
  .value = 0,
};

static int privProcDBGOutput(struct seq_file *pSeq,
			     void *pBuffer)
{
  PearlynData *pData = NULL;

  if (NULL != pSeq) {
    pData = (PearlynData *)pSeq->private;

    if (NULL != pData) {
      spin_lock(&pData->sLock);
      seq_printf(pSeq, "%ld\n", pData->value);
      spin_unlock(&pData->sLock);
    } else {
      seq_printf(pSeq, "error\n");
    }
  }

  return 0;
}

static ssize_t privProcDBGInputSeq(struct file *pFile,
				   const char __user *pBuffer,
				   size_t iCount,
				   loff_t *pPos)
{
  PearlynData *pData = NULL;
  char tmpbuff [64];
  size_t rvalue = iCount;
  int ilength;
  long tmplong;

  if (NULL != pFile) {
    if (NULL != pBuffer) {
      if (iCount > 0) {
	pData = (PearlynData *)&privateData;

	if (NULL != pData) {
	  ilength = rvalue;
	  if (ilength >= sizeof(tmpbuff)) {
	    ilength = sizeof(tmpbuff);
	  }
	  memset(tmpbuff, 0, sizeof(tmpbuff));
	  if (0 == copy_from_user(tmpbuff, pBuffer, ilength)) {
	    if (0 == strict_strtol(tmpbuff, 10, &tmplong)) {
	      spin_lock(&pData->sLock);
	      if ((tmplong > 0) && (tmplong <= 255)) {
		pData->value = tmplong;
	      }
	      spin_unlock(&pData->sLock);

	      if (NULL != pData->pDev) {
		pData->pDev->brightness = pData->value;
		if (!(pData->pDev->flags & LED_SUSPENDED)) {
		  pData->pDev->brightness_set(pData->pDev, pData->value);
		}
	      }
	    }
	  }
	}
      }
    }
  }

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
void LEDDBGProcInit(void *pPtr)
{
  PearlynData *pData = (PearlynData *)pPtr;

  if (NULL != pData) {
    spin_lock_init(&pData->sLock);
    pData->pDBGEntry = proc_create_data(LEDDBG_PROC_NAME,
					0666,
					NULL,
					&private_proc_dbg_fops,
					pPtr);
  }
}

void LEDDBGProcDeinit(void *pPtr)
{
  PearlynData *pData = (PearlynData *)pPtr;;

  if (NULL != pData) {
    if (NULL != pData->pDBGEntry) {
      /* Don't remove it, if we didn't have it. */
      remove_proc_entry(LEDDBG_PROC_NAME, NULL);
      pData->pDBGEntry = NULL;
    }

  }
}

void razer_led_setdev(struct led_classdev *pDev)
{
  PearlynData *pData = (PearlynData *)&privateData;

  pData->pDev = pDev;
}
EXPORT_SYMBOL(razer_led_setdev);

/*
  The linux kernel module insmod entry point.
*/
static int __init privLedInit(void)
{
  /*
   * This is where the driver will call functions to install
   * its functions.  Keep this area simple.
   */
  /* Clear our ugle global data */
  privateData.value = 0;

  LEDDBGProcInit(&privateData);

  return 0;
}

/*
   Inform the linux kernel of the entry point.
*/
module_init(privLedInit);

/*
  The linux kernel module insmod exit point.
*/
static void __exit privLedExit(void)
{
  LEDDBGProcDeinit(&privateData);
}

/*
   Inform the linux kernel of the exit point.
*/
module_exit(privLedExit);

MODULE_LICENSE("GPL");

/* Module Information for modinfo */
MODULE_VERSION(LEDDBG_STR_VERSION);
MODULE_ALIAS(LEDDBG_MODULE);

/* Author Information */
MODULE_AUTHOR("Stuart Wells <stuart.wells@razerzone.com>");
MODULE_DESCRIPTION("Razer Driver for LED");
