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
#define UINT32_MAX 4294967295UL
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

#define REG_GCLK_CLKCTRL           (*(RwReg16*)0x40000C02UL) /**< \brief (GCLK) Generic Clock Control */

#define REG_SERCOM1_I2CS_CTRLA     (*(RwReg  *)0x42000C00UL) /**< \brief (SERCOM1) I2CM Control A */
#define REG_SERCOM1_I2CS_CTRLB     (*(RwReg  *)0x42000C04UL) /**< \brief (SERCOM1) I2CM Control B */
#define REG_SERCOM1_I2CS_INTENSET  (*(RwReg8 *)0x42000C16UL) /**< \brief (SERCOM1) I2CM Interrupt Enable Set */
#define REG_SERCOM1_I2CS_INTFLAG   (*(RwReg8 *)0x42000C18UL) /**< \brief (SERCOM1) I2CS Interrupt Flag Status and Clear */
#define REG_SERCOM1_I2CS_STATUS    (*(RwReg16*)0x42000C1AUL) /**< \brief (SERCOM1) I2CS Status */
#define REG_SERCOM1_I2CS_SYNCBUSY  (*(RoReg  *)0x42000C1CUL) /**< \brief (SERCOM1) I2CM Syncbusy */
#define REG_SERCOM1_I2CS_ADDR      (*(RwReg  *)0x42000C24UL) /**< \brief (SERCOM1) I2CS Address */
#define REG_SERCOM1_I2CS_DATA      (*(RwReg8 *)0x42000C28UL) /**< \brief (SERCOM1) I2CS Data */

#define STATUS_SYNCBUSY_BIT        0x80

#define SCB_VTOR_TBLOFF_Pos                 7                                             /*!< SCB VTOR: TBLOFF Position */
#define SCB_VTOR_TBLOFF_Msk                (0x1FFFFFFUL << SCB_VTOR_TBLOFF_Pos)           /*!< SCB VTOR: TBLOFF Mask */

#define DUMMY __attribute__ ((weak, alias ("irq_handler_dummy")))

#define BOOT_DIAG_MAGIC 0x45565344UL
#define BOOT_STAGE_RESET_ENTRY 1
#define BOOT_STAGE_MAIN_ENTRY 10
#define BOOT_STAGE_NVM_READ_WAIT 11
#define BOOT_STAGE_CALCBLOCK_CLEARED 12
#define BOOT_STAGE_SENSOR_CLEARED 13
#define BOOT_STAGE_MANAGED_INFO_READY 14
#define BOOT_STAGE_AVERAGES_CLEARED 15
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

bool alldataready = false;
bool dmabool = false;
volatile bool packet_ready = false;
uint32_t ESPbyteIndex = 0;

uint8_t temp = 0;
uint8_t ManagedReadCommand = 0;
uint8_t ESPReadBufferIndex = 0;
uint8_t DMAresultIndex = 0;
int16_t DMAresults[6][8]; //We copy the 8 ADC results to this buffer using DMA
			//Layout: MainCT1_V, MainCT1_A, MainCT2_V, MainCT2_A, MainCT_3_V, MainCT_3_A, Mux1_A, Mux2_A

//Something temporarily, currently to analyze results
//uint16_t testindex = 0;
//uint16_t testresults[0x200];

uint8_t MuxCounter = 0; //Varies between 0 and 7 to switch between the 8 muxes, each serving 2 50A CT's
const uint32_t outputpinTable [8] = { 0x1000000, 0x1010000, 0x1020000, 0x1030000, 0, 0x10000, 0x20000, 0x30000 }; //all possible combinations of pin 16, 17 and 24.
int16_t averages[22]; //Here we save the averages: 3x voltages, 3x  Main CT current, 16x small CT current
int32_t OffsetEstimateQ8[22];
uint8_t OffsetWarmupWindows = 0;
const uint8_t MuxTable[16] = { 12, 4, 13, 5, 14, 6, 11, 3, 15, 7, 18, 10, 16, 8, 17, 9 }; //used for the order of saving Current data to the final output packet

volatile uint32_t DiagSequence = 0;
volatile uint32_t DiagSampleBlocks = 0;
volatile uint32_t DiagPacketsBuilt = 0;
volatile uint32_t DiagPacketsRead = 0;
volatile uint32_t DiagDmaTransferErrors = 0;
volatile uint32_t DiagPacketOverruns = 0;
volatile uint32_t DiagI2cPartialReads = 0;
volatile uint32_t DiagI2cOversizeReads = 0;
volatile uint32_t DiagPowerTimingLatestMinus2Max = 0;
volatile uint32_t DiagPowerTimingLatestMinus4Max = 0;
volatile uint32_t DiagPowerTimingMinus2Minus4Max = 0;
volatile uint32_t DiagPowerTimingLatestMinus2MeanSum = 0;
volatile uint32_t DiagPowerTimingLatestMinus4MeanSum = 0;
volatile uint32_t DiagPowerTimingMinus2Minus4MeanSum = 0;
volatile uint32_t DiagPowerTimingMeanSamples = 0;
volatile uint16_t DiagLastSampleCount = 0;
volatile uint16_t DiagLastI2cReadLen = 0;

