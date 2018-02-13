/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 The University Of Calgary- FISHLAB
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
 * Author: Geoff Messier <gmessier@ucalgary.ca>
 *         Michael Herrmann <mjherrma@ucalgary.ca>
 *
 * Based on the PHY by Gary Pei <guangyu.pei@boeing.com>
 */

#include "ns3/zigbee-phy.h"

#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/simulator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"

#include "ns3/mac16-address.h"
#include "ns3/isa100-dl-header.h"
#include "ns3/zigbee-trx-current-model.h"
#include "fish-wpan-spectrum-signal-parameters.h"
#include "isa100-error-model.h"
#include "fish-wpan-spectrum-value-helper.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/antenna-model.h"
#include "ns3/mobility-model.h"
#include "ns3/spectrum-channel.h"
#include "ns3/packet-burst.h"

#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("ZigbeePhy");

using namespace ns3;
using namespace std;

NS_OBJECT_ENSURE_REGISTERED (ZigbeePhy);

const char * ZigbeePhyEnumNames[] = {"BUSY","BUSY_RX","BUSY_TX","FORCE_TRX_OFF","IDLE","INVALID_PARAM","RX_ON","SUCCESS",
                                     "TRX_OFF","TX_ON","UNSUPPORTED_ATTRIBUTE","READ_ONLY","UNSPECIFIED","SLEEP"};

// Table 22 in section 6.4.1 of ieee802.15.4
const uint32_t ZigbeePhy::aMaxPhyPacketSize = 127; // max PSDU in octets
const uint32_t ZigbeePhy::aTurnaroundTime = 12;  // RX-to-TX or TX-to-RX in symbol periods

// Assume a PHY operating at 2.45 GHz and following Section 6.5
// - A symbol is defined as 32 OQPSK modulated spreading chips and 4 bits
//   are mapped to each symbol.
// - The chip rate is 2 Mchip/sec (32 times the symbol rate).
// - This gives a brick wall double sided bandwidth of 2~MHz but Section 6.5.3.1 allows
//   for a 7~MHz bandwidth before the signal must be 20dB down from peak.
// dataRate = 250e3 bit/sec
// symbolRate = 62.5e3 sym/sec


// For now, the spectrum PHY code assumes a 10MHz channel (ie the channel stretches from the
// centre frequencies of the two neighbouring channels.  The despreading and symbol de-mapping
// of the PHY layer reduces to a signal with a 2~MHz/32*4 = 250~kHz.  So, the noise
// should be reduced by a factor of 10MHz/250kHz = 40 or 16.2 dB.

// The spreading gain accounts for going from a 2~MHz signal bandwidth to a 250~kbit/sec
// data stream.  This results in a linear processing gain of 8.0.

double spreadingGain = 8.0; // linear gain


TypeId
ZigbeePhy::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ZigbeePhy")
    .SetParent<Object> ()
    .AddConstructor<ZigbeePhy> ()

    .AddAttribute ("SupplyVoltage",
                  "The voltage of the energy supply (V).",
                  DoubleValue (3.0),  // in Volts
                  MakeDoubleAccessor (&ZigbeePhy::SetSupplyVoltage,
                                      &ZigbeePhy::GetSupplyVoltage),
                  MakeDoubleChecker<double> (0))
    .AddAttribute ("PhyBitRate",
                   "The bit rate of the phy in bits/second.",
                   DoubleValue (250e3),  // Default a PHY operating at 2.45 GHz and following Section 6.5
                   MakeDoubleAccessor (&ZigbeePhy::m_bitRate),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("PhySymbolRate",
                   "The symbol rate of the phy in symbols/second.",
                   DoubleValue (62.5e3), // Default is 4 bits mapped to each symbol
                   MakeDoubleAccessor (&ZigbeePhy::m_symbolRate),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("NoiseFloorDbm", // Noise floor for 250 kbit/s
                   "The noise floor of the receiver (dBm)",
                   DoubleValue (-120.0),
                   MakeDoubleAccessor (&ZigbeePhy::m_noiseFloorDbm),
                   MakeDoubleChecker<double>())
    .AddAttribute ("SensitivityDbm",
                   "The sensitivity of the receiver (dBm)",
                   DoubleValue (-101.0), // Sensitivity for the RF233 is -101 dBm for 250kbit/s.
                   MakeDoubleAccessor (&ZigbeePhy::m_rxSensitivityDbm),
                   MakeDoubleChecker<double>())
    .AddAttribute ("NoiseFigureDbm",
                   "The noise figure of the receiver (dBm)",
                   DoubleValue (6.0), // Default noise figure for RF233.
                   MakeDoubleAccessor (&ZigbeePhy::m_noiseFigureDbm),
                   MakeDoubleChecker<double>())


    .AddTraceSource ("TrxState",
                     "The state of the transceiver",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_trxStateLogger),
										 "ns3::TracedCallback::ZigbeePhyEnumeration")
    .AddTraceSource ("PhyTxBegin",
                     "Trace source indicating a packet has begun transmitting over the channel medium",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_phyTxBeginTrace),
										 "ns3::TracedCallback::Packet")
    .AddTraceSource ("PhyTxEnd",
                     "Trace source indicating a packet has been completely transmitted over the channel.",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_phyTxEndTrace),
										 "ns3::TracedCallback::Packet")
    .AddTraceSource ("PhyTxDrop",
                     "Trace source indicating a packet has been dropped by the device during transmission",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_phyTxDropTrace),
										 "ns3::TracedCallback::Packet")
    .AddTraceSource ("PhyRxBegin",
                     "Trace source indicating a packet has begun being received from the channel medium by the device",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_phyRxBeginTrace),
										 "ns3::TracedCallback::Packet")
    .AddTraceSource ("PhyRxEnd",
                     "Trace source indicating a packet has been completely received from the channel medium by the device",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_phyRxEndTrace),
										 "ns3::TracedCallback::Packet")
    .AddTraceSource ("PhyRxDrop",
                     "Trace source indicating a packet has been dropped by the device during reception",
                     MakeTraceSourceAccessor (&ZigbeePhy::m_phyRxDropTrace),
										 "ns3::TracedCallback::Packet")


    .AddTraceSource("InfoDropTrace",
                    " Trace source with all dropped packets and info about why they were dropped",
                    MakeTraceSourceAccessor (&ZigbeePhy::m_infoDropTrace),
                    "ns3::TracedCallback::Packet")
    .AddTraceSource("PhyTaskTrace",
                    " Trace source tracking Phy tasks",
                    MakeTraceSourceAccessor (&ZigbeePhy::m_phyTaskTrace),
                    "ns3::TracedCallback::PhyInfo")
  ;
  return tid;
}

