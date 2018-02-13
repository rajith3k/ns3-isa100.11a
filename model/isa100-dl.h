/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 The University of Calgary- FISHLAB
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Geoffrey Messier <gmessier@ucalgary.ca>
 *         Michael Herrmann <mjherrma@ucalgary.ca>
 *
 */


#ifndef ISA100_DL_H
#define ISA100_DL_H

#include <deque>

#include "ns3/zigbee-phy.h"
#include "ns3/mac16-address.h"
#include "ns3/isa100-processor.h"


namespace ns3 {

class Packet;

/**
 * \defgroup isa100-11a-dl ISA 100.11a DL Implementation
 *
 * This documents the code that implements the 100.11a DL layer.
 *  @{
 */


// ------- DL State Machine Types ----------

/** Enum defining MAC state.
 * - Doesn't seem to correspond to any one table in the standard.
 */
typedef enum
{
  MAC_IDLE,
  CHANNEL_ACCESS_FAILURE,
  CHANNEL_IDLE,
  SET_PHY_TX_ON
} LrWpanMacState;


/** Enum of result of a request made to the MAC by the service layer to transmit a MPDU.
 * - Table 42 of 802.15.4-2006
 */
typedef enum
{
  IEEE_802_15_4_SUCCESS                = 0,
  IEEE_802_15_4_TRANSACTION_OVERFLOW   = 1,
  IEEE_802_15_4_TRANSACTION_EXPIRED    = 2,
  IEEE_802_15_4_CHANNEL_ACCESS_FAILURE = 3,
  IEEE_802_15_4_INVALID_ADDRESS        = 4,
  IEEE_802_15_4_INVALID_GTS            = 5,
  IEEE_802_15_4_NO_ACK                 = 6,
  IEEE_802_15_4_COUNTER_ERROR          = 7,
  IEEE_802_15_4_FRAME_TOO_LONG         = 8,
  IEEE_802_15_4_UNAVAILABLE_KEY        = 9,
  IEEE_802_15_4_UNSUPPORTED_SECURITY   = 10,
  IEEE_802_15_4_INVALID_PARAMETER      = 11
} LrWpanMcpsDataConfirmStatus;




// ------- DL Inter-Layer Communication Types ----------


/** Used by upper layers to request a DL transmission.
 * - DL-DATA.request
 * - Partial implementation of Table 104.
 * - A separate pointer to a packet is used instead of the DSDU pointer to octets.
 * - DSDUHandle is implemented to be equivalent to the 802.15.4 MSDU handle (8 bit wraparound counter).
 */
struct DlDataRequestParams
{
	Mac16Address m_srcAddr;
	Mac16Address m_destAddr;
	uint8_t m_dsduLength; ///< DPDU payload length (in octets)
	uint8_t m_dsduHandle; ///< Identifier for this data request used by confirm callback.
};

/** Used by DL to indicate status of a transmission request.
 * - DL-DATA.confirm
 * - Implements Table 105
 */
typedef enum{
	SUCCESS,
	FAILURE
} DlDataRequestStatus;

/** Used by DL to indicate status of a transmission request.
 * - DL-DATA.confirm
 * - Implements Table 105
 */
struct DlDataConfirmParams{
	uint8_t m_dsduHandle;  ///< Matches the value used in the request being confirmed.
	DlDataRequestStatus m_status;  ///< Indicates result of data send request.
};


/** Used by DL to pass a received packet up to the higher layers
 * - DL-DATA.indication
 * - Partial implementation of Table 107.
 * - A separate pointer to a packet is used instead of the DSDU pointer to octets.
 */
struct DlDataIndicationParams
{
	Mac16Address m_srcAddr;
	Mac16Address m_destAddr;
	uint8_t m_dsduLength; ///< DPDU payload length (in octets)
};


// ------ DL to Higher Layer Communication Interface -------

/** Callback to return the outcome of a transmission request.
 * - Takes one DataConfirmParams argument.
 *
 */
typedef Callback< void, DlDataConfirmParams > DlDataConfirmCallback;

/** Callback used to pass a packet from the MAC to the service layer.
 * - Called upon packet reception.
 * - Takes packet pointer and DlDataIndicationParams arguments.
 */
typedef Callback< void, DlDataIndicationParams, Ptr<Packet> > DlDataIndicationCallback;

/** Callback used to indicate that the lower layers (Dl and Phy) have woken up
 */
typedef Callback< void > DlWokeUpCallback;

/** Callback used to indicate that the superframe has no more tx/rx slots
 * - Argument is the number of remaining slots in the superframe
 */
typedef Callback< void, uint16_t> DlFrameCompleteCallback;

/** Callback used to indicate that there are upcoming idle slots
 * - Argument is the number of idle slots including the slot which is just starting
 */
typedef Callback< void, uint16_t> DlInactiveSlotsCallback;


// ------- DL to PHY Communication Interface ---------

/** Callback type used to set an operating parameter in the PHY layer.
 */
typedef Callback< void, ZigbeePibAttributeIdentifier, ZigbeePhyPIBAttributes* > PlmeSetAttributeCallback;

/** Callback type used to request a CCA.
 */
typedef Callback< void > PlmeCcaRequestCallback;


/** Callback type used to request a TRX state change.
 */
typedef Callback< void, ZigbeePhyEnumeration > PlmeSetTrxStateRequestCallback;


/** Callback type used to request a data transmission.
 */
typedef Callback< void, uint32_t, Ptr<Packet> > PdDataRequestCallback;

/** Callback type used to put the PHY to sleep for a given amount of time
 */
typedef Callback< void, Time > PlmeSleepForCallback;



// ------- DL Superframe/Hopping Pattern Types ----------

/** Enum defining link activity types.
 */
typedef enum{
	TRANSMIT,
	RECEIVE,
	SHARED
} DlLinkType;

class Isa100Dl;

/** Class that stores the ISA-100 Data Link superframe schedule.
 * - Includes both channel hopping and time slot schedules.
 * - Supports multi-superframes
 */
class Isa100DlSfSchedule : public Object{
	friend class Isa100Dl;

public:
	Isa100DlSfSchedule();

