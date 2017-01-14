/*
 *	HDA injector / Audio Enabler
 *
 *	Copyright (C) 2012	Chameleon Team
 *	Edit by Fabio (ErmaC)
 *	HDA bus scans and codecs enumeration by Zenith432
 *
 *	HDA injector is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	HDA injector is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	Alternatively you can choose to comply with APSL
 *
 *	Permission is hereby granted, free of charge, to any person obtaining a
 *	copy of this software and associated documentation files (the "Software"),
 *	to deal in the Software without restriction, including without limitation
 *	the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *	and/or sell copies of the Software, and to permit persons to whom the
 *	Software is furnished to do so, subject to the following conditions:
 *
 *	The above copyright notice and this permission notice shall be included in
 *	all copies or substantial portions of the Software.
 *
 ******************************************************************************
 * http://www.leidinger.net/FreeBSD/dox/dev_sound/html/df/d54/hdac_8c_source.html
 *
 * Copyright (c) 2006 Stephane E. Potvin <sepotvin@videotron.ca>
 * Copyright (c) 2006 Ariff Abdullah <ariff@FreeBSD.org>
 * Copyright (c) 2008-2012 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Intel High Definition Audio (Controller) driver for FreeBSD.
 *
 ******************************************************************************/

#include "boot.h"
#include "bootstruct.h"
#include "cpu.h"
#include "pci.h"
#include "pci_root.h"
#include "platform.h"
#include "device_inject.h"
#include "convert.h"
#include "hda.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define HEADER      __FILE__ " [" TOSTRING(__LINE__) "]: "

#ifndef DEBUG_HDA
	#define DEBUG_HDA 0
#endif

#if DEBUG_HDA
	#define DBG(x...)  verbose(x)
#else
	#define DBG(x...)
#endif

#ifndef DEBUG_CODEC
	#define DEBUG_CODEC 0
#endif

#if DEBUG_CODEC
	#define CDBG(x...)  verbose(x)
#else
	#define CDBG(x...)
#endif

#define UNKNOWN "Unknown "

#define hdacc_lock(codec)       snd_mtxlock((codec)->lock)
#define hdacc_unlock(codec)     snd_mtxunlock((codec)->lock)
#define hdacc_lockassert(codec) snd_mtxassert((codec)->lock)
#define hdacc_lockowned(codec)  mtx_owned((codec)->lock)

const char *hda_slot_name[]		=	{ "AAPL,slot-name", "Built In" };

uint8_t default_HDEF_layout_id[]		=	{0x01, 0x00, 0x00, 0x00};
#define HDEF_LEN ( sizeof(default_HDEF_layout_id) / sizeof(uint8_t) )
uint8_t default_HDAU_layout_id[]		=	{0x01, 0x00, 0x00, 0x00};
#define HDAU_LEN ( sizeof(default_HDAU_layout_id) / sizeof(uint8_t) )
static uint8_t connector_type_value[]          =	{0x00, 0x08, 0x00, 0x00};

/* Structures */

static hda_controller_devices know_hda_controller[] = {
	//8086  Intel Corporation
	{ HDA_INTEL_OAK,	"Oaktrail"		/*, 0, 0 */ },
	{ HDA_INTEL_BAY,	"BayTrail"		/*, 0, 0 */ },
	{ HDA_INTEL_HSW1,	"Haswell"		/*, 0, 0 */ },
	{ HDA_INTEL_HSW2,	"Haswell"		/*, 0, 0 */ },
	{ HDA_INTEL_HSW3,	"Haswell"		/*, 0, 0 */ },
	{ HDA_INTEL_BDW,	"Broadwell"		/*, 0, 0 */ },
	{ HDA_INTEL_CPT,	"Cougar Point"		/*, 0, 0 */ },
	{ HDA_INTEL_PATSBURG,	"Patsburg"		/*, 0, 0 */ },
	{ HDA_INTEL_PPT1,	"Panther Point"		/*, 0, 0 */ },
	{ HDA_INTEL_BRASWELL,	"Braswell"		/*, 0, 0 */ },
	{ HDA_INTEL_82801F,	"82801F"		/*, 0, 0 */ },
	{ HDA_INTEL_63XXESB,	"631x/632xESB"		/*, 0, 0 */ },
	{ HDA_INTEL_82801G,	"82801G"		/*, 0, 0 */ },
	{ HDA_INTEL_82801H,	"82801H"		/*, 0, 0 */ },
	{ HDA_INTEL_82801I,	"82801I"		/*, 0, 0 */ },
	{ HDA_INTEL_ICH9,	"ICH9"			/*, 0, 0 */ },
	{ HDA_INTEL_82801JI,	"82801JI"		/*, 0, 0 */ },
	{ HDA_INTEL_82801JD,	"82801JD"		/*, 0, 0 */ },
	{ HDA_INTEL_PCH,	"5 Series/3400 Series"	/*, 0, 0 */ },
	{ HDA_INTEL_PCH2,	"5 Series/3400 Series"	/*, 0, 0 */ },
	{ HDA_INTEL_SCH,	"SCH"			/*, 0, 0 */ },
	{ HDA_INTEL_LPT1,	"Lynx Point"		/*, 0, 0 */ },
	{ HDA_INTEL_LPT2,	"Lynx Point"		/*, 0, 0 */ },
	{ HDA_INTEL_WCPT,	"Wildcat Point"		/*, 0, 0 */ },
	{ HDA_INTEL_WELLS1,	"Wellsburg"		/*, 0, 0 */ },
	{ HDA_INTEL_WELLS2,	"Wellsburg"		/*, 0, 0 */ },
	{ HDA_INTEL_WCPTLP,	"Wildcat Point-LP"	/*, 0, 0 */ },
	{ HDA_INTEL_LPTLP1,	"Lynx Point-LP"		/*, 0, 0 */ },
	{ HDA_INTEL_LPTLP2,	"Lynx Point-LP"		/*, 0, 0 */ },
	{ HDA_INTEL_SRSPLP,	"Sunrise Point-LP"	/*, 0, 0 */ },
	{ HDA_INTEL_SRSP,	"Sunrise Point"		/*, 0, 0 */ },

	//10de  NVIDIA Corporation
	{ HDA_NVIDIA_MCP51,	"MCP51" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_MCP55,	"MCP55" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_MCP61_1,	"MCP61" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP61_2,	"MCP61" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP65_1,	"MCP65" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP65_2,	"MCP65" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP67_1,	"MCP67" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP67_2,	"MCP67" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP73_1,	"MCP73" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP73_2,	"MCP73" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP78_1,	"MCP78" /*, 0, HDAC_QUIRK_64BIT */ },
	{ HDA_NVIDIA_MCP78_2,	"MCP78" /*, 0, HDAC_QUIRK_64BIT */ },
	{ HDA_NVIDIA_MCP78_3,	"MCP78" /*, 0, HDAC_QUIRK_64BIT */ },
	{ HDA_NVIDIA_MCP78_4,	"MCP78" /*, 0, HDAC_QUIRK_64BIT */ },
	{ HDA_NVIDIA_MCP79_1,	"MCP79" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP79_2,	"MCP79" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP79_3,	"MCP79" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP79_4,	"MCP79" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP89_1,	"MCP89" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP89_2,	"MCP89" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP89_3,	"MCP89" /*, 0, 0 */ },
	{ HDA_NVIDIA_MCP89_4,	"MCP89" /*, 0, 0 */ },
	{ HDA_NVIDIA_0BE2,	"(0x0be2)" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_0BE3,	"(0x0be3)" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_0BE4,	"(0x0be4)" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GT100,	"GT100" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GT104,	"GT104" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GT106,	"GT106" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GT108,	"GT108" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GT116,	"GT116" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GF119,	"GF119" /*, 0, 0 */ },
	{ HDA_NVIDIA_GF110_1,	"GF110" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GF110_2,	"GF110" /*, 0, HDAC_QUIRK_MSI */ },
	{ HDA_NVIDIA_GK110,	"GK110" /*, 0, ? */ },
	{ HDA_NVIDIA_GK106,	"GK106" /*, 0, ? */ },
	{ HDA_NVIDIA_GK107,	"GK107" /*, 0, ? */ },
	{ HDA_NVIDIA_GK104,	"GK104" /*, 0, ? */ },
	{ HDA_NVIDIA_GP104_2,	"Pascal GP104-200" /*, 0, ? */ },
	{ HDA_NVIDIA_GM204_2,	"Maxwell GP204-200" /*, 0, ? */ },

	//1002  Advanced Micro Devices [AMD] nee ATI Technologies Inc
	{ HDA_ATI_SB450,	"SB4x0" /*, 0, 0 */ },
	{ HDA_ATI_SB600,	"SB600" /*, 0, 0 */ },
	{ HDA_ATI_RS600,	"RS600" /*, 0, 0 */ },
	{ HDA_ATI_HUDSON,	"Hudson" /*, 0, 0 */ },
	{ HDA_ATI_RS690,	"RS690" /*, 0, 0 */ },
	{ HDA_ATI_RS780,	"RS780" /*, 0, 0 */ },
	{ HDA_ATI_RS880,	"RS880" /*, 0, 0 */ },
	{ HDA_ATI_TRINITY,	"Trinity" /*, 0, ? */ },
	{ HDA_ATI_R600,		"R600" /*, 0, 0 */ },
	{ HDA_ATI_RV610,	"RV610" /*, 0, 0 */ },
	{ HDA_ATI_RV620,	"RV620" /*, 0, 0 */ },
	{ HDA_ATI_RV630,	"RV630" /*, 0, 0 */ },
	{ HDA_ATI_RV635,	"RV635" /*, 0, 0 */ },
	{ HDA_ATI_RV710,	"RV710" /*, 0, 0 */ },
	{ HDA_ATI_RV730,	"RV730" /*, 0, 0 */ },
	{ HDA_ATI_RV740,	"RV740" /*, 0, 0 */ },
	{ HDA_ATI_RV770,	"RV770" /*, 0, 0 */ },
	{ HDA_ATI_RV810,	"RV810" /*, 0, 0 */ },
	{ HDA_ATI_RV830,	"RV830" /*, 0, 0 */ },
	{ HDA_ATI_RV840,	"RV840" /*, 0, 0 */ },
	{ HDA_ATI_RV870,	"RV870" /*, 0, 0 */ },
	{ HDA_ATI_RV910,	"RV910" /*, 0, 0 */ },
	{ HDA_ATI_RV930,	"RV930" /*, 0, 0 */ },
	{ HDA_ATI_RV940,	"RV940" /*, 0, 0 */ },
	{ HDA_ATI_RV970,	"RV970" /*, 0, 0 */ },
	{ HDA_ATI_R1000,	"R1000" /*, 0, 0 */ }, // HDMi
	{ HDA_ATI_SI,		"SI" /*, 0, 0 */ },
	{ HDA_ATI_VERDE,	"Cape Verde" /*, 0, ? */ }, // HDMi

	//17f3  RDC Semiconductor, Inc.
	{ HDA_RDC_M3010,	"M3010" /*, 0, 0 */ },

	//1106  VIA Technologies, Inc.
	{ HDA_VIA_VT82XX,	"VT8251/8237A" /*, 0, 0 */ },

	//1039  Silicon Integrated Systems [SiS]
	{ HDA_SIS_966,		"966" /*, 0, 0 */ },

	//10b9  ULi Electronics Inc.(Split off ALi Corporation in 2003)
	{ HDA_ULI_M5461,	"M5461" /*, 0, 0 */ },

	/* Unknown */
	{ HDA_INTEL_ALL,	"Unknown Intel device" /*, 0, 0 */ },
	{ HDA_NVIDIA_ALL,	"Unknown NVIDIA device" /*, 0, 0 */ },
	{ HDA_ATI_ALL,		"Unknown ATI device" /*, 0, 0 */ },
	{ HDA_VIA_ALL,		"Unknown VIA device" /*, 0, 0 */ },
	{ HDA_SIS_ALL,		"Unknown SiS device" /*, 0, 0 */ },
	{ HDA_ULI_ALL,		"Unknown ULI device" /*, 0, 0 */ },
};
#define HDAC_DEVICES_LEN (sizeof(know_hda_controller) / sizeof(know_hda_controller[0]))