ZigbeePhy::ZigbeePhy ()
  : m_edRequest (),
    m_setTRXState ()
{
  m_currentDraws = CreateObject<ZigbeeTrxCurrentModel>();

  m_trxState = IEEE_802_15_4_PHY_TRX_OFF;
  ChangeTrxState ((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_TRX_OFF);
  m_trxStatePending = IEEE_802_15_4_PHY_TRX_OFF;

  // default PHY PIB attributes
  m_phyPIBAttributes.phyCurrentChannel = 11;
  m_phyPIBAttributes.phyTransmitPower = 4.0;  // Max tx power for RF233.
  m_phyPIBAttributes.phyCurrentPage = 0;
  for (uint32_t i = 0; i < 32; i++)
    {
      m_phyPIBAttributes.phyChannelsSupported[i] = 0x07ffffff;
    }
  m_phyPIBAttributes.phyCCAMode = 2;

  m_rxEdPeakPower = 0.0;
  m_rxTotalPower = 4.0;  // TxPower in dBm.
  m_rxTotalNum = 0;

  FishWpanSpectrumValueHelper psdHelper;
  m_txPsd = psdHelper.CreateTxPowerSpectralDensity (m_phyPIBAttributes.phyTransmitPower,
                                                    m_phyPIBAttributes.phyCurrentChannel);
  m_noise = psdHelper.CreateNoisePowerSpectralDensity (m_phyPIBAttributes.phyCurrentChannel);
  m_rxPsd = 0;
  Ptr <Packet> none = 0;
  m_currentRxPacket.m_packet = 0;
  m_currentRxPacket.m_isCorrupt = false;
  m_currentTxPacket.m_packet = 0;
  m_currentTxPacket.m_isCorrupt = false;
  m_errorModel = 0;

	m_random = CreateObject<UniformRandomVariable>();


  int nTypes = 7;
  string energyTypes[] = {
  		"Broadcast",
  		"Data",
  		"Ack",
  		"BusyRx",
  		"RxOn",
  		"TxOn",
  		"TrxOff"
  };

  m_energyCategories.assign(energyTypes,energyTypes+nTypes);

  m_supplyVoltage = 0;
  m_lastUpdateTime = Seconds(0.0);
}

ZigbeePhy::~ZigbeePhy ()
{
}

void
ZigbeePhy::DoDispose ()
{
  NS_LOG_FUNCTION (this);
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Phy ended");

  m_mobility = 0;
  m_device = 0;
  m_channel = 0;
  m_txPsd = 0;
  m_rxPsd = 0;
  m_noise = 0;
  m_errorModel = 0;
  m_pdDataIndicationCallback = MakeNullCallback< void, uint32_t, Ptr<Packet>, uint32_t, double > ();
  m_pdDataConfirmCallback = MakeNullCallback< void, ZigbeePhyEnumeration > ();
  m_plmeCcaConfirmCallback = MakeNullCallback< void, ZigbeePhyEnumeration > ();
  m_plmeEdConfirmCallback = MakeNullCallback< void, ZigbeePhyEnumeration,uint8_t > ();
  m_plmeGetAttributeConfirmCallback = MakeNullCallback< void, ZigbeePhyEnumeration, ZigbeePibAttributeIdentifier, ZigbeePhyPIBAttributes* > ();
  m_plmeSetTRXStateConfirmCallback = MakeNullCallback< void, ZigbeePhyEnumeration > ();
  m_plmeSetAttributeConfirmCallback = MakeNullCallback< void, ZigbeePhyEnumeration, ZigbeePibAttributeIdentifier > ();
  m_phyDropCallback=MakeNullCallback<void, ZigbeePhyDropSource>();


  SpectrumPhy::DoDispose ();
}

Ptr<NetDevice>
ZigbeePhy::GetDevice () const
{
//  NS_LOG_FUNCTION (this);
  return m_device;
}


Ptr<MobilityModel>
ZigbeePhy::GetMobility ()
{
//  NS_LOG_FUNCTION (this);
  return m_mobility;
}


void
ZigbeePhy::SetDevice (Ptr<NetDevice> d)
{
  NS_LOG_FUNCTION (this << d);
  m_device = d;
}


void
ZigbeePhy::SetMobility (Ptr<MobilityModel> m)
{
  NS_LOG_FUNCTION (this << m);
  m_mobility = m;
}


void
ZigbeePhy::SetChannel (Ptr<SpectrumChannel> c)
{
  NS_LOG_FUNCTION (this << c);
  m_channel = c;
}


Ptr<SpectrumChannel>
ZigbeePhy::GetChannel (void)
{
  return m_channel;
}


Ptr<const SpectrumModel>
ZigbeePhy::GetRxSpectrumModel () const
{
  if (m_txPsd)
    {
      return m_txPsd->GetSpectrumModel ();
    }
  else
    {
      return 0;
    }
}

Ptr<AntennaModel>
ZigbeePhy::GetRxAntenna ()
{
  return m_antenna;
}

void
ZigbeePhy::SetAntenna (Ptr<AntennaModel> a)
{
  NS_LOG_FUNCTION (this << a);
  m_antenna = a;
}

vector<string>&
ZigbeePhy::GetEnergyCategories()
{
	return m_energyCategories;
}

void ZigbeePhy::DecrementChannelRxSignals()
{
  NS_LOG_FUNCTION (this);

	m_rxTotalNum--;

  NS_LOG_LOGIC(" Number of 802.15.4 signals in channel decremented to " << m_rxTotalNum);
}

void
ZigbeePhy::StartRx (Ptr<SpectrumSignalParameters> spectrumRxParams)
{
  NS_LOG_FUNCTION (this << spectrumRxParams << " Time(s): " << Simulator::Now().GetSeconds());

  // Don't receive if sleeping
  if (m_trxState == PHY_SLEEP)
    return;

  double rxPowerDbm = 10*log10((*(spectrumRxParams->psd))[m_phyPIBAttributes.phyCurrentChannel - 11]*2.0e6*1000);
  std::stringstream ss;
  ss << "Packet arrived at receiver, Rx Power: " << rxPowerDbm << " dBm";
  std::string msg = ss.str();
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), msg);

  FishWpanSpectrumValueHelper psdHelper;

  // Copy in the received signal information and the packet (all contained in ZigbeeSpectrumSignalParameters).
  Ptr<FishWpanSpectrumSignalParameters> lrWpanRxParams = DynamicCast<FishWpanSpectrumSignalParameters> (spectrumRxParams);

  if (!lrWpanRxParams)
  	return;

  // Check to make sure the received signal is in our channel.
  double linNoiseFloor = pow(10.0, m_noiseFloorDbm / 10.0) / 1000.0;
  if( (*(spectrumRxParams->psd))[m_phyPIBAttributes.phyCurrentChannel - 11]*2.0e6 < linNoiseFloor )
  	return;

  // Increment the received signal counter to indicate that an 802.15.4 compliant signal has been
  // detected in the channel.
  m_rxTotalNum++;
  NS_LOG_LOGIC(" Number of 802.15.4 signals in channel incremented to " << m_rxTotalNum);

  Time duration = lrWpanRxParams->duration;
  Simulator::Schedule (duration, &ZigbeePhy::DecrementChannelRxSignals, this);

  // If state is RX_ON, transition to BUSY_RX and process packet.
  if( m_trxState == IEEE_802_15_4_PHY_RX_ON ){

  	NS_LOG_LOGIC(" TRX in RX_ON, starting packet reception.");
    m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Started receiving the packet from the channel");

  	Ptr<Packet> p = (lrWpanRxParams->packetBurst->GetPackets ()).front ();

  	m_currentRxPacket.m_packet = p;
  	m_currentRxPacket.m_isCorrupt = false;
  	m_rxPsd = lrWpanRxParams->psd;
  	m_rxTotalPower = psdHelper.TotalAvgPower (*m_rxPsd);

  	Simulator::Schedule (duration, &ZigbeePhy::EndRx, this);
  	m_phyRxBeginTrace (p);

  	ChangeTrxState((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_BUSY_RX);
  	m_trxStatePending = IEEE_802_15_4_PHY_IDLE;

  	return;
  }

  // If state is BUSY_RX, this means a collision has occured.
  // - For now, drop both packets but this could be improved to an SNIR based method.
  if( m_trxState == IEEE_802_15_4_PHY_BUSY_RX ){

  	NS_LOG_LOGIC(" TRX in RX_BUSY, dropping both packets.");

  	// Arriving packet doesn't even get placed in arrival trace.
  	// GGM: This means that the channel will be occupied with corrupt, useless packets only until the end of the
  	//      first packet.  It should actually remain occupied until the end of the last packet to transmit so
  	//      channel congestion will be under-estimated.
  	Ptr<Packet> p = (lrWpanRxParams->packetBurst->GetPackets ()).front ();
  	m_phyRxDropTrace(p);
  	m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),p, "Phy is already busy receiving another packet.");

  	// Flag current packet as corrupted.  Will be dropped by EndRx().
  	m_currentRxPacket.m_isCorrupt = true;

  	return;
  }

  // If state is anything else, drop the packet.
  NS_LOG_LOGIC(" TRX not in receive state, dropping incoming packet.");

  Ptr<Packet> p = (lrWpanRxParams->packetBurst->GetPackets ()).front ();
  m_phyRxDropTrace(p);

  std::stringstream strs;
  strs << "Phy is in state " << ZigbeePhyEnumNames[m_trxState] << ", and cannot receive packets.";
  msg = strs.str();
  m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),p, msg);

  return;

}

