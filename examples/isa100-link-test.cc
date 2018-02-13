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


using namespace ns3;


int nAtten = 3;
int nPackets = 20000;
float atten[] = { 50, 105.5, 200 };

int *noTxPackets;
int *noRxPackets;
int attInd = 0;

static void IncTxPackets( int *ind, Ptr<FishFixedLossModel> chan, Mac16Address addr, Ptr<const Packet> p)
{
	// Stop incrementing if all the attenuation values have been completed.
	if(*ind == nAtten)
		return;

	noTxPackets[*ind]++;

	// Output results and increment to the next attenuation value.
	if( noTxPackets[*ind] >= nPackets ){
		printf("Atten: %g, Tx: %d, Rx: %d, FER: %g\n",
				atten[*ind],noTxPackets[*ind],noRxPackets[*ind],
				(double)(noTxPackets[*ind] - noRxPackets[*ind])/noTxPackets[*ind]);
		(*ind)++;

		chan->SetLoss(atten[*ind]);
	}

}

static void IncRxPackets( int *packetInd,  Mac16Address addr, Ptr<const Packet> p)
{
	noRxPackets[*packetInd]++;
}


int main (int argc, char *argv[])
{
	// Initialize FER counters.
	noTxPackets = new int[nAtten];
	noRxPackets = new int[nAtten];

	for(int i=0; i<nAtten; i++){
		noTxPackets[i] = 0;
		noRxPackets[i] = 0;
	}


	// Change the random number seed to alter the random number sequence used by the simulator.
  RngSeedManager::SetSeed (100);

//	LogComponentEnableAll(LOG_LEVEL_ALL);
//	LogComponentEnable("Isa100Dl",LOG_LEVEL_LOGIC);
//	LogComponentEnable("Packet",LOG_LEVEL_LOGIC);
//	LogComponentEnable("Isa100Helper",LOG_LEVEL_LOGIC);
//	LogComponentEnable("ZigbeePhy",LOG_LEVEL_DEBUG);


  // These classes implement a path loss model used to determine the power level of recevied wireless signals.
  Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel> ();
  Ptr<FishFixedLossModel> propModel = CreateObject<FishFixedLossModel> ();
  channel->AddPropagationLossModel (propModel);
  propModel->SetLoss(atten[0]);

  int16_t numberOfNodes=2;
  NodeContainer nc;
  nc.Create(numberOfNodes);

  Isa100Helper isaHelper;

  // These are ISA100 frame size parameters.  Look in the ISA100 standard for more information.
  isaHelper.SetDlAttribute("SuperFramePeriod",UintegerValue(1));
  isaHelper.SetDlAttribute("SuperFrameSlotDuration",TimeValue(MilliSeconds(10)));

  NetDeviceContainer devContainer;
	devContainer = isaHelper.Install(nc, channel, 0);

	// Configure the two nodes to transmit directly from one to the other.
	uint8_t node0HoppingPattern[] = { 11 };
	uint8_t node1HoppingPattern[] = { 11 };

	uint16_t linkSchedule[] = { 0 };

	DlLinkType node0LinkTypes[] = { RECEIVE };
	DlLinkType node1LinkTypes[] = { TRANSMIT };

	isaHelper.SetSfSchedule(0,node0HoppingPattern,1,linkSchedule,node0LinkTypes,1);
	isaHelper.SetSfSchedule(1,node1HoppingPattern,1,linkSchedule,node1LinkTypes,1);

	// The mobile positions aren't used by the channel model but they still need to exist.
	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

	positionAlloc->Add(ns3::Vector(0,0,0));
	positionAlloc->Add(ns3::Vector(1,0,0));

	isaHelper.SetDeviceConstantPosition(devContainer,positionAlloc);

	// Link the functions that record the number of transmitted and received packets.
	Ptr<NetDevice> baseDevice;
	Ptr<Isa100NetDevice> netDevice;

	baseDevice = devContainer.Get(0);
	netDevice = baseDevice->GetObject<Isa100NetDevice>();
	netDevice->GetDl()->TraceConnectWithoutContext ("DlRx", MakeBoundCallback (&IncRxPackets, &attInd));

	baseDevice = devContainer.Get(1);
	netDevice = baseDevice->GetObject<Isa100NetDevice>();
	netDevice->GetDl()->TraceConnectWithoutContext ("DlTx", MakeBoundCallback (&IncTxPackets, &attInd, propModel));


  // Configure the application that generates the packets.
	Ptr<Isa100PacketGeneratorApplication> appNode1 = CreateObject<Isa100PacketGeneratorApplication>();

	appNode1->SetAttribute("DestAddress",Mac16AddressValue("00:00"));
	appNode1->SetAttribute("NumberOfPackets",UintegerValue(nPackets*nAtten));
	appNode1->SetAttribute("StartTime",TimeValue(Seconds(0.0)));
	appNode1->SetAttribute("TxInterval",TimeValue(MilliSeconds(10.0)));
	appNode1->SetAttribute("PacketSize",UintegerValue(5));
	appNode1->SetAttribute("SrcAddress",Mac16AddressValue("00:01"));

	isaHelper.InstallApplication(nc,1,appNode1);

	// Run simulator
  Simulator::Stop (Seconds (0.05*nPackets*nAtten));
	NS_LOG_UNCOND ("Simulation is running ....");
	Simulator::Run ();

	delete[] noTxPackets;
	delete[] noRxPackets;

	return 0;
}
