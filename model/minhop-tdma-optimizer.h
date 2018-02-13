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
 * Author:   Geoffrey Messier <gmessier@ucalgary.ca>
 */


#ifndef MINHOP_TDMA_OPTIMIZER
#define MINHOP_TDMA_OPTIMIZER

#include "ns3/tdma-optimizer-base.h"

using namespace ns3;
using namespace std;

/**
 * \class MinHopTdmaOptimizer
 *
 * \brief A minimum hop TDMA scheduler.
 *
 * This class is based on a variant of the breadth first minimum hop search algorithm.
 *
 */
class MinHopTdmaOptimizer : public TdmaOptimizerBase
{
public:

  static TypeId GetTypeId (void);

  MinHopTdmaOptimizer ();

  ~MinHopTdmaOptimizer ();

  /** Perform some initial calculations required by the optimizer.
   *   @param c node container with all nodes
   *   @param propModel the propagation loss model
   */

  void SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel);

  /** Solve for the packet flows using a minimum hop breadth first search algorithm.
   * @return packetFlows A matrix of packet flows between nodes.
   */
  virtual std::vector< std::vector< int > > SolveTdma (void);

private:

  /** Finds the parent of node i with the lowest index.
   *
   * @param i Node index.
   * @param packetFlows Matrix of packet flows.
   */
  int FindFirstParent(int i, vector< vector<int> > &packetFlows);

  /** Determines if a node has been processed by the min hop algorithm as a potential parent.
   *
   * @param i Node index.
   * @param packetFlows Matrix of packet flows.
   */
  int HasNotBeenProcessed(int i, vector< vector<int> > &packetFlows);

  /** Perform a breadth first search to find a minimum number of hops network.
   *
   * @param parent Index of current parent.
   * @param packetFlows Vector of packet flows.
   * @param hopCount Vector indicating number of hops each node is from sink.
   * @param curTxPwr Current transmit power for each node to reach next hop.
   */
  void BreadthFirstMinHopFlowSolver(int parent, vector< vector<int> > &packetFlows, vector<int> &hopCount, vector<double> &curTxPwr);




};



#endif /* MINHOP_TMDA_OPTIMIZER */
