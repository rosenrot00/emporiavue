typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

typedef uint8_t bool;
#define false 0
#define true 1
#define CHAR_BIT 8
#define UINT32_C(value) value##UL
#define INT16_MIN (-32768)
#define INT16_MAX 32767

__attribute__((used, noinline, optimize("O0")))
void *memset(void *dst, int value, unsigned int length)
{
	volatile uint8_t *out = (volatile uint8_t *) dst;
	while (length-- > 0)
		*out++ = (uint8_t) value;
	return dst;
}

__attribute__((used, noinline, optimize("O0")))
void *memcpy(void *dst, const void *src, unsigned int length)
{
	volatile uint8_t *out = (volatile uint8_t *) dst;
	const volatile uint8_t *in = (const volatile uint8_t *) src;
	while (length-- > 0)
		*out++ = *in++;
	return dst;
}

typedef volatile       uint8_t  RwReg8;  /**< Read-Write  8-bit register (volatile unsigned int) */
typedef volatile       uint8_t  RoReg8;  /**< Read only  8-bit register (volatile const unsigned int) */
typedef volatile       uint16_t RwReg16; /**< Read-Write 16-bit register (volatile unsigned int) */
typedef volatile       uint16_t RoReg16; /**< Read only 16-bit register (volatile unsigned int) */
typedef volatile       uint32_t RwReg;   /**< Read-Write 32-bit register (volatile unsigned int) */
typedef volatile       uint32_t RoReg;   /**< Read only 32-bit register (volatile const unsigned int) */

#ifdef __cplusplus
  #define   __I     volatile             /*!< Defines 'read only' permissions                 */
#else
  #define   __I     volatile const       /*!< Defines 'read only' permissions                 */
#endif
#define     __O     volatile             /*!< Defines 'write only' permissions                */
#define     __IO    volatile             /*!< Defines 'read / write' permissions              */

#define REG_ADC_CTRLA              (*(RwReg8 *)0x42002000UL)
#define REG_ADC_REFCTRL            (*(RwReg8 *)0x42002001UL)
#define REG_ADC_SAMPCTRL           (*(RwReg8 *)0x42002003UL)
#define REG_ADC_CTRLB              (*(RwReg16*)0x42002004UL)
#define REG_ADC_INPUTCTRL          (*(RwReg  *)0x42002010UL)
#define REG_ADC_EVCTRL             (*(RwReg8 *)0x42002014UL) /**< \brief (ADC) Event Control */
#define REG_ADC_INTFLAG            (*(RwReg8 *)0x42002018UL) /**< \brief (ADC) Interrupt Flag Status and Clear */
#define REG_ADC_STATUS             (*(RoReg8 *)0x42002019UL) /**< \brief (ADC) Status */
#define REG_ADC_RESULT             (*(RoReg16*)0x4200201AUL) /**< \brief (ADC) Result */
#define REG_ADC_CALIB              (*(RwReg16*)0x42002028UL)

#define REG_SYSCTRL_PCLKSR         (*(RoReg  *)0x4000080CUL) /**< \brief (SYSCTRL) Power and Clocks Status */
#define REG_SYSCTRL_OSC32K         (*(RwReg  *)0x40000818UL) /**< \brief (SYSCTRL) 32kHz Internal Oscillator (OSC32K) Control */
#define REG_SYSCTRL_OSC8M          (*(RwReg  *)0x40000820UL) /**< \brief (SYSCTRL) 8MHz Internal Oscillator (OSC8M) Control */
#define REG_SYSCTRL_DFLLCTRL       (*(RwReg16*)0x40000824UL) /**< \brief (SYSCTRL) DFLL48M Control */
#define REG_SYSCTRL_DFLLVAL        (*(RwReg  *)0x40000828UL) /**< \brief (SYSCTRL) DFLL48M Value */

#define REG_GCLK_STATUS            (*(RoReg8 *)0x40000C01UL) /**< \brief (GCLK) Status */
#define REG_GCLK_CLKCTRL           (*(RwReg16*)0x40000C02UL) /**< \brief (GCLK) Generic Clock Control */
#define REG_GCLK_GENCTRL           (*(RwReg  *)0x40000C04UL) /**< \brief (GCLK) Generic Clock Generator Control */
#define REG_GCLK_GENDIV            (*(RwReg  *)0x40000C08UL) /**< \brief (GCLK) Generic Clock Generator Division */

#define REG_PM_APBCMASK            (*(RwReg  *)0x40000420UL) /**< \brief (PM) APBC Mask */

#define REG_NVMCTRL_CTRLB          (*(RwReg  *)0x41004004UL) /**< \brief (NVMCTRL) Control B */

#define REG_EVSYS_CHANNEL          (*(RwReg  *)0x42000404UL) /**< \brief (EVSYS) Channel */
#define REG_EVSYS_USER             (*(RwReg16*)0x42000408UL) /**< \brief (EVSYS) User Multiplexer */

#define REG_TC1_CTRLA              (*(RwReg16*)0x42001800UL) /**< \brief (TC1) Control A */
#define REG_TC1_EVCTRL             (*(RwReg16*)0x4200180AUL) /**< \brief (TC1) Event Control */
#define REG_TC1_INTFLAG            (*(RwReg8 *)0x4200180EUL) /**< \brief (TC1) Interrupt Flag Status and Clear */
#define REG_TC1_STATUS             (*(RoReg8 *)0x4200180FUL) /**< \brief (TC1) Status */
#define REG_TC1_COUNT16_CC0        (*(RwReg16*)0x42001818UL) /**< \brief (TC1) COUNT16 Compare/Capture 0 */