void ZigbeePhy::EndRx ()
{
  NS_LOG_FUNCTION (this << " Time(s): " << Simulator::Now().GetSeconds());
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Finished receiving a packet from the channel");
  FishWpanSpectrumValueHelper psdHelper;

  // If the packet is flagged as corrupt, drop immediately.
  if(m_currentRxPacket.m_isCorrupt){

  	NS_LOG_LOGIC(" Packet previously corrupted, dropping.");
  	m_phyRxDropTrace(m_currentRxPacket.m_packet);
    m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),m_currentRxPacket.m_packet, "Phy received another packet while receiving this one.");
  }

  // Use error model to determine if the packet is intact.
  else if (m_errorModel != 0){

  	// The TotalAvgPower function integrates across the PSD.  Useful for frequency selective channels
  	// later on.
    double noiseFactor = pow(10.0, m_noiseFigureDbm / 10.0);
  	double sinr = psdHelper.TotalAvgPower (*m_rxPsd) / psdHelper.TotalAvgPower (*m_noise) * spreadingGain / noiseFactor;

  	// The received power of the signal
    double rxPowerDbm = 10*log10(psdHelper.TotalAvgPower(*m_rxPsd)*1000);

  	NS_LOG_DEBUG(
  			" RxPower: " << rxPowerDbm << " dBm, " <<
  			" NoisePower: " << 10*log10(psdHelper.TotalAvgPower(*m_noise)*1000/spreadingGain) << " dBm"
  	);


  	NS_ASSERT(m_currentRxPacket.m_packet);
  	Ptr<Packet> p = m_currentRxPacket.m_packet;

  	// Simplified calculation; the chunk is the whole packet
  	double per = 1.0 - m_errorModel->GetChunkSuccessRate (sinr, p->GetSize () * 8);

  	NS_LOG_DEBUG(" PER: " << per << " for SNR " << 10*log10(sinr) << "dB and " << p->GetSize()*8 << " bits");

  	// Packet ok.
  	if (m_random->GetValue (0,1.0) > per){
  		NS_LOG_DEBUG (" Reception success!");
  		m_phyRxEndTrace (Mac16Address::ConvertFrom(m_device->GetAddress()), p, sinr);
  		if (!m_pdDataIndicationCallback.IsNull ()){
  			m_pdDataIndicationCallback (p->GetSize (), p, (uint32_t)sinr, rxPowerDbm);
  		}
  	}
  	// Packet corrupt.
  	else{
  		NS_LOG_DEBUG (" Reception failure!");
  		m_phyRxDropTrace (p);
  	  m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),p, "Phy determined that bits were randomly corrupted.");
  	}
  }

  // Error model is missing so just assume the packet is ok.
  else{
  	NS_LOG_WARN ("Missing ErrorModel");
  	Ptr<Packet> p = m_currentRxPacket.m_packet;
  	m_phyRxEndTrace (Mac16Address::ConvertFrom(m_device->GetAddress()), p, 0);

  	if (!m_pdDataIndicationCallback.IsNull ()){
  		m_pdDataIndicationCallback (p->GetSize (), p, 0, 0);
  	}
  }

  m_rxTotalPower = psdHelper.TotalAvgPower (*m_noise);
  m_currentRxPacket.m_packet = 0;
  m_currentRxPacket.m_isCorrupt = false;

  // Check if a state change has been requested during the received frame.
  if (m_trxStatePending != IEEE_802_15_4_PHY_IDLE){

  	NS_LOG_LOGIC ("Apply pending state change to " << m_trxStatePending);
  	ChangeTrxState (m_trxStatePending);
  	m_trxStatePending = IEEE_802_15_4_PHY_IDLE;

  	if (!m_plmeSetTRXStateConfirmCallback.IsNull ()){
  	    m_plmeSetTRXStateConfirmCallback (IEEE_802_15_4_PHY_SUCCESS);
  	}
  }
  // Otherwise, stay in receive mode.
  else{
  	ChangeTrxState((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_RX_ON);
  }
}