	virtual ~Isa100DlSfSchedule();

	static TypeId GetTypeId (void);

  /** Set the schedule.
   * - Note that this schedule is repeated over and over again.
   * - The hopping pattern is changed on every timeslot, whether there is
   *   link activity scheduled or not.
   *
   */
	void SetSchedule(
	      const uint8_t* startHop, uint32_t numHop,
	      uint16_t* startSlotSched, DlLinkType* startSlotType, uint32_t numSlots);

  /** Set the schedule.
   * - Note that this schedule is repeated over and over again.
   * - The hopping pattern is changed on every timeslot, whether there is
   *   link activity scheduled or not.
   *
   */
	void SetSchedule(
			vector<uint8_t> hoppingPattern,
			vector<uint16_t> scheduleSlots,
			vector<DlLinkType> scheduleTypes);


	/** Get the sf link slot schedule
	 */
	std::vector<uint16_t> *GetLinkSlotSchedule(void);

  /** Get the sf link slot types
   */
	std::vector<DlLinkType> *GetLinkSlotTypes(void);

	/* Get the frame boundaries for a multi-frame sf
	 */
	std::vector<uint16_t> *GetFrameBounds(void);


private:
	std::vector<uint8_t> m_dlHoppingPattern;         ///< Channel hopping pattern (dlmo.Ch Table 160).
	std::vector<uint16_t> m_dlLinkScheduleSlots;     ///< Slots where link activity is defined.
	std::vector<DlLinkType> m_dlLinkScheduleTypes;   ///< Type of link activity in each slot.
	std::vector<Mac16Address> m_dlLinkScheduleDests; ///< Destination of each link for a TDMA schedule
	std::vector<uint16_t> m_multiFrameBounds;        ///< Indexes in the above vectors which represent a new Frame
  std::vector<uint16_t> m_numPktsInSlot;           ///< The number of packets which are being sent during each slot
	uint16_t m_currMultiFrameI;                      ///< Index of the current frame in a multiframe superframe
};


class Isa100RoutingAlgorithm;

/** Class that implements the ISA-100 Data Link (DL) layer.
 *
 */
class Isa100Dl : public Object
{
public:
  static TypeId GetTypeId (void);

