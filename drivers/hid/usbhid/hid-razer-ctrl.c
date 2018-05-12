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
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/jiffies.h>
#include <linux/wait.h>

#include "../hid-ids.h"
#include "hid-razer.h"
#include "RzrProtocol.h"

#define RZRDNG_STR_VERSION "1.0"
#define RZRDNG_NAME "usbrzrdongle"

// time out in msecs
#define MAX_TIMEOUT 5000
#define REQUIRED_HEARTBEAT_CREATE 3

enum {
  DONGLE_KEYBOARD=0,
  DONGLE_MOUSE=1,
  DONGLE_CONTROLLER=2,
} DONGLE_PIPE;

enum {
  DEVICE_HEARTBEAT=1,
  DEVICE_PAIR_SUCCESS=2,
  DEVICE_PAIR_FAIL=3,
} DEVICE_NOTIFICATION;
#define RAZER_CTRL_QUEUE "RazerControl"

DongleData sGlobalData;

typedef struct {
  struct work_struct ctrl_work_hdr;
  EventData event;
  unsigned int uiCommand;
} EventControl;


extern void hid_razer_dongle_connect(void);
extern void hid_razer_dongle_disconnect(void);

static void priv_dongle_heart(unsigned char uDev,
			      unsigned char uLink)
{
  DongleData *pDongle = &sGlobalData;
  unsigned int loop = 0;
  unsigned int uEmpty = MAX_CONTROLLER;
  bool bFound = false;

  mutex_lock(&pDongle->sMutex);
  switch(uDev)
    {
    case DONGLE_KEYBOARD:
      if (false == pDongle->keyboard.bActive) {
	pDongle->ulConnectCount++;
	hid_razer_keyboard_connect();
	pDongle->keyboard.bActive = true;
	pDongle->keyboard.timeout = jiffies_to_msecs(jiffies);
      } else {
	pDongle->keyboard.timeout = jiffies_to_msecs(jiffies);
      }
      break;
    case DONGLE_MOUSE:
      if (false == pDongle->mouse.bActive) {
	pDongle->ulConnectCount++;
	hid_razer_mouse_connect();
	pDongle->mouse.bActive = true;
	pDongle->mouse.timeout = jiffies_to_msecs(jiffies);
      } else {
	pDongle->mouse.timeout = jiffies_to_msecs(jiffies);
      }
      break;
    case DONGLE_CONTROLLER:
      uEmpty = MAX_CONTROLLER;
      bFound = false;

      for (loop = 0; loop < MAX_CONTROLLER; loop++) {
	if (true == pDongle->controller [loop].bActive) {
	  if (uLink == pDongle->controller [loop].uLink) {
	    pDongle->controller [loop].timeout = jiffies_to_msecs(jiffies);


	    if (NULL == pDongle->controller [loop].pPtr) {
	      if (pDongle->controller [loop].uHeartCount >= REQUIRED_HEARTBEAT_CREATE) {
		pDongle->ulConnectCount++;
		pDongle->controller [loop].pPtr =
		  rzr_controller_add(pDongle, uLink);
		RZRDNG_MSG("actually created a conntroller\n");

	      } else {
		pDongle->controller [loop].uHeartCount++;
	      }
	    }
	    bFound = true;
	  }
	} else {
	  if (uEmpty >= MAX_CONTROLLER) {
	    uEmpty = loop;
	  }
	}
      }

      if (false == bFound) {
	if (uEmpty < MAX_CONTROLLER) {
	  pDongle->controller [uEmpty].timeout = jiffies_to_msecs(jiffies);
	  pDongle->controller [uEmpty].uLink = uLink;
	  pDongle->controller [uEmpty].uHeartCount = 0;
	  pDongle->controller [uEmpty].bActive = true;
	  RZRDNG_MSG("Create a controller?\n");
	} else {
	  RZRDNG_LOG("No free controller found %d\n", uEmpty);
	}
      }
      break;
    default:
      break;
    }

  mutex_unlock(&sGlobalData.sMutex);
}

