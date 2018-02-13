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

#include "ns3/goldsmith-tdma-optimizer.h"

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

ILOSTLBEGIN

NS_LOG_COMPONENT_DEFINE ("GoldsmithTdmaOptimizer");

namespace ns3 {

struct nodeElement {
  std::vector<NetworkLink *> inLinks;
  std::vector<NetworkLink *> outLinks;
};

NS_OBJECT_ENSURE_REGISTERED (GoldsmithTdmaOptimizer);

TypeId GoldsmithTdmaOptimizer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::GoldsmithTdmaOptimizer")
    .SetParent<TdmaOptimizerBase> ()
    .AddConstructor<GoldsmithTdmaOptimizer> ()

    ;

  return tid;
}

GoldsmithTdmaOptimizer::GoldsmithTdmaOptimizer () : TdmaOptimizerBase()
{
  NS_LOG_FUNCTION (this);
}

GoldsmithTdmaOptimizer::~GoldsmithTdmaOptimizer ()
{
  NS_LOG_FUNCTION (this);
}

void GoldsmithTdmaOptimizer::SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel)
{
  NS_LOG_FUNCTION (this);

  // Setup the common base properties
  TdmaOptimizerBase::SetupOptimization(c, propModel);

  // Indicate that optimization has been setup
  m_isSetup = true;
}

