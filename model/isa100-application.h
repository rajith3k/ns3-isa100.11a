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
 * Author: Geoffrey Messier <gmessier@ucalgary.ca>
 *         Hazem Gomaa <gomaa.hazem@gmail.com>
 *         Michael Herrmann <mjherrma@gmail.com>
 */

#ifndef ISA100_APPLICATION_H
#define ISA100_APPLICATION_H

#include "ns3/application.h"
#include "isa100-dl.h"
#include "ns3/isa100-processor.h"
#include "ns3/isa100-sensor.h"

namespace ns3 {

/** Enumeration for identifying sensor node states for the CSMA Application
 */
typedef enum
{
  SENSOR_IDLE_STATE         = 0x00,
  SAMPLING_STATE            = 0x01,
  SENDING_DATA_STATE        = 0x02,
  WAIT_FOR_SLEEP_CMD_STATE  = 0x03,
  SLEEP_PREP_STATE          = 0x04,
  REBROADCASTING_STATE      = 0x05,
  SLEEPING_STATE            = 0x06
} SensorNodeStatesEnum;

/** Enumeration for identifying sink node states for the CSMA Application
 */
typedef enum
{
  SINK_IDLE_STATE           = 0x00,
  WAIT_FOR_SAMPLING_STATE   = 0x01,
  RECEIVE_STATE             = 0x02,
  BROADCASTED_SLEEP_STATE   = 0x03
} SinkNodeStatesEnum;

/** Enumeration for identifying sensor node states for the TDMA application
 */
typedef enum
{
  SENSOR_TDMA_IDLE_STATE     = 0x07,
  SENSOR_TDMA_START_STATE    = 0x08,
  TX_REQ_STATE               = 0x09,
  TX_CONFIRM_STATE           = 0x0a,
  ACTIVE_SLOT_STATE          = 0x0b,
  SLEEP_STATE                = 0x0c
} SensorNodeTdmaStatesEnum;

/** Enumeration for identifying sink node states for the TDMA application
 */
typedef enum
{
  SINK_TDMA_IDLE_STATE   = 0x04,
  WAIT_FIRST_FRAME_STATE = 0x05,
  RX_DATA_STATE          = 0x06,
  END_OF_FRAME_STATE     = 0x07
} SinkNodeTdmaStatesEnum;



/**
 * \defgroup isa100-11a-application ISA 100.11a Application Implementation
 *
 * This documents the code that implements the 100.11a application classes.
 *  @{
 */

/** Callback to request a data transmission from the DL.
 * - Takes a DlDataRequestParams argument and a packet pointer.
 */
typedef Callback< void, DlDataRequestParams, Ptr<Packet> > DlDataRequestCallback;


/** Callback to put the lower layers (Dl and Phy) to sleep for a given amount of time.
 * - Time argument to indicate when to wake up
 */
typedef Callback< void, Time> DlSleepCallback;


/** Callback for measurement samples to use energy from the phy
 * - double argument to indicate how much energy (J) to deplete
 */
typedef Callback< void, Time, double> PhyConsumeEnergyCallback;


// ----------------------------- ISA 100 General Application ----------------------------

class Isa100Application : public Application
{
public:

  Isa100Application ();

  virtual ~Isa100Application ();

  static TypeId GetTypeId (void);

  // ------- Application Layer to DL Communication Interface ---------

  /** Set the data request callback.
   * - Callback used to pass data to DL for transmission.
   *
   * @param c Callback function.
   */
  void SetDlDataRequestCallback (DlDataRequestCallback c);

  // ------- DL to Application Layer Communication Interface ---------

  /** Called when a packet is received
   *
   * @param params Dl data indication parameters
   * @param p Pointer to packet containing only the data payload
   */
  virtual void DlDataIndication (DlDataIndicationParams params, Ptr<Packet> p);

  /** Called to return the outcome of a transmission request.
   * @param parmas Dl data indication parameters
   */
  virtual void DlDataConfirm (DlDataConfirmParams params);


  /**}@*/


protected:


  /** Called automatically at m_startTime.
   * - Inherited from Application.
   */
  virtual void StartApplication (void);

  /** Called automatically at m_stopTime.
   * - Inherited from Application.
   */
  virtual void StopApplication (void);


