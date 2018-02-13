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

#include <algorithm>
#include "ns3/boolean.h"


#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/traced-callback.h"
#include "ns3/node-container.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/application.h"
#include <ns3/random-variable-stream.h>

#include "ns3/packet.h"
#include "ns3/isa100-net-device.h"
#include "ns3/isa100-routing.h"
#include "ns3/isa100-dl.h"
#include "ns3/isa100-dl-header.h"
#include "ns3/isa100-dl-trailer.h"


namespace ns3 {

#define BROADCAST_ADDR 0xffff
#define DISTR_NO_PATH_BROADCAST 0xeeee // also found in the distributed routing algorithm

// Union for converting between an unsigned array of 2 bytes and a 16-bit number
typedef union
{
  uint16_t num16bit;
  uint8_t byte[2];
}uTwoBytes_t;


NS_LOG_COMPONENT_DEFINE ("Isa100Dl");


/*#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                   \
  std::clog << "[address " << m_shortAddress << "] ";
*/


NS_OBJECT_ENSURE_REGISTERED (Isa100DlSfSchedule);

Isa100DlSfSchedule::Isa100DlSfSchedule()
{
  m_currMultiFrameI = 0;
}

void Isa100DlSfSchedule::SetSchedule(
		const uint8_t* startHop, uint32_t numHop,
		uint16_t* startSlotSched, DlLinkType* startSlotType, uint32_t numSlots)
{
	m_dlHoppingPattern.assign(startHop,startHop+numHop);
	m_dlLinkScheduleSlots.assign(startSlotSched,startSlotSched+numSlots);
	m_dlLinkScheduleTypes.assign(startSlotType,startSlotType+numSlots);

	// One frame superframe
	m_multiFrameBounds.push_back(0);
}

void Isa100DlSfSchedule::SetSchedule(
		vector<uint8_t> hoppingPattern,
		vector<uint16_t> scheduleSlots,
		vector<DlLinkType> scheduleTypes)
{
	m_dlHoppingPattern = hoppingPattern;
	m_dlLinkScheduleSlots = scheduleSlots;
	m_dlLinkScheduleTypes = scheduleTypes;

	// One frame superframe
	m_multiFrameBounds.push_back(0);
}


std::vector<uint16_t> * Isa100DlSfSchedule::GetLinkSlotSchedule(void)
{
  return &m_dlLinkScheduleSlots;
}

std::vector<DlLinkType> * Isa100DlSfSchedule::GetLinkSlotTypes(void)
{
  return &m_dlLinkScheduleTypes;
}

std::vector<uint16_t> * Isa100DlSfSchedule::GetFrameBounds(void)
{
  return &m_multiFrameBounds;
}

Isa100DlSfSchedule::~Isa100DlSfSchedule()
{
	;
}

TypeId Isa100DlSfSchedule::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100DlSfSchedule")
    .SetParent<Object> ()
    .AddConstructor<Isa100DlSfSchedule> ()
    ;

    return tid;
}


NS_OBJECT_ENSURE_REGISTERED (Isa100Dl);


TypeId Isa100Dl::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100Dl")
    .SetParent<Object> ()

    .AddConstructor<Isa100Dl> ()

    .AddAttribute ("Address","16 bit DL address of node.",
    		Mac16AddressValue(),
    		MakeMac16AddressAccessor(&Isa100Dl::m_address),
    		MakeMac16AddressChecker())

    .AddAttribute ("SuperFramePeriod","Number of timeslots in superframe.",
    		UintegerValue(1),
    		MakeUintegerAccessor(&Isa100Dl::m_sfPeriod),
    		MakeUintegerChecker<uint16_t>())

    .AddAttribute ("SuperFrameSlotDuration","Duration of superframe slot (s)",
    		TimeValue( MilliSeconds (10)),
    		MakeTimeAccessor(&Isa100Dl::m_sfSlotDuration),
    		MakeTimeChecker())

    .AddAttribute ("SuperFrameSchedule","Hopping and link activity schedule",
    		PointerValue(),
    		MakePointerAccessor(&Isa100Dl::m_sfSchedule),
    		MakePointerChecker<Isa100DlSfSchedule>())

    .AddAttribute ("MaxFrameRetries","Max number of retries allowed after a transmission failure",
        UintegerValue(3),
        MakeUintegerAccessor(&Isa100Dl::m_maxFrameRetries),
        MakeUintegerChecker<uint8_t>(0,7))

    .AddAttribute ("MaxTxPowerDbm","Maximum transmit power (dBm)",
        IntegerValue(4),
        MakeIntegerAccessor(&Isa100Dl::m_maxTxPowerDbm),
        MakeIntegerChecker<int8_t>(-32,31))

    .AddAttribute ("MinTxPowerDbm","Minimum transmit power (dBm)",
            IntegerValue(-17),
            MakeIntegerAccessor(&Isa100Dl::m_minTxPowerDbm),
            MakeIntegerChecker<int8_t>(-32,31))

    .AddAttribute ("BackoffExponent","The backoff exponent determining the max backoff",
            UintegerValue(5),
            MakeUintegerAccessor(&Isa100Dl::m_backoffExponent),
            MakeUintegerChecker<uint8_t>())

    .AddAttribute ("ArqBackoffExponent","The backoff exponent determining the max arq backoff",
            UintegerValue(3),
            MakeUintegerAccessor(&Isa100Dl::m_arqBackoffExponent),
            MakeUintegerChecker<uint8_t>())

		.AddAttribute ("TxEarliest", "Earliest time a transmitter sends a packet after the start of a frame.",
            TimeValue (Seconds(2.212e-3)),  // Timing value from ISA100.11a
            MakeTimeAccessor (&Isa100Dl::m_xmitEarliest),
            MakeTimeChecker())

		.AddAttribute ("DlSleepEnabled", "Whether the DL is capable of sleeping.",
				BooleanValue (false),
				MakeBooleanAccessor (&Isa100Dl::m_dlSleepEnabled),
				MakeBooleanChecker())

	  .AddAttribute ("AckEnabled", "Whether the ACK mechanism is used in the DL.",
	  		BooleanValue (false),
				MakeBooleanAccessor (&Isa100Dl::m_ackEnabled),
				MakeBooleanChecker())

    .AddTraceSource ("DlTx",
    		"Trace source indicating a packet has arrived for transmission by this device",
    		MakeTraceSourceAccessor (&Isa100Dl::m_dlTxTrace),
				"ns3::TracedCallback::Packet")

    .AddTraceSource ("DlTxDrop",
    		"Trace source indicating a packet has been dropped by the device before transmission",
    		MakeTraceSourceAccessor (&Isa100Dl::m_dlTxDropTrace),
				"ns3::TracedCallback::Packet")

    .AddTraceSource ("DlRx",
    		"A packet has been received by this device, has been passed up from the physical layer "
    		"and is being forwarded up the local protocol stack.  This is a non-promiscuous trace,",
    		MakeTraceSourceAccessor (&Isa100Dl::m_dlRxTrace),
				"ns3::TracedCallback::Packet")

    .AddTraceSource ("DlRxDrop",
    		"Trace source indicating a packet was received, but dropped before being forwarded up the stack",
    		MakeTraceSourceAccessor (&Isa100Dl::m_dlRxDropTrace),
				"ns3::TracedCallback::Packet")

    .AddTraceSource ("DlForward",
    		"Trace source for packets forwarded on by an intermediate routing node.",
    		MakeTraceSourceAccessor (&Isa100Dl::m_dlForwardTrace),
				"ns3::TracedCallback::Packet")

    .AddTraceSource("InfoDropTrace",
                    " Trace source with all dropped packets and info about why they were dropped",
                    MakeTraceSourceAccessor (&Isa100Dl::m_infoDropTrace),
                    "ns3::TracedCallback::Packet")

    .AddTraceSource("ProcessLinkTrace",
                    " Trace source tracking Dl timeslots and the process link function",
                    MakeTraceSourceAccessor (&Isa100Dl::m_processLinkTrace),
                    "ns3::TracedCallback::DlInfo")

    .AddTraceSource("DlTaskTrace",
                    " Trace source tracking Dl tasks",
                    MakeTraceSourceAccessor (&Isa100Dl::m_dlTaskTrace),
                    "ns3::TracedCallback::DlInfo")

    .AddTraceSource("RetrxTrace",
                    " Trace source indicating when a retransmission occurs",
                    MakeTraceSourceAccessor (&Isa100Dl::m_retrxTrace),
                    "ns3::TracedCallback::DlInfo")

  ;
  return tid;
}