const uint32_t EORTable[64] = { 0x90E0700, 0x15121B1C, 0x31363F38, 0x2D2A2324, 0x797E7770, 0x65626B6C, 0x41464F48, 0x5D5A5354,
			   0xE9EEE7E0, 0xF5F2FBFC, 0xD1D6DFD8, 0xCDCAC3C4, 0x999E9790, 0x85828B8C, 0xA1A6AFA8, 0xBDBAB3B4,
			   0xCEC9C0C7, 0xD2D5DCDB, 0xF6F1F8FF, 0xEAEDE4E3, 0xBEB9B0B7, 0xA2A5ACAB, 0x8681888F, 0x9A9D9493,
			   0x2E292027, 0x32353C3B, 0x1611181F, 0xA0D0403, 0x5E595057, 0x42454C4B, 0x6661686F, 0x7A7D7473,
			   0x80878E89, 0x9C9B9295, 0xB8BFB6B1, 0xA4A3AAAD, 0xF0F7FEF9, 0xECEBE2E5, 0xC8CFC6C1, 0xD4D3DADD,
			   0x60676E69,0x7C7B7275, 0x585F5651, 0x44434A4D, 0x10171E19, 0xC0B0205, 0x282F2621, 0x34333A3D,
			   0x4740494E, 0x5B5C5552, 0x7F787176, 0x63646D6A, 0x3730393E, 0x2B2C2522, 0xF080106, 0x13141D1A,
			   0xA7A0A9AE, 0xBBBCB5B2, 0x9F989196, 0x83848D8A, 0xD7D0D9DE, 0xCBCCC5C2, 0xEFE8E1E6, 0xF3F4FDFA };

struct __attribute__((__packed__)) ReadingPowerEntry
{
    int32_t phase[3];
};

typedef struct ReadingPowerEntry ReadingPowerEntry;

struct __attribute__((__packed__)) SensorReadingType
{
    uint8_t is_unread;
    uint8_t checksum;
    uint8_t unknown;
    uint8_t sequence_num;

    ReadingPowerEntry power[19];

    uint16_t voltage[3];
    uint16_t Cyclecount[3];  // First one is the amount of samples between Ct1 V zero crossings. 2nd and 3rd are CT2 and CT3 crossing after CT1 crossed.
    uint16_t current[19];

    uint16_t end;
};

struct SensorReadingType SensorReadings[2];
volatile uint8_t ActiveSensorReadingIndex = 0;
uint8_t BuildSensorReadingIndex = 1;
uint8_t SensorSequence = 0;

#define ESPpacketlength       0x11C
#define EMPORIAVUE_HARDWARE_ID       2
#define EMPORIAVUE_FIRMWARE_VERSION  26
#define EMPORIAVUE_I2C_INFO_COMMAND  0xF0
#define EMPORIAVUE_I2C_DIAGNOSTIC_COMMAND 0xF1

#define MANAGED_READ_COMMAND_NONE       0
#define MANAGED_READ_COMMAND_INFO       1
#define MANAGED_READ_COMMAND_DIAGNOSTIC 2

#define MAIN_SAMPLE_COUNT              12987
#define MUX_SAMPLE_COUNT               1623
#define MAIN_RMS_SCALE_NUMERATOR       100
#define MUX_RMS_SCALE_NUMERATOR        800
#define MAIN_POWER_SCALE_MULTIPLIER    10
#define MUX_POWER_SCALE_MULTIPLIER     80
#define ADC_OFFSET_CHANNEL_COUNT       22
#define ADC_OFFSET_Q_SHIFT             8
#define ADC_OFFSET_INVALID_MIN         -1999
#define ADC_OFFSET_STARTUP_WINDOWS     4
#define ADC_OFFSET_SMOOTHING_SHIFT     4
#define ADC_OFFSET_MAX_STEP            96
#define POWER_TIMING_DIAG_CURRENT_THRESHOLD 8
#define POWER_TIMING_DIAG_DECIMATION_MASK 15
#define POWER_TIMING_DIAG_MEAN_SHIFT 16

#define TC1_ADC_TRIGGER_PERIOD         0x4C
#define ADC_SAMPLE_TIME_LENGTH         1
#define ADC_REFCTRL_VREFA              3
#define ADC_INPUTCTRL_SCAN8_DIFF_GAIN2 0x1270000UL
#define ADC_CTRLB_DIV8_12BIT_DIFF      0x101

struct __attribute__((__packed__)) ManagedInfoType
{
	uint16_t hardware_id;
	uint32_t firmware_version;
	uint16_t i2c_frame_length;
	uint32_t crc32;
} ManagedInfo;

struct __attribute__((__packed__)) ManagedDiagnosticType
{
	uint16_t hardware_id;
	uint32_t firmware_version;
	uint16_t i2c_frame_length;
	uint32_t diagnostic_sequence;
	uint32_t sample_blocks;
	uint32_t packets_built;
	uint32_t packets_read;
	uint32_t dma_transfer_errors;
	uint32_t packet_overruns;
	uint32_t i2c_partial_reads;
	uint32_t i2c_oversize_reads;
	uint32_t power_timing_latest_minus2_max;
	uint32_t power_timing_latest_minus4_max;
	uint32_t power_timing_minus2_minus4_max;
	uint32_t power_timing_latest_minus2_mean_abs;
	uint32_t power_timing_latest_minus4_mean_abs;
	uint32_t power_timing_minus2_minus4_mean_abs;
	uint16_t last_sample_count;
	uint16_t last_i2c_read_len;
	uint32_t crc32;
} ManagedDiagnostic;

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

