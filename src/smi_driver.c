/* Header:   //Mercury/Projects/archives/XFree86/4.0/smi_driver.c-arc   1.42   03 Jan 2001 13:52:16   Frido  $ */

/*
Copyright (C) 1994-1999 The XFree86 Project, Inc.  All Rights Reserved.
Copyright (C) 2000 Silicon Motion, Inc.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the names of The XFree86 Project and
Silicon Motion shall not be used in advertising or otherwise to promote the
sale, use or other dealings in this Software without prior written
authorization from The XFree86 Project or Silicon Motion.
*/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/siliconmotion/smi_driver.c,v 1.30 2003/04/23 21:51:44 tsi Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "xf86Resources.h"
#include "xf86DDC.h"
#include "xf86int10.h"
#include "vbe.h"
#include "shadowfb.h"
#include "smi.h"

#ifdef SMI_DEBUG
#undef SMI_DEBUG
#endif


#include "globals.h"
#define DPMS_SERVER
#include <X11/extensions/dpms.h>

#ifdef RANDR
static Bool    SMI_DriverFunc(ScrnInfoPtr pScrnInfo, xorgDriverFuncOp op,
			      pointer data);
#endif

/*#include "smi_501.h"*/
extern void setPower (SMIPtr pSmi, unsigned int nGates, unsigned int nClock,
		      int control_value);
extern void setDPMS (SMIPtr pSmi, int state);
extern void crtSetMode (SMIPtr pSmi, int nWidth, int nHeight,
			unsigned int fMode, int nHertz, int fbPitch, int bpp);
extern void panelSetMode (SMIPtr pSmi, int nWidth, int nHeight,
			  unsigned int fMode, int nHertz, int fbPitch,
			  int bpp);
extern void panelUseCRT (SMIPtr pSmi, BOOL bEnable);
extern Bool entity_init_501(ScrnInfoPtr pScrn, int entity);

void SMI_PrintMode(DisplayModePtr mode);

/*
 * Internals
 */
void SMI_EnableMmio (ScrnInfoPtr pScrn);
static void SMI_DisableMmio (ScrnInfoPtr pScrn);

/*
 * Forward definitions for the functions that make up the driver.
 */

static const OptionInfoRec *SMI_AvailableOptions (int chipid, int busid);
static void SMI_Identify (int flags);
static Bool SMI_Probe (DriverPtr drv, int flags);
static Bool SMI_PreInit (ScrnInfoPtr pScrn, int flags);
static Bool SMI_EnterVT (int scrnIndex, int flags);
static void SMI_LeaveVT (int scrnIndex, int flags);
static void SMI_Save (ScrnInfoPtr pScrn);
static void SMI_BiosWriteMode (ScrnInfoPtr pScrn, DisplayModePtr mode, SMIRegPtr restore);
static void SMI_WriteMode (ScrnInfoPtr pScrn, vgaRegPtr, SMIRegPtr);
static Bool SMI_ScreenInit (int scrnIndex, ScreenPtr pScreen, int argc,
			    char **argv);
static int SMI_InternalScreenInit (int scrnIndex, ScreenPtr pScreen);
static void SMI_PrintRegs (ScrnInfoPtr);
static ModeStatus SMI_ValidMode (int scrnIndex, DisplayModePtr mode,
				 Bool verbose, int flags);
static void SMI_DisableVideo (ScrnInfoPtr pScrn);
static void SMI_EnableVideo (ScrnInfoPtr pScrn);
static Bool SMI_MapMem (ScrnInfoPtr pScrn);
static void SMI_UnmapMem (ScrnInfoPtr pScrn);
static Bool SMI_ModeInit (ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool SMI_CloseScreen (int scrnIndex, ScreenPtr pScreen);
static Bool SMI_SaveScreen (ScreenPtr pScreen, int mode);
static void SMI_LoadPalette (ScrnInfoPtr pScrn, int numColors, int *indicies,
			     LOCO * colors, VisualPtr pVisual);
static void SMI_DisplayPowerManagementSet (ScrnInfoPtr pScrn,
					   int PowerManagementMode,
					   int flags);
static Bool SMI_ddc1 (int scrnIndex);
static unsigned int SMI_ddc1Read (ScrnInfoPtr pScrn);
static void SMI_FreeScreen (int ScrnIndex, int flags);
static void SMI_ProbeDDC (ScrnInfoPtr pScrn, int index);

static Bool SMI_MSOCSetMode (ScrnInfoPtr pScrn, DisplayModePtr mode);

static void SMI_SaveReg(ScrnInfoPtr pScrn );

static void SMI_RestoreReg(ScrnInfoPtr pScrn );

void SMI_SaveReg_502(ScrnInfoPtr pScrn );
void SMI_RestoreReg_502(ScrnInfoPtr pScrn );
void EnableOverlay(SMIPtr pSmi);
#define SILICONMOTION_NAME          "Silicon Motion"
#define SILICONMOTION_DRIVER_NAME   "siliconmotion"
#define SILICONMOTION_VERSION_NAME  "2.2.19"
#define SILICONMOTION_VERSION_MAJOR 2
#define SILICONMOTION_VERSION_MINOR 2
#define SILICONMOTION_PATCHLEVEL    19
#define SILICONMOTION_DRIVER_VERSION ((SILICONMOTION_VERSION_MAJOR << 24) | \
                                      (SILICONMOTION_VERSION_MINOR << 16) | \
                                      (SILICONMOTION_PATCHLEVEL))



extern int	free_video_memory;
int	entity_index[MAX_ENTITIES] = {-1, -1, -1, -1, -1, -1, -1, -1,
					-1, -1, -1, -1, -1, -1, -1, -1};
int	pci_tag = 0x0;
static int	saved_console_reg = -1;

char videoInterpolation = 0xff;	


/*
 * This contains the functions needed by the server after loading the
 * driver module.  It must be supplied, and gets added the driver list by
 * the Module Setup funtion in the dynamic case.  In the static case a
 * reference to this is compiled in, and this requires that the name of
 * this DriverRec be an upper-case version of the driver name.
 */

/* #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0) */
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
_X_EXPORT DriverRec SILICONMOTION =
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
	 _X_EXPORT DriverRec SILICONMOTION =
#else
DriverRec SILICONMOTION =
#endif
{
  SILICONMOTION_DRIVER_VERSION,
  SILICONMOTION_DRIVER_NAME,
  SMI_Identify,
  SMI_Probe,
  SMI_AvailableOptions,
  NULL,
  0
};

/* Supported chipsets */
static SymTabRec SMIChipsets[] = {
  {PCI_CHIP_SMI910, "Lynx"},
  {PCI_CHIP_SMI810, "LynxE"},
  {PCI_CHIP_SMI820, "Lynx3D"},
  {PCI_CHIP_SMI710, "LynxEM"},
  {PCI_CHIP_SMI712, "LynxEM+"},
  {PCI_CHIP_SMI720, "Lynx3DM"},
  {PCI_CHIP_SMI731, "Cougar3DR"},
  {PCI_CHIP_SMI501, "MSOC"},
  {-1, NULL}
};

static PciChipsets SMIPciChipsets[] = {
  /* numChipset,          PciID,                          Resource */
  {PCI_CHIP_SMI910, PCI_CHIP_SMI910, RES_SHARED_VGA},
  {PCI_CHIP_SMI810, PCI_CHIP_SMI810, RES_SHARED_VGA},
  {PCI_CHIP_SMI820, PCI_CHIP_SMI820, RES_SHARED_VGA},
  {PCI_CHIP_SMI710, PCI_CHIP_SMI710, RES_SHARED_VGA},
  //{PCI_CHIP_SMI712, PCI_CHIP_SMI712, RES_SHARED_VGA},
  {PCI_CHIP_SMI712, PCI_CHIP_SMI712, resVgaIo},
  {PCI_CHIP_SMI720, PCI_CHIP_SMI720, RES_SHARED_VGA},
  {PCI_CHIP_SMI731, PCI_CHIP_SMI731, RES_SHARED_VGA},
  //{PCI_CHIP_SMI501, PCI_CHIP_SMI501, RES_SHARED_VGA},
  {PCI_CHIP_SMI501, PCI_CHIP_SMI501, resVgaIo},  
  {-1, -1, RES_UNDEFINED}
};

typedef enum
{
  OPTION_PCI_BURST,
  OPTION_FIFO_CONSERV,
  OPTION_FIFO_MODERATE,
  OPTION_FIFO_AGGRESSIVE,
  OPTION_PCI_RETRY,
  OPTION_NOACCEL,
  OPTION_MCLK,
  OPTION_SHOWCACHE,
  OPTION_SWCURSOR,  
  OPTION_CLONE_MODE,
  OPTION_24BITPANEL,
  OPTION_HWCURSOR,
  OPTION_SHADOW_FB,
  OPTION_ROTATE,
  OPTION_VIDEOKEY,
  OPTION_BYTESWAP,
  /* CZ 26.10.2001: interlaced video */
  OPTION_INTERLACED,
  /* end CZ */
  OPTION_USEBIOS,
  OPTION_ZOOMONLCD,
  OPTION_PANELWIDTH,
  OPTION_PANELHEIGHT,
  NUMBER_OF_OPTIONS,
    /*Add for CSC Video*/
  OPTION_CSCVIDEO
} SMIOpts;

static const OptionInfoRec SMIOptions[] = {
  {OPTION_PCI_BURST, "pci_burst", OPTV_BOOLEAN, {0}, TRUE},
  {OPTION_FIFO_CONSERV, "fifo_conservative", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_FIFO_MODERATE, "fifo_moderate", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_FIFO_AGGRESSIVE, "fifo_aggressive", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_PCI_RETRY, "pci_retry", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_NOACCEL, "NoAccel", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_MCLK, "set_mclk", OPTV_FREQ, {0}, FALSE},
  {OPTION_SHOWCACHE, "show_cache", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_HWCURSOR, "HWCursor", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_SWCURSOR, "SWCursor", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_SHADOW_FB, "ShadowFB", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_CLONE_MODE, "CloneMode", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_24BITPANEL, "24BitPanel", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_ROTATE, "Rotate", OPTV_ANYSTR, {0}, FALSE},
  {OPTION_VIDEOKEY, "VideoKey", OPTV_INTEGER, {0}, FALSE},
  {OPTION_BYTESWAP, "ByteSwap", OPTV_BOOLEAN, {0}, FALSE},
  /* CZ 26.10.2001: interlaced video */
  {OPTION_INTERLACED, "Interlaced", OPTV_BOOLEAN, {0}, FALSE},
  /* end CZ */
  {OPTION_USEBIOS, "UseBIOS", OPTV_BOOLEAN, {0}, FALSE},
  {OPTION_ZOOMONLCD, "ZoomOnLCD", OPTV_BOOLEAN, {0}, FALSE},
  /* Added by Belcon */
  {OPTION_PANELHEIGHT, "LCDHeight", OPTV_INTEGER, {0}, FALSE},
  {OPTION_PANELWIDTH, "LCDWidth", OPTV_INTEGER, {0}, FALSE},
    /*Add for CSC Video*/
  {OPTION_CSCVIDEO, "CSCVideo",OPTV_BOOLEAN, {0}, FALSE},
  {-1, NULL, OPTV_NONE, {0}, FALSE}
};

/*
 * Lists of symbols that may/may not be required by this driver.
 * This allows the loader to know which ones to issue warnings for.
 *
 * Note that vgahwSymbols and xaaSymbols are referenced outside the
 * XFree86LOADER define in later code, so are defined outside of that
 * define here also.
 */

static const char *vgahwSymbols[] = {
  "vgaHWCopyReg",
  "vgaHWGetHWRec",
  "vgaHWGetIOBase",
  "vgaHWGetIndex",
  "vgaHWInit",
  "vgaHWLock",
  "vgaHWUnLock",
  "vgaHWMapMem",
  "vgaHWProtect",
  "vgaHWRestore",
  "vgaHWSave",
  "vgaHWSaveScreen",
  "vgaHWSetMmioFuncs",
  "vgaHWSetStdFuncs",
  "vgaHWUnmapMem",
/* #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0) */
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
  "vgaHWddc1SetSpeedWeak",
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
 	 "vgaHWddc1SetSpeedWeak", 
#endif
  NULL
};

static const char *xaaSymbols[] = {
// #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0)
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
  	"XAAGetCopyROP",
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
  "XAAGetCopyROP",
#else
  "XAACopyROP",
#endif
  "XAACreateInfoRec",
  "XAADestroyInfoRec",
// #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0)
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
  "XAAGetFallbackOps",
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
 	 "XAAGetFallbackOps",
#else
  "XAAFallbackOps",
  "XAAScreenIndex",
#endif
  "XAAInit",
// #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0)
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
  "XAAGetPatternROP",
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
  "XAAGetPatternROP",
#else
  "XAAPatternROP",
#endif
  NULL
};

static const char *ramdacSymbols[] = {
  "xf86CreateCursorInfoRec",
  "xf86DestroyCursorInfoRec",
  "xf86InitCursor",
  NULL
};

static const char *ddcSymbols[] = {
  "xf86PrintEDID",
  "xf86DoEDID_DDC1",
  "xf86InterpretEDID",
  "xf86DoEDID_DDC2",
  "xf86SetDDCproperties",
  NULL
};

static const char *i2cSymbols[] = {
  "xf86CreateI2CBusRec",
  "xf86CreateI2CDevRec",
  "xf86DestroyI2CBusRec",
  "xf86DestroyI2CDevRec",
  "xf86I2CBusInit",
  "xf86I2CDevInit",
  "xf86I2CReadBytes",
  "xf86I2CWriteByte",
  NULL
};

static const char *shadowSymbols[] = {
  "ShadowFBInit",
  NULL
};

static const char *int10Symbols[] = {
  "xf86ExecX86int10",
  "xf86FreeInt10",
  "xf86InitInt10",
  NULL
};

static const char *vbeSymbols[] = {
  "VBEInit",
  "vbeDoEDID",
  "vbeFree",
  NULL
};

static const char *fbSymbols[] = {

  "fbPictureInit",
  "fbScreenInit",
  NULL
};


static unsigned char SeqTable[144];
static unsigned char CrtTable[168];
static unsigned char DprIndexTable[27]={
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 
	0x10, 0x12, 0x14, 0x18, 0x1c, 0x1e, 0x20, 0x24,
	0x28, 0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x34, 0x38,
	0x3c, 0x40, 0x44,
};
static unsigned int DprTable[27];
static unsigned char VprIndexTable[23]={
	0x00, 0x04, 0x08, 0x0c, 0x10, 0x14, 0x18, 0x1c, 
	0x20, 0x24, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x40, 
	0x54, 0x58, 0x5c, 0x60, 0x64, 0x68, 0x6c
};
static unsigned int VprTable[23];

#ifdef XFree86LOADER

static MODULESETUPPROTO (siliconmotionSetup);

static XF86ModuleVersionInfo SMIVersRec = {
  "siliconmotion",
  MODULEVENDORSTRING,
  MODINFOSTRING1,
  MODINFOSTRING2,
// #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0)
#ifdef XORG_VERSION_CURRENT
  XORG_VERSION_CURRENT,
#else
  XF86_VERSION_CURRENT,
#endif
  SILICONMOTION_VERSION_MAJOR,
  SILICONMOTION_VERSION_MINOR,
  SILICONMOTION_PATCHLEVEL,
  ABI_CLASS_VIDEODRV,
  ABI_VIDEODRV_VERSION,
  MOD_CLASS_VIDEODRV,
  {0, 0, 0, 0}
};

/*
 * This is the module init data for XFree86 modules.
 *
 * Its name has to be the driver name followed by ModuleData.
 */

// #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0)
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
_X_EXPORT XF86ModuleData siliconmotionModuleData =
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
_X_EXPORT XF86ModuleData siliconmotionModuleData =
#else
XF86ModuleData siliconmotionModuleData =
#endif
{
  &SMIVersRec,
  siliconmotionSetup,
  NULL
};

static pointer
siliconmotionSetup (pointer module, pointer opts, int *errmaj, int *errmin)
{
  static Bool setupDone = FALSE;

  
  if (!setupDone)
    {
      setupDone = TRUE;
      xf86AddDriver (&SILICONMOTION, module, 0);

      /*
       * Modules that this driver always requires can be loaded here
       * by calling LoadSubModule().
       */

      /*
       * Tell the loader about symbols from other modules that this module
       * might refer to.
       */
      LoaderRefSymLists (vgahwSymbols, fbSymbols, xaaSymbols, ramdacSymbols,
			 ddcSymbols, i2cSymbols, int10Symbols, vbeSymbols,
			 shadowSymbols, NULL);

      /*
       * The return value must be non-NULL on success even though there
       * is no TearDownProc.
       */
      
      return (pointer) 1;
    }
  else
    {
      if (errmaj)
	{
	  *errmaj = LDR_ONCEONLY;
	}
      
      return (NULL);
    }
}

#endif /* XFree86LOADER */



SMIRegPtr
SMIEntPriv (ScrnInfoPtr pScrn)
{
  DevUnion *pPriv;
  

//  pPriv = xf86GetEntityPrivate (pScrn->entityList[0], gSMIEntityIndex);
  pPriv = xf86GetEntityPrivate (pScrn->entityList[0], entity_index[pScrn->entityList[0]]);

  
  return pPriv->ptr;
}



static Bool
SMI_GetRec (ScrnInfoPtr pScrn)
{
  

  /*
   * Allocate an 'Chip'Rec, and hook it into pScrn->driverPrivate.
   * pScrn->driverPrivate is initialised to NULL, so we can check if
   * the allocation has already been done.
   */
  if (pScrn->driverPrivate == NULL)
    {
      pScrn->driverPrivate = xnfcalloc (sizeof (SMIRec), 1);
#if SMI_DEBUG
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "For screen %d, allocate an 'chip' rec (0x%x)\n", pScrn->scrnIndex, pScrn->driverPrivate);
#endif
    }

  
  return (pScrn->driverPrivate ? TRUE : FALSE);
}

static void
SMI_FreeRec (ScrnInfoPtr pScrn)
{
  

  if (pScrn->driverPrivate != NULL)
    {
      xfree (pScrn->driverPrivate);
      pScrn->driverPrivate = NULL;
    }

  
}

static const OptionInfoRec *
SMI_AvailableOptions (int chipid, int busid)
{
  
  
  return (SMIOptions);
}

static void
SMI_Identify (int flags)
{
  

  xf86PrintChipsets (SILICONMOTION_NAME, "driver (version "
		     SILICONMOTION_VERSION_NAME
		     ") for Silicon Motion Lynx chipsets", SMIChipsets);

  
}

static Bool
SMI_Probe (DriverPtr drv, int flags)
{
  int i;
  GDevPtr *devSections;
  int *usedChips;
  int numDevSections;
  int numUsed;
  Bool foundScreen = FALSE;
  static int dualcount;

  


  numDevSections = xf86MatchDevice (SILICONMOTION_DRIVER_NAME, &devSections);

  if (numDevSections <= 0)
    {
      /* There's no matching device section in the config file, so quit now.
       */
      
      return (FALSE);
    }
#if 0
{
	int		i =0;
	pciVideoPtr	*pci_video = NULL, *pointer = NULL;

	for (i = 0; i<numDevSections; i++) {
		xf86DrvMsg("", X_INFO, "Belcon: numDevSections,I: %d, struct GDevPtr:identifier is %s, vendor is %s, board is %s, chipset is %s, ramdac is %s, driver is %s, claimed is %d, clock chip is %s, busID is %s, active is %d, inUse is %d, videoRam is %d(0x%x), textClockFreq is %d, BiosBase is 0x%x, MemBase is 0x%x, IOBase is 0x%x, chipID is 0x%x, chipRev is 0x%x, options is %p, irq is %d, screen is %d\n", i, devSections[i]->identifier, devSections[i]->vendor, devSections[i]->board, devSections[i]->chipset, devSections[i]->ramdac, devSections[i]->driver, devSections[i]->claimed, devSections[i]->clockchip, devSections[i]->busID, devSections[i]->active, devSections[i]->inUse, devSections[i]->videoRam, devSections[i]->videoRam, devSections[i]->textClockFreq, devSections[i]->BiosBase, devSections[i]->MemBase, devSections[i]->IOBase, devSections[i]->chipID, devSections[i]->chipRev, devSections[i]->options, devSections[i]->irq, devSections[i]->screen);
	}
	pci_video = xf86GetPciVideoInfo();
	xf86DrvMsg("", X_INFO, "Belcon:Test: vendor is 0x%x\n", pci_video[0]->vendor);
	i = 0;
	for (pointer = pci_video; *pointer != NULL; pointer++) {
		xf86DrvMsg("", X_INFO, "Belcon:Test, xf86PciVideoInfo struct:\nVender is 0x%x\nchiptype is 0x%x\nchipRev is 0x%x\nsubsysVendor is 0x%x\nsubsysCard is 0x%x\nbus is 0x%x, device is 0x%x, function is 0x%x\nclass is 0x%x, subclass is 0x%x, interface is 0x%x\nsize is 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\ntype is %s, biosSize is 0x%x\n", (*pointer)->vendor, (*pointer)->chipType, (*pointer)->chipRev, (*pointer)->subsysVendor, (*pointer)->subsysCard, (*pointer)->bus, (*pointer)->device, (*pointer)->func, (*pointer)->class, (*pointer)->subclass, (*pointer)->interface, (*pointer)->size[0], (*pointer)->size[1], (*pointer)->size[2], (*pointer)->size[3], (*pointer)->size[4], (*pointer)->size[5], (*pointer)->type, (*pointer)->biosSize);
	}
}
#endif

#ifndef XSERVER_LIBPCIACCESS
  if (xf86GetPciVideoInfo () == NULL)
    {
      
      return (FALSE);
    }
#endif

  numUsed = xf86MatchPciInstances (SILICONMOTION_NAME, PCI_SMI_VENDOR_ID,
				   SMIChipsets, SMIPciChipsets, devSections,
				   numDevSections, drv, &usedChips);
  xf86DrvMsg("", X_INFO, "numUsed (%d)\n", numUsed);
#if 0
  {
	int i = 0;
	for (i = 0; i < numUsed; i++) {
  xf86DrvMsg ("", X_INFO, "Belcon:usedChips %d is 0x%x\n", i, usedChips[i]);
	}
  }
  xf86DrvMsg ("", X_NOTICE,
	      "numUsed (%d); numDevSections:(%d); dualcount:(%d); \n ",
	      numUsed, numDevSections, dualcount);
#endif
  /* Free it since we don't need that list after this */
  xfree (devSections);

  if (numUsed <= 0)
    {
      
      return (FALSE);
    }

  if (flags & PROBE_DETECT)
    {
      foundScreen = TRUE;
    }
  else
    {
      for (i = 0; i < numUsed; i++)
	{
	  EntityInfoPtr pEnt;
	  ScrnInfoPtr pScrn = NULL;
	  pScrn = NULL;

#if SMI_DEBUG
	xf86DrvMsg("", X_INFO, "Belcon:entityshareable is %d\n", xf86IsEntitySharable(usedChips[i]));
#endif
	  if ((pScrn = xf86ConfigPciEntity (pScrn, 0, usedChips[i],
					    SMIPciChipsets, 0, 0, 0, 0, 0)))
	    {
#if SMI_DEBUG
		xf86DrvMsg("", X_INFO, "Belcon:usedchips[%d] is %d, pScrn->virtualX is %d\n", i, usedChips[i], pScrn->virtualX);
		xf86DrvMsg("", X_INFO, "XF86_VERSION_CURRENT is 0x%x\n", XF86_VERSION_CURRENT);
#endif
                if (pScrn->display != NULL) {
		  xf86DrvMsg("", X_INFO, "Belcon: pScrn->display->modes: frameX0: %d, frameY0: %d\nvirtualX: %d, virtualY: %d\ndepth: %d, fbbpp %d\ndefaultVisual: %d\n", pScrn->display->frameX0, pScrn->display->frameY0, pScrn->display->virtualX, pScrn->display->virtualY, pScrn->display->depth, pScrn->display->fbbpp, pScrn->display->defaultVisual);
                } else {
                  xf86DrvMsg("", X_INFO, "Belcon: null pointer\n");
                }
	      /* Fill in what we can of the ScrnInfoRec */
	      pScrn->driverVersion = SILICONMOTION_DRIVER_VERSION;
	      pScrn->driverName = SILICONMOTION_DRIVER_NAME;
	      pScrn->name = SILICONMOTION_NAME;
	      pScrn->Probe = SMI_Probe;
	      pScrn->PreInit = SMI_PreInit;
	      pScrn->ScreenInit = SMI_ScreenInit;
	      pScrn->SwitchMode = SMI_SwitchMode;
	      pScrn->AdjustFrame = SMI_AdjustFrame;
	      pScrn->EnterVT = SMI_EnterVT;
	      pScrn->LeaveVT = SMI_LeaveVT;
	      pScrn->FreeScreen = SMI_FreeScreen;
	      pScrn->ValidMode = SMI_ValidMode;
	      foundScreen = TRUE;
	    }

	  pEnt = xf86GetEntityInfo (usedChips[i]);
#if SMI_DEBUG
	xf86DrvMsg("", X_INFO, "Belcon:Test, pEnt->index is 0x%x, chipset is 0x%x, active is 0x%x\n", pEnt->index, pEnt->chipset, pEnt->active);
{
	int	belcon = 0;
	for (belcon=0; belcon<pScrn->numEntities; belcon++)
	xf86DrvMsg("", X_INFO, "Belcon:Test, pScrn->entiyList[%d] is 0x%x, pScrn->bitsPerPixel is %d, pScrn->entityInstanceList[%d] is %d\n", belcon, pScrn->entityList[belcon], pScrn->bitsPerPixel, belcon, pScrn->entityInstanceList[belcon]);
}
#endif

		switch(pEnt->chipset) {
			case PCI_CHIP_SMI501:
				xf86DrvMsg("", X_INFO, "501 found\n");
				entity_init_501(pScrn, usedChips[i]);
			break;

			default:
			break;
		}

		xfree (pEnt);
	}

    }


  xfree (usedChips);
  
  return (foundScreen);
}