  Isa100Dl ();

  virtual ~Isa100Dl ();

  /** Scheduled by ctor to execute at simulation time zero.
   *
   */
  void Start();

  /** Set the superframe schedule.
   *
   * \param schedule Object storing schedule information.
   */
  void SetDlSfSchedule(Ptr<Isa100DlSfSchedule> schedule);

  /** Set the routing algorithm object.
   *
   * \param algorithm Pointer to the routing algorithm object.
   */
  void SetRoutingAlgorithm(Ptr<Isa100RoutingAlgorithm> routingAlgorithm);

  /** Get the routing algorithm object.
   *
   * \return Pointer to the routing algorithm object.
   */
  Ptr<Isa100RoutingAlgorithm> GetRoutingAlgorithm();

  /** Set/Get the tx power level (dBm) for this node to reach all others.
   * Converts double values to 6-bit ints by rounding up (ceiling).
   *
   * @param txPowers An array of tx power levels in which the index corresponds to the
   *                 other nodes' addresses
   * @param size The number of nodes in the network
   */
  void SetTxPowersDbm (double * txPowers, uint8_t numNodes);
  int8_t* GetTxPowersDbm (void);

  /** Set/Get the tx power level (dBm) for this node to reach another node
   * Converts double values to 6-bit ints by rounding up (ceiling).
   *
   * @param txPower The tx power level required to reach the indexed node
   * @param destNodeI The index of the node to reach
   */
  void SetTxPowerDbm (double txPower, uint8_t destNodeI);
  int8_t GetTxPowerDbm (uint8_t destNodeI);


  /** Function used to process the PSDU received from the PHY.
   *  - Called at a random delay uniformly distributed between 0 and m_minLIFSPeriod to account for MAC processing.
   *
   *  @param psduLength number of bytes in the PSDU
   *  @param p the packet that was received
   *  @param lqi Link quality (LQI) value measured during reception of the PPDU
   *  @param rxPowDbm The received power of the signal in dBm
   */
  void ProcessPdDataIndication (uint32_t psduLength, Ptr<Packet> p, uint32_t lqi, double rxPowDbm);



  // ------ Higher Layer to DL Communication Interface -------

  /** Used by higher layers to request a transmission.
   * - DL-DATA.request
   *
   * @param params Transmission parameters.
   * @param p The packet to send.
   */
  void DlDataRequest (DlDataRequestParams params, Ptr<Packet> p);

  // ------ DL to Higher Layer Communication Interface -------

  /** Set the data indication callback.
   * Called when an MPDU is received and must be passed to service layer.
   *
   * @param c Callback function.
   */
  void SetDlDataIndicationCallback (DlDataIndicationCallback c);

  /** Set the data confirm callback function.
   * Confirms result of a transmission request.
   *
   * @param c Callback function.
   */
  void SetDlDataConfirmCallback (DlDataConfirmCallback c);

  /* Set the dl woke up callback function
   *
   * @param c Callback function.
   */
  void SetDlWokeUpCallback (DlWokeUpCallback c);

  /* Set the dl frame complete callback function
   *
   * @param c Callback function.
   */
  void SetDlFrameCompleteCallback (DlFrameCompleteCallback c);

  /* Set the dl inactive slots callback function
   *
   * @param c Callback function.
   */
  void SetDlInactiveSlotsCallback (DlInactiveSlotsCallback c);


