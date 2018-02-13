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

#include "ns3/convex-integer-tdma-optimizer.h"

#include "ns3/boolean.h"
#include "ns3/integer.h"
#include "ns3/log.h"

#include "ns3/zigbee-trx-current-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/mobility-model.h"
#include "ns3/isa100-net-device.h"
#include "ns3/isa100-dl.h"
#include "ns3/isa100-processor.h"

#include <ilcplex/ilocplex.h>
#include <algorithm>

ILOSTLBEGIN

NS_LOG_COMPONENT_DEFINE ("ConvexIntTdmaOptimizer");

namespace ns3 {

struct nodeElement {
  std::vector<NetworkLink *> inLinks;
  std::vector<NetworkLink *> outLinks;
};

NS_OBJECT_ENSURE_REGISTERED (ConvexIntTdmaOptimizer);

TypeId ConvexIntTdmaOptimizer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ConvexIntTdmaOptimizer")
    .SetParent<TdmaOptimizerBase> ()
    .AddConstructor<ConvexIntTdmaOptimizer> ()

    ;

  return tid;
}

ConvexIntTdmaOptimizer::ConvexIntTdmaOptimizer () : TdmaOptimizerBase()
{
  NS_LOG_FUNCTION (this);
}

ConvexIntTdmaOptimizer::~ConvexIntTdmaOptimizer ()
{
  NS_LOG_FUNCTION (this);
}

void ConvexIntTdmaOptimizer::SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel)
{
  NS_LOG_FUNCTION (this);

  // Setup the common base properties
  TdmaOptimizerBase::SetupOptimization(c, propModel);


  // Indicate that optimization has been setup
  m_isSetup = true;
}