void
ZigbeePhy::PdDataRequest (const uint32_t psduLength, Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << psduLength << *p << " Time of Request: " << Simulator::Now().GetSeconds());
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Requested to transmit a packet");

  // Don't transmit if sleeping
  if (m_trxState == PHY_SLEEP){
    if (!m_pdDataConfirmCallback.IsNull ())
    {
      m_pdDataConfirmCallback (PHY_SLEEP);
      m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),p, "Phy rejected data request because phy is sleeping.");
    }
    return;
  }

  if (psduLength > aMaxPhyPacketSize)
    {
      if (!m_pdDataConfirmCallback.IsNull ())
        {
          m_pdDataConfirmCallback (IEEE_802_15_4_PHY_UNSPECIFIED);
        }
      NS_LOG_DEBUG ("Drop packet because psduLength too long: " << psduLength);
      m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),p, "Phy rejected data request because packet is too long.");

      return;
    }


  // Transmit the packet only if the TRX is TX_ON.
  if (m_trxState == IEEE_802_15_4_PHY_TX_ON){

  	NS_ASSERT (m_channel);

  	/*
  	 * GGM:
  	 * Really!?  Do we really create a new dynamic txParams for every transmitted packet.  Seems
  	 * v wasteful.  Other observations:
  	 * - We don't seem to hook the txPHY object to anything.  Why not to this PHY?
  	 * - The PacketBurst object only has one object.
  	 * - It must be m_channel (SpectrumChannel) that figures out which PHY(s) receives?
  	 */

  	Ptr<FishWpanSpectrumSignalParameters> txParams = Create<FishWpanSpectrumSignalParameters> ();
  	txParams->duration = Seconds (p->GetSize() * 8.0 / m_bitRate);
  	txParams->txPhy = GetObject<SpectrumPhy> ();
  	txParams->psd = m_txPsd;
  	txParams->txAntenna = m_antenna;

  	Ptr<PacketBurst> pb = CreateObject<PacketBurst> ();
  	pb->AddPacket (p);
  	txParams->packetBurst = pb;


  	NS_LOG_LOGIC(" Duration of packet (us): " << (txParams->duration).GetMicroSeconds());


  	m_channel->StartTx (txParams);
    m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Started transmitting a packet");

  	m_phyTxBeginTrace (p);
  	m_currentTxPacket.m_packet = p;
  	m_currentTxPacket.m_isCorrupt = false;

    m_pdDataRequest = Simulator::Schedule (txParams->duration, &ZigbeePhy::EndTx, this);
    ChangeTrxState ((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_BUSY_TX);
  	return;
  }

  // Otherwise, we need to drop the packet.
  else{

  	if (!m_pdDataConfirmCallback.IsNull ()){
  		m_pdDataConfirmCallback (m_trxState);
  	}
  	m_phyTxDropTrace (p);

  	std::stringstream ss;
    ss << "Phy is in state " << m_trxState << ", and cannot transmit packets.";
    std::string msg = ss.str();
    m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),p, msg);
  	return;
  }
}