  // ------- DL to PHY Communication Interface ---------


  /** Set the callback function used request a CCA.
   *
   * @param c Callback function.
   */
  void SetPlmeCcaRequestCallback (PlmeCcaRequestCallback c);

  /** Set the callback function used request a TRX change.
   *
   * @param c Callback function.
   */
  void SetPlmeSetTrxStateRequestCallback (PlmeSetTrxStateRequestCallback c);


  /** Set the callback function used request a PHY transmission.
   *
   * @param c Callback function.
   */
  void SetPdDataRequestCallback (PdDataRequestCallback c);

  /** Set the callback function used to set PHY operating attributes.
   *
   * @param c Callback function.
   */
  void SetPlmeSetAttributeCallback (PlmeSetAttributeCallback c);

  /** Set the callback function used to sleep the PHY
   *
   * @param c Callback function.
   */
  void SetPlmeSleepForCallback (PlmeSleepForCallback c);



  // ------- PHY to DL Communication Interface ---------

  /** Function used to receive a PSDU from the PHY.
   *  - IEEE 802.15.4-2006 section 6.2.1.3
   *  - PD-DATA.indication
   *  - Used as a callback function by the PHY.
   *
   *  @param psduLength number of bytes in the PSDU
   *  @param p the packet that was received
   *  @param lqi Link quality (LQI) value measured during reception of the PPDU
   *  @param rxPowDbm The received power of the signal in dBm
   */
  void PdDataIndication (uint32_t psduLength, Ptr<Packet> p, uint32_t lqi, double rxPowDbm);

  /** Function the PHY calls to confirm a transmission.
   *  - IEEE 802.15.4-2006 section 6.2.1.2
   *  - Used as a callback function by the PHY.
   *  - PD-DATA.confirm
   *
   *  @param status to report to MAC
   */
  void PdDataConfirm (ZigbeePhyEnumeration status);

  /** Function PHY calls to confirm status of a CCA request.
   *  - IEEE 802.15.4-2006 section 6.2.2.2
   *  - PLME-CCA.confirm status
   *  - Note that status enum contains many other values not used here.
   *  - Used as a callback function by the PHY.
   *
   *  @param status TRX_OFF, BUSY or IDLE
   */
  void PlmeCcaConfirm (ZigbeePhyEnumeration status);


  /** Function PHY calls to confirm result of state change request.
   *  - IEEE 802.15.4-2006 section 6.2.2.8
   *  - PLME-SET-TRX-STATE.confirm
   *  - Set PHY state
   *
   *  @param state in RX_ON,TRX_OFF,FORCE_TRX_OFF,TX_ON
   */
  void PlmeSetTrxStateConfirm (ZigbeePhyEnumeration status);

  /** Function PHY calls to confirm request to set a PHY operating parameter.
   *  - IEEE 802.15.4-2006 section 6.2.2.10
   *  - PLME-SET.confirm
   *  - Set attributes per definition from Table 23 in section 6.4.2
   *
   *  @param status SUCCESS, UNSUPPORTED_ATTRIBUTE, INVALID_PARAMETER, or READ_ONLY
   *  @param id the attributed identifier
   */
  void PlmeSetAttributeConfirm (ZigbeePhyEnumeration status,
                                ZigbeePibAttributeIdentifier id);

  /** Function PHY calls to indicate sleep time is over
   */
  void PlmeWakeUp (void);

  // ------- Interface to Node Hardware --------

  /** Set pointer to the node processor.
   *
   * @param battery Pointer to processor.
   */
  void SetProcessor(Ptr<Isa100Processor> processor);




  // ------- Calculate Results/Statistics ---------

  /** Calculate the average number of transmissions
   *
   * @return average number of transmissions
   */
  double CalculateAvgRetrx (void) const;

