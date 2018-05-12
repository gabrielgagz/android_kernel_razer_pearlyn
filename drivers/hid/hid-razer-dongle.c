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
#include "hid-ids.h"

#define HIDRD_STR_VERSION "1.0"
#define HIDRD_NAME "RzrDongleKeyboard"
#define HIDRD_LOG(fmt, args...) printk("hidrd::%s "fmt, __func__, args)
#define HIDRD_MSG(fmt) printk("hidrd::%s "fmt, __func__)

#define HIDRD_KEYBOARD (1 << 0)
#define HIDRD_MOUSE (1 << 1)

typedef struct {
  struct {
    spinlock_t sLock;
    struct hid_device *pHid;
    bool bActive;
  } keyboard;

  struct {
    spinlock_t sLock;
    struct hid_device *pHid;
    bool bActive;
  } mouse;
} RazerDongle;

static RazerDongle sGlobalDongle = {
  .keyboard.pHid = NULL,
  .keyboard.bActive = false,
  .keyboard.sLock = __SPIN_LOCK_INITIALIZER(sGlobalDongle.keyboard.sLock),
  .mouse.pHid = NULL,
  .mouse.bActive = false,
  .mouse.sLock = __SPIN_LOCK_INITIALIZER(sGlobalDongle.mouse.sLock),
};

static const struct hid_device_id private_razer_devices[] = {
  { HID_USB_DEVICE(USB_VENDOR_ID_RAZER, USB_DEVICE_ID_RAZER_DONGLE) },
  { }
};
MODULE_DEVICE_TABLE(hid, private_razer_devices);

static int private_controller_probe(struct hid_device *pHid,
				    const struct hid_device_id *pId)
{
  int rvalue = 0;
  struct usb_host_interface *pDesc = NULL;
  struct usb_interface *pFace = NULL;
  RazerDongle *pDongle = &sGlobalDongle;


  HIDRD_LOG("(%p) (%p)\n", pHid, pId);

  if (NULL != pHid) {
    pFace = to_usb_interface(pHid->dev.parent);

    if (NULL != pFace) {
      pDesc = pFace->cur_altsetting;
      HIDRD_LOG("interface %d\n", pDesc->desc.bInterfaceNumber);

      switch(pDesc->desc.bInterfaceNumber)
	{
	case 0:
	  spin_lock(&pDongle->keyboard.sLock);
	  HIDRD_MSG("Dongle Keyboard Interface\n");
	  if (NULL == pDongle->keyboard.pHid) {
	    pDongle->keyboard.bActive = false;
	    pDongle->keyboard.pHid = pHid;
	    hid_set_drvdata(pHid, (void *)HIDRD_KEYBOARD);
	  } else {
	    HIDRD_MSG("Dongle Keyboard already registered.\n");
	  }
	  spin_unlock(&pDongle->keyboard.sLock);
	  break;
	case 1:
	  spin_lock(&pDongle->mouse.sLock);
	  HIDRD_MSG("Dongle mouse Interface\n");
	  if (NULL == pDongle->mouse.pHid) {
	    pDongle->mouse.bActive = false;
	    pDongle->mouse.pHid = pHid;
	    hid_set_drvdata(pHid, (void *)HIDRD_MOUSE);
	  } else {
	    HIDRD_MSG("Dongle Keyboard already registered.\n");
	  }
	  spin_unlock(&pDongle->mouse.sLock);
	  break;
	default:
	  HIDRD_MSG("Dongle unknown Interface\n");
	  break;
	}
    }
  }

  return rvalue;
}