void
ZigbeePhy::EndTx ()
{
  NS_LOG_FUNCTION (this);
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Finished transmitting a packet");

  if (m_currentTxPacket.m_isCorrupt == false){

  	NS_LOG_DEBUG (" Packet successfully transmitted");
  	m_phyTxEndTrace (m_currentTxPacket.m_packet);

  	if (!m_pdDataConfirmCallback.IsNull ()){
  		m_pdDataConfirmCallback (IEEE_802_15_4_PHY_SUCCESS);
  	}
  }
  else{
  	NS_LOG_DEBUG (" Packet transmission aborted");
  	m_phyTxDropTrace (m_currentTxPacket.m_packet);
    m_infoDropTrace(Mac16Address::ConvertFrom(m_device->GetAddress()),m_currentTxPacket.m_packet, "Phy changed channels during transmission and corrupted the packet.");

  	if (!m_pdDataConfirmCallback.IsNull ()){
  		m_pdDataConfirmCallback (m_trxState);
  	}
  }

  m_currentTxPacket.m_packet = 0;       // Null out the packet.
  m_currentTxPacket.m_isCorrupt = false;  // Set the transmission condition to success.

  // Check if a state change was requested during the transmitted frame.
  if (m_trxStatePending != IEEE_802_15_4_PHY_IDLE)
  {

  	NS_LOG_LOGIC (" Apply pending state change to " << m_trxStatePending);
  	ChangeTrxState (m_trxStatePending);
  	m_trxStatePending = IEEE_802_15_4_PHY_IDLE;

  	if (!m_plmeSetTRXStateConfirmCallback.IsNull ()){
  		m_plmeSetTRXStateConfirmCallback (IEEE_802_15_4_PHY_SUCCESS);
  	}
  }
  // Otherwise, switch back into TX ON mode
  else
  {

  	//GGM: Change this so the transceiver swtiches off rather than sends another packet.

  	ChangeTrxState ((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_TRX_OFF);

/*
  	ChangeTrxState ((ZigbeePhyEnumeration)IEEE_802_15_4_PHY_TX_ON);

  	// Allow the dl to transmit again if another packet is queued
    if (!m_plmeSetTRXStateConfirmCallback.IsNull ()){
      m_plmeSetTRXStateConfirmCallback (IEEE_802_15_4_PHY_TX_ON);
    }
    */
  }
}



void
ZigbeePhy::PlmeCcaRequest (void)
{
  NS_LOG_FUNCTION (this);
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Requested to perform CCA");

  // Do not process CCA requests if sleeping
  if (m_trxState == PHY_SLEEP){
    if (!m_plmeCcaConfirmCallback.IsNull()){
      m_plmeCcaConfirmCallback (m_trxState);
    }
  }

  // GGM: To make the DL code less annoying, allow a CCA check in TX_ON and TRX_OFF.  This assumes
  //      a quick switch to rx mode and back again.

  if (m_trxState == IEEE_802_15_4_PHY_RX_ON || m_trxState == IEEE_802_15_4_PHY_TX_ON || m_trxState == IEEE_802_15_4_PHY_TRX_OFF){
  	// The standard states the CCA detection occurs in 8 symbol periods.
  	Time ccaTime = Seconds (8.0 / m_symbolRate );
  	NS_LOG_LOGIC(" CCA will end in " << ccaTime.GetSeconds() << "s");

  	Simulator::Schedule (ccaTime, &ZigbeePhy::EndCca, this);
  }
  else{
  	if (!m_plmeCcaConfirmCallback.IsNull ()){
  		m_plmeCcaConfirmCallback (m_trxState);
  	}
  }
}



void
ZigbeePhy::EndCca ()
{
  NS_LOG_FUNCTION (this << Simulator::Now().GetSeconds());
  m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), "Finished CCA");
  ZigbeePhyEnumeration sensedChannelState;

  double linSensitivity = pow (10.0, m_rxSensitivityDbm / 10.0) / 1000.0;
  NS_LOG_LOGIC(" PhyBusy: " << PhyIsBusy() << " Number of Rx Signals: " << m_rxTotalNum << " Energy Threshold: " << m_rxTotalPower/linSensitivity);

  // This first check just looks at transceiver state.  Channel can't be idle
  // if this TRX is in the middle of sending or receiving.
  if ( PhyIsBusy () ){
  	sensedChannelState = IEEE_802_15_4_PHY_BUSY;
  }

  // ED only as per 6.9.9
  // - ED threshold at most 10 dB above receiver sensitivity
  else if (m_phyPIBAttributes.phyCCAMode == 1){
  	if (m_rxTotalPower / linSensitivity >= 10.0){
  		sensedChannelState = IEEE_802_15_4_PHY_BUSY;
  	}
  	else{
  		sensedChannelState = IEEE_802_15_4_PHY_IDLE;
  	}
  }

  // CCA only as per 6.9.9
  else if (m_phyPIBAttributes.phyCCAMode == 2){
  	if (m_rxTotalNum > 0){
  		sensedChannelState = IEEE_802_15_4_PHY_BUSY;
  	}
  	else{
  		sensedChannelState = IEEE_802_15_4_PHY_IDLE;
  	}
  }

  // Both CCA and ED, as per 6.9.9
  else if (m_phyPIBAttributes.phyCCAMode == 3){
  	if ((m_rxTotalPower / linSensitivity >= 10.0)
  			&& (m_rxTotalNum > 0)){
  		sensedChannelState = IEEE_802_15_4_PHY_BUSY;
  	}
  	else{
  		sensedChannelState = IEEE_802_15_4_PHY_IDLE;
  	}
  }
  else
  	NS_FATAL_ERROR("Incorrect CCA mode");

  NS_LOG_LOGIC (" Channel sensed state: " << sensedChannelState);

  if (!m_plmeCcaConfirmCallback.IsNull ()){
  	m_plmeCcaConfirmCallback (sensedChannelState);
  }
}



void
ZigbeePhy::PlmeEdRequest (void)
{
  NS_LOG_FUNCTION (this);

  // Do not process ED requests if sleeping
    if (m_trxState == PHY_SLEEP){
      if (!m_plmeEdConfirmCallback.IsNull()){
        m_plmeEdConfirmCallback (m_trxState,0);
      }
    }

  if (m_trxState == IEEE_802_15_4_PHY_RX_ON)
    {
      m_rxEdPeakPower = m_rxTotalPower;
      Time edTime = Seconds (8.0 / m_symbolRate);
      m_edRequest = Simulator::Schedule (edTime, &ZigbeePhy::EndEd, this);
    }
  else
    {
      if (!m_plmeEdConfirmCallback.IsNull ())
        {
          m_plmeEdConfirmCallback (m_trxState,0);
        }
    }
}


