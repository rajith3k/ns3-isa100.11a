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


#ifndef FISH_802_15_4_PHY_H
#define FISH_802_15_4_PHY_H

#include "ns3/packet.h"
#include "ns3/mac16-address.h"
#include "ns3/isa100-error-model.h"
#include "ns3/event-id.h"
#include "ns3/spectrum-phy.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/zigbee-trx-current-model.h"
#include "ns3/isa100-battery.h"

#include <map>
#include <vector>
#include <iomanip>

using namespace ns3;
using namespace std;




/**
 * \defgroup lr-wpan-phy LR-WPAN PHY Code
 *
 * This documents the LR-WPAN code that implements the 802.15.4 PHY layer.
 *  @{
 */





/** Number of symbols contained in the PHY protocol data unit (PPDU) header.
 * - IEEE802.15.4-2006 Figure 16, Table 19 and 20 in section 6.3.
 * - Contains both the SHR and PHR.
 *  + SHR = synchronization header
 *  + PHR = PHY header.  Always 1 octet but num symbols is a function of modulation.
 */
typedef  struct
{
  double shrPreamble; ///< Num sync preamble symbols (training sequence).
  double shrSfd;      ///< Num start of frame symbols.
  double phr;         ///< Num PHY header symbols.
} ZigbeePhyPpduHeaderSymbolNumber;

/** Structure for holding a packet and state indicating packet integrity.
 */
typedef struct{
	Ptr<Packet> m_packet;  ///< Pointer to packet.
	bool m_isCorrupt;  ///< Indicates whether the packet has been corrupted by interference.
} PacketAndStatus;



/** Enum defining operational state of PHY transceiver.
 * - IEEE802.15.4-2006 PHY Emumerations Table 18 in section 6.2.3.
 *  + BUSY = clear channel assessment (CCA) detected a busy channel
 *  + BUSY_RX = transceiver asked to change state while receiving
 *  + BUSY_TX = transceiver asked to change state while transmitting
 *  + FORCE_TRX_OFF = turn transceiver off immediately
 *  + IDLE = CCA detects an idle channel
 *  + INVALID_PARAMETER = SET/GET request issued with out of range parameter
 *  + RX_ON = receiver enabled state
 *  + SUCCESS = SET/GET, energy detection (ED) or state change successful
 *  + TRX_OFF = transceiver off
 *  + TX_ON = transmitter on
 *  + UNSUPPORTED_ATTRIBUTE = SET/GET with unsupported attribute
 *  + READ_ONLY = SET/GET with read only attribute
 */
typedef enum
{
  IEEE_802_15_4_PHY_BUSY  = 0x00,
  IEEE_802_15_4_PHY_BUSY_RX = 0x01,
  IEEE_802_15_4_PHY_BUSY_TX = 0x02,
  IEEE_802_15_4_PHY_FORCE_TRX_OFF = 0x03,
  IEEE_802_15_4_PHY_IDLE = 0x04,
  IEEE_802_15_4_PHY_INVALID_PARAMETER = 0x05,
  IEEE_802_15_4_PHY_RX_ON = 0x06,
  IEEE_802_15_4_PHY_SUCCESS = 0x07,
  IEEE_802_15_4_PHY_TRX_OFF = 0x08,
  IEEE_802_15_4_PHY_TX_ON = 0x09,
  IEEE_802_15_4_PHY_UNSUPPORTED_ATTRIBUTE = 0xa,
  IEEE_802_15_4_PHY_READ_ONLY = 0xb,
  IEEE_802_15_4_PHY_UNSPECIFIED = 0xc, // all cases not covered by ieee802.15.4
  PHY_SLEEP = 0xd // Not in standard
} ZigbeePhyEnumeration;

// Matching names of ZigbeePhyEnumeration used for readable printing (make sure cpp definition matches above enum)
extern const char * ZigbeePhyEnumNames[];

/** Enum for indexing different types of PHY operational information.
 * - IEEE802.15.4-2006 PHY PIB Attributes Table 23 in section 6.4.2.
 */
typedef enum
{
  phyCurrentChannel = 0x00,
  phyChannelsSupported = 0x01,
  phyTransmitPower = 0x02,
  phyCCAMode = 0x03,
  phyCurrentPage = 0x04,
  phyMaxFrameDuration = 0x05,
  phySHRDuration = 0x06,
  phySymbolsPerOctet = 0x07
} ZigbeePibAttributeIdentifier;

/** Structure for PHY operating parameters.
 */