static void private_controller_remove(struct hid_device *pHid)
{
  unsigned long ulDevice = (unsigned long)hid_get_drvdata(pHid);
  RazerDongle *pDongle = &sGlobalDongle;
  bool bActive = false;

  switch(ulDevice)
    {
    case HIDRD_KEYBOARD:
      HIDRD_MSG("Dongle keyboard Disconnect\n");
      spin_lock(&pDongle->keyboard.sLock);

      if (true == pDongle->keyboard.bActive) {
	if (NULL != pDongle->keyboard.pHid) {
	  pDongle->keyboard.pHid = NULL;
	  bActive = true;
	}
	pDongle->keyboard.bActive = false;
      }

      spin_unlock(&pDongle->keyboard.sLock);
      break;
    case HIDRD_MOUSE:
      HIDRD_MSG("Dongle Mouse Disconnect\n");
      spin_lock(&pDongle->mouse.sLock);
      if (true == pDongle->mouse.bActive) {
	if (NULL != pDongle->mouse.pHid) {
	  pDongle->mouse.pHid = NULL;
	  bActive = true;
	}

	pDongle->mouse.bActive = false;
      }

      spin_unlock(&pDongle->mouse.sLock);
      break;
    default:
      HIDRD_MSG("Dongle unknown disconnect\n");
      break;
    }

  /* If we were active, do some work here */
  if (true == bActive) {
    hid_hw_stop(pHid);
  }
}

static struct hid_driver private_razer_controller = {
  .name = "razer",
  .id_table = private_razer_devices,
  .probe = private_controller_probe,
  .remove = private_controller_remove
};


module_hid_driver(private_razer_controller);

void hid_razer_keyboard_connect(void)
{
  RazerDongle *pDongle = &sGlobalDongle;
  int rvalue = 0;

  spin_lock(&pDongle->keyboard.sLock);

  if ((false == pDongle->keyboard.bActive) && (NULL != pDongle->keyboard.pHid)) {

    rvalue = hid_parse(pDongle->keyboard.pHid);

    if (0 != rvalue) {
      hid_err(pDongle->keyboard.pHid, "failed to parse map\n");
    } else {
      rvalue = hid_hw_start(pDongle->keyboard.pHid,
			    HID_CONNECT_DEFAULT | HID_CONNECT_HIDDEV_FORCE);
      if (0 != rvalue) {
	hid_err(pDongle->keyboard.pHid, "Hardware failed to start\n");
      } else {
	/* We have a connected keyboard. */
	pDongle->keyboard.bActive = true;
      }
    }
  } else {
    HIDRD_MSG("Dongle Keyboard already active!\n");
  }

  spin_unlock(&pDongle->keyboard.sLock);
}
EXPORT_SYMBOL(hid_razer_keyboard_connect);

static void private_disconnect_device(struct hid_device *pHid)
{
  unsigned int uiType, uiID;
  unsigned int loop;
  struct hid_report *pReport = NULL;
  struct hid_report_enum *pEnum = NULL;

  if (down_interruptible(&pHid->driver_lock)) {
    HIDRD_MSG("Dongle hid device lock error!\n");
    return;
  }

  if (down_interruptible(&pHid->driver_input_lock)) {
    HIDRD_MSG("Dongle hid driver lock error!\n");
    up(&pHid->driver_lock);
    return;
  }
  pHid->io_started = false;

  hid_hw_stop(pHid);

  for (uiType = 0; uiType < HID_REPORT_TYPES; uiType++) {
    pEnum = pHid->report_enum + uiType;

    if (NULL != pEnum) {
      for (uiID = 0; uiID < HID_MAX_IDS; uiID++) {
	pReport = pEnum->report_id_hash[uiID];

	if (NULL != pReport) {
	  for (loop = 0; loop < pReport->maxfield; loop++) {
	    if (NULL != pReport->field [loop]) {
	      kfree(pReport->field [loop]);
	    }
	  }
	  kfree(pReport);
	  pReport = NULL;
	}
      }
      memset(pEnum, 0, sizeof(struct hid_report_enum));
      INIT_LIST_HEAD(&pEnum->report_list);
    }

    kfree(pHid->rdesc);
    pHid->rdesc = NULL;
    pHid->rsize = 0;

    /* Do we have a collection */
    if (NULL != pHid->collection) {
      kfree(pHid->collection);
      pHid->collection = NULL;
    }
    pHid->collection_size = 0;
    pHid->maxcollection = 0;
    pHid->maxapplication = 0;
    pHid->status &= ~HID_STAT_PARSED;
  }

  if (!pHid->io_started) {
    up(&pHid->driver_input_lock);
  }

  up(&pHid->driver_lock);
}