/* CODECs */
/*
 * ErmaC: There's definitely a lot of different versions of the same audio codec variant out there...
 * in the next struct you will find a "generic" but IMHO detailed list of
 * possible codec... anyway to specific a new one or find difference beetween revision
 * check it under linux enviroment with:
 * $cat /proc/asound/Intel/codec#0
 * --------------------------------
 *  Codec: Analog Devices AD1989B
 *  Address: 0
 *  AFG Function Id: 0x1 (unsol 0)
 *  Vendor Id: 0x11d4989b
 *  Subsystem Id: 0x10438372
 *  Revision Id: 0x100300
 * --------------------------------
 * or
 * $cat /proc/asound/NVidia/codec#0
 * --------------------------------
 *  Codec: Nvidia GPU 14 HDMI/DP
 *  Address: 0
 *  AFG Function Id: 0x1 (unsol 0)
 *  Vendor Id: 0x10de0014
 *  Subsystem Id: 0x10de0101
 *  Revision Id: 0x100100
 * --------------------------------
 */

static hdacc_codecs know_codecs[] = {
	{ HDA_CODEC_CS4206, 0,		"CS4206" },
	{ HDA_CODEC_CS4207, 0,		"CS4207" },
	{ HDA_CODEC_CS4208, 0,		"CS4208" },
	{ HDA_CODEC_CS4210, 0,		"CS4210" },
	{ HDA_CODEC_CS4213, 0,          "CS4213" },

	{ HDA_CODEC_ALC221, 0,          "ALC221" },
	{ HDA_CODEC_ALC231, 0,          "ALC231" },
	{ HDA_CODEC_ALC233, 0,          "ALC233" },
	{ HDA_CODEC_ALC233, 0x0003,	"ALC3236" },
	{ HDA_CODEC_ALC235, 0,          "ALC235" },
	{ HDA_CODEC_ALC255, 0,          "ALC255" },
	{ HDA_CODEC_ALC256, 0,          "ALC256" },
	{ HDA_CODEC_ALC260, 0,          "ALC260" },
//	{ HDA_CODEC_ALC262, 0x0100,	"ALC262" }, // Revision Id: 0x100100
	{ HDA_CODEC_ALC262, 0,          "ALC262" },
	{ HDA_CODEC_ALC267, 0,          "ALC267" },
	{ HDA_CODEC_ALC268, 0,          "ALC268" },
	{ HDA_CODEC_ALC269, 0,          "ALC269" },
	{ HDA_CODEC_ALC270, 0,          "ALC270" },
	{ HDA_CODEC_ALC272, 0,          "ALC272" },
	{ HDA_CODEC_ALC273, 0,          "ALC273" },
	{ HDA_CODEC_ALC275, 0,          "ALC275" },
	{ HDA_CODEC_ALC276, 0,          "ALC276" },
	{ HDA_CODEC_ALC280, 0,          "ALC280" },
	{ HDA_CODEC_ALC282, 0,          "ALC282" },
	{ HDA_CODEC_ALC283, 0,          "ALC283" },
	{ HDA_CODEC_ALC284, 0,          "ALC284" },
	{ HDA_CODEC_ALC285, 0,          "ALC285" },
	{ HDA_CODEC_ALC286, 0,          "ALC286" },
	{ HDA_CODEC_ALC288, 0,          "ALC288" },
	{ HDA_CODEC_ALC290, 0,          "ALC290" },
	{ HDA_CODEC_ALC292, 0,          "ALC292" },
	{ HDA_CODEC_ALC292, 0x0001,     "ALC3232" },
	{ HDA_CODEC_ALC293, 0,          "ALC293" },
	{ HDA_CODEC_ALC298, 0,          "ALC298" },
	{ HDA_CODEC_ALC660, 0,          "ALC660-VD" },
	{ HDA_CODEC_ALC662, 0,          "ALC662" },
	{ HDA_CODEC_ALC662, 0x0101,	"ALC662 rev1" },
	{ HDA_CODEC_ALC662, 0x0002,	"ALC662 rev2" },
	{ HDA_CODEC_ALC662, 0x0300,	"ALC662 rev3" },
	{ HDA_CODEC_ALC663, 0,          "ALC663" },
	{ HDA_CODEC_ALC665, 0,          "ALC665" },
	{ HDA_CODEC_ALC667, 0,          "ALC667" },
	{ HDA_CODEC_ALC668, 0,          "ALC668" },
	{ HDA_CODEC_ALC670, 0,          "ALC670" },
	{ HDA_CODEC_ALC671, 0,          "ALC671" },
	{ HDA_CODEC_ALC680, 0,          "ALC680" },
	{ HDA_CODEC_ALC861, 0x0340,	"ALC660" },
	{ HDA_CODEC_ALC861, 0,          "ALC861" },
	{ HDA_CODEC_ALC861VD, 0,        "ALC861-VD" },
	{ HDA_CODEC_ALC867, 0,          "ALC891" },
//	{ HDA_CODEC_ALC880, 0x0800,	"ALC880" }, // Revision Id: 0x100800
	{ HDA_CODEC_ALC880, 0,          "ALC880" },
	{ HDA_CODEC_ALC882, 0,          "ALC882" },
	{ HDA_CODEC_ALC883, 0,          "ALC883" },
	{ HDA_CODEC_ALC885, 0x0101,	"ALC889A" }, // Revision Id: 0x100101
	{ HDA_CODEC_ALC885, 0x0103,	"ALC889A" }, // Revision Id: 0x100103
	{ HDA_CODEC_ALC885, 0,          "ALC885" },
	{ HDA_CODEC_ALC886, 0,          "ALC886" },
	{ HDA_CODEC_ALC887, 0,          "ALC887" },
	{ HDA_CODEC_ALC888, 0x0101,	"ALC1200" }, // Revision Id: 0x100101
	{ HDA_CODEC_ALC888, 0,          "ALC888" },
	{ HDA_CODEC_ALC889, 0,          "ALC889" },
	{ HDA_CODEC_ALC892, 0,          "ALC892" },
	{ HDA_CODEC_ALC898, 0,          "ALC898" },
//	{ HDA_CODEC_ALC899, 0,		"ALC899" },
	{ HDA_CODEC_ALC900, 0,          "ALC1150" },

	{ HDA_CODEC_AD1882, 0,          "AD1882" },
	{ HDA_CODEC_AD1882A, 0,         "AD1882A" },
	{ HDA_CODEC_AD1883, 0,          "AD1883" },
	{ HDA_CODEC_AD1884, 0,          "AD1884" },
	{ HDA_CODEC_AD1884A, 0,         "AD1884A" },
	{ HDA_CODEC_AD1981HD, 0,        "AD1981HD" },
	{ HDA_CODEC_AD1983, 0,          "AD1983" },
	{ HDA_CODEC_AD1984, 0,          "AD1984" },
	{ HDA_CODEC_AD1984A, 0,         "AD1984A" },
	{ HDA_CODEC_AD1984B, 0,         "AD1984B" },
	{ HDA_CODEC_AD1986A, 0,         "AD1986A" },
	{ HDA_CODEC_AD1987, 0,          "AD1987" },
	{ HDA_CODEC_AD1988, 0,          "AD1988A" },
	{ HDA_CODEC_AD1988B, 0,         "AD1988B" },
	{ HDA_CODEC_AD1989A, 0,         "AD1989A" },
	{ HDA_CODEC_AD1989B, 0x0200,	"AD2000B" }, // Revision Id: 0x100200
	{ HDA_CODEC_AD1989B, 0x0300,	"AD2000B" }, // Revision Id: 0x100300
	{ HDA_CODEC_AD1989B, 0,         "AD1989B" },

	{ HDA_CODEC_XFIEA, 0,           "Creative X-Fi Extreme A" },
	{ HDA_CODEC_XFIED, 0,           "Creative X-Fi Extreme D" },
	{ HDA_CODEC_CA0132, 0,          "Creative CA0132" },
	{ HDA_CODEC_SB0880, 0,          "Creative SB0880 X-Fi" },
	{ HDA_CODEC_CMI9880, 0,         "CMedia CMI9880" },
	{ HDA_CODEC_CMI98802, 0,        "CMedia CMI9880" },

	{ HDA_CODEC_CXD9872RDK, 0,      "CXD9872RD/K" },
	{ HDA_CODEC_CXD9872AKD, 0,      "CXD9872AKD" },
	{ HDA_CODEC_STAC9200D, 0,       "STAC9200D" },
	{ HDA_CODEC_STAC9204X, 0,       "STAC9204X" },
	{ HDA_CODEC_STAC9204D, 0,       "STAC9204D" },
	{ HDA_CODEC_STAC9205X, 0,       "STAC9205X" },
	{ HDA_CODEC_STAC9205D, 0,       "STAC9205D" },
	{ HDA_CODEC_STAC9220, 0,        "STAC9220" },
	{ HDA_CODEC_STAC9220_A1, 0,     "STAC9220_A1" },
	{ HDA_CODEC_STAC9220_A2, 0,     "STAC9220_A2" },
	{ HDA_CODEC_STAC9221, 0,        "STAC9221" },
	{ HDA_CODEC_STAC9221_A2, 0,     "STAC9221_A2" },
	{ HDA_CODEC_STAC9221D, 0,       "STAC9221D" },
	{ HDA_CODEC_STAC922XD, 0,       "STAC9220D/9223D" },
	{ HDA_CODEC_STAC9227X, 0,       "STAC9227X" },
	{ HDA_CODEC_STAC9227D, 0,       "STAC9227D" },
	{ HDA_CODEC_STAC9228X, 0,       "STAC9228X" },
	{ HDA_CODEC_STAC9228D, 0,       "STAC9228D" },
	{ HDA_CODEC_STAC9229X, 0,       "STAC9229X" },
	{ HDA_CODEC_STAC9229D, 0,       "STAC9229D" },
	{ HDA_CODEC_STAC9230X, 0,       "STAC9230X" },
	{ HDA_CODEC_STAC9230D, 0,       "STAC9230D" },
	{ HDA_CODEC_STAC9250, 0,        "STAC9250" },
	{ HDA_CODEC_STAC9250D, 0,	"STAC9250D" },
	{ HDA_CODEC_STAC9251, 0,        "STAC9251" },
	{ HDA_CODEC_STAC9250D_1, 0,	"STAC9250D" },
	{ HDA_CODEC_STAC9255, 0,        "STAC9255" },
	{ HDA_CODEC_STAC9255D, 0,       "STAC9255D" },
	{ HDA_CODEC_STAC9254, 0,        "STAC9254" },
	{ HDA_CODEC_STAC9254D, 0,       "STAC9254D" },
	{ HDA_CODEC_STAC9271X, 0,       "STAC9271X" },
	{ HDA_CODEC_STAC9271D, 0,       "STAC9271D" },
	{ HDA_CODEC_STAC9272X, 0,       "STAC9272X" },
	{ HDA_CODEC_STAC9272D, 0,       "STAC9272D" },
	{ HDA_CODEC_STAC9273X, 0,       "STAC9273X" },
	{ HDA_CODEC_STAC9273D, 0,       "STAC9273D" },
	{ HDA_CODEC_STAC9274, 0,        "STAC9274" },
	{ HDA_CODEC_STAC9274D, 0,       "STAC9274D" },
	{ HDA_CODEC_STAC9274X5NH, 0,    "STAC9274X5NH" },
	{ HDA_CODEC_STAC9274D5NH, 0,    "STAC9274D5NH" },
	{ HDA_CODEC_STAC9202, 0,	"STAC9202" },
	{ HDA_CODEC_STAC9202D, 0,	"STAC9202D" },
	{ HDA_CODEC_STAC9872AK, 0,      "STAC9872AK" },

	{ HDA_CODEC_IDT92HD005, 0,      "92HD005" },
	{ HDA_CODEC_IDT92HD005D, 0,     "92HD005D" },
	{ HDA_CODEC_IDT92HD206X, 0,     "92HD206X" },
	{ HDA_CODEC_IDT92HD206D, 0,     "92HD206D" },
	{ HDA_CODEC_IDT92HD66B1X5, 0,   "92HD66B1X5" },
	{ HDA_CODEC_IDT92HD66B2X5, 0,   "92HD66B2X5" },
	{ HDA_CODEC_IDT92HD66B3X5, 0,   "92HD66B3X5" },
	{ HDA_CODEC_IDT92HD66C1X5, 0,   "92HD66C1X5" },
	{ HDA_CODEC_IDT92HD66C2X5, 0,   "92HD66C2X5" },
	{ HDA_CODEC_IDT92HD66C3X5, 0,   "92HD66C3X5" },
	{ HDA_CODEC_IDT92HD66B1X3, 0,   "92HD66B1X3" },
	{ HDA_CODEC_IDT92HD66B2X3, 0,   "92HD66B2X3" },
	{ HDA_CODEC_IDT92HD66B3X3, 0,   "92HD66B3X3" },
	{ HDA_CODEC_IDT92HD66C1X3, 0,   "92HD66C1X3" },
	{ HDA_CODEC_IDT92HD66C2X3, 0,   "92HD66C2X3" },
	{ HDA_CODEC_IDT92HD66C3_65, 0,  "92HD66C3_65" },
	{ HDA_CODEC_IDT92HD700X, 0,     "92HD700X" },
	{ HDA_CODEC_IDT92HD700D, 0,     "92HD700D" },
	{ HDA_CODEC_IDT92HD71B5, 0,     "92HD71B5" },
	{ HDA_CODEC_IDT92HD71B5_2, 0,   "92HD71B5" },
	{ HDA_CODEC_IDT92HD71B6, 0,     "92HD71B6" },
	{ HDA_CODEC_IDT92HD71B6_2, 0,   "92HD71B6" },
	{ HDA_CODEC_IDT92HD71B7, 0,     "92HD71B7" },
	{ HDA_CODEC_IDT92HD71B7_2, 0,   "92HD71B7" },
	{ HDA_CODEC_IDT92HD71B8, 0,     "92HD71B8" },
	{ HDA_CODEC_IDT92HD71B8_2, 0,   "92HD71B8" },
	{ HDA_CODEC_IDT92HD73C1, 0,     "92HD73C1" },
	{ HDA_CODEC_IDT92HD73D1, 0,     "92HD73D1" },
	{ HDA_CODEC_IDT92HD73E1, 0,     "92HD73E1" },
	{ HDA_CODEC_IDT92HD95, 0,	"92HD95" },
	{ HDA_CODEC_IDT92HD75B3, 0,     "92HD75B3" },
	{ HDA_CODEC_IDT92HD88B3, 0,     "92HD88B3" },
	{ HDA_CODEC_IDT92HD88B1, 0,     "92HD88B1" },
	{ HDA_CODEC_IDT92HD88B2, 0,     "92HD88B2" },
	{ HDA_CODEC_IDT92HD88B4, 0,     "92HD88B4" },
	{ HDA_CODEC_IDT92HD75BX, 0,     "92HD75BX" },
	{ HDA_CODEC_IDT92HD81B1C, 0,    "92HD81B1C" },
	{ HDA_CODEC_IDT92HD81B1X, 0,    "92HD81B1X" },
	{ HDA_CODEC_IDT92HD83C1C, 0,    "92HD83C1C" },
	{ HDA_CODEC_IDT92HD83C1X, 0,    "92HD83C1X" },
	{ HDA_CODEC_IDT92HD87B1_3, 0,   "92HD87B1/3" },
	{ HDA_CODEC_IDT92HD87B2_4, 0,   "92HD87B2/4" },
	{ HDA_CODEC_IDT92HD89C3, 0,     "92HD89C3" },
	{ HDA_CODEC_IDT92HD89C2, 0,     "92HD89C2" },
	{ HDA_CODEC_IDT92HD89C1, 0,     "92HD89C1" },
	{ HDA_CODEC_IDT92HD89B3, 0,     "92HD89B3" },
	{ HDA_CODEC_IDT92HD89B2, 0,     "92HD89B2" },
	{ HDA_CODEC_IDT92HD89B1, 0,     "92HD89B1" },
	{ HDA_CODEC_IDT92HD89E3, 0,     "92HD89E3" },
	{ HDA_CODEC_IDT92HD89E2, 0,     "92HD89E2" },
	{ HDA_CODEC_IDT92HD89E1, 0,     "92HD89E1" },
	{ HDA_CODEC_IDT92HD89D3, 0,     "92HD89D3" },
	{ HDA_CODEC_IDT92HD89D2, 0,     "92HD89D2" },
	{ HDA_CODEC_IDT92HD89D1, 0,     "92HD89D1" },
	{ HDA_CODEC_IDT92HD89F3, 0,     "92HD89F3" },
	{ HDA_CODEC_IDT92HD89F2, 0,     "92HD89F2" },
	{ HDA_CODEC_IDT92HD89F1, 0,     "92HD89F1" },
	{ HDA_CODEC_IDT92HD90BXX, 0,    "92HD90BXX" },
	{ HDA_CODEC_IDT92HD91BXX, 0,    "92HD91BXX" },
	{ HDA_CODEC_IDT92HD93BXX, 0,    "92HD93BXX" },
	{ HDA_CODEC_IDT92HD98BXX, 0,    "92HD98BXX" },
	{ HDA_CODEC_IDT92HD99BXX, 0,    "92HD99BXX" },

	{ HDA_CODEC_CX20549, 0,         "CX20549 (Venice)" },
	{ HDA_CODEC_CX20551, 0,         "CX20551 (Waikiki)" },
	{ HDA_CODEC_CX20561, 0,         "CX20561 (Hermosa)" },
	{ HDA_CODEC_CX20582, 0,         "CX20582 (Pebble)" },
	{ HDA_CODEC_CX20583, 0,         "CX20583 (Pebble HSF)" },
	{ HDA_CODEC_CX20584, 0,         "CX20584" },
	{ HDA_CODEC_CX20585, 0,         "CX20585" },
	{ HDA_CODEC_CX20588, 0,         "CX20588" },
	{ HDA_CODEC_CX20590, 0,         "CX20590" },
	{ HDA_CODEC_CX20631, 0,         "CX20631" },
	{ HDA_CODEC_CX20632, 0,         "CX20632" },
	{ HDA_CODEC_CX20641, 0,         "CX20641" },
	{ HDA_CODEC_CX20642, 0,         "CX20642" },
	{ HDA_CODEC_CX20651, 0,         "CX20651" },
	{ HDA_CODEC_CX20652, 0,         "CX20652" },
	{ HDA_CODEC_CX20664, 0,         "CX20664" },
	{ HDA_CODEC_CX20665, 0,         "CX20665" },
	{ HDA_CODEC_CX20751, 0,		"CX20751/2" },
	{ HDA_CODEC_CX20751_2, 0,	"CX20751/2" },
	{ HDA_CODEC_CX20751_4, 0,	"CX20753/4" },
	{ HDA_CODEC_CX20755, 0,         "CX20755" },
	{ HDA_CODEC_CX20756, 0,         "CX20756" },
	{ HDA_CODEC_CX20757, 0,         "CX20757" },
	{ HDA_CODEC_CX20952, 0,         "CX20952" },

	{ HDA_CODEC_VT1708_8, 0,        "VT1708_8" },
	{ HDA_CODEC_VT1708_9, 0,        "VT1708_9" },
	{ HDA_CODEC_VT1708_A, 0,        "VT1708_A" },
	{ HDA_CODEC_VT1708_B, 0,        "VT1708_B" },
	{ HDA_CODEC_VT1709_0, 0,        "VT1709_0" },
	{ HDA_CODEC_VT1709_1, 0,        "VT1709_1" },
	{ HDA_CODEC_VT1709_2, 0,        "VT1709_2" },
	{ HDA_CODEC_VT1709_3, 0,        "VT1709_3" },
	{ HDA_CODEC_VT1709_4, 0,        "VT1709_4" },
	{ HDA_CODEC_VT1709_5, 0,        "VT1709_5" },
	{ HDA_CODEC_VT1709_6, 0,        "VT1709_6" },
	{ HDA_CODEC_VT1709_7, 0,        "VT1709_7" },
	{ HDA_CODEC_VT1708B_0, 0,       "VT1708B_0" },
	{ HDA_CODEC_VT1708B_1, 0,       "VT1708B_1" },
	{ HDA_CODEC_VT1708B_2, 0,       "VT1708B_2" },
	{ HDA_CODEC_VT1708B_3, 0,       "VT1708B_3" },
	{ HDA_CODEC_VT1708B_4, 0,       "VT1708B_4" },
	{ HDA_CODEC_VT1708B_5, 0,       "VT1708B_5" },
	{ HDA_CODEC_VT1708B_6, 0,       "VT1708B_6" },
	{ HDA_CODEC_VT1708B_7, 0,       "VT1708B_7" },
	{ HDA_CODEC_VT1708S_0, 0,       "VT1708S_0" },
	{ HDA_CODEC_VT1708S_1, 0,       "VT1708S_1" },
	{ HDA_CODEC_VT1708S_2, 0,       "VT1708S_2" },
	{ HDA_CODEC_VT1708S_3, 0,       "VT1708S_3" },
	{ HDA_CODEC_VT1708S_4, 0,       "VT1708S_4" },
	{ HDA_CODEC_VT1708S_5, 0,       "VT1708S_5" },
	{ HDA_CODEC_VT1708S_6, 0,       "VT1708S_6" },
	{ HDA_CODEC_VT1708S_7, 0,       "VT1708S_7" },
	{ HDA_CODEC_VT1702_0, 0,        "VT1702_0" },
	{ HDA_CODEC_VT1702_1, 0,        "VT1702_1" },
	{ HDA_CODEC_VT1702_2, 0,        "VT1702_2" },
	{ HDA_CODEC_VT1702_3, 0,        "VT1702_3" },
	{ HDA_CODEC_VT1702_4, 0,        "VT1702_4" },
	{ HDA_CODEC_VT1702_5, 0,        "VT1702_5" },
	{ HDA_CODEC_VT1702_6, 0,        "VT1702_6" },
	{ HDA_CODEC_VT1702_7, 0,        "VT1702_7" },
	{ HDA_CODEC_VT1716S_0, 0,       "VT1716S_0" },
	{ HDA_CODEC_VT1716S_1, 0,       "VT1716S_1" },
	{ HDA_CODEC_VT1718S_0, 0,       "VT1718S_0" },
	{ HDA_CODEC_VT1718S_1, 0,       "VT1718S_1" },
	{ HDA_CODEC_VT1802_0, 0,        "VT1802_0" },
	{ HDA_CODEC_VT1802_1, 0,        "VT1802_1" },
	{ HDA_CODEC_VT1812, 0,          "VT1812" },
	{ HDA_CODEC_VT1818S, 0,         "VT1818S" },
	{ HDA_CODEC_VT1828S, 0,         "VT1828S" },
	{ HDA_CODEC_VT2002P_0, 0,       "VT2002P_0" },
	{ HDA_CODEC_VT2002P_1, 0,       "VT2002P_1" },
	{ HDA_CODEC_VT2020, 0,          "VT2020" },

	{ HDA_CODEC_ATIRS600_1, 0,      "RS600" },
	{ HDA_CODEC_ATIRS600_2, 0,      "RS600" },
	{ HDA_CODEC_ATIRS690, 0,        "RS690/780" },
	{ HDA_CODEC_ATIR6XX, 0,         "R6xx" },

	{ HDA_CODEC_NVIDIAMCP67, 0,     "MCP67" },
	{ HDA_CODEC_NVIDIAMCP73, 0,     "MCP73" },
	{ HDA_CODEC_NVIDIAMCP78, 0,     "MCP78" },
	{ HDA_CODEC_NVIDIAMCP78_2, 0,   "MCP78" },
	{ HDA_CODEC_NVIDIAMCP78_3, 0,   "MCP78" },
	{ HDA_CODEC_NVIDIAMCP78_4, 0,   "MCP78" },
	{ HDA_CODEC_NVIDIAMCP7A, 0,     "MCP7A" },
	{ HDA_CODEC_NVIDIAGT220, 0,     "GT220" },
	{ HDA_CODEC_NVIDIAGT21X, 0,     "GT21x" },
	{ HDA_CODEC_NVIDIAMCP89, 0,     "MCP89" },
	{ HDA_CODEC_NVIDIAGT240, 0,     "GT240" },
	{ HDA_CODEC_NVIDIAGTS450, 0,    "GTS450" },
	{ HDA_CODEC_NVIDIAGT440, 0,     "GT440" }, // Revision Id: 0x100100
	{ HDA_CODEC_NVIDIAGTX470, 0,     "GT470" },
	{ HDA_CODEC_NVIDIAGTX550, 0,    "GTX550" },
	{ HDA_CODEC_NVIDIAGTX570, 0,    "GTX570" },
	{ HDA_CODEC_NVIDIAGT610, 0,	"GT610" },


	{ HDA_CODEC_INTELIP, 0,         "Ibex Peak" },
	{ HDA_CODEC_INTELBL, 0,         "Bearlake" },
	{ HDA_CODEC_INTELCA, 0,         "Cantiga" },
	{ HDA_CODEC_INTELEL, 0,         "Eaglelake" },
	{ HDA_CODEC_INTELIP2, 0,        "Ibex Peak" },
	{ HDA_CODEC_INTELCPT, 0,        "Cougar Point" },
	{ HDA_CODEC_INTELPPT, 0,        "Panther Point" },
	{ HDA_CODEC_INTELLLP, 0,        "Haswell" },
	{ HDA_CODEC_INTELBRW, 0,        "Broadwell" },
	{ HDA_CODEC_INTELSKL, 0,        "Skylake" },
	{ HDA_CODEC_INTELCDT, 0,        "CedarTrail" },
	{ HDA_CODEC_INTELVLV, 0,        "Valleyview2" },
	{ HDA_CODEC_INTELBSW, 0,        "Braswell" },
	{ HDA_CODEC_INTELCL, 0,         "Crestline" },

	{ HDA_CODEC_SII1390, 0,         "SiI1390 HDMi" },
	{ HDA_CODEC_SII1392, 0,         "SiI1392 HDMi" },

	// Unknown CODECs
	{ HDA_CODEC_ADXXXX, 0,          "Analog Devices" },
	{ HDA_CODEC_AGEREXXXX, 0,       "Lucent/Agere Systems" },
	{ HDA_CODEC_ALCXXXX, 0,         "Realtek" },
	{ HDA_CODEC_ATIXXXX, 0,         "ATI" },
	{ HDA_CODEC_CAXXXX, 0,          "Creative" },
	{ HDA_CODEC_CMIXXXX, 0,         "CMedia" },
	{ HDA_CODEC_CMIXXXX2, 0,        "CMedia" },
	{ HDA_CODEC_CSXXXX, 0,          "Cirrus Logic" },
	{ HDA_CODEC_CXXXXX, 0,          "Conexant" },
	{ HDA_CODEC_CHXXXX, 0,          "Chrontel" },
	{ HDA_CODEC_IDTXXXX, 0,         "IDT" },
	{ HDA_CODEC_INTELXXXX, 0,       "Intel" },
	{ HDA_CODEC_MOTOXXXX, 0,        "Motorola" },
	{ HDA_CODEC_NVIDIAXXXX, 0,      "NVIDIA" },
	{ HDA_CODEC_SIIXXXX, 0,         "Silicon Image" },
	{ HDA_CODEC_STACXXXX, 0,        "Sigmatel" },
	{ HDA_CODEC_VTXXXX, 0,          "VIA" },
};

