#include <stdbool.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "kbd.h"

#define LSB(n) (n & 255)
#define MSB(n) ((n >> 8) & 255)
#define MAX(a, b) ((a) > (b) ? a : b)
#define MIN(a, b) ((a) < (b) ? a : b)

#define GET_STATUS			0
#define CLEAR_FEATURE		1
#define SET_FEATURE			3
#define SET_ADDRESS			5
#define GET_DESCRIPTOR		6
#define GET_CONFIGURATION	8
#define SET_CONFIGURATION   9
#define GET_INTERFACE	    10
#define SET_INTERFACE		11

#define HID_GET_REPORT			1
#define HID_GET_IDLE			2
#define HID_GET_PROTOCOL		3
#define HID_SET_REPORT			9
#define HID_SET_IDLE			10
#define HID_SET_PROTOCOL		11

#define EP_TYPE_CONTROL			0x00
#define EP_TYPE_BULK_IN			0x81
#define EP_TYPE_BULK_OUT		0x80
#define EP_TYPE_INTERRUPT_IN		0xC1
#define EP_TYPE_INTERRUPT_OUT		0xC0
#define EP_TYPE_ISOCHRONOUS_IN		0x41
#define EP_TYPE_ISOCHRONOUS_OUT		0x40

#define EP_SINGLE_BUFFER		0x02
#define EP_DOUBLE_BUFFER		0x06

#define EP_SIZE(s)	((s) == 64 ? 0x30 :	\
			((s) == 32 ? 0x20 :	\
			((s) == 16 ? 0x10 :	\
			             0x00)))

#define ENDPOINT0_SIZE		32

#define KEYBOARD_INTERFACE	0
#define KEYBOARD_ENDPOINT	3
#define KEYBOARD_SIZE		8
#define KEYBOARD_BUFFER		EP_DOUBLE_BUFFER

static const uint8_t PROGMEM endpoint_config_table[] = {
	0,
	0,
	1, EP_TYPE_INTERRUPT_IN,  EP_SIZE(KEYBOARD_SIZE) | KEYBOARD_BUFFER,
	0
};

#define VENDOR_ID		0x16C0
#define PRODUCT_ID		0x047C
#define NUM_DESC_LIST        1

static const uint8_t PROGMEM device_descriptor[] = {
	18,					// bLength
	1,					// bDescriptorType
	0x00, 0x02,				// bcdUSB
	0,					// bDeviceClass
	0,					// bDeviceSubClass
	0,					// bDeviceProtocol
	ENDPOINT0_SIZE,				// bMaxPacketSize0
	LSB(VENDOR_ID), MSB(VENDOR_ID),		// idVendor
	LSB(PRODUCT_ID), MSB(PRODUCT_ID),	// idProduct
	0x00, 0x01,				// bcdDevice
	0,					// iManufacturer
	0,					// iProduct
	0,					// iSerialNumber
	1					// bNumConfigurations
};