static Bool
SMI_PreInit (ScrnInfoPtr pScrn, int flags)
{
  EntityInfoPtr pEnt;
  SMIPtr pSmi;
  MessageType from;
  int i, tmpvalue;
  double real;
  ClockRangePtr clockRanges;
  char *s;
  unsigned char config, m, n, shift;
  int mclk;
  vgaHWPtr hwp;
  int vgaCRIndex, vgaIOBase;
  DisplayModePtr p;
  vbeInfoPtr pVbe = NULL;


  unsigned char Generic_EDID[] =
    { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x22, 0x64, 0x91, 0x89,
    0xE4, 0x12, 0x00, 0x00,
    0x16, 0x10, 0x01, 0x03, 0x80, 0x29, 0x1A, 0x78, 0x2A, 0x9B, 0xB6, 0xA4,
    0x53, 0x4B, 0x9D, 0x24,
    0x14, 0x4F, 0x54, 0xBF, 0xEF, 0x00, 0x31, 0x46, 0x61, 0x46, 0x71, 0x4F,
    0x81, 0x40, 0x81, 0x80,
    0x95, 0x00, 0x95, 0x0F, 0x01, 0x01, 0x9A, 0x29, 0xA0, 0xD0, 0x51, 0x84,
    0x22, 0x30, 0x50, 0x98,
    0x36, 0x00, 0x98, 0xFF, 0x10, 0x00, 0x00, 0x1C, 0x00, 0x00, 0x00, 0xFD,
    0x00, 0x31, 0x4B, 0x1E,
    0x50, 0x0E, 0x00, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,
    0x00, 0xFC, 0x00, 0x48,
    0x57, 0x31, 0x39, 0x31, 0x44, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x00, 0x00, 0x00, 0xFF,
    0x00, 0x36, 0x32, 0x32, 0x47, 0x48, 0x33, 0x32, 0x43, 0x41, 0x34, 0x38,
    0x33, 0x36, 0x00, 0x29
  };


  int chipType;

  
#if SMI_DEBUG
  xf86EnableAccess(pScrn);
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "pScrn is %p, screen index is %d, resource type is %d\n", pScrn, pScrn->scrnIndex, pScrn->resourceType);
/*
  xf86DrvMsg("", X_INFO, "PCI: command is 0x%x, line %d\n", pciReadByte(pSmi->PciTag, PCI_CMD_STAT_REG), __LINE__);
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "pScrn->access is %p, pScrn->currentAccess->pIoAccess is %p, pScrn->currentAccess->pMemAccess is %p\n", (EntityAccessPtr)pScrn->access, pScrn->CurrentAccess->pIoAccess, pScrn->CurrentAccess->pMemAccess);
  read_cmd_reg(4);
  if (pScrn->access) {
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line:%d\n", __LINE__);
  }
  if (pScrn->CurrentAccess->pIoAccess != ((EntityAccessPtr) pScrn->access)) {
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line:%d\n", __LINE__);
  }
  if (pScrn->CurrentAccess->pMemAccess != ((EntityAccessPtr) pScrn->access)) {
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line:%d\n", __LINE__);
  }
    xf86DrvMsg(pScrn->scrnIndex, X_INFO, "****************\n");
*/
#endif

#if 0
if (pScrn->confScreen)
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "confScreen is %p\n", pScrn->confScreen);
if (pScrn->confScreen->options)
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "options is %p\n", pScrn->confScreen->options);
if (pScrn->confScreen && pScrn->confScreen->options) {
	char	*ptr = pScrn->confScreen->options;
	while (ptr) {
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Options: %s\n", *ptr);
		ptr++;
	}
}
#endif

  if (pScrn->numEntities > 1)
    {
      
      return (FALSE);
    }

  /* Allocate the SMIRec driverPrivate */
  if (!SMI_GetRec (pScrn))
    {
      
      return (FALSE);
    }

  pSmi = SMIPTR (pScrn);
#ifdef SMI_DEBUG
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "pScrn is %p(0x%x), pSmi is %p(0x%x), flags is 0x%x\n", pScrn, pScrn, pSmi, pSmi, flags);
#endif

  pEnt = xf86GetEntityInfo (pScrn->entityList[0]);

  /* Get the entity, and make sure it is PCI. */
  if (pEnt->location.type != BUS_PCI) {
      
      return (FALSE);
  }
  pSmi->pEnt = pEnt;
// Belcon 
/*
	for (p = pScrn->modes; p != NULL; p = p->next) {
		xf86DrvMsg("", X_INFO, "Belcon Debug: mode name is %s\n", p->name);
	}
*/

  pSmi->PciInfo = xf86GetPciInfoForEntity (pEnt->index);

#ifndef XSERVER_LIBPCIACCESS
 	chipType = pSmi->PciInfo->chipType;
#else
	chipType = PCI_DEV_DEVICE_ID(pSmi->PciInfo);	//caesar modified
#endif

  if (flags & PROBE_DETECT)
    {
        if (SMI_MSOC != chipType) {
          xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Not sm502, chiptype is 0x%x\n", chipType);
          SMI_ProbeDDC (pScrn, xf86GetEntityInfo (pScrn->entityList[0])->index);
          
        }
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SM502\n");
        return (TRUE);
    }

//      xf86DrvMsg("", X_INFO, "Belcon Debug: entityList[0] is %d, %d, chipset is 0x%x, active is %d\n", pScrn->entityList[0], pEnt->index, pEnt->chipset, pEnt->active);

/*
  pSmi->PciInfo = xf86GetPciInfoForEntity (pEnt->index);

  chipType = pSmi->PciInfo->chipType;
*/
/*
	xf86DrvMsg("", X_INFO, "Belcon Debug: chipType is 0x%x\n", chipType);
	xf86DrvMsg("", X_INFO, "Belcon Debug: numEntities is %d\n", pScrn->numEntities);
*/

  /* The vgahw module should be loaded here when needed */
  if (!xf86LoadSubModule (pScrn, "vgahw"))
    {
      
      return (FALSE);
    }

  xf86LoaderReqSymLists (vgahwSymbols, NULL);
  /*
   * Allocate a vgaHWRec
   */
  if (!vgaHWGetHWRec (pScrn))
    {
      
      return (FALSE);
    }


  /* Set pScrn->monitor */
  pScrn->monitor = pScrn->confScreen->monitor;

  /*
   * The first thing we should figure out is the depth, bpp, etc.  Our
   * default depth is 8, so pass it to the helper function.  We support
   * only 24bpp layouts, so indicate that.
   */
  switch (chipType) {
    case SMI_MSOC:
      if (smi_setdepbpp_501(pScrn) == FALSE) {
	
        return FALSE;
      }
    break;

    default:
      if (smi_setdepbpp(pScrn) == FALSE) {
	
        return FALSE;
      }
    break;
  }

/*
  if (SMI_MSOC != chipType)
    {
      if (!xf86SetDepthBpp (pScrn, 0, 0, 0, Support24bppFb))
	{
	  
	  return (FALSE);

	}
      switch (pScrn->depth)
	{
	case 8:
	case 16:
	case 24:
	  break;

	default:
	  xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		      "Given depth (%d) is not supported by this driver\n",
		      pScrn->depth);
	  
	  return (FALSE);
	}
    }
  else
    {
      if (!xf86SetDepthBpp
	  (pScrn, 8, 8, 8,
	   Support32bppFb | SupportConvert24to32 | PreferConvert24to32))
	{
	  
	  return (FALSE);
	}

      switch (pScrn->depth)
	{
	case 8:
	case 16:
	case 24:
	  break;
	case 32:
	  pScrn->depth = 24;
	  break;

	default:
	  xf86DrvMsg (pScrn->scrnIndex, X_ERROR,
		      "Given depth (%d) is not supported by this driver\n",
		      pScrn->depth);
	  
	  return (FALSE);
	}

    }
*/

  xf86PrintDepthBpp (pScrn);

  /*
   * This must happen after pScrn->display has been set because
   * xf86SetWeight references it.
   */
  if ((pScrn->depth > 8))	/*&&(pScrn->depth <= 24)) */
    {
      /* The defaults are OK for us */
      rgb BitsPerComponent = { 0, 0, 0 };
      rgb BitMask = { 0, 0, 0 };

      if (!xf86SetWeight (pScrn, BitsPerComponent, BitMask))
	{
	  
	  return (FALSE);
	}
    }
  if (!xf86SetDefaultVisual (pScrn, -1))
    {
      
      return (FALSE);
    }

  /* We don't currently support DirectColor at > 8bpp */
  if ((pScrn->depth > 8) && (pScrn->defaultVisual != TrueColor))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Given default visual (%s) "
		  "is not supported at depth %d\n",
		  xf86GetVisualName (pScrn->defaultVisual), pScrn->depth);
      
      return (FALSE);
    }

  /* We use a programmable clock */
  pScrn->progClock = TRUE;

  /* Collect all of the relevant option flags (fill in pScrn->options) */
  xf86CollectOptions (pScrn, NULL);


  /* Set the bits per RGB for 8bpp mode */
  if (pScrn->depth == 8)
    {
      if (SMI_MSOC == chipType)
	{
	  pScrn->rgbBits = 8;
	}
      else
	{
	  pScrn->rgbBits = 6;
	}
    }else if(pScrn->depth == 16){
	/* Use 8 bit LUT for gamma correction*/
	pScrn->rgbBits = 8;
	}
  	
  /* Process the options */
  if (!(pSmi->Options = xalloc (sizeof (SMIOptions))))
    return FALSE;
  memcpy (pSmi->Options, SMIOptions, sizeof (SMIOptions));
  xf86ProcessOptions (pScrn->scrnIndex, pScrn->options, pSmi->Options);

  if (xf86ReturnOptValBool (pSmi->Options, OPTION_PCI_BURST, FALSE))
    {
      pSmi->pci_burst = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: pci_burst - PCI burst "
		  "read enabled\n");
    }
  else
    {
      pSmi->pci_burst = FALSE;
    }


     /*For CSC video*/
     if (xf86ReturnOptValBool (pSmi->Options, OPTION_CSCVIDEO, FALSE))
    {
      		pSmi->IsCSCVideo = TRUE;
      		xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option:  csc video "
		  	"read enabled\n");
    }
  else
    {
      		pSmi->IsCSCVideo = FALSE;
    }

  pSmi->NoPCIRetry = TRUE;
  if (xf86ReturnOptValBool (pSmi->Options, OPTION_PCI_RETRY, FALSE))
    {
      if (xf86ReturnOptValBool (pSmi->Options, OPTION_PCI_BURST, FALSE))
	{
	  pSmi->NoPCIRetry = FALSE;
	  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: pci_retry\n");
	}
      else
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "\"pci_retry\" option "
		      "requires \"pci_burst\".\n");
	}
    }

  if (xf86IsOptionSet (pSmi->Options, OPTION_FIFO_CONSERV))
    {
      pSmi->fifo_conservative = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: fifo_conservative "
		  "set\n");
    }
  else
    {
      pSmi->fifo_conservative = FALSE;
    }

  if (xf86IsOptionSet (pSmi->Options, OPTION_FIFO_MODERATE))
    {
      pSmi->fifo_moderate = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: fifo_moderate set\n");
    }
  else
    {
      pSmi->fifo_moderate = FALSE;
    }

  if (xf86IsOptionSet (pSmi->Options, OPTION_FIFO_AGGRESSIVE))
    {
      pSmi->fifo_aggressive = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
		  "Option: fifo_aggressive set\n");
    }
  else
    {
      pSmi->fifo_aggressive = FALSE;
    }

  if (xf86ReturnOptValBool (pSmi->Options, OPTION_NOACCEL, FALSE))
    {
      pSmi->NoAccel = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
		  "Option: NoAccel - Acceleration " "disabled\n");
    }
  else
    {
      pSmi->NoAccel = FALSE;
    }

  if (xf86ReturnOptValBool (pSmi->Options, OPTION_SHOWCACHE, FALSE))
    {
      pSmi->ShowCache = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: show_cache set\n");
    }
  else
    {
      pSmi->ShowCache = FALSE;
    }

  if (xf86GetOptValFreq (pSmi->Options, OPTION_MCLK, OPTUNITS_MHZ, &real))
    {
      pSmi->MCLK = (int) (real * 1000.0);
      if (chipType != SMI_MSOC) {
        if (pSmi->MCLK <= 120000)
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: set_mclk set to "
		      "%1.3f MHz\n", pSmi->MCLK / 1000.0);
	}
        else
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_WARNING, "Memory Clock value of "
		      "%1.3f MHz is larger than limit of 120 MHz\n",
		      pSmi->MCLK / 1000.0);
	  pSmi->MCLK = 0;
	}
      }
    }
  else
    {
      pSmi->MCLK = 0;
    }

  from = X_DEFAULT;
  pSmi->hwcursor = TRUE;
  if (xf86GetOptValBool (pSmi->Options, OPTION_HWCURSOR, &pSmi->hwcursor))
    {
      from = X_CONFIG;
    }
  if (xf86ReturnOptValBool (pSmi->Options, OPTION_SWCURSOR, FALSE))
    {
      pSmi->hwcursor = FALSE;
      from = X_CONFIG;
    }

  if (xf86GetOptValBool (pSmi->Options, OPTION_CLONE_MODE, &pSmi->clone_mode))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Clone mode %s.\n",
		  pSmi->clone_mode ? "enabled" : "disabled");
      if (pSmi->clone_mode){
         pSmi->hwcursor = FALSE; 
	  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "No hardware cursor in clone mode\n");  
         if (pScrn->bitsPerPixel != 16) {
	  xf86DrvMsg(pScrn->scrnIndex, X_ERROR, "CloneMode only supported at "
			   "depth 16\n");
	  
	  return (FALSE);
	  }
      	}
    }

  xf86DrvMsg (pScrn->scrnIndex, from, "Using %s Cursor\n",
	      pSmi->hwcursor ? "Hardware" : "Software");


 /*Add by Teddy for 24bit panel*/
  if (xf86GetOptValBool (pSmi->Options, OPTION_24BITPANEL, &pSmi->pnl24))
	 {
	   xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "24bit panel mode %s.\n",
		   pSmi->pnl24 ? "enabled" : "disabled");
	 }

  

  if (xf86GetOptValBool (pSmi->Options, OPTION_SHADOW_FB, &pSmi->shadowFB))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "ShadowFB %s.\n",
		  pSmi->shadowFB ? "enabled" : "disabled");
    }

#if 0				/* PDR#932 */
  if ((pScrn->depth == 8) || (pScrn->depth == 16))
#endif /* PDR#932 */
    if ((s = xf86GetOptValString (pSmi->Options, OPTION_ROTATE)))
      {
	if (!xf86NameCmp (s, "CCW"))
	  {
	    pSmi->shadowFB = TRUE;
	    pSmi->rotate = SMI_ROTATE_CCW;
	    xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Rotating screen "
			"clockwise\n");
	  }
	else if (!xf86NameCmp (s, "CW"))
	  {
	    pSmi->shadowFB = TRUE;
	    pSmi->rotate = SMI_ROTATE_CW;
	    xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Rotating screen counter "
			"clockwise\n");
	  }
	else if (!xf86NameCmp (s, "UD"))
	  {
	    pSmi->shadowFB = TRUE;
	    pSmi->rotate = SMI_ROTATE_UD;
	    xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Rotating screen half "
			"clockwise\n");
	  }	
  else if(!xf86NameCmp(s, "RandR")) 
    {
#ifdef RANDR
	    pSmi->shadowFB = TRUE;
	    pSmi->RandRRotation = TRUE;
		pSmi->rotate = SMI_ROTATE_ZERO;	
//		pSmi->rotate = SMI_ROTATE_CW;
	    xf86DrvMsg(pScrn->scrnIndex, X_CONFIG,
		    "Using RandR rotation - acceleration disabled\n");
#else
	    xf86DrvMsg(pScrn->scrnIndex, X_WARNING,
		    "This driver was not compiled with support for the Resize and "
		    "Rotate extension.  Cannot honor 'Option \"Rotate\" "
		    "\"RandR\"'.\n");
#endif
    }
	else
	  {
	    xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "\"%s\" is not a valid "
			"value for Option \"Rotate\"\n", s);
	    xf86DrvMsg (pScrn->scrnIndex, X_INFO,
			"Valid options are \"CW\" or " "\"CCW\" or " "\"UD\" or " "\"RandR\"\n");
	  }
      }

  if (pSmi->rotate)
    {
      /* Disable XF86 rotation, it hoses up SMI rotation */
      xf86DisableRandR ();
    }

  if (xf86GetOptValInteger (pSmi->Options, OPTION_VIDEOKEY, &pSmi->videoKey))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: Video key set to "
		  "0x%08X\n", pSmi->videoKey);
    }
  else
    {
      pSmi->videoKey = (1 << pScrn->offset.red) | (1 << pScrn->offset.green)
	| (((pScrn->mask.blue >> pScrn->offset.blue) - 1)
	   << pScrn->offset.blue);
    }

  if (xf86ReturnOptValBool (pSmi->Options, OPTION_BYTESWAP, FALSE))
    {
      pSmi->ByteSwap = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: ByteSwap enabled.\n");
    }
  else
    {
      pSmi->ByteSwap = FALSE;
    }

  /* CZ 26.10.2001: interlaced video */
  if (xf86ReturnOptValBool (pSmi->Options, OPTION_INTERLACED, FALSE))
    {
      pSmi->interlaced = TRUE;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
		  "Option: Interlaced enabled.\n");
    }
  else
    {
      pSmi->interlaced = FALSE;
    }
  /* end CZ */

  if (xf86GetOptValBool (pSmi->Options, OPTION_USEBIOS, &pSmi->useBIOS))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: UseBIOS %s.\n",
		  pSmi->useBIOS ? "enabled" : "disabled");
    }
  else
    {
      /* Default to UseBIOS enabled. */
      pSmi->useBIOS = FALSE;
    }

  if (xf86GetOptValBool (pSmi->Options, OPTION_ZOOMONLCD, &pSmi->zoomOnLCD))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: ZoomOnLCD %s.\n",
		  pSmi->zoomOnLCD ? "enabled" : "disabled");
    }
  else
    {
      /* Default to ZoomOnLCD enabled. */
      pSmi->zoomOnLCD = TRUE;
    }
  if (xf86GetOptValBool (pSmi->Options, OPTION_PANELHEIGHT, &pSmi->lcdHeight))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: LCDHeight %d.\n",
		  pSmi->lcdHeight);
    }
  else
    {
      /* Default to ZoomOnLCD enabled. */
      pSmi->lcdHeight = 0;
    }
  if (xf86GetOptValBool (pSmi->Options, OPTION_PANELWIDTH, &pSmi->lcdWidth))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "Option: LCDWidth %d.\n",
		  pSmi->lcdWidth);
    }
  else
    {
      /* Default to ZoomOnLCD enabled. */
      pSmi->lcdWidth = 0;
    }

  /* Find the PCI slot for this screen */
  pEnt = xf86GetEntityInfo (pScrn->entityList[0]);
  if ((pEnt->location.type != BUS_PCI) || (pEnt->resources))
    {
      xfree (pEnt);
      SMI_FreeRec (pScrn);
      
      return (FALSE);
    }

  if (chipType != SMI_MSOC)	/* IGX -- NO vga on 501 */
    {

      if (xf86LoadSubModule (pScrn, "int10"))
	{
	  xf86LoaderReqSymLists (int10Symbols, NULL);
	  pSmi->pInt10 = xf86InitInt10 (pEnt->index);
	}

      if (pSmi->pInt10 && xf86LoadSubModule (pScrn, "vbe"))
	{
	  xf86LoaderReqSymLists (vbeSymbols, NULL);
	  pVbe = VBEInit (pSmi->pInt10, pEnt->index);
	}
    }

  xf86RegisterResources (pEnt->index, NULL, ResExclusive);
/*	xf86SetOperatingState(resVgaIo, pEnt->index, ResUnusedOpr); */
/*	xf86SetOperatingState(resVgaMem, pEnt->index, ResDisableOpr); */

  /*
   * Set the Chipset and ChipRev, allowing config file entries to
   * override.
   */
  if (pEnt->device->chipset && *pEnt->device->chipset)
    {
      pScrn->chipset = pEnt->device->chipset;
      pSmi->Chipset = xf86StringToToken (SMIChipsets, pScrn->chipset);
      from = X_CONFIG;
    }
  else if (pEnt->device->chipID >= 0)
    {
#ifndef XSERVER_LIBPCIACCESS    
      pSmi->Chipset = pEnt->device->chipID;
#else
	pSmi->Chipset = PCI_DEV_DEVICE_ID(pSmi->PciInfo);	//caesar modified
#endif
      pScrn->chipset =
	(char *) xf86TokenToString (SMIChipsets, pSmi->Chipset);
      from = X_CONFIG;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "ChipID override: 0x%04X\n",
		  pSmi->Chipset);
    }
  else
    {
      from = X_PROBED;
      pSmi->Chipset = pSmi->PciInfo->chipType;
      pScrn->chipset =
	(char *) xf86TokenToString (SMIChipsets, pSmi->Chipset);
    }

  if (pEnt->device->chipRev >= 0)
    {
      pSmi->ChipRev = pEnt->device->chipRev;
      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "ChipRev override: %d\n",
		  pSmi->ChipRev);
    }
  else
    {
#ifndef XSERVER_LIBPCIACCESS    
      pSmi->ChipRev = pSmi->PciInfo->chipRev;
#else
	pSmi->ChipRev = PCI_DEV_REVISION(pSmi->PciInfo);	//caesar modified
#endif
    }
  xfree (pEnt);

  /*
   * This shouldn't happen because such problems should be caught in
   * SMI_Probe(), but check it just in case.
   */
  if (pScrn->chipset == NULL)
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "ChipID 0x%04X is not "
		  "recognised\n", pSmi->Chipset);
      
      return (FALSE);
    }
  if (pSmi->Chipset < 0)
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Chipset \"%s\" is not "
		  "recognised\n", pScrn->chipset);
      
      return (FALSE);
    }

  xf86DrvMsg (pScrn->scrnIndex, from, "Chipset: \"%s\"\n", pScrn->chipset);
  
#ifndef XSERVER_LIBPCIACCESS
  pSmi->PciTag = pciTag (pSmi->PciInfo->bus, pSmi->PciInfo->device,
			 pSmi->PciInfo->func);
  pci_tag = pSmi->PciTag;
  config = pciReadByte(pSmi->PciTag, PCI_CMD_STAT_REG);
  pciWriteByte(pSmi->PciTag, PCI_CMD_STAT_REG, config | PCI_CMD_MEM_ENABLE);
  #endif

  /*boyod */

  pSmi->IsSecondary = FALSE;
  pSmi->IsPrimary = TRUE;
  pSmi->IsLCD = TRUE;
  pSmi->IsCRT = FALSE;

  switch (chipType) {
    case SMI_MSOC:
      smi_set_dualhead_501(pScrn, pSmi);
    break;

    default:
    break;
  }
