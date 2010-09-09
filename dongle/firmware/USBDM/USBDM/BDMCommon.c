/*! \file
    \brief USBDM - Common BDM routines.

   \verbatim
   This software was modified from \e TBLCF software
   This software was modified from \e TBDML software

   USBDM
   Copyright (C) 2007  Peter O'Donoghue

   Turbo BDM Light
   Copyright (C) 2005  Daniel Malik

   Turbo BDM Light ColdFire
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
   +=======================================================================================
   | 10 Apr 2010 | Changed to accommodate changes to Vpp interface                    - pgo 
   |  5 Feb 2010 | bdm_cycleTargetVdd() now disables Vdd monitoring                   - pgo
   |  4 Feb 2010 | bdm_cycleTargetVdd() parametised for mode                          - pgo
   | 19 Oct 2009 | Modified Timer code - Folder together with JS16 code               - pgo
   | 20 Sep 2009 | Increased Reset wait                                               - pgo
   |    Sep 2009 | Major changes for V2                                               - pgo
   +=======================================================================================

   ToDo / Issues
   +=========================================================================
   |  * The use of LVC45 driver ICs produces glitches and/or bus conflicts.  
   |    These are unavoidable unless external resistors are used.             
   +=========================================================================
   +=========================================================================
   |  There are still some issues with the timers on HCS12. Changing prescale
   |  does not have immediate effect (or even in a predicatable manner!)
   +=========================================================================

   \endverbatim
*/

#if TARGET_HARDWARE!=H_VERSALOON
#include <hidef.h> /* for EnableInterrupts macro */
#endif
#include "Derivative.h"
#include "Common.h"
#include "Configure.h"
#include "Commands.h"
#include "BDMCommon.h"
#include "BDM.H"
#include "BDM_CF.H"
#include "BDM_RS08.H"
#include "CmdProcessing.h"


//=============================================================================================================================================
#define SOFT_RESET       1280U //!< us - longest time needed for soft reset of the BDM interface (512 BDM cycles @ 400kHz = 1280us)

//! Status of the BDM
//!
static const CableStatus_t cable_statusDefault =  {
   T_OFF,             // target_type
   WAIT,              // ackn
   NO_RESET_ACTIVITY, // reset
   SPEED_NO_INFO,     // speed
   0,                 // power
   0,                 // wait150_cnt
   0,                 // wait64_cnt
   0,                 // sync_length
};

#if TARGET_HARDWARE!=H_VERSALOON
//=========================================================================
// Timer routines
//
//=========================================================================
#if (SLOW_TIMER_PRESCALE_MASK >= 8)
#error "SLOW_TIMER_PRESCALE_MASK value out of range"
#endif

//! Wait for given time in fast timer ticks
//!
//!  @param delay Delay time in fast timer ticks
//!
void fastTimerWait(U16 delay){
   TPMSC                = FAST_TPMxSC_VALUE;          // Set timer tick rate
   TPMCNTH              = 0;                          // Restart timer counter
   TIMEOUT_TPMxCnSC_CHF = 0;                          // Clear timeout flag
   TIMEOUT_TPMxCnVALUE  = delay;                      // Set Timeout value
   while (TIMEOUT_TPMxCnSC_CHF==0){                   // Wait for  timeout
   }
   TPMSC                = ACKN_TPMxSC_VALUE;          // Restore default tick rate
}

//! Wait for given time in slow timer ticks
//!
//!  @param delay Delay time in slow timer ticks
//!
void slowTimerWait(U16 delay){
   TPMSC                = SLOW_TPMxSC_VALUE;          // Set timer tick rate
   TPMCNTH              = 0;                          // Restart timer counter
   TIMEOUT_TPMxCnSC_CHF = 0;                          // Clear timeout flag
   TIMEOUT_TPMxCnVALUE  = delay;                      // Set Timeout value
   while (TIMEOUT_TPMxCnSC_CHF==0){                   // Wait for  timeout
   }
   TPMSC                = ACKN_TPMxSC_VALUE;          // Restore default tick rate
}
#endif