#define REG_PORT_DIR	            (*(RwReg  *)0x41004400UL)
#define REG_PORT_OUT	            (*(RwReg  *)0x41004410UL)
#define REG_PORT_OUTCLR            (*(RwReg  *)0x41004414UL) /**< \brief (PORT) Data Output Value Clear 0 */
#define REG_PORT_OUTSET            (*(RwReg  *)0x41004418UL) /**< \brief (PORT) Data Output Value Set 0 */
#define REG_PORT_IN                (*(RoReg  *)0x41004420UL)
#define REG_PINCFG2	            (*(RwReg8 *)0x41004442UL)
#define REG_PINCFG3	            (*(RwReg8 *)0x41004443UL)
#define REG_PINCFG4	            (*(RwReg8 *)0x41004444UL)
#define REG_PINCFG5	            (*(RwReg8 *)0x41004445UL)
#define REG_PINCFG6	            (*(RwReg8 *)0x41004446UL)
#define REG_PINCFG7	            (*(RwReg8 *)0x41004447UL)
#define REG_PINCFG8	            (*(RwReg8 *)0x41004448UL)
#define REG_PINCFG9	            (*(RwReg8 *)0x41004449UL)
#define REG_PINCFG10	            (*(RwReg8 *)0x4100444AUL)
#define REG_PINCFG11	            (*(RwReg8 *)0x4100444BUL)
#define REG_PINCFG14	            (*(RwReg8 *)0x4100444EUL)
#define REG_PINCFG15	            (*(RwReg8 *)0x4100444FUL)
#define REG_PINCFG22	            (*(RwReg8 *)0x41004456UL)
#define REG_PINCFG23	            (*(RwReg8 *)0x41004457UL)
#define REG_PINCFG25	            (*(RwReg8 *)0x41004459UL)
#define REG_PINCFG27	            (*(RwReg8 *)0x4100445BUL)
#define REG_PINCFG28	            (*(RwReg8 *)0x4100445CUL)
#define REG_PINCFG30	            (*(RwReg8 *)0x4100445EUL)
#define REG_PINCFG31	            (*(RwReg8 *)0x4100445FUL)
#define REG_PORT_PMUX1             (*(RwReg8 *)0x41004431UL)
#define REG_PORT_PMUX2	            (*(RwReg8 *)0x41004432UL)
#define REG_PORT_PMUX3	            (*(RwReg8 *)0x41004433UL)
#define REG_PORT_PMUX5	            (*(RwReg8 *)0x41004435UL)
#define REG_PORT_PMUX7	            (*(RwReg8 *)0x41004437UL)
#define REG_PORT_PMUX11            (*(RwReg8 *)0x4100443BUL)

#define REG_DMAC_CTRL              (*(RwReg16*)0x41004800UL) /**< \brief (DMAC) Control */
#define REG_DMAC_PRICTRL0          (*(RwReg  *)0x41004814UL) /**< \brief (DMAC) Priority Control 0 */
#define REG_DMAC_INTPEND           (*(RwReg16*)0x41004820UL) /**< \brief (DMAC) Interrupt Pending */
#define REG_DMAC_BASEADDR          (*(RwReg  *)0x41004834UL) /**< \brief (DMAC) Descriptor Memory Section Base Address */
#define REG_DMAC_WRBADDR           (*(RwReg  *)0x41004838UL) /**< \brief (DMAC) Write-Back Memory Section Base Address */
#define REG_DMAC_CHID              (*(RwReg8 *)0x4100483FUL) /**< \brief (DMAC) Channel ID */
#define REG_DMAC_CHCTRLA           (*(RwReg8 *)0x41004840UL) /**< \brief (DMAC) Channel Control A */
#define REG_DMAC_CHCTRLB           (*(RwReg  *)0x41004844UL) /**< \brief (DMAC) Channel Control B */
#define REG_DMAC_CHINTENSET        (*(RwReg8 *)0x4100484DUL) /**< \brief (DMAC) Channel Interrupt Enable Set */
#define REG_DMAC_CHINTFLAG         (*(RwReg8 *)0x4100484EUL) /**< \brief (DMAC) Channel Interrupt Flag Status and Clear */

#define REG_NVIC_SETENA	    (*(RwReg  *)0xE000E100UL) //Interrupt Set-Enable Register
#define REG_NVIC_PRIO1		    (*(RwReg  *)0xE000E404UL) //Interrupt Priority Register 1
#define REG_NVIC_PRIO2		    (*(RwReg  *)0xE000E408UL) //Interrupt Priority Register 1
#define REG_NVIC_PRIO3		    (*(RwReg  *)0xE000E40CUL) //Interrupt Priority Register 3

#define REG_SERCOM1_CTRLA          (*(RwReg  *)0x42000C00UL)
#define REG_SERCOM1_CTRLB          (*(RwReg  *)0x42000C04UL)
#define REG_SERCOM1_BAUD           (*(RwReg8 *)0x42000C0CUL)
#define REG_SERCOM1_INTFLAG        (*(RwReg8 *)0x42000C18UL)
#define REG_SERCOM1_SYNCBUSY       (*(RoReg  *)0x42000C1CUL)
#define REG_SERCOM1_DATA           (*(RwReg8 *)0x42000C28UL)

#define STATUS_SYNCBUSY_BIT        0x80

#define SCB_VTOR_TBLOFF_Pos                 7                                             /*!< SCB VTOR: TBLOFF Position */
#define SCB_VTOR_TBLOFF_Msk                (0x1FFFFFFUL << SCB_VTOR_TBLOFF_Pos)           /*!< SCB VTOR: TBLOFF Mask */

#define DUMMY __attribute__ ((weak, alias ("irq_handler_dummy")))