#define __SIZE_OF_VAR__(x) ((char *)(&x + 1) - (char *)&x)

struct CalcBlocType {
int32_t ADCCurrentsum[19]; //all ADC currents summed up. Max 12987 times FFFF = 32BA CD45, so 4 bytes are enough.
int64_t ADCVoltsquaresum[3];
int32_t ADCVoltagesum[3];
int64_t ADCsquareCurrentsum[19];
int64_t RawPVsum[19][3];

uint32_t CtCycles[3]; //First one counts the amount of cycles between the voltage1 zerocrossings to the positive.
		       //The next two the zerocrossings for those 2 CT's after that CT1 crossing.
uint16_t AmountCtCycles[3];
uint16_t SampleCounter; //Keeps track of the amount of samples it has taken for each ESP packet.
} calcblock[2]; //double buffered

static uint16_t scale_rms_exact(uint64_t sum, uint32_t numerator)
{
	uint64_t scaled = sum * numerator;
	uint32_t low = 0;
	uint32_t high = 65535;
	uint32_t result = 0;

	while (low <= high)
	{
		uint32_t mid = (low + high) >> 1;
		uint64_t threshold = (uint64_t) mid * (uint64_t) mid * MAIN_SAMPLE_COUNT;
		if (threshold <= scaled)
		{
			result = mid;
			low = mid + 1;
		}
		else
		{
			if (mid == 0)
				break;
			high = mid - 1;
		}
	}

	return (uint16_t) result;
}

__attribute__((noinline))
static uint32_t div_u32_by_u16(uint32_t numerator, uint16_t denominator)
{
	uint32_t quotient = 0;
	uint32_t remainder = 0;
	uint32_t bit = 0x80000000UL;

	while (bit != 0)
	{
		remainder <<= 1;
		if ((numerator & bit) != 0)
			remainder++;
		if (remainder >= denominator)
		{
			remainder -= denominator;
			quotient |= bit;
		}
		bit >>= 1;
	}

	return quotient;
}

__attribute__((noinline))
static uint64_t div_u64_by_u16(uint64_t numerator, uint16_t denominator)
{
	uint64_t quotient = 0;
	uint32_t remainder = 0;
	uint64_t bit = ((uint64_t) 1) << 63;

	while (bit != 0)
	{
		remainder <<= 1;
		if ((numerator & bit) != 0)
			remainder++;
		if (remainder >= denominator)
		{
			remainder -= denominator;
			quotient |= bit;
		}
		bit >>= 1;
	}

	return quotient;
}

__attribute__((noinline))
static int32_t div_s32_by_u16(int32_t value, uint16_t denominator)
{
	if (value < 0)
	{
		uint32_t magnitude = (uint32_t) (-(value + 1)) + 1;
		return -(int32_t) div_u32_by_u16(magnitude, denominator);
	}
	return (int32_t) div_u32_by_u16((uint32_t) value, denominator);
}

__attribute__((noinline))
static int64_t div_s64_by_u16(int64_t value, uint16_t denominator)
{
	if (value < 0)
	{
		uint64_t magnitude = (uint64_t) (-(value + 1)) + 1;
		return -(int64_t) div_u64_by_u16(magnitude, denominator);
	}
	return (int64_t) div_u64_by_u16((uint64_t) value, denominator);
}

static inline uint16_t scale_rms_main(uint64_t sum)
{
	return scale_rms_exact(sum, MAIN_RMS_SCALE_NUMERATOR);
}

static inline uint16_t scale_rms_mux(uint64_t sum)
{
	return scale_rms_exact(sum, MUX_RMS_SCALE_NUMERATOR);
}

static inline int32_t scale_power_main(int64_t raw)
{
	return (int32_t) div_s64_by_u16(raw * MAIN_POWER_SCALE_MULTIPLIER, MAIN_SAMPLE_COUNT);
}

static inline int32_t scale_power_mux(int64_t raw)
{
	return (int32_t) div_s64_by_u16(raw * MUX_POWER_SCALE_MULTIPLIER, MAIN_SAMPLE_COUNT);
}

static inline int32_t clamp_i32(int32_t value, int32_t min, int32_t max)
{
	if (value < min)
		return min;
	if (value > max)
		return max;
	return value;
}

static inline uint32_t abs_i32_to_u32(int32_t value)
{
	if (value < 0)
		return (uint32_t) (-(value + 1)) + 1;
	return (uint32_t) value;
}

static inline void update_max_u32(volatile uint32_t *target, uint32_t value)
{
	if (value > *target)
		*target = value;
}

static inline void add_saturated_u32(volatile uint32_t *target, uint32_t value)
{
	if (UINT32_MAX - *target < value)
		*target = UINT32_MAX;
	else
		*target += value;
}

