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
    0x01,       // bDeviceClass: each interface specifies class
    0x00,       // bDeviceSubClass
    0x20,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0
    0x83, 0x04, // idVendor
    0x11, 0x57, // idProduct
    0x00, 0x01, // bcdDevice
    0x01,       // iManufacturer
    0x02,       // iProduct
    0x03,       // iSerialNumber
    0x01        // bNumConfigurations
};

// Configuration Descriptor + Audio Control + Audio Streaming + Iso EP
static const uint8_t config_descriptor[] = {
    // Configuration Descriptor
    0x09, 0x02, 0x3b, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,

    // Standard AC Interface Descriptor
    0x09, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01, 0x20, 0x00,

    // Class-Specific AC Interface Descriptor
    0x08, 0x24, 0x01, 0x00, 0x02, 0x16, 0x00, 0x01, 0x01,

    // Standard AS Interface Descriptor (Alternate 0)
    0x09, 0x04, 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,

    // Standard AS Interface Descriptor (Alternate 1)
    0x09, 0x04, 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,

    // Class-Specific AS Interface Descriptor
    0x07, 0x24, 0x01, 0x01, 0x01, 0x01, 0x00,

    // Standard Isochronous EP Descriptor
    0x07, 0x05, 0x01, 0x01, 0x40, 0x00, 0x01};

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
  USB_DEV->DCFG |= USB_OTG_DCFG_DSPD;
  USB_DEV->DIEPMSK |= USB_OTG_DIEPMSK_XFRCM;
  USB_DEV->DOEPMSK |= USB_OTG_DOEPMSK_XFRCM;

  USB->GINTMSK |= USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |
                  USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_IEPINT;
  //   USB_INEP[0].DIEPINT |= USB_OTG_DIEPINT_XFRC;
  USB->GAHBCFG |= USB_OTG_GAHBCFG_GINT;

  NVIC_SetPriority(OTG_FS_IRQn, 0);
  NVIC_EnableIRQ(OTG_FS_IRQn);

  USB_DEV->DCTL &= ~USB_OTG_DCTL_SDIS;
}

static volatile uint32_t reset_cnt = 0;
static volatile uint32_t setup_cnt = 0;

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

          USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos) |
                                 ((len & USB_OTG_DIEPTSIZ_XFRSIZ_Msk)
                                  << USB_OTG_DIEPTSIZ_XFRSIZ_Pos);
          USB_INEP[0].DIEPCTL |= USB_OTG_DIEPCTL_EPENA | USB_OTG_DIEPCTL_CNAK;

          for (uint8_t i = 0; i < (len + 3) / 4; i++) {
            uint32_t val = 0;
            for (uint8_t b = 0; b < 4; b++) {
              if (4 * i + b < len) {
                val |= ((uint32_t)data[4 * i + b]) << (8 * b);
              }
            }
            *fifo = val;
          }
        }

      } else if (bRequest == 0x05) {
        uint8_t addr = wValue & 0x7f;
        USB_DEV->DCFG |= addr << USB_OTG_DCFG_DAD_Pos;
        USB_INEP[0].DIEPTSIZ = (1 << USB_OTG_DIEPTSIZ_PKTCNT_Pos);
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

  if (gintsts & USB_OTG_GINTSTS_IEPINT_Msk) {
    uint32_t diepint = USB_INEP[0].DIEPINT;

    if (diepint & USB_OTG_DIEPINT_XFRC_Msk) {
      USB_INEP[0].DIEPINT = USB_OTG_DIEPINT_XFRC;

      USB_OUTEP[0].DOEPTSIZ = (1 << USB_OTG_DOEPTSIZ_PKTCNT_Pos) | 64;
      USB_OUTEP[0].DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
    }
  }
}