#define BOOT_DIAG_MAGIC 0x45565344UL
#define BOOT_STAGE_RESET_ENTRY 1
#define BOOT_STAGE_MAIN_ENTRY 10
#define BOOT_STAGE_NVM_READ_WAIT 11
#define BOOT_STAGE_PORT_CONFIGURED 16
#define BOOT_STAGE_CLOCK_CONFIGURED 17
#define BOOT_STAGE_NVM_MANUAL_WRITE 18
#define BOOT_STAGE_EVENT_SYSTEM_CONFIGURED 19
#define BOOT_STAGE_DMAC_CONFIGURED 20
#define BOOT_STAGE_ADC_CONFIGURED 21
#define BOOT_STAGE_TC1_CONFIGURED 22
#define BOOT_STAGE_NVIC_CONFIGURED 23
#define BOOT_STAGE_DMA_ENABLED 24
#define BOOT_STAGE_ADC_ENABLED 25
#define BOOT_STAGE_TC1_ENABLED 26
#define BOOT_STAGE_SERCOM1_CONFIGURED 27
#define BOOT_STAGE_MAIN_LOOP 28
#define BOOT_STAGE_HARDFAULT 0xff

struct BootDiagnosticType
{
	uint32_t magic;
	uint32_t stage;
	uint32_t hardfault_lr;
	uint32_t hardfault_msp;
	uint32_t hardfault_psp;
	uint32_t stacked_pc;
	uint32_t stacked_lr;
	uint32_t reserved;
};

__attribute__((used, section(".bss.$RESERVED")))
volatile struct BootDiagnosticType BootDiagnostic;

static inline void set_boot_stage(uint32_t stage)
{
	BootDiagnostic.magic = BOOT_DIAG_MAGIC;
	BootDiagnostic.stage = stage;
}

bool dmabool = false;
uint8_t DMAresultIndex = 0;
int16_t DMAresults[6][8]; //We copy the 8 ADC results to this buffer using DMA
			//Layout: MainCT1_V, MainCT1_A, MainCT2_V, MainCT2_A, MainCT_3_V, MainCT_3_A, Mux1_A, Mux2_A

uint8_t MuxCounter = 0; //Varies between 0 and 7 to switch between the 8 muxes, each serving 2 50A CT's
const uint32_t outputpinTable [8] = { 0x1000000, 0x1010000, 0x1020000, 0x1030000, 0, 0x10000, 0x20000, 0x30000 }; //all possible combinations of pin 16, 17 and 24.

#define EMPORIAVUE_HARDWARE_ID       2
#define EMPORIAVUE_MODE_ID       2
#define EMPORIAVUE_FIRMWARE_VERSION  10

#define TC1_ADC_TRIGGER_PERIOD         0x4C
#define ADC_SAMPLE_TIME_LENGTH         1
#define ADC_REFCTRL_VREFA              3
#define ADC_INPUTCTRL_SCAN8_DIFF_GAIN2 0x1270000UL
#define ADC_CTRLB_DIV8_12BIT_DIFF      0x101

#define SPI_FRAME_VERSION              1
#define SPI_FRAME_TYPE_RAW_SAMPLES     1
#define SPI_FRAME_SIZE                 1024
#define SPI_HEADER_SIZE                12
#define SPI_CRC_SIZE                   4
#define SPI_PAYLOAD_SIZE               (SPI_FRAME_SIZE - SPI_HEADER_SIZE - SPI_CRC_SIZE)
#define SPI_RAW_SCAN_COUNT             56
#define SPI_FLAG_OVERRUN               0x0001
#define SPI_FLAG_DMA_ERROR             0x0002
#define SPI_FRAME_CS_PIN_MASK          0x80000000UL  // PA31/SWDIO, active-low SPI frame/CS.
#define SPI_SAMPLE_PERIOD_TICKS        632

struct __attribute__((__packed__)) SpiFrameHeader
{
	uint8_t version;
	uint8_t type;
	uint16_t length;
	uint16_t sequence;
	uint16_t flags;
	uint32_t sample_counter;
};

struct __attribute__((__packed__)) SpiRawScan
{
	int16_t value[8];
	uint8_t mux_index;
	uint8_t reserved;
};

struct __attribute__((__packed__, aligned(4))) SpiRawFrame
{
	struct SpiFrameHeader header;
	struct SpiRawScan scans[SPI_RAW_SCAN_COUNT];
	uint32_t crc32;
};

typedef char SpiRawFrameSizeCheck[(sizeof(struct SpiRawFrame) == SPI_FRAME_SIZE) ? 1 : -1];
typedef char SpiRawPayloadSizeCheck[(sizeof(struct SpiRawScan) * SPI_RAW_SCAN_COUNT == SPI_PAYLOAD_SIZE) ? 1 : -1];

#define SPI_FRAME_BUFFER_COUNT         3
#define SPI_READY_QUEUE_CAPACITY       (SPI_FRAME_BUFFER_COUNT - 1)

struct SpiRawFrame SpiFrames[SPI_FRAME_BUFFER_COUNT];
volatile uint8_t SpiBuildFrameIndex = 0;
volatile uint8_t SpiReadyFrameIndex[SPI_READY_QUEUE_CAPACITY];
volatile uint8_t SpiReadyHead = 0;
volatile uint8_t SpiReadyCount = 0;
volatile uint8_t SpiBuildScanIndex = 0;
volatile uint16_t SpiFrameSequence = 0;
volatile uint16_t SpiPendingFlags = 0;
volatile uint32_t SpiSampleCounter = 0;
volatile uint32_t SpiFrameOverruns = 0;

static uint32_t spi_enter_critical(void)
{
	uint32_t primask;
	__asm__ __volatile__(
		"mrs %0, primask\n"
		"cpsid i"
		: "=r" (primask)
		:
		: "memory");
	return primask;
}