  /** Calculate the ratio of frames dropped out of total frames sent
   *
   * @return The frame drop ratio
   */
  double CalculateDropRatio (void) const;

  /** Get the time duration until the start of the next timeslot
   *
   * @return the time duration
   */
  Time GetTimeToNextSlot (void);

  /** Flush any pending packets out of the tx queue
   */
  void FlushTxQueue (void);

  /**}@*/



private:

  // ------ Private Member Functions -------

  virtual void DoDispose (void);

  /** Implements slotted channel hoping.
   * - A simple algorithm that changes channel each superframe timeslot.
   * - This function could be later replaced with a base class to allow
   *   different hopping algorithms (ie. slow or hybrid hopping) to be
   *   implemented via inheritance.
   */
  void ChannelHop();

  /** Processes link activity.
   * - Recursively scheduled to execute on the active slots indicated
   *   by the superframe link schedule.
   */
  void ProcessLink();

  /** Used for scheduling a PHY state change request.
   *
   * \param state Requested state.
   */
  void ProcessTrxStateRequest(ZigbeePhyEnumeration state);

  /** Used for delaying the CCA request by m_xmitEarliest
   *
   */
  void CallPlmeCcaRequest();

  /** Determines if given packet is an ACK
   *
   * \param p Packet to check
   * \returns true if packet is an ACK, otherwise return false
   */
   bool IsAckPacket(Ptr<const Packet> p);


  // ------- Trace Functions --------
  /** Trace source for all packets entering transmitter.
   */
  TracedCallback< Mac16Address, Ptr<const Packet> > m_dlTxTrace;

  /** Trace source for packets dropped in the transmitter.
   *  - Usually packets in the queue unable to be sent due to channel congestion.
   */
  TracedCallback< Mac16Address, Ptr<const Packet> > m_dlTxDropTrace;

  /** Trace source for all packets successfully received by the DL.
   */
  TracedCallback< Mac16Address, Ptr<const Packet> > m_dlRxTrace;

  /** Trace source for packets dropped in the receiver.
   *  - Due to mismatched address since noise and interference handled at PHY.
   */
  TracedCallback< Mac16Address, Ptr<const Packet> > m_dlRxDropTrace;

  /** Trace source for packets forwarded in a multi-hop routing scheme.
   */
  TracedCallback< Mac16Address, Ptr<const Packet> > m_dlForwardTrace;

  /** Trace source for info about dropped packets.
   */
  TracedCallback<Mac16Address, Ptr<const Packet> , std::string> m_infoDropTrace;

  /** Trace source for logging process link and each timeslot
   *  - Address, link type, tx queue size, backoff counter, arq backoff counter
   */
  TracedCallback<Mac16Address, DlLinkType, uint16_t, uint16_t, uint16_t> m_processLinkTrace;

  /** Trace source for info about dl tasks.
   *  - Address, task description
   */
  TracedCallback<Mac16Address, std::string> m_dlTaskTrace;

  /** Trace source for indicating a retransmission
   *  - Address
   */
  TracedCallback<Mac16Address> m_retrxTrace;

  // -------- Member Variables ----------

  /** Structure for storing a queued packet and associated information.
   * - Fields can be added in the future to indicate different priorities in the queue.
   */
  struct TxQueueElement
  {
    uint8_t m_dsduHandle;
    uint8_t m_txAttemptsRem;
    Ptr<Packet> m_packet;
  };
  std::deque<TxQueueElement*> m_txQueue;  ///< Transmit packet queue.

