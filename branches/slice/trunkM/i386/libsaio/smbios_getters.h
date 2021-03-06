
#include "libsaio.h"
#include "smbios.h"
#include "platform.h"
#include "pci.h"

#ifndef __LIBSAIO_SMBIOS_GETTERS_H
#define __LIBSAIO_SMBIOS_GETTERS_H

#define SMBIOS_RANGE_START      0x000F0000
#define SMBIOS_RANGE_END        0x000FFFFF

#define NOT_AVAILABLE			"N/A"

typedef enum {
	kSMBString,
	kSMBByte,
	kSMBWord,
	kSMBDWord
//	kSMBQWord
} SMBValueType;

typedef union {
	const char	*string;
	uint8_t		byte;
	uint16_t	word;
	uint32_t	dword;
//	uint64_t	qword;
} returnType;

extern bool getProcessorInformationExternalClock(returnType *value);
extern bool getProcessorInformationMaximumClock(returnType *value);
extern bool getSMBOemProcessorBusSpeed(returnType *value);
extern bool getSMBOemProcessorType(returnType *value);
extern bool getSMBMemoryDeviceMemoryType(returnType *value);
extern bool getSMBMemoryDeviceMemorySpeed(returnType *value);
extern bool getSMBMemoryDeviceManufacturer(returnType *value);
extern bool getSMBMemoryDeviceSerialNumber(returnType *value);
extern bool getSMBMemoryDevicePartNumber(returnType *value);
//extern bool scanDMI(void);
extern SMBStructHeader* FindFirstDmiTableOfType(int type, int minlength);
extern SMBStructHeader* FindNextDmiTableOfType(int type, int minlength);
extern void getSmbiosProductName();
extern void getSmbiosMacModel(void);
extern void getSmbiosTableStructure(struct SMBEntryPoint *smbios);

SMBEntryPoint *getAddressOfSmbiosTable(void);
const char * smbiosStringAtIndex(SMBStructHeader* smHeader, int index, int* length);

#endif /* !__LIBSAIO_SMBIOS_GETTERS_H */
