#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/hid.h>

#include "hid-razer.h"
#include "RzrProtocol.h"

struct _Rzr_HeaderDecription
{
  u8 uStatus;
  u8 uTransactionID;
  u16 uPacketNum;
  u8 uProtocolType;
  u8 uDataSize;
  u8 uCommandClass;
  u8 uCommandID;
  u8 uData [82];
}  __attribute__ ((packed));

typedef struct _Rzr_HeaderDecription  Rzr_HeaderDescription;

/*
Routine description:

    This function computes a razer checksum.

Arguments:

    pBuffer - A razer command buffer
    uBufferSize - This is size of the command buffer

Return value:
    computed checksum
*/
u8 ForgeRcvr_ComputeChecksum(u8 *pBuffer,
			     u8 uBufferSize)
{
  int iIndex;
  u8 uSum = 0;
  int iLength;

  /*  Right now, the BufferSize can only be 90 bytes. */
  if (RZR_COMMAND_SIZE == uBufferSize) {

    /* The checksum is calculated after the transaction ID and up to the
       checksum itself. */
    iLength = RZR_COMMAND_SIZE - 1;
    for (iIndex = 2; iIndex < iLength; ++iIndex) {
      uSum ^= pBuffer[iIndex];
    }
  } else {
    printk("Command wrong length\n");
  }

  return uSum;
}

/* Submit the led reports */
void RzrSubmitReport(u8 *pBuffer, u8 uCount)
{
  Rzr_HeaderDescription sHeader;
  // bool bBusy = true;

  /*
    This the data payload for this command. Since this is a read command,
    the data payload is just the Razer header and nothing else in the payload
    beside the checksum.
  */
  memset(&sHeader, 0, sizeof(Rzr_HeaderDescription));
  sHeader.uStatus = 0;
  sHeader.uTransactionID = 1;
  sHeader.uPacketNum = 0;
  sHeader.uProtocolType = 0;
  sHeader.uDataSize = 2;
  sHeader.uCommandClass = 0;
  sHeader.uCommandID = 0x32;

  memcpy(&sHeader.uData [0], pBuffer, uCount);
  sHeader.uData [80] = ForgeRcvr_ComputeChecksum((u8 *)&sHeader,
						 sizeof(sHeader));
  rzr_ctrl_write((u8 *)&sHeader, sizeof(sHeader));
}