void
ZigbeePhy::EndEd ()
{
  NS_LOG_FUNCTION (this);
  uint8_t energyLevel;

  // Per IEEE802.15.4-2006 sec 6.9.7
  double linSensitivity = pow (10.0, m_rxSensitivityDbm / 10.0) / 1000.0;
  double ratio = m_rxEdPeakPower / linSensitivity;
  ratio = 10.0 * log10 (ratio);
  if (ratio <= 10.0)
    {  // less than 10 dB
      energyLevel = 0;
    }
  else if (ratio >= 40.0)
    { // less than 40 dB
      energyLevel = 255;
    }
  else
    {
      // in-between with linear increase per sec 6.9.7
      energyLevel = (uint8_t)((ratio / 10.0 - 1.0) * (255.0 / 3.0));
    }

  if (!m_plmeEdConfirmCallback.IsNull ())
    {
      m_plmeEdConfirmCallback (IEEE_802_15_4_PHY_SUCCESS, energyLevel);
    }
}



void
ZigbeePhy::PlmeGetAttributeRequest (ZigbeePibAttributeIdentifier id)
{
  NS_LOG_FUNCTION (this << id);

  ZigbeePhyEnumeration status;

  switch (id)
    {
    case phyCurrentChannel:
    case phyChannelsSupported:
    case phyTransmitPower:
    case phyCCAMode:
    case phyCurrentPage:
    case phyMaxFrameDuration:
    case phySHRDuration:
    case phySymbolsPerOctet:
      {
        status = IEEE_802_15_4_PHY_SUCCESS;
        break;
      }
    default:
      {
        status = IEEE_802_15_4_PHY_UNSUPPORTED_ATTRIBUTE;
        break;
      }
    }

  if (m_trxState == PHY_SLEEP){
    status = PHY_SLEEP;
  }

  if (!m_plmeGetAttributeConfirmCallback.IsNull ())
    {
      ZigbeePhyPIBAttributes retValue;
      memcpy (&retValue, &m_phyPIBAttributes, sizeof(ZigbeePhyPIBAttributes));
      m_plmeGetAttributeConfirmCallback (status,id,&retValue);
    }
}

void ZigbeePhy::PlmeSetTRXStateRequest (ZigbeePhyEnumeration state)
{
  NS_LOG_FUNCTION (this << state << Simulator::Now().GetSeconds());

  // Prevent the trace from being flooded from each process link rx on request if nothing is happening
  if (state != IEEE_802_15_4_PHY_RX_ON)
  {
    std::stringstream ss;
    ss << "Requested to change state to " << ZigbeePhyEnumNames[state];
    std::string msg = ss.str();
    m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), msg);
  }

  /*
   * ZigbeeModification:
   * - No longer implements force TRX off.
   * - Also doesn't implement the turnaround delay for switching from transmit to receive mode.
   */

  NS_ABORT_IF ( (state != IEEE_802_15_4_PHY_RX_ON)
                && (state != IEEE_802_15_4_PHY_TRX_OFF)
                && (state != IEEE_802_15_4_PHY_TX_ON)
								&& (state != PHY_SLEEP));

  NS_LOG_LOGIC ("Trying to set m_trxState from " << m_trxState << " to " << state);

  // this method always overrides previous state setting attempts.
  // This includes both cancelling a currently running state change and
  //  setting the pending state to idle.
  if (m_trxStatePending != IEEE_802_15_4_PHY_IDLE){
      m_trxStatePending = IEEE_802_15_4_PHY_IDLE;
  }
  if (m_setTRXState.IsRunning ()){
  	NS_LOG_DEBUG ("Cancel m_setTRXState");
  	m_setTRXState.Cancel ();
  }

  // Current State == Desired State
  if (state == m_trxState){
  	if (!m_plmeSetTRXStateConfirmCallback.IsNull ()){
  		m_plmeSetTRXStateConfirmCallback (state);
  	}
  	return;
  }

  // Current State = TRX Off || Rx On (but idle) || Tx On (but idle) || Sleeping
  // Action: Go immediately to the requested state.
    if( m_trxState == IEEE_802_15_4_PHY_RX_ON
    		|| m_trxState == IEEE_802_15_4_PHY_TRX_OFF
    		|| m_trxState == IEEE_802_15_4_PHY_TX_ON
				|| m_trxState == PHY_SLEEP){

  	ChangeTrxState(state);

  	if (!m_plmeSetTRXStateConfirmCallback.IsNull ()){
  		m_plmeSetTRXStateConfirmCallback (state);
  	}
  	return;
  }

  // Current State = Busy Transmitting
  // Action: Set pending state and let EndTx() take care of the state change.
  if( m_trxState == IEEE_802_15_4_PHY_BUSY_TX ){
  	NS_LOG_DEBUG (" Phy busy transmitting; setting state pending to " << state);
  	m_trxStatePending = state;
  	return;
  }

  // Current State = Busy Receiving
  // Action: Set pending state and let EndRx() take care of the state change.
  if( m_trxState == IEEE_802_15_4_PHY_BUSY_RX ){
  	NS_LOG_DEBUG (" Phy busy receiving; setting state pending to " << state);
  	m_trxStatePending = state;
  	return;
  }


  NS_FATAL_ERROR (" Invalid Zigbee PHY state transition.");
}


bool
ZigbeePhy::ChannelSupported (uint8_t channel)
{
  NS_LOG_FUNCTION (this << channel);
  bool retValue = false;

  for (uint32_t i = 0; i < 32; i++)
    {
      if ((m_phyPIBAttributes.phyChannelsSupported[i] & (1 << channel)) != 0)
        {
          retValue = true;
          break;
        }
    }

  return retValue;
}

