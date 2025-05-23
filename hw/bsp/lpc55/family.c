/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018, hathach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This file is part of the TinyUSB stack.
 */

#include "bsp/board_api.h"
#include "board.h"
#include "fsl_device_registers.h"
#include "fsl_gpio.h"
#include "fsl_power.h"
#include "fsl_iocon.h"
#include "fsl_usart.h"
#include "fsl_clock.h"
#include "fsl_sctimer.h"

#if NEOPIXEL_NUMBER
#include "sct_neopixel.h"
#endif

#ifdef BOARD_TUD_RHPORT
  #define PORT_SUPPORT_DEVICE(_n)  (BOARD_TUD_RHPORT == _n)
#else
  #define PORT_SUPPORT_DEVICE(_n)  0
#endif

#ifdef BOARD_TUH_RHPORT
  #define PORT_SUPPORT_HOST(_n)    (BOARD_TUH_RHPORT == _n)
#else
  #define PORT_SUPPORT_HOST(_n)    0
#endif

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM
//--------------------------------------------------------------------+

// IOCON pin mux
#define IOCON_PIO_DIGITAL_EN     0x0100u // Enables digital function
#define IOCON_PIO_FUNC0          0x00u   // Selects pin function 0
#define IOCON_PIO_FUNC1          0x01u   // Selects pin function 1
#define IOCON_PIO_FUNC4          0x04u   // Selects pin function 4
#define IOCON_PIO_FUNC7          0x07u   // Selects pin function 7
#define IOCON_PIO_INV_DI         0x00u   // Input function is not inverted
#define IOCON_PIO_MODE_INACT     0x00u   // No addition pin function
#define IOCON_PIO_OPENDRAIN_DI   0x00u   // Open drain is disabled
#define IOCON_PIO_SLEW_STANDARD  0x00u   // Standard mode, output slew rate control is enabled

#define IOCON_PIO_DIG_FUNC0_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC0) // Digital pin function 0 enabled
#define IOCON_PIO_DIG_FUNC1_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC1) // Digital pin function 1 enabled
#define IOCON_PIO_DIG_FUNC4_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC4) // Digital pin function 2 enabled
#define IOCON_PIO_DIG_FUNC7_EN   (IOCON_PIO_DIGITAL_EN | IOCON_PIO_FUNC7) // Digital pin function 2 enabled

//--------------------------------------------------------------------+
// Forward USB interrupt events to TinyUSB IRQ Handler
//--------------------------------------------------------------------+
void USB0_IRQHandler(void) {
  tusb_int_handler(0, true);
}

void USB1_IRQHandler(void) {
  tusb_int_handler(1, true);
}

