/*-------------------------------------------------------------

usbkeyboard.c -- Usb keyboard support(boot protocol)

Copyright (C) 2008, 2009
DAVY Guillaume davyg2@gmail.com
dhewg

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <string.h>
#include <malloc.h>

#include <gccore.h>
#include <ogc/usb.h>

#include <wiikeyboard/usbkeyboard.h>

#define	HEAP_SIZE					4096
#define DEVLIST_MAXSIZE				16
#define MAX_KEYBOARDS				4
#define MAX_SKIPPED					16
#define KEY_ERROR					0x01
#define MAXKEYCODE					6

#define USB_MOD_CTRL_L				0x01
#define USB_MOD_SHIFT_L				0x02
#define USB_MOD_ALT_L				0x04
#define USB_MOD_META_L				0x08
#define USB_MOD_CTRL_R				0x10
#define USB_MOD_SHIFT_R				0x20
#define USB_MOD_ALT_R				0x40
#define USB_MOD_META_R				0x80

struct ukbd_data {
	u16	modifiers;
	u8	keycode[MAXKEYCODE];
} ATTRIBUTE_PACKED;

/*
 * One attached keyboard. Reports are read with one asynchronous transfer per
 * keyboard, so a keyboard that never sends anything (eg. the keyboard
 * interface of a wireless receiver with only a mouse paired) cannot hold up
 * the others, or the search for new ones.
 *
 * A slot is never freed or reused while a transfer is still outstanding
 * (`reading`), as the transfer completes into `buffer`. The buffers are kept
 * for the lifetime of the module.
 */
struct ukbd {
	bool used;				//!< slot holds an open device
	bool connected;			//!< open and not removed
	volatile bool removed;	//!< removal notification arrived (set from the USB callback)
	volatile bool reading;	//!< an interrupt read is outstanding
	volatile bool ready;	//!< a read completed, see `result`
	volatile s32 result;

	s32 fd;
	s32 device_id;
	u16 vid;
	u16 pid;

	struct ukbd_data sc_ndata;
	struct ukbd_data sc_odata;

	u8 leds;

	u8 configuration;
	u32 interface;
	u32 altInterface;

	u8 ep;
	u32 ep_size;

	u8 *buffer;				//!< 8 bytes in the IOS heap, the target of the read
};

//! A HID device that was looked at and is not a keyboard, so it is not opened again
struct skipped {
	s32 device_id;
	u16 vid;
	u16 pid;
};

static s32 hId = -1;
static struct ukbd _kbds[MAX_KEYBOARDS];
static struct skipped _skipped[MAX_SKIPPED];
static int _skippedCount = 0;
static eventcallback _cb = NULL;

static u8 _ukbd_mod_map[][2] = {
	{ USB_MOD_CTRL_L, 224 },
	{ USB_MOD_SHIFT_L, 225 },
	{ USB_MOD_ALT_L, 226 },
	{ USB_MOD_META_L, 227 },
	{ USB_MOD_CTRL_R, 228 },
	{ USB_MOD_SHIFT_R, 229 },
	{ USB_MOD_ALT_R, 230 },
	{ USB_MOD_META_R, 231 }
};

#define MODMAPSIZE (sizeof(_ukbd_mod_map)/sizeof(_ukbd_mod_map[0]))

static void _submit(USBKeyboard_eventType type, u8 code)
{
	if (!_cb)
		return;

	USBKeyboard_event ev;
	ev.type = type;
	ev.keyCode = code;

	_cb(ev);
}

//Callback when the keyboard is disconnected (runs in the USB callback context,
//so it only flags the keyboard: the events are posted from USBKeyboard_Scan)
static s32 _disconnect(s32 retval, void *data)
{
	struct ukbd *kbd = (struct ukbd *) data;

	(void) retval;

	if (kbd)
		kbd->removed = true;

	return 1;
}

//Get the protocol, 0=boot protocol and 1=report protocol
static s32 _get_protocol(struct ukbd *kbd)
{
	s32 protocol;
	u8 *buffer = 0;

	if(!kbd || kbd->fd==-1) return -1;

	buffer = iosAlloc(hId, 1);

	if (buffer == NULL)
		return -1;

	USB_WriteCtrlMsg(kbd->fd, USB_REQTYPE_INTERFACE_GET, USB_REQ_GETPROTOCOL, 0, kbd->interface, 1, buffer);

	protocol = *buffer;
	iosFree(hId, buffer);

	return protocol;
}

//Modify the protocol, 0=boot protocol and 1=report protocol
static s32 _set_protocol(struct ukbd *kbd, u8 protocol)
{
	if(!kbd || kbd->fd==-1) return -1;
	return USB_WriteCtrlMsg(kbd->fd, USB_REQTYPE_INTERFACE_SET, USB_REQ_SETPROTOCOL, protocol, kbd->interface, 0, NULL);
}