typedef struct
{
  uint8_t phyCurrentChannel;  ///< 27 LSB in 32 bit word indicating which of 27 channels are active.
  uint32_t phyChannelsSupported[32]; ///< Array of supported channels (5 MSB for channel page, 27 LSB for channels within page).
  uint8_t phyTransmitPower;   ///< Tolerance of tx power accuracy.
  uint8_t phyCCAMode;         ///< Energy above threshold, carrier sense only or both.
  uint32_t phyCurrentPage;    ///< Current channel page.
  uint32_t phyMaxFrameDuration; ///< phySHRDuration + ceil(maxPHYPacketSize+1)*phySymbolsPerOctet
  uint32_t phySHRDuration;      ///< Num symbols in sync frame.
  double phySymbolsPerOctet;    ///< Symbols per octet 0.4 for 20 bit spread spectrum/ASK, 1.6 for 5 bit spread spectrum/ASK,
                                ///< 2 for 16-ary orthog mod with O-QPSK or 8 for BPSK.
} ZigbeePhyPIBAttributes;

/** Enum to trace packet drops (FISH).
 */
typedef enum
{
	phyDropRxHiddenNode = 0x00,
	phyDropRxLowSNR = 0x01,
	phyDropTx=0x02
}ZigbeePhyDropSource;

/** Callback to transfer PHY service data unit (PSDU) to MAC layer.
 * - Denoted PD-DATA.indication in the standard.
 * - Transfers phy service data unit (PSDU) to MAC layer where it becomes
 *   a MAC protocol data unit (MPDU).
 *
 *  @param psduLength Number of bytes in the PSDU.
 *  @param p The packet to be transmitted.
 *  @param lqi Link quality (LQI) value measured during reception of the PPDU.
 *  @param rxPowerDbm Received power in dBm.
 */
typedef Callback< void, uint32_t, Ptr<Packet>, uint32_t, double > PdDataIndicationCallback;

/** Callback to confirm the end of transmission of an MPDU (ie PSDU) from one PHY entity to another.
 * - Denoted PD-DATA.confirm in standard.
 * - Issued in response to a PD-DATA.request primitive.
 * - Error codes:
 *   + RX_ON: PSDU discarded since PHY was in RX_ON
 *   + TRX_OFF: PSDU discarded since PHY was TRX_OFF
 *   + BUSY_TX: PSDU discarded since PHY busy transmitting another PSDU
 *
 * @param status: SUCCESS or error code
 */
typedef Callback< void, ZigbeePhyEnumeration > PdDataConfirmCallback;

/** Callback to confirm result of clear channel assessment (CCA).
 * - PLME-CCA.confirm in standard
 * - Reports the results of the CCA: TRX_OFF, BUSY, IDLE
 *
 * @param status Values: TRX_OFF, BUSY, IDLE
 */
typedef Callback< void, ZigbeePhyEnumeration > PlmeCcaConfirmCallback;

/** Callback to report result of energy detection (ED).
 * - PLME-ED.confirm in standard.
 * - Reports result of energy detection: SUCCESS, TRX_OFF or TX_ON
 * - Also returns energy level if SUCCESS.
 * - Energy level 0x00 = < 10dB above rx sensitivity
 * - Range of ED values at least 40 dB.
 * - ED mapping from linear to dB has accuracy of +/- 6dB.
 *
 * @param status Values: SUCCESS, TRX_OFF or TX_ON
 * @param energyLevel The energy level of ED (0x00 - 0xff).
 */
typedef Callback< void, ZigbeePhyEnumeration,uint8_t > PlmeEdConfirmCallback;

/** Callback to return a PHY operational parameter.
 * - PLME-GET.confirm in standard.
 * - Returns a field from the ZigbeePhyPIBAttributes structure.
 *
 * @param status Values: SUCCESS or UNSUPPORTED_ATTRIBUTE
 * @param id The identifier of attribute (ZigbeePibAttributeIdentifier).
 * @param attribute The pointer to attribute struct.
 */
typedef Callback< void, ZigbeePhyEnumeration,
                  ZigbeePibAttributeIdentifier,
                  ZigbeePhyPIBAttributes* > PlmeGetAttributeConfirmCallback;

/** Callback returns status of request to change transceiver state.
 * - PLME-SET-TRX-STATE.confirm in standard.
 * - Returns SUCCESS if change successful.
 * - TX_ON, RX_ON or TRX_OFF indicates already in desired state.
 *
 * @param status Values: SUCCESS, TX_ON, RX_ON or TRX_OFF
 */
typedef Callback< void, ZigbeePhyEnumeration > PlmeSetTRXStateConfirmCallback;

