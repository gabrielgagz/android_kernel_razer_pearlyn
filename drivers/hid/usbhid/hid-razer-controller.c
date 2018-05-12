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
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/hid.h>

#include "hid-razer.h"
#include "RzrProtocol.h"

#define RAZER_SERVAL2_VENDOR 0x1532
#define RAZER_SERVAL2_PRODUCT 0x0900
#define RAZER_MAX_REPORT 4096

static unsigned char private_serval2_controller [] = {
0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
0x09, 0x05,        // Usage (Game Pad)
0xA1, 0x01,        // Collection (Application)
0xA1, 0x02,        //   Collection (Logical)
0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
0x09, 0x30,        //     Usage (X)
0x09, 0x31,        //     Usage (Y)
0x09, 0x32,        //     Usage (Z)
0x09, 0x35,        //     Usage (Rz)
0x75, 0x08,        //     Report Size (8)
0x95, 0x04,        //     Report Count (4)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x35, 0x00,        //     Physical Minimum (0)
0x46, 0xFF, 0x00,  //     Physical Maximum (255)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x39,        //     Usage (Hat switch)
0x75, 0x04,        //     Report Size (4)
0x95, 0x01,        //     Report Count (1)
0x25, 0x07,        //     Logical Maximum (7)
0x46, 0x3B, 0x01,  //     Physical Maximum (315)
0x66, 0x14, 0x00,  //     Unit (System: English Rotation, Length: Centimeter)
0x81, 0x42,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
0x66, 0x00, 0x00,  //     Unit (None)
0x05, 0x09,        //     Usage Page (Button)
0x09, 0x01,        //     Usage (0x01)
0x09, 0x02,        //     Usage (0x02)
0x09, 0x04,        //     Usage (0x04)
0x09, 0x05,        //     Usage (0x05)
0x09, 0x07,        //     Usage (0x07)
0x09, 0x08,        //     Usage (0x08)
0x05, 0x0C,        //     Usage Page (Consumer)
0x0A, 0x24, 0x02,  //     Usage (AC Back)
0x05, 0x09,        //     Usage Page (Button)
0x09, 0x0C,        //     Usage (0x0C)
0x09, 0x0E,        //     Usage (0x0E)
0x09, 0x0F,        //     Usage (0x0F)
0x05, 0x0C,        //     Usage Page (Consumer)
0x0A, 0x21, 0x02,  //     Usage (AC Search)
0x0A, 0x23, 0x02,  //     Usage (AC Home)
0x05, 0x09,        //     Usage Page (Button)
0x09, 0x0B,        //     Usage (0x0B)
0x75, 0x01,        //     Report Size (1)
0x95, 0x0D,        //     Report Count (13)
0x15, 0x00,        //     Logical Minimum (0)
0x25, 0x01,        //     Logical Maximum (1)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x75, 0x01,        //     Report Size (1)
0x95, 0x07,        //     Report Count (7)
0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x05, 0x02,        //     Usage Page (Sim Ctrls)
0x09, 0xC5,        //     Usage (Brake)
0x09, 0xC4,        //     Usage (Accelerator)
0x75, 0x08,        //     Report Size (8)
0x95, 0x02,        //     Report Count (2)
0x15, 0x00,        //     Logical Minimum (0)
0x26, 0xFF, 0x00,  //     Logical Maximum (255)
0x35, 0x00,        //     Physical Minimum (0)
0x46, 0xFF, 0x00,  //     Physical Maximum (255)
0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0xC0,              //   End Collection
0x05, 0x08,        //   Usage Page (LEDs)
0x09, 0x01,        //   Usage (Num Lock)
0x09, 0x02,        //   Usage (Caps Lock)
0x09, 0x03,        //   Usage (Scroll Lock)
0x09, 0x04,        //   Usage (Compose)
0x09, 0x4F,        //   Usage (0x4F)
0x09, 0x50,        //   Usage (0x50)
0x09, 0x51,        //   Usage (0x51)
0x09, 0x52,        //   Usage (0x52)
0x75, 0x01,        //   Report Size (1)
0x95, 0x08,        //   Report Count (8)
0x15, 0x00,        //   Logical Minimum (0)
0x25, 0x01,        //   Logical Maximum (1)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              // End Collection
};
// 160 bytes

typedef struct private_hid_controller {
  DongleData *pDongle;
  struct hid_device *pHid;
  bool bStart;
  unsigned char uLink;
  unsigned char uLed;
  unsigned int uLedCount;
} ControllerData;


static int private_hid_parse(struct hid_device *pHid)
{
  ControllerData *pController = NULL;
  int rvalue = 0;

  if (NULL != pHid) {
    pController = (ControllerData *)pHid->driver_data;

    RZRDNG_LOG("Called %p\n", pController);

    if (NULL != pController) {
      rvalue = hid_parse_report(pHid,
			  private_serval2_controller,
			  sizeof(private_serval2_controller));
      printk("Parse report ->%d\n", rvalue);
    }
  } else {
    RZRDNG_LOG("Called error %p\n", pController);
  }

  return rvalue;
}

static int private_hid_start(struct hid_device *pHid)
{
  ControllerData *pController = NULL;

  if (NULL != pHid) {
    pController = (ControllerData *)pHid->driver_data;
    if (NULL != pController) {
      pController->bStart = true;
    }
  }

  RZRDNG_LOG("Called %p\n", pController);
  return 0;

}