static void priv_dongle_delete(unsigned char uDev,
			       unsigned char uNum)
{
  DongleData *pDongle = &sGlobalData;
  void *pTmpVal = NULL;

  switch(uDev)
    {
    case DONGLE_KEYBOARD:
      if (true == pDongle->keyboard.bActive) {
	mutex_lock(&pDongle->sMutex);
	hid_razer_keyboard_disconnect();
	pDongle->keyboard.bActive = false;
	pDongle->ulDestroyCount++;
	mutex_unlock(&pDongle->sMutex);
      }
      break;
    case DONGLE_MOUSE:
      if (true == pDongle->mouse.bActive) {
	mutex_lock(&pDongle->sMutex);
	hid_razer_mouse_disconnect();
	pDongle->mouse.bActive = false;
	pDongle->ulDestroyCount++;
	mutex_unlock(&pDongle->sMutex);
      }
      break;
    case DONGLE_CONTROLLER:
      if ((0 <= uNum) && (MAX_CONTROLLER > uNum)) {
	if (NULL != pDongle->controller [uNum].pPtr) {
	  pTmpVal = pDongle->controller [uNum].pPtr;
	  pDongle->controller [uNum].pPtr = NULL;
	  pDongle->controller [uNum].uLink = 0;
	  pDongle->controller [uNum].bActive = false;
	  pDongle->controller [uNum].timeout = 0;
	  pDongle->controller [uNum].uHeartCount = 0;
	}

	if (NULL != pTmpVal) {
	  rzr_controller_del(pTmpVal);
	  pDongle->ulDestroyCount++;
	  pTmpVal = NULL;
	}
	break;
      default:
	break;
      }
    }
}

static int priv_disconnect_thread(void *pPtr)
{
  DongleData *pDongle = (DongleData *)pPtr;
  int loop;
  unsigned long ulmtime;
  unsigned long delta;

  while ((false == kthread_should_stop()) &&
         (false != pDongle->bConnected)) {
    /* Wait a second, before checking to see if controlers should die */
    wait_event_interruptible_timeout(pDongle->threadQueue,
				     (false == pDongle->bConnected),
				     msecs_to_jiffies(1000));
    ulmtime = jiffies_to_msecs(jiffies);

    if (true == pDongle->bConnected) {
      if (true == pDongle->keyboard.bActive) {
	delta = ulmtime - pDongle->keyboard.timeout;
	if (delta > MAX_TIMEOUT) {
	  mutex_lock(&pDongle->sMutex);
	  priv_dongle_delete(DONGLE_KEYBOARD, 1);
	  pDongle->ulTimeoutCount++;
	  mutex_unlock(&pDongle->sMutex);
	}
      }

      if (true == pDongle->mouse.bActive) {
	delta = ulmtime - pDongle->mouse.timeout;
	if (delta > MAX_TIMEOUT) {
	  mutex_lock(&pDongle->sMutex);
	  priv_dongle_delete(DONGLE_MOUSE, 1);
	  pDongle->ulTimeoutCount++;
	  mutex_unlock(&pDongle->sMutex);
	}
      }

      for (loop = 0; loop < MAX_CONTROLLER; loop++) {
	if (true == pDongle->controller [loop].bActive) {
	  delta = ulmtime - pDongle->controller [loop].timeout;
	  RZRDNG_LOG("Controller %d active %d %p delta %lu\n",
		     loop, pDongle->controller [loop].bActive,
		     pDongle->controller [loop].pPtr, delta);
	  if (NULL == pDongle->controller [loop].pPtr) {
	    if (delta > 500) {
	      RZRDNG_LOG("Remove stale controller request %d %d\n",
			 loop, pDongle->controller [loop].uLink);

	      mutex_lock(&pDongle->sMutex);
	      pDongle->controller [loop].pPtr = NULL;
	      pDongle->controller [loop].uLink = 0;
	      pDongle->controller [loop].bActive = false;
	      pDongle->controller [loop].timeout = 0;
	      pDongle->controller [loop].uHeartCount = 0;
	      mutex_unlock(&pDongle->sMutex);
	    }
	  } else {
	    if (delta > MAX_TIMEOUT) {
	      printk("Time outs %lu %lu\n", ulmtime,
		     pDongle->controller [loop].timeout);
	      RZRDNG_LOG("Timeout for controller %x %lu\n",
			 pDongle->controller [loop].uLink,
			 delta);
	      mutex_lock(&pDongle->sMutex);
	      priv_dongle_delete(DONGLE_CONTROLLER, loop);
	      pDongle->ulTimeoutCount++;
	      mutex_unlock(&pDongle->sMutex);
	    }
	  }
	}
      }
    }
  }


  RZRDNG_MSG("Thread going away Calling all delete\n");
  /* If thread is going away, make sure any connected device dies */
  mutex_lock(&pDongle->sMutex);
  priv_dongle_delete(DONGLE_KEYBOARD, 1);
  priv_dongle_delete(DONGLE_MOUSE, 1);
  priv_dongle_delete(DONGLE_CONTROLLER, 0);
  priv_dongle_delete(DONGLE_CONTROLLER, 1);
  priv_dongle_delete(DONGLE_CONTROLLER, 2);
  priv_dongle_delete(DONGLE_CONTROLLER, 3);
  mutex_unlock(&pDongle->sMutex);

  /* Wait to die */
  while ((false == kthread_should_stop())) {
    msleep(1000);
    RZRDNG_MSG("Waiting to die\n");
  }

  return 0;
}