#define HDACC_CODECS_LEN        (sizeof(know_codecs) / sizeof(know_codecs[0]))

/*****************
 * Device Methods
 *****************/

/* get HDA device name */
static char *get_hda_controller_name(uint16_t controller_device_id, uint16_t controller_vendor_id)
{
	static char desc[128];

	const char *name_format  = "Unknown HD Audio device %s";
	uint32_t controller_model = ((controller_device_id << 16) | controller_vendor_id);
	int i;

	/* Get format for vendor ID */
	switch (controller_vendor_id)
	{
		case ATI_VENDORID:
			name_format = "ATI %s HDA Controller (HDMi)"; break;

		case INTEL_VENDORID:
			name_format = "Intel %s HDA Controller"; break;

		case NVIDIA_VENDORID:
			name_format = "nVidia %s HDA Controller (HDMi)"; break;

		case RDC_VENDORID:
			name_format = "RDC %s HDA Controller"; break;

		case SIS_VENDORID:
			name_format = "SiS %s HDA Controller"; break;

		case ULI_VENDORID:
			name_format = "ULI %s HDA Controller"; break;

		case VIA_VENDORID:
			name_format = "VIA %s HDA Controller"; break;

		default:
			break;
	}

	for (i = 0; i < HDAC_DEVICES_LEN; i++)
	{
		if (know_hda_controller[i].model == controller_model)
		{
			snprintf(desc, sizeof(desc), name_format, know_hda_controller[i].desc);
			return desc;
		}
	}

	/* Not in table */
	snprintf(desc, sizeof(desc), "Unknown HDA device, vendor %04x, model %04x",
		controller_vendor_id, controller_device_id);
	return desc;
}