//! Initialises the timers, input captures and interrupts
//!
U8 initTimers(void) {

#if TARGET_HARDWARE!=H_VERSALOON
   //====================================================================
   // Set up timers
   TPMSC      = 0;                     // CLKSB/A = 00 for immediate modulo change
   TPMSC      = ACKN_TPMxSC_VALUE;     // Set timer tick rate

   // Set up Input capture & timeout timers
   TIMEOUT_TPMxCnSC    = TIMEOUT_TPMxCnSC_OC_MASK;         // TPMx.CHa : Output compare no pin
   SYNC_TPMxCnSC       = SYNC_TPMxCnSC_FALLING_EDGE_MASK;  // TPMx.CHb : Input capture, falling edge on pin
#endif

#if (CAPABILITY&CAP_VDDSENSE)
   //====================================================================
   // Set up Vdd monitoring (ACMP interrupts or Input capture)
   // Clear existing flag, enable falling & rising transitions (Vdd rising & falling!), enable Bandgap (~1.2V)
   CONFIGURE_VDD_SENSE();         // Capture Vdd rising & falling edges
   CLEAR_VDD_SENSE_FLAG();        // Clear Vdd Change Event
   ENABLE_VDD_SENSE_INT();        // Enable Vdd IC interrupts
#endif

#if (CAPABILITY&CAP_RESET)
   //===================================================================
   // Setup RESET detection (Input Capture or keyboard interrupt)
   if (bdm_option.useResetSignal) {
      CONFIGURE_RESET_SENSE();         // Capture RESET falling edges
      CLEAR_RESET_SENSE_FLAG();        // Clear RESET IC Event
      ENABLE_RESET_SENSE_INT();        // Enable RESET IC interrupts
      }
   else
      DISABLE_RESET_SENSE_INT();       // Disable RESET IC interrupts
#endif

   cable_status.reset = NO_RESET_ACTIVITY;  // Clear the reset detection flag

   return BDM_RC_OK;
}

#if TARGET_HARDWARE!=H_VERSALOON
//=========================================================================
// Target monitoring and status routines
//
//=========================================================================

//! Interrupt function servicing the IC interrupt from Vdd changes
//! This routine has several purposes:
//!  - Triggers POR into Debug mode on targets\n
//!  - Turns off Target power on short circuits\n
//!  - Updates Target power status\n
//!
interrupt
void bdm_targetVddSense(void) {

#if (CAPABILITY&CAP_VDDSENSE)
   CLEAR_VDD_SENSE_FLAG(); // Clear Vdd Change Event

   if (VDD_SENSE) {  // Vdd rising
      // Needs to be done on non-interrupt thread?
#if (CAPABILITY&CAP_CFVx)
      if  (cable_status.target_type == T_CFVx)
         bdmCF_powerOnReset();
      else
#endif // (CAPABILITY&CAP_CFVx)
         (void)bdmHCS_powerOnReset();
      bdm_checkTargetVdd();
      }
   else { // Vdd falling
      VDD_OFF();   // Turn off Vdd in case it's an overload
#if (CAPABILITY&CAP_CFVx)
      if  (cable_status.target_type == T_CFVx)
          bdmcf_interfaceIdle();  // Make sure BDM interface is idle
      else
#endif // (CAPABILITY&CAP_CFVx)
      {
          bdmHCS_interfaceIdle();  // Make sure BDM interface is idle

          // BKGD pin=L, RESET pin=L
          DATA_PORT      = BDM_DIR_Rx_WR;
          DATA_PORT_DDR  = BDM_DIR_Rx_MASK|BDM_OUT_MASK;
          RESET_LOW();
      }
      bdm_checkTargetVdd();
      }
#endif // CAP_VDDSENSE
}

//! Interrupt function servicing the IC interrupt from RESET_IN assertion
//!
interrupt
void bdm_resetSense(void) {
   CLEAR_RESET_SENSE_FLAG();             // Acknowledge RESET IC Event
   cable_status.reset = RESET_DETECTED;  // Record that reset was asserted
}
#endif

//=========================================================================
// Target power control
//
//=========================================================================

#define VDD_2v  (((2*255)/5)*9/10)  // 10% allowance on 2V
#define VDD_3v3 (((3*255)/5)*9/10)  // 10% allowance on 3.3V
#define VDD_5v  (((5*255)/5)*9/10)  // 10% allowance on 5V