/*
  if (chipType = SMI_MSOC)
    {

      if (xf86IsEntityShared (pScrn->entityList[0]))
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
		      "xf86IsEntityShared: Yes\n");
	  SMIRegPtr pSMIEnt = SMIEntPriv (pScrn);
	  if (!xf86IsPrimInitDone (pScrn->entityList[0]))
	    {
	      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
			  "xf86IsPrimInitDone: No\n");

	      xf86SetPrimInitDone (pScrn->entityList[0]);
	      pSmi->IsPrimary = TRUE;
	      pSmi->IsSecondary = FALSE;
	      pSMIEnt->pPrimaryScrn = pScrn;
	      pSmi->IsLCD = TRUE;
	      pSmi->IsCRT = FALSE;

	    }
	  else if (pSMIEnt->DualHead)
	    {
#if SMI_DEBUG
	      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG,
			  "xf86IsPrimInitDone: Yes\n");
#endif

	      pSmi->IsSecondary = TRUE;
	      pSMIEnt->pSecondaryScrn = pScrn;
	      pSmi->IsCRT = TRUE;

	    }
	  else
	    {
	      return FALSE;
	    }

	}

    }
*/

  /* And compute the amount of video memory and offscreen memory */
  pSmi->videoRAMKBytes = 0;

// xf86DrvMsg("", X_INFO, "Belcon Debug: pScrn->videoRam is %d\n", pScrn->videoRam);
// xf86DrvMsg("", X_INFO, "Belcon Debug: Chipset is 0x%x\n", pSmi->Chipset);
/* Added by Belcon according to driver of sm712 */
  // Detect amount of installed ram */
 // config = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x71);

  if (!pScrn->videoRam) {
    switch(pSmi->Chipset) {
      case SMI_LYNX3D:
        smi_setvideomem_820(config, pScrn, pSmi);
      break;

      case SMI_LYNX3DM:
        smi_setvideomem_720(config, pScrn, pSmi);
      break;

      case SMI_COUGAR3DR:
        smi_setvideomem_730(config, pScrn, pSmi);
      break;

      case SMI_MSOC:
        smi_setvideomem_501(config, pScrn, pSmi);
      break;

      case SMI_LYNXEM:
      case SMI_LYNXEMplus:
        smi_setvideomem_712(config, pScrn, pSmi);
      break;

      default:
        smi_setvideomem(config, pScrn, pSmi);
      break;
    }
  } else {
    pSmi->videoRAMKBytes = pScrn->videoRam;
    pSmi->videoRAMBytes = pScrn->videoRam * 1024;

    xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "videoram: %dk\n",
		pSmi->videoRAMKBytes);
  }
#if SMI_DEBUG
  xf86DrvMsg (pScrn->scrnIndex, X_PROBED, " videoram: %dkB\n",
		  pSmi->videoRAMKBytes);
#endif

/*
  if (!pScrn->videoRam)
    {
      switch (pSmi->Chipset)
	{
	default:
	  {
	    int mem_table[4] = { 1, 2, 4, 0 };
	    pSmi->videoRAMKBytes = mem_table[(config >> 6)] * 1024;
	    break;
	  }

	case SMI_LYNX3D:
	  {
	    int mem_table[4] = { 0, 2, 4, 6 };
	    pSmi->videoRAMKBytes = mem_table[(config >> 6)] * 1024 + 512;
	    break;
	  }

	case SMI_LYNX3DM:
	  {
	    int mem_table[4] = { 16, 2, 4, 8 };
	    pSmi->videoRAMKBytes = mem_table[(config >> 6)] * 1024;
	    break;
	  }

	case SMI_COUGAR3DR:
	  {
	    // DANGER - Cougar3DR BIOS is broken - hardcode video ram size 
	    pSmi->videoRAMKBytes = 16 * 1024;
	    break;
	  }

	case SMI_MSOC:
	  {
	    // Comment because our demo board can't get right size of videoRAMKBytes by GPIO 
#if 0
	    int mem_table[6] = { 4, 8, 16, 32, 64, 2 };
	    int memval = READ_SCR (pSmi, SCR10);
	    memval =
	      (memval & SCR10_LOCAL_MEM_SIZE) >> SCR10_LOCAL_MEM_SIZE_SHIFT;
	    pSmi->videoRAMKBytes = mem_table[memval] * 1024;
#else
	    pSmi->videoRAMKBytes = 8 * 1024 - FB_RESERVE4USB / 1024;
#endif
	    break;
	  }
	}
      pSmi->videoRAMBytes = pSmi->videoRAMKBytes * 1024;
      pScrn->videoRam = pSmi->videoRAMKBytes;

      if (SMI_MSOC == pSmi->Chipset)
	{
	//  pSmi->FBReserved = pSmi->videoRAMBytes - 4096;
	  pSmi->FBReserved = pSmi->videoRAMBytes - 4096;
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line %d: pSmi->FBReserved is 0x%x\n", __LINE__, pSmi->FBReserved);
	}

      xf86DrvMsg (pScrn->scrnIndex, X_PROBED, " videoram: %dkB\n",
		  pSmi->videoRAMKBytes);
    }
  else
    {
      pSmi->videoRAMKBytes = pScrn->videoRam;
      pSmi->videoRAMBytes = pScrn->videoRam * 1024;

      xf86DrvMsg (pScrn->scrnIndex, X_CONFIG, "videoram: %dk\n",
		  pSmi->videoRAMKBytes);
    }

  if (SMI_MSOC == pSmi->Chipset)
    {
      //      xf86DrvMsg("", X_INFO, "Belcon: %s\n", pSmi->IsSecondary ? "DualHead" : "SingleHead");
      if (xf86IsEntityShared (pScrn->entityList[0]))
	{
          if (pSmi->IsSecondary) {
              pScrn->videoRam = 4 * 1024 - FB_RESERVE4USB / 1024;
            } else {
              pScrn->videoRam = 4 * 1024;
            }
	//  pScrn->videoRam /= 2;
	}
      pSmi->videoRAMKBytes = pScrn->videoRam;
      pSmi->videoRAMBytes = pScrn->videoRam * 1024;
      pSmi->fbMapOffset = pScrn->videoRam * 1024;

      if (!pSmi->IsSecondary)
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_INFO,
		      "Using %dk of videoram for primary head\n",
		      pScrn->videoRam);
	  pSmi->FBOffset = 0;
	  pScrn->fbOffset = pSmi->FBOffset + pSmi->fbMapOffset;
	}
      else
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_INFO,
		      "Using %dk of videoram for secondary head\n",
		      pScrn->videoRam);
	//  pScrn->fbOffset = pScrn->videoRam * 1024;
	  pScrn->fbOffset = 4 * 1024 * 1024;
	  pSmi->FBOffset = pScrn->fbOffset;
	  //      pScrn->memPhysBase += pScrn->fbOffset;
	}
#if SMI_DEBUG
          ErrorF ("preinit():FBOffset is 0x%x\n", pSmi->FBOffset);
#endif
    }
*/
  /*boyod */
  SMI_MapMem (pScrn);

  if (chipType != SMI_MSOC)	/* IGX */
    {
      SMI_DisableVideo (pScrn);

      hwp = VGAHWPTR (pScrn);
      vgaIOBase = hwp->IOBase;
      vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
/*	    vgaCRReg   = vgaIOBase + VGA_CRTC_DATA_OFFSET;  */
      pSmi->PIOBase = hwp->PIOOffset;

      xf86ErrorFVerb (VERBLEV, "\tSMI_PreInit vgaCRIndex=%x, vgaIOBase=%x, "
		      "MMIOBase=%x\n", vgaCRIndex, vgaIOBase, hwp->MMIOBase);

      /* Next go on to detect amount of installed ram */
      //  config = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x71);


#ifdef _XSERVER64

/*  Fake the reading of a valid EDID block to allow the population of the
    default configuration file upon startup. */

      if (xf86LoadSubModule (pScrn, "i2c"))
	{
	  xf86LoaderReqSymLists (i2cSymbols, NULL);
	  SMI_I2CInit (pScrn);	/* IGXMAL DEBUG */
	  xf86ErrorFVerb (VERBLEV, "\tSMI_I2CInit ");
	}


      if (xf86LoadSubModule (pScrn, "ddc"))
	{
	  xf86MonPtr pMon = NULL;

	  xf86LoaderReqSymLists (ddcSymbols, NULL);
	  {

	    unsigned char *EDID_Block = Generic_EDID;
	    pMon = xf86InterpretEDID (pScrn, EDID_Block);

	    if (pMon != NULL)
	      {
		pMon = xf86PrintEDID (pMon);
		if (pMon != NULL)
		  {
		    xf86SetDDCproperties (pScrn, pMon);
		  }
	      }
	  }
	}

#else


      if (xf86LoadSubModule (pScrn, "i2c"))
	{
	  xf86LoaderReqSymLists (i2cSymbols, NULL);
	  SMI_I2CInit (pScrn);
	}

      if (xf86LoadSubModule (pScrn, "ddc"))
	{
	  xf86MonPtr pMon = NULL;

	  xf86LoaderReqSymLists (ddcSymbols, NULL);
#if 1				/* PDR#579 */
	  if (pVbe)
	    {
	      pMon = vbeDoEDID (pVbe, NULL);
	      if (pMon != NULL)
		{
		  if ((pMon->rawData[0] == 0x00)
		      && (pMon->rawData[1] == 0xFF)
		      && (pMon->rawData[2] == 0xFF)
		      && (pMon->rawData[3] == 0xFF)
		      && (pMon->rawData[4] == 0xFF)
		      && (pMon->rawData[5] == 0xFF)
		      && (pMon->rawData[6] == 0xFF)
		      && (pMon->rawData[7] == 0x00))
		    {
		      pMon = xf86PrintEDID (pMon);
		      if (pMon != NULL)
			{
			  xf86SetDDCproperties (pScrn, pMon);
			}
		    }
		}
	    }
#else
	  if ((pVbe)
	      && ((pMon = xf86PrintEDID (vbeDoEDID (pVbe, NULL))) != NULL))
	    {
	      xf86SetDDCproperties (pScrn, pMon);
	    }
#endif
	  else if (!SMI_ddc1 (pScrn->scrnIndex))
	    {
	      if (pSmi->I2C)
		{
		  xf86SetDDCproperties (pScrn,
					xf86PrintEDID (xf86DoEDID_DDC2
						       (pScrn->scrnIndex,
							pSmi->I2C)));
		}
	    }
	}

      vbeFree (pVbe);
      xf86FreeInt10 (pSmi->pInt10);




  pSmi->pInt10 = NULL;

#endif
    }				/* End !SMI_MSOC */

  /*
   * If the driver can do gamma correction, it should call xf86SetGamma()
   * here. (from MGA, no ViRGE gamma support yet, but needed for
   * xf86HandleColormaps support.)
   */
  {
    Gamma zeros = { 0.0, 0.0, 0.0 };

    if (!xf86SetGamma (pScrn, zeros))
      {
	
	SMI_EnableVideo (pScrn);
	SMI_UnmapMem (pScrn);
	return (FALSE);
      }
  }


  /* Lynx built-in ramdac speeds */
  pScrn->numClocks = 4;

  if ((pScrn->clock[3] <= 0) && (pScrn->clock[2] > 0))
    {
      pScrn->clock[3] = pScrn->clock[2];
    }

  switch (pSmi->Chipset) {
    case SMI_LYNX3DM:
    case SMI_COUGAR3DR:
    case SMI_MSOC:
      smi_setclk(pScrn, 200000, 200000, 200000, 200000);
    break;

    default:
      smi_setclk(pScrn, 135000, 135000, 135000, 135000);
    break;
  }
/*
  if ((pSmi->Chipset == SMI_LYNX3DM) ||
      (pSmi->Chipset == SMI_COUGAR3DR) || (pSmi->Chipset == SMI_MSOC))
    {
      if (pScrn->clock[0] <= 0)
	pScrn->clock[0] = 200000;
      if (pScrn->clock[1] <= 0)
	pScrn->clock[1] = 200000;
      if (pScrn->clock[2] <= 0)
	pScrn->clock[2] = 200000;
      if (pScrn->clock[3] <= 0)
	pScrn->clock[3] = 200000;
    }
  else
    {
      if (pScrn->clock[0] <= 0)
	pScrn->clock[0] = 135000;
      if (pScrn->clock[1] <= 0)
	pScrn->clock[1] = 135000;
      if (pScrn->clock[2] <= 0)
	pScrn->clock[2] = 135000;
      if (pScrn->clock[3] <= 0)
	pScrn->clock[3] = 135000;
    }
*/

  /* Now set RAMDAC limits */
  switch (pSmi->Chipset)
    {
    default:
      pSmi->minClock = 12000;
/*			pSmi->maxClock = 270000;*/
      pSmi->maxClock = 270000;

      break;
    }
  xf86ErrorFVerb (VERBLEV, "\tSMI_PreInit minClock=%d, maxClock=%d\n",
		  pSmi->minClock, pSmi->maxClock);

  if (SMI_MSOC != pSmi->Chipset)
    {

      /* Detect current MCLK and print it for user */
      m = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6A);
      n = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6B);
      switch (n >> 6)
	{
	default:
	  shift = 1;
	  break;

	case 1:
	  shift = 4;
	  break;

	case 2:
	  shift = 2;
	  break;
	}
      n &= 0x3F;
      mclk = ((1431818 * m) / n / shift + 50) / 100;
      xf86DrvMsg (pScrn->scrnIndex, X_PROBED,
		  "Detected current MCLK value of " "%1.3f MHz\n",
		  mclk / 1000.0);
    }

  SMI_EnableVideo (pScrn);
  SMI_UnmapMem (pScrn);
xf86DrvMsg("", X_INFO, "pScrn->virtualX is %d, pScrn->display->virtualX is %d\n", pScrn->virtualX, pScrn->display->virtualX);
  pScrn->virtualX = pScrn->display->virtualX;

  /*
   * Setup the ClockRanges, which describe what clock ranges are available,
   * and what sort of modes they can be used for.
   */
  clockRanges = xnfcalloc (sizeof (ClockRange), 1);
  clockRanges->next = NULL;

  clockRanges->minClock = pSmi->minClock;
  clockRanges->maxClock = pSmi->maxClock;
  clockRanges->clockIndex = -1;
  clockRanges->interlaceAllowed = FALSE;
  clockRanges->doubleScanAllowed = FALSE;


  /*boyod */
/*             tmpvalue = VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31) & ~0xCD;
               VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmpvalue );
*/
#if 0
  xf86DrvMsg ("", X_INFO,
	      "Belcon: before xf86ValidateModes, Monitor->Modes->HDisplay is %d\n",
	      pScrn->monitor->Modes->HDisplay);
  xf86DrvMsg ("", X_INFO,
	      "Belcon: before xf86ValidateModes,minClock is %d(kHz), maxClock is %d(kHz)\n",
	      clockRanges->minClock, clockRanges->maxClock);
// Belcon Debug
	for (p = pScrn->monitor->Modes; p != NULL; p = p->next) {
		xf86DrvMsg("", X_INFO, "Belcon Debug: Monitor mode name is %s\n", p->name);
	}
	for (p = pScrn->display->modes; p != NULL; p = p->next) {
		xf86DrvMsg("", X_INFO, "Belcon Debug: Request mode name is %s\n", p->name);
	}
#endif

  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "BDEBUG pScrn->display->virtualX is %d, pScrn->virtualX is %d\n", pScrn->display->virtualX, pScrn->virtualX);
  i = xf86ValidateModes (pScrn,	/* Screen pointer                                         */
//  i = smi_xf86ValidateModes (pScrn,	/* Screen pointer                                         */
			 pScrn->monitor->Modes,	/* Available monitor modes                        */
			 pScrn->display->modes,	/* req mode names for screen              */
			 clockRanges,	/* list of clock ranges allowed           */
			 NULL,	/* use min/max below                              */
			 128,	/* min line pitch (width)                         */
			 4096,	/* maximum line pitch (width)             */
			 128,	/* bits of granularity for line pitch */
			 /* (width) above                                          */
			 128,	/* min virtual height                             */
			 4096,	/* max virtual height                             */
			 pScrn->display->virtualX,	/* force virtual x                                        */
			 pScrn->display->virtualY,	/* force virtual Y                                        */
			 pSmi->videoRAMBytes,	/* size of aperture used to access        */
			 /* video memory                                           */
			 LOOKUP_BEST_REFRESH);	/* how to pick modes                              */
xf86DrvMsg("", X_INFO, "displayWidth is %d\n", pScrn->displayWidth);
  pScrn->displayWidth = (pScrn->virtualX + 15) & ~15;
xf86DrvMsg("", X_INFO, "displayWidth is %d\n", pScrn->displayWidth);
#if 0
if (pSmi->Chipset == SMI_MSOC)
{
	SMIRegPtr	pSMIEnt = SMIEntPriv (pScrn);
	if (pSMIEnt->DualHead == TRUE) {
		pScrn->videoRam = (pScrn->displayWidth * pScrn->virtualY * pScrn->bitsPerPixel) / (1024 * 8);
	} else {
		pScrn->videoRam = free_video_memory;
	}
/*
} else {
	pScrn->videoRam = free_video_memory;
}
*/
//	pScrn->videoRam = (pScrn->virtualX * pScrn->virtualY * pScrn->bitsPerPixel) / (1024 * 8) + ( pSmi->IsPrimary ? 170 : 0);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BDEBUG: PreInit, free_video_memory is %d\n", free_video_memory);
	free_video_memory -= (pScrn->videoRam + 4);
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BDEBUG: PreInit, free_video_memory is %d\n", free_video_memory);
	pSmi->videoRAMKBytes = (pScrn->videoRam + 4);
	pSmi->videoRAMBytes = pSmi->videoRAMKBytes * 1024;
	pSmi->fbMapOffset = (pScrn->videoRam + 4) * 1024;
	pSmi->FBReserved = pSmi->videoRAMBytes - 4096;
	pSmi->FBCursorOffset = pSmi->videoRAMBytes - 2048;
	if (!pSmi->IsSecondary) {
		pSmi->FBOffset = 0;
	//	pScrn->fbOffset = pSmi->FBOffset + pSmi->fbMapOffset;
		pScrn->fbOffset = pSmi->FBOffset;
		pSmi->fbMapOffset = 0;
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "primary pSmi is %p, pScrn is %p\n", pSmi, pScrn);
	} else {
		ScrnInfoPtr	primaryScrn = pSMIEnt->pPrimaryScrn;
		SMIRegPtr	primarySMIEnt = SMIEntPriv(primaryScrn);
		SMIPtr		primarySmi = SMIPTR(primaryScrn);

		pSmi->FBOffset += free_video_memory * 1024;
		pScrn->fbOffset = pSmi->FBOffset;
		pSmi->fbMapOffset = pSmi->FBOffset;


xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  primary pSmi is %p, pScrn is %p\n", primarySmi, primaryScrn);
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "  primary pScrn->fbOffset is 0x%x, secondary fbOffset is 0x%x\n", primaryScrn->fbOffset, pScrn->fbOffset);
		primaryScrn->videoRam = (pScrn->fbOffset - primaryScrn->fbOffset) / 1024 - 4;
		xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Re-count video memroy size for primary head. %dK\n", primaryScrn->videoRam);
		primarySmi->videoRAMKBytes = primaryScrn->videoRam + 4;
		primarySmi->videoRAMBytes = primaryScrn->videoRam * 1024;
		primarySmi->FBReserved = primarySmi->videoRAMBytes - 4096;
		primarySmi->FBCursorOffset = pSmi->videoRAMBytes - 2048;

	}
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "LINE: %d, pScrn->fbOffset is 0x%x\n", __LINE__, pScrn->fbOffset);
}
#endif
  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "BDEBUG pScrn->display->virtualX is %d, pScrn->virtualX is %d, pScrn->virtualY is %d, pScrn->bitsPerPixel is %d, pScrn->depth is %d\n", pScrn->display->virtualX, pScrn->virtualX, pScrn->virtualY, pScrn->bitsPerPixel, pScrn->depth);
  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BDEBUG pScrn->videoRam is %d\n", pScrn->videoRam);
#if 0
// Belcon Debug
	for (p = pScrn->modes; p != NULL; p = p->next) {
		xf86DrvMsg("", X_INFO, "Belcon Debug: mode name is %s\n", p->name);
		xf86DrvMsg("", X_INFO, "Belcon Debug: status is %s\n", xf86ModeStatusToString(p->status));
	}
#endif

  if (i == -1)
    {
      SMI_FreeRec (pScrn);
      
      return (FALSE);
    }

  /* Prune the modes marked as invalid */
  xf86PruneDriverModes (pScrn);
/*
// Belcon Debug
	for (p = pScrn->modes; p != NULL; p = p->next) {
		xf86DrvMsg("", X_INFO, "Belcon Debug: after xf86PruneDriverModes\n");
		xf86DrvMsg("", X_INFO, "Belcon Debug: mode name is %s\n", p->name);
		xf86DrvMsg("", X_INFO, "Belcon Debug: status is %s\n", xf86ModeStatusToString(p->status));
	}
*/

  if ((i == 0) || (pScrn->modes == NULL))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "No valid modes found\n");
      SMI_FreeRec (pScrn);
      
      return (FALSE);
    }

  /* Don't need this for 501 */
  if (SMI_MSOC != pSmi->Chipset)
    {
      xf86SetCrtcForModes (pScrn, 0);
    }

  /* Set the current mode to the first in the list */
  pScrn->currentMode = pScrn->modes;

  /* Print the list of modes being used */
  xf86PrintModes (pScrn);

  /* Set display resolution */
  xf86SetDpi (pScrn, 0, 0);


  if ((xf86LoadSubModule (pScrn, "fb") == NULL))
    {
      SMI_FreeRec (pScrn);
      
      return (FALSE);
    }

  xf86LoaderReqSymLists (fbSymbols, NULL);

  /* Load XAA if needed */
  if (!pSmi->NoAccel || pSmi->hwcursor)
    {
      if (!xf86LoadSubModule (pScrn, "xaa"))
	{
	  SMI_FreeRec (pScrn);
	  
	  return (FALSE);
	}
      xf86LoaderReqSymLists (xaaSymbols, NULL);
    }

  /* Load ramdac if needed */
  if (pSmi->hwcursor)
    {
      if (!xf86LoadSubModule (pScrn, "ramdac"))
	{
	  SMI_FreeRec (pScrn);
	  
	  return (FALSE);
	}
      xf86LoaderReqSymLists (ramdacSymbols, NULL);
    }

  if (pSmi->shadowFB)
    {
      if (!xf86LoadSubModule (pScrn, "shadowfb"))
	{
	  SMI_FreeRec (pScrn);
	  
	  return (FALSE);
	}
      xf86LoaderReqSymLists (shadowSymbols, NULL);
    }

  
#ifndef XSERVER_LIBPCIACCESS
  xf86DrvMsg("", X_INFO, "PCI: command is 0x%x, line %d\n", pciReadByte(pSmi->PciTag, PCI_CMD_STAT_REG), __LINE__);
  read_cmd_reg(4);
  #endif
  
  return (TRUE);
}

/*
 * This is called when VT switching back to the X server.  Its job is to
 * reinitialise the video mode. We may wish to unmap video/MMIO memory too.
 */

