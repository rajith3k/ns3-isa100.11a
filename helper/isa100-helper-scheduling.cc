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


#include "ns3/isa100-helper.h"

#include "ns3/position-allocator.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/boolean.h"

#include "ns3/zigbee-trx-current-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/isa100-application.h"
#include "ns3/isa100-routing.h"
#include "ns3/isa100-net-device.h"
#include "ns3/goldsmith-tdma-optimizer.h"
#include "ns3/minhop-tdma-optimizer.h"
#include "ns3/convex-integer-tdma-optimizer.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/packet.h"


NS_LOG_COMPONENT_DEFINE ("Isa100HelperScheduling");

namespace ns3 {


// ... General Functions ...

void Isa100Helper::SetSfSchedule(
		uint32_t nodeInd, uint8_t *hopPattern, uint32_t numHop,
		uint16_t *linkSched, DlLinkType *linkTypes, uint32_t numLink
		)
{
	Ptr<NetDevice> baseDevice = m_devices.Get(nodeInd);
	Ptr<Isa100NetDevice> netDevice = baseDevice->GetObject<Isa100NetDevice>();

	if(!netDevice)
		NS_FATAL_ERROR("Installing schedule on non-existent ISA100 net device.");

	Ptr<Isa100DlSfSchedule> schedulePtr = CreateObject<Isa100DlSfSchedule>();
	schedulePtr->SetSchedule(hopPattern,numHop,linkSched,linkTypes,numLink);
	netDevice->GetDl()->SetDlSfSchedule(schedulePtr);

}


void Isa100Helper::SetTdmaOptAttribute(std::string n, const AttributeValue &v)
{
  NS_LOG_FUNCTION (this);

  m_tdmaOptAttributes.insert ( std::pair< std::string, Ptr<AttributeValue> > (n,v.Copy()) );
}

void Isa100Helper::SetTdmaOptimizerAttributes(Ptr<TdmaOptimizerBase> optimizer)
{
  NS_LOG_FUNCTION (this);

  if(!m_tdmaOptAttributes.size())
    NS_FATAL_ERROR("Optimizer needs its attributes configured before solving.");

  std::map<std::string,Ptr<AttributeValue> >::iterator it;
  for (it=m_tdmaOptAttributes.begin(); it!=m_tdmaOptAttributes.end(); ++it)
  {
    std::string name = it->first;
    if(!name.empty())
    {
      optimizer->SetAttribute(it->first , *it->second);
    }
  }
}



SchedulingResult Isa100Helper::CreateOptimizedTdmaSchedule(NodeContainer c, Ptr<PropagationLossModel> propModel,
    uint8_t *hopPattern, uint32_t numHop, OptimizerSelect optSelect, Ptr<OutputStreamWrapper> stream)
{

  Ptr<Isa100NetDevice> devPtr = c.Get(1)->GetDevice(0)->GetObject<Isa100NetDevice>();
	UintegerValue numSlotsV;
  devPtr->GetDl()->GetAttribute("SuperFramePeriod", numSlotsV);
  m_numTimeslots = numSlotsV.Get();


  // Create the optimization solver
  Ptr<TdmaOptimizerBase> tdmaOptimizer;

  switch(optSelect){
  	case TDMA_MIN_HOP:
  	{
  		tdmaOptimizer = CreateObject<MinHopTdmaOptimizer>();
  		break;
  	}

  	case TDMA_GOLDSMITH:
  	{
  		tdmaOptimizer = CreateObject<GoldsmithTdmaOptimizer> ();
  		break;
  	}

  	case TDMA_CONVEX_INT:
  	{
  		tdmaOptimizer = CreateObject<ConvexIntTdmaOptimizer> ();
  		break;
  	}

  	default:
  		NS_FATAL_ERROR("Invalid selection of optimizer!");
  }

  // Set the attributes
  SetTdmaOptimizerAttributes(tdmaOptimizer);

  // Pass network information to setup the optimizer
  tdmaOptimizer->SetupOptimization(c, propModel);

  // Solve the optimization to create flow matrix
  vector< vector<int> > slotFlows;
  slotFlows = tdmaOptimizer->SolveTdma();

  // Configure the TDMA schedule and source routes.
  CalculateTxPowers(c,propModel);

  IntegerValue intV;
  tdmaOptimizer->GetAttribute("PacketsPerSlot", intV);

  return ScheduleAndRouteTdma(slotFlows,intV.Get());

}


SchedulingResult Isa100Helper::ScheduleAndRouteTdma(vector< vector<int> > flows, int packetsPerSlot)
{

	int numNodes = m_devices.GetN();
	SchedulingResult schedulingResult = SCHEDULE_FOUND;

  vector<NodeSchedule> nodeSchedules(numNodes);
  vector<std::string> routingStrings(numNodes,"No Route");
  vector< vector<int> > scheduleSummary;

  schedulingResult = FlowMatrixToTdmaSchedule(nodeSchedules,scheduleSummary,flows);

  if(schedulingResult != SCHEDULE_FOUND)
  	return schedulingResult;

  schedulingResult = CalculateSourceRouteStrings(routingStrings,scheduleSummary);

  if(schedulingResult != SCHEDULE_FOUND)
  	return schedulingResult;

  for(int nNode=0; nNode < numNodes; nNode++){

    // Assign schedule to DL
  	Ptr<NetDevice> baseDevice = m_devices.Get(nNode);
  	Ptr<Isa100NetDevice> netDevice = baseDevice->GetObject<Isa100NetDevice>();

    if(!baseDevice || !netDevice)
      NS_FATAL_ERROR("Installing TDMA schedule on non-existent ISA100 net device.");

    // Create routing object only for field nodes
    // NOTE: This current implementation only allows for a single path from a field node to the sink.
    if(nNode > 0)
    {
    	std::string routingTable[] = { routingStrings[ nNode ] };
    	int numNodes = 1;

    	Ptr<Isa100RoutingAlgorithm> routingAlgorithm = CreateObject<Isa100SourceRoutingAlgorithm>(numNodes,routingTable);
    	netDevice->GetDl()->SetRoutingAlgorithm(routingAlgorithm);

    	Mac16AddressValue address;
    	netDevice->GetDl()->GetAttribute("Address",address);
    	netDevice->GetDl()->GetRoutingAlgorithm()->SetAttribute("Address",address);
    }

    // Set the tx power levels in DL
    netDevice->GetDl()->SetTxPowersDbm(m_txPwrDbm[nNode], numNodes);

    // Set the sfSchedule
    vector<uint8_t> hoppingPattern(1,11);  // Stay on channel 11.

    Ptr<Isa100DlSfSchedule> schedulePtr = CreateObject<Isa100DlSfSchedule>();

    schedulePtr->SetSchedule(hoppingPattern,nodeSchedules[nNode].slotSched,nodeSchedules[nNode].slotType);

    netDevice->GetDl()->SetDlSfSchedule(schedulePtr);
  }

  return schedulingResult;
}


// This power calculation is happening *everywhere* and should be centralized.
void Isa100Helper::CalculateTxPowers(NodeContainer c, Ptr<PropagationLossModel> propModel)
{
  const uint32_t numNodes = c.GetN();

  // Obtain all node locations
  std::vector<Ptr<MobilityModel> > positions;
  for (uint32_t i = 0; i < numNodes; i++)
  {
  	Ptr<Node> node = c.Get(i);
  	Ptr<NetDevice> baseDevice = node->GetDevice(0);
  	Ptr<Isa100NetDevice> devPtr = baseDevice->GetObject<Isa100NetDevice>();
  	positions.push_back(devPtr->GetPhy()->GetMobility());
  }

  // Determine attenuation between each link
  Ptr<NetDevice> baseDevice = c.Get(1)->GetDevice(0);
  Ptr<Isa100NetDevice> netDevice = baseDevice->GetObject<Isa100NetDevice>();

  IntegerValue txPowerValue;
  netDevice->GetDl()->GetAttribute("MaxTxPowerDbm",txPowerValue);
  double maxTxPowerDbm = txPowerValue.Get();

  DoubleValue rxSensValue;
  netDevice->GetPhy()->GetAttribute("SensitivityDbm",rxSensValue);
  double rxSensitivityDbm = rxSensValue.Get();


  m_txPwrDbm = new double*[numNodes];

  for(int iNode=0; iNode < numNodes; iNode++)
  	m_txPwrDbm[iNode] = new double[numNodes];

  for(int iNode=0; iNode < numNodes; iNode++){

  	for(int jNode=iNode; jNode < numNodes; jNode++){
  		if(iNode == jNode)
  			m_txPwrDbm[iNode][jNode] = rxSensitivityDbm;
  		else{
  			m_txPwrDbm[iNode][jNode] = -(propModel->CalcRxPower (0, positions[iNode], positions[jNode])) + rxSensitivityDbm;
  			m_txPwrDbm[jNode][iNode] = m_txPwrDbm[iNode][jNode];
  		}
  	}
  }



}


// ... TDMA Superframe Generation Functions ...


#define NS_LOG_DEBUG_VECTOR_DUMP(mstr,v) { \
	std::stringstream sss; \
	sss << mstr << ": "; \
	for(int z=0; z<v.size();z++) \
		sss << v[z] << " "; \
	NS_LOG_DEBUG(sss.str()); \
}

