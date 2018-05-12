#
# Makefile for the USB input drivers
#

# Multipart objects.
usbhid-y	:= hid-core.o hid-quirks.o

# Optional parts of multipart objects.

ifeq ($(CONFIG_USB_HIDDEV),y)
	usbhid-y	+= hiddev.o
endif
ifeq ($(CONFIG_HID_PID),y)
	usbhid-y	+= hid-pidff.o
endif

obj-$(CONFIG_USB_HID)		+= usbhid.o
obj-$(CONFIG_USB_KBD)		+= usbkbd.o
obj-$(CONFIG_USB_MOUSE)		+= usbmouse.o
obj-$(CONFIG_USB_RAZER_DONGLE)	+= hid-razer-ctrl.o hid-razer-proc.o hid-razer-controller.o RzrProtocol.o