static void private_hid_stop(struct hid_device *pHid)
{
  ControllerData *pController = (ControllerData *)NULL;

  if (NULL != pHid) {
    pController = (ControllerData *)pHid->driver_data;
    if (NULL != pController) {
      pController->bStart = false;
    }
  }

  RZRDNG_LOG("Called %p\n", pController);
}

static int private_hid_open(struct hid_device *pHid)
{
  ControllerData *pController = (ControllerData *)pHid->driver_data;

  RZRDNG_LOG("Called %p\n", pController);

  //return uhid_queue_event(uhid, UHID_OPEN);
  return 0;
}

static void private_hid_close(struct hid_device *pHid)
{
  ControllerData *pController = (ControllerData *)pHid->driver_data;

  RZRDNG_LOG("Called %p\n", pController);

  //private_queue_event(uhid, UHID_CLOSE);
}


static struct hid_ll_driver private_hid_driver = {
  .start = private_hid_start,
  .stop = private_hid_stop,
  .open = private_hid_open,
  .close = private_hid_close,
  .parse = private_hid_parse,
};


static int private_hid_get_raw(struct hid_device *pHid,
			       unsigned char uNum,
			       __u8 *pBuffer, size_t uCount,
			       unsigned char uType)
{

  RZRDNG_LOG("Called %p %d %p %d %d\n",
	     pHid, uNum, pBuffer, uCount, uType);

  return 0;
}


static int private_hid_output_raw(struct hid_device *pHid,
				  __u8 *pBuffer, size_t uCount,
				  unsigned char uType)
{
  EventData event;
  ControllerData *pController = NULL;
  u8 uLed = 0;

  memset((void *)&event, 0, sizeof(EventData));
  if (NULL != pHid) {
    pController = pHid->driver_data;

    if ((NULL != pBuffer) && (1 == uCount) && (1 == uType)) {
      event.byte [0] = pController->uLink;
      uLed = pBuffer [0];
      if ((0x01 <= uLed) && (0x0f >= uLed)) {
	if (0x01 == uLed) {
	  event.byte [1] = 0x0f;
	} else {
	  event.byte [1] = uLed;
	}

	if ((uLed != pController->uLed) || (pController->uLedCount < 3)) {
	  if (uLed == pController->uLed) {
	    pController->uLedCount++;
	  } else {
	    pController->uLedCount = 0;
	  }

	  if (true == rzr_ctrl_getslot()) {
	    rzr_ctrl_process(&event, DONGLE_THREAD_LED);
	  }
	}
      }
    }
  }

  return uCount;
}

/* Code to initialize a virtual controller */
void *rzr_controller_add(void *pPtr, unsigned int uLink)
{
  DongleData *pDongle = (DongleData *)pPtr;
  ControllerData *pController = NULL;
  int tmpval = 0;

  if (NULL != pDongle) {
    pController = (ControllerData *)kmalloc(sizeof(ControllerData), GFP_NOFS);

    if (NULL != pController) {
      /* Remember our connected dongle */
      pController->pDongle = pDongle;

      pController->pHid = hid_allocate_device();
      if (IS_ERR(pController->pHid)) {
	RZRDNG_LOG("Could not allocate controller device ->%ld\n",
		   PTR_ERR(pController->pHid));
	kfree(pController);
	pController = NULL;
      } else {
	/* Set the virtual name of our device */
	pController->uLink = uLink;
	memset(pController->pHid->name, 0, 127);
	strcpy(pController->pHid->name, "Razer Serval");
	memset(pController->pHid->phys, 0, 63);
	memset(pController->pHid->uniq, 0, 63);
	pController->pHid->bus = BUS_USB;
	pController->pHid->vendor = RAZER_SERVAL2_VENDOR;
	pController->pHid->product = RAZER_SERVAL2_PRODUCT;
	pController->pHid->version = 0;
	pController->pHid->country = 0;

	pController->pHid->ll_driver = &private_hid_driver;
        pController->pHid->hid_get_raw_report = private_hid_get_raw;
	pController->pHid->hid_output_raw_report = private_hid_output_raw;
	pController->pHid->driver_data = (void *)pController;
	pController->bStart = false;
	pController->uLed = 0;
	pController->uLedCount = 0;

	//pController->pHid->dev.parent = THIS_MODULE;
	printk("Before hid_add_device\n");
	tmpval = hid_add_device(pController->pHid);
	if (0 != tmpval) {
	  if (NULL != pController->pHid) {
	    hid_destroy_device(pController->pHid);
	    pController->pHid = NULL;
	  }
	  kfree(pController);
	  pController = NULL;
	  RZRDNG_MSG("Could not add controller device\n");
	}
      }
    }
  }

  return(pController);
}

void rzr_controller_del(void *pPtr)
{
  ControllerData *pController = (ControllerData *)pPtr;

  if (NULL != pController) {

    if (NULL != pController->pHid) {
      hid_destroy_device(pController->pHid);
      pController->pHid = NULL;
    }
    pController->pDongle = NULL;
    pController->uLink = 0;

    printk("*** destroyed controller data\n");
    kfree(pController);
  }
}

void rzr_controller_event(void *pPtr, __u8 *pBuffer, size_t uCount)
{
  ControllerData *pController = (ControllerData *)pPtr;
  size_t uMin = RAZER_MAX_REPORT;

  if (NULL != pController) {
    if (uCount < uMin) {
      uMin = uCount;
    }

    hid_input_report(pController->pHid,
		     HID_INPUT_REPORT,
		     pBuffer,
		     uMin, 1);
  }
}