std::vector< std::vector<int> > ConvexIntTdmaOptimizer::SolveTdma (void)
{
  NS_LOG_FUNCTION (this);
  NS_ASSERT_MSG(m_isSetup, "TDMA Optimizer: Must setup optimization before calling Solve!");

  typedef IloArray<IloIntVarArray> IntVarMatrix;

  vector< vector<int> > flows(m_numNodes);

  // Note: For now, hard code the optimizer to only use one multi-frame.  Will need to expand the scheduler to change this.
  // - The multi-frame version solves the next frame with initial energies updated to reflect how much energy was used up
  //   in the first frame.
  m_numMultiFrames = 1;
  NS_LOG_DEBUG("** Hard coded to 1 multiframe.");


  // Solve a schedule for 'numMultiFrames' frames
  for (m_currMultiFrame = 0; m_currMultiFrame < m_numMultiFrames; m_currMultiFrame++)
  {
    NS_LOG_UNCOND("---------------- Solving Frame " << (uint16_t)m_currMultiFrame << " ----------------");

    tdmaSchedule currFrameSchd;

    // Determine initial energies if solving frame 0 (otherwise they are updated at the end of the previous)
    if (m_currMultiFrame == 0)
    {
      for (uint8_t i = 0; i < m_numNodes; i ++){
        m_frameInitEnergiesJ.push_back(m_initialEnergy);
      }
    }

    IloEnv env;
    try
    {
      IloModel model (env);

      // Variables for optimization
      // Packet flows and max energy
      IntVarMatrix pktFlowsVars (env, m_numNodes);
      IntVarMatrix numSlotsVars (env, m_numNodes);
      IloNumVar lifetimeInvVar (env, 0.0, IloInfinity, "1_div_Lifetime"); // in seconds
      IloNumVarArray nodeEnergies (env);

      char flowName[16];
      char linkName[16];
      char nodeE[16];

      // Iterate through all combinations of nodes to create variables
      for(IloInt i = 0; i < m_numNodes; i++){

        // Initialize node energy consumption variable
        sprintf(nodeE, "E_used_%li", i);
        nodeEnergies.add(IloNumVar(env, 0, m_frameInitEnergiesJ[i], nodeE));

        // Initialize ith row of flows array
        pktFlowsVars[i] = IloIntVarArray(env);
        numSlotsVars[i] = IloIntVarArray(env);

        for(IloInt j = 0; j < m_numNodes; j++){

          sprintf(flowName, "W_%li_%li", i, j);
          sprintf(linkName, "L_%li_%li", i, j);

          // Node doesn't transmit to itself
          if (i == j)
          {
            pktFlowsVars[i].add(IloIntVar(env, 0, 0, flowName));
            numSlotsVars[i].add(IloIntVar(env, 0, 0, linkName));
          }

          // Node transmits to a different one
          else
          {
            // Check if node is beyond range (cost to tx a bit is higher than allowed)
            if (m_txEnergyByte[i][j] > m_maxTxEnergyByte)
            {
              pktFlowsVars[i].add(IloIntVar(env, 0, 0, flowName));
              numSlotsVars[i].add(IloIntVar(env, 0, 0, linkName));
            }

            else
            {
              // Sink node does not transmit
              if (i == m_sinkIndex)
              {
                pktFlowsVars[i].add(IloIntVar(env, 0, 0, flowName));
                numSlotsVars[i].add(IloIntVar(env, 0, 0, linkName));
              }

              else
              {
                pktFlowsVars[i].add(IloIntVar(env, 0, IloIntMax, flowName));
                numSlotsVars[i].add(IloIntVar(env, 0, IloIntMax, linkName));
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
        IloExpr sumEnergyIdle(env);

        for (int j = 0; j < m_numNodes; j++)
        {
          // TDMA sum of assigned link times
          sumLinkTimes += pktFlowsVars[i][j] * m_numBytesPkt * 8 / m_bitRate;

          // Sum of flows
          sumFlowsOut += pktFlowsVars[i][j];
          sumFlowsIn  += pktFlowsVars[j][i];

          // Energy
          sumEnergyTx += m_txEnergyByte[i][j] * pktFlowsVars[i][j] * m_numBytesPkt;
          sumEnergyRx += m_rxEnergyByte * pktFlowsVars[j][i] * m_numBytesPkt;

        }

        // TDMA constraint
        model.add(sumLinkTimes <= m_usableSlotDuration.GetSeconds() * m_numTimeslots);

        if (i != m_sinkIndex)
        {
          // conservation of flow constraint
          model.add (sumFlowsIn + m_numPktsNode == sumFlowsOut);

          // conservation of energy
          model.add (sumEnergyTx + sumEnergyRx + sumEnergyIdle == nodeEnergies[i]);

          // max inverse lifetime constraint
          model.add (nodeEnergies[i] / (m_frameInitEnergiesJ[i] * m_slotDuration.GetSeconds() * m_numTimeslots) <= lifetimeInvVar);
        }
      }

      // Specify objective (minimize the maximum lifetime inverse out of all nodes)
      model.add (IloMinimize(env, lifetimeInvVar));

      IloCplex cplex (model);

      // Disable output, might be nice to integrate with ns3 logging as well if possible
      cplex.setOut(env.getNullStream());

      // Might be nice to figure out how to integrate this output with NS_LOG levels
      // Exports the model. Can use LP, SAV or MPS format

      cplex.setParam(IloCplex::EpGap, 0.01);   // Integer gap tolerance
      cplex.setParam(IloCplex::MIPDisplay, 2); // Display level output
      cplex.setParam(IloCplex::TiLim, 60*5); // Max optimization time (in sec)

      // Solve the optimization
      if (!cplex.solve ()) {
        NS_FATAL_ERROR ("Failed to optimize LP: " << cplex.getStatus());
      }

      // Obtain results
      IloNumArray flowVals (env);
      IloNumArray linkVals (env);
      IloNum objVal;
      std::vector<NetworkLink> linkList;
      std::vector<uint16_t> linksNumPkts;
      double lifetimeResult;

      NS_ASSERT_MSG(cplex.getStatus() == IloAlgorithm::Optimal, "Convex solver couldn't find optimal solution!");
      objVal = cplex.getObjValue();
      lifetimeResult = 1 / objVal;

      NS_LOG_DEBUG (" Solution status = " << cplex.getStatus());
      NS_LOG_DEBUG (" Solution value, Lifetime Inverse  = " << objVal);
      NS_LOG_UNCOND (std::fixed << std::setprecision (2) << " Calculated lifetime value   = " << lifetimeResult);

      for(int i=0; i < m_numNodes; i++)
      	flows[i].assign(m_numNodes,0);


      for(int i=0; i < m_numNodes; i++) {

      	if(i != 0 ){

      		cplex.getValues (flowVals,pktFlowsVars[i]);
      //    NS_LOG_DEBUG(" Packet Flow Values for Node " << i << "  =  " << flowVals);

          /* These lines would be used to update initial energy for the next frame optimization.
           *  IloNum usedEnergy = cplex.getValue (nodeEnergies[i]) / energyScale;
           *  m_frameInitEnergiesJ[i] -= usedEnergy;
           */

      		for(int j=0; j < m_numNodes; j++)
      			flows[i][j] += ceil((double)flowVals[j] / m_packetsPerSlot);
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
  }

	NS_LOG_DEBUG(" Flow matrix:");
	std::stringstream ss;

	for(int i=0; i < m_numNodes; i++){

		ss.str( std::string() );
		ss << "Node " << i << ": ";

		for(int j=0; j < m_numNodes; j++){

			if(flows[i][j] !=0)
				ss << j << "(" << flows[i][j] << ",";

			// Determine number of packets per slot for each link.
			flows[i][j] = ceil((double)flows[i][j] / m_packetsPerSlot);

			if(flows[i][j] != 0)
				ss << flows[i][j] << "), ";
		}

		NS_LOG_DEBUG( ss.str() );
	}



  return flows;
}

} // namespace ns3
