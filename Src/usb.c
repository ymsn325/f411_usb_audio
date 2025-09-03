#include "usb.h"
#include <stddef.h>
#include <stdint.h>
#include <stm32f411xe.h>

#define USB USB_OTG_FS
#define USB_DEV                                                                \
  ((USB_OTG_DeviceTypeDef *)((uint32_t)USB_OTG_FS_PERIPH_BASE +                \
                             USB_OTG_DEVICE_BASE))
#define USB_INEP                                                               \
  ((USB_OTG_INEndpointTypeDef *)((uint32_t)USB_OTG_FS_PERIPH_BASE +            \
                                 USB_OTG_IN_ENDPOINT_BASE))
#define USB_OUTEP                                                              \
  ((USB_OTG_OUTEndpointTypeDef *)((uint32_t)USB_OTG_FS_PERIPH_BASE +           \
                                  USB_OTG_OUT_ENDPOINT_BASE))
#define USB_FIFO(ep)                                                           \
  ((uint32_t *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_FIFO_BASE +                   \
                ((ep) * USB_OTG_FIFO_SIZE)))

static const uint8_t device_descriptor[] = {
    0x12, // bLength
    0x01, // bDescriptorType
    0x00,
    0x02,       // bcdUSB
    0xef,       // bDeviceClass
    0x02,       // bDeviceSubClass
    0x01,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0
    0x00, 0x00, // idVendor
    0x00, 0x00, // idProduct
    0x00, 0x00, // bcdDevice
    0x00,       // iManufacture
    0x00,       // iProduct
    0x00,       // iSerialNumber
    0x01        // bNumConfigurations
};

static const uint8_t config_descriptor[] = {
    // standard configuration descriptor
    0x09,       // bLength
    0x02,       // bDescriptorType
    0x96, 0x00, // wTotalLength
    0x02,       // bNumInterfaces
    0x01,       // bConfigurationValue
    0x00,       // iConfiguration
    0xc0,       // bmAttributes
    50,         // bMaxPower
                //

    // IAD
    0x08, // bLength
    0x0b, // bDescriptorType
    0x00, // bFirstInterface
    0x02, // bInterfaceCount
    0x01, // bFunctionClass
    0x00, // bFunctionSubClass
    0x20, // bFunctionProtocol
    0x00, // iFunction

    // standard AC interface descriptor (interface 0)
    0x09, // bLength
    0x04, // bDescriptorType
    0x00, // bInterfaceNubmer
    0x00, // bAlternateSetting
    0x00, // bNumEndpoints
    0x01, // bInterfaceClass
    0x01, // bInterfaceSubClass
    0x20, // bInterfaceProtocol
    0x00, // iInterface

    // class specific AC interface descriptor
    0x09,       // bLength
    0x24,       // bDescriptorType
    0x01,       // bDescriptorSubType
    0x00, 0x02, // bcdADC
    0x01,       // bCategory
    0x37, 0x00, // wTotalLength
    0x11,       // bmControls

    // clock source descriptor
    0x08, // bLength
    0x24, // bDescripterType
    0x0a, // bDescriptorSubtype
    0x01, // bClockID
    0x01, // bmAttributes
    0x00, // bmControls
    0x00, // bAssocTerminal
    0x00, // iCockSource

    // clock selector descriptor
    0x08, // bLength
    0x24, // bDescriptorType
    0x0b, // bDescriptorSubType
    0x02, // bClockID
    0x01, // bNrInPins
    0x01, // baCSourceID[1]
    0x00, // bmControls
    0x00, // iClockSelector

    // clock multiplier descriptor
    0x07, // bLength
    0x24, // bDescriptorType
    0x0c, // bDescriptorSubType
    0x03, // bClockID
    0x01, // bCSourceID
    0x00, // bmControls
    0x00, // iClockMultiplier

    // input terminal descriptor
    0x11,                   // bLength
    0x24,                   // bDescriptorType
    0x02,                   // bDescriptorSubType
    0x01,                   // bTerminalID
    0x01, 0x02,             // wTerminalType
    0x01,                   // bAssocTerminal
    0x01,                   // bCSourceID
    0x01,                   // bNrChannels
    0x01, 0x00, 0x00, 0x00, // bmChannelConfig
    0x00,                   // iChannelNames
    0x00, 0x00,             // bmControls
    0x00,                   // iTerminal

    // output terminal descriptor
    0x0c,       // bLength
    0x24,       // bDescriptorType
    0x03,       // bDescriptorSubType
    0x02,       // bTerminalID
    0x01, 0x03, // wTerminalType
    0x00,       // bAssocTerminal
    0x01,       // bSourceID
    0x01,       // bCSourceID
    0x00, 0x00, // bmControls
    0x00,       // iTerminal

    // standard AS interface descriptor (interface 1, alt 0)
    0x09, // bLength
    0x04, // bDescriptorType
    0x01, // bInterfaceNumber
    0x00, // bAlternateSetting
    0x00, // bNumEndpoints
    0x01, // bInterfaceClass
    0x02, // bInterfaceSubClass
    0x20, // bInterfaceProtocol
    0x00, // iInterface

    // standard AS interface descriptor (interface 1, alt 1)
    0x09, // bLength
    0x04, // bDescriptorType
    0x01, // bInterfaceNumber
    0x01, // bAlternateSetting
    0x01, // bNumEndpoints
    0x01, // bInterfaceClass
    0x02, // bInterfaceSubClass
    0x20, // bInterfaceProtocol
    0x00, // iInterface

    // class-specific AS interface descriptor
    0x10,                   // bLength
    0x24,                   // bDescriptorType
    0x01,                   // bDescriptorSubType
    0x01,                   // bTerminalLink
    0x00,                   // bmControls
    0x01,                   // bFormatType
    0x01, 0x00, 0x00, 0x00, // bmFormats
    0x02,                   // bNrChannels
    0x03, 0x00, 0x00, 0x00, // bmChannelConfig
    0x00,                   // iChannelNames

    // format type descriptor
    0x0e,             // bLength
    0x24,             // bDescriptorType
    0x02,             // bDescriptorSubType
    0x01,             // bFormatType
    0x02,             // bNrChannels
    0x02,             // bSubFrameSize
    0x10,             // bBitResolution
    0x01,             // bSamFreqType
    0x80, 0xbb, 0x00, // tLowerSamFreq
    0x80, 0xbb, 0x00, // tUpperSamFreq

    // standard isochronous endpoint descriptor
    0x07,       // bLength
    0x05,       // bDescriptorType
    0x01,       // bEndpointAddress
    0x05,       // bmAttributes
    0xc0, 0x00, // wMaxPacketSize
    0x01,       // bInterval

    // class-specific endpoint descriptor
    0x08,      // bLength
    0x25,      // bDescriptorType
    0x01,      // bDescriptorSubType
    0x00,      // bmAttributes
    0x00,      // bmControls
    0x00,      // bLockDelayUnits
    0x00, 0x00 // wLockDelay
};

