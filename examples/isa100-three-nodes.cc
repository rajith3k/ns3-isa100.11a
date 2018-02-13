/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 The University Of Calgary- FISHLAB
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
 * Author:   Hazem Gomaa <gomaa.hazem@gmail.com>
 */

#include "ns3/log.h"
#include "ns3/core-module.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"
#include <ns3/single-model-spectrum-channel.h>
#include <ns3/constant-position-mobility-model.h>
#include "ns3/mobility-module.h"
#include "ns3/net-device-container.h"
#include "ns3/trace-helper.h"
#include "ns3/callback.h"
#include "ns3/application.h"
#include "ns3/rng-seed-manager.h"


#include <iostream>
#include "ns3/mac16-address.h"
#include "ns3/zigbee-phy.h"
#include "ns3/isa100-11a-module.h"



#define LD_EXP 3.0


using namespace ns3;

/*
 * This simulation uses a technique called "tracing" to record transmitted and received packets.
 * Essentially, a callback function in the ISA100 DL is attached to the PrintPacket function below.
 * Every time the DL transmits or receives a packet, it calls PrintPacket and the information from
 * that packet is sent to a text file.
 *
 * See the tracing section of the ns3 tutorial for more information.
 *
 */

static void PrintPacket (Ptr<OutputStreamWrapper> stream, Mac16Address addr, Ptr<const Packet> p)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << addr << "\t" << *p << std::endl;
}