/* get Codec name */
static char *get_hda_codec_name( uint16_t codec_vendor_id, uint16_t codec_device_id, uint8_t codec_revision_id, uint8_t codec_stepping_id )
{
	static char desc[128];

	char		*lName_format  = NULL;
	uint32_t	lCodec_model = ((uint32_t)(codec_vendor_id) << 16) + (codec_device_id);
	uint32_t	lCodec_rev = (((uint16_t)(codec_revision_id) << 8) + codec_stepping_id);
	int i;

	// Get format for vendor ID
	switch ( codec_vendor_id ) // uint16_t
	{
		case ANALOGDEVICES_VENDORID:
			lName_format = "Analog Devices %s"; break;

		case AGERE_VENDORID:
			lName_format = "Agere Systems %s "; break;

		case REALTEK_VENDORID:
			lName_format = "Realtek %s"; break;

		case ATI_VENDORID:
			lName_format = "ATI %s"; break;

		case CREATIVE_VENDORID:
			lName_format = "Creative %s"; break;

		case CMEDIA_VENDORID:
		case CMEDIA2_VENDORID:
			lName_format = "CMedia %s"; break;

		case CIRRUSLOGIC_VENDORID:
			lName_format = "Cirrus Logic %s"; break;

		case CONEXANT_VENDORID:
			lName_format = "Conexant %s"; break;

		case CHRONTEL_VENDORID:
			lName_format = "Chrontel %s"; break;

		case IDT_VENDORID:
			lName_format = "IDT %s"; break;

		case INTEL_VENDORID:
			lName_format = "Intel %s"; break;

		case MOTO_VENDORID:
			lName_format = "Motorola %s"; break;

		case NVIDIA_VENDORID:
			lName_format = "nVidia %s"; break;

		case SII_VENDORID:
			lName_format = "Silicon Image %s"; break;

		case SIGMATEL_VENDORID:
			lName_format = "Sigmatel %s"; break;

		case VIA_VENDORID:
			lName_format = "VIA %s"; break;

		default:
			lName_format = UNKNOWN; break;
			break;
	}

	for (i = 0; i < HDACC_CODECS_LEN; i++)
	{
		if ( know_codecs[i].id == lCodec_model )
		{
			if ( ( know_codecs[i].rev == 0x00000000 ) || ( know_codecs[i].rev == lCodec_rev ) )
			{
//				verbose("\tRevision in table (%06x) | burned chip revision (%06x).\n", know_codecs[i].rev, lCodec_rev );
				snprintf(desc, sizeof(desc), lName_format, know_codecs[i].name);
				return desc;
			}
		}
	}

	if ( ( lName_format != UNKNOWN ) && ( strstr(lName_format, "%s" ) != NULL ) )
	{
		// Dirty way to remove '%s' from the end of the lName_format
		int len = strlen(lName_format);
		lName_format[len-2] = '\0';
	}

	// Not in table
	snprintf(desc, sizeof(desc), "unknown %s Codec", lName_format);
	return desc;
}