// Keyboard Protocol 1, HID 1.11 spec, Appendix B, page 59-60
static const uint8_t PROGMEM keyboard_hid_report_desc[] = {
        0x05, 0x01,          // Usage Page (Generic Desktop),
        0x09, 0x06,          // Usage (Keyboard),
        0xA1, 0x01,          // Collection (Application),
        0x75, 0x01,          //   Report Size (1),
        0x95, 0x08,          //   Report Count (8),
        0x05, 0x07,          //   Usage Page (Key Codes),
        0x19, 0xE0,          //   Usage Minimum (224),
        0x29, 0xE7,          //   Usage Maximum (231),
        0x15, 0x00,          //   Logical Minimum (0),
        0x25, 0x01,          //   Logical Maximum (1),
        0x81, 0x02,          //   Input (Data, Variable, Absolute), ;Modifier byte
        0x95, 0x01,          //   Report Count (1),
        0x75, 0x08,          //   Report Size (8),
        0x81, 0x03,          //   Input (Constant),                 ;Reserved byte
        0x95, 0x05,          //   Report Count (5),
        0x75, 0x01,          //   Report Size (1),
        0x05, 0x08,          //   Usage Page (LEDs),
        0x19, 0x01,          //   Usage Minimum (1),
        0x29, 0x05,          //   Usage Maximum (5),
        0x91, 0x02,          //   Output (Data, Variable, Absolute), ;LED report
        0x95, 0x01,          //   Report Count (1),
        0x75, 0x03,          //   Report Size (3),
        0x91, 0x03,          //   Output (Constant),                 ;LED report padding
        0x95, 0x06,          //   Report Count (6),
        0x75, 0x08,          //   Report Size (8),
        0x15, 0x00,          //   Logical Minimum (0),
        0x25, 0x68,          //   Logical Maximum(104),
        0x05, 0x07,          //   Usage Page (Key Codes),
        0x19, 0x00,          //   Usage Minimum (0),
        0x29, 0x68,          //   Usage Maximum (104),
        0x81, 0x00,          //   Input (Data, Array),
        0xc0                 // End Collection
};

#define CONFIG1_DESC_SIZE        (9+9+9+7)
#define KEYBOARD_HID_DESC_OFFSET (9+9)
static const uint8_t PROGMEM configuration_descriptor[CONFIG1_DESC_SIZE] = {
	// configuration descriptor, USB spec 9.6.3, page 264-266, Table 9-10
	9, 					// bLength;
	2,					// bDescriptorType;
	LSB(CONFIG1_DESC_SIZE),			// wTotalLength
	MSB(CONFIG1_DESC_SIZE),
	1,					// bNumInterfaces
	1,					// bConfigurationValue
	0,					// iConfiguration
	0xC0,					// bmAttributes
	50,					// bMaxPower
	// interface descriptor, USB spec 9.6.5, page 267-269, Table 9-12
	9,					// bLength
	4,					// bDescriptorType
	KEYBOARD_INTERFACE,			// bInterfaceNumber
	0,					// bAlternateSetting
	1,					// bNumEndpoints
	0x03,					// bInterfaceClass (0x03 = HID)
	0x01,					// bInterfaceSubClass (0x01 = Boot)
	0x01,					// bInterfaceProtocol (0x01 = Keyboard)
	0,					// iInterface
	// HID interface descriptor, HID 1.11 spec, section 6.2.1
	9,					// bLength
	0x21,					// bDescriptorType
	0x11, 0x01,				// bcdHID
	0,					// bCountryCode
	1,					// bNumDescriptors
	0x22,					// bDescriptorType
	sizeof(keyboard_hid_report_desc),	// wDescriptorLength
	0,
	// endpoint descriptor, USB spec 9.6.6, page 269-271, Table 9-13
	7,					// bLength
	5,					// bDescriptorType
	KEYBOARD_ENDPOINT | 0x80,		// bEndpointAddress
	0x03,					// bmAttributes (0x03=intr)
	KEYBOARD_SIZE, 0,			// wMaxPacketSize
	1					// bInterval
};

static volatile uint8_t current_configuration = 0;
static volatile uint8_t keyboard_modifier_keys = 0;
static volatile uint8_t keyboard_keys[6] = {0, 0, 0, 0, 0, 0};
static volatile uint8_t keyboard_idle_config = 125;
static volatile uint8_t keyboard_idle_count = 0;
static volatile uint8_t keyboard_protocol = 1;
static volatile uint8_t keyboard_leds = 0;

