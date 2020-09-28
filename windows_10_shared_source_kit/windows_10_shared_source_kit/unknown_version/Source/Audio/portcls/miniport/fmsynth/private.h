/*****************************************************************************
 * private.h - FM synth miniport private definitions
 *****************************************************************************
 * Copyright (c) Microsoft.  All rights reserved.
 */

#ifndef _FMSYNTH_PRIVATE_H_
#define _FMSYNTH_PRIVATE_H_

#pragma warning (disable : 4100)

#include "portcls.h"
#include "stdunk.h"
#include "ksdebug.h"

#include "miniport.h"









enum {
    CHAN_MASTER = (-1),
    CHAN_LEFT = 0,
    CHAN_RIGHT = 1
};

/*****************************************************************************
 * Classes
 */

/*****************************************************************************
 * CMiniportMidiFM
 *****************************************************************************
 * FM miniport.  This object is associated with the device and is
 * created when the device is started.  The class inherits IMiniportMidi
 * so it can expose this interface and CUnknown so it automatically gets
 * reference counting and aggregation support.
 */
class CMiniportMidiFM
:   public IMiniportMidi,
    public IPowerNotify,
    public CUnknown
{
private:
    PPORTMIDI       m_Port;                 // Callback interface.
    PUCHAR          m_PortBase;             // Base port address.
    BOOLEAN         m_BoardNotResponsive;   // Indicates dead hardware.
    BOOLEAN         m_bInit;                // true if we have already done init.
    BOOLEAN         m_fStreamExists;        // True if we have a stream.

    BYTE            m_SavedRegValues[0x200]; // Shadow copies of the FM registers.
    POWER_STATE     m_PowerState;            // Saved power state (D0 = full power, D3 = off)
    BOOLEAN         m_volNodeNeeded;         // Whether we need to furnish a volume node.
    KSPIN_LOCK      m_SpinLock;              // Protects writes to hardware.

    /*************************************************************************
     * CMiniportMidiFM methods
     *
     * These are private member functions used internally by the object.  See
     * MINIPORT.CPP for specific descriptions.
     *
     */
    NTSTATUS 
    ProcessResources
    (
        IN      PRESOURCELIST   ResourceList
    );

    void SoundMidiSendFM(IN PUCHAR PortBase, _In_range_(0, 0x200 - 1) ULONG Address, IN UCHAR Data); // low-level--write registers

    BOOL SoundSynthPresent(IN PUCHAR base, IN PUCHAR inbase);   // detect if synth is present.
    BOOL SoundMidiIsOpl3(VOID);     // returns true if the device is an opl3 and false if not.
    VOID Opl3_BoardReset(VOID);
    VOID MiniportMidiFMResume(VOID);

public:
    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();

//  expand constructor to take bool for whether to include volume
    CMiniportMidiFM(PUNKNOWN pUnknownOuter,int createVolNode)
    :   CUnknown(pUnknownOuter)
    {
        m_PortBase = NULL;
        m_volNodeNeeded = (createVolNode != 0);
    };

    ~CMiniportMidiFM();

    /*************************************************************************
     * IMiniport methods
     */
    STDMETHODIMP_(NTSTATUS) 
    GetDescription
    (   _Out_     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
    );
    STDMETHODIMP_(NTSTATUS) 
    DataRangeIntersection
    (   _In_          ULONG           PinId
    ,   _In_          PKSDATARANGE    DataRange
    ,   _In_          PKSDATARANGE    MatchingDataRange
    ,   _In_          ULONG           OutputBufferLength
    ,   _Out_writes_bytes_to_opt_(OutputBufferLength, *ResultantFormatLength)
                      PVOID           ResultantFormat
    ,   _Out_         PULONG          ResultantFormatLength
    )
    {
        return STATUS_NOT_IMPLEMENTED;
    }

    /*************************************************************************
     * IMiniportMidi methods
     */
    STDMETHODIMP_(NTSTATUS) Init
    (
        _In_      PUNKNOWN        UnknownNotUsed  OPTIONAL,
        _In_      PRESOURCELIST   ResourceList,
        _In_      PPORTMIDI       Port,
        _Out_     PSERVICEGROUP * ServiceGroup
    );
    STDMETHODIMP_(NTSTATUS) NewStream
    (
        _Out_     PMINIPORTMIDISTREAM *   Stream,
        _In_opt_  PUNKNOWN                OuterUnknown    OPTIONAL,
        _When_((PoolType&NonPagedPoolMustSucceed)!=0,
            __drv_reportError("Must succeed pool allocations are forbidden. "
                              "Allocation failures cause a system crash"))
        _In_      POOL_TYPE               PoolType,
        _In_      ULONG                   Pin,
        _In_      BOOLEAN                 Capture,
        _In_      PKSDATAFORMAT           DataFormat,
        _Out_     PSERVICEGROUP *         ServiceGroup
    );
    STDMETHODIMP_(void) Service
    (   void
    );

    /*************************************************************************
     * IPowerNotify methods
     */
    STDMETHODIMP_(void) PowerChangeNotify(
        _In_  POWER_STATE     PowerState
    );

    
/*************************************************************************
     * Friends
     */
    friend class CMiniportMidiStreamFM;

};