static void priv_dongle_control(struct work_struct *pPtr)
{
  EventControl *pEvent = (EventControl *)pPtr;
  DongleData *pDongle = &sGlobalData;
  unsigned int loop = 0;
  void *pKillThread = NULL;

  if (NULL != pEvent) {

    switch(pEvent->uiCommand)
      {
      case DONGLE_THREAD_CREATE:
	mutex_lock(&pDongle->sMutex);
	if (false == pDongle->bConnected) {
	  /* Create the thread. */
	  if (NULL == pDongle->pThread) {
	    pDongle->bConnected = true;

	    /* Create recv queue, before setting callbacks */
	    init_waitqueue_head(&pDongle->threadQueue);

	    /* Start the thread. */
	    pDongle->pThread = (void *)kthread_run(priv_disconnect_thread,
						   (void *)pDongle,
						   pDongle->sThreadName);
	  }
	}
	mutex_unlock(&pDongle->sMutex);
	break;

      case DONGLE_THREAD_DESTROY:
	mutex_lock(&pDongle->sMutex);
	if (NULL != pDongle->pThread) {
	  pDongle->bConnected = false;

	  /* Wake up everyone, so theytk go away. */
	  wake_up_all(&pDongle->threadQueue);
	  pKillThread = pDongle->pThread;
	  pDongle->pThread = NULL;
	}
	mutex_unlock(&pDongle->sMutex);

	if (NULL != pKillThread) {
	  kthread_stop((struct task_struct *)pKillThread);
 	} else {
	  printk("Total error\n");
	}
	break;

      case DONGLE_THREAD_HEARTBEAT:
	if (0 == pEvent->event.byte [0]) {
	  switch(pEvent->event.byte [1])
	    {
	    case DEVICE_HEARTBEAT:
	      priv_dongle_heart(pEvent->event.byte [2], pEvent->event.byte [3]);
	      break;
	    case DEVICE_PAIR_SUCCESS:
	      break;
	    case DEVICE_PAIR_FAIL:
	      break;
	    default:
	      RZRDNG_LOG("unknown status  %x:%x:%x:%x %x:%x:%x:%x recieved\n",
			 pEvent->event.byte [0],
			 pEvent->event.byte [1],
			 pEvent->event.byte [2],
			 pEvent->event.byte [3],
			 pEvent->event.byte [4],
			 pEvent->event.byte [5],
			 pEvent->event.byte [6],
			 pEvent->event.byte [7]
			 );
	      break;
	    }
	} else {
	  for (loop = 0; loop < MAX_CONTROLLER; loop++) {
	    if ((NULL != pDongle->controller [loop].pPtr) &&
		(true ==  pDongle->controller [loop].bActive)) {
	      if (pEvent->event.byte [0] == pDongle->controller [loop].uLink) {
		rzr_controller_event(pDongle->controller [loop].pPtr,
				 &pEvent->event.byte [1],
				 9);
		pDongle->controller [loop].timeout = jiffies_to_msecs(jiffies);
		break;
	      }
	    }
	  }
	}
	break;
      case DONGLE_THREAD_LED:
	mutex_lock(&pDongle->sMutex);
	RzrSubmitReport((char *)pEvent->event.byte, 2);
	mutex_unlock(&pDongle->sMutex);
	break;
      default:
	break;
    }
  }

  return;
}

