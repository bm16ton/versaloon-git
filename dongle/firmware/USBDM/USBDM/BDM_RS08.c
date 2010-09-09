/*! \file
    \brief USBDM - RS08 low level BDM communication.

   \verbatim
   This software was modified from \e TBDML software

   USBDM
   Copyright (C) 2007  Peter O'Donoghue

   Turbo BDM Light (TBDML)
   Copyright (C) 2005  Daniel Malik

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
   \endverbatim

   \verbatim
   Change History
   +========================================================================================
   |    Apr 2010 | All significant RS08 code is now in USBDM.dll (only Vpp control remains)
   |    Feb 2010 | Greatly simplified for Version 3 USBDM - most code now in USBDM.dll - pgo
   +========================================================================================
   \endverbatim
*/

#if TARGET_HARDWARE!=H_VERSALOON
#include <hidef.h>          /* common defines and macros */
#endif
#include "Derivative.h"
#include "Common.h"
#include "Configure.h"
#include "Commands.h"
#include "bdm.h"
#include "bdmMacros.h"
#include "TargetDefines.h"
#include "CmdProcessing.h"
#include "BDMCommon.h"
#include "BDM_RS08.h"

#if ((CAPABILITY & CAP_FLASH) != 0)

//========================================================
//! Control target VPP level
//!
//! @param level (FlashState_t) control value for VPP \n
//!
//! @return
//!     BDM_RC_OK   => Success \n
//!     else        => Error
//!
U8 bdmSetVpp(U8 level ) {

   // Safety check - don't turn on Vpp for wrong target
   if ((cable_status.target_type != T_RS08) && (commandBuffer[2] != BDM_TARGET_VPP_OFF)) {
      VPP_OFF();
      return BDM_RC_ILLEGAL_COMMAND;
   }

   switch (level) {
      case BDM_TARGET_VPP_OFF      :  // Flash programming voltage off
         VPP_OFF();
         FLASH12V_OFF();
         cable_status.flashState = BDM_TARGET_VPP_OFF;
         break;

      case BDM_TARGET_VPP_STANDBY  :  // Flash programming voltage off (inverter on)
         VPP_OFF();
         FLASH12V_ON();
         cable_status.flashState = BDM_TARGET_VPP_STANDBY;
         break;

      case BDM_TARGET_VPP_ON       :  // Flash programming voltage on
         // Must already be in standby
         if (cable_status.flashState != BDM_TARGET_VPP_STANDBY)
            return BDM_RC_ILLEGAL_PARAMS;
         
         // Must have Vdd BEFORE Vpp
         if ((cable_status.power != BDM_TARGET_VDD_EXT) &&
             (cable_status.power != BDM_TARGET_VDD_INT))
            return BDM_RC_VDD_NOT_PRESENT;
            
         VPP_ON();
         cable_status.flashState = BDM_TARGET_VPP_ON;
         break;
         
      default :
         return BDM_RC_ILLEGAL_PARAMS;
   }
   return BDM_RC_OK;
}

#endif