static inline void update_power_timing_diagnostic(int32_t current_difference, int32_t latest_minus2,
						  int32_t latest_minus4, int32_t minus2_minus4)
{
	uint32_t latest_minus2_abs = abs_i32_to_u32(current_difference * latest_minus2);
	uint32_t latest_minus4_abs = abs_i32_to_u32(current_difference * latest_minus4);
	uint32_t minus2_minus4_abs = abs_i32_to_u32(current_difference * minus2_minus4);

	update_max_u32(&DiagPowerTimingLatestMinus2Max, latest_minus2_abs);
	update_max_u32(&DiagPowerTimingLatestMinus4Max, latest_minus4_abs);
	update_max_u32(&DiagPowerTimingMinus2Minus4Max, minus2_minus4_abs);
	add_saturated_u32(&DiagPowerTimingLatestMinus2MeanSum, latest_minus2_abs >> POWER_TIMING_DIAG_MEAN_SHIFT);
	add_saturated_u32(&DiagPowerTimingLatestMinus4MeanSum, latest_minus4_abs >> POWER_TIMING_DIAG_MEAN_SHIFT);
	add_saturated_u32(&DiagPowerTimingMinus2Minus4MeanSum, minus2_minus4_abs >> POWER_TIMING_DIAG_MEAN_SHIFT);
	DiagPowerTimingMeanSamples++;
}

static inline int16_t sanitize_adc_offset_target(int32_t average)
{
	if (average < ADC_OFFSET_INVALID_MIN)
		return 0;
	if (average > INT16_MAX)
		return INT16_MAX;
	if (average < INT16_MIN)
		return INT16_MIN;
	return (int16_t) average;
}

__attribute__((noinline))
static void update_adc_offset(uint8_t channel, int32_t window_average)
{
	int32_t target_q8 = ((int32_t) sanitize_adc_offset_target(window_average)) << ADC_OFFSET_Q_SHIFT;
	if (OffsetWarmupWindows < ADC_OFFSET_STARTUP_WINDOWS)
	{
		OffsetEstimateQ8[channel] = target_q8;
	}
	else
	{
		const int32_t max_step_q8 = ((int32_t) ADC_OFFSET_MAX_STEP) << ADC_OFFSET_Q_SHIFT;
		int32_t delta_q8 = target_q8 - OffsetEstimateQ8[channel];
		delta_q8 = clamp_i32(delta_q8, -max_step_q8, max_step_q8);
		if (delta_q8 >= 0)
			OffsetEstimateQ8[channel] += delta_q8 >> ADC_OFFSET_SMOOTHING_SHIFT;
		else
			OffsetEstimateQ8[channel] -= (-delta_q8) >> ADC_OFFSET_SMOOTHING_SHIFT;
	}
	averages[channel] = (int16_t) (OffsetEstimateQ8[channel] >> ADC_OFFSET_Q_SHIFT);
}

volatile int8_t cbi = 0; //calcblock index varies between 0 and 1 for the double buffer
uint16_t FreqCT = 0; //used to calculate the amount of samples between Voltage (CT1) zerocrossings.
uint8_t CTC[3] = { 0, 0, 0 }; //used to calculate FreqCT and the voltage degrees

extern unsigned int _etext;
extern unsigned int _data;
extern unsigned int _edata;
extern unsigned int _bss;
extern unsigned int _ebss;
extern int main(void);

void irq_handler_reset(void);
void irq_handler_dmac(void);
void irq_handler_sercom1(void);
uint32_t crc32(const uint8_t *data, unsigned int length);
void configure_managed_i2c_info(void);
void configure_managed_i2c_diagnostic(void);
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

uint32_t crc32(const uint8_t *data, unsigned int length)
{
	uint32_t crc = 0xFFFFFFFF;
	for (unsigned int index = 0; index < length; index++)
	{
		crc = crc ^ data[index];
		for (uint8_t bit = 0; bit < 8; bit++)
		{
			crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
		}
	}
	return ~crc;
}

void configure_managed_i2c_info(void)
{
	ManagedInfo.hardware_id = EMPORIAVUE_HARDWARE_ID;
	ManagedInfo.firmware_version = EMPORIAVUE_FIRMWARE_VERSION;
	ManagedInfo.i2c_frame_length = ESPpacketlength;
	ManagedInfo.crc32 = crc32((uint8_t*) &ManagedInfo, __SIZE_OF_VAR__(ManagedInfo) - __SIZE_OF_VAR__(ManagedInfo.crc32));
}

