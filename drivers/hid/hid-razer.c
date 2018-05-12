/*
 * Copyright (C) 2015 Razer Inc.  All Right Reserved
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>

#include "hid-ids.h"

#define HIDRZR_STR_VERSION "1.0"
#define HIDRZR_NAME "HidRazer"
#define HIDRZR_LOG(fmt, args...) printk("hidrzr::%s "fmt, __func__, args)
#define HIDRZR_MSG(fmt) printk("hidrzr::%s "fmt, __func__)

#define CORTEX_BUTTON 0x40
#define CORTEX_LOCATION 6

static const struct hid_device_id private_rzr_devices[] = {
  { HID_USB_DEVICE(USB_VENDOR_ID_RAZER,  USB_DEVICE_ID_RAZER_SERVAL) },	
};
MODULE_DEVICE_TABLE(hid, private_rzr_devices);

typedef struct {
  const struct hid_device_id *pId;
  bool bMode;
  unsigned long ulTime;
} RazerInfo;

static int private_probe(struct hid_device *pHdev, 
			 const struct hid_device_id *pId)
{
  RazerInfo *pInfo = NULL;
  int rvalue = 0;

  pInfo = kzalloc(sizeof(RazerInfo), GFP_KERNEL);
  if (NULL == pInfo) {
    rvalue = -ENOMEM; 
    goto error_out;
  }

  pInfo->pId = pId;
  pInfo->bMode = false;

  /* Were ignoring the cortex button for the first second. */
  pInfo->ulTime = jiffies_to_msecs(jiffies);

  hid_set_drvdata(pHdev, pInfo);
  rvalue = hid_parse(pHdev);
  if (rvalue) {
    goto error_out;
  }

  rvalue = hid_hw_start(pHdev, HID_CONNECT_DEFAULT |
			HID_CONNECT_HIDDEV_FORCE);
  if (rvalue) {
    goto error_out;
  }

 error_out:
  if (0 != rvalue) {
    if (NULL != pInfo) {
      kfree(pInfo);
    }
  }
  return rvalue;
}

static void private_remove(struct hid_device *pHdev)
{
  hid_hw_stop(pHdev);
  kfree(hid_get_drvdata(pHdev));
}

static int private_raw_event(struct hid_device *pHdev, 
			     struct hid_report *pReport,
			     __u8 *pRd,
			     int uiSize)
{
  RazerInfo *pInfo;
  unsigned long delta;

  pInfo = hid_get_drvdata(pHdev);
  if (NULL != pInfo) {
    if (USB_DEVICE_ID_RAZER_SERVAL == pInfo->pId->product) {
      if (false == pInfo->bMode) {
	delta = jiffies_to_msecs(jiffies) - pInfo->ulTime;
	if (delta <= 1000) {
	  if (0 != (CORTEX_BUTTON & *(pRd + CORTEX_LOCATION))) {
	    *(pRd + CORTEX_LOCATION) &= ~CORTEX_BUTTON;
	  }

	  if (0 != *(pRd + CORTEX_LOCATION)) {
	    pInfo->bMode = true;
	  }
	} else {
	  /* Its after one second, let it happen */
	  pInfo->bMode = true;
	}
      }
    }
  }

  return 0;
}

static struct hid_driver private_rzr_driver = {
	.name = "RazerHid",
	.id_table = private_rzr_devices,
	.probe = private_probe,
	.remove = private_remove,
	.raw_event = private_raw_event
};
module_hid_driver(private_rzr_driver);

MODULE_LICENSE("GPL");

/* Module Information for modinfo */
MODULE_VERSION(HIDRZR_STR_VERSION);
MODULE_ALIAS(HIDRZR_NAME);

/* Author Information */
MODULE_AUTHOR("Stuart Wells <stuart.wells@razerzone.com>");
MODULE_DESCRIPTION("Razer HID Driver");