//!  Checks Target Vdd  - Updates Target Vdd LED & status
//!
//!  Updates \ref cable_status
//!
void bdm_checkTargetVdd(void) {
#if (CAPABILITY&CAP_VDDSENSE)
   if (bdm_targetVddMeasure() > VDD_2v) {
      RED_LED_ON();
      if (bdm_option.targetVdd == BDM_TARGET_VDD_OFF)
         cable_status.power = BDM_TARGET_VDD_EXT;
      else
         cable_status.power = BDM_TARGET_VDD_INT;
   }
   else {
      RED_LED_OFF();
      if (bdm_option.targetVdd == BDM_TARGET_VDD_OFF)
         cable_status.power = BDM_TARGET_VDD_NONE;
      else
         cable_status.power = BDM_TARGET_VDD_ERR;
   }
#else
   // No target Vdd sensing - assume external Vdd is present
   cable_status.power = BDM_TARGET_VDD_EXT;
#endif // CAP_VDDSENSE
}

#if (CAPABILITY&CAP_VDDCONTROL)

//! Turns on Target Vdd if enabled.
//!
//!  @return
//!   \ref BDM_RC_OK                => Target Vdd confirmed on target \n
//!   \ref BDM_RC_VDD_NOT_PRESENT   => Target Vdd not present
//!
U8 bdm_setTargetVdd( void ) {
U8 rc = BDM_RC_OK;

    switch (bdm_option.targetVdd) {
      case BDM_TARGET_VDD_OFF :
         VDD_OFF();
         // Check for externally supplied target Vdd (> 2 V)
         WAIT_US(VDD_RISE_TIME); // Wait for Vdd to rise & stabilise
         if (bdm_targetVddMeasure()<VDD_2v)
            rc = BDM_RC_VDD_NOT_PRESENT;
         break;
      case BDM_TARGET_VDD_3V3 :
         VDD3_ON();
         // Wait for Vdd to rise to 90% of 3V
         WAIT_WITH_TIMEOUT_MS( 250 /* ms */, (bdm_targetVddMeasure()>VDD_3v3));
         WAIT_US(VDD_RISE_TIME); // Wait for Vdd to rise & stabilise
         if (bdm_targetVddMeasure()<VDD_3v3) {
            VDD_OFF(); // In case of Vdd overload
            rc = BDM_RC_VDD_NOT_PRESENT;
            }
         break;
      case BDM_TARGET_VDD_5V  :
         VDD5_ON();
         // Wait for Vdd to rise to 90% of 5V
         WAIT_WITH_TIMEOUT_MS( 250 /* ms */, (bdm_targetVddMeasure()>VDD_5v));
         WAIT_US(VDD_RISE_TIME); // Wait for Vdd to rise & stabilise
         if (bdm_targetVddMeasure()<VDD_5v) {
            VDD_OFF(); // In case of Vdd overload
            rc = BDM_RC_VDD_NOT_PRESENT;
            }
         break;
    }

#if (CAPABILITY&CAP_VDDSENSE)
   CLEAR_VDD_SENSE_FLAG(); // Clear Vdd Change Event
#endif

   bdm_checkTargetVdd(); // Update Target Vdd LED & status

   return (rc);
}

#endif // CAP_VDDCONTROL