//Completion of an interrupt read (runs in the USB callback context)
static s32 _read_cb(s32 result, void *usrdata)
{
	struct ukbd *kbd = (struct ukbd *) usrdata;

	if (result >= 8)
		memcpy(&kbd->sc_ndata, kbd->buffer, 8);

	kbd->result = result;
	kbd->ready = true;
	kbd->reading = false;

	return 0;
}

//Start a read of an input report from the interrupt pipe
static void _start_read(struct ukbd *kbd)
{
	kbd->ready = false;
	kbd->reading = true;

	if (USB_ReadIntrMsgAsync(kbd->fd, kbd->ep, 8, kbd->buffer, &_read_cb, kbd) < 0)
		kbd->reading = false;	// not started: tried again next scan
}

#if 0
//Get an input report from control pipe
static s32 _get_output_report(struct ukbd *kbd, u8 *leds)
{
	u8 *buffer = 0;
	if(!kbd || kbd->fd==-1) return -1;
	buffer = iosAlloc(hId, 1);

	if (buffer == NULL)
		return -1;

	s32 ret = USB_WriteCtrlMsg(kbd->fd, USB_REQTYPE_INTERFACE_GET, USB_REQ_GETREPORT, USB_REPTYPE_OUTPUT << 8, kbd->interface, 1, buffer);

	memcpy(leds, buffer, 1);
	iosFree(hId, buffer);

	return ret;
}
#endif

//Set an input report to control pipe
static s32 _set_output_report(struct ukbd *kbd)
{
	u8 *buffer = 0;
	if(!kbd || kbd->fd==-1) return -1;
	buffer = iosAlloc(hId, 1);

	if (buffer == NULL)
		return -1;

	memcpy(buffer, &kbd->leds, 1);
	s32 ret = USB_WriteCtrlMsg(kbd->fd, USB_REQTYPE_INTERFACE_SET, USB_REQ_SETREPORT, USB_REPTYPE_OUTPUT << 8, kbd->interface, 1, buffer);

	iosFree(hId, buffer);

	return ret;
}

static bool _is_open(const usb_device_entry *e)
{
	int i;

	for (i = 0; i < MAX_KEYBOARDS; i++) {
		const struct ukbd *kbd = &_kbds[i];

		if (kbd->used && kbd->device_id == e->device_id && kbd->vid == e->vid && kbd->pid == e->pid)
			return true;
	}

	return false;
}

static bool _is_skipped(const usb_device_entry *e)
{
	int i;

	for (i = 0; i < _skippedCount; i++) {
		if (_skipped[i].device_id == e->device_id && _skipped[i].vid == e->vid && _skipped[i].pid == e->pid)
			return true;
	}

	return false;
}

static void _skip(const usb_device_entry *e)
{
	if (_skippedCount >= MAX_SKIPPED)
		return;	// full: the device is looked at again next time

	_skipped[_skippedCount].device_id = e->device_id;
	_skipped[_skippedCount].vid = e->vid;
	_skipped[_skippedCount].pid = e->pid;
	_skippedCount++;
}

//Forget the skipped devices that are no longer attached, so that a different
//device that gets the same id later is looked at
static void _prune_skipped(const usb_device_entry *list, u8 count)
{
	int i, j;

	for (i = 0; i < _skippedCount; ) {
		bool present = false;

		for (j = 0; j < count; j++) {
			if (list[j].device_id == _skipped[i].device_id && list[j].vid == _skipped[i].vid && list[j].pid == _skipped[i].pid) {
				present = true;
				break;
			}
		}

		if (present)
			i++;
		else
			_skipped[i] = _skipped[--_skippedCount];
	}
}

//Close a keyboard that went away, releasing what it was holding down
static void _drop(struct ukbd *kbd)
{
	int i, j;

	if (kbd->connected) {
		kbd->connected = false;

		for (i = 0; i < MODMAPSIZE; ++i) {
			if (kbd->sc_odata.modifiers & _ukbd_mod_map[i][0])
				_submit(USBKEYBOARD_RELEASED, _ukbd_mod_map[i][1]);
		}

		for (j = 0; j < MAXKEYCODE; j++) {
			if (kbd->sc_odata.keycode[j] > 3)
				_submit(USBKEYBOARD_RELEASED, kbd->sc_odata.keycode[j]);
		}

		_submit(USBKEYBOARD_DISCONNECTED, 0);
	}

	if (kbd->fd != -1)
		USB_CloseDevice(&kbd->fd);

	kbd->fd = -1;
	kbd->removed = false;
}

