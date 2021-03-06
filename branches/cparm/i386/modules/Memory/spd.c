/*
 * spd.c - serial presence detect memory information
 *
 * Originally restored from pcefi10.5
 * Dynamic mem detection original impl. by Rekursor
 * System profiler fix and other fixes by Mozodojo.
 */

#include "libsaio.h"
#include "pci.h"
#include "platform.h"
#include "spd.h"
#include "saio_internal.h"
#include "bootstruct.h"
#include "memvendors.h"

#ifndef DEBUG_SPD
#define DEBUG_SPD 0
#endif

#if DEBUG_SPD
#define DBG(x...)	printf(x)
#include "mem.h"
#else
#define DBG(x...)
#endif

static const char *spd_memory_types[] =
{
	"RAM",          /* 00h  Undefined */
	"FPM",          /* 01h  FPM */
	"EDO",          /* 02h  EDO */
	"",				/* 03h  PIPELINE NIBBLE */
	"SDRAM",        /* 04h  SDRAM */
	"",				/* 05h  MULTIPLEXED ROM */
	"DDR SGRAM",	/* 06h  SGRAM DDR */
	"DDR SDRAM",	/* 07h  SDRAM DDR */
	"DDR2 SDRAM",   /* 08h  SDRAM DDR 2 */
	"",				/* 09h  Undefined */
	"",				/* 0Ah  Undefined */
	"DDR3 SDRAM"   /* 0Bh  SDRAM DDR 3 */
};

#define UNKNOWN_MEM_TYPE 2
static uint8_t spd_mem_to_smbios[] =
{
	UNKNOWN_MEM_TYPE,          /* 00h  Undefined */
	UNKNOWN_MEM_TYPE,          /* 01h  FPM */
	UNKNOWN_MEM_TYPE,          /* 02h  EDO */
	UNKNOWN_MEM_TYPE,	   /* 03h  PIPELINE NIBBLE */
	SMB_MEM_TYPE_SDRAM,        /* 04h  SDRAM */
	SMB_MEM_TYPE_ROM,	   /* 05h  MULTIPLEXED ROM */
	SMB_MEM_TYPE_SGRAM,        /* 06h  SGRAM DDR */
	SMB_MEM_TYPE_DDR,          /* 07h  SDRAM DDR */
	SMB_MEM_TYPE_DDR2,         /* 08h  SDRAM DDR 2 */
	UNKNOWN_MEM_TYPE,  	   /* 09h  Undefined */
	UNKNOWN_MEM_TYPE,	   /* 0Ah  Undefined */
	SMB_MEM_TYPE_DDR3          /* 0Bh  SDRAM DDR 3 */
};
//#define SPD_TO_SMBIOS_SIZE (sizeof(spd_mem_to_smbios)/sizeof(uint8_t))

#define rdtsc(low,high) \
__asm__ __volatile__("rdtsc" : "=a" (low), "=d" (high))

#define SMBHSTSTS 0
#define SMBHSTCNT 2
#define SMBHSTCMD 3
#define SMBHSTADD 4
#define SMBHSTDAT 5
//#define SBMBLKDAT 7

static unsigned char smb_read_byte_intel(uint32_t base, uint8_t adr, uint8_t cmd);
static void init_spd(char * spd, uint32_t base, int slot);
static const char * getVendorName(RamSlotInfo_t* slot, uint32_t base, int slot_num);
static int getDDRspeedMhz(const char * spd);
static const char *getDDRSerial(const char* spd);
static const char * getDDRPartNum(char* spd, uint32_t base, int slot);
static void read_smb_intel(pci_dt_t *smbus_dev);


/** Read one byte from the intel i2c, used for reading SPD on intel chipsets only. */
static unsigned char smb_read_byte_intel(uint32_t base, uint8_t adr, uint8_t cmd)
{
    int l1, h1, l2, h2;
    unsigned long long t;
	
    outb(base + SMBHSTSTS, 0x1f);					// reset SMBus Controller
    outb(base + SMBHSTDAT, 0xff);
	
    rdtsc(l1, h1);
    
    uint64_t tsc = get_env(envTSCFreq);
    
    while ( inb(base + SMBHSTSTS) & 0x01)    // wait until read
    {  
		rdtsc(l2, h2);
		t = ((h2 - h1) * 0xffffffff + (l2 - l1)) / (tsc / 100);
		if (t > 5)
			return 0xFF;                  // break
    }
	
    outb(base + SMBHSTCMD, cmd);
    outb(base + SMBHSTADD, (adr << 1) | 0x01 );
    outb(base + SMBHSTCNT, 0x48 );
	
    rdtsc(l1, h1);
	
 	while (!( inb(base + SMBHSTSTS) & 0x02))		// wait til command finished
	{	
		rdtsc(l2, h2);
		t = ((h2 - h1) * 0xffffffff + (l2 - l1)) / (tsc / 100);
		if (t > 5)
			break;									// break after 5ms
    }
    return inb(base + SMBHSTDAT);
}