static Bool
SMI_EnterVT (int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  SMIPtr pSmi = SMIPTR (pScrn);
  Bool ret;

  

  /* Enable MMIO and map memory */
/*
	SMI_MapMem(pScrn);
	SMI_Save(pScrn);
*/

  if (!SMI_MapMem (pScrn))
    {
      
      return (FALSE);
    }

  //SMI_Save (pScrn);
  SMI_RestoreReg(pScrn);

   /* FBBase may have changed after remapping the memory */
   pScrn->pScreen->ModifyPixmapHeader(pScrn->pScreen->GetScreenPixmap(pScrn->pScreen), -1,-1,-1,-1,-1, pSmi->FBBase + pSmi->FBOffset);
   pScrn->pixmapPrivate.ptr=pSmi->FBBase + pSmi->FBOffset;
		
  /* #670 */
  if (pSmi->shadowFB)
    {
      pSmi->FBOffset = pSmi->savedFBOffset;
      pSmi->FBReserved = pSmi->savedFBReserved;
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line %d: pSmi->FBReserved is 0x%x\n", __LINE__, pSmi->FBReserved);
    }

  ret = SMI_ModeInit (pScrn, pScrn->currentMode);

  /* #670 */
  if (ret && pSmi->shadowFB)
    {
      BoxRec box;

      /* #920 */
      if (pSmi->paletteBuffer)
	{
	  int i;

	  VGAOUT8 (pSmi, VGA_DAC_WRITE_ADDR, 0);
	  for (i = 0; i < 256 * 3; i++)
	    {
	      VGAOUT8 (pSmi, VGA_DAC_DATA, pSmi->paletteBuffer[i]);
	    }
	  xfree (pSmi->paletteBuffer);
	  pSmi->paletteBuffer = NULL;
	}

      if (pSmi->pSaveBuffer)
	{
	  memcpy (pSmi->FBBase, pSmi->pSaveBuffer, pSmi->saveBufferSize);
	  xfree (pSmi->pSaveBuffer);
	  pSmi->pSaveBuffer = NULL;
	}

      box.x1 = 0;
      box.y1 = 0;
      box.x2 = pScrn->virtualY;
      box.y2 = pScrn->virtualX;

      if (pSmi->Chipset == SMI_COUGAR3DR)
	{
	  SMI_RefreshArea730 (pScrn, 1, &box);
	}
      else
	{
	  SMI_RefreshArea (pScrn, 1, &box);
	}

    }

  /* Reset the grapics engine */
  if (!pSmi->NoAccel)
    SMI_EngineReset (pScrn);

  
  return (ret);
}

/*
 * This is called when VT switching away from the X server.  Its job is to
 * restore the previous (text) mode. We may wish to remap video/MMIO memory
 * too.
 */

static void
SMI_LeaveVT (int scrnIndex, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  SMIRegPtr SMISavePtr = &pSmi->SavedReg;

  

  SMI_AccelSync(pScrn);

  SMI_SaveReg(pScrn );
  /* #670 */
  if (pSmi->shadowFB)
    {
      pSmi->pSaveBuffer = xnfalloc (pSmi->saveBufferSize);
      if (pSmi->pSaveBuffer)
	{
	  memcpy (pSmi->pSaveBuffer, pSmi->FBBase, pSmi->saveBufferSize);
	}

      pSmi->savedFBOffset = pSmi->FBOffset;
      pSmi->savedFBReserved = pSmi->FBReserved;

      /* #920 */
#if 0
      if (pSmi->Bpp == 1)
	{
	  pSmi->paletteBuffer = xnfalloc (256 * 3);
	  if (pSmi->paletteBuffer)
	    {
	      int i;

	      VGAOUT8 (pSmi, VGA_DAC_READ_ADDR, 0);
	      for (i = 0; i < 256 * 3; i++)
		{
		  pSmi->paletteBuffer[i] = VGAIN8 (pSmi, VGA_DAC_DATA);
		}
	    }
	}
#endif
    }

  memset (pSmi->FBBase, 0, 256 * 1024);	/* #689 */
#if  XORG_VERSION_CURRENT <=  XORG_VERSION_NUMERIC(7,1,1,0,0)
	//caesar removed for X server 1.5.3
 	//SMI_WriteMode (pScrn, vgaSavePtr, SMISavePtr);
#endif
  /*
   * Belcon: Fix #001
   * 	Restore hardware cursor register
   */
  if (pSmi->dcrF0)
    WRITE_DCR(pSmi, DCRF0, pSmi->dcrF0);
  if (pSmi->dcr230)
    WRITE_DCR(pSmi, DCR230, pSmi->dcr230);
  /* #001 ended */

  /* Teddy: close clone mode */
  if (pSmi->clone_mode) {
	 CARD8 tmp;
	 tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
	 VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp&0x0f);	
  }
  
  SMI_UnmapMem (pScrn);

  
}

/*
 * This function performs the inverse of the restore function: It saves all the
 * standard and extended registers that we are going to modify to set up a video
 * mode.
 */

static void
SMI_Save (ScrnInfoPtr pScrn)
{
  int i;
  CARD32 offset;

  vgaHWPtr hwp = VGAHWPTR (pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  SMIPtr pSmi = SMIPTR (pScrn);
  SMIRegPtr save = &pSmi->SavedReg;

  int vgaIOBase = hwp->IOBase;
  int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
  int vgaCRData = vgaIOBase + VGA_CRTC_DATA_OFFSET;

  

  xf86DrvMsg(pScrn->scrnIndex, X_INFO, "Belcon:DEBUG:EntityList %d\n", pScrn->entityList[0]);

  if (SMI_MSOC != pSmi->Chipset)
    {

      /* Save the standard VGA registers */
      vgaHWSave (pScrn, vgaSavePtr, VGA_SR_ALL);
      save->smiDACMask = VGAIN8 (pSmi, VGA_DAC_MASK);
      VGAOUT8 (pSmi, VGA_DAC_READ_ADDR, 0);
      for (i = 0; i < 256; i++)
	{
	  save->smiDacRegs[i][0] = VGAIN8 (pSmi, VGA_DAC_DATA);
	  save->smiDacRegs[i][1] = VGAIN8 (pSmi, VGA_DAC_DATA);
	  save->smiDacRegs[i][2] = VGAIN8 (pSmi, VGA_DAC_DATA);
	}
      for (i = 0, offset = 2; i < 8192; i++, offset += 8)
	{
	  save->smiFont[i] = *(pSmi->FBBase + offset);
	}

      /* Now we save all the extended registers we need. */
      save->SR17 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x17);
/* Debug */
      if (-1 == saved_console_reg) {
        save->SR18 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18);
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_Save(), Set save->SR18 value: 0x%x\n", save->SR18);
        saved_console_reg = 0x0;
      } else {
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_Save(), assign (pSmi->SavedReg).SR18 to save->SR18 is 0x%x\n", (pSmi->SavedReg).SR18);
        save->SR18 = (pSmi->SavedReg).SR18;
      }
//      save->SR18 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18);
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_Save(), save->SR18 is 0x%x\n", save->SR18);
      save->SR21 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
      save->SR31 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
      save->SR32 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x32);
      save->SR6A = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6A);
      save->SR6B = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6B);
      /*Add for 64bit */
      save->SR6C = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6C);
      save->SR6D = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6D);

      /* Belcon: clone mode */
      if (pSmi->clone_mode) {
        save->SR6E = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6E);
        save->SR6F = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6F);
      }

      save->SR81 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81);
      save->SRA0 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0xA0);


      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "SEQ Save\n");
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "17.%X\n", save->SR17);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "18.%X\n", save->SR18);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "21.%X\n", save->SR21);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "31.%X\n", save->SR31);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "32.%X\n", save->SR32);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "6A.%X\n", save->SR6A);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "6B.%X\n", save->SR6B);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "6C.%X\n", save->SR6C);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "6D.%X\n", save->SR6D);

      /* Belcon: clone mode */
      if (pSmi->clone_mode) {
        xf86DrvMsg (pScrn->scrnIndex, X_INFO, "6E.%X\n", save->SR6E);
        xf86DrvMsg (pScrn->scrnIndex, X_INFO, "6F.%X\n", save->SR6F);
      }

      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "81.%X\n", save->SR81);
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "A0.%X\n", save->SRA0);

      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "59.%X\n",
		  VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x59));


      if (SMI_LYNXM_SERIES (pSmi->Chipset))
	{
	  /* Save primary registers */
	  save->CR90[14] = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E);
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E,
			 save->CR90[14] & ~0x20);

	  for (i = 0; i < 16; i++)
	    {
	      save->CR90[i] =
		VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x90 + i);
	    }
	  save->CR30 = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x30);
 xf86DrvMsg("", X_INFO, "Belcon:test:line: %d cr30 is 0x%x\n", __LINE__, save->CR30);
	  save->CR33 = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x33);
	  save->CR3A = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x3A);

          /* Belcon : clone mode */
          if (pSmi->clone_mode) {
            for ( i = 0; i < 8; i++) {
              save->FPR50[i] =
                VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x50 + i);
            }
            save->FPR50[0x0A] =
              VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x5A);
          }

	  for (i = 0; i < 14; i++)
	    {
	      // save->CR40_2[i] =
	      save->CR40[i] =
		VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x40 + i);
// xf86DrvMsg("", X_INFO, "Belcon:haha, saveing 1st CRT%d: 0x%x\n", 0x40 + i, save->CR40[i]);
	    }

	  /* Save secondary registers */
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E,
			 save->CR90[14] | 0x20);
	  save->CR33_2 = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x33);
	  for (i = 0; i < 14; i++)
	    {
	      save->CR40_2[i] = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData,
					      0x40 + i);
xf86DrvMsg("", X_INFO, "Belcon:haha, saveing 2nd CRT%d: 0x%x\n", 0x40 + i, save->CR40_2[i]);
	    }
	  save->CR9F_2 = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9F);

	  /* Save common registers */
	  for (i = 0; i < 14; i++)
	    {
	      save->CRA0[i] =
		VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0xA0 + i);
	    }

	  /* PDR#1069 */
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E, save->CR90[14]);
	}
      else
	{
	  save->CR30 = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x30);
 xf86DrvMsg("", X_INFO, "Belcon:test:line: %d cr30 is 0x%x\n", __LINE__, save->CR30);
	  save->CR33 = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x33);
	  save->CR3A = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x3A);
	  for (i = 0; i < 14; i++)
	    {
	      save->CR40[i] =
		VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x40 + i);
	    }
	}

      /* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
      if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR))
	{
	  save->CCR66 =
	    VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66);
	}
      /* end CZ */
    }


  save->DPR10 = READ_DPR (pSmi, 0x10);
  save->DPR1C = READ_DPR (pSmi, 0x1C);
  save->DPR20 = READ_DPR (pSmi, 0x20);
  save->DPR24 = READ_DPR (pSmi, 0x24);
  save->DPR28 = READ_DPR (pSmi, 0x28);
  save->DPR2C = READ_DPR (pSmi, 0x2C);
  save->DPR30 = READ_DPR (pSmi, 0x30);
  save->DPR3C = READ_DPR (pSmi, 0x3C);
  save->DPR40 = READ_DPR (pSmi, 0x40);
  save->DPR44 = READ_DPR (pSmi, 0x44);

  save->VPR00 = READ_VPR (pSmi, 0x00);
  save->VPR0C = READ_VPR (pSmi, 0x0C);
  save->VPR10 = READ_VPR (pSmi, 0x10);

  if (pSmi->Chipset == SMI_COUGAR3DR)
    {
      save->FPR00_ = READ_FPR (pSmi, FPR00);
      save->FPR0C_ = READ_FPR (pSmi, FPR0C);
      save->FPR10_ = READ_FPR (pSmi, FPR10);
    }

  save->CPR00 = READ_CPR (pSmi, 0x00);

  if (SMI_MSOC != pSmi->Chipset)
    {
      if (!pSmi->ModeStructInit)
	{
	  /* XXX Should check the return value of vgaHWCopyReg() */
	  vgaHWCopyReg (&hwp->ModeReg, vgaSavePtr);
	  memcpy (&pSmi->ModeReg, save, sizeof (SMIRegRec));
	  pSmi->ModeStructInit = TRUE;
	}
    }

  if (xf86GetVerbosity () > 1)
    {
      xf86DrvMsgVerb (pScrn->scrnIndex, X_INFO, VERBLEV,
		      "Saved current video mode.  Register dump:\n");
      //SMI_PrintRegs (pScrn);
    }

  
}

int GetFreshRate(DisplayModePtr mode)
{
	char *pString = mode->name;
	float Refresh;

	while(((*pString) != '\0'))
		{
			if(*pString == '@')
				break;
			
                      pString++;
		}
	

	if((*pString) == '\0')
	{
		Refresh = mode->VRefresh;
		if(Refresh > 85 * (1.0 - SYNC_TOLERANCE)) return 85;
		if(Refresh > 75 * (1.0 - SYNC_TOLERANCE)) return 75;
		if(Refresh > 60 * (1.0 - SYNC_TOLERANCE)) return 60;

		return 60;
		
	}
	else
	{
		
		pString++;
		if((*pString == '6')&&(*(pString+1) == '0')) return 60;
		if((*pString == '7')&&(*(pString+1) == '5')) return 75;
		if((*pString == '8')&&(*(pString+1) == '5')) return 85;

		return 60;
	}

}




static void
SMI_BiosWriteMode (ScrnInfoPtr pScrn, DisplayModePtr mode, SMIRegPtr restore)
{
  	int i;
  	CARD8 tmp;
  	EntityInfoPtr pEnt;

  	vgaHWPtr hwp  = VGAHWPTR (pScrn);
  	SMIPtr pSmi     = SMIPTR (pScrn);
  	int vgaIOBase  = hwp->IOBase;
  	int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
  	int vgaCRData   = vgaIOBase + VGA_CRTC_DATA_OFFSET;
	
     //if not use bios call, return
     if(!pSmi->useBIOS) return;	

     int modeindex = 0; 
     int VsyncBits = 0x0;        //00 -- 43Hz
                                           //01 -- 60Hz 
				               //10 -- 75Hz
				               //11 -- 85Hz
  /* Find the INT 10 mode number */
    	static struct
    	{
      		int x, y, bpp, freshrate;
      		CARD16 mode;
    	} 
    modeTable[] =
    {
      {640, 480, 8, 60,0x50},
      {640, 480, 16, 60,0x52},
      {640, 480, 24, 60,0x53},
      {640, 480, 32, 60,0x54},
      {640, 480, 8, 75,0x50},
      {640, 480, 16, 75,0x52},
      {640, 480, 24, 75,0x53},
      {640, 480, 32, 75,0x54},
      {640, 480, 8, 85,0x50},
      {640, 480, 16, 85,0x52},
      {640, 480, 24, 85,0x53},
      {640, 480, 32, 85,0x54},

      {800, 480, 8, 60,0x4A},
      {800, 480, 16, 60,0x4C},
      {800, 480, 24, 60,0x4D},
      {800, 480, 8, 75,0x4A},
      {800, 480, 16, 75,0x4C},
      {800, 480, 24, 75,0x4D},
      {800, 480, 8, 85,0x4A},
      {800, 480, 16, 85,0x4C},
      {800, 480, 24, 85,0x4D},

	  
      {800, 600, 8, 60,0x55},
      {800, 600, 16,60, 0x57},
      {800, 600, 24, 60,0x58},
      {800, 600, 32, 60,0x59},
      {800, 600, 8, 75,0x55},
      {800, 600, 16,75, 0x57},
      {800, 600, 24, 75,0x58},
      {800, 600, 32, 75,0x59},
      {800, 600, 8, 85,0x55},
      {800, 600, 16,85, 0x57},
      {800, 600, 24, 85,0x58},
      {800, 600, 32, 85,0x59},
 

      {1024, 768, 8, 60,0x60},
      {1024, 768, 16, 60,0x62},
      {1024, 768, 24, 60,0x63},
      {1024, 768, 32, 60,0x64},
      {1024, 768, 8, 75,0x60},
      {1024, 768, 16, 75,0x62},
      {1024, 768, 24, 75,0x63},
      {1024, 768, 32, 75,0x64},
      {1024, 768, 8, 85,0x60},
      {1024, 768, 16, 85,0x62},
      {1024, 768, 24, 85,0x63},
      {1024, 768, 32, 85,0x64},

	  
      {1280, 1024, 8, 60,0x65},
      {1280, 1024, 16, 60,0x67},
      {1280, 1024, 24, 60,0x68},
      {1280, 1024, 32, 60,0x69},
      {1280, 1024, 8, 75,0x65},
      {1280, 1024, 16, 75,0x67},
      {1280, 1024, 24, 75,0x68},
      {1280, 1024, 32, 75,0x69},
     };

  
  //check mode width, height, freshrate
   for (i = 0; i < sizeof (modeTable) / sizeof (modeTable[0]); i++)
      {
	if ((modeTable[i].x == mode->HDisplay)
	    && (modeTable[i].y == mode->VDisplay)
	    && (modeTable[i].bpp == pScrn->bitsPerPixel)
	    &&(modeTable[i].freshrate == GetFreshRate(mode)))
	  {
	    //new->mode = modeTable[i].mode;
	    modeindex = i;
	    //xf86DrvMsg("", X_INFO, "Mill:  H -- %d, V -- %d, BPP -- %d\n", mode->HDisplay, mode->VDisplay, pScrn->bitsPerPixel );
		break;
	  }
      }
   
  xf86DrvMsg (pScrn->scrnIndex, X_INFO, " Mill: modeindex -- %d\n", modeindex);

  vgaHWProtect (pScrn, TRUE);

  /* Wait for engine to become idle */
  WaitIdle ();

  //xf86DrvMsg (pScrn->scrnIndex, X_INFO, " Mill: pSmi: 0x%x, useBIOS %d, pinit10 %d, mode : %d\n",
	//pSmi, pSmi->useBIOS,	pSmi->pInt10,  restore->mode);

  //if no use int10 function, load it    - = - mill.chen
  if(pSmi->pInt10 == NULL)
  {
	pEnt = xf86GetEntityInfo (pScrn->entityList[0]);
	pSmi->pInt10 = xf86InitInt10 (pEnt->index);
	
	//xf86DrvMsg (pScrn->scrnIndex, X_INFO, " Mill: pSmi: 0x%x, useBIOS %d, pinit10 0x%x, mode : %d\n",
	    //          pSmi, pSmi->useBIOS,	pSmi->pInt10,  restore->mode);
  }

  //begin to use bios call
  if (pSmi->pInt10 != NULL) 
  {
      pSmi->pInt10->num = 0x10;
      pSmi->pInt10->ax = modeTable[modeindex].mode | 0x80;
      ///////////////////////////mill added
     xf86DrvMsg (pScrn->scrnIndex, X_INFO, "Mill: Setting mode 0x%02X\n",
	       modeTable[modeindex].mode );

      switch(modeTable[modeindex].freshrate)
      	{
      		case 60: 	VsyncBits = 0x1; break;
		case 75:		VsyncBits = 0x2; break;
		case 85:		VsyncBits = 0x3; break;
		default:		VsyncBits = 0x0; 		
      	}

	xf86DrvMsg (pScrn->scrnIndex, X_INFO, "Mill: modeTable[i].freshrate 0x%02X\n",
	       modeTable[i].freshrate );
	  
      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x71);   // only use in our bios -- mill added
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x71, tmp & 0xFC | VsyncBits);

      SMI_DisableMmio (pScrn);
      xf86ExecX86int10 (pSmi->pInt10);

      /* Enable linear mode. */
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
      tmp = inb (pSmi->PIOBase + VGA_SEQ_DATA);
      outb (pSmi->PIOBase + VGA_SEQ_DATA, tmp | 0x01);

      /* Enable DPR/VPR registers. */
      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03);


	 //////////////////////////////////mill added
      xf86FreeInt10 (pSmi->pInt10);
      pSmi->pInt10 = NULL;
	}

   //old code
   /* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
  if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR))
    {
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66, restore->CCR66);
    }
  /* end CZ */

  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6C, restore->SR6C);
  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6D, restore->SR6D);

  /* Belcon: clone mode */
  if (pSmi->clone_mode) {
    VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6E, restore->SR6E);
    VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6F, restore->SR6F);
  }

xf86DrvMsg(pScrn->scrnIndex, X_INFO, "6c %02X, 6d %02X, 6e %02X, 6f %02X\n", restore->SR6C, restore->SR6D, restore->SR6E, restore->SR6F);
  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, 0x00);

  WRITE_DPR (pSmi, 0x10, restore->DPR10);
  WRITE_DPR (pSmi, 0x1C, restore->DPR1C);
  WRITE_DPR (pSmi, 0x20, restore->DPR20);
  WRITE_DPR (pSmi, 0x24, restore->DPR24);
  WRITE_DPR (pSmi, 0x28, restore->DPR28);
  WRITE_DPR (pSmi, 0x2C, restore->DPR2C);
  WRITE_DPR (pSmi, 0x30, restore->DPR30);
  WRITE_DPR (pSmi, 0x3C, restore->DPR3C);
  WRITE_DPR (pSmi, 0x40, restore->DPR40);
  WRITE_DPR (pSmi, 0x44, restore->DPR44);

  WRITE_VPR (pSmi, 0x00, restore->VPR00);
  WRITE_VPR (pSmi, 0x0C, restore->VPR0C);
  WRITE_VPR (pSmi, 0x10, restore->VPR10);

  xf86DrvMsg (pScrn->scrnIndex, X_ERROR, " FPR0C = %X FPR10 = %X\n",
	      restore->VPR0C, restore->VPR10);

  if (pSmi->Chipset == SMI_COUGAR3DR)
    {
      WRITE_FPR (pSmi, FPR00, restore->FPR00_);
      WRITE_FPR (pSmi, FPR0C, restore->FPR0C_);
      WRITE_FPR (pSmi, FPR10, restore->FPR10_);
    }

  WRITE_CPR (pSmi, 0x00, restore->CPR00);

	
      //old code 
  if (xf86GetVerbosity () > 1)
    {
       xf86DrvMsgVerb (pScrn->scrnIndex, X_INFO, VERBLEV,
		      "Done restoring mode.  Register dump:\n");
   //    SMI_PrintRegs (pScrn);
    }

  vgaHWProtect (pScrn, FALSE);

  


}
/*
 * This function is used to restore a video mode. It writes out all of the
 * standard VGA and extended registers needed to setup a video mode.
 */

static void
SMI_WriteMode (ScrnInfoPtr pScrn, vgaRegPtr vgaSavePtr, SMIRegPtr restore)
{
  int i;
  CARD8 tmp;
  CARD32 offset;

  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  int vgaIOBase = hwp->IOBase;
  int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
  int vgaCRData = vgaIOBase + VGA_CRTC_DATA_OFFSET;

  

  vgaHWProtect (pScrn, TRUE);

  /* Wait for engine to become idle */
  WaitIdle ();

/*
#ifdef SMI_DEBUG
{
    for (i = 0; i < 14; i++)
	xf86DrvMsg(pScrn->scrnIndex, X_INFO, " CR4%x is 0x%x\n", i, restore->CR40[i]);
}
#endif
*/

#if 1

  if (pSmi->useBIOS && (pSmi->pInt10 != NULL) && (restore->mode != 0))
    {
      pSmi->pInt10->num = 0x10;
      pSmi->pInt10->ax = restore->mode | 0x80;
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "Setting mode 0x%02X\n",
		  restore->mode);

      SMI_DisableMmio (pScrn);
      xf86ExecX86int10 (pSmi->pInt10);

      /* Enable linear mode. */
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
      tmp = inb (pSmi->PIOBase + VGA_SEQ_DATA);
      outb (pSmi->PIOBase + VGA_SEQ_DATA, tmp | 0x01);

      /* Enable DPR/VPR registers. */
      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03);
    }
  else
#endif
    {

#if 0
      pSmi->pInt10->num = 0x10;
      pSmi->pInt10->ax = 0x63;
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, " Setting mode 63\n");
      SMI_DisableMmio (pScrn);
      xf86ExecX86int10 (pSmi->pInt10);	/*Call Bios  boyod */
      SMI_EnableMmio (pScrn);
#else
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, " Restore mode 0x%02X\n",
		  restore->mode);

      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x17, restore->SR17);

      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18) & ~0x1F;
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_WriteMode(), read SR18 is 0x%x\n", VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18));
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x18, tmp | (restore->SR18 & 0x1F) | 0x11);
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_WriteMode(), save->SR18 is 0x%x, writing 0x%x to SEQ18 now\n", restore->SR18, tmp | (restore->SR18 & 0x1F));

      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
      // VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03);
      /* changed by Belcon to enable CRT output */
      /* Belcon : clone mode */
      if (pSmi->clone_mode) {
        VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x13 | 0x20);
      } else {      //if set PDR21[5] to 0,many garbage will be written into video memory
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03 | 0x20);
      //VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, tmp & ~0x03 & ~0x20);     
      }