static void spi_exit_critical(uint32_t primask)
{
	__asm__ __volatile__("msr primask, %0" : : "r" (primask) : "memory");
}

static uint8_t spi_ready_queue_contains(uint8_t frame_index)
{
	for (uint8_t offset = 0; offset < SpiReadyCount; offset++)
	{
		const uint8_t queue_index = (uint8_t) ((SpiReadyHead + offset) % SPI_READY_QUEUE_CAPACITY);
		if (SpiReadyFrameIndex[queue_index] == frame_index)
			return 1;
	}
	return 0;
}

static uint8_t spi_find_free_build_frame(void)
{
	for (uint8_t frame_index = 0; frame_index < SPI_FRAME_BUFFER_COUNT; frame_index++)
	{
		if (!spi_ready_queue_contains(frame_index))
			return frame_index;
	}
	return SpiBuildFrameIndex;
}

static const uint32_t Crc32NibbleTable[16] = {
	0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
	0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
	0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
	0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
};

static uint32_t crc32_update_byte(uint32_t crc, uint8_t value)
{
	crc ^= value;
	crc = (crc >> 4) ^ Crc32NibbleTable[crc & 0x0FUL];
	crc = (crc >> 4) ^ Crc32NibbleTable[crc & 0x0FUL];
	return crc;
}

static void finalize_spi_frame(void)
{
	struct SpiRawFrame* frame = &SpiFrames[SpiBuildFrameIndex];

	frame->header.version = SPI_FRAME_VERSION;
	frame->header.type = SPI_FRAME_TYPE_RAW_SAMPLES;
	frame->header.length = SPI_PAYLOAD_SIZE;
	frame->header.sequence = SpiFrameSequence++;
	frame->header.flags = SpiPendingFlags;
	frame->crc32 = 0;
	SpiPendingFlags = 0;

	if (SpiReadyCount >= SPI_READY_QUEUE_CAPACITY)
	{
		SpiFrameOverruns++;
		SpiPendingFlags |= SPI_FLAG_OVERRUN;
		SpiBuildScanIndex = 0;
		return;
	}

	SpiBuildScanIndex = 0;
	const uint8_t ready_tail = (uint8_t) ((SpiReadyHead + SpiReadyCount) % SPI_READY_QUEUE_CAPACITY);
	SpiReadyFrameIndex[ready_tail] = SpiBuildFrameIndex;
	SpiReadyCount++;
	SpiBuildFrameIndex = spi_find_free_build_frame();
	__asm__ __volatile__("dmb sy");
}

static void capture_spi_scan(uint8_t dma_result_index, uint8_t mux_index)
{
	struct SpiRawFrame* frame = &SpiFrames[SpiBuildFrameIndex];

	if (SpiBuildScanIndex == 0)
		frame->header.sample_counter = SpiSampleCounter;

	struct SpiRawScan* scan = &frame->scans[SpiBuildScanIndex];
	for (uint8_t index = 0; index < 8; index++)
		scan->value[index] = DMAresults[dma_result_index][index];
	scan->mux_index = mux_index;
	scan->reserved = 0;
	if (SpiBuildScanIndex == 0)
		scan->reserved = (uint8_t) (SPI_SAMPLE_PERIOD_TICKS & 0xFF);
	else if (SpiBuildScanIndex == 1)
		scan->reserved = (uint8_t) ((SPI_SAMPLE_PERIOD_TICKS >> 8) & 0xFF);

	SpiSampleCounter++;
	SpiBuildScanIndex++;
	if (SpiBuildScanIndex >= SPI_RAW_SCAN_COUNT)
		finalize_spi_frame();
}

struct DMAdescriptorType {
  uint16_t BTCTRL;
  uint16_t BTCNT;
  uint32_t SRCADDR;
  uint32_t DSTADDR;
  uint32_t DESCADDR;
}  __attribute__((packed, aligned(16)))DMAdescriptor, __attribute__((packed, aligned(16)))DMAdescriptorwriteback;

#define __SIZE_OF__(x) \
({x __tmp_x_[2]; \
((unsigned int)(&__tmp_x_[1]) - (unsigned int)(&__tmp_x_[0])); \
})

extern unsigned int _etext;
extern unsigned int _data;
extern unsigned int _edata;
extern unsigned int _bss;
extern unsigned int _ebss;
extern int main(void);

void irq_handler_reset(void);
void irq_handler_dmac(void);
void irq_handler_sercom1(void);
DUMMY void irq_handler_nmi(void);
void irq_handler_hard_fault(void);
DUMMY void irq_handler_sv_call(void);
DUMMY void irq_handler_pend_sv(void);
DUMMY void irq_handler_sys_tick(void);
DUMMY void irq_handler_pm(void);
DUMMY void irq_handler_sysctrl(void);
DUMMY void irq_handler_wdt(void);
DUMMY void irq_handler_rtc(void);
DUMMY void irq_handler_eic(void);
DUMMY void irq_handler_nvmctrl(void);
DUMMY void irq_handler_evsys(void);
DUMMY void irq_handler_sercom0(void);
DUMMY void irq_handler_tc1(void);
DUMMY void irq_handler_tc2(void);
DUMMY void irq_handler_adc(void);
DUMMY void irq_handler_ptc(void);

extern void _stack_top(void);

