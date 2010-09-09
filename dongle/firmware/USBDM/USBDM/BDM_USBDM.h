/*! \file
    \brief Header file for BDM routines.
    
 */
#ifndef _BDM_H_
#define _BDM_H_

//===================================================================================
// Shared interface
void  bdmHCS_init(void);
void  bdmHCS_interfaceIdle(void);
void  bdmHCS_suspend(void);
void  bdmHCS_off( void );
U8    bdmHCS_powerOnReset(void);

U16   bdm_targetVddMeasure(void);

#if TARGET_HARDWARE!=H_VERSALOON
// Interrupt monitoring routines
interrupt void bdm_resetSense(void);
interrupt void bdm_targetVddSense(void);
#endif

//=============================================================
// BDM mode interface
#if TARGET_HARDWARE!=H_VERSALOON
U8    bdm_RxTxSelect(void);
void  bdm_txPrepare(void);
void  bdm_txFinish(void);
#endif
void  bdm_ackn(void);
#if TARGET_HARDWARE!=H_VERSALOON
U8    bdm_wait64(void);
U8    bdm_wait150(void);
#endif
#if (TARGET_HARDWARE!=H_VERSALOON)
void  bdm_acknInit(void);
#else
U8    bdm_acknInit(void);
#endif
U8    bdm_hardwareReset(U8 mode); 
U8    bdm_targetReset( U8 mode);
U8    bdm_syncMeasure(void);
U8    bdm_softwareReset(U8 mode);
U8    bdm_connect(void);
U8    bdm_physicalConnect(void);
U8    bdm_enableBDM(void);
U8    bdm_readBDMStatus(U8 *status);
U8    bdm_writeBDMControl(U8 bdm_sts);
void  bdm_checkTiming(void);
void  bdm_checkWaitTiming(void);

U8    bdmHC12_confirmSpeed(U16 syncValue);
U8    bdm_makeActiveIfStopped(void);
U8    bdm_halt(void);
U8    bdm_go(void);
U8    bdm_step(void);

U8    bdm_setInterfaceLevel(U8 level);

//! MACRO to calculate a SYNC value from a given frequency in Hz
#define SYNC_MULTIPLE(x)  (U16)((2*128*(60000000UL/10))/(x/10))

#pragma MESSAGE DISABLE C1106 // Disable warnings about Non-standard bitfield types
//! Target status
typedef struct {
   TargetType_t      target_type:8;  //!< Target type \ref TargetType_t
   AcknMode_t        ackn:8;         //!< Target supports ACKN see \ref AcknMode_t
   ResetMode_t       reset:8;        //!< Target has been reset, see \ref ResetMode_T
   SpeedMode_t       speed:8;        //!< Target speed determination method, see \ref SpeedMode_t
   TargetVddState_t  power:8;        //!< Target Vdd state
   TargetVppSelect_t flashState:8;   //!< State of RS08 Flash programming,  see \ref FlashState_t
   U16               sync_length;    //!< Length of the target SYNC pulse in 60MHz ticks
   U16               wait150_cnt;    //!< Time for 150 BDM cycles in bus cycles of the MCU divided by N
   U16               wait64_cnt;     //!< Time for 64 BDM cycles in bus cycles of the MCU divided by N
} CableStatus_t;

//! Target interface options
typedef struct {
   U8  targetVdd;                //!< Target Vdd (off, 3.3V or 5V)
   U8  cycleVddOnReset;          //!< Cycle target Power  when resetting
   U8  cycleVddOnConnect;        //!< Cycle target Power if connection problems (when resetting?)
   U8  leaveTargetPowered;       //!< Leave target power on exit
   U8  autoReconnect;            //!< Automatically re-connect to target (for speed change - experimental)
   U8  guessSpeed;               //!< Guess speed for target w/o ACKN
   U8  useAltBDMClock;           //!< Use alternative BDM clock source in target (HCS08)
   U8  useResetSignal;           //!< Use RESET signal on BDM interface
   U8  reserved[4];
} BDM_Option_t;

#pragma MESSAGE DEFAULT C1106 // Restore warnings about Non-standard bitfield types

#if TARGET_HARDWARE!=H_VERSALOON
extern void bdm_txEmpty(U8 data);
extern U8   bdm_rxEmpty(void);

#ifdef __HC08__
#pragma DATA_SEG __SHORT_SEG Z_PAGE
#endif // __HC08__
extern U8   (*bdm_rx_ptr)(void); // Pointer to BDM Rx routines
extern void (*bdm_tx_ptr)(U8);   // Pointer to BDM Tx routines

//! Returns true if BDM Rx & Tx routines have been set for the current communication speed
#define BDM_TXRX_SET ((bdm_rx_ptr != bdm_rxEmpty) && (bdm_tx_ptr != bdm_txEmpty))

#ifdef __HC08__
#pragma DATA_SEG DEFAULT
#endif // __HC08__

#define bdm_rx()      (*bdm_rx_ptr)() 
#define bdm_tx(data)  (*bdm_tx_ptr)(data) 

#endif

#endif // _BDM_H_