SchedulingResult Isa100Helper::FlowMatrixToTdmaSchedule(vector<NodeSchedule> &lAll, vector< vector<int> > &scheduleSummary, vector< vector<int> > packetFlows)
{

	NS_LOG_DEBUG("Flow Scheduler:");

	int numNodes = packetFlows.size();

	// Uses the scheduling algorithm from Cui, Madan and Goldsmith

	// Init q with all nodes that can reach the sink directly.
	vector<int> q;
	for(int i=0; i<numNodes; i++)
		if(packetFlows[i][0])
			q.push_back(i);


	// Determine the maximum slot index
	int nSlot = -1;
	for(int i=0; i < numNodes; i++)
		for(int j=0; j < numNodes; j++)
			nSlot += packetFlows[i][j];

	NS_LOG_UNCOND(" Scheduling " << nSlot << " slots.");
	if(nSlot > m_numTimeslots)
		return INSUFFICIENT_SLOTS;

	scheduleSummary.resize(nSlot+1);
	for(int iInit=0; iInit <= nSlot; iInit++)
		scheduleSummary[iInit].assign(2,0);

	// Schedule all the direct transmissions.
	for(int qInd=0; qInd < q.size(); qInd++){
		PopulateNodeSchedule(q[qInd],0,packetFlows[ q[qInd] ][0],lAll,nSlot,scheduleSummary);
		packetFlows[ q[qInd] ][0] = -1;  // Indicate this edge has been scheduled.
	}

	int qInd = 0;
	vector<int> q0 = q;

	while( q0.size() ){

		q0.clear();
		while( q.size() ){

			NS_LOG_DEBUG_VECTOR_DUMP("q",q);
			NS_LOG_DEBUG_VECTOR_DUMP("q0",q0);

			// Leaf nodes have already been scheduled when their parents were processed.
			if( IsLeaf(q[qInd],packetFlows) ){
				NS_LOG_DEBUG("Erasing " << q[qInd]);
				q.erase(q.begin()+qInd);
			}

			// Current node can only accept input links if all its output links are scheduled.
			// Otherwise, it won't be able to get rid of all its incoming links.
			else if( q.size() && AllOutlinksScheduled( q[qInd], packetFlows) ){

				// Determine all nodes that can reach this non-leaf node
				vector <int> nI;
				for(int i=0; i < numNodes; i++)
					if(packetFlows[i][ q[qInd] ])
						nI.push_back(i);

				for(int nIInd=0; nIInd < nI.size(); nIInd++){

					PushBackNoDuplicates(nI[nIInd], q0);
					PopulateNodeSchedule(nI[nIInd],q[qInd],packetFlows[ nI[nIInd] ][ q[qInd] ],lAll,nSlot,scheduleSummary);
					packetFlows[ nI[nIInd] ][ q[qInd] ] = -1;
				}

				q.erase(q.begin()+qInd);
				NS_LOG_DEBUG("Erasing " << q[qInd]);

			}

			// If the current node does not have its outlinks scheduled but it also does not have a parent in q,
			// delete since its parent is at a level we're not processing yet.  The deleted node will get added
			// back when we finally reach its parent.
			else if( q.size() && !AllOutlinksScheduled( q[qInd], packetFlows) && NoParentInQ(q[qInd],q,packetFlows)){

				q.erase(q.begin()+qInd);
				NS_LOG_DEBUG("Erasing " << q[qInd]);

			}

			if(q.size())
				qInd = (qInd+1) % q.size();
		}

		q = q0;
		qInd = 0;
	}

	return SCHEDULE_FOUND;

}