//Open the keyboards that are attached and not open yet. Returns how many were
//opened, or a negative value if there was none to open.
//Called regularly: devices that were looked at and are not keyboards are
//remembered, so only a device that is new gets opened.
s32 USBKeyboard_Open(const eventcallback cb)
{
	usb_device_entry *buffer;
	u8 device_count, i, conf;
	u16 vid, pid;
	s32 opened = 0;
	u32 iConf, iInterface, iEp;
	usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	if (hId < 0)
		return -1;

	buffer = (usb_device_entry*)iosAlloc(hId, DEVLIST_MAXSIZE * sizeof(usb_device_entry));
	if(buffer == NULL)
		return -1;

	memset(buffer, 0, DEVLIST_MAXSIZE * sizeof(usb_device_entry));
	device_count = 0;

	if (USB_GetDeviceList(buffer, DEVLIST_MAXSIZE, USB_CLASS_HID, &device_count) < 0)
	{
		iosFree(hId, buffer);
		return -2;
	}

	_cb = cb;
	_prune_skipped(buffer, device_count);

	for (i = 0; i < device_count; i++)
	{
		struct ukbd *kbd = NULL;
		bool found = false;
		int k;
		s32 fd = 0;

		vid = buffer[i].vid;
		pid = buffer[i].pid;

		if ((vid == 0) || (pid == 0))
			continue;

		if (_is_open(&buffer[i]) || _is_skipped(&buffer[i]))
			continue;

		for (k = 0; k < MAX_KEYBOARDS; k++) {
			if (!_kbds[k].used) {
				kbd = &_kbds[k];
				break;
			}
		}

		if (!kbd)
			break;	// every slot is in use

		if (USB_OpenDevice(buffer[i].device_id, vid, pid, &fd) < 0)
			continue;

		if (USB_GetDescriptors(fd, &udd) < 0) {
			USB_CloseDevice(&fd);
			continue;
		}

		for(iConf = 0; iConf < udd.bNumConfigurations && !found; iConf++)
		{
			ucd = &udd.configurations[iConf];

			for(iInterface = 0; iInterface < ucd->bNumInterfaces && !found; iInterface++)
			{
				uid = &ucd->interfaces[iInterface];

				if ((uid->bInterfaceClass == USB_CLASS_HID) &&
					(uid->bInterfaceSubClass == USB_SUBCLASS_BOOT) &&
					(uid->bInterfaceProtocol== USB_PROTOCOL_KEYBOARD))
				{
					for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
					{
						ued = &uid->endpoints[iEp];

						if (ued->bmAttributes != USB_ENDPOINT_INTERRUPT)
							continue;

						if (!(ued->bEndpointAddress & USB_ENDPOINT_IN))
							continue;

						kbd->configuration = ucd->bConfigurationValue;
						kbd->interface = uid->bInterfaceNumber;
						kbd->altInterface = uid->bAlternateSetting;

						kbd->ep = ued->bEndpointAddress;
						kbd->ep_size = ued->wMaxPacketSize;

						found = true;

						break;
					}
				}
			}
		}

		USB_FreeDescriptors(&udd);

		if (!found) {
			USB_CloseDevice(&fd);
			_skip(&buffer[i]);
			continue;
		}

		kbd->fd = fd;
		kbd->device_id = buffer[i].device_id;
		kbd->vid = vid;
		kbd->pid = pid;
		kbd->leds = 0;
		kbd->removed = false;
		kbd->ready = false;
		memset(&kbd->sc_ndata, 0, sizeof(kbd->sc_ndata));
		memset(&kbd->sc_odata, 0, sizeof(kbd->sc_odata));

		if (!kbd->buffer)
			kbd->buffer = (u8*)iosAlloc(hId, 8);

		if (!kbd->buffer ||
			USB_GetConfiguration(fd, &conf) < 0 ||
			(conf != kbd->configuration && USB_SetConfiguration(fd, kbd->configuration) < 0) ||
			(kbd->altInterface != 0 && USB_SetAlternativeInterface(fd, kbd->interface, kbd->altInterface) < 0) ||
			(_get_protocol(kbd) != 0 && (_set_protocol(kbd, 0) < 0 || _get_protocol(kbd) == 1)) ||
			USB_DeviceRemovalNotifyAsync(fd, &_disconnect, kbd) < 0)
		{
			USB_CloseDevice(&fd);
			kbd->fd = -1;
			continue;	// looked at again next time
		}

		kbd->connected = true;
		kbd->used = true;
		opened++;
	}

	iosFree(hId, buffer);

	return opened > 0 ? opened : -3;
}

//Close all the keyboards
void USBKeyboard_Close(void)
{
	int i;

	for (i = 0; i < MAX_KEYBOARDS; i++) {
		struct ukbd *kbd = &_kbds[i];

		if (kbd->used && kbd->fd != -1)
			USB_CloseDevice(&kbd->fd);

		kbd->fd = -1;
		kbd->connected = false;
		kbd->removed = false;
		kbd->used = false;
	}

	_skippedCount = 0;
}