/****************************************************************
name: BOARD_BootClockPLL100M
outputs:
- {id: System_clock.outFreq, value: 100 MHz}
settings:
- {id: PLL0_Mode, value: Normal}
- {id: ANALOG_CONTROL_FRO192M_CTRL_ENDI_FRO_96M_CFG, value: Enable}
- {id: ENABLE_CLKIN_ENA, value: Enabled}
- {id: ENABLE_SYSTEM_CLK_OUT, value: Enabled}
- {id: SYSCON.MAINCLKSELB.sel, value: SYSCON.PLL0_BYPASS}
- {id: SYSCON.PLL0CLKSEL.sel, value: SYSCON.CLK_IN_EN}
- {id: SYSCON.PLL0M_MULT.scale, value: '100', locked: true}
- {id: SYSCON.PLL0N_DIV.scale, value: '4', locked: true}
- {id: SYSCON.PLL0_PDEC.scale, value: '4', locked: true}
sources:
- {id: ANACTRL.fro_hf.outFreq, value: 96 MHz}
- {id: SYSCON.XTAL32M.outFreq, value: 16 MHz, enabled: true}
******************************************************************/
void BootClockPLL150MFromFRO12M(void)
{
    /*!< Set up the clock sources */
    /*!< Configure FRO192M */
    POWER_DisablePD(kPDRUNCFG_PD_FRO192M);               /*!< Ensure FRO is on  */
    CLOCK_SetupFROClocking(12000000U);                   /*!< Set up FRO to the 12 MHz, just for sure */
    CLOCK_AttachClk(kFRO12M_to_MAIN_CLK);                /*!< Switch to FRO 12MHz first to ensure we can change the clock setting */

    POWER_SetVoltageForFreq(150000000U);                  /*!< Set voltage for the one of the fastest clock outputs: System clock output */
    CLOCK_SetFLASHAccessCyclesForFreq(150000000U);           /*!< Set FLASH wait states for core */

    /*!< Set up PLL */
    CLOCK_AttachClk(kFRO12M_to_PLL0);                    /*!< Switch PLL0CLKSEL to EXT_CLK */
    POWER_DisablePD(kPDRUNCFG_PD_PLL0);                  /* Ensure PLL is on  */
    POWER_DisablePD(kPDRUNCFG_PD_PLL0_SSCG);
    const pll_setup_t pll0Setup = {
	    .pllctrl = SYSCON_PLL0CTRL_CLKEN_MASK | SYSCON_PLL0CTRL_SELI(15U) |SYSCON_PLL0CTRL_SELP(7U),
	    .pllndec = SYSCON_PLL0NDEC_NDIV(1U),
	    .pllpdec = SYSCON_PLL0PDEC_PDIV(1U),
	    .pllsscg = {0x0U,(SYSCON_PLL0SSCG1_MDIV_EXT(25U) | SYSCON_PLL0SSCG1_SEL_EXT_MASK)},
	    .pllRate = 150000000U,
	    .flags =  PLL_SETUPFLAG_WAITLOCK
    };
    CLOCK_SetPLL0Freq(&pll0Setup);                       /*!< Configure PLL0 to the desired values */

    /*!< Set up dividers */
    CLOCK_SetClkDiv(kCLOCK_DivAhbClk, 1U, false);         /*!< Set AHBCLKDIV divider to value 1 */

    /*!< Set up clock selectors - Attach clocks to the peripheries */
    CLOCK_AttachClk(kPLL0_to_MAIN_CLK);                 /*!< Switch MAIN_CLK to PLL0 */

    /*!< Set SystemCoreClock variable. */
    SystemCoreClock = 150000000U;
}