void kbd_init(void)
{
    current_configuration = 0;
    DDRD |= (1<<5);
    PORTD |= (1<<5);

    CLKPR = 0x80;
    CLKPR = 0;

    UHWCON = (1<<UVREGE);
    USBCON = ((1<<USBE) | (1<<FRZCLK) | (0<<OTGPADE) | (0<<VBUSTE));
    //PLLCSR = 0;
    //PLLFRQ = ((0<<PINMUX) | (1<<PLLUSB) | (0<<PLLTM1) | (0<<PLLTM0) | (1<<PDIV3) | (0<<PDIV2) | (1<<PDIV1) | (1<<PDIV0));
    PLLCSR = ((1<<PINDIV) | (1<<PLLE) | (0<<PLOCK));
    while ((PLLCSR & (1<<PLOCK)) == 0);
    USBCON = ((1<<USBE) | (0<<FRZCLK) | (1<<OTGPADE) | (0<<VBUSTE));
    UDCON = 0;
    UDIEN = ((1<<EORSTE) | (1<<SOFE));
    sei();
}

ISR(USB_GEN_vect)
{
    static uint8_t div4 = 0;
    uint8_t intbits = UDINT;
    UDINT = 0;
    if (intbits & (1<<EORSTI)) {
        UENUM = 0;
        UECONX = 1;
        UECFG0X = EP_TYPE_CONTROL;
        UECFG1X = EP_SIZE(ENDPOINT0_SIZE) | EP_SINGLE_BUFFER;
        UEIENX = (1<<RXSTPE);
        current_configuration = 0;
    }
    if ((intbits & (1<<SOFI) && (current_configuration > 0))) {
        if ((current_configuration > 0) && ((++div4 & 3) == 0)) {
            UENUM = KEYBOARD_ENDPOINT;
            if (UEINTX & (1<<RWAL)) {
                keyboard_idle_count++;
                if (keyboard_idle_count == keyboard_idle_config) {
                    keyboard_idle_count = 0;
                    UEDATX = keyboard_modifier_keys;
                    UEDATX = 0;
                    for (uint8_t i = 0; i < 6; i++) {
                        UEDATX = keyboard_keys[i];
                    }
                    UEINTX = 0x3A;
                }
            }
        }
    }
}