bool rzr_ctrl_getslot()
{
  DongleData *pDongle = &sGlobalData;
  bool rvalue = false;

  spin_lock(&pDongle->sLock);
  if (true ==  pDongle->bCtrlEmpty) {
      pDongle->bCtrlEmpty = false;
      rvalue = true;
  }
  spin_unlock(&pDongle->sLock);

  return rvalue;
}

void rzr_ctrl_process(EventData *pEvent, unsigned int uiCommand)
{
  mm_segment_t cur_fs;
  EventControl *pWork = NULL;

  /* We could be called from user space */
  cur_fs = get_fs();

  set_fs(KERNEL_DS);
  if (NULL != sGlobalData.pDongleQueue) {
    pWork = (EventControl *)kmalloc(sizeof(EventControl), GFP_NOFS);
    memset(pWork, 0, sizeof(EventControl));
    if (NULL != pWork) {
      INIT_WORK((struct work_struct *)pWork, priv_dongle_control);
      if (NULL != pEvent) {
	memcpy((void *)&pWork->event, pEvent, sizeof(EventData));
      } else {
	memset((void *)&pWork->event, 0, sizeof(EventData));
      }
      pWork->uiCommand = uiCommand;
      queue_work(sGlobalData.pDongleQueue,
		 (struct work_struct *)pWork);
    } else {
      kfree(pWork);
    }
  }

  set_fs(cur_fs);
}

static void private_irq_in(struct urb *pUrb)
{
  int rvalue = 0;
  EventData event;
  int isize = 0;
  DongleData *pDongle = &sGlobalData;


  switch(pUrb->status)
    {
    case 0:
      usb_mark_last_busy(interface_to_usbdev(pDongle->pFace));

      isize = sizeof(EventData);
      if (isize > pDongle->pUrbIn->actual_length) {
	isize = pDongle->pUrbIn->actual_length;
      }
      if (isize != sizeof(EventData)) {
	RZRDNG_LOG("Odin is a cat %d\n", isize);
      }
      memcpy(&event, pDongle->pUrbIn->transfer_buffer, isize);
      rzr_ctrl_process(&event, DONGLE_THREAD_HEARTBEAT);
      pDongle->ulInputMsg++;
      break;
    case -EPIPE:
      usb_mark_last_busy(interface_to_usbdev(pDongle->pFace));

      RZRDNG_MSG("pipe error\n");
      break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
      RZRDNG_MSG("shutdown error\n");
      break;
    case -EILSEQ:
    case -EPROTO:
    case -ETIME:
    case -ETIMEDOUT:
      /* Protocol error for usb.  Unpluged */
      RZRDNG_MSG("time error\n");
      break;
    default:
      RZRDNG_LOG("irq input status %d error\n", pUrb->status);
      break;
    }


  /* success case */
  rvalue = usb_submit_urb(pUrb, GFP_ATOMIC);
  if (rvalue) {
    RZRDNG_LOG("irq submit %d error\n", rvalue);
  }
}