bool setup_hda_devprop(pci_dt_t *hda_dev)
{
	struct		DevPropDevice	*device = NULL;
	char		*devicepath = NULL;
	char		*controller_name = NULL;
	int		len;
	uint8_t		BuiltIn = 0x00;
	uint16_t	controller_vendor_id = hda_dev->vendor_id;
	uint16_t	controller_device_id = hda_dev->device_id;
	const char	*value;

	// Skip keys
	bool do_skip_n_devprop = false;
	bool do_skip_a_devprop = false;
	getBoolForKey(kSkipNvidiaGfx, &do_skip_n_devprop, &bootInfo->chameleonConfig);
	getBoolForKey(kSkipAtiGfx, &do_skip_a_devprop, &bootInfo->chameleonConfig);

	verbose("\tClass code: [%04X]\n", hda_dev->class_id);

	devicepath = get_pci_dev_path(hda_dev);
	controller_name = get_hda_controller_name(controller_device_id, controller_vendor_id);

	if (!string)
	{
		string = devprop_create_string();
		if (!string)
		{
			return 0;
		}
	}

	if (!devicepath)
	{
		return 0;
	}

	device = devprop_add_device(string, devicepath);
	if (!device)
	{
		return 0;
	}

	verbose("\tModel name: %s [%04x:%04x] (rev %02x)\n\tSubsystem: [%04x:%04x]\n\t%s\n",
		 controller_name, hda_dev->vendor_id, hda_dev->device_id, hda_dev->revision_id,
		hda_dev->subsys_id.subsys.vendor_id, hda_dev->subsys_id.subsys.device_id, devicepath);

	probe_hda_bus(hda_dev->dev.addr);

	switch ((controller_device_id << 16) | controller_vendor_id)
	{

	/***********************************************************************
	* The above case are intended as for HDEF device at address 0x001B0000
	***********************************************************************/
		case HDA_INTEL_OAK:
		case HDA_INTEL_BAY:
		case HDA_INTEL_HSW1:
		case HDA_INTEL_HSW2:
		case HDA_INTEL_HSW3:
		case HDA_INTEL_BDW:
		case HDA_INTEL_CPT:
		case HDA_INTEL_PATSBURG:
		case HDA_INTEL_PPT1:
		case HDA_INTEL_BRASWELL:
		case HDA_INTEL_82801F:
		case HDA_INTEL_63XXESB:
		case HDA_INTEL_82801G:
		case HDA_INTEL_82801H:
		case HDA_INTEL_82801I:
		case HDA_INTEL_ICH9:
		case HDA_INTEL_82801JI:
		case HDA_INTEL_82801JD:
		case HDA_INTEL_PCH:
		case HDA_INTEL_PCH2:
		case HDA_INTEL_SCH:
		case HDA_INTEL_LPT1:
		case HDA_INTEL_LPT2:
		case HDA_INTEL_WCPT:
		case HDA_INTEL_WELLS1:
		case HDA_INTEL_WELLS2:
		case HDA_INTEL_WCPTLP:
		case HDA_INTEL_LPTLP1:
		case HDA_INTEL_LPTLP2:
		case HDA_INTEL_SRSPLP:
		case HDA_INTEL_SRSP:

			/* if the key value kHDEFLayoutID as a value set that value, if not will assign a default layout */
			if (getValueForKey(kHDEFLayoutID, &value, &len, &bootInfo->chameleonConfig) && len == HDEF_LEN * 2)
			{
				uint8_t new_HDEF_layout_id[HDEF_LEN];
				if (hex2bin(value, new_HDEF_layout_id, HDEF_LEN) == 0)
				{
					memcpy(default_HDEF_layout_id, new_HDEF_layout_id, HDEF_LEN);
					verbose("\tUsing user supplied HDEF layout-id: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
					default_HDEF_layout_id[0], default_HDEF_layout_id[1], default_HDEF_layout_id[2], default_HDEF_layout_id[3]);
				}
			}
			else
			{
				verbose("\tUsing default HDEF layout-id: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
					default_HDEF_layout_id[0], default_HDEF_layout_id[1], default_HDEF_layout_id[2], default_HDEF_layout_id[3]);
			}
			devprop_add_value(device, "layout-id", default_HDEF_layout_id, HDEF_LEN);
			devprop_add_value(device, "AAPL,slot-name", (uint8_t *)"Built-in", sizeof("Built-in")); // 0x09
			devprop_add_value(device, "name", (uint8_t *)"audio", 6); // 0x06
			devprop_add_value(device, "device-type", (uint8_t *)"High Definition Audio Controller", sizeof("High Definition Audio Controller"));
			devprop_add_value(device, "device_type", (uint8_t *)"Sound", sizeof("Sound"));
			devprop_add_value(device, "built-in", &BuiltIn, 1);
			devprop_add_value(device, "hda-gfx", (uint8_t *)"onboard-1", sizeof("onboard-1")); // 0x0a
			// "AFGLowPowerState" = <03000000>
			break;

	/*****************************************************************************************************************
	 * The above case are intended as for HDAU (NVIDIA) device onboard audio for GFX card with Audio controller HDMi *
	 *****************************************************************************************************************/
		case HDA_NVIDIA_GK107:
		case HDA_NVIDIA_GF110_1:
		case HDA_NVIDIA_GF110_2:
		case HDA_NVIDIA_GK106:
		case HDA_NVIDIA_GK104:
		case HDA_NVIDIA_GF119:
		case HDA_NVIDIA_GT116:
		case HDA_NVIDIA_GT104:
		case HDA_NVIDIA_GT108:
		case HDA_NVIDIA_GT106:
		case HDA_NVIDIA_GT100:
		case HDA_NVIDIA_0BE4:
		case HDA_NVIDIA_0BE3:
		case HDA_NVIDIA_0BE2:
			if ( do_skip_n_devprop )
			{
				verbose("Skip Nvidia audio device!\n");
			}
			else
			{
				/* if the key value kHDAULayoutID as a value set that value, if not will assign a default layout */
				if (getValueForKey(kHDAULayoutID, &value, &len, &bootInfo->chameleonConfig) && len == HDAU_LEN * 2)
				{
					uint8_t new_HDAU_layout_id[HDAU_LEN];
					if (hex2bin(value, new_HDAU_layout_id, HDAU_LEN) == 0)
					{
						memcpy(default_HDAU_layout_id, new_HDAU_layout_id, HDAU_LEN);
						verbose("\tUsing user supplied HDAU layout-id: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
							default_HDAU_layout_id[0], default_HDAU_layout_id[1], default_HDAU_layout_id[2], default_HDAU_layout_id[3]);
					}
				}
				else
				{
					verbose("\tUsing default HDAU layout-id: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
						default_HDAU_layout_id[0], default_HDAU_layout_id[1], default_HDAU_layout_id[2], default_HDAU_layout_id[3]);
				}

				devprop_add_value(device, "layout-id", default_HDAU_layout_id, HDAU_LEN); /*FIX ME*/
				devprop_add_value(device, "@0,connector-type", connector_type_value, 4);
				devprop_add_value(device, "@1,connector-type", connector_type_value, 4);
				devprop_add_value(device, "hda-gfx", (uint8_t *)"onboard-2", sizeof("onboard-2"));
				devprop_add_value(device, "built-in", &BuiltIn, 1);
			}
			break;

		/**************************************************************************************************************
		 * The above case are intended as for HDAU (ATi) device onboard audio for GFX card with Audio controller HDMi *
		 **************************************************************************************************************/
		case HDA_ATI_SB450:
		case HDA_ATI_SB600:
		case HDA_ATI_HUDSON:
		case HDA_ATI_RS600:
		case HDA_ATI_RS690:
		case HDA_ATI_RS780:
		case HDA_ATI_R600:
		case HDA_ATI_RV630:
		case HDA_ATI_RV610:
		case HDA_ATI_RV670:
		case HDA_ATI_RV635:
		case HDA_ATI_RV620:
		case HDA_ATI_RV770:
		case HDA_ATI_RV730:
		case HDA_ATI_RV710:
		case HDA_ATI_RV740:
		case HDA_ATI_RV870:
		case HDA_ATI_RV840:
		case HDA_ATI_RV830:
		case HDA_ATI_RV810:
		case HDA_ATI_RV970:
		case HDA_ATI_RV940:
		case HDA_ATI_RV930:
		case HDA_ATI_RV910:
		case HDA_ATI_R1000:
		case HDA_ATI_SI:
		case HDA_ATI_VERDE:
			if ( do_skip_a_devprop )
			{
				verbose("Skip ATi/AMD audio device!\n");
			}
			else
			{
				/* if the key value kHDAULayoutID as a value set that value, if not will assign a default layout */
				if (getValueForKey(kHDAULayoutID, &value, &len, &bootInfo->chameleonConfig) && len == HDAU_LEN * 2)
				{
					uint8_t new_HDAU_layout_id[HDAU_LEN];
					if (hex2bin(value, new_HDAU_layout_id, HDAU_LEN) == 0)
					{
						memcpy(default_HDAU_layout_id, new_HDAU_layout_id, HDAU_LEN);
						verbose("\tUsing user supplied HDAU layout-id: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
							default_HDAU_layout_id[0], default_HDAU_layout_id[1], default_HDAU_layout_id[2], default_HDAU_layout_id[3]);
					}
				}
				else
				{
					verbose("\tUsing default HDAU layout-id: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
						default_HDAU_layout_id[0], default_HDAU_layout_id[1], default_HDAU_layout_id[2], default_HDAU_layout_id[3]);
				}

				devprop_add_value(device, "layout-id", default_HDAU_layout_id, HDAU_LEN); /*FIX ME*/
				devprop_add_value(device, "hda-gfx", (uint8_t *)"onboard-2", 10);
				devprop_add_value(device, "built-in", &BuiltIn, 1);
			}
			break;

		default:
			break;
	}

	stringdata = malloc(sizeof(uint8_t) * string->length);
	memcpy(stringdata, (uint8_t*)devprop_generate_string(string), string->length);
	stringlength = string->length;

	return true;
}