/* SPD i2c read optimization: prefetch only what we need, read non prefetcheable bytes on the fly */
#define READ_SPD(spd, base, slot, x) spd[x] = smb_read_byte_intel(base, 0x50 + slot, x)

int spd_indexes[] = {
	SPD_MEMORY_TYPE,
	SPD_DDR3_MEMORY_BANK,
	SPD_DDR3_MEMORY_CODE,
	SPD_NUM_ROWS,
	SPD_NUM_COLUMNS,
	SPD_NUM_DIMM_BANKS,
	SPD_NUM_BANKS_PER_SDRAM,
	4,7,8,9,12,64, /* TODO: give names to these values */
	95,96,97,98, 122,123,124,125 /* UIS */
};
#define SPD_INDEXES_SIZE (sizeof(spd_indexes) / sizeof(int))

/** Read from spd *used* values only*/
static void init_spd(char * spd, uint32_t base, int slot)
{
	int i;
	for (i=0; (unsigned)i< SPD_INDEXES_SIZE; i++) {
		READ_SPD(spd, base, slot, spd_indexes[i]);
	}
}

/** Get Vendor Name from spd, 2 cases handled DDR3 and DDR2, 
 have different formats, always return a valid ptr.*/
static const char * getVendorName(RamSlotInfo_t* slot, uint32_t base, int slot_num)
{
    uint8_t bank = 0;
    uint8_t code = 0;
    int i = 0;
    uint8_t * spd = (uint8_t *) slot->spd;
	
    if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR3) { // DDR3
        bank = (spd[SPD_DDR3_MEMORY_BANK] & 0x07f); // constructors like Patriot use b7=1
        code = spd[SPD_DDR3_MEMORY_CODE];
        for (i=0; (unsigned)i < VEN_MAP_SIZE; i++)
            if (bank==vendorMap[i].bank && code==vendorMap[i].code)
                return vendorMap[i].name;
    }
    else if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR2) {
        if(spd[64]==0x7f) {
            for (i=64; i<72 && spd[i]==0x7f;i++) {
				bank++;
				READ_SPD(spd, base, slot_num,i+1); // prefetch next spd byte to read for next loop
			}
			READ_SPD(spd, base, slot_num,i);
            code = spd[i];
        } else {
            code = spd[64]; 
            bank = 0;
        }
        for (i=0; (unsigned)i < VEN_MAP_SIZE; i++)
            if (bank==vendorMap[i].bank && code==vendorMap[i].code)
                return vendorMap[i].name;
    }
    /* OK there is no vendor id here lets try to match the partnum if it exists */
    if (strstr(slot->PartNo,"GU332") == slot->PartNo) // Unifosa fingerprint
        return "Unifosa";
    return "NoName";
}

/** Get Default Memory Module Speed (no overclocking handled) */
static int getDDRspeedMhz(const char * spd)
{
    if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR3) { 
        switch(spd[12])  {
			case 0x0f:
				return 1066;
			case 0x0c:
				return 1333;
			case 0x0a:
				return 1600;
			case 0x14:
			default:
				return 800;
        }
    } 
    else if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR2)  {
        switch(spd[9]) {
			case 0x50:
				return 400;
			case 0x3d:
				return 533;
			case 0x30:
				return 667;
			case 0x25:
			default:
				return 800;
        }
    }
    return  800; // default freq for unknown types
}

#define SMST(a) ((uint8_t)((spd[a] & 0xf0) >> 4))
#define SLST(a) ((uint8_t)(spd[a] & 0x0f))

/** Get DDR3 or DDR2 serial number, 0 most of the times, always return a valid ptr */
//char asciiSerial[16];
static const char *getDDRSerial(const char* spd)
{
    static char asciiSerial[16];
	
    if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR3) // DDR3
    {
		snprintf(asciiSerial, sizeof(asciiSerial),"%X%X%X%X%X%X%X%X", SMST(122) /*& 0x7*/, SLST(122), SMST(123), SLST(123), SMST(124), SLST(124), SMST(125), SLST(125));
    }
    else if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR2) // DDR2 or DDR
    { 
		snprintf(asciiSerial, sizeof(asciiSerial),"%X%X%X%X%X%X%X%X", SMST(95) /*& 0x7*/, SLST(95), SMST(96), SLST(96), SMST(97), SLST(97), SMST(98), SLST(98));
    }
	
    return strdup(asciiSerial);
}

