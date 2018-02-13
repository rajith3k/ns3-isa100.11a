/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014 Geoffrey Messier
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
 *
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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include "ns3/mac16-address.h"
#include "ns3/zigbee-phy.h"
#include "ns3/isa100-11a-module.h"






using namespace ns3;

long Seeder()
{
  long randnum;
  int fd = open ("/dev/urandom", O_RDONLY);
  if (fd != -1) {
    read( fd, &randnum, 4 );
    close(fd);
  }
  else
    NS_FATAL_ERROR("ERROR: Can't read /dev/urandom.");

  if(randnum<0) randnum*=-1;
  if(!randnum) NS_FATAL_ERROR("ERROR: Zero seed!\n");

  return randnum;
}

static void PrintPacket (Ptr<OutputStreamWrapper> stream, Mac16Address addr, Ptr<const Packet> p)
{
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << addr << "\t" << *p << std::endl;
}

void ConnectPacketStream(int nodeInd, NetDeviceContainer &devContainer, std::string traceName, const CallbackBase& callback)
{
	Ptr<NetDevice> baseDevice;
	Ptr<Isa100NetDevice> netDevice;
	baseDevice = devContainer.Get(nodeInd);
	netDevice = baseDevice->GetObject<Isa100NetDevice>();
	netDevice->GetDl()->TraceConnectWithoutContext (traceName, callback);
}



NS_LOG_COMPONENT_DEFINE("Isa100RoutingTest");


/*
 *                   n0
 *                   |
 *                   |
 *           n3-----n1-----n2
 *           |
 *           |
 *           n4
 */