//!  Cycle power ON to target
//!
//! @param mode
//!    - \ref RESET_SPECIAL => Power on in special mode,
//!    - \ref RESET_NORMAL  => Power on in normal mode
//!
//!  BKGD/BKPT is held low when power is re-applied to start
//!  target with BDM active if RESET_SPECIAL
//!
//!  @return
//!   \ref BDM_RC_OK                	=> Target Vdd confirmed on target \n
//!   \ref BDM_RC_VDD_WRONG_MODE    	=> Target Vdd not controlled by BDM interface \n
//!   \ref BDM_RC_VDD_NOT_PRESENT   	=> Target Vdd failed to rise 		\n
//!   \ref BDM_RC_RESET_TIMEOUT_RISE    => RESET signal failed to rise 		\n
//!   \ref BDM_RC_BKGD_TIMEOUT      	=> BKGD signal failed to rise
//!
U8 bdm_cycleTargetVddOn(U8 mode) {
U8 rc = BDM_RC_OK;

   mode &= RESET_MODE_MASK;

#if (CAPABILITY&CAP_VDDCONTROL)

#if (CAPABILITY&CAP_CFVx)
   if  (cable_status.target_type == T_CFVx) {
      bdmcf_interfaceIdle();  // Make sure BDM interface is idle
      if (mode == RESET_SPECIAL)
         BKPT_LOW();
   }
   else
#endif // (CAPABILITY&CAP_CFVx)
   {
      bdmHCS_interfaceIdle();  // Make sure BDM interface is idle
      if (mode == RESET_SPECIAL) {
         // BKGD pin=L
#if TARGET_HARDWARE!=H_VERSALOON
         DATA_PORT      = BDM_DIR_Rx_WR;
         DATA_PORT_DDR  = BDM_DIR_Rx_MASK|BDM_OUT_MASK;
#else
         BDM_CLR();
#endif
      }
   }

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN     = 0;
   DEBUG_PIN     = 1;
   DEBUG_PIN     = 0;
   DEBUG_PIN     = 1;
#endif //  (DEBUG&CYCLE_DEBUG)

   // Power on with TargetVdd monitoring off
   DISABLE_VDD_SENSE_INT();
   rc = bdm_setTargetVdd();
   CLEAR_VDD_SENSE_FLAG();
   ENABLE_VDD_SENSE_INT();
   if (rc != BDM_RC_OK) // No target Vdd
      goto cleanUp;

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN     = 1;
   DEBUG_PIN     = 0;
#endif //  (DEBUG&CYCLE_DEBUG)

   // RESET rise may be delayed by target POR
   if (bdm_option.useResetSignal) {
      WAIT_WITH_TIMEOUT_S( 2 /* s */, (RESET_IN!=0) );
   }

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN   = 0;
   DEBUG_PIN   = 1;
#endif // (DEBUG&CYCLE_DEBUG)

   // Let signals settle & CPU to finish reset (with BKGD held low)
   WAIT_US(BKGD_WAIT);

#if (CAPABILITY&CAP_RESET)
   if (bdm_option.useResetSignal && (RESET_IN==0)) {
      // RESET didn't rise
      rc = BDM_RC_RESET_TIMEOUT_RISE;
      goto cleanUp;
      }
#endif //(CAPABILITY&CAP_RESET)

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN     = 1;
   DEBUG_PIN     = 0;
#endif // (DEBUG&CYCLE_DEBUG)

#if (CAPABILITY&CAP_CFVx)
   if  (cable_status.target_type == T_CFVx)
      bdmcf_interfaceIdle();  // Release BKPT etc
   else
#endif // (CAPABILITY&CAP_CFVx)
      bdmHCS_interfaceIdle();  // Release BKGD

   // Let processor start up
   WAIT_MS(RESET_RECOVERY);

#if 0
// Removed - some targets may be holding BKGD low (e.g. used as port pin)
// This situation is handled elsewhere (requires power cycle)
   if (BDM_IN==0) { // BKGD didn't rise!
      rc = BDM_RC_BKGD_TIMEOUT;
      goto cleanUp;
      }
#endif // 0

   cable_status.reset  = RESET_DETECTED; // Cycling the power should have reset it!

cleanUp:
#if (CAPABILITY&CAP_CFVx)
   if  (cable_status.target_type == T_CFVx)
      bdmcf_interfaceIdle();  // Release BKPT etc
   else
#endif // (CAPABILITY&CAP_CFVx)
      bdmHCS_interfaceIdle();  // Release BKGD

   WAIT_MS( 250 /* ms */);

//   EnableInterrupts;
#endif // CAP_VDDCONTROL

   bdm_checkTargetVdd(); // Update Target Vdd LED & power status

   return(rc);
}