/*
      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x22);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, 0x02);
*/


      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31) & ~0xC0;

      /* Belcon: clone mode */
      if (pSmi->clone_mode) {
        VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp |
		     (restore->SR31 | 0xC3));
      } else {
        VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp |
		     (restore->SR31 & 0xC0));
      }
     // VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp | 0xC0);

      tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x32) & ~0x07;
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x32, tmp |
		     (restore->SR32 & 0x07));
      if (restore->SR6B != 0xFF)
	{
	  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6A,
			 restore->SR6A);
	  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6B,
			 restore->SR6B);
	}
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, " Mode Pclock 0x%02X 0x%02X\n",
		  restore->SR6C, restore->SR6D);
      /*boyod add */
      if (restore->SR6D != 0x0)
	{
	  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6C,
			 restore->SR6C);
	  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6D,
			 restore->SR6D);
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, " Alert: SEQ6C is 0x%02X, SEQ6D is 0x%02X\n", VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6c), VGAIN8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6d));
	}

      /* Belcon: clone mode */
      if ((pSmi->clone_mode) && (restore->SR6F != 0x0)) {
	  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6E,
			 restore->SR6E);
	  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x6F,
			 restore->SR6F);
      }


      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, restore->SR81);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0xA0, restore->SRA0);

      /* Restore the standard VGA registers */
      vgaHWRestore (pScrn, vgaSavePtr, VGA_SR_ALL);
      if (restore->smiDACMask)
	{
	  VGAOUT8 (pSmi, VGA_DAC_MASK, restore->smiDACMask);
	}
      else
	{
	  VGAOUT8 (pSmi, VGA_DAC_MASK, 0xFF);
	}
      VGAOUT8 (pSmi, VGA_DAC_WRITE_ADDR, 0);
      for (i = 0; i < 256; i++)
	{
	  VGAOUT8 (pSmi, VGA_DAC_DATA, restore->smiDacRegs[i][0]);
	  VGAOUT8 (pSmi, VGA_DAC_DATA, restore->smiDacRegs[i][1]);
	  VGAOUT8 (pSmi, VGA_DAC_DATA, restore->smiDacRegs[i][2]);
	}
      for (i = 0, offset = 2; i < 8192; i++, offset += 8)
	{
	  *(pSmi->FBBase + offset) = restore->smiFont[i];
	}

      if (SMI_LYNXM_SERIES (pSmi->Chipset))
	{
        /*
         * FIXME:: hardcode here
         */
/*
        if (pSmi->Chipset == SMI_LYNXEMplus) {
	    tmp = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E);
	    VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E, 0x00);
        }
*/
	  /* Restore secondary registers */
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E,
			 restore->CR90[14] | 0x20);

	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x30, restore->CR30);
// xf86DrvMsg("", X_INFO, "Belcon:test:line: %d cr30 is 0x%x\n", __LINE__, restore->CR30);
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x33, restore->CR33_2);

/*
          tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x33) | 0x20;
          VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x33, tmp);
*/

          /* Belcon : clone mode */
          if (pSmi->clone_mode) {
            for (i = 0; i < 8; i++) {
              VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x50 + i,
                            restore->FPR50[i]);
            }
            VGAOUT8_INDEX(pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x5A,
                          restore->FPR50[0x0A]);

            /* LCD Frame Buffer starting address */
            VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x40, (CARD8)((restore->VPR0C) & 0xFF));
            VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x41, (CARD8)((restore->VPR0C >> 8) & 0xFF));
            VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x44, 0);
            /* why | 0x40 ? hardcode here. Belcon */
            VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x45, (CARD8)((restore->VPR0C >> 13) & 0x38) | 0x40);
          }

	  for (i = 0; i < 14; i++)
	    {
	      VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x40 + i,
			     restore->CR40_2[i]);
// xf86DrvMsg("", X_INFO, "Belcon:haha restore secondary CR%d: 0x%x\n", 0x40+i, restore->CR40_2[i]);
	    }
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9F, restore->CR9F_2);

	  /* Restore primary registers */
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E,
			 restore->CR90[14] & ~0x20);

	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x33, restore->CR33);
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x3A, restore->CR3A);
	  for (i = 0; i < 14; i++)
	    {
	      VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x40 + i,
			     restore->CR40[i]);
xf86DrvMsg("", X_INFO, "Belcon:haha restore primary CR%d: 0x%x\n", 0x40+i, restore->CR40[i]);
	    }

	  for (i = 0; i < 16; i++)
	    {
	      if (i != 14)
		{
		  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x90 + i,
				 restore->CR90[i]);
		}

	    }
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E,
			 restore->CR90[14]);

	  /* Restore common registers */
	  for (i = 0; i < 14; i++)
	    {
	      VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0xA0 + i,
			     restore->CRA0[i]);
	    }
	}

      /* Restore the standard VGA registers */
      if (xf86IsPrimaryPci (pSmi->PciInfo))
	{
	  vgaHWRestore (pScrn, vgaSavePtr, VGA_SR_CMAP | VGA_SR_FONTS);
	}

      if (restore->modeInit)
	vgaHWRestore (pScrn, vgaSavePtr, VGA_SR_ALL);

      if (!SMI_LYNXM_SERIES (pSmi->Chipset))
	{
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x30, restore->CR30);
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x33, restore->CR33);
	  VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x3A, restore->CR3A);
	  for (i = 0; i < 14; i++)
	    {
	      VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x40 + i,
			     restore->CR40[i]);
	    }
	}

#endif
    }

  /* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
  if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR))
    {
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66, restore->CCR66);
    }
  /* end CZ */

  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x81, 0x00);

  WRITE_DPR (pSmi, 0x10, restore->DPR10);
  WRITE_DPR (pSmi, 0x1C, restore->DPR1C);
  WRITE_DPR (pSmi, 0x20, restore->DPR20);
  WRITE_DPR (pSmi, 0x24, restore->DPR24);
  WRITE_DPR (pSmi, 0x28, restore->DPR28);
  WRITE_DPR (pSmi, 0x2C, restore->DPR2C);
  WRITE_DPR (pSmi, 0x30, restore->DPR30);
  WRITE_DPR (pSmi, 0x3C, restore->DPR3C);
  WRITE_DPR (pSmi, 0x40, restore->DPR40);
  WRITE_DPR (pSmi, 0x44, restore->DPR44);

  WRITE_VPR (pSmi, 0x00, restore->VPR00);
  WRITE_VPR (pSmi, 0x0C, restore->VPR0C);
  WRITE_VPR (pSmi, 0x10, restore->VPR10);

  xf86DrvMsg (pScrn->scrnIndex, X_INFO, " FPR00 = %X FPR0C = %X FPR10 = %X\n",
	      restore->VPR00, restore->VPR0C, restore->VPR10);

  if (pSmi->Chipset == SMI_COUGAR3DR)
    {
      WRITE_FPR (pSmi, FPR00, restore->FPR00_);
      WRITE_FPR (pSmi, FPR0C, restore->FPR0C_);
      WRITE_FPR (pSmi, FPR10, restore->FPR10_);
    }

  WRITE_CPR (pSmi, 0x00, restore->CPR00);




#ifdef  _EXPERIMENTAL_XSERVER64	/*  IGXMAL -- Manage Simultaneous (virtual refresh) mode */
  {

    /* Get Hres, Vres for this mode */
    int iHres;
    int iVres;

    if (restore->mode >= 0x65)
      {
	iHres = 1280;
	iVres = 1024;
      }
    else if (restore->mode >= 0x60)
      {
	iHres = 1024;
	iVres = 768;
      }
    else if ((restore->mode >= 0x50) && (restore->mode <= 0x54))
      {
	iHres = 640;
	iVres = 480;
      }
    else
      {
	iHres = 800;
	iVres = 600;
      }

xf86DrvMsg(pScrn->scrnIndex, X_INFO, " XServer 64, iHres is %d, iVres is %d\n", iHres, iVres);
xf86DrvMsg(pScrn->scrnIndex, X_INFO, " XServer 64, lcdWidth %d, lcdWidth is %d\n", iHres, iVres);
    if (restore->mode > 0)
      {
	int iVertAdjust;
	int iHorzAdjust;

	/*boyod */
	/* Set VR bit for hi-res modes */
	tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31) & ~0x07;
	VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp | 0x80);

	/* Handle centering here */
	/* If (res < panelsize) then set 3d4.9e and 3d4.a6 */
	if ((iHres < pSmi->lcdWidth) || (iVres < pSmi->lcdHeight))
	  {
	    tmp = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E);
	    VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0x9E, 0x00);

	    iVertAdjust = min (63, (pSmi->lcdHeight - iVres) / 2 / 4);
	    iHorzAdjust = min (127, (pSmi->lcdWidth - iHres) / 2 / 8);

	    VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0xA7, iHorzAdjust);
	    VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, 0xA6, iVertAdjust);
	  }
      }
    else
      {
	/* Clear virtual refresh bit for text modes */
	tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31) & ~0x80;
	VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, tmp);


	/*   removed by Boyod */
	/* clear centering bits in 3d4.9e */
/*
           tmp = VGAIN8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E) & ~0x00;
           VGAOUT8_INDEX(pSmi, vgaCRIndex, vgaCRData, 0x9E, tmp);
*/
      }
  }
#endif


  if (xf86GetVerbosity () > 1)
    {
      xf86DrvMsgVerb (pScrn->scrnIndex, X_INFO, VERBLEV,
		      "Done restoring mode.  Register dump:\n");
     // SMI_PrintRegs (pScrn);
    }

  vgaHWProtect (pScrn, FALSE);

  
}

static Bool
SMI_MapMem (ScrnInfoPtr pScrn)
{
  SMIPtr pSmi = SMIPTR (pScrn);
  vgaHWPtr hwp;
  CARD32 memBase;
  int	ret;

  
  switch (pSmi->Chipset)
    {
    default:
      ret = smi_mapmemory(pScrn, pSmi);
      break;

    case SMI_COUGAR3DR:
      ret = smi_mapmemory_730(pScrn, pSmi);
      break;

    case SMI_LYNX3D:
      ret = smi_mapmemory_820(pScrn, pSmi);
      break;

    case SMI_LYNXEM:
    case SMI_LYNXEMplus:
      ret = smi_mapmemory_712(pScrn, pSmi);
      break;

    case SMI_LYNX3DM:
      ret = smi_mapmemory_720(pScrn, pSmi);
      break;

    case SMI_MSOC:
	  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "SMI_MSOC: %s:%d\n", __func__, __LINE__);
	      ret = smi_mapmemory_501(pScrn, pSmi);
	  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "SMI_MSOC: %s:%d\n", __func__, __LINE__);
      break;
  }
  if (ret == FALSE)
    return (ret);

  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "%s Panel Size = %dx%d\n",
	      (pSmi->lcd == 0) ? "OFF" : (pSmi->lcd == 1) ? "TFT" : "DSTN",
	      pSmi->lcdWidth, pSmi->lcdHeight);
  xf86DrvMsg (pScrn->scrnIndex, X_INFO, " SiliconMotion Driver\n");
#if 0
  if (SMI_MSOC != pSmi->Chipset)	/* 501?  No VGA!  */
    {
      /* Assign hwp->MemBase & IOBase here */
      hwp = VGAHWPTR (pScrn);
      if (pSmi->IOBase != NULL)
	{
	  vgaHWSetMmioFuncs (hwp, pSmi->MapBase,
			     pSmi->IOBase - pSmi->MapBase);
	}
      vgaHWGetIOBase (hwp);

      /* Map the VGA memory when the primary video */
      if (xf86IsPrimaryPci (pSmi->PciInfo))
	{
	  hwp->MapSize = 0x10000;
	  if (!vgaHWMapMem (pScrn))
	    {
	      
	      return (FALSE);
	    }
	  pSmi->PrimaryVidMapped = TRUE;
	}
    }
#endif
  
  return (TRUE);
}

/* UnMapMem - contains half of pre-4.0 EnterLeave function.  The EnterLeave
 * function which en/disable access to IO ports and ext. regs
 */

static void
SMI_UnmapMem (ScrnInfoPtr pScrn)
{
  SMIPtr pSmi = SMIPTR (pScrn);

  

  /* Unmap VGA mem if mapped. */
  if (pSmi->PrimaryVidMapped)
    {
      vgaHWUnmapMem (pScrn);
      pSmi->PrimaryVidMapped = FALSE;
    }

  SMI_DisableMmio (pScrn);
  
  	if(pSmi->MapBase){
#ifndef XSERVER_LIBPCIACCESS
	xf86UnMapVidMem (pScrn->scrnIndex, (pointer) pSmi->MapBase, pSmi->MapSize);
#else
	pci_device_unmap_range(pSmi->PciInfo,(pointer)pSmi->MapBase,pSmi->MapSize);
#endif
  	}

  if (pSmi->FBBase){
  #ifndef XSERVER_LIBPCIACCESS
      	xf86UnMapVidMem (pScrn->scrnIndex, (pointer) pSmi->FBBase,pSmi->videoRAMBytes);
  #else
  	pci_device_unmap_range(pSmi->PciInfo,(pointer)pSmi->FBBase,pSmi->videoRAMBytes);
  #endif
  	}
	pSmi->IOBase = NULL;
	//xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_UnmapMem(), pSmi->SR18Value is 0x%x\n", pSmi->SR18Value);
	
}

/* This gets called at the start of each server generation. */

static Bool
SMI_ScreenInit (int scrnIndex, ScreenPtr pScreen, int argc, char **argv)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SMIPtr pSmi = SMIPTR (pScrn);
  EntityInfoPtr pEnt;



  pEnt = xf86GetEntityInfo (pScrn->entityList[0]);

#ifdef _XSERVER64
  if (!pSmi->pInt10 && !(SMI_MSOC == pSmi->Chipset))
    {
      pSmi->pInt10 = xf86InitInt10 (pEnt->index);
    }
#endif

  /* Map MMIO regs and framebuffer */
  if (!SMI_MapMem (pScrn))
    {
      
      return (FALSE);
    }

 /* Save the chip/graphics state */
  SMI_Save (pScrn);

 /* Teddy:Fill in some needed pScrn fields */
  pScrn->vtSema = TRUE;
  pScrn->pScreen = pScreen;
  pSmi->FBOffset = 0;
  pScrn->fbOffset = pSmi->FBOffset + pSmi->fbMapOffset;
 

   /* Initialize the first mode */

  if (!SMI_ModeInit (pScrn, pScrn->currentMode))
    {
      
      return (FALSE);
    }

  /* Clear frame buffer */
  memset (pSmi->FBBase, 0, pSmi->videoRAMBytes);

  /*
   * The next step is to setup the screen's visuals, and initialise the
   * framebuffer code.  In cases where the framebuffer's default choises for
   * things like visual layouts and bits per RGB are OK, this may be as simple
   * as calling the framebuffer's ScreenInit() function.  If not, the visuals
   * will need to be setup before calling a fb ScreenInit() function and fixed
   * up after.
   *
   * For most PC hardware at depths >= 8, the defaults that cfb uses are not
   * appropriate.  In this driver, we fixup the visuals after.
   */

  /*
   * Reset the visual list.
   */
  miClearVisualTypes ();

  /* Setup the visuals we support. */

  /*
   * For bpp > 8, the default visuals are not acceptable because we only
   * support TrueColor and not DirectColor.  To deal with this, call
   * miSetVisualTypes with the appropriate visual mask.
   */

   if (!miSetVisualTypes (pScrn->depth, miGetDefaultVisualMask(pScrn->depth),pScrn->rgbBits, pScrn->defaultVisual))
    {
      return (FALSE);
    }



  if (!miSetPixmapDepths ())
  {
		  
		    return FALSE;
   }



  if (!SMI_InternalScreenInit (scrnIndex, pScreen))
    {
      
      return (FALSE);
    }



  xf86SetBlackWhitePixels (pScreen);

  if (pScrn->bitsPerPixel > 8)
    {
      VisualPtr visual;
      /* Fixup RGB ordering */
      visual = pScreen->visuals + pScreen->numVisuals;
      while (--visual >= pScreen->visuals)
	{
	  if ((visual->class | DynamicClass) == DirectColor)
	    {
	      visual->offsetRed = pScrn->offset.red;
	      visual->offsetGreen = pScrn->offset.green;
	      visual->offsetBlue = pScrn->offset.blue;
	      visual->redMask = pScrn->mask.red;
	      visual->greenMask = pScrn->mask.green;
	      visual->blueMask = pScrn->mask.blue;
	    }
	}
    }

/* #ifdef USE_FB*/
  /* must be after RGB ordering fixed */

  fbPictureInit (pScreen, 0, 0);

/*#endif*/

  /* CZ 18.06.2001: moved here from smi_accel.c to have offscreen
     framebuffer in NoAccel mode */
  {
    int numLines, maxLines;
    BoxRec AvailFBArea;

xf86DrvMsg(pScrn->scrnIndex, X_INFO, "BDEBUG: pSmi->FBReserved is %d, pSmi->width is %d, pSmi->Bpp is %d\n", pSmi->FBReserved, pSmi->width, pSmi->Bpp);
//    maxLines = pSmi->FBReserved / (pSmi->width * pSmi->Bpp);
    maxLines = pSmi->FBReserved / (pScrn->displayWidth * pSmi->Bpp);
    if (pSmi->rotate)
      {
	numLines = maxLines;
      }
    else
      {
	/* CZ 3.11.2001: What does the following code? see also smi_video.c aaa line 1226 */
/*#if SMI_USE_VIDEO */
#if 0
	numLines = ((pSmi->FBReserved - pSmi->width * pSmi->Bpp
		     * pSmi->height) * 25 / 100 + pSmi->width
		    * pSmi->Bpp - 1) / (pSmi->width * pSmi->Bpp);
	numLines += pSmi->height;
#else
	numLines = maxLines;
#endif
      }

    AvailFBArea.x1 = 0;
    AvailFBArea.y1 = 0;
    AvailFBArea.x2 = (pSmi->width + 15) & ~15;;
//    AvailFBArea.x2 = pScrn->displayWidth;
    AvailFBArea.y2 = numLines;
    xf86DrvMsg (pScrn->scrnIndex, X_INFO,
		"FrameBuffer Box: %d,%d - %d,%d\n",
		AvailFBArea.x1, AvailFBArea.y1, AvailFBArea.x2,
		AvailFBArea.y2);

    xf86InitFBManager (pScreen, &AvailFBArea);

  }
  /* end CZ */

  /* Initialize acceleration layer */
  if (!pSmi->NoAccel)
    {
      if (!SMI_AccelInit (pScreen))
	{
	  
	  return (FALSE);
	}
    }

  miInitializeBackingStore (pScreen);

  /* hardware cursor needs to wrap this layer */

  SMI_DGAInit (pScreen);

  /* Initialise cursor functions */
  miDCInitialize (pScreen, xf86GetPointerScreenFuncs ());

  /* Initialize HW cursor layer.  Must follow software cursor
   * initialization.
   */
 #if 1 	//caesar removed 
  if (pSmi->hwcursor)
    {
      if (!SMI_HWCursorInit (pScreen))
	{
	  xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Hardware cursor "
		      "initialization failed\n");
	}
    }
  #endif

  if (pSmi->shadowFB)
    {
      Bool bRetCode;

      RefreshAreaFuncPtr refreshArea;

      if (pSmi->Chipset == SMI_COUGAR3DR)
	{
	  refreshArea = SMI_RefreshArea730;
	}
      else
	{
	  refreshArea = SMI_RefreshArea;
	}


      if (pSmi->rotate || pSmi->RandRRotation) //caesar modify
	{
	  if (pSmi->PointerMoved == NULL)
	    {
	      pSmi->PointerMoved = pScrn->PointerMoved;
	      pScrn->PointerMoved = SMI_PointerMoved;
	    }
	}

      bRetCode = ShadowFBInit (pScreen, refreshArea);
    }

  /* Initialise default colormap */
  if (!miCreateDefColormap (pScreen))
    {
      
      return (FALSE);
    }

  /* Initialize colormap layer.  Must follow initialization of the default
   * colormap.  And SetGamma call, else it will load palette with solid white.
   */
  /* CZ 2.11.2001: CMAP_PALETTED_TRUECOLOR for gamma correction */
  if (!xf86HandleColormaps
      (pScreen, 256, pScrn->rgbBits, SMI_LoadPalette, NULL,
       CMAP_RELOAD_ON_MODE_SWITCH | CMAP_PALETTED_TRUECOLOR))
    {
      
      return (FALSE);
    }

  pScreen->SaveScreen = SMI_SaveScreen;
  pSmi->CloseScreen = pScreen->CloseScreen;
  pScreen->CloseScreen = SMI_CloseScreen;

  /* Added by Belcon to enable LCD Panel Control Select */
  if (pSmi->Chipset == SMI_LYNXEMplus)
    {
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x22, 2);
    }

  if (!xf86DPMSInit (pScreen, SMI_DisplayPowerManagementSet, 0))
    {
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "DPMS initialization failed!\n");
    }

xf86DrvMsg(pScrn->scrnIndex, X_INFO, " Belcon: before SMI_InitVideo\n");
  SMI_InitVideo (pScreen);
xf86DrvMsg(pScrn->scrnIndex, X_INFO, " Belcon: after SMI_InitVideo\n");

#ifdef RANDR
    /* Install our DriverFunc.  We have to do it this way instead of using the
     * HaveDriverFuncs argument to xf86AddDriver, because InitOutput clobbers
     * pScrn->DriverFunc */
    pScrn->DriverFunc = SMI_DriverFunc;
#endif


  /* Report any unused options (only for the first generation) */
  if (serverGeneration == 1)
    {
      xf86ShowUnusedOptions (pScrn->scrnIndex, pScrn->options);
    }

  
  
#ifndef XSERVER_LIBPCIACCESS
	{
  volatile CARD8	config;
  config = pciReadByte(pSmi->PciTag, PCI_CMD_STAT_REG);
//  pciWriteByte(pSmi->PciTag, PCI_CMD_STAT_REG, config | PCI_CMD_MEM_ENABLE);
	}
#endif

  
  return (TRUE);
}

/* Common init routines needed in EnterVT and ScreenInit */