//-----------------------------------------------------------------------------
__attribute__ ((used, section(".vectors")))
void (* const vectors[])(void) =
{
  &_stack_top,                   // 0 - Initial Stack Pointer Value

  // Cortex-M0+ handlers
  irq_handler_reset,             // 1 - Reset
  irq_handler_nmi,               // 2 - NMI
  irq_handler_hard_fault,        // 3 - Hard Fault
  0,                             // 4 - Reserved
  0,                             // 5 - Reserved
  0,                             // 6 - Reserved
  0,                             // 7 - Reserved
  0,                             // 8 - Reserved
  0,                             // 9 - Reserved
  0,                             // 10 - Reserved
  irq_handler_sv_call,           // 11 - SVCall
  0,                             // 12 - Reserved
  0,                             // 13 - Reserved
  irq_handler_pend_sv,           // 14 - PendSV
  irq_handler_sys_tick,          // 15 - SysTick

  // Peripheral handlers
  irq_handler_pm,                // 0 - Power Manager
  irq_handler_sysctrl,           // 1 - System Controller
  irq_handler_wdt,               // 2 - Watchdog Timer
  irq_handler_rtc,               // 3 - Real Time Counter
  irq_handler_eic,               // 4 - External Interrupt Controller
  irq_handler_nvmctrl,           // 5 - Non-Volatile Memory Controller
  irq_handler_dmac,              // 6 - Direct Memory Access Controller, we use this interrupt!
  0,		                 // 7 - Reserved (usb)
  irq_handler_evsys,             // 8 - Event System
  irq_handler_sercom0,           // 9 - Serial Communication Interface 0
  irq_handler_sercom1,           // 10 - Serial Communication Interface 1, we use this interrupt!
  0,		                  // 11 - Reserved (Serial Communication Interface 2)
  0,                             // 12 - Reserved
  irq_handler_tc1,               // 13 - Timer/Counter 1
  irq_handler_tc2,               // 14 - Timer/Counter 2
  irq_handler_adc,               // 15 - Analog-to-Digital Converter
  0,                             // 16 - Reserved
  0,		                 // 17 - Digital-to-Analog Converter
  irq_handler_ptc,               // 18 - Peripheral Touch Controller
};

/* Memory mapping of Cortex-M0+ Hardware */
#define SCS_BASE            (0xE000E000UL)                            /*!< System Control Space Base Address */
#define SysTick_BASE        (SCS_BASE +  0x0010UL)                    /*!< SysTick Base Address              */
#define NVIC_BASE           (SCS_BASE +  0x0100UL)                    /*!< NVIC Base Address                 */
#define SCB_BASE            (SCS_BASE +  0x0D00UL)                    /*!< System Control Block Base Address */

#define SCB                 ((SCB_Type       *)     SCB_BASE      )   /*!< SCB configuration struct           */
#define SysTick             ((SysTick_Type   *)     SysTick_BASE  )   /*!< SysTick configuration struct       */
#define NVIC                ((NVIC_Type      *)     NVIC_BASE     )   /*!< NVIC configuration struct          */

typedef struct
{
  __I  uint32_t CPUID;                   /*!< Offset: 0x000 (R/ )  CPUID Base Register                                   */
  __IO uint32_t ICSR;                    /*!< Offset: 0x004 (R/W)  Interrupt Control and State Register                  */
  __IO uint32_t VTOR;                    /*!< Offset: 0x008 (R/W)  Vector Table Offset Register                          */
  __IO uint32_t AIRCR;                   /*!< Offset: 0x00C (R/W)  Application Interrupt and Reset Control Register      */
  __IO uint32_t SCR;                     /*!< Offset: 0x010 (R/W)  System Control Register                               */
  __IO uint32_t CCR;                     /*!< Offset: 0x014 (R/W)  Configuration Control Register                        */
       uint32_t RESERVED1;
  __IO uint32_t SHP[2];                  /*!< Offset: 0x01C (R/W)  System Handlers Priority Registers. [0] is RESERVED   */
  __IO uint32_t SHCSR;                   /*!< Offset: 0x024 (R/W)  System Handler Control and State Register             */
} SCB_Type;

void irq_handler_dummy(void)
{
  while (1);
}

__attribute__((naked, used)) void irq_handler_hard_fault(void)
{
	__asm__ __volatile__(
		"ldr r0, =BootDiagnostic\n"
		"ldr r1, =0x45565344\n"
		"str r1, [r0, #0]\n"
		"movs r1, #255\n"
		"str r1, [r0, #4]\n"
		"mov r1, lr\n"
		"str r1, [r0, #8]\n"
		"mrs r2, msp\n"
		"str r2, [r0, #12]\n"
		"mrs r1, psp\n"
		"str r1, [r0, #16]\n"
		"ldr r3, =0x20000000\n"
		"cmp r2, r3\n"
		"blo 1f\n"
		"ldr r3, =0x20001000\n"
		"cmp r2, r3\n"
		"bhs 1f\n"
		"ldr r1, [r2, #24]\n"
		"str r1, [r0, #20]\n"
		"ldr r1, [r2, #20]\n"
		"str r1, [r0, #24]\n"
		"1:\n"
		"b 1b\n"
	);
}

void irq_handler_reset(void)
{
  set_boot_stage(BOOT_STAGE_RESET_ENTRY);

  unsigned int *src, *dst;

  src = &_etext;
  dst = &_data;
  while (dst < &_edata)
    *dst++ = *src++;

  dst = &_bss;
  while (dst < &_ebss)
    *dst++ = 0;

  SCB->VTOR = (uint32_t)vectors;

  main();

  while (1);
}

void irq_handler_sercom1(void)
{
}

void  enableDMA ()
{
	if (dmabool == false)
	{
		dmabool = true;
		DMAdescriptor.BTCNT = 8;//BTCNT, number of beats per transaction. We're moving 8 ADC results each time.
		DMAdescriptor.SRCADDR = (uint32_t) &REG_ADC_RESULT;//Source address
		DMAdescriptor.DSTADDR = (uint32_t) &DMAresults[DMAresultIndex+1][0] ;//Destination address + (transaction length), see manual
		DMAdescriptor.DESCADDR = 0;
		REG_DMAC_CHID = 0;
		REG_DMAC_CHCTRLA = REG_DMAC_CHCTRLA | 2;//Enable the DMA channel;
	}
}