int main (int argc, char *argv[])
{
	// The row index corresponds to the node index of the final destination.
	// The columns contain the address list.

	std::string node0RoutingTable[] = {
			"00:00",
			"00:01",
			"00:01 00:02",
			"00:01 00:03",
			"00:01 00:03 00:04"
	};

	std::string node1RoutingTable[] = {
			"00:00",
			"00:01",
			"00:02",
			"00:03",
			"00:03 00:04"
	};

	std::string node2RoutingTable[] = {
			"00:01 00:00",
			"00:01",
			"00:02",
			"00:01 00:03",
			"00:01 00:03 00:04"
	};

	std::string node3RoutingTable[] = {
			"00:01 00:00",
			"00:01",
			"00:01 00:02",
			"00:03",
			"00:04"
	};

	std::string node4RoutingTable[] = {
			"00:03 00:01 00:00",
			"00:03 00:01",
			"00:03 00:01 00:02",
			"00:03",
			"00:04"
	};




	//	LogComponentEnableAll(LOG_LEVEL_ALL);
	LogComponentEnable("Isa100Dl",LOG_LEVEL_LOGIC);
	//	LogComponentEnable("Packet",LOG_LEVEL_LOGIC);
	//	LogComponentEnable("Isa100Helper",LOG_LEVEL_LOGIC);
//	LogComponentEnable("LrWpanPhy",LOG_LEVEL_LOGIC);
//	LogComponentEnable("RicianShadowLogDistancePropagationLossModel",LOG_LEVEL_LOGIC);
	LogComponentEnable("Isa100Routing",LOG_LEVEL_LOGIC);
//	LogComponentEnable("Packet",LOG_LEVEL_LOGIC);
//	LogComponentEnable("PacketMetadata",LOG_LEVEL_LOGIC);


	Packet::EnablePrinting ();

	long seed = Seeder();
	NS_LOG_INFO("Seed " << seed << "\n");
	RngSeedManager::SetSeed(seed);

  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel> ();
  Ptr<FishLogDistanceLossModel> propModel = CreateObject<FishLogDistanceLossModel> ();
  channel->AddPropagationLossModel (propModel);


  propModel->SetAttribute("PathLossExponent",DoubleValue(2.2));
  propModel->SetAttribute("IsStationaryNetwork",BooleanValue(false));


  int16_t numberOfNodes=5;
  NodeContainer nc;
  nc.Create(numberOfNodes);

  Isa100Helper isaHelper;

  isaHelper.SetDlAttribute("SuperFramePeriod",UintegerValue(4));
  isaHelper.SetDlAttribute("SuperFrameSlotDuration",TimeValue(MilliSeconds(10)));

  NetDeviceContainer devContainer;
	devContainer = isaHelper.Install(nc, channel,0);

	// Assume channels 11-26 are utilized in the hopping pattern (802.15.4 channel page 0, OQPSK, 250kbit/s)
	uint8_t hoppingPattern[] = { 11 };
	uint16_t linkSchedule[] = { 0 };
	DlLinkType nodeLinkTypes[] = { SHARED };

	isaHelper.SetSfSchedule(0,hoppingPattern,1,linkSchedule,nodeLinkTypes,1);
	isaHelper.SetSfSchedule(1,hoppingPattern,1,linkSchedule,nodeLinkTypes,1);
	isaHelper.SetSfSchedule(2,hoppingPattern,1,linkSchedule,nodeLinkTypes,1);
	isaHelper.SetSfSchedule(3,hoppingPattern,1,linkSchedule,nodeLinkTypes,1);
	isaHelper.SetSfSchedule(4,hoppingPattern,1,linkSchedule,nodeLinkTypes,1);

	isaHelper.SetSourceRoutingTable(0,5,node0RoutingTable);
	isaHelper.SetSourceRoutingTable(1,5,node1RoutingTable);
	isaHelper.SetSourceRoutingTable(2,5,node2RoutingTable);
	isaHelper.SetSourceRoutingTable(3,5,node3RoutingTable);
	isaHelper.SetSourceRoutingTable(4,5,node4RoutingTable);

	/*
	 *                   n0
	 *                   |
	 *                   |
	 *           n3-----n1-----n2
	 *           |
	 *           |
	 *           n4
	 */

	/*
	 * A distance of 515m was calculated to have a ~1% FER (ie. a 10% FER for 10% of the time) by
	 * the LinkBudget.m script in the fish/doc directory.
	 */
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

	positionAlloc->Add(ns3::Vector(0,515,1030));
	positionAlloc->Add(ns3::Vector(1,515,515));
	positionAlloc->Add(ns3::Vector(2,1030,515));
	positionAlloc->Add(ns3::Vector(3,0,515));
	positionAlloc->Add(ns3::Vector(4,0,0));

	isaHelper.SetDeviceConstantPosition(devContainer,positionAlloc);

	/*
	 * Set up a scenario where n4 is sending to n2 and n0 is sending to n3.
	 */

	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream ("TxPackets.txt",std::ios::out);
	Ptr<OutputStreamWrapper> rxOkStream = asciiTraceHelper.CreateFileStream ("RxOkPackets.txt",std::ios::out);
	Ptr<OutputStreamWrapper> rxDropStream = asciiTraceHelper.CreateFileStream ("RxDropPackets.txt",std::ios::out);
	Ptr<OutputStreamWrapper> fwdStream = asciiTraceHelper.CreateFileStream ("ForwardPackets.txt",std::ios::out);

	ConnectPacketStream(0,devContainer,"DlTx",MakeBoundCallback(&PrintPacket, txStream));
	ConnectPacketStream(4,devContainer,"DlTx",MakeBoundCallback(&PrintPacket, txStream));

	ConnectPacketStream(2,devContainer,"DlRx",MakeBoundCallback(&PrintPacket, rxOkStream));
	ConnectPacketStream(3,devContainer,"DlRx",MakeBoundCallback(&PrintPacket, rxOkStream));

	ConnectPacketStream(0,devContainer,"DlForward",MakeBoundCallback(&PrintPacket, fwdStream));
	ConnectPacketStream(1,devContainer,"DlForward",MakeBoundCallback(&PrintPacket, fwdStream));
	ConnectPacketStream(2,devContainer,"DlForward",MakeBoundCallback(&PrintPacket, fwdStream));
	ConnectPacketStream(3,devContainer,"DlForward",MakeBoundCallback(&PrintPacket, fwdStream));
	ConnectPacketStream(4,devContainer,"DlForward",MakeBoundCallback(&PrintPacket, fwdStream));

	ConnectPacketStream(0,devContainer,"PhyRxDrop",MakeBoundCallback(&PrintPacket, rxDropStream));
	ConnectPacketStream(1,devContainer,"PhyRxDrop",MakeBoundCallback(&PrintPacket, rxDropStream));
	ConnectPacketStream(2,devContainer,"PhyRxDrop",MakeBoundCallback(&PrintPacket, rxDropStream));
	ConnectPacketStream(3,devContainer,"PhyRxDrop",MakeBoundCallback(&PrintPacket, rxDropStream));
	ConnectPacketStream(4,devContainer,"PhyRxDrop",MakeBoundCallback(&PrintPacket, rxDropStream));


	// Create a single application object to connect to Node 1 net device.
	Ptr<Isa100PacketGeneratorApplication> appNode0 = CreateObject<Isa100PacketGeneratorApplication>();

	appNode0->SetAttribute("DestAddress",Mac16AddressValue("00:03"));
	appNode0->SetAttribute("NumberOfPackets",UintegerValue(1));
	appNode0->SetAttribute("StartTime",TimeValue(Seconds(0.0)));
	appNode0->SetAttribute("TxInterval",TimeValue(MilliSeconds(10.0)));
	appNode0->SetAttribute("PacketSize",UintegerValue(32));
	appNode0->SetAttribute("SrcAddress",Mac16AddressValue("00:00"));

	isaHelper.InstallApplication(nc,0,appNode0);

	Ptr<Isa100PacketGeneratorApplication> appNode4 = CreateObject<Isa100PacketGeneratorApplication>();

	appNode4->SetAttribute("DestAddress",Mac16AddressValue("00:02"));
	appNode4->SetAttribute("NumberOfPackets",UintegerValue(1));
	appNode4->SetAttribute("StartTime",TimeValue(Seconds(0.25111)));
	appNode4->SetAttribute("TxInterval",TimeValue(MilliSeconds(10.0)));
	appNode4->SetAttribute("PacketSize",UintegerValue(32));
	appNode4->SetAttribute("SrcAddress",Mac16AddressValue("00:04"));

	isaHelper.InstallApplication(nc,4,appNode4);



	// Run simulator
  Simulator::Stop (Seconds (1.0));
	NS_LOG_UNCOND ("Simulation is running ....");
	Simulator::Run ();
//	Simulator::Destroy ();


	return 0;
}