static int
SMI_InternalScreenInit (int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SMIPtr pSmi = SMIPTR (pScrn);
  int width, height, displayWidth;
  int bytesPerPixel = pScrn->bitsPerPixel / 8;
  int xDpi, yDpi;
  int ret;

  

  if (pSmi->rotate && pSmi->rotate != SMI_ROTATE_UD)
    {
      width = pScrn->virtualY;
//      width = (pScrn->virtualY + 15) & ~15;
      height = pScrn->virtualX;
//      height = (pScrn->virtualX + 15) & ~15;
      xDpi = pScrn->yDpi;
      yDpi = pScrn->xDpi;
      displayWidth = ((width * bytesPerPixel + 15) & ~15) / bytesPerPixel;
    }
  else
    {
      width = pScrn->virtualX;
//	  width = (pScrn->virtualX + 15) & ~15;		
      height = pScrn->virtualY;
//      height = (pScrn->virtualY + 15) & ~15;
      xDpi = pScrn->xDpi;
      yDpi = pScrn->yDpi;
      displayWidth = pScrn->displayWidth;
    }

  if (pSmi->shadowFB)
    {
      pSmi->ShadowWidth = width;
      pSmi->ShadowHeight = height;
//      pSmi->ShadowWidthBytes = (width * bytesPerPixel + 15) & ~15;
      pSmi->ShadowWidthBytes = ((width + 15) & ~15) * bytesPerPixel ;			
      if (bytesPerPixel == 3)
	{
		if(pSmi->rotate == SMI_ROTATE_CW || pSmi->rotate == SMI_ROTATE_CCW)	
	  	{
	  		pSmi->ShadowPitch = ((height * 3) << 16) | pSmi->ShadowWidthBytes;
		}
		else
		{
			pSmi->ShadowPitch = ((width * 3) << 16) | pSmi->ShadowWidthBytes;
		}
	}
      else
	{

		if(pSmi->rotate == SMI_ROTATE_CW || pSmi->rotate == SMI_ROTATE_CCW)	
	  	{
//	  		pSmi->ShadowPitch = (height << 16) |(pSmi->ShadowWidthBytes / bytesPerPixel);
	  		pSmi->ShadowPitch = (((height + 15) & ~15)<< 16) |(pSmi->ShadowWidthBytes / bytesPerPixel);				
		}
		else
		{
//			pSmi->ShadowPitch = (width << 16) |(pSmi->ShadowWidthBytes / bytesPerPixel);	
			pSmi->ShadowPitch = (((width + 15) & ~15)<< 16) |(pSmi->ShadowWidthBytes / bytesPerPixel);	
		}
	 
	}



      pSmi->saveBufferSize = pSmi->ShadowWidthBytes * pSmi->ShadowHeight;
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line %d: pSmi->FBReserved is 0x%x\n", __LINE__, pSmi->FBReserved);
      pSmi->FBReserved -= pSmi->saveBufferSize;
      pSmi->FBReserved &= ~0x15;
xf86DrvMsg(pScrn->scrnIndex, X_INFO, "line %d: pSmi->FBReserved is 0x%x\n", __LINE__, pSmi->FBReserved);
      WRITE_VPR (pSmi, 0x0C, (pSmi->FBOffset = pSmi->FBReserved) >> 3);

      if (pSmi->Chipset == SMI_COUGAR3DR)
	{
	  WRITE_FPR (pSmi, FPR0C, pSmi->FBOffset >> 3);
	}

#if 1
      if (SMI_MSOC == pSmi->Chipset)
	{
 xf86DrvMsg("", X_INFO, "pSmi->SCRBase is 0x%x, DCRBase is 0x%x, FBOffset is 0x%x\n", pSmi->SCRBase, pSmi->DCRBase, pSmi->FBOffset);
	  if (!pSmi->IsSecondary) {
	    WRITE_DCR (pSmi, DCR0C, pSmi->FBOffset);
	  } else {
	    WRITE_DCR (pSmi, DCR204, pSmi->FBOffset);
          }
	}
#endif

      pScrn->fbOffset = pSmi->FBOffset + pSmi->fbMapOffset;
      xf86DrvMsg (pScrn->scrnIndex, X_INFO, "Shadow: width=%d height=%d "
		  "offset=0x%08X pitch=0x%08X\n", pSmi->ShadowWidth,
		  pSmi->ShadowHeight, pSmi->FBOffset, pSmi->ShadowPitch);
xf86DrvMsg("", X_INFO, "line %d: Internalxxx: fbOffset = 0x%x\n", __LINE__, pScrn->fbOffset);
    }
  else
    {
#if 0
      pSmi->FBOffset = 0;
      pScrn->fbOffset = pSmi->FBOffset + pSmi->fbMapOffset;
#endif
xf86DrvMsg("", X_INFO, "line %d: Internalxxx: fbOffset = 0x%x, FBOffset is 0x%x\n", __LINE__, pScrn->fbOffset, pSmi->FBOffset);
    }

  /*
   * Call the framebuffer layer's ScreenInit function, and fill in other
   * pScreen fields.
   */


  DEBUG ((VERBLEV, "\tInitializing FB @ 0x%08X for %dx%d (%d)\n",
	  pSmi->FBBase, width, height, displayWidth));
  switch (pScrn->bitsPerPixel)
    {
/* #ifdef USE_FB*/
    case 8:
    case 16:
    case 24:
    case 32:

#ifdef XORG_VERSION_CURRENT
	#if XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
	   if (SMI_MSOC != pSmi->Chipset && pScrn->bitsPerPixel == 32)
		 ret = fbScreenInit (pScreen, pSmi->FBBase, width, height, xDpi,
			      yDpi, displayWidth, pScrn->bitsPerPixel);
	#endif
#endif
      if ((SMI_MSOC == pSmi->Chipset && pScrn->bitsPerPixel != 24) ||
	  (SMI_MSOC != pSmi->Chipset && pScrn->bitsPerPixel != 32))

	{
	  ret = fbScreenInit (pScreen, pSmi->FBBase, width, height, xDpi,
			      yDpi, displayWidth, pScrn->bitsPerPixel);
	}
      break;

    default:
      xf86DrvMsg (scrnIndex, X_ERROR, "Internal error: invalid bpp (%d) "
		  "in SMI_InternalScreenInit\n", pScrn->bitsPerPixel);
      
      return (FALSE);
    }

  if ((SMI_MSOC == pSmi->Chipset) && (pScrn->bitsPerPixel == 8))
    {
      /* Initialize Palette entries 0 and 1, they don't seem to get hit */
      if (!pSmi->IsSecondary)
	{
	  WRITE_DCR (pSmi, DCR400 + 0, 0x00000000);	/* CRT Palette       */
	  WRITE_DCR (pSmi, DCR400 + 4, 0x00FFFFFF);	/* CRT Palette       */
	}
      else
	{
	  WRITE_DCR (pSmi, DCR800 + 0, 0x00000000);	/* Panel Palette */
	  WRITE_DCR (pSmi, DCR800 + 4, 0x00FFFFFF);	/* Panel Palette */
	}

    }

  
  return (ret);
}

/* Checks if a mode is suitable for the selected configuration. */
static ModeStatus
SMI_ValidMode (int scrnIndex, DisplayModePtr mode, Bool verbose, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  SMIPtr pSmi = SMIPTR (pScrn);
  float refresh;

  
// xf86DrvMsg("", X_INFO, "Belcon: %d x %d , %d-bpp\n", mode->HDisplay, mode->VDisplay, pScrn->bitsPerPixel);
  refresh = (mode->VRefresh > 0) ? mode->VRefresh
    : mode->Clock * 1000.0 / mode->VTotal / mode->HTotal;
  xf86DrvMsg (scrnIndex, X_INFO, " Mode: %dx%d %d-bpp, %fHz\n",
	      mode->HDisplay, mode->VDisplay, pScrn->bitsPerPixel, refresh);

  if (pSmi->shadowFB)
    {
      int mem;

/*		if (pScrn->bitsPerPixel > 16)
		{
			
			return(MODE_BAD);
		}
*/
      mem = (pScrn->virtualX * pScrn->bitsPerPixel / 8 + 15) & ~15;
      mem *= pScrn->virtualY * 2;

      if (mem > pSmi->FBReserved)	/* PDR#1074 */
	{
	  
	  return (MODE_MEM);
	}
    }

/*
 * FIXME:
 *	Why we need these code here?
 * #if 1
 */
#if 0
  if (!pSmi->useBIOS || pSmi->lcd)
    {

#if 1				/* PDR#983 */
      if (pSmi->zoomOnLCD)
	{
xf86DrvMsg("", X_INFO, "HDisplay %d, lcdWidth %d\n", mode->HDisplay, pSmi->lcdWidth);
	  if ((mode->HDisplay > pSmi->lcdWidth)
	      || (mode->VDisplay > pSmi->lcdHeight))
	    {
	      
	      return (MODE_PANEL);
	    }
	}
      else
#endif
	{
	  if ((mode->HDisplay != pSmi->lcdWidth)
	      || (mode->VDisplay != pSmi->lcdHeight))
	    {
	      
	      return (MODE_PANEL);
	    }
	}

    }
#endif

  // Mode added by Belcon according new mode table
  if (!(((mode->HDisplay == 1280) && (mode->VDisplay == 1024)) ||
	((mode->HDisplay == 1024) && (mode->VDisplay == 768)) ||
	((mode->HDisplay == 800) && (mode->VDisplay == 600)) ||
	((mode->HDisplay == 640) && (mode->VDisplay == 480)) ||
	((mode->HDisplay == 320) && (mode->VDisplay == 240)) ||
	((mode->HDisplay == 400) && (mode->VDisplay == 300)) ||
	((mode->HDisplay == 1280) && (mode->VDisplay == 960)) ||
	((mode->HDisplay == 1280) && (mode->VDisplay == 768)) ||
	((mode->HDisplay == 1280) && (mode->VDisplay == 720)) ||
	((mode->HDisplay == 1366) && (mode->VDisplay == 768)) ||
	((mode->HDisplay == 1360) && (mode->VDisplay == 768)) ||
	((mode->HDisplay == 1024) && (mode->VDisplay == 600)) ||
	((mode->HDisplay == 800) && (mode->VDisplay == 480)) ||
	((mode->HDisplay == 720) && (mode->VDisplay == 540)) ||
	((mode->HDisplay == 1440) && (mode->VDisplay == 960)) ||
	((mode->HDisplay == 720) && (mode->VDisplay == 480))))
    {
      xf86DrvMsg(pScrn->scrnIndex, X_INFO, "HDisplay %d, VDisplay %d\n", mode->HDisplay, mode->VDisplay);
      return (MODE_BAD_WIDTH);
    }


#if 1				/* PDR#944 */
  if ((pSmi->rotate && pSmi->rotate != SMI_ROTATE_UD) && (SMI_MSOC != pSmi->Chipset))
    {
      if ((mode->HDisplay != pSmi->lcdWidth)
	  || (mode->VDisplay != pSmi->lcdHeight))
	{
	  
	  return (MODE_PANEL);
	}
    }
#endif

  
  return (MODE_OK);
}
static void SMI_SaveReg(ScrnInfoPtr pScrn )
{
	int i;
  	vgaHWPtr hwp = VGAHWPTR (pScrn);
  	SMIPtr pSmi = SMIPTR (pScrn);
  	int vgaIOBase = hwp->IOBase;
  	int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
  	int vgaCRData = vgaIOBase + VGA_CRTC_DATA_OFFSET;
	
	
	/*save seq registers*/
#if 0 
	for(i = 0; i < (sizeof(SeqTable) / sizeof(unsigned char)); i++)
	{
		SeqTable[i] = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, i);	
/*
		if ( 0x18 == i) {
			SeqTable[i] |= 0x11;
		}
	}
//	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SaveReg, SR18 is 0x%x\n", SeqTable[18]);
	
	/*save crt registers*/
	for(i = 0; i < (sizeof(CrtTable) / sizeof(unsigned char)); i++)
	{
		CrtTable[i] = VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRData, i);
	}
#endif
	
	/*save dpr registers*/
	for(i = 0; i < (sizeof(DprTable) / sizeof(unsigned int)); i++)
	{
		DprTable[i] = READ_DPR(pSmi, DprIndexTable[i]);
	}
	for(i = 0; i < (sizeof(VprTable) / sizeof(unsigned int)); i++)
	{
		VprTable[i] = READ_VPR(pSmi, VprIndexTable[i]);
	}
	
	
	return;
}

static void SMI_RestoreReg(ScrnInfoPtr pScrn )
{
	int i;
  	vgaHWPtr hwp = VGAHWPTR (pScrn);
  	SMIPtr pSmi = SMIPTR (pScrn);
  	int vgaIOBase = hwp->IOBase;
  	int vgaCRIndex = vgaIOBase + VGA_CRTC_INDEX_OFFSET;
  	int vgaCRData = vgaIOBase + VGA_CRTC_DATA_OFFSET;
	

#if 0	
	/*restore seq registers*/
	for(i = 0; i < (sizeof(SeqTable) / sizeof(unsigned char)); i++)
	{
		VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, i, SeqTable[i]);	
	}
	
//	xf86DrvMsg(pScrn->scrnIndex, X_INFO, "RestoreReg, SR18 is 0x%x\n", SeqTable[18]);
	/*restore crt registers*/
	for(i = 0; i < (sizeof(CrtTable) / sizeof(unsigned char)); i++)
	{	
		VGAOUT8_INDEX (pSmi, vgaCRIndex, vgaCRData, i, CrtTable[i]);	
	}
#endif
	/*restore dpr registers*/
	for(i = 0; i < (sizeof(DprTable) / sizeof(unsigned int)); i++)
	{
		WRITE_DPR(pSmi, DprIndexTable[i], DprTable[i]); 
	}
	for(i = 0; i < (sizeof(VprTable) / sizeof(unsigned int)); i++)
	{
		WRITE_VPR(pSmi, VprIndexTable[i], VprTable[i]); 
	}

#if 0
	if (SMI_MSOC == pSmi->Chipset) {
		/* Restore cursor color which maybe changed by kernel driver */
		WRITE_DCR(pSmi, DCRF8,  0xFFFFFFFF);
		WRITE_DCR(pSmi, DCRFC,  0x0);
		WRITE_DCR(pSmi, DCR238,  0xFFFFFFFF);
		WRITE_DCR(pSmi, DCR23C,  0x0);
	}
#endif

	
	return;
}

static Bool
SMI_ModeInit (ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  unsigned char tmp;
  int panelIndex, modeIndex, i;
  int xyAddress[] = { 320, 400, 512, 640, 800, 1024, 1280, 1600, 2048 };
  CARD32 DEDataFormat = 0;

  /* Store values to current mode register structs */
  SMIRegPtr new = &pSmi->ModeReg;
  vgaRegPtr vganew = &hwp->ModeReg;

  

  if (SMI_MSOC == pSmi->Chipset)
    {
      pSmi->Bpp = pScrn->bitsPerPixel / 8;


      if (pSmi->rotate && pSmi->rotate != SMI_ROTATE_UD)
	{
	  pSmi->width = pScrn->virtualY;
	  pSmi->height = pScrn->virtualX;
	  pSmi->Stride = (pSmi->height * pSmi->Bpp + 15) & ~15;
	}
      else
	{
	  pSmi->width = pScrn->virtualX;
	  pSmi->height = pScrn->virtualY;
	  pSmi->Stride = (pSmi->width * pSmi->Bpp + 15) & ~15;
	}



      	SMI_MSOCSetMode (pScrn, mode);

      	/* Adjust the viewport */
      	SMI_AdjustFrame (pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

   	/*Add by Teddy for 24bit panel*/
  	if (pSmi->pnl24){
	  if (SMI_MSOC == pSmi->Chipset)
	  	{
	  	smi_set_24bitpanel(pScrn);
	  	}
  	}
      
      return (TRUE);
    }

  if (!vgaHWInit (pScrn, mode))
    {
      
      return (FALSE);
    }

  new->modeInit = TRUE;

  if (pSmi->rotate && pSmi->rotate != SMI_ROTATE_UD)
    {
      pSmi->width = pScrn->virtualY;
      pSmi->height = pScrn->virtualX;
    }
  else
    {
      pSmi->width = pScrn->virtualX;
      pSmi->height = pScrn->virtualY;
    }


  pSmi->Bpp = pScrn->bitsPerPixel / 8;

  outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x17);
  tmp = inb (pSmi->PIOBase + VGA_SEQ_DATA);

  if (pSmi->pci_burst)
    {
      new->SR17 = tmp | 0x20;
    }
  else
    {
      new->SR17 = tmp & ~0x20;
    }

  outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
  new->SR18 = inb (pSmi->PIOBase + VGA_SEQ_DATA) | 0x11;
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_ModeInit(), new->SR18 is 0x%x\n", new->SR18);

  outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x21);
  new->SR21 = inb (pSmi->PIOBase + VGA_SEQ_DATA) & ~0x03;

  if (pSmi->Chipset != SMI_COUGAR3DR)
    {
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x31);
      new->SR31 = inb (pSmi->PIOBase + VGA_SEQ_DATA) & ~0xC0;

      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x32);
      new->SR32 = inb (pSmi->PIOBase + VGA_SEQ_DATA) & ~0x07;

      if (SMI_LYNXM_SERIES (pSmi->Chipset))
	{
	  new->SR32 |= 0x04;
	}
    }

  new->SRA0 = new->CR33 = new->CR3A = 0x00;

  xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "lcdWidth = %d HDisplay = %d\n",
	      pSmi->lcdWidth, mode->HDisplay);

/*boyod*/

  if (pSmi->lcdWidth == 640)
    {
      panelIndex = 0;
    }
  else if (pSmi->lcdWidth == 800)
    {
      panelIndex = 1;
    }
  else if (pSmi->lcdWidth == 1024)
    {
      panelIndex = 2;
    }
  else
    {
      panelIndex = 3;
    }



  if (mode->HDisplay == 640)
    {
      modeIndex = 0;
    }
  else if (mode->HDisplay == 800)
    {
      if (mode->VDisplay == 480)
        modeIndex = 1;
      else
        modeIndex = 2;
    }
  else if (mode->HDisplay == 1024)
    {
      if (mode->VDisplay == 600)
      	modeIndex = 3;
      else
        modeIndex = 4;
    }
  else
    {
      modeIndex = 5;
    }

  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "HDisplay %d, VDisplay %d\n",
	      mode->HDisplay, mode->VDisplay);
  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "panelIndex = %d modeIndex = %d\n",
	      panelIndex, modeIndex);
// #ifdef LCD_SIZE_DETECT
  if (SMI_LYNXM_SERIES (pSmi->Chipset))
#ifdef LCD_SIZE_DETECT
    {
      static unsigned char PanelTable[4][16] = {
	/*              640x480         */
	{0x5F, 0x4F, 0x00, 0x52, 0x1E, 0x0B, 0xDF, 0x00, 0xE9, 0x0B, 0x2E,
	 0x00, 0x4F, 0xDF, 0x07, 0x82},
	/*              800x600                */
	{0x7F, 0x63, 0x00, 0x69, 0x19, 0x72, 0x57, 0x00, 0x58, 0x0C, 0xA2,
	 0x20, 0x4F, 0xDF, 0x1C, 0x85},
	/*              1024x768                */
	{0xA3, 0x7F, 0x00, 0x83, 0x14, 0x24, 0xFF, 0x00, 0x02, 0x08, 0xA7,
	 0xE0, 0x4F, 0xDF, 0x52, 0x89},
	/*              other           */
	{0xCE, 0x9F, 0x00, 0xA7, 0x15, 0x28, 0xFF, 0x28, 0x00, 0xA3, 0x5A,
	 0x20, 0x9F, 0xFF, 0x53, 0x0B},
      };
  xf86DrvMsg (pScrn->scrnIndex, X_INFO, "LCD_SIZE_DETECT\n");

      for (i = 0; i < 14; i++)
	{
	  new->CR40[i] = PanelTable[panelIndex][i];
	}
      new->SR6C = PanelTable[panelIndex][14];
      new->SR6D = PanelTable[panelIndex][15];


      if (panelIndex == 3)
	{
	  new->CR90[14] = 0x18;
	}
      else
	{
	  new->CR90[14] = 0x00;
	}

      if (mode->VDisplay < pSmi->lcdHeight)
	{
	  new->CRA0[6] = (pSmi->lcdHeight - mode->VDisplay) / 8;
	}
      else
	{
	  new->CRA0[6] = 0;
	}

      if (mode->HDisplay < pSmi->lcdWidth)
	{
	  new->CRA0[7] = (pSmi->lcdWidth - mode->HDisplay) / 16;
	}
      else
	{
	  new->CRA0[7] = 0;
	}
    }
  else