void Isa100Helper::PopulateNodeSchedule(int src, int dst, int weight, vector<NodeSchedule> &schedules, int &nSlot, vector< vector<int> > &scheduleSummary)
{
	for(int nPacket=0; nPacket < weight; nPacket++){

		schedules[src].slotSched.insert(schedules[src].slotSched.begin(), nSlot);
		schedules[src].slotType.insert(schedules[src].slotType.begin(), TRANSMIT);

		scheduleSummary[nSlot][0] = src;
		scheduleSummary[nSlot][1] = dst;

		schedules[dst].slotSched.insert(schedules[dst].slotSched.begin(), nSlot--);
		schedules[dst].slotType.insert(schedules[dst].slotType.begin(), RECEIVE);

		NS_LOG_DEBUG( " (" << src << ")->(" << dst << ") in slot " << (nSlot+1) );
	}

}

int Isa100Helper::AllOutlinksScheduled(int node, vector< vector<int> > &packetFlows)
{
	for(int j=0; j < packetFlows[node].size(); j++)
		if( packetFlows[node][j] > 0 )
			return 0;
	return 1;
}

void Isa100Helper::PushBackNoDuplicates(int node, vector<int> &q0)
{
	for(int qInd=0; qInd < q0.size(); qInd++)
		if(q0[qInd] == node)
			return;

	q0.push_back(node);

}

