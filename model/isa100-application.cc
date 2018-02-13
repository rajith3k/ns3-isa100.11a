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

#include <numeric>

#include "ns3/isa100-application.h"
#include "ns3/isa100-net-device.h"
#include "ns3/random-variable-stream.h"

#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/packet.h"

// Broadcast packet information
// Prevent the complier from adding padding to the unions larger than 1 byte
#pragma pack(push,1)

typedef union
{
  struct
  {
    uint32_t broadcastId;     ///< Unique ID of this broadcast
    uint32_t sleepDurationMs; ///< Sleep duration in milliseconds
  };
  uint8_t bytes[8];
}BroadcastPacketPayload;

// Allow padding again
#pragma pack(pop)

// Packet identifier
typedef enum
{
  DATA_PACKET = 1,
  BROADCAST_PACKET = 2
} PacketTypeIdentifier;

NS_LOG_COMPONENT_DEFINE ("Isa100Application");

namespace ns3 {

// ----------------------------- ISA 100 General Application -----------------------------

NS_OBJECT_ENSURE_REGISTERED (Isa100Application);

TypeId Isa100Application::GetTypeId (void)
{
     static TypeId tid = TypeId ("ns3::Isa100Application")
    .SetParent<Object> ()

    .AddConstructor<Isa100Application> ()

		.AddAttribute ("StartTime", "Application starting time",
					 TimeValue(Seconds(0.0)),
					 MakeTimeAccessor (&Isa100Application::m_startTime),
					 MakeTimeChecker())

		.AddAttribute ("PacketSize", "packet size",
					UintegerValue (50),
					MakeUintegerAccessor (&Isa100Application::m_packetSize),
					MakeUintegerChecker<uint32_t> ())

		.AddAttribute ("DestAddress", "The address of the destination",
	    		Mac16AddressValue(),
	    		MakeMac16AddressAccessor(&Isa100Application::m_dstAddress),
	    		MakeMac16AddressChecker())

		.AddAttribute ("SrcAddress", "The address of the source.",
				Mac16AddressValue (),
				MakeMac16AddressAccessor (&Isa100Application::m_srcAddress),
				MakeMac16AddressChecker ())

				;
  return tid;
}

Isa100Application::Isa100Application ()
{
	;
}

Isa100Application::~Isa100Application ()
{
}



void Isa100Application::StartApplication (void)
{
	;
}

void Isa100Application::StopApplication (void)
{
	;
}

void Isa100Application::SetDlDataRequestCallback (DlDataRequestCallback c)
{
	m_dlDataRequest = c;
}

void Isa100Application::DlDataIndication (DlDataIndicationParams params, Ptr<Packet> p)
{
  NS_LOG_INFO("Node " << m_srcAddress << " received packet: " << p);
}

void Isa100Application::DlDataConfirm (DlDataConfirmParams params)
{
  NS_LOG_INFO("Node " << m_srcAddress << " received confirmation of packet transmission.");
}


// ----------------------- Basic Packet Generator Application ------------------------------

NS_OBJECT_ENSURE_REGISTERED (Isa100PacketGeneratorApplication);

TypeId Isa100PacketGeneratorApplication::GetTypeId (void)
{

	static TypeId tid = TypeId ("ns3::Isa100PacketGeneratorApplication")

			.SetParent<Isa100Application> ()

			.AddConstructor<Isa100PacketGeneratorApplication> ()

			.AddAttribute ("NumberOfPackets", "Number of packets",
					UintegerValue (1),
					MakeUintegerAccessor (&Isa100PacketGeneratorApplication::m_numberOfPackets),
					MakeUintegerChecker<uint64_t> ())

		  .AddAttribute ("StopTime", "Application stopping time",
		  			TimeValue(Seconds(0.0)),
					MakeTimeAccessor (&Isa100PacketGeneratorApplication::m_stopTime),
					MakeTimeChecker())

		.AddAttribute ("TxInterval", "Time between two consecutive packets",
					TimeValue(Seconds(0.0)),
				 	MakeTimeAccessor(&Isa100PacketGeneratorApplication::m_txInterval),
					MakeTimeChecker())


				;
  return tid;
}

Isa100PacketGeneratorApplication::Isa100PacketGeneratorApplication ()
{
	m_numberPacketsSent = 0;
	m_numberOfPackets = 0;

}

Isa100PacketGeneratorApplication::~Isa100PacketGeneratorApplication ()
{
}



void Isa100PacketGeneratorApplication::StartApplication (void)
{

	m_sendPacketEvent = Simulator::ScheduleNow(&Isa100PacketGeneratorApplication::SendPacket,this);

}

void Isa100PacketGeneratorApplication::StopApplication (void)
{

	m_sendPacketEvent.Cancel();

}


void Isa100PacketGeneratorApplication::SendPacket()
{
  NS_LOG_FUNCTION (this);

	Ptr<Packet> p = Create<Packet>(m_packetSize);

	DlDataRequestParams params;
	params.m_srcAddr = m_srcAddress;
	params.m_destAddr = m_dstAddress;
	params.m_dsduLength = m_packetSize;
	params.m_dsduHandle = 0;  // Consider getting rid of this parameter.

	m_dlDataRequest(params,p);

	if(++m_numberPacketsSent < m_numberOfPackets || m_numberOfPackets == 0)
		Simulator::Schedule(m_txInterval,&Isa100PacketGeneratorApplication::SendPacket,this);

}







// -----------------------------  ISA 100 FieldNode Application ----------------------------

NS_OBJECT_ENSURE_REGISTERED (Isa100FieldNodeApplication);

TypeId Isa100FieldNodeApplication::GetTypeId (void)
{
     static TypeId tid = TypeId ("ns3::Isa100FieldNodeApplication")
    .SetParent<Isa100Application> ()

    .AddConstructor<Isa100FieldNodeApplication> ()

    .AddTraceSource ("ReportTx",
        "Trace source indicating when a node generates a packet.",
        MakeTraceSourceAccessor (&Isa100FieldNodeApplication::m_reportTxTrace),
        "ns3::TracedCallback::ReportTx")



        ;
  return tid;
}

Isa100FieldNodeApplication::Isa100FieldNodeApplication ()
{
	m_sensor = 0;
	m_processor = 0;
}

Isa100FieldNodeApplication::~Isa100FieldNodeApplication ()
{
	;
}

void Isa100FieldNodeApplication::SetSensor(Ptr<Isa100Sensor> sensor)
{
	m_sensor = sensor;
}

void Isa100FieldNodeApplication::SetProcessor(Ptr<Isa100Processor> processor)
{
	m_processor = processor;
}

void Isa100FieldNodeApplication::StartSensing()
{
  NS_LOG_FUNCTION (this << Simulator::Now().GetSeconds());

  m_processor->SetState(PROCESSOR_ACTIVE);
	m_sensor->StartSensing();

  m_sampleTaskId = Simulator::Schedule(m_updatePeriod,&Isa100FieldNodeApplication::StartSensing,this);
}


void Isa100FieldNodeApplication::SensorSampleCallback(double data)
{
  NS_LOG_FUNCTION (this << Simulator::Now().GetSeconds());

  /*
	 * NOTE: There currently is no callback mechanism to sync the application layer sampling to the packet delivery.
	 * It's possible the two may get out of sync in a network that models timing drift or in the event of severe
	 * congestion.  This may have to be addressed in future versions of the simulator.
	 *
	 * Probably the simplest fix is to have a callback to the DL that just asks the DL if it's ready to accept
	 * a new packet.  The definition of "ready" can be sorted out by the DL.
	 */

  // A new sample exists, request to send the measurement to the sink
  Ptr<Packet> measurementPacket = Create<Packet>(m_packetSize);

  DlDataRequestParams params;
  params.m_dsduHandle = DATA_PACKET;
  params.m_srcAddr = m_srcAddress;
  params.m_destAddr = m_dstAddress;
  params.m_dsduLength = m_packetSize;

  // Send the packet to the sink and put the processor to sleep.
  m_dlDataRequest(params,measurementPacket);
  m_processor->SetState(PROCESSOR_SLEEP);

  // Log the transmission
  m_reportTxTrace(params.m_srcAddr);


}

void Isa100FieldNodeApplication::StartApplication (void)
{
	// Get schedule information
  UintegerValue framePeriodV;
  TimeValue slotDurationV;
  PointerValue sfSchedV;
  Ptr<Isa100NetDevice> devPtr = m_node->GetDevice(0)->GetObject<Isa100NetDevice>();
  devPtr->GetDl()->GetAttribute("SuperFramePeriod", framePeriodV);
  devPtr->GetDl()->GetAttribute("SuperFrameSlotDuration", slotDurationV);
  devPtr->GetDl()->GetAttribute("SuperFrameSchedule", sfSchedV);
  m_slotDuration = slotDurationV.Get();
  uint16_t numMultiFrames =sfSchedV.Get<Isa100DlSfSchedule>()->GetFrameBounds()->size();
  m_updatePeriod = m_slotDuration * framePeriodV.Get() / numMultiFrames;

  TimeValue sensingTime;
  devPtr->GetSensor()->GetAttribute("SensingTime",sensingTime);
  m_sampleDuration = sensingTime.Get();

  // Determine the time to start the sample, so it completes one slot before the same active slot next frame
  Time delayUntilSample = m_updatePeriod - m_sampleDuration - 2 * m_slotDuration;
  NS_ASSERT_MSG(delayUntilSample >= 0, "TDMA App: The frame length must be greater than the amount of time it takes for one sample plus 1 slot.");
  m_sampleTaskId = Simulator::Schedule(delayUntilSample,&Isa100FieldNodeApplication::StartSensing,this);

}

void Isa100FieldNodeApplication::StopApplication (void)
{
  m_sampleTaskId.Cancel();
}




// ----------------------------- ISA 100 Backbone Application ----------------------------

NS_OBJECT_ENSURE_REGISTERED (Isa100BackboneNodeApplication);

TypeId Isa100BackboneNodeApplication::GetTypeId (void)
{
     static TypeId tid = TypeId ("ns3::Isa100BackboneNodeApplication")
    .SetParent<Isa100Application> ()

    .AddConstructor<Isa100BackboneNodeApplication> ()

    .AddTraceSource ("ReportRx",
        "Trace source indicating when a node generates a packet.",
        MakeTraceSourceAccessor (&Isa100BackboneNodeApplication::m_reportRxTrace),
        "ns3::TracedCallback::ReportRx")

        ;
  return tid;
}

Isa100BackboneNodeApplication::Isa100BackboneNodeApplication ()
{
	;
}

Isa100BackboneNodeApplication::~Isa100BackboneNodeApplication ()
{
	;
}

void Isa100BackboneNodeApplication::StartApplication (void)
{
	;
}

void Isa100BackboneNodeApplication::StopApplication (void)
{
	;
}


void Isa100BackboneNodeApplication::DlDataIndication (DlDataIndicationParams params, Ptr<Packet> p)
{
  // There shouldn't be broadcasts happening in TDMA
  Mac16Address broadcastAddr ("ff:ff");
  NS_ASSERT_MSG(params.m_destAddr != broadcastAddr, "Sink App:TDMA does not support broadcasts!");

  m_reportRxTrace(params.m_srcAddr);
}

void Isa100BackboneNodeApplication::DlDataConfirm (DlDataConfirmParams params)
{
  // The sink never transmits any packets
  NS_FATAL_ERROR("Sink Application: In TDMA the sink does not transmit.");
}





} // namespace ns3