void
ZigbeePhy::PlmeSetAttributeRequest (ZigbeePibAttributeIdentifier id,
                                    ZigbeePhyPIBAttributes* attribute)
{
  NS_ASSERT (attribute);
  NS_LOG_FUNCTION (this << id << attribute);
  ZigbeePhyEnumeration status = IEEE_802_15_4_PHY_SUCCESS;

  // Allow attributes to still be set, but indicate that the phy is sleeping
  if (m_trxState == PHY_SLEEP){
    status = PHY_SLEEP;
  }

  switch (id)
    {
    case phyCurrentChannel:
      {
        if (!ChannelSupported (attribute->phyCurrentChannel))
          {
            status = IEEE_802_15_4_PHY_INVALID_PARAMETER;

            NS_LOG_LOGIC(" phyCurrentChannel: Channel not supported.");
          }
        if (m_phyPIBAttributes.phyCurrentChannel != attribute->phyCurrentChannel)
          {    //any packet in transmission or reception will be corrupted
            if (m_currentRxPacket.m_packet)
              {
                m_currentRxPacket.m_isCorrupt = true;
              }
            if (PhyIsBusy ())
              {
                m_currentTxPacket.m_isCorrupt = true;
                m_pdDataRequest.Cancel ();
                m_currentTxPacket.m_packet = 0;
                if (!m_pdDataConfirmCallback.IsNull ())
                  {
                    m_pdDataConfirmCallback (IEEE_802_15_4_PHY_TRX_OFF);
                  }

                if (m_trxStatePending != IEEE_802_15_4_PHY_IDLE)
                  {
                    m_trxStatePending = IEEE_802_15_4_PHY_IDLE;
                  }
              }

            NS_LOG_LOGIC(" phyCurrentChannel: Changing channel from " << (uint16_t)m_phyPIBAttributes.phyCurrentChannel << " to " << (uint16_t)attribute->phyCurrentChannel);
            std::stringstream ss;
            ss << "Changing the channel from " << (uint16_t)m_phyPIBAttributes.phyCurrentChannel << " to " << (uint16_t)attribute->phyCurrentChannel;
            std::string msg = ss.str();
            m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), msg);

            m_phyPIBAttributes.phyCurrentChannel = attribute->phyCurrentChannel;
            FishWpanSpectrumValueHelper psdHelper;
            m_txPsd = psdHelper.CreateTxPowerSpectralDensity (m_phyPIBAttributes.phyTransmitPower, m_phyPIBAttributes.phyCurrentChannel);
          }
        else
        	NS_LOG_LOGIC(" phyCurrentChannel: Channel already set to " << (uint16_t)m_phyPIBAttributes.phyCurrentChannel);

        break;
      }
    case phyChannelsSupported:
      {   // only the first element is considered in the array
        if ((attribute->phyChannelsSupported[0] & 0xf8000000) != 0)
          {    //5 MSBs reserved
            status = IEEE_802_15_4_PHY_INVALID_PARAMETER;
          }
        else
          {
            m_phyPIBAttributes.phyChannelsSupported[0] = attribute->phyChannelsSupported[0];
          }
        break;
      }
    case phyTransmitPower:
      {
        if (attribute->phyTransmitPower > 0xbf)
          {
            status = IEEE_802_15_4_PHY_INVALID_PARAMETER;
          }
        else
          {
            m_phyPIBAttributes.phyTransmitPower = attribute->phyTransmitPower;
            FishWpanSpectrumValueHelper psdHelper;
            int8_t txPower = ((int8_t)m_phyPIBAttributes.phyTransmitPower);
            txPower <<= 2;
            txPower >>= 2;
            m_txPsd = psdHelper.CreateTxPowerSpectralDensity (txPower, m_phyPIBAttributes.phyCurrentChannel);

            std::stringstream ss;
            ss << "Setting the transmit power to " << (int16_t)txPower << " dBm";
            std::string msg = ss.str();

            NS_LOG_DEBUG(msg);

            m_phyTaskTrace(Mac16Address::ConvertFrom(m_device->GetAddress()), msg);

            m_currentDraws->UpdateTxCurrent(txPower);
            UpdateBattery();

          }
        break;
      }
    case phyCCAMode:
      {
        if ((attribute->phyCCAMode < 1) || (attribute->phyCCAMode > 3))
          {
            status = IEEE_802_15_4_PHY_INVALID_PARAMETER;
          }
        else
          {
            m_phyPIBAttributes.phyCCAMode = attribute->phyCCAMode;
          }
        break;
      }
    default:
      {
        status = IEEE_802_15_4_PHY_UNSUPPORTED_ATTRIBUTE;
        break;
      }
    }

  if (!m_plmeSetAttributeConfirmCallback.IsNull ())
    {
      m_plmeSetAttributeConfirmCallback (status, id);
    }
}

void
ZigbeePhy::SetBatteryCallback(BatteryDecrementCallback c)
{
	NS_LOG_FUNCTION(this);
	m_batteryDecrementCallback = c;
}

void
ZigbeePhy::SetPdDataIndicationCallback (PdDataIndicationCallback c)
{
  NS_LOG_FUNCTION (this);
  m_pdDataIndicationCallback = c;
}

void
ZigbeePhy::SetPdDataConfirmCallback (PdDataConfirmCallback c)
{
  NS_LOG_FUNCTION (this);
  m_pdDataConfirmCallback = c;
}

void
ZigbeePhy::SetPlmeCcaConfirmCallback (PlmeCcaConfirmCallback c)
{
  NS_LOG_FUNCTION (this);
  m_plmeCcaConfirmCallback = c;
}

void
ZigbeePhy::SetPlmeEdConfirmCallback (PlmeEdConfirmCallback c)
{
  NS_LOG_FUNCTION (this);
  m_plmeEdConfirmCallback = c;
}

void
ZigbeePhy::SetPlmeGetAttributeConfirmCallback (PlmeGetAttributeConfirmCallback c)
{
  NS_LOG_FUNCTION (this);
  m_plmeGetAttributeConfirmCallback = c;
}