vector< vector<int> > GoldsmithTdmaOptimizer::SolveTdma (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG(m_isSetup, "TDMA Optimizer: Must setup optimization before calling Solve!");

  typedef IloArray<IloNumVarArray> NumVarMatrix;

  IloEnv env;
  vector< vector<int> > flows(m_numNodes);

  try
  {
  	IloModel model (env);

  	// Variables for optimization
  	// Packet flows and max energy
  	NumVarMatrix bitFlowsVars (env, m_numNodes);
  	IloNumVar maxNodeEnergyVar (env, 0.0, m_initialEnergy, "MaxEnergy"); // in seconds

  	char flowName[16];

  	// Iterate through all combinations of nodes to create variables
  	for(IloInt i = 0; i < m_numNodes; i++){

  		// Initialize ith row of flows array
  		bitFlowsVars[i] = IloNumVarArray(env);

  		for(IloInt j = 0; j < m_numNodes; j++){

  			sprintf(flowName, "W_%li_%li", i, j);

  			// Node doesn't transmit to itself
  			if (i == j)
  			{
  				bitFlowsVars[i].add(IloNumVar(env, 0, 0, flowName));
  			}

  			// Node transmits to a different one
  			else
  			{
  				// Check if node is beyond range (cost to tx a bit is higher than allowed)
  				if (m_txEnergyBit[i][j] > m_maxTxEnergyBit)
  				{
  					bitFlowsVars[i].add(IloNumVar(env, 0, 0, flowName));
  				}

  				else
  				{
  					// Sink node does not transmit
  					if (i == m_sinkIndex)
  					{
  						bitFlowsVars[i].add(IloNumVar(env, 0, 0, flowName));
  					}

  					else
  					{
  						bitFlowsVars[i].add(IloNumVar(env, 0, IloIntMax, flowName));
  					}
  				}
  			}
  		}
  	}

  	// Create constraints
  	for (uint32_t i = 0; i < m_numNodes; i++)
  	{
  		IloExpr sumLinkTimes(env);
  		IloExpr sumFlowsOut(env);
  		IloExpr sumFlowsIn(env);
  		IloExpr sumEnergyTx(env);
  		IloExpr sumEnergyRx(env);

  		for (int j = 0; j < m_numNodes; j++)
  		{
  			// TDMA sum of assigned link times
  			sumLinkTimes += bitFlowsVars[i][j] / m_bitRate;

  			// Sum of flows
  			sumFlowsOut += bitFlowsVars[i][j];
  			sumFlowsIn  += bitFlowsVars[j][i];

  			// Energy
  			sumEnergyTx += m_txEnergyBit[i][j] * bitFlowsVars[i][j];
  			sumEnergyRx += m_rxEnergyBit * bitFlowsVars[j][i];
  		}

  		if (i != m_sinkIndex)
  		{
  			// TDMA constraint
  			model.add(sumLinkTimes <= m_usableSlotDuration.GetSeconds() * m_numTimeslots);

  			// conservation of flow
  			model.add (sumFlowsIn + m_numPktsNode * m_numBytesPkt * 8 == sumFlowsOut);

  			// conservation of energy
  			model.add (sumEnergyTx + sumEnergyRx <= m_initialEnergy);
  			model.add (sumEnergyTx + sumEnergyRx <= maxNodeEnergyVar);
  		}
  	}

  	// Specify objective (minimize the maximum node's energy per frame)
  	model.add (IloMinimize(env, maxNodeEnergyVar));

  	IloCplex cplex (model);


  	// Might be nice to figure out how to integrate this output with NS_LOG levels
  	// Exports the model. Can use LP, SAV or MPS format
  	cplex.exportModel("scratch/optmodel.lp");

  	//cplex.setParam(IloCplex::EpGap, 0.11);

  	// Disable output, would be nice to integrate with ns3 logging as well if possible
  	//cplex.setOut(env.getNullStream());

  	// use the dual simplex, otherwise let cplex choose
  	// cplex.setParam(IloCplex::RootAlg, IloCplex::Dual);

  	cplex.setParam(IloCplex::TiLim, 60*5); // Max optimization time (in sec)

  	// Solve the optimization
  	if (!cplex.solve ()) {
  		NS_FATAL_ERROR ("Failed to optimize LP: " << cplex.getStatus());
  	}

  	// Obtain results
  	IloNumArray flowVals (env);
  	IloNum objVal;
  	std::vector<NetworkLink> linkList;
  	std::vector<uint16_t> linksNumPkts;
  	double lifetimeResult;

  	NS_ASSERT_MSG(cplex.getStatus() == IloAlgorithm::Optimal, "Convex solver couldn't find optimal solution!");
  	objVal = cplex.getObjValue();
  	lifetimeResult = m_initialEnergy / objVal * m_slotDuration.GetSeconds() * m_numTimeslots;

  	NS_LOG_DEBUG (" Solution status = " << cplex.getStatus());
  	NS_LOG_DEBUG (" Solution value, Max Energy  = " << objVal);
  	NS_LOG_UNCOND (std::fixed << std::setprecision (2) << " Calculated lifetime value   = " << lifetimeResult);


  	for(int i=0; i < m_numNodes; i++)
  		flows[i].assign(m_numNodes,0);


  	std::stringstream ss;

  	for(int i=0; i < m_numNodes; i++) {

  		if(i != 0 ){

  			cplex.getValues (flowVals,bitFlowsVars[i]);
  			//   NS_LOG_DEBUG(" Packet Flow Values for Node " << i << "  =  " << flowVals);

  			ss.str( std::string() );
  			ss << "Node " << i << ": ";


  			for(int j=0; j < m_numNodes; j++){

  				int numPackets = ceil(flowVals[j] / (8*m_numBytesPkt));

  				if(flowVals[j] != 0)
  					ss << j << "(" << flowVals[j] << "," << numPackets << ",";

  				int numSlots = ceil((double)numPackets / m_packetsPerSlot);

  				flows[i][j] = numSlots;

  				if(flowVals[j] != 0)
  					ss << flows[i][j] << "), ";
  			}

  			NS_LOG_DEBUG(ss.str());
  		}
  	}


  }
  catch (IloAlgorithm::CannotExtractException& e) {
    NS_LOG_UNCOND ("CannotExtractExpection: " << e);
    IloExtractableArray failed = e.getExtractables();
    for (IloInt i = 0; i < failed.getSize(); i++){
      NS_LOG_UNCOND("\t" << failed[i]);
    }
    NS_FATAL_ERROR("Concert Fatal Error.");
  }
  catch (IloException& e) {
     NS_FATAL_ERROR ("Concert exception caught: " << e );
  }
  catch (...) {
     NS_FATAL_ERROR ("Unknown exception caught");
  }

  env.end ();

  return flows;
}

} // namespace ns3