/** Get DDR3 or DDR2 Part Number, always return a valid ptr */
//char asciiPartNo[32];
static const char * getDDRPartNum(char* spd, uint32_t base, int slot)
{
	static char asciiPartNo[32];
	int i, start=0, index = 0;
	
    if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR3) {
		start = 128;
	}
    else if (spd[SPD_MEMORY_TYPE]==SPD_MEMORY_TYPE_SDRAM_DDR2) {
		start = 73;
	}
	
    // Check that the spd part name is zero terminated and that it is ascii:
    bzero(asciiPartNo, sizeof(asciiPartNo));
	char c;
	for (i=start; (unsigned)i < start + sizeof(asciiPartNo); i++) {
		READ_SPD(spd, base, slot, i); // only read once the corresponding model part (ddr3 or ddr2)
		c = spd[i];
		if (isalpha(c) || isdigit(c) || ispunct(c)) // It seems that System Profiler likes only letters and digits...
			asciiPartNo[index++] = c;
		else if (!isascii(c))
			break;
	}
	
	return strdup(asciiPartNo);
}

int mapping []= {0,2,1,3,4,6,5,7,8,10,9,11};


/** Read from smbus the SPD content and interpret it for detecting memory attributes */
static void read_smb_intel(pci_dt_t *smbus_dev)
{ 
    int        i, speed;
    uint8_t    spd_size, spd_type;
    uint32_t   base;
#if DEBUG_SPD
    uint32_t    mmio, hostc;
#endif
    bool       dump = false;
    RamSlotInfo_t*  slot;
	
	uint16_t cmd = pci_config_read16(smbus_dev->dev.addr, 0x04);
	DBG("SMBus CmdReg: 0x%x\n", cmd);
	pci_config_write16(smbus_dev->dev.addr, 0x04, cmd | 1);
	
    base = pci_config_read16(smbus_dev->dev.addr, 0x20) & 0xFFFE;
#if DEBUG_SPD
    mmio = pci_config_read32(smbus_dev->dev.addr, 0x10);// & ~0x0f;
	hostc = pci_config_read8(smbus_dev->dev.addr, 0x40);
    DBG("Scanning SMBus [%04x:%04x], mmio: 0x%x, ioport: 0x%x, hostc: 0x%x\n", 
		smbus_dev->vendor_id, smbus_dev->device_id, mmio, base, hostc);
#endif
	
    getBoolForKey("DumpSPD", &dump, DEFAULT_BOOT_CONFIG);
    bool fullBanks ;  // needed at least for laptops
    
    int DMIMaxMemorySlots = (int)get_env(envDMIMaxMemorySlots);
    int DMIMemModules = (int)get_env(envDMIMemModules);
	
    fullBanks = (bool)(DMIMemModules == DMIMaxMemorySlots) ;
    
    // Search MAX_RAM_SLOTS slots
	char spdbuf[MAX_SPD_SIZE];
	
    RamSlotInfo_t *RamDIMM = get_env_ptr(envRamDimm);
	
    static int	DmiDIMM[MAX_RAM_SLOTS];	// Information and SPD mapping for each slot
	
    uint64_t		RamFrequency = get_env(envRamFrequency);
    
    for (i = 0; i <  MAX_RAM_SLOTS; i++){
		DBG("Scanning slot %d\n", i);
        slot = &RamDIMM[i];
        spd_size = smb_read_byte_intel(base, 0x50 + i, 0);
        // Check spd is present
        if (spd_size && (spd_size != 0xff) ) {
			slot->spd = spdbuf;
            slot->InUse = true;
			
            bzero(slot->spd, spd_size);
            
            // Copy spd data into buffer
            
			//for (x = 0; x < spd_size; x++) slot->spd[x] = smb_read_byte_intel(base, 0x50 + i, x);
            init_spd(slot->spd, base, i);
			
            switch (slot->spd[SPD_MEMORY_TYPE])  {
				case SPD_MEMORY_TYPE_SDRAM_DDR2:
					
					/*slot->ModuleSize = ((1 << (slot->spd[SPD_NUM_ROWS] & 0x0f) + (slot->spd[SPD_NUM_COLUMNS] & 0x0f) - 17) * 
					 ((slot->spd[SPD_NUM_DIMM_BANKS] & 0x7) + 1) * slot->spd[SPD_NUM_BANKS_PER_SDRAM]);*/
					
					slot->ModuleSize = ((1 << ((slot->spd[SPD_NUM_ROWS] & 0x0f) + (slot->spd[SPD_NUM_COLUMNS] & 0x0f) - 17)) * 
										((slot->spd[SPD_NUM_DIMM_BANKS] & 0x7) + 1) * slot->spd[SPD_NUM_BANKS_PER_SDRAM]);
					break;
					
				case SPD_MEMORY_TYPE_SDRAM_DDR3:
					
					slot->ModuleSize = ((slot->spd[4] & 0x0f) + 28 ) + ((slot->spd[8] & 0x7)  + 3 );
					slot->ModuleSize -= (slot->spd[7] & 0x7) + 25;
					slot->ModuleSize = ((1 << slot->ModuleSize) * (((slot->spd[7] >> 3) & 0x1f) + 1));
					
					break;
            }
            
            spd_type = (slot->spd[SPD_MEMORY_TYPE] < ((char) 12) ? slot->spd[SPD_MEMORY_TYPE] : 0);
            slot->Type = spd_mem_to_smbios[spd_type];
            slot->PartNo = getDDRPartNum(slot->spd, base, i);
            slot->Vendor = getVendorName(slot, base, i);
            slot->SerialNo = getDDRSerial(slot->spd);
			
            // determine spd speed
            speed = getDDRspeedMhz(slot->spd);
            if (slot->Frequency<(uint32_t)speed) slot->Frequency = speed;
			
			// pci memory controller if available, is more reliable
			if ( RamFrequency > 0) {
				uint32_t freq = (uint32_t)(RamFrequency / 500000);
				// now round off special cases
				uint32_t fmod100 = freq %100;
				switch(fmod100) {
					case  1:	freq--;	break;
					case 32:	freq++;	break;
					case 65:	freq++; break;
					case 98:	freq+=2;break;
					case 99:	freq++; break;
				}
				slot->Frequency = freq;
			}
			
			verbose("Slot: %d Type %d %dMB (%s) %dMHz Vendor=%s\n      PartNo=%s SerialNo=%s\n", 
					i, 
					(int)slot->Type,
					slot->ModuleSize, 
					spd_memory_types[spd_type],
					slot->Frequency,
					slot->Vendor,
					slot->PartNo,
					slot->SerialNo); 
#if DEBUG_SPD			
			dumpPhysAddr("spd content: ",slot->spd, spd_size);
			getc();
#endif           
        }
        // laptops sometimes show slot 0 and 2 with slot 1 empty when only 2 slots are presents so:
        DmiDIMM[i]= 
		i>0 && RamDIMM[1].InUse==false && fullBanks && (DMIMaxMemorySlots==2) ? 
		mapping[i] : i; // for laptops case, mapping setup would need to be more generic than this
        
		slot->spd = NULL;
		
    } // for
    
    safe_set_env_ptr(envDmiDimm, DmiDIMM, sizeof(DmiDIMM));
	
}