Isa100Dl::Isa100Dl ()
{
	NS_LOG_FUNCTION (this);

	m_dlHopIndex = 0;
	m_dlLinkIndex = 0;
	m_expBackoffCounter = 0;
	m_expArqBackoffCounter = 0;
	m_tdmaPktsLeft = 0;

	for(int i=0; i<256; i++)
	{
	  m_packetTxSeqNum[i] = 0;
		m_nextRxPacketSeqNum[i] = 0;
		m_txPowerDbm[i] = 100; // set to an invalid transmit power level
	}
	m_usePowerCtrl = 0;

	m_address = Mac16Address::Allocate();


  // GGM: What's this even for?
  //  UniformVariable uniformVar;
  //  uniformVar = UniformVariable (0, 255);

  m_backoffExponent = 5;

  Simulator::Schedule(Time(Seconds(0.0)),&Isa100Dl::Start,this);

  m_routingAlgorithm = 0;

	m_uniformRv = CreateObject<UniformRandomVariable>();

	// Timing values from 802.15.4
	m_minLIFSPeriod = Seconds(40.0/62500.0);

	// Timing values from ISA100.11a
	m_clockError = Seconds(1.0/32000);
	m_xmitEarliest = Seconds(2.212e-3);

	m_processor = 0;
	m_dlSleepEnabled = false;
	m_ackEnabled = false;


	// Logging/results variables
	m_numFramesSent = 0;
	m_numRetrx = 0;
	m_numFramesDrop = 0;

}

Isa100Dl::~Isa100Dl ()
{
}


void Isa100Dl::Start()
{
	NS_LOG_FUNCTION (this);
	m_dlTaskTrace(m_address, "Dl started");

	if(m_sfSchedule->m_dlLinkScheduleSlots.empty())
		NS_FATAL_ERROR("No superframe schedule programmed into net device.");

	Time clockError = Seconds(m_clockError.GetSeconds() * m_uniformRv->GetValue(0.0,1.0));
	NS_LOG_LOGIC(" Clock Error: " << clockError.GetSeconds() << "s");

	m_nextProcessLink = Simulator::Schedule(Time(m_sfSlotDuration* (m_sfSchedule->m_dlLinkScheduleSlots[0])) + clockError,&Isa100Dl::ProcessLink,this);
}

void Isa100Dl::DoDispose ()
{
	NS_LOG_FUNCTION (this);
  m_dlTaskTrace(m_address, "Dl ended");

  for (uint32_t i = 0; i < m_txQueue.size (); i++)
    {
      m_txQueue[i]->m_packet = 0;
      delete m_txQueue[i];
    }
  m_txQueue.clear ();

  m_dlDataConfirmCallback = MakeNullCallback< void, DlDataConfirmParams > ();
  m_dlDataIndicationCallback = MakeNullCallback< void, DlDataIndicationParams, Ptr<Packet> > ();
  m_dlWokeUpCallback = MakeNullCallback<void >();
  m_dlFrameCompleteCallback = MakeNullCallback<void, uint16_t >();
  m_dlInactiveSlotsCallback = MakeNullCallback<void, uint16_t >();
  m_plmeSetAttribute = MakeNullCallback< void, ZigbeePibAttributeIdentifier, ZigbeePhyPIBAttributes* > ();
  m_plmeCcaRequest = MakeNullCallback< void > ();
  m_plmeSetTrxStateRequest = MakeNullCallback< void, ZigbeePhyEnumeration > ();
  m_pdDataRequest = MakeNullCallback< void, uint32_t, Ptr<Packet> > ();
  m_plmeSleepFor = MakeNullCallback< void, Time > ();

}

void Isa100Dl::SetDlSfSchedule(Ptr<Isa100DlSfSchedule> schedule)
{
	NS_LOG_FUNCTION (this);
  m_dlTaskTrace(m_address, "Super frame schedule set");

	m_sfSchedule = schedule;
}