void irq_handler_dmac(void) //We've configured it to enable Channel Transfer Complete interrupt and Channel Transfer Error interrupt.
{

	uint8_t lastindex = DMAresultIndex;

	DMAresultIndex++;
	if (DMAresultIndex > 5)
		DMAresultIndex = 0;

	REG_DMAC_CHID = REG_DMAC_INTPEND & 7; //These bits store the lowest channel number with pending interrupts.
	uint8_t CHintflag = REG_DMAC_CHINTFLAG;
	if ((CHintflag & 2) == 2) //TCMPL: Transfer Complete. This flag is set when a block transfer is completed and the corresponding interrupt block action is enabled
	{
		REG_DMAC_CHINTFLAG = 2; //This flag is cleared by writing a one to it
		dmabool = false;
	}

	if ((CHintflag & 1) == 1) //Transfer Error. This flag is set when a bus error is detected during a beat transfer or when the DMAC fetches an invalid descriptor
	{
		REG_DMAC_CHINTFLAG = 1; //This flag is cleared by writing a one to it
		dmabool = false;
		SpiPendingFlags |= SPI_FLAG_DMA_ERROR;
	}

	__asm__ __volatile__("dmb sy" ::: "memory");

	uint8_t Muxnr = MuxCounter;
	MuxCounter++;
	MuxCounter = MuxCounter & 7; //max 7

	REG_PORT_OUTSET = 0x2000000;//00000010 00000000 00000000 00000000, so pin 25 high.
	REG_PORT_OUT = ((REG_PORT_OUT ^ outputpinTable[MuxCounter]) & 0x1030000) ^ REG_PORT_OUT; //0x1030000 = 00000001 00000011 00000000 00000000 = pins 16, 17, 24.

	enableDMA();

	capture_spi_scan(lastindex, Muxnr);

	REG_PORT_OUTCLR = 0x2000000;//set pin 25 low.
}

void enableADC()
{
	REG_ADC_CTRLA = REG_ADC_CTRLA | 2;
	do {
	}   while ((REG_ADC_STATUS & STATUS_SYNCBUSY_BIT) != 0);
}

void enable_TC1()
{
	REG_TC1_CTRLA = REG_TC1_CTRLA | 2;
	do {
	} while (REG_TC1_STATUS >= 0x80); //We defined it as unsigned, checking for bit 7, SYNCBUSY.
}

static void spi_write_byte_queued(uint8_t value)
{
	while ((REG_SERCOM1_INTFLAG & 1) == 0) // DRE
	{
	}
	REG_SERCOM1_DATA = value;
	if ((REG_SERCOM1_INTFLAG & 4) != 0) // RXC, clear previous dummy received data
		(void) REG_SERCOM1_DATA;
}

static void spi_frame_guard_delay(void)
{
	for (uint8_t index = 0; index < 32; index++)
		__asm__ __volatile__("nop");
}

static void spi_frame_cs_assert(void)
{
	REG_PORT_OUTCLR = SPI_FRAME_CS_PIN_MASK;
	spi_frame_guard_delay();
}

static void spi_frame_cs_deassert(void)
{
	spi_frame_guard_delay();
	REG_PORT_OUTSET = SPI_FRAME_CS_PIN_MASK;
}

static void transmit_spi_frame_if_ready(void)
{
	if (SpiReadyCount == 0)
		return;

	const uint8_t frame_index = SpiReadyFrameIndex[SpiReadyHead];
	const struct SpiRawFrame* frame = &SpiFrames[frame_index];
	const uint8_t* bytes = (const uint8_t*) frame;
	uint32_t crc = 0xFFFFFFFFUL;

	spi_frame_cs_assert();
	for (uint16_t index = 0; index < SPI_FRAME_SIZE - SPI_CRC_SIZE; index++)
	{
		crc = crc32_update_byte(crc, bytes[index]);
		spi_write_byte_queued(bytes[index]);
	}
	crc ^= 0xFFFFFFFFUL;
	spi_write_byte_queued((uint8_t) crc);
	spi_write_byte_queued((uint8_t) (crc >> 8));
	spi_write_byte_queued((uint8_t) (crc >> 16));
	spi_write_byte_queued((uint8_t) (crc >> 24));
	while ((REG_SERCOM1_INTFLAG & 2) == 0) // TXC
	{
	}
	if ((REG_SERCOM1_INTFLAG & 4) != 0)
		(void) REG_SERCOM1_DATA;
	spi_frame_cs_deassert();
	const uint32_t primask = spi_enter_critical();
	SpiReadyHead = (uint8_t) ((SpiReadyHead + 1) % SPI_READY_QUEUE_CAPACITY);
	SpiReadyCount--;
	spi_exit_critical(primask);
}

void COnfigSerCom1 ()
{
	REG_GCLK_CLKCTRL = 0x410F; // Clock Enable, GCLK1, GCLK_SERCOM1_CORE = 16 MHz.
	REG_SERCOM1_CTRLA = 1; // SWRST
	do {
	} while (REG_SERCOM1_SYNCBUSY != 0);

	REG_SERCOM1_BAUD = 0; // 16 MHz core clock / 2 = 8 MHz SPI.
	REG_SERCOM1_CTRLB = 0;
	do {
	} while (REG_SERCOM1_SYNCBUSY != 0);

	REG_SERCOM1_CTRLA = 0x0C; // SPI master, mode 0, MSB first, DO=PAD0(PA22), SCK=PAD1(PA23).
	REG_SERCOM1_CTRLA = REG_SERCOM1_CTRLA | 2; // ENABLE
	do {
	} while (REG_SERCOM1_SYNCBUSY != 0);
}

