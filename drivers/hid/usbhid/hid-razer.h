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
#ifndef RAZER_DONGLE_INCLUDE
#define RAZER_DONGLE_INCLUDE

#include <linux/hid.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#define RZRDNG_LOG(fmt, args...) printk("rzrdng::%s "fmt, __func__, args)
#define RZRDNG_MSG(fmt) printk("rzrdng::%s "fmt, __func__)

#define MAX_INTERFACE 4
#define MAX_BUFFER_SIZE 10
#define MAX_CONTROLLER 4
#define MAX_CTRL_BUFFER_SIZE 90

typedef struct priv_endpoint_data {
} RzrInterfaceData;

typedef struct priv_dongle_data {
  /* Dongle / interface information */
  struct usb_interface *pFace;
  struct urb *pUrbIn;
  struct urb *pUrbCtrl;
  char *pBufferIn;
  char *pBufferCtrl;
  bool bUsed;
  dma_addr_t dmaBuffIn;
  dma_addr_t dmaBuffCtrl;
  int bInterfaceNumber;
  struct usb_ctrlrequest *pCr;
  bool bCtrlEmpty;
  unsigned long ulInputMsg;
  unsigned long ulConnectCount;
  unsigned long ulDestroyCount;
  unsigned long ulTimeoutCount;

  /* Protection / proc control */
  spinlock_t sLock;
  struct mutex sMutex;
  struct proc_dir_entry *pProcEntry;
  void *pDongleQueue;

  /* Controller information */
  struct {
    void *pPtr;
    bool bActive;
    unsigned int uHeartCount;
    unsigned long timeout;
    unsigned int uLink;
  } controller [MAX_CONTROLLER];

  /* Keyboard information */
  struct {
    unsigned long timeout;
    bool bActive;
  } keyboard;

  /* Mouse information */
  struct {
    unsigned long timeout;
    bool bActive;
  } mouse;

  void *pThread;
  char sThreadName[64];
  bool bConnected;
  wait_queue_head_t threadQueue;
} DongleData;

typedef struct priv_event_data {
  unsigned char byte [MAX_BUFFER_SIZE];
} EventData;

enum {
  DONGLE_THREAD_CREATE=0,
  DONGLE_THREAD_DESTROY=1,
  DONGLE_THREAD_HEARTBEAT=2,
  DONGLE_THREAD_LED=3,
};

extern void rzr_proc_init(void *pPtr);
extern void rzr_proc_deinit(void *pPtr);
extern void rzr_ctrl_process(EventData *pEvent, unsigned int uiCommand);
extern void *rzr_controller_add(void *pPtr, unsigned int uLink);
extern void rzr_controller_del(void *pPtr);
extern void rzr_controller_event(void *pPtr, __u8 *pBuffer, size_t uCount);
extern void rzr_ctrl_write(u8 *pBuffer, unsigned int uiSize);
extern bool rzr_ctrl_getslot(void);

/* Used in hid-razer-dongle.c */
void hid_razer_keyboard_connect(void);
void hid_razer_keyboard_disconnect(void);
bool hid_razer_keyboard_active(void);
void hid_razer_mouse_connect(void);
void hid_razer_mouse_disconnect(void);
bool hid_razer_mouse_active(void);
#endif