/*
 * Structure of HDA MMIO Region
 */
struct HDARegs
{
	uint16_t gcap;
	uint8_t vmin;
	uint8_t vmaj;
	uint16_t outpay;
	uint16_t inpay;
	uint32_t gctl;
	uint16_t wakeen;
	uint16_t statests;
	uint16_t gsts;
	uint8_t rsvd0[6];
	uint16_t outstrmpay;
	uint16_t instrmpay;
	uint8_t rsvd1[4];
	uint32_t intctl;
	uint32_t intsts;
	uint8_t rsvd2[8];
	uint32_t walclk;
	uint8_t rsvd3[4];
	uint32_t ssync;
	uint8_t rsvd4[4];
	uint32_t corblbase;
	uint32_t corbubase;
	uint16_t corbwp;
	uint16_t corbrp;
	uint8_t corbctl;
	uint8_t corbsts;
	uint8_t corbsize;
	uint8_t rsvd5;
	uint32_t rirblbase;
	uint32_t rirbubase;
	uint16_t rirbwp;
	uint16_t rintcnt;
	uint8_t rirbctl;
	uint8_t rirbsts;
	uint8_t rirbsize;
	uint8_t rsvd6;
	uint32_t icoi;
	uint32_t icii;
	uint16_t icis;
	uint8_t rsvd7[6];
	uint32_t dpiblbase;
	uint32_t dpibubase;
	uint8_t rsvd8[8];
/*
 * Stream Descriptors follow
 */
} __attribute__((aligned(16), packed));