void board_init(void) {
  // Enable IOCON clock
  CLOCK_EnableClock(kCLOCK_Iocon);

  // Init 150 MHz clock from PLL0 and internal FRO12M oscillator.
  BootClockPLL150MFromFRO12M();

#if CFG_TUSB_OS == OPT_OS_NONE
// 1ms tick timer
  SysTick_Config(SystemCoreClock / 1000);

#elif CFG_TUSB_OS == OPT_OS_FREERTOS
#if PORT_SUPPORT_DEVICE(0)
  // Explicitly disable systick to prevent its ISR runs before scheduler start
  SysTick->CTRL &= ~1U;

  // If freeRTOS is used, IRQ priority is limit by max syscall ( smaller is higher )
  NVIC_SetPriority(USB0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#elif PORT_SUPPORT_DEVICE(1)
  NVIC_SetPriority(USB1_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
#endif
#endif

  // Init all GPIO ports
  GPIO_PortInit(GPIO, 0);
  GPIO_PortInit(GPIO, 1);

  // LED
#if LED_ENABLE
  IOCON_PinMuxSet(IOCON, LED_PORT, LED_PIN, IOCON_PIO_DIG_FUNC0_EN);
  gpio_pin_config_t const led_config = {kGPIO_DigitalOutput, 1};
  GPIO_PinInit(GPIO, LED_PORT, LED_PIN, &led_config);
#endif

#if NEOPIXEL_NUMBER
  IOCON_PinMuxSet(IOCON, NEOPIXEL_PORT, NEOPIXEL_PIN, NEOPIXEL_IOMUX);

  sctpix_init(NEOPIXEL_TYPE);
  sctpix_addCh(NEOPIXEL_CH, pixelData, NEOPIXEL_NUMBER);
#endif

  // board_led_write calls to sct_show(), so it is neccesary call it after sctpix_init().
  board_led_write(0);

  // Button
#if BUTTON_ENABLE
  IOCON_PinMuxSet(IOCON, BUTTON_PORT, BUTTON_PIN, IOCON_PIO_DIG_FUNC0_EN);
  gpio_pin_config_t const button_config = {kGPIO_DigitalInput, 0};
  GPIO_PinInit(GPIO, BUTTON_PORT, BUTTON_PIN, &button_config);
#endif

#if UART_ENABLE
#ifdef UART_DEV
  // UART
  IOCON_PinMuxSet(IOCON, UART_RX_PINMUX);
  IOCON_PinMuxSet(IOCON, UART_TX_PINMUX);

  // Enable UART when debug log is on
  CLOCK_AttachClk(kFRO12M_to_FLEXCOMM0);
  usart_config_t uart_config;
  USART_GetDefaultConfig(&uart_config);
  uart_config.baudRate_Bps = CFG_BOARD_UART_BAUDRATE;
  uart_config.enableTx = true;
  uart_config.enableRx = true;
  USART_Init(UART_DEV, &uart_config, 12000000);
#endif
#endif

#if defined(CPU_LPC5536JBD100)
  // USB VBUS
  /* PORT1 PIN33 configured as USB0_VBUS */

  const uint32_t port1_pin31_config = (/* Pin is configured as USB0_VBUS */
                                       IOCON_PIO_FUNC7 |
                                       /* No addition pin function */
                                       IOCON_PIO_MODE_INACT |
                                       /* Standard mode, output slew rate control is enabled */
                                       IOCON_PIO_SLEW_STANDARD |
                                       /* Input function is not inverted */
                                       IOCON_PIO_INV_DI |
                                       /* Enables digital function */
                                       IOCON_PIO_DIGITAL_EN |
                                       /* Open drain is disabled */
                                       IOCON_PIO_OPENDRAIN_DI);

  /* PORT1 PIN31 (coords: 92) is configured as USB0_VBUS */
  IOCON_PinMuxSet(IOCON, 1U, 31U, port1_pin31_config);
#else
  // USB VBUS
  /* PORT0 PIN22 configured as USB0_VBUS */
  IOCON_PinMuxSet(IOCON, 0U, 22U, IOCON_PIO_DIG_FUNC7_EN);
#endif

#if defined(BOARD_TUD_RHPORT)  && BOARD_TUD_RHPORT == 0
  // Port0 is Full Speed

#if defined(CPU_LPC5536JBD100)
  POWER_DisablePD(kPDRUNCFG_PD_USBFSPHY);   //Turn on USB0 Phy
  RESET_PeripheralReset(kUSB0_DEV_RST_SHIFT_RSTn); //Reset the IP to make sure it's in reset state
#else
  POWER_DisablePD(kPDRUNCFG_PD_USB0_PHY);     //Turn on USB0 Phy
  RESET_PeripheralReset(kUSB0D_RST_SHIFT_RSTn);   //Reset the IP to make sure it's in reset state
#endif

  RESET_PeripheralReset(kUSB0HSL_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB0HMR_RST_SHIFT_RSTn);

  // Enable USB Clock Adjustments to trim the FRO for the full speed controller
  ANACTRL->FRO192M_CTRL |= ANACTRL_FRO192M_CTRL_USBCLKADJ_MASK;
  CLOCK_SetClkDiv(kCLOCK_DivUsb0Clk, 1, false);

#if defined(CPU_LPC5536JBD100)
  CLOCK_AttachClk(kFRO_HF_to_USB0);
#else
  CLOCK_AttachClk(kFRO_HF_to_USB0_CLK);
#endif

  /*According to reference manual, device mode setting has to be set by access usb host register */
  CLOCK_EnableClock(kCLOCK_Usbhsl0);  // enable usb0 host clock
  USBFSH->PORTMODE |= USBFSH_PORTMODE_DEV_ENABLE_MASK;
  CLOCK_DisableClock(kCLOCK_Usbhsl0); // disable usb0 host clock

  /* enable USB Device clock */
  CLOCK_EnableUsbfs0DeviceClock(kCLOCK_UsbfsSrcFro, CLOCK_GetFreq(kCLOCK_FroHf));
#endif

#if defined(BOARD_TUD_RHPORT)  && BOARD_TUD_RHPORT == 1
  // Port1 is High Speed

  /* Turn on USB1 Phy */
  POWER_DisablePD(kPDRUNCFG_PD_USB1_PHY);

  /* reset the IP to make sure it's in reset state. */
  RESET_PeripheralReset(kUSB1H_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1D_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1_RST_SHIFT_RSTn);
  RESET_PeripheralReset(kUSB1RAM_RST_SHIFT_RSTn);

  /* According to reference manual, device mode setting has to be set by access usb host register */
  CLOCK_EnableClock(kCLOCK_Usbh1); // enable usb0 host clock

  USBHSH->PORTMODE = USBHSH_PORTMODE_SW_PDCOM_MASK; // Put PHY powerdown under software control
  USBHSH->PORTMODE |= USBHSH_PORTMODE_DEV_ENABLE_MASK;

  CLOCK_DisableClock(kCLOCK_Usbh1); // disable usb0 host clock

  /* enable USB Device clock */
  CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_UsbPhySrcExt, XTAL0_CLK_HZ);
  CLOCK_EnableUsbhs0DeviceClock(kCLOCK_UsbSrcUnused, 0U);
  CLOCK_EnableClock(kCLOCK_UsbRam1);

  // Enable PHY support for Low speed device + LS via FS Hub
  USBPHY->CTRL |= USBPHY_CTRL_SET_ENUTMILEVEL2_MASK | USBPHY_CTRL_SET_ENUTMILEVEL3_MASK;

  // Enable all power for normal operation
  USBPHY->PWD = 0;

  USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_CLKGATE_MASK;
  USBPHY->CTRL_SET = USBPHY_CTRL_SET_ENAUTOCLR_PHY_PWD_MASK;

  // TX Timing
//  uint32_t phytx = USBPHY->TX;
//  phytx &= ~(USBPHY_TX_D_CAL_MASK | USBPHY_TX_TXCAL45DM_MASK | USBPHY_TX_TXCAL45DP_MASK);
//  phytx |= USBPHY_TX_D_CAL(0x0C) | USBPHY_TX_TXCAL45DP(0x06) | USBPHY_TX_TXCAL45DM(0x06);
//  USBPHY->TX = phytx;

    ARM_MPU_SetMemAttr(0, 0x44); // Normal memory, non-cacheable (inner and outer)
    ARM_MPU_SetRegion(0, ARM_MPU_RBAR(0x40100000, ARM_MPU_SH_NON, 0, 1, 1), ARM_MPU_RLAR(0x40104000, 0));
    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk | MPU_CTRL_HFNMIENA_Msk);
#endif
}