  // ------- Trace Functions --------
  uint32_t m_packetSize;  ///< Data payload of the packets generated (bytes).
  Mac16Address m_dstAddress;  ///< Address of the packet destination.
  Mac16Address m_srcAddress;  ///< Address of the node hosting the application object.
  Time m_startTime; ///< Start time for the application.

  DlDataRequestCallback m_dlDataRequest;  ///< Pointer to the ISA100 DL data request routine.
};

// ------------------ ISA 100 Generic Packet Generation Application ------------------------

/*
 * This class assumes the functionality of generating a specific number of packets that used to be
 * in the base application class.
 */
class Isa100PacketGeneratorApplication : public Isa100Application
{
public:
  Isa100PacketGeneratorApplication ();

  virtual ~Isa100PacketGeneratorApplication ();

  static TypeId GetTypeId (void);



private:


  /** Called automatically at m_startTime.
   * - Inherited from Application.
   */
  void StartApplication (void);

  /** Called automatically at m_stopTime.
   * - Inherited from Application.
   */
  virtual void StopApplication (void);


  /** Function scheduled to transmit a single packet.
   *
   */
  void SendPacket();

  uint64_t m_numberOfPackets;  ///< Total number of packets to generate.
	Time m_txInterval; ///< Time between packets.

  uint64_t m_numberPacketsSent;  ///< Current number of packets generated.
  EventId m_sendPacketEvent;  ///< Linked to the next scheduled SendPacket() call.
	;
};


// ------------------ ISA 100 Field Node Network Application ------------------------

class Isa100FieldNodeApplication : public Isa100Application
{
public:

  Isa100FieldNodeApplication ();

  virtual ~Isa100FieldNodeApplication ();

  static TypeId GetTypeId (void);

  /** Called when sensing operation is complete
   *
   * @param data The sensor data sample.
   */
  void SensorSampleCallback(double data);

  /** Sets the pointer to the sensor being used by the application
   *
   * @param sensor Pointer to the sensor
   */
  void SetSensor(Ptr<Isa100Sensor> sensor);

  /** Sets the pointer to the processor being used by the application
   *
   * @param processor Pointer to the processor
   */
  void SetProcessor(Ptr<Isa100Processor> processor);


private:
  /** Called automatically at m_startTime.
   * - Override the Application class function
   *
   * - IF USING MULTIFRAME SUPERFRAME THIS ASSUMES ALL FRAMES WITHIN A SF ARE EQUAL LENGTH
   */
  virtual void StartApplication (void);

  /** Called automatically at m_stopTime.
   * - Override the Application class function
   */
  virtual void StopApplication (void);

  /** Start the sensing operation
   */
  void StartSensing();


  Time m_slotDuration;                   ///< Duration of a timeslot
  Time m_updatePeriod;                   ///< One update period: equal to both the sample time and the length of a superframe.
  Time m_sampleDuration;    			     ///< Time required to perform a sample.

  Ptr<Isa100Sensor> m_sensor;  ///<  Pointer to the sensor being used by the application.
  Ptr<Isa100Processor> m_processor;  ///<  Pointer to the processor being used by the application.

  EventId m_sampleTaskId;                ///< event Id of the sample task

  // ------- Trace Functions --------
  TracedCallback< Mac16Address > m_reportTxTrace;


};


// ----------------------------- ISA 100 Backbone Node Application ----------------------------

class Isa100BackboneNodeApplication : public Isa100Application
{
public:

  Isa100BackboneNodeApplication ();

  virtual ~Isa100BackboneNodeApplication ();

  static TypeId GetTypeId (void);

  /** Called when a packet is received
   *
   * @param params Dl data indication parameters
   * @param p Pointer to packet containing only the data payload
   */
  virtual void DlDataIndication (DlDataIndicationParams params, Ptr<Packet> p);

  /** Called to return the outcome of a transmission request.
   * - Override the Isa100 Base Application function
   */
  virtual void DlDataConfirm (DlDataConfirmParams params);


  /**}@*/


private:
  /** Called automatically at m_startTime.
   * - Override the Application class function
   */
  virtual void StartApplication (void);

  /** Called automatically at m_stopTime.
   * - Override the Application class function
   */
  virtual void StopApplication (void);


  // ------- Trace Functions --------
  TracedCallback< Mac16Address > m_reportRxTrace;
};






} // namespace ns3

#endif /* ISA100_APPLICATION_H */