static void private_ctrl(struct urb *pUrb)
{
  DongleData *pDongle = &sGlobalData;

  switch(pUrb->status)
    {
    case 0:
      break;
    case -EPIPE:
      RZRDNG_MSG("pipe error\n");
      break;
    case -ECONNRESET:
    case -ENOENT:
    case -ESHUTDOWN:
      RZRDNG_MSG("shutdown error\n");
      break;
    case -EILSEQ:
    case -EPROTO:
    case -ETIME:
    case -ETIMEDOUT:
      /* Protocol error for usb.  Unpluged */
      RZRDNG_MSG("time error\n");
      break;
    default:
      RZRDNG_LOG("ctrl input status %d error\n", pUrb->status);
      break;
    }

  usb_autopm_put_interface_async(pDongle->pFace);

  spin_lock(&pDongle->sLock);
  pDongle->bCtrlEmpty = true;
  spin_unlock(&pDongle->sLock);
}

void rzr_ctrl_write(u8 *pBuffer, unsigned int uiSize)
{
  DongleData *pDongle = &sGlobalData;
  int rvalue;

  /* The setup packet is a HID Set_Report for the request and then a HID
     Get_Report for the response. These types of packets are always sent to
     the interface which is why it must be sent through a parameter.

     The HID message are documented here...
     http://www.usb.org/developers/hidpage/HID1_11.pdf
  */

  pDongle->pCr->bRequestType =
    USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT;
  pDongle->pCr->bRequest = HID_REQ_SET_REPORT;
  pDongle->pCr->wIndex = cpu_to_le16(pDongle->bInterfaceNumber);
  pDongle->pCr->wLength = cpu_to_le16(uiSize);
  pDongle->pCr->wValue = cpu_to_le16(0x0300);

  /* We have a wait queue, it will wait for this flag to become true. */
  pDongle->bCtrlEmpty = false;

  pDongle->pUrbCtrl->pipe = usb_sndctrlpipe(interface_to_usbdev(pDongle->pFace), 0);
  pDongle->pUrbCtrl->transfer_buffer_length = uiSize;

  memcpy(pDongle->pBufferCtrl, pBuffer, uiSize);
  pDongle->pUrbCtrl->dev = interface_to_usbdev(pDongle->pFace);

  rvalue = usb_submit_urb(pDongle->pUrbCtrl, GFP_ATOMIC);
  if (0 > rvalue) {
    RZRDNG_LOG("usb_submit_urb failed: %d\n", rvalue);
  }
}

static const struct usb_device_id priv_razer_devices[] = {
  {
    .match_flags = USB_DEVICE_ID_MATCH_INT_NUMBER | USB_DEVICE_ID_MATCH_DEVICE,
    .idVendor = USB_VENDOR_ID_RAZER,
    .idProduct = USB_DEVICE_ID_RAZER_DONGLE,
    .bInterfaceNumber = 2
  },
  { }
};

static void priv_dongle_free(DongleData *pDongle)
{
  struct usb_device *pDev = NULL;

  RZRDNG_LOG("calhled %p\n", pDongle);

  if (NULL != pDongle) {
    if (true == pDongle->bUsed) {
      pDongle->bUsed = false;
    }

    if (NULL != pDongle->pUrbIn) {
      usb_free_urb(pDongle->pUrbIn);
      pDongle->pUrbIn = NULL;
    }

    if (NULL != pDongle->pUrbCtrl) {
      usb_free_urb(pDongle->pUrbCtrl);
      pDongle->pUrbCtrl = NULL;
    }

    if (NULL != pDongle->pFace) {
      pDev = interface_to_usbdev(pDongle->pFace);

      usb_set_intfdata(pDongle->pFace, NULL);
      pDongle->pFace = NULL;

      if (NULL != pDongle->pBufferIn) {
	usb_free_coherent(pDev,
			  MAX_BUFFER_SIZE,
			  pDongle->pBufferIn, pDongle->dmaBuffIn);
	pDongle->pBufferIn = NULL;
      }

      if (NULL != pDongle->pBufferCtrl) {
	usb_free_coherent(pDev,
			  MAX_BUFFER_SIZE,
			  pDongle->pBufferCtrl, pDongle->dmaBuffCtrl);
	pDongle->pBufferCtrl = NULL;
      }

      if (NULL != pDongle->pCr) {
	kfree(pDongle->pCr);
	pDongle->pCr = NULL;
      }
    }
  }
}