void configureNestedVectoredInterruptController ()
{
	__asm__ __volatile__("dmb sy");
	__asm__ __volatile__("CPSIE I");
	REG_NVIC_PRIO1 = (REG_NVIC_PRIO1 & 0xFF00FFFF) | 0x400000;
	REG_NVIC_SETENA = 0x40; //Enable interrupt: 01000000 = int 6 = our DMA IRQ.
	REG_NVIC_PRIO3 = (REG_NVIC_PRIO3 & 0xFFFF00FF) | 0xC000;
	REG_NVIC_SETENA = 0x2000; //Enable interrupt: 00100000 00000000 = int 13, why ??
}

void configureDirectMemoryAccessController ()
{
	REG_DMAC_BASEADDR = (uint32_t) &DMAdescriptor;
	DMAdescriptor.BTCTRL = 0x909; //0000 1001 0000 1001.  stepsize = 0, So next address = beatsize*1
					   //STEPSEL = 0, so DST
					   // DSTINC = 1, so auto increment destination
					   // SRCINC = 0, so no auto increment on source.
					   //Beatsize = 16 bit
					   // blockact = 1 = INT = Channel in normal operation and block interrupt
					   //Event Output Selection = 0, DISABLE, no event generation
					   //valid.
	REG_DMAC_WRBADDR  = (uint32_t) &DMAdescriptorwriteback; //Write-Back Memory Section Base Address, 0x10 bytes long
	REG_DMAC_PRICTRL0 = 0x81818181; //10000001 10000001 10000001 10000001, Round-robin scheduling scheme for channels with level 3 priority.
	REG_DMAC_CHID = 0;//Channel ID 0
	REG_DMAC_CHCTRLB = 0x801200 ; // 00000000 10000000 00010010 00000000
					//Software Command: no action
					//Trigger action: BEAT. One trigger required for each beat transfer
					//Channel resume operation
					//Trigger source = ADC Result Ready Trigger
					//Channel priority level 0
					//Channel event generation is disabled.
					//Channel event action will not be executed on any incoming event.
					//Event Input Action = NO action
	REG_DMAC_CHINTENSET = 3;//enable Channel Transfer Complete interrupt and Channel Transfer Error interrupt.
	REG_DMAC_CTRL = 0xF02; //00001111 00000010 = Transfer requests for all Priority levels are handled. No CRC. DMA enable.
}

void config_PORT()
{
	REG_PORT_DIR = 0x83030000; // PA16, PA17, PA24, PA25 mux outputs plus PA31 FRAME/CS.
	REG_PORT_OUT = SPI_FRAME_CS_PIN_MASK; // FRAME/CS idle high, rest driven low.
	REG_PINCFG2 = 1;
	REG_PINCFG3 = 1;
	REG_PINCFG4 = 1;
	REG_PINCFG5 = 1;
	REG_PINCFG6 = 1;
	REG_PINCFG7 = 1;
	REG_PINCFG8 = 4;
	REG_PINCFG9 = 4;
	REG_PINCFG10 = 1;
	REG_PINCFG11 = 1;
	REG_PINCFG14 = 1;
	REG_PINCFG15 = 1;
	REG_PINCFG22 = 5;
	REG_PINCFG23 = 5;
	REG_PINCFG25 = 4;
	REG_PINCFG27 = 4;
	REG_PINCFG28 = 4;
	REG_PINCFG31 = 0;
	// PA30/SWCLK and reset stay untouched so connect-under-reset remains available.
	REG_PORT_PMUX1 = 0x11;
	REG_PORT_PMUX2 = 0x11;
	REG_PORT_PMUX3 = 0x11;
	REG_PORT_PMUX5 = 0x11;
	REG_PORT_PMUX7 = 0x11;
	REG_PORT_PMUX11 = 0x22;
}

void ConfigureTimerCounter1 () {
	REG_TC1_CTRLA = 1; //SWRST
	do {
	} while (REG_TC1_STATUS >= 0x80); //We defined it as unsigned, checking for bit 7, SYNCBUSY.

	REG_TC1_CTRLA = 0x60; //0110 0000 Match PWM, 16 bit mode
	REG_TC1_COUNT16_CC0 = TC1_ADC_TRIGGER_PERIOD; //The period.
	REG_TC1_INTFLAG = 0x3B; //clear flags
	REG_TC1_EVCTRL = 0x100; // bit 8 = Overflow/Underflow event is enabled and will be generated for every counter overflow/underflow.
	do {
	} while (REG_TC1_STATUS >= 0x80); //We defined it as unsigned, checking for bit 7, SYNCBUSY.
}

void Config_NVMCTRL ()
{
	REG_NVMCTRL_CTRLB = 0x82; //1000 0010, Write commands must be issued through the CMD register. 1 wait states for a read operation
}

void config_EventSystem ()
{
	REG_EVSYS_USER = 0x10C; //0001 00001100 connect channel 0, ADC_START as Event user
	REG_EVSYS_CHANNEL = 0x61F0000; // 0000 0110 0001 1111 00000000 00000000, EVGEN: 00011111 = TC1 OVF = TC1 underflow/overflow
}