/*
 * Data to be discovered for HDA codecs
 */

struct HDACodecInfo
{
	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t revision_id;
	uint8_t stepping_id;
	uint8_t maj_rev;
	uint8_t min_rev;
	uint8_t num_function_groups;
	const char     *name;
};

/*
 * Timing Functions
 */

static int wait_for_register_state_16(uint16_t const volatile* reg,
				uint16_t target_mask,
				uint16_t target_value,
				uint32_t timeout_us,
				uint32_t tsc_ticks_per_us)
{
	uint64_t deadline = rdtsc64() + MultU32x32(timeout_us, tsc_ticks_per_us);
	do
	{
		uint16_t value = *reg;
		if ((value & target_mask) == target_value)
			return 0;
		CpuPause();
	}
	while (rdtsc64() < deadline);
	return -1;
}

static void delay_us(uint32_t timeout_us, uint32_t tsc_ticks_per_us)
{
	uint64_t deadline = rdtsc64() + MultU32x32(timeout_us, tsc_ticks_per_us);

	do
	{
		CpuPause();
	}
	while (rdtsc64() < deadline);
}

static struct HDARegs volatile* hdaMemory = NULL;
static uint32_t tsc_ticks_per_us = 0U;

#define ICIS_ICB 1U
#define ICIS_IRV 2U

