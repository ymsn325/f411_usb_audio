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
    0x12,       // bLength
    0x01,       // bDescriptorType (Device)
    0x00, 0x02, // bcdUSB 2.00
    0xef,       // bDeviceClass: each interface specifies class
    0x02,       // bDeviceSubClass
    0x01,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0
    0x83, 0x04, // idVendor
    0x11, 0x57, // idProduct
    0x00, 0x01, // bcdDevice
    0x01,       // iManufacturer
    0x02,       // iProduct
    0x03,       // iSerialNumber
    0x01        // bNumConfigurations
};

// Configuration Descriptor + Audio Control + Audio Streaming + Isochronous EP
static const uint8_t config_descriptor[] = {
    // Configuration Descriptor
    0x09,       // bLength = 9 bytes
    0x02,       // bDescriptorType = config
    0x57, 0x00, // wTotalLength
    0x01,       // bNumInterfaces
    0x01,       // nConfigurationValue
    0x00,       // iConfiguration
    0x80,       // bmAttributes = self powered
    250,        // bMaxPower = 500mA

    // Interface Association Descriptor
    0x08, // bLength
    0x0b, // bDescriptorType = INTERFACE ASSICOIATION descriptor
    0x00, // bFirstInterface
    0x02, // bInterfaceCount
    0x01, // bFunctionClass
    0x00, // bFunctionSubClass
    0x20, // bFunctionProtocol = AF_VERSION_02_)0
    0x00, // iFunction

    // Standard AC Interface Descriptor
    0x09, // bLength
    0x04, // bDescriptorType = INTERFACE
    0x00, // bInterfaceNumber
    0x00, // bAlternateSetting
    0x00, // bNumEndpoints
    0x01, // bInterfaceClass
    0x01, // bInterfaceSubClass = AUDIOCONTROL
    0x20, // bInterfaceProtocol = IP_VERSION_02_00
    0x00, // iInterface

    // Class-Specific AC Interface Descriptor
    0x09,       // bLength
    0x24,       // bDescriptorType = CS_INTERFACE
    0x01,       // bDescriptorSubType  = HEADER
    0x02, 0x00, // bcdADC = UAC2.0
    0x01,       // bCategory = DESKTOP_SPEAKER
    0x3e, 0x00, // wTotalLength = 9 bytes
    0x00,       // bmControls = No control

    // Clock Source Descriptor
    0x08, // bLength
    0x24, // bDeescriptorType = CS_INTERFACE
    0x0a, // bDescriptorSubSype = CLOCK_SOURCE
    0x00, // bClockID
    0x01, // bmAttributes = Internal fixed clock
    0x07, // bmControl
    0x00, // bAssocTerminal
    0x00, // iClockSource

    // Clock Selector Descriptor
    0x08, // bLength
    0x24, // bDescriptorType = CS_INTERFACE
    0x0b, // bDescriptorSubType = CLOCK_SELECTOR
    0x01, // bClockID
    0x01, // bNrInPins
    0x00, // baCSourceID[1]
    0x03, // bmControls
    0x00, // iClockSelector

    // Clock Multiplier Descriptor
    0x08, // bLength = 8 bytes
    0x24, // bDescriptorType = CS_INTERFACE
    0x0C, // bDescriptorSubtype = CLOCK_MULTIPLIER
    0x02, // bClockID = このクロックマルチプライヤのID
    0x00, // bCSourceID = 乗算元 Clock Source ID
    0x03, // bmControls = 0b11 → ホストが変更可能
    0x04, // bMultiplier = 4倍
    0x00, // iClockMultiplier = 文字列なし

    // Input Terminal Descriptor
    0x11,                   // bLength = 17 bytes
    0x24,                   // bDescriptorType = CS_INTERFACE
    0x02,                   // bDescriptorSubtype = INPUT_TERMINAL
    0x01,                   // bTerminalID = 1
    0x01, 0x02,             // wTerminalType = USB Streaming (0x0201)
    0x00,                   // bAssocTerminal = なし
    0x00,                   // bCSourceID = Clock Source ID 0x10
    0x02,                   // bNrChannels = 2 (ステレオ)
    0x03, 0x00, 0x00, 0x00, // wChannelConfig = 左右チャンネル
    0x00,                   // iChannelNames = なし
    0x00, 0x00,             // bmControls = none
    0x00,                   // iTerminal = なし

    // Output Terminal Descriptor
    0x0c,       // bLength = 9 bytes
    0x24,       // bDescriptorType = CS_INTERFACE
    0x03,       // bDescriptorSubtype = OUTPUT_TERMINAL
    0x02,       // bTerminalID = 2
    0x01, 0x03, // wTerminalType = Speaker (0x0301)
    0x00,       // bAssocTerminal = なし
    0x01,       // bSourceID = Input Terminal ID (または Feature Unit ID)
    0x00,       // bCSourceID = Clock Source ID
    0x00, 0x00, // bmControls = none
    0x00        // iTerminal = なし
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
        interface_num = wLength;

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