/** Callback returns status of request to set an attribute.
 * - PLME-SET.confirm in standard.
 *
 * @param status Values: SUCCESS, UNSUPORTED_PARAMETER, INVALID_PARAMETER, READ_ONLY.
 * @param id The attribute identifier from the ZigbeePibAttributeIdentifier enum.
 */
typedef Callback< void, ZigbeePhyEnumeration,
                  ZigbeePibAttributeIdentifier > PlmeSetAttributeConfirmCallback;


/** Callback to identify source of packet drop (FISH).
 *
 * @param source Source of the packet drop from ZigbeePhyDropSource enum.
 */
typedef Callback<void, ZigbeePhyDropSource> PhyDropCallback;



/** Lr-Wpan PHY class.
 *
 */

class ZigbeePhy : public SpectrumPhy
{

public:
  // The second is true if the first is flagged as error
 // typedef std::pair<Ptr<Packet>, bool>  PacketAndStatus;

  static TypeId GetTypeId (void);

  static const uint32_t aMaxPhyPacketSize; ///< Table 22 in section 6.4.1 of ieee802.15.4
  static const uint32_t aTurnaroundTime;   ///< Table 22 in section 6.4.1 of ieee802.15.4

  ZigbeePhy ();
  virtual ~ZigbeePhy ();

  /** Set the NetDevice instance associated with this PHY.
   *
   * @param d the NetDevice instance
   */
   void SetDevice (Ptr<NetDevice> d);

  /** Get the NetDevice instance associated with this PHY.
   *
   * @return a Ptr to the associated NetDevice instance
   */
  Ptr<NetDevice> GetDevice () const;

  /** Set the mobility model associated with this device.
   *
   * @param m the mobility model
   */
  void SetMobility (Ptr<MobilityModel> m);

  /** Get the associated MobilityModel instance
   *
   * @return a Ptr to the associated MobilityModel instance
   */
  Ptr<MobilityModel> GetMobility ();

  /** Set the channel attached to this device.
   *
   * @param c the channel
   */
  void SetChannel (Ptr<SpectrumChannel> c);

  /** Get the channel attached to this device.
   *
   * @return the channel
   */
  Ptr<SpectrumChannel> GetChannel (void);

  /** Get the spectrum model associated with this device.
   *
   * @return returns the SpectrumModel that this SpectrumPhy expects to be used
   * for all SpectrumValues that are passed to StartRx. If 0 is
   * returned, it means that any model will be accepted.
   */
  Ptr<const SpectrumModel> GetRxSpectrumModel () const;

  /** Get the AntennaModel used by the NetDevice for reception
   *
   * @return a Ptr to the AntennaModel used by the NetDevice for reception
   */
  Ptr<AntennaModel> GetRxAntenna ();

  /** Set the AntennaModel used by the NetDevice for reception
    *
    * @param a a Ptr to the AntennaModel used by the NetDevice for reception
    */
  void SetAntenna (Ptr<AntennaModel> a);

  /** Set the Power Spectral Density of outgoing signals in W/Hz.
   *
   * @param txPsd A SpectrumValue object specifying the spectral mask of the tx signal.
   */
  void SetTxPowerSpectralDensity (Ptr<SpectrumValue> txPsd);

  /** Set the noise power spectral density.
   *
   * @param noisePsd A SpectrumValue object specifying the Noise Power Spectral Density in
   * power units (Watt, Pascal...) per Hz.
   */
  void SetNoisePowerSpectralDensity (Ptr<const SpectrumValue> noisePsd);

  /** Get the noise power spectral density
   *
   * @return The Noise Power Spectral Density
   */
  Ptr<const SpectrumValue> GetNoisePowerSpectralDensity (void);

  /** Notify the SpectrumPhy instance of an incoming waveform.
   * - In this case, we notify the Zigbee PHY derived class since this is a virtual function.
   *
   * @param params the SpectrumSignalParameters associated with the incoming waveform
   */
  virtual void StartRx (Ptr<SpectrumSignalParameters> params);

  /** Used to submit a MPDU for transmission to the PHY.
   *  - IEEE 802.15.4-2006 section 6.2.1.1
   *  - PD-DATA.request
   *
   *  @param psduLength Number of payload bytes in the PSDU.
   *  @param p The packet to be transmitted.
   */
  void PdDataRequest (const uint32_t psduLength, Ptr<Packet> p);

  /** Used to request a clear channel assessment (CCA).
   *  - IEEE 802.15.4-2006 section 6.2.2.1
   *  - PLME-CCA.request
   *  - Perform a CCA per section 6.9.9
   */
  void PlmeCcaRequest (void);