ISR(USB_COM_vect)
{
    UENUM = 0;
    uint8_t intbits = UEINTX;
    if (intbits & (1<<RXSTPI)) {
        uint8_t bmRequestType = UEDATX;
        uint8_t bRequest = UEDATX;
        uint16_t wValue = UEDATX;
        wValue |= (UEDATX << 8);
        uint16_t wIndex = UEDATX;
        wIndex |= (UEDATX << 8);
        uint16_t wLength = UEDATX;
        wLength |= (UEDATX << 8);
        UEINTX = ~((1<<RXSTPI) | (1<<RXOUTI) | (1<<TXINI));
        if (bRequest == GET_DESCRIPTOR) {
            const uint8_t *p;
            uint16_t left;
            if (wValue == 0x0100 && wIndex == 0x0000) {
                p = device_descriptor;
                left = MIN(wLength, sizeof(device_descriptor));
            } else if (wValue == 0x0200 && wIndex == 0x0000) {
                p = configuration_descriptor;
                left = MIN(wLength, sizeof(configuration_descriptor));
            } else if (wValue == 0x2200 && wIndex == 0x0000) {
                p = keyboard_hid_report_desc;
                left = MIN(wLength, sizeof(keyboard_hid_report_desc));
            } else if (wValue == 0x2100 && wIndex == 0x0000) {
                p = configuration_descriptor + KEYBOARD_HID_DESC_OFFSET;
                left = MIN(wLength, 9);
            } else {
                UECONX = ((1<<STALLRQ) | (1<<EPEN));
                return;
            }
            while (left > 0) {
                while ((UEINTX & (1<<TXINI)) == 0);
                uint16_t n = MIN(left, ENDPOINT0_SIZE);
                for (uint16_t i = 0; i < n; i++) {
                    UEDATX = pgm_read_byte(p);
                    p++;
                }
                left -= n;
                UEINTX &= ~(1<<TXINI);
            }
            return;
        } else if (bRequest == SET_ADDRESS) {
            UEINTX &= ~(1<<TXINI);
            while ((UEINTX & (1<<TXINI)) == 0);
            UDADDR = wValue | (1<<ADDEN);
            return;
        } else if (bRequest == SET_CONFIGURATION && bmRequestType == 0) {
            current_configuration = wValue;
            UEINTX &= ~(1<<TXINI);
            UENUM = 1;
            UECONX = 0;
            UENUM = 2;
            UECONX = 0;
            UENUM = 3;
            UECONX = 1;
            UECFG0X = EP_TYPE_INTERRUPT_IN;
            UECFG1X = EP_SIZE(KEYBOARD_SIZE) | KEYBOARD_BUFFER;
            UENUM = 4;
            UECONX = 0;
            UERST = ((0<<EPRST6) | (0<<EPRST5) | (1<<EPRST4) | (1<<EPRST3) | (1<<EPRST2) | (1<<EPRST1) | (0<<EPRST0));
            UERST = 0;
            return;
        } else if (bRequest == GET_CONFIGURATION && bmRequestType == 0x80) {
            while ((UEINTX & (1<<TXINI)) == 0);
            UEDATX = current_configuration;
            UEINTX &= ~(1<<TXINI);
            return;
        } else if (bRequest == GET_STATUS) {
            while ((UEINTX & (1<<TXINI)) == 0);
            if (bmRequestType == 0x82) {
                UENUM = wIndex;
                bool halted = (UECONX & (1<<STALLRQ)) != 0;
                UEDATX = halted ? 1 : 0;
                UEDATX = 0;
            } else {
                UEDATX = 0;
                UEDATX = 0;
            }
            UEINTX = ~(1<<TXINI);
            return;
        } else if ((bRequest == CLEAR_FEATURE || bRequest == SET_FEATURE) && bmRequestType == 0x02 && wValue == 0) {
            uint8_t i = wIndex & 0x007f;
            if (i >= 1 && i <= 4) {
                UEINTX = ~(1<<TXINI);
                UENUM = i;
                if (bRequest == SET_FEATURE) {
                    UECONX = ((1<<STALLRQ) | (1<<EPEN));
                } else {
                    UECONX = ((1<<STALLRQC) | (1<<EPEN));
                    UERST = (1 << i);
                    UERST = 0;
                }
                return;
            }
        } else if (wIndex == KEYBOARD_INTERFACE) {
            if (bmRequestType == 0xA1) {
                if (bRequest == HID_GET_REPORT) {
                    while ((UEINTX & (1<<TXINI)) == 0);
                    UEDATX = keyboard_modifier_keys;
                    UEDATX = 0;
                    for (uint8_t i = 0; i < 6; i++) {
                        UEDATX = keyboard_keys[i];
                    }
                    UEINTX = ~(1<<TXINI);
                    return;
                } else if (bRequest == HID_GET_IDLE) {
                    while ((UEINTX & (1<<TXINI)) == 0);
                    UEDATX = keyboard_idle_config;
                    UEINTX = ~(1<<TXINI);
                    return;
                } else if (bRequest == HID_GET_PROTOCOL) {
                    while ((UEINTX & (1<<TXINI)) == 0);
                    UEDATX = keyboard_protocol;
                    UEINTX = ~(1<<TXINI);
                    return;
                }
            } else if (bmRequestType == 0x21) {
                if (bRequest == HID_SET_REPORT) {
                    while ((UEINTX & (1<<RXOUTI)) == 0);
                    keyboard_leds = UEDATX;
                    UEINTX = ~(1<<RXOUTI);
                    UEINTX = ~(1<<TXINI);
                    return;
                } else if (bRequest == HID_SET_IDLE) {
                    keyboard_idle_config = (wValue >> 8);
                    keyboard_idle_count = 0;
                    UEINTX = ~(1<<TXINI);
                    return;
                } else if (bRequest == HID_SET_PROTOCOL) {
                    keyboard_protocol = wValue;
                    UEINTX = ~(1<<TXINI);
                    return;
                }
            }
        }
    }
    UECONX = ((1<<STALLRQ) | (1<<EPEN));
}