void configure_managed_i2c_diagnostic(void)
{
	uint32_t power_timing_mean_samples = DiagPowerTimingMeanSamples;

	ManagedDiagnostic.hardware_id = EMPORIAVUE_HARDWARE_ID;
	ManagedDiagnostic.firmware_version = EMPORIAVUE_FIRMWARE_VERSION;
	ManagedDiagnostic.i2c_frame_length = ESPpacketlength;
	ManagedDiagnostic.diagnostic_sequence = ++DiagSequence;
	ManagedDiagnostic.sample_blocks = DiagSampleBlocks;
	ManagedDiagnostic.packets_built = DiagPacketsBuilt;
	ManagedDiagnostic.packets_read = DiagPacketsRead;
	ManagedDiagnostic.dma_transfer_errors = DiagDmaTransferErrors;
	ManagedDiagnostic.packet_overruns = DiagPacketOverruns;
	ManagedDiagnostic.i2c_partial_reads = DiagI2cPartialReads;
	ManagedDiagnostic.i2c_oversize_reads = DiagI2cOversizeReads;
	ManagedDiagnostic.power_timing_latest_minus2_max = DiagPowerTimingLatestMinus2Max;
	ManagedDiagnostic.power_timing_latest_minus4_max = DiagPowerTimingLatestMinus4Max;
	ManagedDiagnostic.power_timing_minus2_minus4_max = DiagPowerTimingMinus2Minus4Max;
	ManagedDiagnostic.power_timing_latest_minus2_mean_abs =
		power_timing_mean_samples == 0 ? 0 :
		(DiagPowerTimingLatestMinus2MeanSum / power_timing_mean_samples) << POWER_TIMING_DIAG_MEAN_SHIFT;
	ManagedDiagnostic.power_timing_latest_minus4_mean_abs =
		power_timing_mean_samples == 0 ? 0 :
		(DiagPowerTimingLatestMinus4MeanSum / power_timing_mean_samples) << POWER_TIMING_DIAG_MEAN_SHIFT;
	ManagedDiagnostic.power_timing_minus2_minus4_mean_abs =
		power_timing_mean_samples == 0 ? 0 :
		(DiagPowerTimingMinus2Minus4MeanSum / power_timing_mean_samples) << POWER_TIMING_DIAG_MEAN_SHIFT;
	DiagPowerTimingLatestMinus2Max = 0;
	DiagPowerTimingLatestMinus4Max = 0;
	DiagPowerTimingMinus2Minus4Max = 0;
	DiagPowerTimingLatestMinus2MeanSum = 0;
	DiagPowerTimingLatestMinus4MeanSum = 0;
	DiagPowerTimingMinus2Minus4MeanSum = 0;
	DiagPowerTimingMeanSamples = 0;
	ManagedDiagnostic.last_sample_count = DiagLastSampleCount;
	ManagedDiagnostic.last_i2c_read_len = DiagLastI2cReadLen;
	ManagedDiagnostic.crc32 = crc32((uint8_t*) &ManagedDiagnostic, __SIZE_OF_VAR__(ManagedDiagnostic) - __SIZE_OF_VAR__(ManagedDiagnostic.crc32));
}