  /** Used to request an energy detection (ED).
   *  - IEEE 802.15.4-2006 section 6.2.2.3
   *  - PLME-ED.request
   *  - Perform an ED per section 6.9.7
   */
  void PlmeEdRequest (void);

  /** Used to request an operating parameter from the PHY.
   *  - IEEE 802.15.4-2006 section 6.2.2.5
   *  - PLME-GET.request
   *  - Get attributes per definition from Table 23 in section 6.4.2
   *
   *  @param id the attributed identifier
   */
  void PlmeGetAttributeRequest (ZigbeePibAttributeIdentifier id);

  /** Used to change the state of the PHY object.
   *  - IEEE 802.15.4-2006 section 6.2.2.7
   *  - PLME-SET-TRX-STATE.request
   *
   *  @param state Values: RX_ON,TRX_OFF,FORCE_TRX_OFF,TX_ON
   */
  void PlmeSetTRXStateRequest (ZigbeePhyEnumeration state);

  /** Set the PHY operating parameters.
   *  - IEEE 802.15.4-2006 section 6.2.2.9
   *  - PLME-SET.request
   *  - Set attributes per definition from Table 23 in section 6.4.2
   *
   *  @param id Identifies the attribute.
   *  @param * attribute The attribute value.
   */
  void PlmeSetAttributeRequest (ZigbeePibAttributeIdentifier id, ZigbeePhyPIBAttributes* attribute);

  /** Set callback to the function that decrements battery energy. (GGM)
   *
   * @param c Callback function.
   */
  void SetBatteryCallback(BatteryDecrementCallback c);

  /** Set the received data callback.
   * Callback occurs at the end of an RX as part of the
   * interconnections betweenthe PHY and the MAC.  The callback
   * function passes the received packet to the MAC.
   *
   * @param c Callback function.
   */
  void SetPdDataIndicationCallback (PdDataIndicationCallback c);

  /** Set the callback that confirms a PSDU transmission.
   * Callback indicates whether the PSDU was successfully transmitted
   * or dropped due to a busy or switched off PHY.
   *
   * @param c Callback function.
   */
  void SetPdDataConfirmCallback (PdDataConfirmCallback c);

  /** Set the callback that indicates the result of a CCA request.
   * Callback indicates whether the channel is busy/idle or the
   * transceiver is off.
   *
   * @param c Callback function.
   */
  void SetPlmeCcaConfirmCallback (PlmeCcaConfirmCallback c);


  /** Set the callback that indicates the result of an ED request.
   * Callback indicates whether ED was successful and also returns
   * ED value.
   *
   * @param c Callback function.
   */
  void SetPlmeEdConfirmCallback (PlmeEdConfirmCallback c);

  /** Set the callback that returns a PHY attribute.
   * Callback returns a reference to the desired parameter.
   *
   * @param c Callback function.
   */
  void SetPlmeGetAttributeConfirmCallback (PlmeGetAttributeConfirmCallback c);

  /** Set the callback that indicates the result of setting the transceiver state.
   * Indicates if the state change was successful or if it was already in the
   * desired state.
   *
   * @param c Callback function.
   */
  void SetPlmeSetTRXStateConfirmCallback (PlmeSetTRXStateConfirmCallback c);

  /** Set the callback that indicates the result of setting a PHY attribute.
   * Callback indicates whether set was successful and which attribute was set.
   *
   * @param c Callback function.
   */
  void SetPlmeSetAttributeConfirmCallback (PlmeSetAttributeConfirmCallback c);

  /** Sets callback function for tracing the packet drop.
   *
   * @param c Callback function.
   */
  void SetPhyDropCallback(PhyDropCallback c);


  /** Set the error model to use.
   *
   * @param e pointer to ZigbeeErrorModel to use
   */
  void SetErrorModel (Ptr<Isa100ErrorModel> e);

  /** Get the error model being used.
   *
   * @return pointer to ZigbeeErrorModel in use
   */
  Ptr<Isa100ErrorModel> GetErrorModel (void) const;

  /** Get the energy consumption categories for the PHY layer (GGM).
   *
   * @return Vector of string names of the categories.
   */
  vector<string>& GetEnergyCategories();


  /** Set the supply voltage in Volts
   *
   * @param voltage The voltage supply (V)
   */
  void SetSupplyVoltage (double voltage);

  /** Get the voltage supply in Volts
   *
   * @return The voltage supply (V)
   */
  double GetSupplyVoltage (void) const;