  DlDataConfirmCallback m_dlDataConfirmCallback;  ///< Pointer to data confirm callback function.
  DlDataIndicationCallback m_dlDataIndicationCallback;  ///< Pointer to data indication callback function.
  DlWokeUpCallback m_dlWokeUpCallback;    ///< Pointer to woke up callback function.
  DlFrameCompleteCallback m_dlFrameCompleteCallback; ///< Pointer to the frame complete callback function.
  DlInactiveSlotsCallback m_dlInactiveSlotsCallback; ///< Pointer to the inactive slots callback function.
  PlmeSetAttributeCallback m_plmeSetAttribute;  ///< Pointer to PHY set attribute callback.
  PlmeCcaRequestCallback m_plmeCcaRequest;  ///< Pointer to CCA request function.
  PlmeSetTrxStateRequestCallback m_plmeSetTrxStateRequest;  ///< Pointer to set TRX state request function.
  PdDataRequestCallback m_pdDataRequest; ///< Pointer to data request function.
  PlmeSleepForCallback m_plmeSleepFor; ///< Pointer to set Phy sleep for function

  LrWpanMacState m_lrWpanMacState;   ///< State of 802.15.4 MAC layer.
  Mac16Address m_address;   ///< DL 16 bit address for this node.
  uint16_t m_sfPeriod;  ///< Superframe period (number of timeslots).
  Time m_sfSlotDuration; ///< Duration of timeslot within a superframe (ms)

  uint32_t m_dlHopIndex;  ///< Index of current channel in the hopping pattern.
  uint32_t m_dlLinkIndex; ///< Index of current link activity in the superframe.
  uint16_t m_expBackoffCounter; ///< Backoff counter used for shared timeslots.
  uint8_t m_backoffExponent; ///< Used to determine max number of backoff slots.
  uint16_t m_expArqBackoffCounter; ///< Backoff counter used for arq retransmissions.
  uint8_t m_arqBackoffExponent; ///< Used to determine max number of arq backoff slots.
  uint8_t m_packetTxSeqNum[256]; ///< Transmitted packet sequence number.
  uint8_t m_nextRxPacketSeqNum[256];  ///< Sequence number of the next packet expected.
  uint8_t m_maxFrameRetries; ///< The max number of retries allowed after a transmission failure. (Range: 0 to 7)
  int8_t m_maxTxPowerDbm; ///< The maximum transmit power at which this node can transmit at (in dBm)
  int8_t m_minTxPowerDbm; ///< The minimum transmit power at which this node can transmit at (in dBm)

  int8_t m_txPowerDbm[256];   ///< A list of power required to transmit to each node (dBm)
  uint8_t m_usePowerCtrl;     ///< Is power control being used

  Ptr<Isa100DlSfSchedule> m_sfSchedule;  ///< Pointer to the superframe schedule.
  uint16_t m_tdmaPktsLeft;               ///< For a tdma schedule the amount of packets left to send in the current slot

  Ptr<Isa100RoutingAlgorithm> m_routingAlgorithm; ///< Pointer to routing algorithm object.
  std::vector<Mac16Address> m_attemptedLinks; ///< A list of attempted links for the current tx packet

  EventId m_nextProcessLink;   ///< Next scheduled process link event
  Time m_nextProcessLinkDelay; ///< Remaining delay until when process link is suppose to run again

  Ptr<UniformRandomVariable> m_uniformRv; ///< Uniform RV.
  Time m_minLIFSPeriod;               ///< Min amount of time the standard allows for MAC processing.
  Time m_clockError;                    ///< Max allowed sync error between nodes due to clock rounding.
  Time m_xmitEarliest;                  ///< Earliest time a transmitter sends a packet after the start of a frame.

  // Variables to help with generating results/stats
  uint32_t m_numFramesSent;   ///< Total number of transmitted frames
  uint32_t m_numFramesDrop;   ///< Total number of dropped frames (rx and tx)
  uint32_t m_numRetrx;        ///< Total number of retransmissions

  Ptr<Isa100Processor> m_processor; ///< Pointer to the node processor.
  bool m_dlSleepEnabled; ///< Indicates whether DL is capable of sleeping.

  bool m_ackEnabled; ///< Whether the ACK mechanism is used.
};


} // namespace ns3

#endif /* ISA100_DL_H */