int main (int argc, char *argv[])
{

	// Change the random number seed to alter the random number sequence used by the simulator.
  RngSeedManager::SetSeed (100);



  /*
   * ns3 also has a feature called "logging" that allows debugging messages to be printed to the screen.
   * You could also print these messages using regular print statements but logging allows the messages
   * to be grouped into priority levels and turned on and off using the commands below.
   *
   * See the logging section of the ns3 tutorial for more information.
   *
   */

//	LogComponentEnableAll(LOG_LEVEL_ALL);
	LogComponentEnable("Isa100Dl",LOG_LEVEL_LOGIC);
	LogComponentEnable("Isa100Application",LOG_LEVEL_LOGIC);
//	LogComponentEnable("Packet",LOG_LEVEL_LOGIC);
//	LogComponentEnable("Isa100Helper",LOG_LEVEL_LOGIC);
//	LogComponentEnable("ZigbeePhy",LOG_LEVEL_LOGIC);

	// This command is necessary if you want to print out packet information (like we do in PrintPacket above).
  Packet::EnablePrinting ();


  // These classes implement a path loss model used to determine the power level of recevied wireless signals.
  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel> ();
  Ptr<LogDistancePropagationLossModel> propModel = CreateObject<LogDistancePropagationLossModel> ();
  propModel->SetPathLossExponent(LD_EXP);
  channel->AddPropagationLossModel (propModel);

  // We're simulating just 3 nodes.  Nodes are usually stored in a 'container' class.
  int16_t numberOfNodes=3;
  NodeContainer nc;
  nc.Create(numberOfNodes);

  // Helper classes are used in ns3 to help configure nodes before simulation by setting parameters,
  // hooking them into the channel class, setting up the protocol stack, etc.  It's worth looking at the
  // helper source code fairly soon in order to get an idea of how the nodes are configured.
  Isa100Helper isaHelper;

  // These are ISA100 frame size parameters.  Look in the ISA100 standard for more information.
  isaHelper.SetDlAttribute("SuperFramePeriod",UintegerValue(4));
  isaHelper.SetDlAttribute("SuperFrameSlotDuration",TimeValue(MilliSeconds(10)));

  NetDeviceContainer devContainer;
	devContainer = isaHelper.Install(nc, channel, 0);

	/*
   * ISA100 allows frequency hopping to be used to avoid interference and multipath fading.  It also
	 * allows different MAC algorithms to be used to access the timeslots on a particular channel.
	 * You can assign timeslots in a static fashion where certain nodes are configured to transmit and
	 * receive or you can designate timeslots as shared where they will access them using CSMA.
	 */

	// Assume channels 11-26 are available in the hopping pattern (802.15.4 channel page 0, OQPSK)

	// Nodes will hop between channels 11 and 12.
	uint8_t hoppingPattern[] = { 11, 12 };

	// The link schedule is 4 slots long, after which it repeats.
	uint16_t linkSchedule[] = { 0, 1, 2, 3 };

	// This specifies the what each node does in each timeslot.
	DlLinkType node0LinkTypes[] = { RECEIVE, RECEIVE, RECEIVE, SHARED };
	DlLinkType node1LinkTypes[] = { SHARED, SHARED, SHARED, SHARED };
	DlLinkType node2LinkTypes[] = { TRANSMIT, TRANSMIT, TRANSMIT, SHARED };

	isaHelper.SetSfSchedule(0,hoppingPattern,2,linkSchedule,node0LinkTypes,4);
	isaHelper.SetSfSchedule(1,hoppingPattern,2,linkSchedule,node1LinkTypes,4);
	isaHelper.SetSfSchedule(2,hoppingPattern,2,linkSchedule,node2LinkTypes,4);

	// We need to specify node positions since those are used by the channel to determine rx power levels.
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

	positionAlloc->Add(ns3::Vector(0,0,0));
	positionAlloc->Add(ns3::Vector(0,40,0));
	positionAlloc->Add(ns3::Vector(40,0,0));

	isaHelper.SetDeviceConstantPosition(devContainer,positionAlloc);

	/*
	 * Tracing is used to record transmitted/received frames at the DL layer.  In this simulation,
	 * nodes 1 and 2 transmit to node 0.
	 */

	// Write the transmitted DL nodes and received DL nodes to ascii tracefiles.
	// - To write binary and save space, replace 'std:ios:out' with 'std::ios::out | std::ios::binary'

	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> txDlStreamNode1 = asciiTraceHelper.CreateFileStream ("TxDlPackets_Node1.txt",std::ios::out);
	Ptr<OutputStreamWrapper> txDlStreamNode2 = asciiTraceHelper.CreateFileStream ("TxDlPackets_Node2.txt",std::ios::out);
	Ptr<OutputStreamWrapper> rxDlStreamNode0 = asciiTraceHelper.CreateFileStream ("RxDlPackets.txt",std::ios::out);


	// The following code attaches the PrintPacket function to the callback mechanism in the DL code.
	Ptr<NetDevice> baseDevice;
	Ptr<Isa100NetDevice> netDevice;

	baseDevice = devContainer.Get(0);
	netDevice = baseDevice->GetObject<Isa100NetDevice>();
	netDevice->GetDl()->TraceConnectWithoutContext ("DlRx", MakeBoundCallback (&PrintPacket, rxDlStreamNode0));

	baseDevice = devContainer.Get(1);
	netDevice = baseDevice->GetObject<Isa100NetDevice>();
	netDevice->GetDl()->TraceConnectWithoutContext ("DlTx", MakeBoundCallback (&PrintPacket, txDlStreamNode1));

	baseDevice = devContainer.Get(2);
	netDevice = baseDevice->GetObject<Isa100NetDevice>();
	netDevice->GetDl()->TraceConnectWithoutContext ("DlTx", MakeBoundCallback (&PrintPacket, txDlStreamNode2));


	/*
	 * Finally, we need to create applications to generate the transmitted packets and bind them to nodes 1 and 2.
	 */


	// Create a single application object to connect to Node 1 net device.
	Ptr<Isa100PacketGeneratorApplication> appNode1 = CreateObject<Isa100PacketGeneratorApplication>();

	appNode1->SetAttribute("DestAddress",Mac16AddressValue("00:00"));
	appNode1->SetAttribute("NumberOfPackets",UintegerValue(2));
	appNode1->SetAttribute("StartTime",TimeValue(Seconds(0.0)));
	appNode1->SetAttribute("TxInterval",TimeValue(MilliSeconds(10.0)));
	appNode1->SetAttribute("PacketSize",UintegerValue(5));
	appNode1->SetAttribute("SrcAddress",Mac16AddressValue("00:01"));

	isaHelper.InstallApplication(nc,1,appNode1);

	Ptr<Isa100PacketGeneratorApplication> appNode2 = CreateObject<Isa100PacketGeneratorApplication>();

	appNode2->SetAttribute("DestAddress",Mac16AddressValue("00:00"));
	appNode2->SetAttribute("NumberOfPackets",UintegerValue(2));
	appNode2->SetAttribute("StartTime",TimeValue(Seconds(0.0)));
	appNode2->SetAttribute("TxInterval",TimeValue(MilliSeconds(10.0)));
	appNode2->SetAttribute("PacketSize",UintegerValue(5));
	appNode2->SetAttribute("SrcAddress",Mac16AddressValue("00:02"));

	isaHelper.InstallApplication(nc,2,appNode2);



	// Run simulator
  Simulator::Stop (Seconds (3.0));
	NS_LOG_UNCOND ("Simulation is running ....");
	Simulator::Run ();
//	Simulator::Destroy ();


	return 0;
}