void irq_handler_sercom1(void) //We've configured sercom to use IRQ sources "Data Ready" and "Stop received"
{
	if ((REG_SERCOM1_I2CS_INTFLAG & 4) == 4) //Bit 2 – DRDY: Data Ready
	{
		if ((REG_SERCOM1_I2CS_STATUS & 8) == 8) //DIR == 1 = Master read operation is in progress.
		{
			if (ManagedReadCommand == MANAGED_READ_COMMAND_INFO)
			{
				if (ESPbyteIndex < __SIZE_OF_VAR__(ManagedInfo))
				{
					uint8_t* px = (uint8_t*) &ManagedInfo;
					px = px + ESPbyteIndex;
					REG_SERCOM1_I2CS_DATA = *px;
				}
				else
					REG_SERCOM1_I2CS_DATA = 0xFF;

				if (ESPbyteIndex <= __SIZE_OF_VAR__(ManagedInfo))
					ESPbyteIndex++;
			}
			else if (ManagedReadCommand == MANAGED_READ_COMMAND_DIAGNOSTIC)
			{
				if (ESPbyteIndex < __SIZE_OF_VAR__(ManagedDiagnostic))
				{
					uint8_t* px = (uint8_t*) &ManagedDiagnostic;
					px = px + ESPbyteIndex;
					REG_SERCOM1_I2CS_DATA = *px;
				}
				else
					REG_SERCOM1_I2CS_DATA = 0xFF;

				if (ESPbyteIndex <= __SIZE_OF_VAR__(ManagedDiagnostic))
					ESPbyteIndex++;
			}
			else
			{
				if (ESPbyteIndex <  ESPpacketlength)
				{
					uint8_t* px = (uint8_t*) &SensorReadings[ESPReadBufferIndex];
					px = px + ESPbyteIndex;
					REG_SERCOM1_I2CS_DATA = *px; //write data
				}
				else
					REG_SERCOM1_I2CS_DATA = 0xFF;

				if (ESPbyteIndex == 0)
					temp = SensorReadings[ESPReadBufferIndex].is_unread; 	//first byte of data packet.

				if (ESPbyteIndex <=  ESPpacketlength)
					ESPbyteIndex++;
			}
		}
		else {
			uint8_t received = REG_SERCOM1_I2CS_DATA; //read data
			if (received == EMPORIAVUE_I2C_INFO_COMMAND)
			{
				ManagedReadCommand = MANAGED_READ_COMMAND_INFO;
				ESPbyteIndex = 0;
			}
			else if (received == EMPORIAVUE_I2C_DIAGNOSTIC_COMMAND)
			{
				configure_managed_i2c_diagnostic();
				ManagedReadCommand = MANAGED_READ_COMMAND_DIAGNOSTIC;
				ESPbyteIndex = 0;
			}
			else
			{
				ManagedReadCommand = MANAGED_READ_COMMAND_NONE;
				ESPReadBufferIndex = ActiveSensorReadingIndex;
				ESPbyteIndex = received;
			}
		}
	}

	if ((REG_SERCOM1_I2CS_INTFLAG & 1) == 1) //Bit 0 – PREC: Stop Received. This flag is set when a stop condition is detected for a transaction being processed
	{
		REG_SERCOM1_I2CS_INTFLAG = REG_SERCOM1_I2CS_INTFLAG | 1; //Writing a one to this bit will clear the Stop Received interrupt flag.
		if (ManagedReadCommand == MANAGED_READ_COMMAND_NONE)
		{
			DiagLastI2cReadLen = ESPbyteIndex;
			if (ESPbyteIndex >= ESPpacketlength)
			{
				DiagPacketsRead++;
				if (ESPbyteIndex > ESPpacketlength)
					DiagI2cOversizeReads++;
				if (temp != 0)
					SensorReadings[ESPReadBufferIndex].is_unread = 0; //Save 0 to first byte in the Sensordata packet
			}
			else if (ESPbyteIndex > 0)
				DiagI2cPartialReads++;
		}
		ManagedReadCommand = MANAGED_READ_COMMAND_NONE;
		ESPbyteIndex = 0;
		ESPReadBufferIndex = ActiveSensorReadingIndex;
	}
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
		DiagDmaTransferErrors++;
	}

	__asm__ __volatile__("dmb sy" ::: "memory");

	uint8_t Muxnr = MuxCounter;
	MuxCounter++;
	MuxCounter = MuxCounter & 7; //max 7

	REG_PORT_OUTSET = 0x2000000;//00000010 00000000 00000000 00000000, so pin 25 high.
	REG_PORT_OUT = ((REG_PORT_OUT ^ outputpinTable[MuxCounter]) & 0x1030000) ^ REG_PORT_OUT; //0x1030000 = 00000001 00000011 00000000 00000000 = pins 16, 17, 24.

	enableDMA();


	int8_t lastindexMin2 = lastindex - 2;
	if (lastindexMin2 < 0)
		lastindexMin2 = lastindex + 4;

	int8_t lastindexMin4 = lastindex - 4;
	if (lastindexMin4 < 0)
		lastindexMin4 = lastindex + 2;

	int a = 0;
	int dif1;
	int voltage_differences[3];
	int voltage_differences_min2[3];
	int voltage_differences_min4[3];
	int voltage_latest_minus2[3];
	int voltage_latest_minus4[3];
	int voltage_min2_minus4[3];
	bool power_timing_diagnostic_enabled = (DiagSampleBlocks & POWER_TIMING_DIAG_DECIMATION_MASK) == 0;
	FreqCT++;

	//Now process the ADC results!

	//For voltages, 3 main CT amps and the 50A CT's:
	//1. Sum up all ADC results (we use that to calculate the average when we send the ESP packet)
	//2. Difference = Latest ADC result - Average result (from last 0.5 second).
	//3. Sum up the square of that.

	//We use different ADC result buffers though! For the voltages we use the latest buffer. For the mains currents we use
	//the ADC results from 2 buffers ago. And for the 50A CT currents we use the ADC results from 4 buffers ago!

	//For RawPV: Use amp result from point 2 above, multiply with (latest (!) ADC voltage - Average voltage (from last 0.5 second, so the value from 22 values table)).

	for (int ct = 0; ct < 3; ct++)
	{
		voltage_differences[ct] = DMAresults[lastindex][ct * 2] - averages[ct];
		voltage_differences_min2[ct] = DMAresults[lastindexMin2][ct * 2] - averages[ct];
		voltage_differences_min4[ct] = DMAresults[lastindexMin4][ct * 2] - averages[ct];
		voltage_latest_minus2[ct] = voltage_differences[ct] - voltage_differences_min2[ct];
		voltage_latest_minus4[ct] = voltage_differences[ct] - voltage_differences_min4[ct];
		voltage_min2_minus4[ct] = voltage_differences_min2[ct] - voltage_differences_min4[ct];
	}

	//Process the 3 mains first
	for (int ct = 0; ct < 3; ct++)
	{
		//Process the voltages for the 3 mains
		calcblock[cbi].ADCVoltagesum[ct] += DMAresults[lastindex][a]; //Save the sum of all ADC Voltages. At DMAresults 0,2,4. For voltages we always take the latest buffer.
		dif1 = voltage_differences[ct];
		calcblock[cbi].ADCVoltsquaresum[ct] += dif1 * dif1;

		//Now count the zerocrossing cycles for the Voltage1 and the differences with V2 and V3.
		//This will yield the voltage (CT1) frequency and the angle between the voltages.
		//We're going to look for zero crossings (to the positive) of V1 and count the amount of cycles between two of those.
		//We define a zero crossing as:
		// 1) We want to see 40 times a voltage below -100, to confirm we're in the negative part of the wave.
		// 2) Then we'll wait for 3 times a positive in a row! That counts as a crossing.

		//For CT2 and CT3 we count zero crossings (to the positive) after CT1 crossed (to the positive), so that will yield the voltage degrees.

		if (CTC[ct] == 43) // = 40 + 3
		{
			if (dif1 < 1) //See if there's a false positive and the latest voltage actually is negative.
				CTC[ct] = 40;              //If so, reset counter to 40  (indicating we need again 3 positives in a row from now on!)
			else
			{
				calcblock[cbi].CtCycles[ct] += FreqCT; //We've had 40 negatives in a row and then 3 positives in a row!
				calcblock[cbi].AmountCtCycles[ct]++;   //So now save the cycles between zerocrossing and add it to the total for averaging lateron.

				if (ct == 0)
					FreqCT = 0;

				CTC[ct] = 0; //start from 0 again, so we need 40 negatives in a row from now on.
			}

		}
		else if (CTC[ct] < 40)
		{
			if (dif1 >= -100)  //we want a voltage < -100. Otherwise reset counter and we want 40 in a row again!
				CTC[ct] = 0;
			else
				CTC[ct]++; //Increase the amount of times that we've counted voltages < -100
					   //So to confirm we're in the negative zone. We want 40 in a row.
		}
		else if (dif1 < 1)
			CTC[ct] = 40; //hold (or reset to) 40 if we had 40 times a Voltage below -100 in a row, but latest sample is still negative.
		else
			CTC[ct]++; //Now the voltage has gone (zero or) positive and we've seen at least 40 negatives one before!


		//Process the currents for the 3 mains
		calcblock[cbi].ADCCurrentsum[ct] += DMAresults[lastindexMin2][a+1]; //Save the sum of all ADC currents (at DMA 1,3,5)
		dif1 = DMAresults[lastindexMin2][a+1] - averages[ct+3];
		calcblock[cbi].ADCsquareCurrentsum[ct] += dif1 * dif1;


		//Process the RawPV's for the 3 mains
		bool timing_diagnostic_active = power_timing_diagnostic_enabled &&
						abs_i32_to_u32(dif1) >= POWER_TIMING_DIAG_CURRENT_THRESHOLD;
		for (int i = 0; i < 3; i++)
		{
			//int64_t RawPVsum[19][3];
			if (timing_diagnostic_active)
				update_power_timing_diagnostic(dif1, voltage_latest_minus2[i], voltage_latest_minus4[i],
							       voltage_min2_minus4[i]);
			calcblock[cbi].RawPVsum[ct][i] -= dif1 * voltage_differences[i];  //current * volts (at DMAresults 0,2,4)
		}


		a += 2;

	}

	//Process the 2 muxed small CT's
	for (int i = 0; i < 2; i++)
	{
		calcblock[cbi].ADCCurrentsum[3 + (Muxnr*2) + i] += DMAresults[lastindexMin4][6+i]; //Save the sum of all ADC currents for the muxed small CT's (at DMA 6,7)
		dif1 = DMAresults[lastindexMin4][6+i] - averages[6 + (Muxnr*2) + i];
		calcblock[cbi].ADCsquareCurrentsum[3 + (Muxnr*2) + i] += dif1 * dif1;

		//Process the RawPV's for the 2 muxed small CT's
		bool timing_diagnostic_active = power_timing_diagnostic_enabled &&
						abs_i32_to_u32(dif1) >= POWER_TIMING_DIAG_CURRENT_THRESHOLD;
		for (int x = 0; x < 3; x++)
		{
			if (timing_diagnostic_active)
				update_power_timing_diagnostic(dif1, voltage_latest_minus2[x], voltage_latest_minus4[x],
							       voltage_min2_minus4[x]);
			calcblock[cbi].RawPVsum[3 + (Muxnr*2) + i][x] -= dif1 * voltage_differences[x];  //current * volts

		}
	}

	//If we have our 0.5 second of data, switch to the other calcbuffer and continue there.
	DiagSampleBlocks++;
	calcblock[cbi].SampleCounter++;
	if (calcblock[cbi].SampleCounter >= MAIN_SAMPLE_COUNT)
	{
		DiagLastSampleCount = calcblock[cbi].SampleCounter;
		if (packet_ready != false)
			DiagPacketOverruns++;
		cbi++;
		cbi = cbi & 1;
		packet_ready = true;
	}


	//Right now for testing purposes, save DMA results for CT2 to a buffer to analyze.
	//testresults[testindex] = DMAresults[lastindex][2];
	//testindex++;
	//if (testindex >= 0x200)
	//	testindex = 0;


	REG_PORT_OUTCLR = 0x2000000;//set pin 25 low.
}