void
ZigbeePhy::SetPlmeSetTRXStateConfirmCallback (PlmeSetTRXStateConfirmCallback c)
{
  NS_LOG_FUNCTION (this);
  m_plmeSetTRXStateConfirmCallback = c;
}

void
ZigbeePhy::SetPlmeSetAttributeConfirmCallback (PlmeSetAttributeConfirmCallback c)
{
  NS_LOG_FUNCTION (this);
  m_plmeSetAttributeConfirmCallback = c;
}

void
ZigbeePhy::SetPhyDropCallback (PhyDropCallback c)
{
	 NS_LOG_FUNCTION (this);
	 m_phyDropCallback=c;
}


void
ZigbeePhy::ChangeTrxState (ZigbeePhyEnumeration newState)
{
  NS_LOG_LOGIC (this << " state: " << m_trxState << " -> " << newState);

  // Change state
  if(m_device != 0){
    Mac16Address addr = Mac16Address::ConvertFrom(m_device->GetAddress());
    m_trxStateLogger (addr,ZigbeePhyEnumNames[m_trxState], ZigbeePhyEnumNames[newState]);
  }
  m_trxState = newState;

  // Battery update must be called after the state change, not before.
  UpdateBattery();

}



bool
ZigbeePhy::PhyIsBusy (void) const
{
  return ((m_trxState == IEEE_802_15_4_PHY_BUSY_TX)
          || (m_trxState == IEEE_802_15_4_PHY_BUSY_RX)
          || (m_trxState == IEEE_802_15_4_PHY_BUSY));
}



void
ZigbeePhy::SetTxPowerSpectralDensity (Ptr<SpectrumValue> txPsd)
{
  NS_LOG_FUNCTION (this << txPsd);
  NS_ASSERT (txPsd);
  m_txPsd = txPsd;
  NS_LOG_INFO ("\t computed tx_psd: " << *txPsd << "\t stored tx_psd: " << *m_txPsd);
}

void
ZigbeePhy::SetNoisePowerSpectralDensity (Ptr<const SpectrumValue> noisePsd)
{
  NS_LOG_FUNCTION (this << noisePsd);
  NS_LOG_INFO ("\t computed noise_psd: " << *noisePsd );
  NS_ASSERT (noisePsd);
  m_noise = noisePsd;
}

Ptr<const SpectrumValue>
ZigbeePhy::GetNoisePowerSpectralDensity (void)
{
  NS_LOG_FUNCTION (this);
  return m_noise;
}

void
ZigbeePhy::SetErrorModel (Ptr<Isa100ErrorModel> e)
{
  NS_LOG_FUNCTION (this << e);
  NS_ASSERT (e);
  m_errorModel = e;
}

Ptr<Isa100ErrorModel>
ZigbeePhy::GetErrorModel (void) const
{
  NS_LOG_FUNCTION (this);
  return m_errorModel;
}

void
ZigbeePhy::SetSupplyVoltage (double voltage)
{
  NS_LOG_FUNCTION (this);
  m_supplyVoltage = voltage;
}

double
ZigbeePhy::GetSupplyVoltage (void) const
{
  NS_LOG_FUNCTION (this);
  return m_supplyVoltage;
}

Ptr<ZigbeeTrxCurrentModel>
ZigbeePhy::GetTrxCurrents (void) const
{
  NS_LOG_FUNCTION (this);
  return m_currentDraws;
}

void
ZigbeePhy::SetTrxCurrentAttributes (std::map <std::string, Ptr<AttributeValue> > attributes)
{
  NS_LOG_FUNCTION (this);

  if(!attributes.size())
    NS_LOG_WARN("Invoked optimizer using default attributes.");

  std::map<std::string,Ptr<AttributeValue> >::iterator it;
  for (it=attributes.begin(); it!=attributes.end(); ++it)
  {
    std::string name = it->first;
    if(!name.empty())
    {
      m_currentDraws->SetAttribute(it->first , *it->second);
    }
  }
}


void
ZigbeePhy::UpdateBattery (void)
{
  NS_LOG_FUNCTION (this);

  // Calculate the energy consumed since the last update.
  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.GetNanoSeconds () >= 0);

  // Energy is in uJ.
  double energyConsumed = m_current * duration.GetSeconds() * m_supplyVoltage * 1e6;
  if(!m_batteryDecrementCallback.IsNull())
  	m_batteryDecrementCallback(m_energyCategory,energyConsumed);

  NS_LOG_LOGIC("Consumed: " << energyConsumed << " uJ over " << duration.GetSeconds() << " s (" << m_current << " A, " << m_supplyVoltage << " V) (NxtState: " << m_trxState << ")");

  // Update the current power consumption values.
  m_lastUpdateTime = Simulator::Now();

  switch (m_trxState)
  {
    case IEEE_802_15_4_PHY_BUSY_RX:
    {
      m_current = m_currentDraws->GetBusyRxCurrentA();
      m_energyCategory = "BusyRx";
      break;
    }

    case IEEE_802_15_4_PHY_IDLE: /* fall through -- treat idle as rx on */
    case IEEE_802_15_4_PHY_RX_ON:
    {
      m_current = m_currentDraws->GetRxOnCurrentA();
      m_energyCategory = "RxOn";
      break;
    }

    case IEEE_802_15_4_PHY_BUSY_TX:
    {
      m_current = m_currentDraws->GetBusyTxCurrentA();
      m_energyCategory = "BusyTx";
      break;
    }

    case IEEE_802_15_4_PHY_TX_ON:
    {
      m_current = m_currentDraws->GetTxOnCurrentA();
      m_energyCategory = "TxOn";
      break;
    }

    case IEEE_802_15_4_PHY_TRX_OFF:
    {
      m_current = m_currentDraws->GetTrxOffCurrentA();
      m_energyCategory = "TrxOff";
      break;
    }

    case PHY_SLEEP:
    {
      m_current = m_currentDraws->GetSleepCurrentA();
      m_energyCategory = "PhySleep";
      break;
    }
    default:
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModel:Invalid radio state: " << m_trxState);
  }

}