#else

  {
#if 0
    static unsigned char PanelTable[4][4][16] = {
      {				/* 640x480 panel */
       {0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
	0x2E, 0x00, 0x4F, 0xDF, 0x07, 0x82},
       {0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
	0x2E, 0x00, 0x4F, 0xDF, 0x07, 0x82},
       {0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
	0x2E, 0x00, 0x4F, 0xDF, 0x07, 0x82},
       {0x5F, 0x4F, 0x00, 0x53, 0x00, 0x0B, 0xDF, 0x00, 0xEA, 0x0C,
	0x2E, 0x00, 0x4F, 0xDF, 0x07, 0x82},
       },
      {				/* 800x600 panel */
       {0x7F, 0x59, 0x19, 0x5E, 0x8E, 0x72, 0x1C, 0x37, 0x1D, 0x00,
	0xA2, 0x20, 0x4F, 0xDF, 0x1c, 0x85},
       {0x7F, 0x63, 0x00, 0x68, 0x18, 0x72, 0x58, 0x00, 0x59, 0x0C,
	0xE0, 0x20, 0x63, 0x57, 0x1c, 0x85},
       {0x7F, 0x63, 0x00, 0x68, 0x18, 0x72, 0x58, 0x00, 0x59, 0x0C,
	0xE0, 0x20, 0x63, 0x57, 0x1c, 0x85},
       {0x7F, 0x63, 0x00, 0x68, 0x18, 0x72, 0x58, 0x00, 0x59, 0x0C,
	0xE0, 0x20, 0x63, 0x57, 0x1c, 0x85},
       },
      {				/* 1024x768 panel */
       {0xA3, 0x67, 0x0F, 0x6D, 0x1D, 0x24, 0x70, 0x95, 0x72, 0x07,
	0xA3, 0x20, 0x4F, 0xDF, 0x52, 0x89},
       {0xA3, 0x71, 0x19, 0x77, 0x07, 0x24, 0xAC, 0xD1, 0xAE, 0x03,
	0xE1, 0x20, 0x63, 0x57, 0x52, 0x89},
       {0xA3, 0x7F, 0x00, 0x85, 0x15, 0x24, 0xFF, 0x00, 0x01, 0x07,
	0xE5, 0x20, 0x7F, 0xFF, 0x52, 0x89},
       {0xA3, 0x7F, 0x00, 0x85, 0x15, 0x24, 0xFF, 0x00, 0x01, 0x07,
	0xE5, 0x20, 0x7F, 0xFF, 0x52, 0x89},
       },
      {                        /* 1280x1024 panel */
       {0xCE, 0x9F, 0x00, 0xA7, 0x15, 0x28, 0xFF, 0x28, 0x00, 0x03,
	0x4A, 0x20, 0x9F, 0xFF, 0x1E, 0x82},
       {0xCE, 0x9F, 0x00, 0xA7, 0x15, 0x28, 0xFF, 0x28, 0x00, 0x03,
	0x4A, 0x20, 0x9F, 0xFF, 0x1E, 0x82},
       {0xCE, 0x9F, 0x00, 0xA7, 0x15, 0x28, 0xFF, 0x28, 0x00, 0x03,
	0x4A, 0x20, 0x9F, 0xFF, 0x1E, 0x82},
       {0xCE, 0x9F, 0x00, 0xA7, 0x15, 0x28, 0xFF, 0x28, 0x00, 0x03,
	0x4A, 0x20, 0x9F, 0xFF, 0x1E, 0x82},
      }
       
    };

    for (i = 0; i < 14; i++)
      {
	new->CR40[i] = PanelTable[panelIndex][modeIndex][i];
      }
    new->SR6C = PanelTable[panelIndex][modeIndex][14];
    new->SR6D = PanelTable[panelIndex][modeIndex][15];
    /*
     * FIXME::
     * Temparily hardcoded here
     * Refer to WriteMode() function
     */
      if (panelIndex == 3)
	{
	  new->CR90[14] = 0x18;
	}
      else
	{
	  new->CR90[14] = 0x00;
	}

      /* Added by Belcon */
      if (modeIndex < 3) {
          new->CR30 &= ~0x09;
      } else {
          new->CR30 |= 0x09;
      }

      if (mode->VDisplay < pSmi->lcdHeight)
	{
	  new->CRA0[6] = (pSmi->lcdHeight - mode->VDisplay) / 8;
	}
      else
	{
	  new->CRA0[6] = 0;
	}

      if (mode->HDisplay < pSmi->lcdWidth)
	{
	  new->CRA0[7] = (pSmi->lcdWidth - mode->HDisplay) / 16;
	}
      else
	{
	  new->CRA0[7] = 0;
	}
#endif
/*
 * Belcon Added
 */
/*
    if (pSmi->lcdWidth == 0) {
*/
      static unsigned char ModeTable[6][3][16] = {
        /* 640x480 */
        {
          /* 60 Hz */
          {
           0x5F, 0x4F, 0x00, 0x54, 0x00, 0x0B, 0xDF, 0x00,
           0xEA, 0x0C, 0x2E, 0x00, 0x4F, 0xDF, 0x07, 0x82,
          },
          /* 75 Hz */
          {
           0x64, 0x4F, 0x00, 0x52, 0x1A, 0xF2, 0xDF, 0x00,
           0xE0, 0x03, 0x0F, 0xC0, 0x4F, 0xDF, 0x16, 0x85,
          },
          /* 85 Hz */
          {
           0x63, 0x4F, 0x00, 0x57, 0x1E, 0xFB, 0xDF, 0x00,
           0xE0, 0x03, 0x0F, 0xC0, 0x4F, 0xDF, 0x88, 0x9B,
          },
        },

        /* 800x480 */
        {
          /* 60 Hz */
          {
           0x6B, 0x63, 0x00, 0x69, 0x1B, 0xF2, 0xDF, 0x00,
           0xE2, 0xE4, 0x1F, 0xC0, 0x63, 0xDF, 0x2C, 0x17,
          },
          /* 75 Hz */
          {
           0x6B, 0x63, 0x00, 0x69, 0x1B, 0xF2, 0xDF, 0x00,
           0xE2, 0xE4, 0x1F, 0xC0, 0x63, 0xDF, 0x2C, 0x17,
          },
          /* 85 Hz */
          {
           0x6B, 0x63, 0x00, 0x69, 0x1B, 0xF2, 0xDF, 0x00,
           0xE2, 0xE4, 0x1F, 0xC0, 0x63, 0xDF, 0x2C, 0x17,
          },
        },

        /* 800x600 */
        {
          /* 60 Hz */
          {
           0x7F, 0x63, 0x00, 0x69, 0x18, 0x72, 0x57, 0x00,
           0x58, 0x0C, 0xE0, 0x20, 0x63, 0x57, 0x1C, 0x85,
          },
          /* 75 Hz */
          {
           0x7F, 0x63, 0x00, 0x66, 0x10, 0x6F, 0x57, 0x00,
           0x58, 0x0B, 0xE0, 0x20, 0x63, 0x57, 0x4C, 0x8B,
          },
          /* 85 Hz */
          {
           0x7E, 0x63, 0x00, 0x68, 0x10, 0x75, 0x57, 0x00,
           0x58, 0x0B, 0xE0, 0x20, 0x63, 0x57, 0x37, 0x87,
          },
        },

        /* 1024x600 */
        {
          /* 60 Hz */
          {
           0xA3, 0x7F, 0x00, 0x82, 0x0B, 0x6F, 0x57, 0x00,
           0x5C, 0x0F, 0xE0, 0xE0, 0x7F, 0x57, 0x16, 0x07,
          },
          /* 60 Hz */
          {
           0xA3, 0x7F, 0x00, 0x82, 0x0B, 0x6F, 0x57, 0x00,
           0x5C, 0x0F, 0xE0, 0xE0, 0x7F, 0x57, 0x16, 0x07,
          },
          /* 60 Hz */
          {
           0xA3, 0x7F, 0x00, 0x82, 0x0B, 0x6F, 0x57, 0x00,
           0x5C, 0x0F, 0xE0, 0xE0, 0x7F, 0x57, 0x16, 0x07,
          },
        },

        /* 1024x768 */
        {
          /* 60 Hz */
          {
           0xA3, 0x7F, 0x00, 0x86, 0x15, 0x24, 0xFF, 0x00,
           0x01, 0x07, 0xE5, 0x20, 0x7F, 0xFF, 0x52, 0x89,
          },
          /* 75 Hz */
          {
           0x9F, 0x7F, 0x00, 0x82, 0x0E, 0x1E, 0xFF, 0x00,
           0x00, 0x03, 0xE5, 0x20, 0x7F, 0xFF, 0x0B, 0x02,
          },
          /* 85 Hz */
          {
           0xA7, 0x7F, 0x00, 0x86, 0x12, 0x26, 0xFF, 0x00,
           0x00, 0x03, 0xE5, 0x20, 0x7F, 0xFF, 0x70, 0x11,
          },
        },

        /* 1280x1024 */
        {
          /* 60 Hz */
          {
           0xCE, 0x9F, 0x00, 0xA7, 0x15, 0x28, 0xFF, 0x00,
           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x53, 0x0B,
          },
          /* 75 Hz */
          {
           0xCE, 0x9F, 0x00, 0xA2, 0x14, 0x28, 0xFF, 0x00,
//           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x13, 0x02,
           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x42, 0x07,           
          },
          /* 85 Hz */
          {
           0xD3, 0x9F, 0x00, 0xA8, 0x1C, 0x2E, 0xFF, 0x00,
//           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x16, 0x42,
           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x0b, 0x01,           
          },
        },
      };
      int	refresh_rate_index = -1;
      /*
       * SR50 ~ SR57
       *
       * SR50: LCD Overflow Register 1 for Virtual Refresh
       * SR51: LCD Overflow Register 2 for Virtual Refresh
       * SR52: LCD Horizontal Total for Virtual Refresh
       * SR53: LCD Horizontal Display Enable for Virtual Refresh
       * SR54: LCD Horizontal Sync Start for Virtual Refresh
       * SR55: LCD Vertical Total for Virtual Refresh
       * SR56: LCD Vertical Display Enable for Virtual Refresh
       * SR57: LCD Vertical Sync Start for Virtual Refresh
       * SR5A: SYNC Pulse-widths Adjustment
       */
      static unsigned char ModeTable_2[6][3][16] = {
        /* 640x480 */
        {
          /* 60 Hz */
          {
           0x04, 0x24, 0x63, 0x4F, 0x52, 0x0C, 0xDF, 0xE9,
           0x00, 0x03, 0x59, 0x00, 0x4F, 0xDF, 0x03, 0x02,
          },
          /* 75 Hz */
          {
           0x04, 0x24, 0x63, 0x4F, 0x52, 0x0C, 0xDF, 0xE9,
           0x00, 0x03, 0x59, 0xC0, 0x4F, 0xDF, 0x16, 0x85,
          },
          /* 85 Hz */
          {
           0x04, 0x24, 0x63, 0x4F, 0x52, 0x0C, 0xDF, 0xE9,
           0x00, 0x03, 0x59, 0xC0, 0x4F, 0xDF, 0x88, 0x9B,
          },
        },

        /* 800x480 */
        {
          /* 60 Hz */
          {
           0x02, 0x24, 0x7B, 0x63, 0x67, 0xF3, 0xDF, 0xE2,
           0x00, 0x03, 0x41, 0xC0, 0x63, 0xDF, 0x2C, 0x17,
          },
          /* 75 Hz */
          {
           0x6B, 0x63, 0x00, 0x69, 0x1B, 0xF2, 0xDF, 0x00,
           0xE2, 0xE4, 0x1F, 0xC0, 0x63, 0xDF, 0x2C, 0x17,
          },
          /* 85 Hz */
          {
           0x6B, 0x63, 0x00, 0x69, 0x1B, 0xF2, 0xDF, 0x00,
           0xE2, 0xE4, 0x1F, 0xC0, 0x63, 0xDF, 0x2C, 0x17,
          },
        },

        /* 800x600 */
        {
          /* 60 Hz */
          {
           0x04, 0x48, 0x83, 0x63, 0x69, 0x73, 0x57, 0x58,
           0x00, 0x03, 0x7B, 0x20, 0x63, 0x57, 0x0E, 0x05,
          },
          /* 75 Hz */
          {
           0x7F, 0x63, 0x00, 0x66, 0x10, 0x6F, 0x57, 0x00,
           0x58, 0x0B, 0xE0, 0x20, 0x63, 0x57, 0x4C, 0x8B,
          },
          /* 85 Hz */
          {
           0x7E, 0x63, 0x00, 0x68, 0x10, 0x75, 0x57, 0x00,
           0x58, 0x0B, 0xE0, 0x20, 0x63, 0x57, 0x37, 0x87,
          },
        },

         /* 1024x600 */
        {
          /* 60 Hz */
          {
	     0x04, 0x48, 0x95, 0x7F, 0x86, 0x70, 0x57, 0x5B,
	     0x00, 0x60, 0x1c, 0x22, 0x7F, 0x57, 0x16, 0x07,
	    },
          /* 60 Hz */
          {
           0x04, 0x48, 0x95, 0x7F, 0x86, 0x70, 0x57, 0x5B,
	     0x00, 0x60, 0x1c, 0x22, 0x7F, 0x57, 0x16, 0x07,
          },
          /* 60 Hz */
          {
     	     0x04, 0x48, 0x95, 0x7F, 0x86, 0x70, 0x57, 0x5B,
	     0x00, 0x60, 0x1c, 0x22, 0x7F, 0x57, 0x16, 0x07,
          },
        },

        /* 1024x768 */
        {
          /* 60 Hz */
          {
           0x06, 0x68, 0xA7, 0x7F, 0x83, 0x25, 0xFF, 0x02,
           0x00, 0x62, 0x85, 0x20, 0x7F, 0xFF, 0x29, 0x09,
          },
          /* 75 Hz */
          {
           0x9F, 0x7F, 0x00, 0x82, 0x0E, 0x1E, 0xFF, 0x00,
           0x00, 0x03, 0xE5, 0x20, 0x7F, 0xFF, 0x0B, 0x02,
          },
          /* 85 Hz */
          {
           0xA7, 0x7F, 0x00, 0x86, 0x12, 0x26, 0xFF, 0x00,
           0x00, 0x03, 0xE5, 0x20, 0x7F, 0xFF, 0x70, 0x11,
          },
        },

        /* 1280x1024 */
        {
          /* 60 Hz */
          {
           0x08, 0x8C, 0xD5, 0x9F, 0xAB, 0x26, 0xFF, 0x00,
           0x00, 0x03, 0x7E, 0x20, 0x9F, 0xFF, 0x53, 0x0B,
          },
          /* 75 Hz */
          {
           0xCE, 0x9F, 0x00, 0xA2, 0x14, 0x28, 0xFF, 0x00,
           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x42, 0x07,           
          },
          /* 85 Hz */
          {
           0xD3, 0x9F, 0x00, 0xA8, 0x1C, 0x2E, 0xFF, 0x00,
           0x00, 0x03, 0x4A, 0x20, 0x9F, 0xFF, 0x0b, 0x01,           
          },
        },
      };

      if (abs(mode->VRefresh - 60.0) < 5) {
        refresh_rate_index = 0;
      } else if (abs(mode->VRefresh - 75.0) < 5 ){
        refresh_rate_index = 1;
      } else if (abs(mode->VRefresh - 85.0) < 5) {
        refresh_rate_index = 2;
      } else {
        xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Refresh Rate %fHz is not supported\n", mode->VRefresh);
        return (FALSE);
      }
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "Refresh Rate %fHz , index is %d\n", mode->VRefresh, refresh_rate_index);
      for (i = 0; i < 14; i++) {
/*
        new->CR40[i] = ModeTable[refresh_rate_index][modeIndex][i];
*/
        new->CR40[i] = ModeTable[modeIndex][refresh_rate_index][i];
        xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ModeTable[%d][%d][%d] is 0x%x\n", modeIndex, refresh_rate_index, i, new->CR40[i]);
      }

      /* Belcon : clone mode */
      if (pSmi->clone_mode) {
        for (i = 0; i < 14; i++) {
          new->FPR50[i] = ModeTable_2[modeIndex][refresh_rate_index][i];
          xf86DrvMsg(pScrn->scrnIndex, X_INFO, "ModeTable_2[%d][%d][%d] is 0x%x\n", modeIndex, refresh_rate_index, i, new->FPR50[i]);
        }
        new->SR6E = ModeTable_2[modeIndex][refresh_rate_index][14];
        new->SR6F = ModeTable_2[modeIndex][refresh_rate_index][15];
      }

      new->SR6C = ModeTable[modeIndex][refresh_rate_index][14];
      new->SR6D = ModeTable[modeIndex][refresh_rate_index][15];
      xf86DrvMsg (pScrn->scrnIndex, X_ERROR, "SR6C is 0x%x, SR6D is 0x%x\n", new->SR6C, new->SR6D);
      if (panelIndex == 3) {
        new->CR90[14] = 0x18;
      } else {
        new->CR90[14] = 0x00;
      }
      if (modeIndex < 5) {
        new->CR30 &= ~0x09;
      } else {
        new->CR30 |= 0x09;
      }

      if (mode->VDisplay < pSmi->lcdHeight) {
        new->CRA0[6] = (pSmi->lcdHeight - mode->VDisplay) / 8;
      } else {
        new->CRA0[6] = 0;
      }
      if (mode->HDisplay < pSmi->lcdWidth) {
        new->CRA0[7] = (pSmi->lcdWidth - mode->HDisplay) / 16;
      } else {
        new->CRA0[7] = 0;
      }
/*
    }
*/

  }
#endif

  /* CZ 2.11.2001: for gamma correction (TODO: other chipsets?) */
  new->CCR66 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66);
  if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR))
    {
      switch (pScrn->bitsPerPixel)
	{
	case 8:
	  new->CCR66 = (new->CCR66 & 0xF3) | 0x00;	/* 6 bits-RAM */
	  break;
	case 16:
	  new->CCR66 = (new->CCR66 & 0xF3) | 0x00;	/* 6 bits-RAM */
	  /* no Gamma correction in 16 Bit mode (s. Release.txt 1.3.1) */
	  break;
	case 24:
	case 32:
	  new->CCR66 = (new->CCR66 & 0xF3) | 0x04;	/* Gamma correct ON */
	  break;
	default:
	  
	  return (FALSE);
	}
    }

  if (pSmi->Chipset != SMI_COUGAR3DR)
    {
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x30);
      if (inb (pSmi->PIOBase + VGA_SEQ_DATA) & 0x01)
	{
	  new->SR21 = 0x00;
	}
    }

  if (pSmi->MCLK > 0)
    {
      SMI_CommonCalcClock (pScrn->scrnIndex, pSmi->MCLK,
			   1, 1, 31, 0, 2, pSmi->minClock,
			   pSmi->maxClock, &new->SR6A, &new->SR6B);
    }
  else
    {
      new->SR6B = 0xFF;
    }

  if ((mode->HDisplay == 640) && SMI_LYNXM_SERIES (pSmi->Chipset))
    {
      vganew->MiscOutReg &= ~0x0C;
    }
  else
    {
      vganew->MiscOutReg |= 0x0C;
    }
  vganew->MiscOutReg |= 0xE0;
  if (mode->HDisplay == 800)
    {
      vganew->MiscOutReg &= ~0xC0;
    }

  if ((mode->HDisplay == 1024) && SMI_LYNXM_SERIES (pSmi->Chipset))
    {
      vganew->MiscOutReg &= ~0xC0;
    }

  /* Set DPR registers */
  pSmi->Stride = (pSmi->lcdWidth * pSmi->Bpp + 15) & ~15;
  switch (pScrn->bitsPerPixel)
    {
    case 8:
      DEDataFormat = 0x00000000;
      break;

    case 16:
      pSmi->Stride >>= 1;
      DEDataFormat = 0x00100000;
      break;

    case 24:
      DEDataFormat = 0x00300000;
      break;

    case 32:
      pSmi->Stride >>= 2;
      DEDataFormat = 0x00200000;
      break;
    }
  for (i = 0; i < sizeof (xyAddress) / sizeof (xyAddress[0]); i++)
    {
      if (pSmi->rotate && pSmi->rotate != SMI_ROTATE_UD)
	{
	  if (xyAddress[i] == pSmi->lcdHeight)
	    {
	      DEDataFormat |= i << 16;
	      break;
	    }
	}
      else
	{
	  if (xyAddress[i] == pSmi->lcdWidth)
	    {
	      DEDataFormat |= i << 16;
	      break;
	    }
	}
    }
  new->DPR10 = (pSmi->Stride << 16) | pSmi->Stride;
  new->DPR1C = DEDataFormat;
  new->DPR20 = 0;
  new->DPR24 = 0xFFFFFFFF;
  new->DPR28 = 0xFFFFFFFF;
  new->DPR2C = 0;
  new->DPR30 = 0;
  new->DPR3C = (pSmi->Stride << 16) | pSmi->Stride;
  new->DPR40 = 0;
  new->DPR44 = 0;

  /* Set VPR registers (and FPR registers for SM731) */
  switch (pScrn->bitsPerPixel)
    {
    case 8:
      new->VPR00 = 0x00000000;
      new->FPR00_ = 0x00080000;
      break;

    case 16:
      new->VPR00 = 0x00020000;
      new->FPR00_ = 0x000A0000;
      break;

    case 24:
      new->VPR00 = 0x00040000;
      new->FPR00_ = 0x000C0000;
      break;

    case 32:
      new->VPR00 = 0x00030000;
      new->FPR00_ = 0x000B0000;
      break;
    }
  new->VPR0C = pSmi->FBOffset >> 3;
  if (pSmi->rotate && pSmi->rotate != SMI_ROTATE_UD)
    {
      new->VPR10 = ((((pSmi->lcdHeight * pSmi->Bpp) >> 3)
		     + 2) << 16) | ((pSmi->lcdHeight * pSmi->Bpp) >> 3);
    }
  else
    {
      new->VPR10 = ((((pSmi->lcdWidth * pSmi->Bpp) >> 3)
		     + 2) << 16) | ((pSmi->lcdWidth * pSmi->Bpp) >> 3);
    }

  new->FPR0C_ = new->VPR0C;
  new->FPR10_ = new->VPR10;

  /* Set CPR registers */
  new->CPR00 = 0x00000000;

  pScrn->vtSema = TRUE;

  /* Find the INT 10 mode number */
  {
    static struct
    {
      int x, y, bpp;
      CARD16 mode;

    } modeTable[] =
    {
      {
      640, 480, 8, 0x50},
      {
      640, 480, 16, 0x52},
      {
      640, 480, 24, 0x53},
      {
      640, 480, 32, 0x54},
      {
      800, 480, 8, 0x4A},
      {
      800, 480, 16, 0x4C},
      {
      800, 480, 24, 0x4D},
      {
      800, 600, 8, 0x55},
      {
      800, 600, 16, 0x57},
      {
      800, 600, 24, 0x58},
      {
      800, 600, 32, 0x59},
      {
      1024, 768, 8, 0x60},
      {
      1024, 768, 16, 0x62},
      {
      1024, 768, 24, 0x63},
      {
      1024, 768, 32, 0x64},
      {
      1280, 1024, 8, 0x65},
      {
      1280, 1024, 16, 0x67},
      {
      1280, 1024, 24, 0x68},
      {
    1280, 1024, 32, 0x69},};

    new->mode = 0;
 // xf86DrvMsg("", X_INFO, "Belcon: test, %dx%dx%d\n", mode->HDisplay, mode->VDisplay, pScrn->bitsPerPixel);
    for (i = 0; i < sizeof (modeTable) / sizeof (modeTable[0]); i++)
      {
	if ((modeTable[i].x == mode->HDisplay)
	    && (modeTable[i].y == mode->VDisplay)
	    && (modeTable[i].bpp == pScrn->bitsPerPixel))
	  {
	    new->mode = modeTable[i].mode;
	    break;
	  }
      }
  }

  /* Zero the font memory */
  memset (new->smiFont, 0, sizeof (new->smiFont));

  /* Write the mode registers to hardware */
  if (pSmi->useBIOS)	// Use bios call -- by mill.chen
  {
    SMI_BiosWriteMode(pScrn, mode, new);
  }
  else
    SMI_WriteMode (pScrn, vganew, new);

  /* Adjust the viewport */
  SMI_AdjustFrame (pScrn->scrnIndex, pScrn->frameX0, pScrn->frameY0, 0);

  
 // SMI_PrintRegs(pScrn);
  return (TRUE);
}

/*
 * This is called at the end of each server generation.  It restores the
 * original (text) mode.  It should also unmap the video memory, and free any
 * per-generation data allocated by the driver.  It should finish by unwrapping
 * and calling the saved CloseScreen function.
 */

static Bool
SMI_CloseScreen (int scrnIndex, ScreenPtr pScreen)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  vgaRegPtr vgaSavePtr = &hwp->SavedReg;
  SMIRegPtr SMISavePtr = &pSmi->SavedReg;
  Bool ret;

  

  /*
   * Belcon: Fix #001
   * 	Restore hardware cursor register
   */
  if (pSmi->dcrF0)
    WRITE_DCR(pSmi, DCRF0, pSmi->dcrF0);
  if (pSmi->dcr230)
    WRITE_DCR(pSmi, DCR230, pSmi->dcr230);
  /* #001 ended */

  
  if (pScrn->vtSema)
  {
	xf86DrvMsg (pScrn->scrnIndex, X_INFO, "DEBUG:%s %d\n", __FUNCTION__, __LINE__);
	/*
	SMI_WriteMode (pScrn, vgaSavePtr, SMISavePtr);
	vgaHWLock (hwp);
	SMI_UnmapMem (pScrn);
	*/
	SMI_LeaveVT(scrnIndex, 0);/* Teddy:Restore console mode and unmap framebuffer */
  }

  if (pSmi->AccelInfoRec != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      XAADestroyInfoRec (pSmi->AccelInfoRec);
    }
  if (pSmi->CursorInfoRec != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xf86DestroyCursorInfoRec (pSmi->CursorInfoRec);
    }
  if (pSmi->DGAModes != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xfree (pSmi->DGAModes);
    }
  if (pSmi->pInt10 != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xf86FreeInt10 (pSmi->pInt10);
      pSmi->pInt10 = NULL;
    }
  if (pSmi->ptrAdaptor != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xfree (pSmi->ptrAdaptor);
    }
  if (pSmi->BlockHandler != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      pScreen->BlockHandler = pSmi->BlockHandler;
    }
  if (pSmi->I2C != NULL)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xf86DestroyI2CBusRec (pSmi->I2C, FALSE, TRUE);
      xfree (pSmi->I2C);
      pSmi->I2C = NULL;
    }
  /* #670 */
  if (pSmi->pSaveBuffer)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xfree (pSmi->pSaveBuffer);
    }
  /* #920 */
  if (pSmi->paletteBuffer)
    {
      xf86DrvMsg ("", X_INFO, "line %d\n", __LINE__);
      xfree (pSmi->paletteBuffer);
    }

  pScrn->vtSema = FALSE;
  pScreen->CloseScreen = pSmi->CloseScreen;
  ret = (*pScreen->CloseScreen) (scrnIndex, pScreen);

  
  return (ret);
}

static void
SMI_FreeScreen (int scrnIndex, int flags)
{
  SMI_FreeRec (xf86Screens[scrnIndex]);
}

static Bool
SMI_SaveScreen (ScreenPtr pScreen, int mode)
{
  //Bool ret;

  

 //Teddy: ret = vgaHWSaveScreen (pScreen, mode);
  {
  ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
  SMIPtr pSmi = SMIPTR (pScrn);
  }

  
  return (TRUE);
}

void
SMI_AdjustFrame (int scrnIndex, int x, int y, int flags)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  SMIPtr pSmi = SMIPTR (pScrn);
  CARD32 Base;

  

  if (pSmi->ShowCache && y)
    {
      y += pScrn->virtualY - 1;
    }

//xf86DrvMsg("", X_INFO, "pSmi->FBOffset is 0x%x, x is %d, y is %d\n", pSmi->FBOffset, x, y);
 // Base = pSmi->FBOffset + (x + y * pScrn->virtualX) * pSmi->Bpp;
  Base = pSmi->FBOffset + (x + y * pScrn->displayWidth) * pSmi->Bpp;

  if (SMI_LYNX3D_SERIES (pSmi->Chipset) || SMI_COUGAR_SERIES (pSmi->Chipset))
    {
      Base = (Base + 15) & ~15;
#if 1				/* PDR#1058 */
      while ((Base % pSmi->Bpp) > 0)
	{
	  Base -= 16;
	}
#endif
    }
  else if (SMI_MSOC != pSmi->Chipset)
    {
      Base = (Base + 7) & ~7;
#if 1				/* PDR#1058 */
      while ((Base % pSmi->Bpp) > 0)
	{
	  Base -= 8;
	}
#endif
    }

  if (pSmi->Chipset != SMI_MSOC)
    {
      WRITE_VPR (pSmi, 0x0C, Base >> 3);
    }
  else
    {

      if (!pSmi->IsSecondary)
	{
	 // WRITE_DCR (pSmi, DCR0C, 0);
	  WRITE_DCR (pSmi, DCR0C, Base);
#if SMI_DEBUG
	  ErrorF ("LCD Base = %8X\n", 0);
#endif
	}
      else
	{
/* changed by Belcon */
//	  Base = pSmi->videoRAMBytes / 16 << 4;
	//  Base = pSmi->FBOffset / 16 << 4;
         // Base = pScrn->fbOffset / 16 << 4;
#if SMI_DEBUG
	ErrorF ("pScrn->fbOffset = 0x%x\n", pScrn->fbOffset);
	  ErrorF ("CRT Base = %8X\n", Base);
#endif
	  WRITE_DCR (pSmi, DCR204, Base);
	}
#if SMI_DEBUG
          ErrorF ("FBOffset is 0x%x\n", pSmi->FBOffset);
#endif

    }


  if (pSmi->Chipset == SMI_COUGAR3DR)
    {
      WRITE_FPR (pSmi, FPR0C, Base >> 3);
    }

  
}


Bool
SMI_SwitchMode (int scrnIndex, DisplayModePtr mode, int flags)
{
  Bool ret;
  SMIPtr pSmi = SMIPTR (xf86Screens[scrnIndex]);

  

  ret = SMI_ModeInit (xf86Screens[scrnIndex], mode);
  if (!pSmi->NoAccel)
    SMI_EngineReset (xf86Screens[scrnIndex]);
  
  return (ret);
}