//init the ioheap
s32 USBKeyboard_Initialize(void)
{
	if (hId > 0)
		return 0;

	hId = iosCreateHeap(HEAP_SIZE);

	if (hId < 0)
		return IPC_ENOHEAP;

	return IPC_OK;
}

s32 USBKeyboard_Deinitialize(void)
{
	return IPC_OK;
}


//True while at least one keyboard is attached
bool USBKeyboard_IsConnected(void) {
	int i;

	for (i = 0; i < MAX_KEYBOARDS; i++) {
		if (_kbds[i].used && _kbds[i].connected && !_kbds[i].removed)
			return true;
	}

	return false;
}

//Generate events for the callback function from the reports that have arrived,
//and start the next read on each keyboard. Never waits for a keyboard.
s32 USBKeyboard_Scan(void)
{
	int i, j, k, index;
	bool any = false;

	for (k = 0; k < MAX_KEYBOARDS; k++) {
		struct ukbd *kbd = &_kbds[k];

		if (!kbd->used)
			continue;

		if (kbd->connected && kbd->removed)
			_drop(kbd);

		if (!kbd->connected) {
			// free the slot once the read that was outstanding has completed
			if (!kbd->reading)
				kbd->used = false;
			continue;
		}

		any = true;

		if (kbd->ready) {
			kbd->ready = false;

			if (kbd->result >= 8) {
				kbd->sc_ndata.modifiers = (kbd->sc_ndata.modifiers << 8) | (kbd->sc_ndata.modifiers >> 8);

				if (kbd->sc_ndata.keycode[0] != KEY_ERROR) {
					if (kbd->sc_ndata.modifiers != kbd->sc_odata.modifiers) {
						for (i = 0; i < MODMAPSIZE; ++i) {
							if ((kbd->sc_odata.modifiers & _ukbd_mod_map[i][0])
								&& !(kbd->sc_ndata.modifiers & _ukbd_mod_map[i][0]))
								_submit(USBKEYBOARD_RELEASED, _ukbd_mod_map[i][1]);
							else if ((kbd->sc_ndata.modifiers & _ukbd_mod_map[i][0])
								&& !(kbd->sc_odata.modifiers & _ukbd_mod_map[i][0]))
								_submit(USBKEYBOARD_PRESSED, _ukbd_mod_map[i][1]);
						}
					}

					for (i = 0; i < MAXKEYCODE; i++) {
						if (kbd->sc_odata.keycode[i] > 3) {
							index = -1;

							for (j = 0; j < MAXKEYCODE; j++) {
								if (kbd->sc_odata.keycode[i] == kbd->sc_ndata.keycode[j]) {
									index = j;
									break;
								}
							}

							if (index == -1)
								_submit(USBKEYBOARD_RELEASED, kbd->sc_odata.keycode[i]);
						}

						if (kbd->sc_ndata.keycode[i] > 3) {
							index = -1;

							for (j = 0; j < MAXKEYCODE; j++) {
								if (kbd->sc_ndata.keycode[i] == kbd->sc_odata.keycode[j]) {
									index = j;
									break;
								}
							}

							if (index == -1)
								_submit(USBKEYBOARD_PRESSED, kbd->sc_ndata.keycode[i]);
						}
					}

					kbd->sc_odata = kbd->sc_ndata;
				}
			}
		}

		if (!kbd->reading && !kbd->removed)
			_start_read(kbd);
	}

	return any ? 0 : -1;
}

//Turn on/off a led on every keyboard
s32 USBKeyboard_SetLed(const USBKeyboard_led led, bool on)
{
	int i;
	s32 ret = -1;

	for (i = 0; i < MAX_KEYBOARDS; i++) {
		struct ukbd *kbd = &_kbds[i];

		if (!kbd->used || !kbd->connected)
			continue;

		if (on)
			kbd->leds = kbd->leds | (1 << led );
		else
			kbd->leds = kbd->leds & (255 ^ (1 << led));

		ret = (_set_output_report(kbd) < 0 && ret != 1) ? -2 : 1;
	}

	return ret;
}

//Toggle a led on every keyboard
s32 USBKeyboard_ToggleLed(const USBKeyboard_led led)
{
	int i;
	s32 ret = -1;

	for (i = 0; i < MAX_KEYBOARDS; i++) {
		struct ukbd *kbd = &_kbds[i];

		if (!kbd->used || !kbd->connected)
			continue;

		kbd->leds = kbd->leds ^ (1 << led);

		ret = (_set_output_report(kbd) < 0 && ret != 1) ? -2 : 1;
	}

	return ret;
}