  /** Get the transceiver currents
   *
   * @return The object containing the different trx currents
   */
  Ptr<ZigbeeTrxCurrentModel> GetTrxCurrents (void) const;

  /** Set the trx current model attributes
   *
   * @param attributes the attributes to set
   */
  void SetTrxCurrentAttributes (std::map <std::string, Ptr<AttributeValue> > attributes);


  /**}@*/

protected:
  static const ZigbeePhyPpduHeaderSymbolNumber ppduHeaderSymbolNumbers[7];

private:


  /**
   * Update the how much energy the PHY transceiver has consumed from the battery. (GGM)
   */
  void UpdateBattery();




  virtual void DoDispose (void);
  void ChangeState (ZigbeePhyEnumeration newState);
  void ChangeTrxState (ZigbeePhyEnumeration newState);
  void EndTx ();
  void EndRx ();
  void EndEd ();
  void EndCca ();

  double GetPpduHeaderTxTime (void);
  bool ChannelSupported (uint8_t);
  Ptr<MobilityModel> m_mobility;
  Ptr<NetDevice> m_device;
  Ptr<SpectrumChannel> m_channel;
  Ptr<AntennaModel> m_antenna;
  Ptr<SpectrumValue> m_txPsd;
  Ptr<const SpectrumValue> m_rxPsd;
  Ptr<const SpectrumValue> m_noise;
  Ptr<Isa100ErrorModel> m_errorModel;
  ZigbeePhyPIBAttributes m_phyPIBAttributes;

  vector<string> m_energyCategories;  /// Energy consumption categories.

  Ptr<Packet> m_lastTxPacket;
  double m_bitRate;     ///< phy bit rate in bits/sec
  double m_symbolRate;  ///< phy symbol rate in symbols/sec



  Time m_lastUpdateTime;
  double m_current;
  double m_supplyVoltage;
  string m_energyCategory;
  Ptr<ZigbeeTrxCurrentModel> m_currentDraws;
  Time m_wakeUpDuration;

  /** Decrement the number of active signals in the channel.
   *
   */
  void DecrementChannelRxSignals();

  // State variables
  ZigbeePhyEnumeration m_trxState;  /// transceiver state
  TracedCallback<Mac16Address, std::string, std::string> m_trxStateLogger;
  ZigbeePhyEnumeration m_trxStatePending;  /// pending state change
  bool PhyIsBusy (void) const;  /// helper function


  // Trace sources
  /**
   * The trace source fired when a packet begins the transmission process on
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyTxBeginTrace;

  /**
   * The trace source fired when a packet ends the transmission process on
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyTxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet as it tries
   * to transmit it.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyTxDropTrace;

  /**
   * The trace source fired when a packet begins the reception process from
   * the medium.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxBeginTrace;

  /**
   * The trace source fired when a packet ends the reception process from
   * the medium.  Second quantity is received SINR.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Mac16Address, Ptr<const Packet>, double > m_phyRxEndTrace;

  /**
   * The trace source fired when the phy layer drops a packet it has received.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_phyRxDropTrace;

  TracedCallback<Mac16Address, Ptr<const Packet>, std::string> m_infoDropTrace;
  TracedCallback<Mac16Address, std::string> m_phyTaskTrace;

  PdDataIndicationCallback m_pdDataIndicationCallback;
  PdDataConfirmCallback m_pdDataConfirmCallback;
  PlmeCcaConfirmCallback m_plmeCcaConfirmCallback;
  PlmeEdConfirmCallback m_plmeEdConfirmCallback;
  PlmeGetAttributeConfirmCallback m_plmeGetAttributeConfirmCallback;
  PlmeSetTRXStateConfirmCallback m_plmeSetTRXStateConfirmCallback;
  PlmeSetAttributeConfirmCallback m_plmeSetAttributeConfirmCallback;
  PhyDropCallback m_phyDropCallback;

  BatteryDecrementCallback m_batteryDecrementCallback;




  double m_rxEdPeakPower;
  double m_rxTotalPower;
  uint32_t m_rxTotalNum;
  double m_rxSensitivityDbm;  ///< The receiver sensitivity in dBm
  double m_noiseFloorDbm;     ///< The receiver noise floor in dBm
  double m_noiseFigureDbm;    ///< The receiver noise figure (dBm)
  PacketAndStatus m_currentRxPacket;
  PacketAndStatus m_currentTxPacket;

  EventId m_edRequest;
  EventId m_setTRXState;
  EventId m_pdDataRequest;
  Ptr<UniformRandomVariable> m_random;
};



#endif /* LR_WPAN_PHY_H */