static struct smbus_controllers_t smbus_controllers[] = {
	
	{0x8086, 0x269B, "ESB2",read_smb_intel },
	{0x8086, 0x25A4, "6300ESB",read_smb_intel },
	{0x8086, 0x24C3, "ICH4",read_smb_intel },
	{0x8086, 0x24D3, "ICH5",read_smb_intel },
	{0x8086, 0x266A, "ICH6",read_smb_intel },
	{0x8086, 0x27DA, "ICH7",read_smb_intel },
	{0x8086, 0x283E, "ICH8",read_smb_intel },
	{0x8086, 0x2930, "ICH9",read_smb_intel },
	{0x8086, 0x3A30, "ICH10R",read_smb_intel },
	{0x8086, 0x3A60, "ICH10B",read_smb_intel },
	{0x8086, 0x3B30, "5 Series",read_smb_intel },
	{0x8086, 0x1C22, "6 Series",read_smb_intel },
	{0x8086, 0x1E22, "7 Series",read_smb_intel },
	{0x8086, 0x5032, "EP80579",read_smb_intel },
	{0x8086, 0x1D22, "X79 Series",read_smb_intel },
	
};

bool is_smbus_controller(pci_dt_t* pci_dt)
{
	int i = 0;
	for ( ; (unsigned)i <  sizeof(smbus_controllers) / sizeof(smbus_controllers[0]); i++ )
	{
		if (pci_dt->vendor_id == smbus_controllers[i].vendor &&
			pci_dt->device_id == smbus_controllers[i].device)
		{
			return true;
		}        
	}
	return false;
}	


void scan_spd(pci_dt_t* smbus_controller_dev)
{
	int i = 0;
	for ( ; (unsigned)i <  sizeof(smbus_controllers) / sizeof(smbus_controllers[0]); i++ )
	{
		if (smbus_controller_dev->vendor_id == smbus_controllers[i].vendor &&
			smbus_controller_dev->device_id == smbus_controllers[i].device)
		{
			smbus_controllers[i].read_smb(smbus_controller_dev); // read smb
		}        
	}
}