//!  Cycle power OFF to target
//!
//!  @return
//!   \ref BDM_RC_OK                => No error  \n
//!   \ref BDM_RC_VDD_WRONG_MODE    => Target Vdd not controlled by BDM interface \n
//!   \ref BDM_RC_VDD_NOT_REMOVED   => Target Vdd failed to fall \n
//!
U8 bdm_cycleTargetVddOff(void) {
U8 rc = BDM_RC_OK;

#if (CAPABILITY&CAP_VDDCONTROL)

   bdm_checkTargetVdd();

   if (bdm_option.targetVdd == BDM_TARGET_VDD_OFF)
      return BDM_RC_VDD_WRONG_MODE;

#if (CAPABILITY&CAP_CFVx)
   if  (cable_status.target_type == T_CFVx)
      bdmcf_interfaceIdle();  // Make sure BDM interface is idle
   else
#endif // (CAPABILITY&CAP_CFVx)
      bdmHCS_interfaceIdle();  // Make sure BDM interface is idle

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN     = 0;
   DEBUG_PIN     = 1;
#endif

   // Power off & wait for Vdd to fall to ~5%
   VDD_OFF();
   WAIT_WITH_TIMEOUT_S( 5 /* s */, (bdm_targetVddMeasure()<30) );

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN   = 1;
   DEBUG_PIN   = 0;
#endif

   if (bdm_targetVddMeasure()>=40) // Vdd didn't turn off!
      rc = BDM_RC_VDD_NOT_REMOVED;

#if (DEBUG&CYCLE_DEBUG)
   DEBUG_PIN     = 0;
   DEBUG_PIN     = 1;
#endif

   bdm_checkTargetVdd(); // Update Target Vdd LED

   // Wait a while with power off
   WAIT_US(RESET_SETTLE);

   // Clear Vdd monitoring interrupt
#if (CAPABILITY&CAP_VDDSENSE)
   CLEAR_VDD_SENSE_FLAG();  // Clear Vdd monitoring flag
#endif
   bdm_checkTargetVdd();    // Update Target Vdd LED

#endif // CAP_VDDCONTROL

   return(rc);
}

//!  Cycle power to target
//!
//! @param mode
//!    - \ref RESET_SPECIAL => Power on in special mode,
//!    - \ref RESET_NORMAL  => Power on in normal mode
//!
//!  BKGD/BKPT is held low when power is re-applied to start
//!  target with BDM active if RESET_SPECIAL
//!
//!  @return
//!   \ref BDM_RC_OK                => No error \n
//!   \ref BDM_RC_VDD_WRONG_MODE    => Target Vdd not controlled by BDM interface \n
//!   \ref BDM_RC_VDD_NOT_REMOVED   => Target Vdd failed to fall \n
//!   \ref BDM_RC_VDD_NOT_PRESENT   => Target Vdd failed to rise \n
//!   \ref BDM_RC_RESET_TIMEOUT_RISE     => RESET signal failed to rise \n
//!
U8 bdm_cycleTargetVdd(U8 mode) {
U8 rc;

   rc = bdm_cycleTargetVddOff();
   if (rc != BDM_RC_OK)
      return rc;
   WAIT_S(1);
   rc = bdm_cycleTargetVddOn(mode);
   return rc;
}

//!  Measures Target Vdd
//!
//!  @return
//!  8-bit value representing the Target Vdd, N ~ (N/255) * 5V \n
//!  On JM60 hardware the internal ADC is used.
//!  JB16/UF32 doesn't have an ADC so an external comparator is used.  In this case this routine only
//!  returns an indication if Target Vdd is present [255 => Vdd present, 0=> Vdd not present].
//!
U16 bdm_targetVddMeasure(void) {

#if TARGET_HARDWARE!=H_VERSALOON

#if !(CAPABILITY&CAP_VDDSENSE)
   // No Target Vdd measurement - Assume external Vdd supplied
   return 255;
#elif (TARGET_HARDWARE==H_USBDM_JMxxCLD) || \
      (TARGET_HARDWARE==H_USBDM_JMxxCLC) || \
      (TARGET_HARDWARE==H_USBDM_CF_JMxxCLD)

int timeout = 1000;

   ADCCFG = ADCCFG_ADLPC_MASK|ADCCFG_ADIV1_MASK|ADCCFG_ADIV0_MASK|
            ADCCFG_ADLSMP_MASK|ADCCFG_ADIV1_MASK|ADCCFG_ADIV0_MASK|ADCCFG_ADICLK0_MASK;
   APCTL2 = APCTL2_ADPC9_MASK; // Using channel 9
   ADCSC2 = 0;                 // Single software triggered conversion
   ADCSC1 = 9;                 // Trigger single conversion on Ch9
   while ((timeout-->0) && (ADCSC1_COCO == 0)) {
   }
   return ADCR;

#else
   // Simple yes/no code as JB16 doesn't have a ADC
   VDD_SENSE_DDR = 0;
   asm {
      nop
      nop
      nop
   }
   return (VDD_SENSE?255:0);
#endif

#else
   return SampleVtarget() * 255 / 5000;
#endif
}

