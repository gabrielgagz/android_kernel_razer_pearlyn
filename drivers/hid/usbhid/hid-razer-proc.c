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

#include "hid-razer.h"

#define RZRSV2_PROC_NAME "turret_dongle"


static int priv_proc_sv2_output(struct seq_file *pSeq,
				void *pBuffer)
{
  DongleData *pDongle = NULL;

  if (NULL != pSeq) {
    pDongle = (DongleData *)pSeq->private;

    if (NULL != pDongle) {
      spin_lock(&pDongle->sLock);
      seq_printf(pSeq, "Interface 0 keyboard Active %d\n",
		 hid_razer_keyboard_active());
      seq_printf(pSeq, "Interface 1 mouse Active %d\n",
		 hid_razer_mouse_active());
      if (NULL != pDongle->controller [0].pPtr) {
	seq_printf(pSeq, "Interface 2 controller 1 active\n");
      } else {
	seq_printf(pSeq, "Interface 2 controller 1 not active\n");
      }

      if (NULL != pDongle->controller [1].pPtr) {
	seq_printf(pSeq, "Interface 2 controller 2 active\n");
      } else {
	seq_printf(pSeq, "Interface 2 controller 2 not active\n");
      }

      if (NULL != pDongle->controller [2].pPtr) {
	seq_printf(pSeq, "Interface 2 controller 3 active\n");
      } else {
	seq_printf(pSeq, "Interface 2 controller 3 not active\n");
      }

      if (NULL != pDongle->controller [3].pPtr) {
	seq_printf(pSeq, "Interface 2 controller 4 active\n");
      } else {
	seq_printf(pSeq, "Interface 2 controller 4 not active\n");
      }

      if (NULL != pDongle->pThread) {
	seq_printf(pSeq, "kill thread exists\n");
      } else {
	seq_printf(pSeq, "kill thread non existent\n");
      }

      seq_printf(pSeq, "kill thread active %d\n", pDongle->bConnected);

      seq_printf(pSeq, "Interface 2 Active %d\n",
		 pDongle->bUsed);

      seq_printf(pSeq, "Interrupt Messages %ld\n",
		 pDongle->ulInputMsg);

      seq_printf(pSeq, "Controller Create %ld\n",
		 pDongle->ulConnectCount);

      seq_printf(pSeq, "Controller Destroy %ld\n",
		 pDongle->ulDestroyCount);

      seq_printf(pSeq, "Timeout Count %ld\n",
		 pDongle->ulTimeoutCount);

      spin_unlock(&pDongle->sLock);
    } else {
      seq_printf(pSeq, "error\n");
    }
  }

  return 0;
}

#if 0
static unsigned int private_strhex(char nib)
{
  unsigned int rvalue;
  switch(nib)
    {
    case '0':
      rvalue = 0;
      break;
    case '1':
      rvalue = 1;
      break;
    case '2':
      rvalue = 2;
      break;
    case '3':
      rvalue = 3;
      break;
    case '4':
      rvalue = 4;
      break;
    case '5':
      rvalue = 5;
      break;
    case '6':
      rvalue = 6;
      break;
    case '7':
      rvalue = 7;
      break;
    case '8':
      rvalue = 8;
      break;
    case '9':
      rvalue = 9;
      break;
    case 'a':
    case 'A':
      rvalue = 0xa;
      break;
    case 'b':
    case 'B':
      rvalue = 0xb;
      break;
    case 'c':
    case 'C':
      rvalue = 0xc;
      break;
    case 'd':
    case 'D':
      rvalue = 0xd;
      break;
    case 'e':
    case 'E':
      rvalue = 0xe;
      break;
    case 'f':
    case 'F':
      rvalue = 0xf;
      break;
    default:
      rvalue = 0;
      break;
    }

  return rvalue;
}
#endif
#if 0
static ssize_t priv_proc_sv2_input(struct file *pFile,
				   const char __user *pBuffer,
				   size_t iCount,
				   loff_t *pPos)
{

  //DongleData *pDongle = NULL;
  char tmpbuff [64];
  size_t rvalue = iCount;
  int ilength;
  EventData event;
  int consume;
  int loop;

  if (NULL != pFile) {
    if (NULL != pBuffer) {
      if (iCount > 0) {

	ilength = rvalue;
	if (ilength >= sizeof(tmpbuff)) {
	  ilength = sizeof(tmpbuff);
	}
	memset(tmpbuff, 0, sizeof(tmpbuff));
	if (0 == copy_from_user(tmpbuff, pBuffer, ilength)) {
	  consume = 0;
	  for (loop = 0; loop < sizeof(event); loop += 1) {
	    if ((ilength - consume) >= 2) {
	      event.byte [loop] = (private_strhex(tmpbuff [consume]) << 4) +
		private_strhex(tmpbuff [consume + 1]);
	      consume += 2;
	    } else if ((ilength - consume) >= 1) {
	      event.byte [loop] = private_strhex(tmpbuff [consume]);
	      consume += 1;
	    } else {
	      event.byte [loop] = 0;
	    }
	  }

	  rzr_ctrl_process(&event, DONGLE_THREAD_HEARTBEAT);
	}
      }
    }
  }

  return rvalue;
}
#endif

static int priv_proc_sv2_open(struct inode *pINode, struct file *pFile)
{
  return single_open(pFile, priv_proc_sv2_output, PDE_DATA(pINode));
}

static const struct file_operations private_proc_sv2_fops = {
  .owner   = THIS_MODULE,
  .open    = priv_proc_sv2_open,
  .read    = seq_read,
   //.write   = priv_proc_sv2_input,
  .llseek  = seq_lseek,
  .release = single_release,
};

/* Proc entry point */
void rzr_proc_init(void *pPtr)
{
  DongleData *pDongle = (DongleData *)pPtr;

  if (NULL != pDongle) {
    spin_lock_init(&pDongle->sLock);
    pDongle->pProcEntry = proc_create_data(RZRSV2_PROC_NAME,
					0666,
					NULL,
					&private_proc_sv2_fops,
					pPtr);
  }
}

void rzr_proc_deinit(void *pPtr)
{
  DongleData *pDongle = (DongleData *)pPtr;;

  if (NULL != pDongle) {
    if (NULL != pDongle->pProcEntry) {
      /* Don't remove it, if we didn't have it. */
      remove_proc_entry(RZRSV2_PROC_NAME, NULL);
      pDongle->pProcEntry = NULL;
    }
  }
}