static int immediate_command(uint32_t command, uint32_t* response)
{
	/*
	 * Wait up to 1ms for for ICB 0
	 */
	(void) wait_for_register_state_16(&hdaMemory->icis, ICIS_ICB, 0U, 1000U, tsc_ticks_per_us);
	/*
	 * Ignore timeout and force ICB to 0
	 * Clear IRV while at it
	 */
	hdaMemory->icis = ICIS_IRV;
	/*
	 * Program command
	 */
	hdaMemory->icoi = command;
	/*
	 * Trigger command
	 * Clear IRV again just in case
	 */
	hdaMemory->icis = ICIS_ICB | ICIS_IRV;
	/*
	 * Wait up to 1ms for response
	 */
	if (wait_for_register_state_16(&hdaMemory->icis, ICIS_IRV, ICIS_IRV, 1000U, tsc_ticks_per_us) < 0)
	{
		/*
		 * response timed out
		 */
		return -1;
	}
	*response = hdaMemory->icii;
	return 0;
}

#define PACK_CID(x) ((x & 15U) << 28)
#define PACK_NID(x) ((x & 127U) << 20)
#define PACK_VERB_12BIT(x) ((x & 4095U) << 8)
#define PACK_PAYLOAD_8BIT(x) (x & UINT8_MAX)
#define VERB_GET_PARAMETER 0xF00U

static uint32_t get_parameter(uint8_t codec_id, uint8_t node_id, uint8_t parameter_id)
{
	uint32_t command, response;

	command = PACK_CID(codec_id) | PACK_NID(node_id) | PACK_VERB_12BIT(VERB_GET_PARAMETER) | PACK_PAYLOAD_8BIT(parameter_id);
	response = UINT32_MAX;

	/*
	 * Ignore timeout, return UINT32_MAX as error value
	 */
	(void) immediate_command(command, &response);
	return response;
}

#define PARAMETER_VID_DID 0U
#define PARAMETER_RID 2U
#define PARAMETER_NUM_NODES 4U

static void probe_hda_codec(uint8_t codec_id, struct HDACodecInfo *codec_info)
{
	uint32_t response;
	CDBG("\tprobing codec %d\n", codec_id);
	response = get_parameter(codec_id, 0U, PARAMETER_VID_DID);
	codec_info->vendor_id = (response >> 16) & UINT16_MAX;
	codec_info->device_id = response & UINT16_MAX;
	response = get_parameter(codec_id, 0U, PARAMETER_RID);
	codec_info->revision_id = (response >> 8) & UINT8_MAX;
	codec_info->stepping_id = response & UINT8_MAX;
	codec_info->maj_rev = (response >> 20) & 15U;
	codec_info->min_rev = (response >> 16) & 15U;
	response = get_parameter(codec_id, 0U, PARAMETER_NUM_NODES);
	codec_info->num_function_groups = response & UINT8_MAX;
	codec_info->name = get_hda_codec_name(codec_info->vendor_id, codec_info->device_id, codec_info->revision_id, codec_info->stepping_id);

}

static int getHDABar(uint32_t pci_addr, uint32_t* bar_phys_addr)
{
	uint32_t barlow = pci_config_read32(pci_addr, PCI_BASE_ADDRESS_0);

	if ((barlow & PCI_BASE_ADDRESS_SPACE) != PCI_BASE_ADDRESS_SPACE_MEMORY)
	{
		CDBG("\tBAR0 for HDA Controller 0x%x is not an MMIO space\n", pci_addr);
		return -1;
	}

	if ((barlow & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_64)
	{
		uint32_t barhigh = pci_config_read32(pci_addr, PCI_BASE_ADDRESS_1);

		if (barhigh)
		{
			//verbose("\tBAR0 for HDA Controller 0x%x is located ouside 32-bit physical address space (0x%x%08x)\n",
			//pci_addr, barhigh, barlow & PCI_BASE_ADDRESS_MEM_MASK);
			return -1;
		}
	}

	if (bar_phys_addr)
	{
		*bar_phys_addr = (barlow & PCI_BASE_ADDRESS_MEM_MASK);
	}
	return 0;
}

void probe_hda_bus(uint32_t pci_addr)
{
	uint64_t tsc_frequency;
	uint32_t bar_phys_addr;
	uint16_t pci_cmd, statests;
	uint16_t const pci_cmd_wanted = PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
	uint8_t codec_id, original_reset_state;
	struct HDACodecInfo codec_info;

	CDBG("\tlooking for HDA bar0 on pci_addr 0x%x\n", pci_addr);
	if (getHDABar(pci_addr, &bar_phys_addr) < 0)
	{
		return;
	}

	CDBG("\tfound HDA memory at 0x%x\n", bar_phys_addr);
	hdaMemory = (struct HDARegs volatile*) bar_phys_addr;

	tsc_frequency = Platform.CPU.TSCFrequency;
	tsc_ticks_per_us = DivU64x32(tsc_frequency, 1000000U);  // TSC ticks per microsecond
	CDBG("\ttsc_ticks_per_us %d\n", tsc_ticks_per_us);

	/*
	 * Enable Memory Space and Bus Mastering
	 */
	pci_cmd = pci_config_read16(pci_addr, PCI_COMMAND);
	if ((pci_cmd & pci_cmd_wanted) != pci_cmd_wanted)
	{
		pci_cmd |= pci_cmd_wanted;
		pci_config_write16(pci_addr, PCI_COMMAND, pci_cmd);
	}

	/*
	 * Remember entering reset state
	 */
	original_reset_state = (hdaMemory->gctl & HDAC_GCTL_CRST) ? 1U : 0U;

	/*
	 * Reset HDA Controller
	 */
	hdaMemory->wakeen = 0U;
	hdaMemory->statests = UINT16_MAX;
	hdaMemory->gsts = UINT16_MAX;
	hdaMemory->intctl = 0U;
	CDBG("\tStarting reset\n");
	hdaMemory->gctl = 0U;

	/*
	 * Wait up to 10ms to enter Reset
	 */
	if (wait_for_register_state_16((uint16_t volatile const*) &hdaMemory->gctl,
				HDAC_GCTL_CRST,
				0U,
				10000U,
				tsc_ticks_per_us) < 0)
	{
		CDBG("\tHDA Controller 0x%x timed out 10ms entering reset\n", pci_addr);
		return;
	}
	CDBG("\tReset asserted, delay 100us\n");

	/*
	 * Delay 2400 BCLK (100us)
	 */
	delay_us(100U, tsc_ticks_per_us);
	CDBG("\tDeasserting reset\n");

	/*
	 * Wait up to 10ms to exit Reset
	 */
	hdaMemory->gctl = HDAC_GCTL_CRST;
	if (wait_for_register_state_16((uint16_t volatile const*) &hdaMemory->gctl,
				HDAC_GCTL_CRST,
				HDAC_GCTL_CRST,
				10000U,
				tsc_ticks_per_us) < 0)
	{
		CDBG("\tHDA Controller 0x%x timed out 10ms exiting reset\n", pci_addr);
		return;
	}
	CDBG("\tReset complete\n");

	/*
	 * Wait 1ms for codecs to request enumeration (spec says 521us).
	 */
	delay_us(1000U, tsc_ticks_per_us);

	/*
	 * See which codecs want enumeration
	 */
	statests = hdaMemory->statests;
	hdaMemory->statests = statests; // clear statests
	CDBG("\tstatests is now 0x%x\n", statests);
	codec_id = 0U;
	while (statests)
	{
		if (statests & 1U)
		{
			probe_hda_codec(codec_id, &codec_info);

			verbose("\tFound %s (%04x%04x), rev(%04x)",
			codec_info.name,
			codec_info.vendor_id,
			codec_info.device_id,
			codec_info.revision_id);
#if DEBUG_CODEC
			verbose(", stepping 0x%x, major rev 0x%x, minor rev 0x%x, %d function groups",
			codec_info.stepping_id,
			codec_info.maj_rev,
			codec_info.min_rev,
			codec_info.num_function_groups);
#endif
			verbose("\n");
		}
		++codec_id;
		statests >>= 1;
	}

	/*
	 * Restore reset state entered with
	 */
	if (!original_reset_state)
	{
		hdaMemory->gctl = 0U;
	}
}