static void usb_core_reset(void) {
  USB->GRSTCTL |= USB_OTG_GRSTCTL_CSRST;
  while (USB->GRSTCTL & USB_OTG_GRSTCTL_CSRST_Msk)
    ;
}

void usb_init(void) {
  RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;

  usb_core_reset();

  USB->GCCFG |= USB_OTG_GCCFG_PWRDWN | USB_OTG_GCCFG_VBUSBSEN;
  USB->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;
  USB->GRXFSIZ = 128;
  USB->DIEPTXF0_HNPTXFSIZ = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) | 128;
  USB->DIEPTXF[0] = (128 << USB_OTG_DIEPTXF_INEPTXFD_Pos) | 192;
  USB_DEV->DCFG |= USB_OTG_DCFG_DSPD;
  USB_DEV->DIEPMSK |= USB_OTG_DIEPMSK_XFRCM;
  USB_DEV->DOEPMSK |= USB_OTG_DOEPMSK_XFRCM;

  USB->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |
                  USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_IEPINT;
  USB_INEP[0].DIEPINT |= USB_OTG_DIEPINT_XFRC;
  USB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;

  NVIC_SetPriority(OTG_FS_IRQn, 0);
  NVIC_EnableIRQ(OTG_FS_IRQn);

  USB_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;
}

// for debug
static volatile uint32_t reset_cnt = 0;
static volatile uint32_t setup_cnt = 0;
static volatile uint32_t set_interface_cnt = 0;

static const uint8_t *ep0_tx_ptr;
static uint16_t ep0_tx_remeining;