void Isa100Dl::ChannelHop()
{
	NS_LOG_FUNCTION (this << m_address << Simulator::Now().GetSeconds());

	if(!m_sfSchedule)
		NS_FATAL_ERROR("Null ISA100 superframe schedule pointer.");

	uint32_t scheduleSize = m_sfSchedule->m_dlHoppingPattern.size();
	uint8_t channelNum = m_sfSchedule->m_dlHoppingPattern[m_dlHopIndex++ % scheduleSize];

	ZigbeePibAttributeIdentifier id = phyCurrentChannel;
	ZigbeePhyPIBAttributes attribute;

	attribute.phyCurrentChannel = channelNum;

	NS_LOG_LOGIC (" Hopping to channel " << (uint16_t)channelNum);
//	Not interested in hopping info right now, uncomment to add functionality
//  std::stringstream ss;
//  ss << "Hop to channel " << (uint16_t)channelNum;
//  std::string msg = ss.str();
//  m_dlTaskTrace(m_address, msg);

	/*
	 * A few notes here:
	 * - Need to check if this function actually changes channel number all the way through to the spectrum phy.
	 * - Seems to have a lot of fussy business with catching a change in the middle of a transmission.  Shouldn't
	 *   be an issue if we change on frame boundaries but could force a TRX state change before calling this function
	 *   just to be safe.
	 */

	if(!m_plmeSetAttribute.IsNull())
		m_plmeSetAttribute(id,&attribute);
	else
		NS_FATAL_ERROR("m_plmeSetAttribute null.");

}

void Isa100Dl::ProcessTrxStateRequest(ZigbeePhyEnumeration state)
{
	NS_LOG_FUNCTION(this << m_address << Simulator::Now().GetSeconds());

	/*
	 * Assume that when the DL asks the PHY layer to sleep, it sleeps as well and the processor is switched off.
	 * Currently, this should only occur during idle slots.  Any other state change requires the processor to be on.
	 */

	if(m_processor){
		if(state == PHY_SLEEP)
			m_processor->SetState(PROCESSOR_SLEEP);
		else
			m_processor->SetState(PROCESSOR_ACTIVE);
	}

	// Task log
  std::stringstream ss;
  ss << "Request that the transceiver state is changed to " << ZigbeePhyEnumNames[state];
  std::string msg = ss.str();
  m_dlTaskTrace(m_address, msg);

  if(!m_plmeSetTrxStateRequest.IsNull())
    m_plmeSetTrxStateRequest(state);
  else
    NS_FATAL_ERROR("m_plmeSetTrxStateRequest is null.");
}

void Isa100Dl::ProcessLink()
{
	NS_LOG_FUNCTION(this << m_address);

	NS_LOG_LOGIC(this << " " << m_address << " " << Simulator::Now().GetSeconds());

	if(!m_sfSchedule)
		NS_FATAL_ERROR("Null ISA100 superframe schedule pointer.");

	// Hop channels
	ChannelHop();

	uint16_t scheduleSize = m_sfSchedule->m_dlLinkScheduleTypes.size();

	DlLinkType linkType = m_sfSchedule->m_dlLinkScheduleTypes[m_dlLinkIndex % scheduleSize];

	NS_LOG_LOGIC(" Link Type: " << linkType);

  // Trace process link
  m_processLinkTrace(m_address, linkType, m_txQueue.size(), m_expBackoffCounter, m_expArqBackoffCounter);

  // Receive if this is a dedicated receive slot or if it's shared and we have
  // nothing to send.
  if(linkType == RECEIVE || (linkType == SHARED && !m_txQueue.size())){

    NS_LOG_LOGIC(" Setting PHY to Rx On for a single slot.");

    ProcessTrxStateRequest(IEEE_802_15_4_PHY_RX_ON);
  }


  // We can only transmit if there's a packet in the queue and we're not in backoff.
  if(linkType == SHARED && m_txQueue.size() && !m_expBackoffCounter){
  	NS_LOG_LOGIC(" Packet to transmit on shared link, requesting CCA in " << m_xmitEarliest.GetSeconds() << "s");
  	Simulator::Schedule(m_xmitEarliest,&Isa100Dl::CallPlmeCcaRequest,this);
  }

  if(linkType == TRANSMIT){
    NS_LOG_LOGIC(" Setting PHY to Tx On.");

    if(m_expBackoffCounter){
      NS_LOG_LOGIC(" Zeroing backoff counter since we are now in a dedicated transmit slot.");
      m_expBackoffCounter = 0;
    }

    if(m_xmitEarliest == Seconds(0.0))
    	ProcessTrxStateRequest(IEEE_802_15_4_PHY_TX_ON);
    else
    	Simulator::Schedule(m_xmitEarliest, &Isa100Dl::ProcessTrxStateRequest, this,IEEE_802_15_4_PHY_TX_ON);
  }


  if(m_expBackoffCounter > 0){
    m_expBackoffCounter--;
    NS_LOG_LOGIC(" Decrementing backoff counter, value:" << m_expBackoffCounter);
  }

	uint16_t currentSlot = m_sfSchedule->m_dlLinkScheduleSlots[m_dlLinkIndex++ % scheduleSize];
	uint16_t nextSlot = m_sfSchedule->m_dlLinkScheduleSlots[m_dlLinkIndex % scheduleSize];
	uint16_t slotJump;

	NS_LOG_LOGIC(" Current Slot Index: " << currentSlot << " Next Slot Index: " << nextSlot);

	if(nextSlot <= currentSlot)
		slotJump = (nextSlot + m_sfPeriod)- currentSlot;
	else
		slotJump = nextSlot - currentSlot;

	NS_LOG_LOGIC(" Process link scheduled " << slotJump << " slots into the future ("
	             << Time(m_sfSlotDuration*slotJump).GetSeconds() << "s in the future)");

	m_nextProcessLink = Simulator::Schedule(Time(m_sfSlotDuration*slotJump),&Isa100Dl::ProcessLink,this);


  // If there are upcoming idle slots turn off the transceiver for them
	// Note that the transceiver is under the control of the processor but the processor doesn't have visibility
	// into the superframe schedule.  So, we assume the protocol stack is smart enough to put the transceiver to sleep in
	// idle slots.
  if (slotJump > 1)
  {
  	if(m_dlSleepEnabled){
      NS_LOG_LOGIC(" PHY_SLEEP in " << m_sfSlotDuration.GetSeconds() << "s");
      Simulator::Schedule(m_sfSlotDuration,&Isa100Dl::ProcessTrxStateRequest,this,PHY_SLEEP);
  	}
  	else{
      NS_LOG_LOGIC(" TRX Off in " << m_sfSlotDuration.GetSeconds() << "s");
      Simulator::Schedule(m_sfSlotDuration,&Isa100Dl::ProcessTrxStateRequest,this,IEEE_802_15_4_PHY_TRX_OFF);
  	}
  }
}