//=========================================================================
// Common BDM routines
//
//=========================================================================

//! Once off initialisation
//!
void bdm_init(void) {

   // Turn off important things
#if (CAPABILITY&CAP_FLASH)
   (void)bdmSetVpp(BDM_TARGET_VPP_OFF);
#endif   
   VDD_OFF();
   cable_status.target_type = T_OFF;
   (void)initTimers(); // Initialise Timer system & input monitors
}

//!  Sets the BDM interface to a suspended state
//!
//!  - All signals idle \n
//!  - All voltages off.
//!
void bdm_suspend(void){
#if (CAPABILITY&CAP_FLASH)
   (void)bdmSetVpp(BDM_TARGET_VPP_OFF);
#endif
#if (CAPABILITY&CAP_CFVx)
   bdmCF_suspend();
#endif // (CAPABILITY&CAP_CFVx)
   bdmHCS_suspend();
}

//!  Turns off the BDM interface
//!
//!  Depending upon settings, may leave target power on.
//!
void bdm_off( void ) {
#if (CAPABILITY&CAP_FLASH)
   (void)bdmSetVpp(BDM_TARGET_VPP_OFF);
#endif
#if (CAPABILITY&CAP_CFVx)
   bdmCF_off();
#endif (CAPABILITY&CAP_CFVx)
   bdmHCS_off();
   if (!bdm_option.leaveTargetPowered)
      VDD_OFF();
}

//! Initialises BDM module for the given target type
//!
//!  @param target = Target processor (see \ref TargetType_t)
//!
U8 bdm_setTarget(U8 target) {
U8 rc = BDM_RC_OK;

 #ifdef RESET_OUT_PER
   RESET_OUT_PER    = 1;    // Holds RESET_OUT inactive when unused
   RESET_IN_PER     = 1;    // Needed for input level translation to 5V
#endif

   cable_status             = cable_statusDefault; // Set default status/settings
   cable_status.target_type = target; // Assume mode is valid

#if (CAPABILITY&CAP_FLASH)
   (void)bdmSetVpp(BDM_TARGET_VPP_OFF);
#endif

   rc = initTimers();         // re-init timers in case settings changed
   if (rc != BDM_RC_OK)
      return rc;

   if (target == T_OFF) {
      bdm_off();             // Turn off the interface
      return BDM_RC_OK;
   }

   if (bdm_option.cycleVddOnReset && !bdm_option.leaveTargetPowered) {
      rc = bdm_cycleTargetVddOff();
      if (rc != BDM_RC_OK)
         return rc;
   }

   rc = bdm_cycleTargetVddOn(RESET_SPECIAL); // Turn on Vdd if appropriate
   if (rc != BDM_RC_OK)
      return rc;

   switch (target) {
      case T_HC12:
         bdm_option.useResetSignal = 1; // Must use RESET signal on HC12
         bdmHCS_init();
         break;
#if ((CAPABILITY & CAP_FLASH) != 0)
      case T_RS08:
         FLASH12V_ON();                 // Turn on charge pump for RS08 programming
#endif
      case T_HCS08:
      case T_CFV1:
         bdmHCS_init();
         break;
#if (CAPABILITY&CAP_CFVx)
      case T_CFVx:
         bdm_option.useResetSignal = 1; // Must use RESET signal on CFVx
         bdmcf_init();                  // Initialise the BDM interface
         (void)bdmcf_resync();          // Synchronize with the target (ignore error?)
         break;
      case T_JTAG:
         bdm_option.useResetSignal = 1; // Must use RESET signal on CFVx
         jtag_init();                   // Initialise T_JTAG
         break;
#endif // (CAPABILITY&CAP_CFVx)
      default:
         bdm_off();                        // Turn off the interface
         cable_status.target_type = T_OFF; // Safe mode!
         return BDM_RC_UNKNOWN_TARGET;
   }

   // Now give the BDM interface enough time to soft reset in case it was
   // doing something before
   WAIT_US(SOFT_RESET);  // Wait for the longest SOFT RESET time possible

   return rc;
}