void config_Sysctrl_PM_and_GCLK ()
{
	REG_SYSCTRL_OSC32K = 0;
	REG_SYSCTRL_DFLLCTRL = REG_SYSCTRL_DFLLCTRL & 0xFF7F; // & 1111111101111111 = The oscillator is always on, if enabled.
	do {
	} while ((REG_SYSCTRL_PCLKSR & 0x10) == 0); //DFLLRDY. Wait for oscillator stabilization

	uint32_t calibdat = *((uint32_t*)0x806024) >> 0x1a;
	if (calibdat == 0x3f)
		calibdat = 0x1f;

	calibdat = calibdat << 10;
	REG_SYSCTRL_DFLLVAL = calibdat | *((uint32_t*)0x806028) & 0x3ff;
	REG_SYSCTRL_DFLLCTRL = 2; //The DFLL oscillator is enabled.

	do {
	} while ((REG_SYSCTRL_PCLKSR & 0x10) == 0); //DFLLRDY. Wait for oscillator stabilization

	REG_GCLK_GENCTRL = 0x10700; //0000 0001 - 0000 0111 - 0000 0000 GCLKGEN0. Source  = DFLL48M output. The generic clock generator is enabled

	do {
	} while ((REG_GCLK_STATUS & STATUS_SYNCBUSY_BIT) != 0);

	REG_GCLK_GENCTRL = 0x30701; //0000 0011 - 0000 0111 - 0000 0001 GCLKGEN1. Source = DFLL48M output. The generic clock generator is enabled
	REG_GCLK_GENDIV = 0x301; //0000 0011 0000 0001. GCLKGEN1 3 division bits.

	do {
	} while ((REG_GCLK_STATUS & STATUS_SYNCBUSY_BIT) != 0);

	REG_GCLK_CLKCTRL = 0x4007; //0100 0000 0000 0111, enable GCLK0 EVSYS_CHANNEL_0
	REG_GCLK_CLKCTRL = 0x410F; //0100 0001 0000 1111, enable GCLK1 SERCOM1_CORE
	REG_GCLK_CLKCTRL = 0x4112; //0100 0001 0001 0010, enable GCLK1 TC2
	REG_GCLK_CLKCTRL = 0x4113; //0100 0001 0001 0011, enable GCLK1 ADC

	REG_PM_APBCMASK = 0x14A; //0000 0001 0100 1010 //enable: ADC, TC1, SERCOM1
	REG_SYSCTRL_OSC8M = 0;
}

void adc_config() {

	REG_ADC_CTRLA = 1; //SWRST: Writing a one to this bit resets all registers in the ADC,to their initial state, and the ADC will be disabled
	do {
	}   while ((REG_ADC_STATUS & STATUS_SYNCBUSY_BIT) != 0);

        //Load ADC factory calibration values
	uint32_t tmp =  (*((uint32_t*)0x806024) << 5) & 0x700 | *((uint32_t*)0x806020) >> 27 | (*((uint32_t*)0x806024) << 5) & 0xff | ((*((uint32_t*)0x806024) & 7) << 5);
	REG_ADC_CALIB = tmp;
	REG_ADC_SAMPCTRL = ADC_SAMPLE_TIME_LENGTH; //Sampling Time Length
	REG_ADC_REFCTRL = ADC_REFCTRL_VREFA; //VREFA as reference
	REG_ADC_INPUTCTRL = ADC_INPUTCTRL_SCAN8_DIFF_GAIN2; // 00000001 00100111 00000000 00000000
					//  00000001 = GAIN 2X.
					// 0010  =inputoffset: 2.
					// 0111 = Inputscan= 7+1 = 8
	REG_ADC_CTRLB = ADC_CTRLB_DIV8_12BIT_DIFF; //0000 0001 0000 0001
				//ADC clock : Peripheral clock = 1 : 8
				//12 bits conversion
				//Disable digital result correction
				//The ADC conversion result is right-adjusted in the RESULT register.
				//Single conversion mode
				//Differential mode. In this mode, the voltage difference between the MUXPOS and MUXNEG inputs will be converted by the ADC
	REG_ADC_INTFLAG = 0xF; //clear intflags
	REG_ADC_EVCTRL = 1; //A new conversion will be triggered on any incoming event
	do {
	}  while ((REG_ADC_STATUS & STATUS_SYNCBUSY_BIT) != 0);
}

int main(void)
{
	set_boot_stage(BOOT_STAGE_MAIN_ENTRY);
	REG_NVMCTRL_CTRLB = 6;
	set_boot_stage(BOOT_STAGE_NVM_READ_WAIT);

	config_PORT();
	set_boot_stage(BOOT_STAGE_PORT_CONFIGURED);
	config_Sysctrl_PM_and_GCLK ();
	set_boot_stage(BOOT_STAGE_CLOCK_CONFIGURED);
	Config_NVMCTRL();
	set_boot_stage(BOOT_STAGE_NVM_MANUAL_WRITE);
	config_EventSystem();
	set_boot_stage(BOOT_STAGE_EVENT_SYSTEM_CONFIGURED);
	configureDirectMemoryAccessController();
	set_boot_stage(BOOT_STAGE_DMAC_CONFIGURED);
	adc_config();
	set_boot_stage(BOOT_STAGE_ADC_CONFIGURED);
	ConfigureTimerCounter1 ();
	set_boot_stage(BOOT_STAGE_TC1_CONFIGURED);
	configureNestedVectoredInterruptController();
	set_boot_stage(BOOT_STAGE_NVIC_CONFIGURED);
	enableDMA ();
	set_boot_stage(BOOT_STAGE_DMA_ENABLED);
	enableADC ();
	set_boot_stage(BOOT_STAGE_ADC_ENABLED);
	enable_TC1();
	set_boot_stage(BOOT_STAGE_TC1_ENABLED);
	COnfigSerCom1();
	set_boot_stage(BOOT_STAGE_SERCOM1_CONFIGURED);

	set_boot_stage(BOOT_STAGE_MAIN_LOOP);
	for (;;) //main program loop
	{
		transmit_spi_frame_if_ready();
	}
	return 0;
}