void Isa100Dl::CallPlmeCcaRequest()
{
	NS_LOG_FUNCTION(this << m_address << Simulator::Now().GetSeconds());

  // Task log
  m_dlTaskTrace(m_address, "CCA is requested");

	if(!m_plmeCcaRequest.IsNull())
		m_plmeCcaRequest();
	else
		NS_FATAL_ERROR("m_plmeCcaRequest is null.");
}

bool Isa100Dl::IsAckPacket(Ptr<const Packet> p)
{
	// Always false if we're not using ACKs
	if( !m_ackEnabled  )
		return false;

  // The MHR sub-header is the same for both headers so we can safely start assuming an ack header.
  Isa100DlAckHeader ackHeader;
  p->PeekHeader(ackHeader);

  // The source and dest addr modes should be zero
  if (ackHeader.GetMhrFrameControl().dstAddrMode != 0){
    return false;
  }
  if (ackHeader.GetMhrFrameControl().srcAddrMode != 0){
     return false;
   }

  // Should be an ACK header, just double check the reserved signature of the DHR sub-header.
  if (ackHeader.GetDhrFrameControl().reserved != 3){
     return false;
   }

  // For the time being, this check is sufficient. It may need to be improved if headers become more sophisticated
  return true;
}

void Isa100Dl::PlmeCcaConfirm(ZigbeePhyEnumeration status)
{
	NS_LOG_FUNCTION (this << m_address << Simulator::Now().GetSeconds());


	if(status == IEEE_802_15_4_PHY_IDLE){
		NS_LOG_LOGIC(" CCA indicates idle channel, turning Tx on.");

		m_dlTaskTrace(m_address, "CCA reported an idle channel");

		// Make sure queue hasn't been flushed during cca
		if (m_txQueue.size() != 0){
		  ProcessTrxStateRequest((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_TX_ON);
		}
	}
	else{

		m_expBackoffCounter = (uint32_t)(m_uniformRv->GetValue(0,pow(2,m_backoffExponent-1)));

		NS_LOG_LOGIC(" CCA indicates busy channel, starting backoff.");

		// Task log
    std::stringstream ss;
    ss << "CCA reported a busy channel. Backoff counter set to " << m_expBackoffCounter;
    std::string msg = ss.str();
    m_dlTaskTrace(m_address, msg);

    // Put the transceiver into RX while in backoff
    ProcessTrxStateRequest((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_RX_ON);

	}
}


void Isa100Dl::PlmeSetTrxStateConfirm(ZigbeePhyEnumeration status)
{
	NS_LOG_FUNCTION (this << m_address << Simulator::Now().GetSeconds() << status);

	// If the transmitter has been switched on, we have a packet to send.
	if(status == IEEE_802_15_4_PHY_TX_ON)
	{
    NS_LOG_LOGIC(" Set TRX state confirmed (Tx on): " << status);
    m_dlTaskTrace(m_address, "Transceiver state TX_ON has been confirmed. Getting ready to transmit the next queued packet.");

    if(m_txQueue.size() == 0)
    {
    	// If nothing to transmit, then turn off the transceiver
    	ProcessTrxStateRequest(IEEE_802_15_4_PHY_TRX_OFF);
    	return;
    }
    NS_LOG_DEBUG(" " << m_txQueue.size() << " packets to transmit.");


    TxQueueElement *txQElement = m_txQueue.front ();

    Mac16Address nextNodeAddr;
    uTwoBytes_t buffer;
    uint8_t nextNodeInd;

    if(IsAckPacket(txQElement->m_packet)){
    	Isa100DlAckHeader ackHdr;
    	txQElement->m_packet->PeekHeader(ackHdr);
    	nextNodeAddr = ackHdr.GetShortDstAddr();
    }
    else{
    	Isa100DlHeader header;
    	txQElement->m_packet->PeekHeader(header);
    	nextNodeAddr = header.GetShortDstAddr();
    }


    nextNodeAddr.CopyTo(buffer.byte);
    nextNodeInd = buffer.byte[1];

    if(m_usePowerCtrl){

    	// Obtain and format tx power for PHY layer
    	int8_t txPower = m_txPowerDbm[nextNodeInd];

  		ZigbeePibAttributeIdentifier id = phyTransmitPower;
  		ZigbeePhyPIBAttributes attribute;

  		attribute.phyTransmitPower = txPower;

    	NS_LOG_DEBUG(" Tx Power Control " << m_address << " -> " << nextNodeAddr << "(" << (int)nextNodeInd << "): " << (int)txPower << "dBm");

  		// Set the tx power attribute
  		if(!m_plmeSetAttribute.IsNull())
  			m_plmeSetAttribute(id,&attribute);
  		else
  			NS_FATAL_ERROR("m_plmeSetAttribute null.");


/*
    	uint8_t check = txPower & 0xE0;

    	// Make sure the requested power can be represented in 6 bits
    	if(check == 0x00 || check == 0xE0){
    		txPower = txPower & 0x3F;

    		ZigbeePibAttributeIdentifier id = phyTransmitPower;
    		ZigbeePhyPIBAttributes attribute;

    		attribute.phyTransmitPower = txPower;

      	NS_LOG_DEBUG(" Tx Power Control " << m_address << " -> " << nextNodeAddr << ": " << (int)txPower << "dBm");

    		// Set the tx power attribute
    		if(!m_plmeSetAttribute.IsNull())
    			m_plmeSetAttribute(id,&attribute);
    		else
    			NS_FATAL_ERROR("m_plmeSetAttribute null.");
    	}
    	else
    		NS_LOG_ERROR("Could not convert tx power to 6 bits. "
    				"Not able to use Power Control!"); */
    }
    else
    	NS_LOG_DEBUG("Not Using Transmit Power Control.");


    // If in arq backoff don't transmit anything (n/a for acks since they're sent in the same timeslot)

    // GGM: I'm not sure I agree with this.  If we're in ARQ backoff and shouldn't do anything then why did we let the transceiver
    // turn on.  Shouldn't this be done in ProcessLink?


    if(m_expArqBackoffCounter > 0 && !IsAckPacket(txQElement->m_packet))
    {
    	m_expArqBackoffCounter--;

    	NS_LOG_DEBUG(" In ARQ backoff, returning.");

    	return;
    }

    // We're using ACKs but we have no attempts remaining.
    if(m_ackEnabled && txQElement->m_txAttemptsRem == 0)
    {

    	NS_LOG_LOGIC(" Packet could not be transmitted after " << m_maxFrameRetries << " retries. Drop packet.");

    	m_dlTxDropTrace(m_address,m_txQueue.front()->m_packet);
    	m_infoDropTrace(m_address,m_txQueue.front()->m_packet, "Dl exhausted all possible links and transmit attempts for this packet.");
    	m_numFramesDrop++;

    	// Inform the upper layer of a failure
    	DlDataConfirmParams params;
    	params.m_dsduHandle = txQElement->m_dsduHandle;
    	params.m_status = FAILURE;

    	txQElement->m_packet = 0;
    	delete txQElement;
    	m_txQueue.pop_front ();

    	if(!m_dlDataConfirmCallback.IsNull())
    		m_dlDataConfirmCallback(params);

    	return;
    }

    // The first attempt of sending a data packet
    else if(!m_ackEnabled || ( !IsAckPacket(txQElement->m_packet) && (txQElement->m_txAttemptsRem == m_maxFrameRetries + 1) ) )
    {

    	NS_LOG_DEBUG(" First packet transmit attempt.");

    	Isa100DlHeader header;

    	// GGM: I'm going to increment the sequence number here.. double check to see if that's a problem for the ACK mechanism.


    	// Set the sequence number
    	txQElement->m_packet->RemoveHeader(header);
    	header.SetSeqNum(m_packetTxSeqNum[nextNodeInd]++);
    	txQElement->m_packet->AddHeader(header);

    	if(m_ackEnabled){
    		// Decrement transmit attempts remaining for the packet
    		txQElement->m_txAttemptsRem--;

    		// Increase the number of frames sent counter
    		m_numFramesSent++;

    		// Set the arq backoff counter
    		m_expArqBackoffCounter = (uint32_t)(m_uniformRv->GetValue(0,pow(2,m_arqBackoffExponent-1)));
    	}
    }

    // This is a retransmission attempt of a data packet
    else if (m_ackEnabled && !IsAckPacket(txQElement->m_packet))
    {
    	NS_LOG_DEBUG(" Data packet retransmission attempt. " << txQElement->m_txAttemptsRem << " retries remaining.");

    	// Indicate a retransmission is happening
    	m_retrxTrace(m_address);

    	// Decrement transmit attempts remaining for the packet
    	txQElement->m_txAttemptsRem--;

    	// Increase the number of retransmissions
    	m_numRetrx++;

    	// Set the arq backoff counter
    	m_expArqBackoffCounter = (uint32_t)(m_uniformRv->GetValue(0,pow(2,m_arqBackoffExponent-1)));
    }

    // This is the first attempt of an ACK packet
    else if ( IsAckPacket(txQElement->m_packet) )
    {
    	NS_LOG_DEBUG(" ACK packet sent. " << txQElement->m_txAttemptsRem << " attempts remaining.");

    	// Decrement transmit attempts remaining for the packet

    	// GGM: Shouldn't we only decrement txAttempts when we send the data packet?  Do we keep a separate attempts counter for both the ack and data packets?
    	txQElement->m_txAttemptsRem--;

    	// Increase the number of frames sent counter
    	m_numFramesSent++;
    }

    // Should never get here
    else
    	NS_FATAL_ERROR("Reached an impossible DL state.");

    // Transmit the packet
    if(!m_pdDataRequest.IsNull())
    	m_pdDataRequest(txQElement->m_packet->GetSize(),txQElement->m_packet);
    else
    	NS_FATAL_ERROR("m_pdDataRequest is null");

    return;
 	}
}

void Isa100Dl::PlmeWakeUp (void)
{
  // Use the time calculated when going to sleep to schedule the next process link
  // (this keeps the slot intervals multiples of each other before & after sleep)
  // likewise for channel hop
  m_nextProcessLink = Simulator::Schedule(m_nextProcessLinkDelay,&Isa100Dl::ProcessLink, this);

  if(!m_dlWokeUpCallback.IsNull()){
    NS_LOG_LOGIC("DL Layer on Node " << m_address << " is awake once again at time: " << Simulator::Now());
    m_dlTaskTrace(m_address, "Woke up from sleep");
    m_dlWokeUpCallback();
  }
}


void Isa100Dl::PdDataConfirm(ZigbeePhyEnumeration status)
{
	NS_LOG_FUNCTION (this << m_address << Simulator::Now().GetSeconds());

  // Task log
  std::stringstream ss;
  ss << "Phy data request confirmed with status " << ZigbeePhyEnumNames[status];
  std::string msg = ss.str();
  m_dlTaskTrace(m_address, msg);

	DlDataConfirmParams params;

  TxQueueElement *txQElement = m_txQueue.front ();

	if(status == IEEE_802_15_4_PHY_SUCCESS)
	{
		if(m_ackEnabled)
		{
			// Check if packet is an ACK
			if (IsAckPacket (txQElement->m_packet))
			{
				txQElement->m_packet = 0;
				delete txQElement;
				m_txQueue.pop_front ();

				// ACKs are only sent during RX or SHARED slots, so the transceiver
				// should be put back to RX_ON now that the ACK has sent
				ProcessTrxStateRequest((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_RX_ON);

				return;
			}

			else
			{
				// Check if we are expecting an ACK
				Isa100DlHeader header;
				txQElement->m_packet->PeekHeader(header);

				if (header.GetDhdrFrameControl().ackReq == 1)
				{
					// Expecting to receive an ACK; don't remove the packet yet
					// Set the TRX to receive
					ProcessTrxStateRequest((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_RX_ON);
					return;
				}
			}
		}

		// Can remove the packet from the queue and confirm
		params.m_dsduHandle = txQElement->m_dsduHandle;
		params.m_status = SUCCESS;

		Isa100DlHeader dataHdr;
		txQElement->m_packet->PeekHeader(dataHdr);

		txQElement->m_packet = 0;
		delete txQElement;
		m_txQueue.pop_front ();

		// Only confirm to higher layer if the packet originated at this node
		if (dataHdr.GetDaddrSrcAddress() == m_address)
		{
			if(!m_dlDataConfirmCallback.IsNull())
				m_dlDataConfirmCallback(params);
		}

		NS_LOG_LOGIC(" PHY packet transmission confirmed.");
	}

	// If the transceiver isn't in the correct state, assume a transmission takes
	// priority, and try to turn it on again.
	else if(status == IEEE_802_15_4_PHY_RX_ON || status == IEEE_802_15_4_PHY_TRX_OFF){

		// Caution: Is there a mechanism where the PHY refused to turn on?  If so, this loop is endless.

    ProcessTrxStateRequest((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_TX_ON);

		NS_LOG_LOGIC(" PHY packet not transmitted, PHY not in correct state.  Trying to turn on again.");
	}

	// GGM: Actually, this could be called with a PHY_SLEEP state too..

	// Can only be status == IEEE_802_15_4_TX_BUSY
	// If transmitter is busy, the PHY is being overwhelmed and the packet is dropped.
	else{

		m_dlTxDropTrace(m_address,txQElement->m_packet);
    m_infoDropTrace(m_address,txQElement->m_packet, "PHY is busy transmitting another packet.");
		m_numFramesDrop++;

    // Clear the arq backoff
    m_expArqBackoffCounter = 0;

    params.m_dsduHandle = txQElement->m_dsduHandle;
    params.m_status = FAILURE;

    txQElement->m_packet = 0;
    delete txQElement;
    m_txQueue.pop_front ();

    if(!m_dlDataConfirmCallback.IsNull())
    	m_dlDataConfirmCallback(params);
    NS_LOG_LOGIC(" PHY busy transmitting another packet and is being overwhelmed.  Drop packet.");
	}

}


void Isa100Dl::PdDataIndication(uint32_t size, Ptr<Packet> p, uint32_t lqi, double rxPowDbm)
{

  NS_LOG_FUNCTION (this << size << p << lqi << m_address << Simulator::Now().GetSeconds());

  // Task log
  std::stringstream ss;
  ss << "Phy indicated that data was received with an SINR of " << 10*log10(lqi) << " dB";
  std::string msg = ss.str();
  m_dlTaskTrace(m_address, msg);

  Time delay = m_minLIFSPeriod;

  NS_LOG_DEBUG(" MAC delay " << delay.GetSeconds());

	Simulator::Schedule(delay,&Isa100Dl::ProcessPdDataIndication,this,size,p,lqi, rxPowDbm);

}


void Isa100Dl::ProcessPdDataIndication(uint32_t size, Ptr<Packet> p, uint32_t lqi, double rxPowDbm)
{
  NS_LOG_FUNCTION (this << size << *p << lqi << m_address << Simulator::Now().GetSeconds());
  m_dlTaskTrace(m_address, "Processed the received data");


  if (m_ackEnabled && IsAckPacket (p))
  {
  	Isa100DlAckHeader ackHdr;
  	p->PeekHeader(ackHdr);

  	// Get the unique identifier of the packet being ACK'd
  	uint32_t ackDmic = ackHdr.GetDmic();

  	// If that packet is in the tx queue, it can be removed
  	std::deque<TxQueueElement*>::iterator it = m_txQueue.begin();

  	while (it != m_txQueue.end())
  	{
  		Ptr<Packet> currPacket = (*it)->m_packet;
  		if (IsAckPacket(currPacket))
  		{
  			it++;
  			continue;
  		}
  		Isa100DlHeader dataHdr;
  		currPacket->PeekHeader(dataHdr);
  		uint32_t dmic = dataHdr.GetDmic();
  		if (dmic == ackDmic)
  		{
  			// Found Ack'd packet
  			// Clear the arq backoff counter
  			m_expArqBackoffCounter = 0;

  			// Increment seq number
  			uint8_t buffer[2];
  			Mac16Address destAddr = dataHdr.GetShortDstAddr();
  			destAddr.CopyTo(buffer);
  			uint8_t destNodeInd = buffer[1];

  			m_packetTxSeqNum[destNodeInd] = dataHdr.GetSeqNum() + 1;

  			DlDataConfirmParams params;
  			params.m_dsduHandle = (*it)->m_dsduHandle;
  			params.m_status = SUCCESS;

  			// Remove queue item
  			(*it)->m_packet = 0;
  			delete *it;
  			m_txQueue.erase(it);

  			// Only confirm to higher layer if the packet originated at this node
  			if (dataHdr.GetDaddrSrcAddress() == m_address)
  			{
  				if(!m_dlDataConfirmCallback.IsNull())
  					m_dlDataConfirmCallback(params);
  			}

  			m_dlRxTrace(m_address,p);
  			NS_LOG_LOGIC(" ACK Confirmed: Ack with DMIC " << ackDmic << " received at node " << m_address);

  			return;
  		}

  		// Increment iterator
  		it++;
  	}

  	// Ack'd packet could not be found in the tx queue
  	NS_LOG_LOGIC(" ACK Ignored:  Ack with DMIC " << ackDmic << " received at node "
  			<< m_address << ", but corresponding packet in Tx Queue could not be found.");
  }


  else
  {
  	Isa100DlHeader rxDlHdr;
  	Isa100DlTrailer trailer;
  	Ptr<Packet> packetData = p->Copy();
  	Ptr<Packet> origPacket = p->Copy();
  	packetData->RemoveHeader(rxDlHdr);

  	uTwoBytes_t finalDestBuffer;
  	uTwoBytes_t shortDestBuffer;
  	uTwoBytes_t shortSrcBuffer;
  	rxDlHdr.GetDaddrDestAddress().CopyTo(finalDestBuffer.byte);
  	rxDlHdr.GetShortDstAddr().CopyTo(shortDestBuffer.byte);
  	rxDlHdr.GetShortSrcAddr().CopyTo(shortSrcBuffer.byte);
  	uint8_t srcNodeInd = shortSrcBuffer.byte[1];
  	bool forwardPacketOn = false;


  	// Else check for an address match
  	if( rxDlHdr.GetShortDstAddr() == m_address)
  	{
  		// Check if an ACK needs to be sent
  		if (m_ackEnabled && rxDlHdr.GetDhdrFrameControl().ackReq == 1)
  		{
  			// Construct the ACK Packet
  			Ptr<Packet> ack = Create<Packet>(0);
  			Isa100DlAckHeader ackHdr;

  			// Set the destination address
  			ackHdr.SetShortDstAddr(rxDlHdr.GetShortSrcAddr());

  			// Use the DMIC of the packet as the unique identifier
  			ackHdr.SetDmic(rxDlHdr.GetDmic());

  			ack->AddHeader(ackHdr);
  			NS_LOG_LOGIC(" ACK ready: " << *ack);
  			NS_LOG_LOGIC(" ACK Response: Node " << m_address << " received a data packet from " << rxDlHdr.GetShortSrcAddr() <<
  					" and is responding to ACK request with DMIC " << ackHdr.GetDmic());

  			m_dlTxTrace(m_address,ack);

  			// Add the ACK to the front of the queue and transmit it
  			TxQueueElement *txQElement = new TxQueueElement;
  			txQElement->m_packet = ack;
  			txQElement->m_txAttemptsRem = 1;  // Attempt to send the ack just once

  			m_txQueue.push_front(txQElement);

  			ProcessTrxStateRequest((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_TX_ON);

  			return;
  		}

  		if(m_routingAlgorithm)
  		{

  			// Need to remove the trailer to obtain just the packet data
  			packetData->RemoveTrailer(trailer);


  			// GGM: Can we use this instead of programming the transmit power list into the node when we create its schedule?
  			// This received power thing is just for the distributed FA routing but there's no reason why we couldn't use it for everything.

  			// Update the tx power for this neighbour
  			double chLossDb = trailer.GetDistrRoutingTxPower() - rxPowDbm;
  			SetTxPowerDbm(chLossDb - 101, srcNodeInd);

  			// Process the rx packet
  			m_routingAlgorithm->ProcessRxPacket(p,forwardPacketOn);

  		}


  		if(forwardPacketOn)
  		{
  			// Create new DMIC for this packet
  			uint64_t pVal= (uint64_t)PeekPointer(p);
  			uint32_t dmic = (uint32_t)pVal;

  			// Modify header
  			Isa100DlHeader header;
  			p->RemoveHeader(header);
  			header.SetDmic(dmic);

  			// Return the modified header to the packet.
  			p->AddHeader(header);

  			TxQueueElement *txQElement = new TxQueueElement;
  			txQElement->m_dsduHandle = 0;
  			txQElement->m_packet = p;
  			txQElement->m_txAttemptsRem = m_maxFrameRetries + 1; // number of retries plus the initial tx attempt

  			m_txQueue.push_back(txQElement);

  			m_dlForwardTrace(m_address,p);

  			return;

  		}

  		// Accept any packet with a sequence number >= to what we're expecting.

  		// GGM: Disabled this check because we need to handle the 8 bit wrap around (and really redesign this whole DL..)
  		else if (1 || rxDlHdr.GetSeqNum() >= m_nextRxPacketSeqNum[srcNodeInd])
  		{
  			m_nextRxPacketSeqNum[srcNodeInd]= rxDlHdr.GetSeqNum() + 1;
  			m_dlRxTrace(m_address,origPacket);
  			NS_LOG_LOGIC(" Packet received successfully at node address " << m_address << " (Time: " << Simulator::Now().GetSeconds() << ")");

  			DlDataIndicationParams params;
  			params.m_srcAddr = rxDlHdr.GetDaddrSrcAddress();
  			params.m_destAddr = rxDlHdr.GetDaddrDestAddress();
  			params.m_dsduLength = size;

  			if(!m_dlDataIndicationCallback.IsNull())
  				m_dlDataIndicationCallback(params,packetData);

  			return;
  		}
  	}

  	// At this point, we either didn't have an address match or the sequence number was wrong.
  	m_dlRxDropTrace(m_address,origPacket);

  	std::stringstream msg;
  	msg << " Packet Dropped:  Hop dest " << rxDlHdr.GetShortDstAddr() << " received at node "
  			<< m_address << " from " << rxDlHdr.GetShortSrcAddr() << ", Seq num: " << (uint16_t)rxDlHdr.GetSeqNum() << " (expected: " << (uint16_t)m_nextRxPacketSeqNum[srcNodeInd] << ")";

		m_infoDropTrace(m_address,origPacket, msg.str());

  	NS_LOG_LOGIC(msg.str());

  }


}


void Isa100Dl::DlDataRequest (DlDataRequestParams params, Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << *p << m_address << Simulator::Now().GetSeconds());

  // Task log
  std::stringstream ss;
  ss << "A request has been made to send data to " << params.m_destAddr;
  std::string msg = ss.str();
  m_dlTaskTrace(m_address, msg);

  NS_LOG_LOGIC(" Sending packet from " << params.m_srcAddr << " to " << params.m_destAddr);

  Isa100DlHeader dlHdr;
  TxQueueElement *txQElement = new TxQueueElement;

  dlHdr.SetDaddrSrcAddress(params.m_srcAddr);
  dlHdr.SetDaddrDestAddress(params.m_destAddr);

  uTwoBytes_t buffer;
  params.m_destAddr.CopyTo(buffer.byte);

  	if( m_ackEnabled ){

  		DhdrFrameControl frameCtrl = dlHdr.GetDhdrFrameControl();
  		frameCtrl.ackReq = 1;
  		dlHdr.SetDhdrFrameControl(frameCtrl);

  		// Calculate DMIC
  		uint64_t pVal= (uint64_t)PeekPointer(p);
  		uint32_t dmic = (uint32_t)pVal;
  		dlHdr.SetDmic(dmic);

  		// Set generation time for tracing
  		dlHdr.SetTimeGeneratedNS(Simulator::Now().GetNanoSeconds());

  		NS_LOG_LOGIC(" ACK Requested: Node " << m_address << " is sending a data packet to " << dlHdr.GetShortDstAddr() <<
  				" and requests an ACK with DMIC " << dmic);

  		// Number of retries plus the initial tx attempt
  		txQElement->m_txAttemptsRem = m_maxFrameRetries + 1;
  }


//  NS_ASSERT_MSG(m_routingAlgorithm != 0, "DlDataRequest: No routing algorithm defined!");
  	if(m_routingAlgorithm)
  		m_routingAlgorithm->PrepTxPacketHeader(dlHdr);

  p->AddHeader (dlHdr);

  NS_LOG_LOGIC(" Packet ready: " << *p);

  m_dlTxTrace(m_address,p);

  // Add packet to the queue
  txQElement->m_packet = p;
  txQElement->m_dsduHandle = params.m_dsduHandle;
  m_txQueue.push_back(txQElement);

}


double Isa100Dl::CalculateAvgRetrx (void) const
{
  if (m_numFramesSent == 0){
    return 0.0;
  }

  return ((double)m_numRetrx / (double)m_numFramesSent);
}

double Isa100Dl::CalculateDropRatio (void) const
{
  if (m_numFramesSent == 0){
    return 0.0;
  }
  return ((double)m_numFramesDrop / (double)m_numFramesSent);
}

Time Isa100Dl::GetTimeToNextSlot (void)
{
  Time timeToSlot = Time::From(m_nextProcessLink.GetTs()) - Simulator::Now();
  NS_ASSERT_MSG(timeToSlot >= Seconds(0.0), "Something is not right with next process link timing.");

  return timeToSlot;
}

void Isa100Dl::FlushTxQueue(void)
{
  for (uint32_t i = 0; i < m_txQueue.size (); i++)
  {
    m_dlTxDropTrace(m_address,m_txQueue[i]->m_packet);
    m_infoDropTrace(m_address,m_txQueue[i]->m_packet, "Packet was flushed out of the Dl Tx Queue by a higher layer.");
    m_numFramesDrop++;

    m_txQueue[i]->m_packet = 0;
    delete m_txQueue[i];
  }
  m_txQueue.clear ();
}


void Isa100Dl::SetProcessor(Ptr<Isa100Processor> processor)
{
	m_processor = processor;
}


void Isa100Dl::SetPlmeCcaRequestCallback (PlmeCcaRequestCallback c)
{
	NS_LOG_FUNCTION (this);

	m_plmeCcaRequest = c;
}

void Isa100Dl::SetPlmeSetTrxStateRequestCallback (PlmeSetTrxStateRequestCallback c)
{
	NS_LOG_FUNCTION (this);


	m_plmeSetTrxStateRequest = c;
}

void Isa100Dl::SetPdDataRequestCallback (PdDataRequestCallback c)
{
	NS_LOG_FUNCTION (this);

	m_pdDataRequest = c;
}

void Isa100Dl::SetPlmeSetAttributeCallback (PlmeSetAttributeCallback c)
{
	NS_LOG_FUNCTION (this);

	m_plmeSetAttribute = c;
}

void Isa100Dl::SetPlmeSleepForCallback (PlmeSleepForCallback c)
{
  NS_LOG_FUNCTION (this);

  m_plmeSleepFor = c;
}

void Isa100Dl::SetDlDataIndicationCallback(DlDataIndicationCallback c)
{
  NS_LOG_FUNCTION (this);

  m_dlDataIndicationCallback = c;
}

void Isa100Dl::SetDlDataConfirmCallback(DlDataConfirmCallback c)
{
  NS_LOG_FUNCTION (this);

  m_dlDataConfirmCallback = c;
}

void Isa100Dl::SetDlWokeUpCallback (DlWokeUpCallback c)
{
  NS_LOG_FUNCTION (this);

  m_dlWokeUpCallback = c;
}

void Isa100Dl::SetDlFrameCompleteCallback(DlFrameCompleteCallback c)
{
  NS_LOG_FUNCTION(this);

  m_dlFrameCompleteCallback = c;
}

void Isa100Dl::SetDlInactiveSlotsCallback(DlInactiveSlotsCallback c)
{
  NS_LOG_FUNCTION(this);

  m_dlInactiveSlotsCallback = c;
}

void Isa100Dl::SetRoutingAlgorithm(Ptr<Isa100RoutingAlgorithm> routingAlgorithm)
{
	NS_LOG_FUNCTION (this);

	m_routingAlgorithm = routingAlgorithm;
}

Ptr<Isa100RoutingAlgorithm> Isa100Dl::GetRoutingAlgorithm()
{
  NS_LOG_FUNCTION (this);

	return m_routingAlgorithm;
}

void Isa100Dl::SetTxPowersDbm(double * txPowers, uint8_t numNodes)
{
  NS_LOG_FUNCTION (this << txPowers << numNodes);

  // Indicate that we're using power control
  m_usePowerCtrl = 1;

  int8_t val;

	std::stringstream ss;

	ss << "Set Tx Pwr for " << m_address << ": ";

  // PHY layer stores max tx power as a 6-bit signed int so the double needs to be converted and range checked
  for (uint32_t i = 0; i < numNodes; i++)
  {
  	if(txPowers[i] > (double)m_maxTxPowerDbm)
  		val = m_maxTxPowerDbm;
  	else if (txPowers[i] < (double)m_minTxPowerDbm)
  		val = m_minTxPowerDbm;
  	else
  		val = (int8_t)ceil(txPowers[i]);


    // check if tx power is outside of range 6-bit range (-32 to 31)
    if (val < -32)
      val = -32;

    else if (val > 31)
      val = 31;

    m_txPowerDbm[i] = val;

    ss << "(" << i << "," << txPowers[i] << " > " << (int)val << ") ";
  }
  NS_LOG_DEBUG(ss.str());
}

int8_t* Isa100Dl::GetTxPowersDbm (void)
{
  return m_txPowerDbm;
}

void Isa100Dl::SetTxPowerDbm(double txPower, uint8_t destNodeI)
{
  NS_LOG_FUNCTION (this << txPower << destNodeI);

  int8_t val;

  // PHY layer stores max tx power as a 6-bit signed int so the double needs to be converted and range checked
  val = (int8_t)ceil(txPower);

  // Ensure that the tx power is within the min and max
  if (val > m_maxTxPowerDbm)
    val = m_maxTxPowerDbm;

  else if (val < m_minTxPowerDbm)
    val = m_minTxPowerDbm;

  // check if tx power is outside of range 6-bit range (-32 to 31)
  if (val < -32)
    val = -32;

  else if (val > 31)
    val = 31;

  m_txPowerDbm[destNodeI] = val;
}

int8_t Isa100Dl::GetTxPowerDbm (uint8_t destNodeI)
{
  return m_txPowerDbm[destNodeI];
}

} // namespace ns3