void hid_razer_keyboard_disconnect(void)
{
  RazerDongle *pDongle = &sGlobalDongle;
  struct hid_device *pHid = NULL;
  bool bActive = false;

  spin_lock(&pDongle->keyboard.sLock);
  if ((true == pDongle->keyboard.bActive) && (NULL != pDongle->keyboard.pHid)) {
    pHid = pDongle->keyboard.pHid;
    pDongle->keyboard.pHid = NULL;
    bActive = true;
    pDongle->keyboard.bActive = false;
  }
  spin_unlock(&pDongle->keyboard.sLock);

  if (true == bActive) {
    private_disconnect_device(pHid);
  }
}

EXPORT_SYMBOL(hid_razer_keyboard_disconnect);

bool hid_razer_keyboard_active(void)
{
  RazerDongle *pDongle = &sGlobalDongle;
  bool rvalue = false;

  if (NULL != pDongle->keyboard.pHid) {
    rvalue = pDongle->keyboard.bActive;
  }
  return rvalue;
}
EXPORT_SYMBOL(hid_razer_keyboard_active);

void hid_razer_mouse_connect(void)
{
  RazerDongle *pDongle = &sGlobalDongle;
  int rvalue = 0;

  spin_lock(&pDongle->mouse.sLock);

  if ((false == pDongle->mouse.bActive) && (NULL != pDongle->mouse.pHid)) {
    pDongle->mouse.pHid->product = 0x90ff;
    rvalue = hid_parse(pDongle->mouse.pHid);

    if (0 != rvalue) {
      hid_err(pDongle->mouse.pHid, "failed to parse map\n");
    } else {
      rvalue = hid_hw_start(pDongle->mouse.pHid,
			    HID_CONNECT_DEFAULT | HID_CONNECT_HIDDEV_FORCE);
      if (0 != rvalue) {
	hid_err(pDongle->mouse.pHid, "Hardware failed to start\n");
      } else {
	/* We have a connected mouse. */
	pDongle->mouse.bActive = true;
      }
    }
  } else {
    HIDRD_MSG("Dongle Mouse already active!\n");
  }

  spin_unlock(&pDongle->mouse.sLock);
}
EXPORT_SYMBOL(hid_razer_mouse_connect);

void hid_razer_mouse_disconnect(void)
{
  RazerDongle *pDongle = &sGlobalDongle;
  struct hid_device *pHid = NULL;
  bool bActive = false;

  spin_lock(&pDongle->mouse.sLock);
  if ((true == pDongle->mouse.bActive) && (NULL != pDongle->mouse.pHid)) {
    pHid = pDongle->mouse.pHid;
    pDongle->mouse.pHid = NULL;
    bActive = true;
    pDongle->mouse.bActive = false;
  }
  spin_unlock(&pDongle->mouse.sLock);

  if (true == bActive) {
    private_disconnect_device(pHid);
  }
}
EXPORT_SYMBOL(hid_razer_mouse_disconnect);

bool hid_razer_mouse_active(void)
{
  RazerDongle *pDongle = &sGlobalDongle;
  bool rvalue = false;

  if (NULL != pDongle->mouse.pHid) {
    rvalue = pDongle->mouse.bActive;
  }
  return rvalue;
}
EXPORT_SYMBOL(hid_razer_mouse_active);

MODULE_LICENSE("GPL");

/* Module Information for modinfo */
MODULE_VERSION(HIDRD_STR_VERSION);
MODULE_ALIAS(HIDRD_NAME);

/* Author Information */
MODULE_AUTHOR("Stuart Wells <stuart.wells@razerzone.com>");
MODULE_DESCRIPTION("Razer Turrest Dongle Driver");