void Check_and_sendESPpacket()
{
	if (packet_ready == false)
		return;

	uint8_t cbo = cbi +1;
	cbo = cbo & 1; //we check the other (!) calcbuffer to see if we have a finished one waiting for us to process.

	if (calcblock[cbo].SampleCounter >= MAIN_SAMPLE_COUNT)
	{
		packet_ready = false;
		struct SensorReadingType* SensorReading = &SensorReadings[BuildSensorReadingIndex];

		//Track raw ADC DC offsets before RMS and power are calculated.
		for (int i = 0; i < 3; i++)
		{
			//First the MainCT Voltages
			update_adc_offset(i, div_s32_by_u16(calcblock[cbo].ADCVoltagesum[i], MAIN_SAMPLE_COUNT));

			//Now the MainCT Currents
			update_adc_offset(3+i, div_s32_by_u16(calcblock[cbo].ADCCurrentsum[i], MAIN_SAMPLE_COUNT));
		}

		//And the 16 50A CT currents
		for (int i = 0; i < 16; i++)
		{
			update_adc_offset(6+i, div_s32_by_u16(calcblock[cbo].ADCCurrentsum[3+i], MUX_SAMPLE_COUNT)); // Because 12987/8 = 1623
		}
		if (OffsetWarmupWindows < ADC_OFFSET_STARTUP_WINDOWS)
			OffsetWarmupWindows++;

		//Now put it all in the ESP packet buffer
		for (int i = 0; i < 3; i++)
		{
			SensorReading->voltage[i] = scale_rms_main(calcblock[cbo].ADCVoltsquaresum[i]);
			SensorReading->current[i] = scale_rms_main(calcblock[cbo].ADCsquareCurrentsum[i]);
			for (int x = 0; x < 3; x++)
				SensorReading->power[i].phase[x] = scale_power_main(calcblock[cbo].RawPVsum[i][x]);

			if (calcblock[cbo].AmountCtCycles[i] == 0) //make sure we dont divide by 0.
				SensorReading->Cyclecount[i] = 0;
			else
				SensorReading->Cyclecount[i] = calcblock[cbo].CtCycles[i] /  calcblock[cbo].AmountCtCycles[i];


		}

		//Now process the 16 small CT's and put them in the ESP sensor packet. Data ordering in destination according to the MuxTable.
		for (int i = 0; i < 16; i++)
		{
			SensorReading->current[MuxTable[i]] = scale_rms_mux(calcblock[cbo].ADCsquareCurrentsum[i+3]);
			for (int x = 0; x < 3; x++)
				SensorReading->power[MuxTable[i]].phase[x] = scale_power_mux(calcblock[cbo].RawPVsum[i+3][x]);
		}

		//Sensor data packet is ready, so we can now reset the old calcbuffer to 0
		uint8_t* idp = (uint8_t*) &calcblock[cbo];
		for (int x = 0; x < __SIZE_OF_VAR__(calcblock[cbo]); x++)
			*(uint8_t*)(idp+x) = 0;

		//And create the header
		SensorReading->unknown = 0x52;
		SensorReading->sequence_num = ++SensorSequence;


		//Calculate the header's checksum byte
		uint8_t EORvalue = 0x1D;
		uint8_t* NextSensorbyte = (uint8_t*) SensorReading;
			const uint8_t* EORbyte = (const uint8_t *) &EORTable;

		NextSensorbyte = NextSensorbyte + 2; //Skip first 2 bytes
		for (int i = 0; i < ESPpacketlength-2; i++)
		{
			EORvalue = EORvalue ^ *NextSensorbyte;
			EORvalue = *(EORbyte + EORvalue);
			NextSensorbyte++;
		}

		SensorReading->checksum = EORvalue;
		SensorReading->is_unread = 3;
		__asm__ __volatile__("dmb sy");
		uint8_t previous_active = ActiveSensorReadingIndex;
		ActiveSensorReadingIndex = BuildSensorReadingIndex;
		BuildSensorReadingIndex = previous_active;
		DiagPacketsBuilt++;
	}
	else
		packet_ready = false;
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

void COnfigSerCom1 ()
{
	REG_GCLK_CLKCTRL = 0x410F; //01000001 00001111, Clock Enable, GCLK1, GCLK_SERCOM1_CORE
	REG_SERCOM1_I2CS_CTRLB = 0x500; // 0000 0101 00000000, Send ACK. SMEN = 1 = Automatic acknowledge is enabled.Group command is disabled.Smart mode is enabled.
	do {
	} while (REG_SERCOM1_I2CS_SYNCBUSY != 0);

	REG_SERCOM1_I2CS_ADDR = 0xC8; //1100 1000, TENBITEN = 0, so address is: 1100100 = 0x64. GENCEN = 0 (General call address recognition disabled)
	REG_SERCOM1_I2CS_CTRLA = 0x100012;//0001 0000 - 0000 0000 - 0001 0010, slave config(!), 50-100ns hold time, enable,
					   //Standard-mode (Sm) up to 100 kHz and Fast-mode (Fm) up to 400 kHz
	do {
	} while (REG_SERCOM1_I2CS_SYNCBUSY != 0);

	REG_NVIC_PRIO2 = (REG_NVIC_PRIO2 & 0xFF00FFFF) | 0xC00000;
	REG_NVIC_SETENA = 0x400; //Enable interrupt: 0100 00000000 = int 10 = our SerCom IRQ.
	REG_SERCOM1_I2CS_INTENSET = 5;//enables the Data Ready interrupt and the Stop Received interrupt
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
	REG_PORT_DIR = 0x3030000; //00000011 00000011 00000000 00000000, so outputs are pin 16, 17, 24, 25. Rest is input.
	REG_PORT_OUT = 0xC00000; // 00000000 11000000 00000000 00000000, so 22 and 23 driven high, rest driven low.
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
	// PA30/PA31 are SWD. Keep them untouched so development builds stay attachable.
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

	//Let's start with clean blocks
	uint8_t* idp = (uint8_t*) &calcblock;
	for (int x = 0; x < __SIZE_OF_VAR__(calcblock); x++)
		*(uint8_t*)(idp+x) = 0;
	set_boot_stage(BOOT_STAGE_CALCBLOCK_CLEARED);

	idp = (uint8_t*) &SensorReadings;
	for (int x = 0; x < __SIZE_OF_VAR__(SensorReadings); x++)
		*(uint8_t*)(idp+x) = 0;
	set_boot_stage(BOOT_STAGE_SENSOR_CLEARED);

	configure_managed_i2c_info();
	set_boot_stage(BOOT_STAGE_MANAGED_INFO_READY);

	for (int i = 0; i < ADC_OFFSET_CHANNEL_COUNT; i++)
	{
		averages[i] = 0;
		OffsetEstimateQ8[i] = 0;
	}
	OffsetWarmupWindows = 0;
	set_boot_stage(BOOT_STAGE_AVERAGES_CLEARED);

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
		Check_and_sendESPpacket();
	}
	return 0;
}