bool Isa100Helper::IsLeaf(int node, vector< vector<int> > packetFlows)
{
	// A node is a leaf if nothing transmits to it
	int i;
	for(i=0; i < packetFlows[node].size() && !packetFlows[i][node]; i++) ;

	return (i == packetFlows[node].size());

}

bool Isa100Helper::NoParentInQ(int node, vector<int> q, vector< vector<int> > &packetFlows)
{
	for(int j=0; j < packetFlows[node].size(); j++)
		if( packetFlows[node][j] > 0 )
			for(int qInd=0; qInd < q.size(); qInd++)
				if(q[qInd] == j)
					return false;

	return true;
}



// ... Source Routing List Generation ...

SchedulingResult Isa100Helper::CalculateSourceRouteStrings(vector<std::string> &routingStrings, vector< vector<int> > schedule)
{
	NS_LOG_DEBUG("Routing Strings: ");

	vector<int> hopCount;

	for(int nSlot=0; nSlot < schedule.size(); nSlot++){

		unsigned int curNode = schedule[nSlot][0];
		unsigned int nextNode = schedule[nSlot][1];
		unsigned int startNode = curNode;

		if(routingStrings[startNode] == "No Route"){

			// Follow the path to the sink taking the lowest node index when the path branches.
			std::stringstream ss;
			bool firstEntry = true;
			int numHops = 0;
			while(curNode != 0){

				unsigned int upperByte = (nextNode & 0xff00) >> 8;
				unsigned int lowerByte = (nextNode & 0xff);

				if(firstEntry)
					firstEntry = false;
				else
					ss << " ";

				ss << std::setfill('0') << std::setw(2) << std::hex << upperByte;
				ss << ":";
				ss << std::setfill('0') << std::setw(2) << std::hex << lowerByte;


				curNode = nextNode;
				if(curNode != 0){

					int iNext = nSlot+1;
					for(; schedule[iNext][0] != curNode && iNext < schedule.size(); iNext++) ;

					if(iNext == schedule.size())
						return NO_ROUTE;

					nextNode = schedule[iNext][1];

				}
				numHops++;

			}

			NS_LOG_DEBUG(" Node " << startNode << ": " << ss.str());

			routingStrings[ startNode ] = ss.str();
			hopCount.push_back(numHops);
		}
	}

	m_hopTrace(hopCount);

	return SCHEDULE_FOUND;


}








}
