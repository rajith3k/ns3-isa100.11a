/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 The University Of Calgary- FISHLAB
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
 * Author:   Michael Herrmann <mjherrma@ucalgary.ca>
 */

#include "ns3/minhop-tdma-optimizer.h"

#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/integer.h"

#include "ns3/zigbee-trx-current-model.h"
#include "ns3/mobility-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/isa100-net-device.h"
#include "ns3/isa100-dl.h"
#include "ns3/isa100-battery.h"

#include <ilcplex/ilocplex.h>
#include <algorithm>


NS_LOG_COMPONENT_DEFINE ("MinHopTdmaOptimizer");

using namespace ns3;
using namespace std;

struct nodeElement {
  std::vector<NetworkLink *> inLinks;
  std::vector<NetworkLink *> outLinks;
};

NS_OBJECT_ENSURE_REGISTERED (MinHopTdmaOptimizer);

TypeId MinHopTdmaOptimizer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::MinHopTdmaOptimizer")
    .SetParent<TdmaOptimizerBase> ()
    .AddConstructor<MinHopTdmaOptimizer> ()

    ;

  return tid;
}

MinHopTdmaOptimizer::MinHopTdmaOptimizer () : TdmaOptimizerBase()
{
  NS_LOG_FUNCTION (this);
  m_numNodes = 0;
}

MinHopTdmaOptimizer::~MinHopTdmaOptimizer ()
{
  NS_LOG_FUNCTION (this);

}

void MinHopTdmaOptimizer::SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel)
{
  NS_LOG_FUNCTION (this);

  // Setup the common base properties
  TdmaOptimizerBase::SetupOptimization(c, propModel);

}

vector< vector<int> > MinHopTdmaOptimizer::SolveTdma (void)
{
  NS_LOG_FUNCTION (this);

  // Solve for packet flows
  vector<int> hopCount(m_numNodes,9999999);
  hopCount[0] = 0;
  vector<double> curTxPwr(m_numNodes,1e300);
  vector< vector<int> > flows(m_numNodes);
  for(int i=0; i < m_numNodes; i++)
  	flows[i].assign(m_numNodes,0);

  BreadthFirstMinHopFlowSolver(0,flows,hopCount,curTxPwr);

	NS_LOG_DEBUG(" Flow matrix:");
	std::stringstream ss;

	for(int i=0; i < m_numNodes; i++){

		ss.str( std::string() );
		ss << "Node " << i << ": ";

		for(int j=0; j < m_numNodes; j++){

			// Determine number of packets per slot for each link.
			flows[i][j] = ceil((double)flows[i][j] / m_packetsPerSlot);

			if(flows[i][j])
				ss << j << "(" << flows[i][j] << "), ";
		}

		NS_LOG_DEBUG( ss.str() );
	}

  return flows;

}



int MinHopTdmaOptimizer::FindFirstParent(int i, vector< vector<int> > &packetFlows)
{
	// Determine which node that node i transmits to.
	int j;
	for(j=0; j < packetFlows[i].size() && !packetFlows[i][j]; j++);

	NS_ASSERT(j < packetFlows[i].size());

	return j;
}

int MinHopTdmaOptimizer::HasNotBeenProcessed(int i, vector< vector<int> > &packetFlows)
{
	// Node hasn't been processed yet if it's not connected to anything.
	int j;
	for(j=0; j < packetFlows[i].size() && !packetFlows[i][j]; j++);

	return (j == packetFlows[i].size());
}

void MinHopTdmaOptimizer::BreadthFirstMinHopFlowSolver(int parent, vector< vector<int> > &packetFlows, vector<int> &hopCount, vector<double> &curTxPwr)
{
	vector<int> nextNodeLayer;

	NS_LOG_DEBUG("Breadth First Flow Solver, Parent: " << parent);

	// Check for new links
	for(int nNode=0; nNode < m_numNodes; nNode++){


		if(m_txPowerDbm[parent][nNode] <= m_maxTxPowerDbm
				&& (hopCount[nNode] > hopCount[parent]+1
						|| (hopCount[nNode] == hopCount[parent]+1 && m_txPowerDbm[parent][nNode] < curTxPwr[nNode]) ) ){

			if(HasNotBeenProcessed(nNode,packetFlows)){
				nextNodeLayer.push_back(nNode);

				NS_LOG_DEBUG(" New route neighbour: " << nNode);

				// Update hops and transmit power for this node.
				hopCount[nNode] = hopCount[parent] + 1;
				curTxPwr[nNode] = m_txPowerDbm[parent][nNode];

				// Update the graph to reflect the new node addition.
				int i=nNode, j=parent;
				while(i!=0){
					packetFlows[i][j]++;
					i = j;
					if(j !=0 )
						j = FindFirstParent(i,packetFlows);
				}

			}
		}
	}

	// Perform the search for the next layer of nodes
	for(int iNext=0; iNext < nextNodeLayer.size(); iNext++)
		BreadthFirstMinHopFlowSolver(nextNodeLayer[iNext],packetFlows,hopCount,curTxPwr);


}