void OTG_FS_IRQHandler(void) {
  uint32_t gintsts = USB->GINTSTS;

  if (gintsts & USB_OTG_GINTSTS_USBRST_Msk) {
    reset_cnt++;
    USB_DEV->DCFG &= ~USB_OTG_DCFG_DAD;

    USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_USBAEP |
                           (64 << USB_OTG_DIEPCTL_MPSIZ_Pos) |
                           USB_OTG_DIEPCTL_CNAK;
    USB_OUTEP[0].DOEPCTL |= USB_OTG_DOEPCTL_USBAEP |
                            (64 << USB_OTG_DOEPCTL_MPSIZ_Pos) |
                            USB_OTG_DOEPCTL_CNAK;

    USB_DEV->DAINTMSK |=
        (1 << USB_OTG_DAINTMSK_IEPM_Pos) | (1 << USB_OTG_DAINTMSK_OEPM_Pos);

    USB->GINTSTS = USB_OTG_GINTSTS_USBRST;
  }

  if (gintsts & USB_OTG_GINTSTS_RXFLVL_Msk) {
    uint32_t grxstsp = USB->GRXSTSP;
    uint8_t pktsts =
        (grxstsp & USB_OTG_GRXSTSP_PKTSTS_Msk) >> USB_OTG_GRXSTSP_PKTSTS_Pos;
    uint32_t *fifo = USB_FIFO(0);

    if (pktsts == 0x06) {
      // SETUP packet
      setup_cnt++;
      uint32_t setup[2];

      setup[0] = *fifo;
      setup[1] = *fifo;

      uint8_t bRequest = (setup[0] >> 8) & 0xff;
      uint8_t bmRequestType = setup[0] & 0xff;
      uint16_t wValue = (setup[0] >> 16) & 0xffff;
      uint16_t wLength = (setup[1] >> 16) & 0xffff;
      uint16_t wIndex = setup[1] & 0xffff;

      if (bRequest == 0x06) {
        // GET_DESCRIPTOR
        uint8_t desc_type = wValue >> 8;
        uint16_t len = 0;
        const uint8_t *data = NULL;

        if (desc_type == 0x01) {
          // Device
          data = device_descriptor;
          len = sizeof(device_descriptor);

        } else if (desc_type == 0x02) {
          data = config_descriptor;
          len = sizeof(config_descriptor);
        } else {
          USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_STALL;
        }

        if (data) {
          if (len > wLength) {
            len = wLength;
          }

          ep0_tx_ptr = data;
          ep0_tx_remeining = len;

          uint16_t pkt_len = (ep0_tx_remeining > 64) ? 64 : ep0_tx_remeining;

          USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
                                 ((pkt_len & USB_OTG_DIEPTSIZ_XFRSIZ_Msk)
                                  << USB_OTG_DIEPTSIZ_XFRSIZ_Pos);
          USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;

          for (uint8_t i = 0; i < (pkt_len + 3) / 4; i++) {
            uint32_t val = 0;
            for (uint8_t b = 0; b < 4; b++) {
              if (4 * i + b < pkt_len) {
                val |= ((uint32_t)ep0_tx_ptr[4 * i + b]) << (8 * b);
              }
            }
            *fifo = val;
          }

          ep0_tx_ptr += pkt_len;
          ep0_tx_remeining -= pkt_len;
        }

      } else if (bRequest == 0x05) {
        uint8_t addr = wValue & 0x7f;
        USB_DEV->DCFG |= addr << USB_OTG_DCFG_DAD_Pos;
        USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos);
        USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;

      } else if (bRequest == 0x0b && (bmRequestType & 0x1f) == 0x01) {
        // SET_INTERFACE request
        set_interface_cnt++;
        uint8_t interface_num = wIndex & 0xff;
        uint8_t alt_settings = wValue & 0xff;

        if (interface_num == 1) {
          // AS interface
          if (alt_settings == 0) {
            USB_INEP[1].DIEPCTL &= ~USB_OTG_DIEPCTL_EPENA;
            USB_OUTEP[1].DOEPCTL &= ~USB_OTG_DOEPCTL_EPENA;
          } else if (alt_settings == 1) {
            USB_INEP[1].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) | 192;
            USB_INEP[1].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;
            USB_OUTEP[1].DOEPTSIZ = (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | 192;
            USB_OUTEP[1].DOEPCTL |=
                USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
          }
        }

        // Status stage
        USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos);
        USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;

      } else if (bRequest == 0x09) {
        // SET_CONFIGURATION
        USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) | 0;
        USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;

      } else {
        USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_STALL;
        USB_OUTEP[0].DOEPCTL |= USB_OTG_DOEPCTL_STALL;
      }

    } else if (pktsts == 0x03) {
      // SETUP complete
      USB_OUTEP[0].DOEPTSIZ = (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | 64;
      USB_OUTEP[0].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;

    } else if (pktsts == 0x04) {
      uint32_t bc =
          (grxstsp & USB_OTG_GRXSTSP_BCNT_Msk) >> USB_OTG_GRXSTSP_BCNT_Pos;
      for (uint8_t i = 0; i < (bc + 3) / 4; i++) {
        (void)*fifo;
      }
      USB_OUTEP[0].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
    }
  }

  if (gintsts & USB_OTG_GINTSTS_ENUMDNE_Msk) {
    USB->GINTSTS = USB_OTG_GINTSTS_ENUMDNE;
  }

  if (gintsts & USB_OTG_GINTSTS_IEPINT_Msk) {
    uint32_t diepint = USB_INEP[0].DIEPINT;

    if (diepint & USB_OTG_DIEPINT_XFRC_Msk) {
      USB_INEP[0].DIEPINT = USB_OTG_DIEPINT_XFRC;

      if (ep0_tx_remeining > 0) {
        uint16_t pkt_len = (ep0_tx_remeining > 64) ? 64 : ep0_tx_remeining;

        USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) | pkt_len;
        USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;

        for (uint8_t i = 0; i < (pkt_len + 3) / 4; i++) {
          uint32_t val = 0;
          for (uint8_t b = 0; b < 4; b++) {
            if (4 * i + b < pkt_len) {
              val |= ((uint32_t)ep0_tx_ptr[4 * i + b]) << (8 * b);
            }
          }
          uint32_t *fifo = USB_FIFO(0);
          *fifo = val;
        }

        ep0_tx_ptr += pkt_len;
        ep0_tx_remeining -= pkt_len;
      }

      // USB_OUTEP[0].DOEPTSIZ = (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | 64;
      // USB_OUTEP[0].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
    }
  }
}