static int priv_dongle_probe(struct usb_interface *pFace,
				    const struct usb_device_id *pId)
{
  unsigned int loop;
  struct usb_host_interface *pDesc = NULL;
  struct usb_device *pDev = NULL;
  struct usb_endpoint_descriptor *pEnd = NULL;
  bool bInterruptEnd = false;
  bool bError = true;
  DongleData *pDongle = &sGlobalData;
  int rvalue = 0;

  RZRDNG_LOG("(%p) (%p)\n", pFace, pId);

  if (NULL != pId) {
    RZRDNG_LOG("vendor (%x) product (%x)\n", pId->idVendor, pId->idProduct);
  }


  if ((false == pDongle->bUsed) && (NULL != pFace)) {
    rzr_ctrl_process(NULL, DONGLE_THREAD_CREATE);
    pDev = interface_to_usbdev(pFace);
    pDesc = pFace->cur_altsetting;

    if (NULL != pDesc) {
      for (loop = 0; loop < pDesc->desc.bNumEndpoints; loop++) {
	pEnd = &pDesc->endpoint [loop].desc;

	if (0 != usb_endpoint_xfer_int(pEnd)) {
	  if (true == usb_endpoint_dir_in(pEnd)) {
	    pDongle->pBufferIn = usb_alloc_coherent(pDev, MAX_BUFFER_SIZE,
						  GFP_KERNEL,
						  &pDongle->dmaBuffIn);
	    if (NULL != pDongle->pBufferIn) {
	      pDongle->pUrbIn = usb_alloc_urb(0, GFP_KERNEL);
	      if (NULL != pDongle->pUrbIn) {

		usb_fill_int_urb(pDongle->pUrbIn,
				 pDev,
				 usb_rcvintpipe(pDev, pEnd->bEndpointAddress),
				 pDongle->pBufferIn, MAX_BUFFER_SIZE,
				 private_irq_in,
				 pDongle,
				 pEnd->bInterval);
		pDongle->pUrbIn->transfer_dma = pDongle->dmaBuffIn;
		pDongle->pUrbIn->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		pDongle->bInterfaceNumber = pDesc->desc.bInterfaceNumber;
		bInterruptEnd = true;
		break;
	      }
	    }
	  }
	}

      }

      if (true == bInterruptEnd) {
	pDongle->pBufferCtrl = usb_alloc_coherent(pDev, MAX_CTRL_BUFFER_SIZE,
						GFP_KERNEL,
						&pDongle->dmaBuffCtrl);
	if (NULL != pDongle->pBufferCtrl) {
	  pDongle->pCr = kmalloc(sizeof(*pDongle->pCr), GFP_KERNEL);
	  if (NULL != pDongle->pCr) {
	    pDongle->pUrbCtrl = usb_alloc_urb(0, GFP_KERNEL);
	    if (NULL != pDongle->pUrbCtrl) {
	      usb_fill_control_urb(pDongle->pUrbCtrl,
				   pDev,
				   0,
				   (void *)pDongle->pCr,
				   pDongle->pBufferCtrl, 1,
				   private_ctrl,
				   pDongle);
	      pDongle->pUrbCtrl->transfer_dma = pDongle->dmaBuffCtrl;
	      pDongle->pUrbCtrl->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	      pDongle->bUsed = true;
	      pDongle->pFace = pFace;
	      rvalue = 0;
	      bError = false;

	      /* Start the interrupt input process */
	      usb_submit_urb(pDongle->pUrbIn, GFP_ATOMIC);
	      usb_submit_urb(pDongle->pUrbCtrl, GFP_ATOMIC);
	    }
	  }
	}
      }

      if (true == bError) {
	RZRDNG_MSG("probe failed to use endpoint\n");
	priv_dongle_free(&sGlobalData);
	rvalue = -ENOMEM;
      }
    }
  }

  return rvalue;
}