//--------------------------------------------------------------------+
// Board porting API
//--------------------------------------------------------------------+

void board_led_write(bool state)
{
#if LED_ENABLE
  GPIO_PinWrite(GPIO, LED_PORT, LED_PIN, state ? LED_STATE_ON : (1-LED_STATE_ON));
#endif

#if NEOPIXEL_NUMBER
  if (state) {
    sctpix_setPixel(NEOPIXEL_CH, 0, 0xFF0000);
    sctpix_setPixel(NEOPIXEL_CH, 1, 0xFF0000);
    sctpix_setPixel(NEOPIXEL_CH, 2, 0xFF0000);
  } else {
    sctpix_setPixel(NEOPIXEL_CH, 0, 0x000000);
    sctpix_setPixel(NEOPIXEL_CH, 1, 0x000000);
    sctpix_setPixel(NEOPIXEL_CH, 2, 0x000000);
  }
  sctpix_show();

#endif
}

uint32_t board_button_read(void)
{
#if BUTTON_ENABLE
  // active low
  return BUTTON_STATE_ACTIVE == GPIO_PinRead(GPIO, BUTTON_PORT, BUTTON_PIN);
#else
  return 0;
#endif
}

int board_uart_read(uint8_t* buf, int len) {
  (void) buf;
  (void) len;
  return 0;
}

int board_uart_write(void const * buf, int len)
{
#if UART_ENABLE
  USART_WriteBlocking(UART_DEV, (uint8_t const *) buf, len);
#endif
  return len;
}

#if CFG_TUSB_OS == OPT_OS_NONE
volatile uint32_t system_ticks = 0;

void SysTick_Handler(void) {
  system_ticks++;
}

uint32_t board_millis(void) {
  return system_ticks;
}
#endif


#ifndef __ICCARM__
// Implement _start() since we use linker flag '-nostartfiles'.
// Requires defined __STARTUP_CLEAR_BSS,
extern int main(void);

TU_ATTR_UNUSED void _start(void) {
  // called by startup code
  main();
  while (1) {}
}

#ifdef __clang__
void	_exit (int __status) {
  while (1) {}
}
#endif

#endif