/*****************************************************************************
 * CMiniportMidiStreamFM
 *****************************************************************************
 * FM miniport stream.  This object is associated with a pin and is created
 * when the pin is instantiated.  The class inherits IMiniportMidiStream
 * so it can expose this interface and CUnknown so it automatically gets
 * reference counting and aggregation support.
 */
class CMiniportMidiStreamFM
:   public IMiniportMidiStream,
    public CUnknown
{
private:
    CMiniportMidiFM *   m_Miniport;     // Parent miniport.
    PUCHAR              m_PortBase;     // Base port address.

    // midi stuff
    voiceStruct m_Voice[NUM2VOICES];  /* info on what voice is where */
    DWORD m_dwCurTime;    /* for note on/off */
    /* volume */
    WORD    m_wSynthAttenL;        /* in 1.5dB steps */
    WORD    m_wSynthAttenR;        /* in 1.5dB steps */

    /* support for volume property */
    LONG    m_MinVolValue;      // Minimum value for volume controller
    LONG    m_MaxVolValue;      // Maximum value for volume controller
    ULONG   m_VolStepDelta;     // Correlation between controller and actual decibels
    LONG    m_SavedVolValue[2]; // Saved value for volume controller

    /* channel volumes */
    BYTE    m_bChanAtten[NUMCHANNELS];       /* attenuation of each channel, in .75 db steps */
    BYTE    m_bStereoMask[NUMCHANNELS];              /* mask for left/right for stereo midi files */

    short   m_iBend[NUMCHANNELS];    /* bend for each channel */
    BYTE    m_bPatch[NUMCHANNELS];   /* patch number mapped to */
    BYTE    m_bSustain[NUMCHANNELS];   /* Is sustain in effect on this channel? */

    /*************************************************************************
     * CMiniportMidiStreamFM methods
     *
     * These are private member functions used internally by the object.  See
     * MINIPORT.CPP for specific descriptions.
     */

    VOID WriteMidiData(DWORD dwData);
    // opl3 processing methods.
    VOID Opl3_ChannelVolume(BYTE bChannel, WORD wAtten);
    VOID Opl3_SetPan(BYTE bChannel, BYTE bPan);
    VOID Opl3_PitchBend(BYTE bChannel, short iBend);
    VOID Opl3_NoteOn(BYTE bPatch,BYTE bNote, BYTE bChannel, BYTE bVelocity,short iBend);
    VOID Opl3_NoteOff (BYTE bPatch,BYTE bNote, BYTE bChannel, BYTE bSustain);
    VOID Opl3_AllNotesOff(VOID);
    VOID Opl3_ChannelNotesOff(BYTE bChannel);
    WORD Opl3_FindFullSlot(BYTE bNote, BYTE bChannel);
    WORD Opl3_CalcFAndB (DWORD dwPitch);
    DWORD Opl3_CalcBend (DWORD dwOrig, short iBend);
    BYTE Opl3_CalcVolume (BYTE bOrigAtten, BYTE bChannel,BYTE bVelocity, BYTE bOper, BYTE bMode);
    BYTE Opl3_CalcStereoMask (BYTE bChannel);
    WORD Opl3_FindEmptySlot(BYTE bPatch);
    VOID Opl3_SetVolume(BYTE bChannel);
    VOID Opl3_FMNote(WORD wNote, noteStruct FAR * lpSN);
    VOID Opl3_SetSustain(BYTE bChannel, BYTE bSusLevel);

    void SetFMAtten(LONG channel, LONG level);
    LONG GetFMAtten(LONG channel)    {   return m_SavedVolValue[channel];    };

public:
    NTSTATUS
    Init
    (
        IN      CMiniportMidiFM *   Miniport,
        IN      PUCHAR              PortBase
    );

    /*************************************************************************
     * The following two macros are from STDUNK.H.  DECLARE_STD_UNKNOWN()
     * defines inline IUnknown implementations that use CUnknown's aggregation
     * support.  NonDelegatingQueryInterface() is declared, but it cannot be
     * implemented generically.  Its definition appears in MINIPORT.CPP.
     * DEFINE_STD_CONSTRUCTOR() defines inline a constructor which accepts
     * only the outer unknown, which is used for aggregation.  The standard
     * create macro (in MINIPORT.CPP) uses this constructor.
     */
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportMidiStreamFM);

    ~CMiniportMidiStreamFM();

    /*************************************************************************
     * IMiniportMidiStream methods
     */
    STDMETHODIMP_(NTSTATUS) SetFormat
    (
        _In_      PKSDATAFORMAT   DataFormat
    );
    STDMETHODIMP_(NTSTATUS) SetState
    (
        IN      KSSTATE     State
    );
    STDMETHODIMP_(NTSTATUS) Read
    (
        _In_      PVOID       BufferAddress,
        _In_      ULONG       BufferLength,
        _Out_     PULONG      BytesRead
    );
    STDMETHODIMP_(NTSTATUS) Write
    (
        _In_      PVOID       BufferAddress,
        _In_      ULONG       BytesToWrite,
        _Out_     PULONG      BytesWritten
    );

/*************************************************************************
     * Friends
     */
    friend
    NTSTATUS BasicSupportHandler
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    friend
    NTSTATUS PropertyHandler_Level
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

    friend
    NTSTATUS PropertyHandler_CpuResources
    (
        IN  PPCPROPERTY_REQUEST PropertyRequest
    );

};

#endif