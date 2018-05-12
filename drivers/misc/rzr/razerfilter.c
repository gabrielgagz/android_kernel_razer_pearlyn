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
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/version.h>

#include <linux/fs.h>
#include <linux/seq_file.h>

#define RZRFILTER_STR_VERSION "1.0"
#define RZRFILTER_MODULE "Razer Controller Filtering Mode"
#define RZRFILTER_PROC_NAME "rzr_filter"

#define ASUS_VENDOR 0x0b05
#define ASUS_PRODUCT 0x4500
#define ASUS_NAME "ASUS Gamepad"
const char *pAsusName = ASUS_NAME;

#define RAZER_VENDOR 0x1532
#define RAZER_SERVAL 0x0900

typedef struct {
  struct proc_dir_entry *pDBGEntry;
  int value;
  spinlock_t sLock;
} PearlynData;

static PearlynData privateData = {
  .pDBGEntry = NULL,
  .value = 1,
};

static int privProcFilterOutput(struct seq_file *pSeq,
			     void *pBuffer)
{
  PearlynData *pData = NULL;

  if (NULL != pSeq) {
    pData = (PearlynData *)pSeq->private;

    if (NULL != pData) {
      spin_lock(&pData->sLock);
      seq_printf(pSeq, "%d\n", pData->value);
      spin_unlock(&pData->sLock);
    } else {
      seq_printf(pSeq, "error\n");
    }
  }

  return 0;
}

static ssize_t privProcFilterInputSeq(struct file *pFile,
				      const char __user *pBuffer,
				      size_t iCount,
				   loff_t *pPos)
{
  PearlynData *pData = NULL;
  char tmpbuff [2];
  size_t rvalue = iCount;
  int newval;
  int ilength;

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
	    if ('0' == tmpbuff [0]) {
	      newval = 0;
	    } else {
	      newval = 1;
	    }
	    
	    spin_lock(&pData->sLock);
	    pData->value = newval;
	    spin_unlock(&pData->sLock);
	  }
	}
      }
    }
  }

  return rvalue;
}

static int privateProcFilterOpen(struct inode *pINode, struct file *pFile)
{
  return single_open(pFile, privProcFilterOutput, PDE_DATA(pINode));
}

static const struct file_operations private_proc_dbg_fops = {
  .owner   = THIS_MODULE,
  .open    = privateProcFilterOpen,
  .read    = seq_read,
  .write   = privProcFilterInputSeq,
  .llseek  = seq_lseek,
  .release = single_release,
};

/* Proc entry point */
void RZRFILTERProcInit(void *pPtr)
{
  PearlynData *pData = (PearlynData *)pPtr;

  if (NULL != pData) {
    spin_lock_init(&pData->sLock);
    pData->pDBGEntry = proc_create_data(RZRFILTER_PROC_NAME,
					0666,
					NULL,
					&private_proc_dbg_fops,
					pPtr);
  }
}

void RZRFILTERProcDeinit(void *pPtr)
{
  PearlynData *pData = (PearlynData *)pPtr;;

  if (NULL != pData) {
    if (NULL != pData->pDBGEntry) {
      /* Don't remove it, if we didn't have it. */
      remove_proc_entry(RZRFILTER_PROC_NAME, NULL);
      pData->pDBGEntry = NULL;
    }

  }
}

void razer_filter_controller(struct input_dev *pDev)
{
  PearlynData *pData = (PearlynData *)&privateData;

  if (1 == pData->value) {
    if (RAZER_VENDOR == pDev->id.vendor) {
      if (RAZER_SERVAL == pDev->id.product) {
	pDev->id.vendor = ASUS_VENDOR;
	pDev->id.product = ASUS_PRODUCT;
	pDev->name = pAsusName;
      }
    }
  }
}
EXPORT_SYMBOL(razer_filter_controller);

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
  privateData.value = 1;

  RZRFILTERProcInit(&privateData);

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
  RZRFILTERProcDeinit(&privateData);
}

/*
   Inform the linux kernel of the exit point.
*/
module_exit(privLedExit);

MODULE_LICENSE("GPL");

/* Module Information for modinfo */
MODULE_VERSION(RZRFILTER_STR_VERSION);
MODULE_ALIAS(RZRFILTER_MODULE);

/* Author Information */
MODULE_AUTHOR("Stuart Wells <stuart.wells@razerzone.com>");
MODULE_DESCRIPTION("Razer Driver for LED");