static void priv_dongle_disconnect(struct usb_interface *pFace)
{
  DongleData *pDongle = &sGlobalData;

  RZRDNG_LOG("(%p)\n", pFace);

  if (true == pDongle->bUsed) {
    RZRDNG_LOG("poingt 1 %p\n", pFace);
    if (NULL != pDongle->pUrbIn) {
	usb_kill_urb(pDongle->pUrbIn);
    }

    if (NULL != pDongle->pUrbCtrl) {
      /* Create ctrl queue */
      usb_kill_urb(pDongle->pUrbIn);

      /* Do we have everything transmitted */
      pDongle->bCtrlEmpty = true;
    }

    priv_dongle_free(&sGlobalData);
    rzr_ctrl_process(NULL, DONGLE_THREAD_DESTROY);
  }
}

static struct usb_driver priv_razer_dongle = {
  .name = "razer-usb",
  .id_table = priv_razer_devices,
  .probe = priv_dongle_probe,
  .disconnect = priv_dongle_disconnect,
};

static int __init priv_rzrdng_init(void)
{
  int rvalue = 0;
  int loop;
  DongleData *pDongle = &sGlobalData;

  RZRDNG_MSG("I have installed\n");

  pDongle->pFace = NULL;
  pDongle->bUsed = false;
  pDongle->ulInputMsg = 0;
  pDongle->ulConnectCount = 0;
  pDongle->ulDestroyCount = 0;
  pDongle->ulTimeoutCount = 0;

  for (loop = 0; loop < MAX_CONTROLLER; loop++) {
    pDongle->controller [loop].pPtr = NULL;
    pDongle->controller [loop].bActive = false;
    pDongle->controller [loop].uLink = 0;
    pDongle->controller [loop].timeout = 0;
    pDongle->controller [loop].uHeartCount = 0;
  }

  spin_lock_init(&pDongle->sLock);
  mutex_init(&pDongle->sMutex);

  strcpy(pDongle->sThreadName, "ServalHeart");
  pDongle->pDongleQueue = (void *)create_workqueue(RAZER_CTRL_QUEUE);
  pDongle->pThread = NULL;
  pDongle->bConnected = false;
  pDongle->keyboard.bActive = false;
  pDongle->mouse.bActive = false;

  RZRDNG_MSG("---> Started the proc interface\n");
  rzr_proc_init(&sGlobalData);
  rvalue = usb_register(&priv_razer_dongle);

  return rvalue;
}

/*
   Inform the linux kernel of the entry point.
*/
module_init(priv_rzrdng_init);

/*
  The linux kernel module insmod exit point.
*/
static void __exit priv_rzrdng_exit(void)
{
  DongleData *pDongle = &sGlobalData;

  RZRDNG_MSG("Module has been removed\n");

  if (NULL != pDongle->pDongleQueue) {
    flush_workqueue((struct workqueue_struct *)pDongle->pDongleQueue);
    destroy_workqueue((struct workqueue_struct *)pDongle->pDongleQueue);
  }

  priv_dongle_free(&sGlobalData);

  if (NULL != pDongle->pThread) {
    pDongle->bConnected = false;

    /* Wake up everyone, so they go away. */
    wake_up_all(&pDongle->threadQueue);

    if (NULL != pDongle->pThread) {
      kthread_stop((struct task_struct *)pDongle->pThread);
      pDongle->pThread = NULL;
    }
  }

  usb_deregister(&priv_razer_dongle);
  rzr_proc_deinit(pDongle);
}

/*
   Inform the linux kernel of the exit point.
*/
module_exit(priv_rzrdng_exit);


MODULE_LICENSE("GPL");

/* Module Information for modinfo */
MODULE_VERSION(RZRDNG_STR_VERSION);
MODULE_ALIAS(RZRDNG_NAME);

/* Author Information */
MODULE_AUTHOR("Stuart Wells <stuart.wells@razerzone.com>");
MODULE_DESCRIPTION("Razer Dongle Driver");