void
SMI_LoadPalette (ScrnInfoPtr pScrn, int numColors, int *indicies,
		 LOCO * colors, VisualPtr pVisual)
{
  SMIPtr pSmi = SMIPTR (pScrn);
  int i;
  int iRGB;

  



  /* Enable both the CRT and LCD DAC RAM paths, so both palettes are updated */
  if ((pSmi->Chipset == SMI_LYNX3DM) || (pSmi->Chipset == SMI_COUGAR3DR))
    {
      CARD8 ccr66;

      ccr66 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66);
      ccr66 &= 0x0f;
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x66, ccr66);
    }

  if (SMI_MSOC != pSmi->Chipset)
    {
      for (i = 0; i < numColors; i++)
	{
	  DEBUG ((VERBLEV, "pal[%d] = %d %d %d\n", indicies[i],
		  colors[indicies[i]].red, colors[indicies[i]].green,
		  colors[indicies[i]].blue));
	  VGAOUT8 (pSmi, VGA_DAC_WRITE_ADDR, indicies[i]);
	  VGAOUT8 (pSmi, VGA_DAC_DATA, colors[indicies[i]].red);
	  VGAOUT8 (pSmi, VGA_DAC_DATA, colors[indicies[i]].green);
	  VGAOUT8 (pSmi, VGA_DAC_DATA, colors[indicies[i]].blue);
	}
    }
  else				/* For SMI 501 Palette control */
    {
		if(pScrn->bitsPerPixel == 16) {
		CARD16	lut_r[256],lut_g[256],lut_b[256];
		int	idx, j;

		/* Expand the RGB 565 palette into the 256-elements LUT */
		for (i = 0; i < numColors; i++) {
			idx = indicies[i];

			if (idx < 32) {
				for (j = 0; j < 8; j++) {
					lut_r[idx * 8 + j] = colors[idx].red;
					lut_b[idx * 8 + j] = colors[idx].blue;
				}
			}
			for (j = 0; j < 4; j++)
				lut_g[idx * 4 + j] = colors[idx].green;
		}
		for (i = 0; i < 256; i++) {
			iRGB = (lut_r[i] << 16) | (lut_g[i] << 8) | lut_b[i];
			if (!pSmi->IsSecondary) {
				/* CRT Palette */
				WRITE_DCR (pSmi, DCR400 + (i << 2), iRGB);
			} else {
				/* Panel Palette */
				WRITE_DCR (pSmi, DCR800 + (i << 2), iRGB);
			}
		} 
		
		LEAVE_PROC ("SMI_LoadPalette");
		return;
	}
      for (i = 0; i < numColors; i++)
	{
	  DEBUG ((VERBLEV, "pal[%d] = %d %d %d\n", indicies[i],
		  colors[indicies[i]].red, colors[indicies[i]].green,
		  colors[indicies[i]].blue));

	  iRGB = (colors[indicies[i]].red << 16) |
	    (colors[indicies[i]].green << 8) | (colors[indicies[i]].blue);

	  if (!pSmi->IsSecondary)
	    {
	      WRITE_DCR (pSmi, DCR400 + (4 * (indicies[i])), iRGB);	/* CRT Palette   */
	    }
	  else
	    WRITE_DCR (pSmi, DCR800 + (4 * (indicies[i])), iRGB);	/* Panel Palette */
	}

    }


  
}

static void
SMI_DisableVideo (ScrnInfoPtr pScrn)
{
  SMIPtr pSmi = SMIPTR (pScrn);
  CARD8 tmp;
  int rVal;

  if (SMI_MSOC != pSmi->Chipset)
    {
      if (!(tmp = VGAIN8 (pSmi, VGA_DAC_MASK)))
	return;
      pSmi->DACmask = tmp;
      VGAOUT8 (pSmi, VGA_DAC_MASK, 0);
    }
  else
    {
      rVal = READ_DCR (pSmi, DCR200);
      rVal |= DCR200_CRT_BLANK;
      WRITE_DCR (pSmi, DCR200, rVal);
    }
}

static void
SMI_EnableVideo (ScrnInfoPtr pScrn)
{
  SMIPtr pSmi = SMIPTR (pScrn);
  int rVal;

  if (SMI_MSOC != pSmi->Chipset)
    {
      VGAOUT8 (pSmi, VGA_DAC_MASK, pSmi->DACmask);
    }
  else
    {
      rVal = READ_DCR (pSmi, DCR200);
      rVal &= ~DCR200_CRT_BLANK;
      WRITE_DCR (pSmi, DCR200, rVal);
    }
}


void
SMI_EnableMmio (ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  CARD8 tmp;

  

  if (SMI_MSOC != pSmi->Chipset)
    {
      /*
       * Enable chipset (seen on uninitialized secondary cards) might not be
       * needed once we use the VGA softbooter
       */
      vgaHWSetStdFuncs (hwp);

      /* Enable linear mode */
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
      tmp = inb (pSmi->PIOBase + VGA_SEQ_DATA);
      pSmi->SR18Value = tmp;	/* PDR#521 */
/* Debug */
      if (-1 == saved_console_reg) {
// xf86DrvMsg(pScrn->scrnIndex, X_INFO, "SMI_EnableMmio(), Set save->SR18 value: 0x%x\n", tmp);
        (pSmi->SavedReg).SR18 = tmp;
        saved_console_reg = 0;
      }
/*
      xf86DrvMsg (pScrn->scrnIndex, X_NOTICE,
		  " Enable pSmi->SR18Value:%02X, tmp is %02X\n", pSmi->SR18Value, tmp);
*/
      outb (pSmi->PIOBase + VGA_SEQ_DATA, tmp | 0x11);

      /* Enable 2D/3D Engine and Video Processor */
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x21);
      tmp = inb (pSmi->PIOBase + VGA_SEQ_DATA);
      pSmi->SR21Value = tmp;	/* PDR#521 */
      xf86DrvMsg (pScrn->scrnIndex, X_NOTICE,
		  " Enable pSmi->SR21Value:%02X\n", pSmi->SR21Value);
      outb (pSmi->PIOBase + VGA_SEQ_DATA, tmp & ~0x03);

    }

  
}

void
SMI_DisableMmio (ScrnInfoPtr pScrn)
{
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);

  

  if (SMI_MSOC != pSmi->Chipset)
    {
      vgaHWSetStdFuncs (hwp);

      /* Disable 2D/3D Engine and Video Processor */
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x21);
      outb (pSmi->PIOBase + VGA_SEQ_DATA, pSmi->SR21Value & (~0x80));	/* PDR#521 */

      /* Disable linear mode */
      outb (pSmi->PIOBase + VGA_SEQ_INDEX, 0x18);
//      outb (pSmi->PIOBase + VGA_SEQ_DATA, pSmi->SR18Value);	/* PDR#521 */
      outb (pSmi->PIOBase + VGA_SEQ_DATA, (pSmi->SavedReg).SR18);	/* PDR#521 */
/*
      xf86DrvMsg (pScrn->scrnIndex, X_NOTICE,
		  " Disable pSmi->SR18Value:%02X %02X\n", pSmi->SR18Value, (pSmi->SavedReg).SR18);
*/
    }

  
}

/* This function is used to debug, it prints out the contents of Lynx regs */

static void
SMI_PrintRegs (ScrnInfoPtr pScrn)
{
  unsigned char i, tmp;
  unsigned int j;
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  int vgaCRIndex = hwp->IOBase + VGA_CRTC_INDEX_OFFSET;
  int vgaCRReg = hwp->IOBase + VGA_CRTC_DATA_OFFSET;
  int vgaStatus = hwp->IOBase + VGA_IN_STAT_1_OFFSET;


  if (SMI_MSOC != pSmi->Chipset)
    {

      xf86ErrorFVerb (VERBLEV, "\nSEQUENCER\n"
		      "    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
      for (i = 0x00; i <= 0xAF; i++)
	{
	  if ((i & 0xF) == 0x0)
	    xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
	  if ((i & 0x3) == 0x0)
	    xf86ErrorFVerb (VERBLEV, " ");
	  xf86ErrorFVerb (VERBLEV, "%02X ",
			  VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA,
					i));
	}

      xf86ErrorFVerb (VERBLEV, "\n\nCRT CONTROLLER\n"
		      "    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
      for (i = 0x00; i <= 0xAD; i++)
	{
	  if (i == 0x20)
	    i = 0x30;
	  if (i == 0x50)
#if SMI_DEBUG
	    i = 0x70;
	  if (i == 0x80)
#endif
	    i = 0x90;
	  if ((i & 0xF) == 0x0)
	    xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
	  if ((i & 0x3) == 0x0)
	    xf86ErrorFVerb (VERBLEV, " ");
	  xf86ErrorFVerb (VERBLEV, "%02X ",
			  VGAIN8_INDEX (pSmi, vgaCRIndex, vgaCRReg, i));
	}

      xf86ErrorFVerb (VERBLEV, "\n\nGRAPHICS CONTROLLER\n"
		      "    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
      for (i = 0x00; i <= 0x08; i++)
	{
	  if ((i & 0xF) == 0x0)
	    xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
	  if ((i & 0x3) == 0x0)
	    xf86ErrorFVerb (VERBLEV, " ");
	  xf86ErrorFVerb (VERBLEV, "%02X ",
			  VGAIN8_INDEX (pSmi, VGA_GRAPH_INDEX, VGA_GRAPH_DATA,
					i));
	}

      xf86ErrorFVerb (VERBLEV, "\n\nATTRIBUTE 0CONTROLLER\n"
		      "    x0 x1 x2 x3  x4 x5 x6 x7  x8 x9 xA xB  xC xD xE xF");
      for (i = 0x00; i <= 0x14; i++)
	{
	  (void) VGAIN8 (pSmi, vgaStatus);
	  if ((i & 0xF) == 0x0)
	    xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
	  if ((i & 0x3) == 0x0)
	    xf86ErrorFVerb (VERBLEV, " ");
	  xf86ErrorFVerb (VERBLEV, "%02X ",
			  VGAIN8_INDEX (pSmi, VGA_ATTR_INDEX, VGA_ATTR_DATA_R,
					i));
	}
      (void) VGAIN8 (pSmi, vgaStatus);
      VGAOUT8 (pSmi, VGA_ATTR_INDEX, 0x20);
    }

  xf86ErrorFVerb (VERBLEV, "\n\nDPR    x0       x4       x8       xC");
  for (i = 0x00; i <= 0x44; i += 4)
    {
      if ((i & 0xF) == 0x0)
	xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
      xf86ErrorFVerb (VERBLEV, " %08lX", (unsigned long) READ_DPR (pSmi, i));
    }

  xf86ErrorFVerb (VERBLEV, "\n\nVPR    x0       x4       x8       xC");
  for (i = 0x00; i <= 0x60; i += 4)
    {
      if ((i & 0xF) == 0x0)
	xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
      xf86ErrorFVerb (VERBLEV, " %08lX", (unsigned long) READ_VPR (pSmi, i));

    }

  xf86ErrorFVerb (VERBLEV, "\n\nCPR    x0       x4       x8       xC");
  for (i = 0x00; i <= 0x18; i += 4)
    {
      if ((i & 0xF) == 0x0)
	xf86ErrorFVerb (VERBLEV, "\n%02X|", i);
      xf86ErrorFVerb (VERBLEV, " %08lX", (unsigned long) READ_CPR (pSmi, i));
    }

  if (SMI_MSOC == pSmi->Chipset)
    {
      xf86ErrorFVerb (VERBLEV, "\n\nSCR    x0       x4       x8       xC");
      for (j = 0x00; j <= 0x6C; j += 4)
	{
	  if ((j & 0xF) == 0x0)
	    xf86ErrorFVerb (VERBLEV, "\n%02X|", j);
	  xf86ErrorFVerb (VERBLEV, " %08X", READ_SCR (pSmi, j));
	}
      xf86ErrorFVerb (VERBLEV, "\n\nDCR    x0       x4       x8       xC");
      for (j = 0x200; j <= 0x23C; j += 4)
	{
	  if ((j & 0xF) == 0x0)
	    xf86ErrorFVerb (VERBLEV, "\n%02X|", j);
	  xf86ErrorFVerb (VERBLEV, " %08X", READ_DCR (pSmi, j));
	}
    }

  xf86ErrorFVerb (VERBLEV, "\n\n");
  xf86DrvMsgVerb (pScrn->scrnIndex, X_INFO, VERBLEV,
		  "END register dump --------------------\n");
}

/*
 * SMI_DisplayPowerManagementSet -- Sets VESA Display Power Management
 * Signaling (DPMS) Mode.
 */
static void
SMI_DisplayPowerManagementSet (ScrnInfoPtr pScrn, int PowerManagementMode,
			       int flags)
{
  vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  CARD8 SR01, SR20, SR21, SR22, SR23, SR24, SR31, SR34;

xf86DrvMsg("", X_INFO, "Belcon::PowerManagementMode is %d\n", PowerManagementMode);
  /* If we already are in the requested DPMS mode, just return */
  if (pSmi->CurrentDPMS == PowerManagementMode)
    {
      
      return;
    }


  if (SMI_MSOC != pSmi->Chipset)
    {

#if 0				/* PDR#735 */
      if (pSmi->pInt10 != NULL)
	{
	  pSmi->pInt10->ax = 0x4F10;
	  switch (PowerManagementMode)
	    {
	    case DPMSModeOn:
	      pSmi->pInt10->bx = 0x0001;
	      break;

	    case DPMSModeStandby:
	      pSmi->pInt10->bx = 0x0101;
	      break;

	    case DPMSModeSuspend:
	      pSmi->pInt10->bx = 0x0201;
	      break;

	    case DPMSModeOff:
	      pSmi->pInt10->bx = 0x0401;
	      break;
	    }
	  pSmi->pInt10->cx = 0x0000;
	  pSmi->pInt10->num = 0x10;
	  xf86DrvMsg (pScrn->scrnIndex, X_NOTICE,
		      "PowerManagementMode:%d\n", PowerManagementMode);

	  xf86ExecX86int10 (pSmi->pInt10);

	  xf86DrvMsg (pScrn->scrnIndex, X_NOTICE,
		      "pSmi->pInt10->ax:%d\n", pSmi->pInt10->ax);

	  if (pSmi->pInt10->ax == 0x004F)
	    {
	      pSmi->CurrentDPMS = PowerManagementMode;
#if 1				/* PDR#835 */
	      if (PowerManagementMode == DPMSModeOn)
		{
		  SR01 =
		    VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01);
		  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01,
				 SR01 & ~0x20);
		}
#endif
	      
	      return;
	    }
	}
#else

      /* Save the current SR registers */
      if (pSmi->CurrentDPMS == DPMSModeOn)
	{
	  pSmi->DPMS_SR20 =
	    VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x20);
	  pSmi->DPMS_SR21 =
	    VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
	  pSmi->DPMS_SR31 =
	    VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
	  pSmi->DPMS_SR34 =
	    VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x34);
	}

      /* Read the required SR registers for the DPMS handler */
      SR01 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01);
      SR20 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x20);
      SR21 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21);
      SR22 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x22);
      SR23 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x23);
      SR24 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x24);
      SR31 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31);
      SR34 = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x34);

      switch (PowerManagementMode)
	{
	case DPMSModeOn:
	  /* Screen On: HSync: On, VSync : On */
	  SR01 &= ~0x20;
	  SR20 = pSmi->DPMS_SR20;
	  SR21 = pSmi->DPMS_SR21;
	  SR22 &= ~0x30;
	  SR23 &= ~0xC0;
	  SR24 |= 0x01;
	  SR31 = pSmi->DPMS_SR31;
	  SR34 = pSmi->DPMS_SR34;
	  break;

	case DPMSModeStandby:
	  /* Screen: Off; HSync: Off, VSync: On */
	  SR01 |= 0x20;
	  SR20 = (SR20 & ~0xB0) | 0x10;
	  SR21 |= 0x88;
	  SR22 = (SR22 & ~0x30) | 0x10;
	  SR23 = (SR23 & ~0x07) | 0xD8;
	  SR24 &= ~0x01;
	  SR31 = (SR31 & ~0x07) | 0x00;
	  SR34 |= 0x80;
	  break;

	case DPMSModeSuspend:
	  /* Screen: Off; HSync: On, VSync: Off */
	  SR01 |= 0x20;
	  SR20 = (SR20 & ~0xB0) | 0x10;
	  SR21 |= 0x88;
	  SR22 = (SR22 & ~0x30) | 0x20;
	  SR23 = (SR23 & ~0x07) | 0xD8;
	  SR24 &= ~0x01;
	  SR31 = (SR31 & ~0x07) | 0x00;
	  SR34 |= 0x80;
	  break;

	case DPMSModeOff:
	  /* Screen: Off; HSync: Off, VSync: Off */
	  SR01 |= 0x20;
	  SR20 = (SR20 & ~0xB0) | 0x10;
	  SR21 |= 0x88;
	  SR22 = (SR22 & ~0x30) | 0x30;
	  SR23 = (SR23 & ~0x07) | 0xD8;
	  SR24 &= ~0x01;
	  SR31 = (SR31 & ~0x07) | 0x00;
	  SR34 |= 0x80;
	  break;

	default:
	  xf86ErrorFVerb (VERBLEV, "Invalid PowerManagementMode %d passed to "
			  "SMI_DisplayPowerManagementSet\n",
			  PowerManagementMode);
	  
	  return;
	}

      xf86DrvMsg (pScrn->scrnIndex, X_NOTICE,
		  "PowerManagementMode:%d\n", PowerManagementMode);

      /* Wait for vertical retrace */
      while (hwp->readST01 (hwp) & 0x8);
      while (!(hwp->readST01 (hwp) & 0x8));

      /* Write the registers */
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x01, SR01);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x34, SR34);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x31, SR31);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x20, SR20);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x22, SR22);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x23, SR23);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x21, SR21);
      VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x24, SR24);
#endif

    }
  else
    {

      setDPMS (pSmi, PowerManagementMode);

      switch (PowerManagementMode)
	{
	case DPMSModeOn:
	  setPower (pSmi, 0, 0, 0);
	  regWrite32 (pSmi, 0x10004, 0x180002);  //backlight on
	  break;

	case DPMSModeStandby:
	  xf86DrvMsg ("", X_NOTICE, "DPMSModeOn start\n ");
	  setPower (pSmi, 0, 0, 0);
	  xf86DrvMsg ("", X_NOTICE, "DPMSModeOn stop\n ");
	  break;

	case DPMSModeSuspend:
	  setPower (pSmi, 0, 0, 0);
	  break;

	case DPMSModeOff:
	  setPower (pSmi, 0, 0, 0);
	  break;
	default:
	  break;

	}
    }
  /* Save the current power state */
  pSmi->CurrentDPMS = PowerManagementMode;

  
}

static void
SMI_ProbeDDC (ScrnInfoPtr pScrn, int index)
{
  vbeInfoPtr pVbe;
  
  if (xf86LoadSubModule (pScrn, "vbe"))
    {
      pVbe = VBEInit (NULL, index);
      ConfiguredMonitor = vbeDoEDID (pVbe, NULL);
      vbeFree (pVbe);
    }
  
}

static unsigned int
SMI_ddc1Read (ScrnInfoPtr pScrn)
{
  register vgaHWPtr hwp = VGAHWPTR (pScrn);
  SMIPtr pSmi = SMIPTR (pScrn);
  unsigned int ret;

  

  while (hwp->readST01 (hwp) & 0x8);
  while (!(hwp->readST01 (hwp) & 0x8));

  ret = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72) & 0x08;

  
  return (ret);
}

static Bool
SMI_ddc1 (int scrnIndex)
{
  ScrnInfoPtr pScrn = xf86Screens[scrnIndex];
  SMIPtr pSmi = SMIPTR (pScrn);
  Bool success = FALSE;
  xf86MonPtr pMon;
  unsigned char tmp;

  

  tmp = VGAIN8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72);
  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72, tmp | 0x20);

  pMon = xf86PrintEDID (xf86DoEDID_DDC1 (scrnIndex,
// #if XF86_VERSION_CURRENT >= XF86_VERSION_NUMERIC(6,9,0,0,0)
#if XORG_VERSION_CURRENT >= XORG_VERSION_NUMERIC(6,9,0,0,0)
					 vgaHWddc1SetSpeedWeak (),
#elif XORG_VERSION_CURRENT == XORG_VERSION_NUMERIC(1,3,0,0,0)
	 vgaHWddc1SetSpeedWeak (),
#else
					 vgaHWddc1SetSpeed,
#endif
					 SMI_ddc1Read));

  if (pMon != NULL)
    {
      success = TRUE;
    }
  xf86SetDDCproperties (pScrn, pMon);

  VGAOUT8_INDEX (pSmi, VGA_SEQ_INDEX, VGA_SEQ_DATA, 0x72, tmp);

  
  return (success);
}


static Bool
SMI_MSOCSetMode (ScrnInfoPtr pScrn, DisplayModePtr mode)
{
  SMIPtr pSmi = SMIPTR (pScrn);

  

/* Remarked by Belcon */
//      mode->VRefresh = 60;
  /*boyod */
/*
 xf86DrvMsg("", X_INFO, "Belcon: Hdisplay and Vdisplay is %d, %d\n", mode->HDisplay, mode->VDisplay);
 xf86DrvMsg("", X_INFO, "Belcon: virtualX is %d, virtualY is %d\n", pScrn->virtualX, pScrn->virtualY);
*/

  if (!pSmi->IsSecondary)
    {
// xf86DrvMsg("", X_INFO, "Belcon: SMI_MSOCSetMode(), nHertz is %f\n", mode->VRefresh);
      panelSetMode (pSmi, mode->HDisplay, mode->VDisplay, 0, 60, pSmi->Stride,
		    pScrn->depth);
      panelUseLCD (pSmi, TRUE);
    }
  else
    {
//       xf86DrvMsg("", X_INFO, "Belcon: SMI_MSOCSetMode(), before crtSetMode(), %d x %d, nHertz is %f, pitch is %d, depth is %d\n", mode->HDisplay, mode->VDisplay, mode->VRefresh, pSmi->Stride, pScrn->depth);
      crtSetMode (pSmi, mode->HDisplay, mode->VDisplay, 0,
		  (int) (mode->VRefresh + 0.5), pSmi->Stride, pScrn->depth);
      panelUseCRT (pSmi, TRUE);
    }


  /*boyod */

  

  return (TRUE);

}
void SMI_SaveReg_502(ScrnInfoPtr pScrn )
{
	return;
}

void SMI_RestoreReg_502(ScrnInfoPtr pScrn )
{
	return;
}

#ifdef RANDR
static Bool
SMI_RandRGetInfo(ScrnInfoPtr pScrn, Rotation *rotations)
{
    SMIPtr pSmi = SMIPTR (pScrn); 
    

    if(pSmi->RandRRotation)
       *rotations = RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270;
    else
       *rotations = RR_Rotate_0;
    
    return TRUE;
}

static Bool
SMI_RandRSetConfig(ScrnInfoPtr pScrn, xorgRRConfig *config)
{
    SMIPtr pSmi = SMIPTR (pScrn);
	int bytesPerPixel = pScrn->bitsPerPixel / 8;
		
    
    switch(config->rotation) {
        case RR_Rotate_0:
            pSmi->rotate = SMI_ROTATE_ZERO;
            pScrn->PointerMoved = pSmi->PointerMoved;
            break;

        case RR_Rotate_90:
            pSmi->rotate = SMI_ROTATE_CW;
            pScrn->PointerMoved = SMI_PointerMoved;
			pSmi->height= pSmi->ShadowHeight = pScrn->virtualX;
			pSmi->width = pSmi->ShadowWidth= pScrn->virtualY;
			pSmi->ShadowWidthBytes  = (pSmi->ShadowWidth * bytesPerPixel + 15) & ~15;
            break;

        case RR_Rotate_180:
            pSmi->rotate = SMI_ROTATE_UD;
            pScrn->PointerMoved = SMI_PointerMoved;
			pSmi->height= pSmi->ShadowHeight = pScrn->virtualY;
			pSmi->width = pSmi->ShadowWidth= pScrn->virtualX;
			pSmi->ShadowWidthBytes  = (pSmi->ShadowWidth * bytesPerPixel + 15) & ~15;			
            break;						

        case RR_Rotate_270:
            pSmi->rotate = SMI_ROTATE_CCW;
            pScrn->PointerMoved = SMI_PointerMoved;
			pSmi->height= pSmi->ShadowHeight = pScrn->virtualX;
			pSmi->width = pSmi->ShadowWidth= pScrn->virtualY;
			pSmi->ShadowWidthBytes  = (pSmi->ShadowWidth * bytesPerPixel + 15) & ~15;
            break;

        default:
            xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
                    "Unexpected rotation in SMI_RandRSetConfig!\n");
            pSmi->rotate = 0;
            pScrn->PointerMoved = pSmi->PointerMoved;
            return FALSE;
    }  	
    
    return TRUE;
}

static Bool
SMI_DriverFunc(ScrnInfoPtr pScrn, xorgDriverFuncOp op, pointer data)
{
    
    switch(op) {
       case RR_GET_INFO:
          
          return SMI_RandRGetInfo(pScrn, (Rotation*)data);
       case RR_SET_CONFIG:
          
          return SMI_RandRSetConfig(pScrn, (xorgRRConfig*)data);
       default:
          
          return FALSE;
    }
    
    return FALSE;
}
#endif
