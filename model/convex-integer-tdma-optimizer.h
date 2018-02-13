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


#ifndef CONVEX_INT_TDMA_OPTIMIZER
#define CONVEX_INT_TDMA_OPTIMIZER

#include "ns3/tdma-optimizer-base.h"

namespace ns3 {

/**
 * \class ConvexIntTdmaOptimizer
 *
 * \brief Convex Integer optimizer for a TDMA schedule
 *
 * This convex optimization is based on the Journal article:
 *   Cross-layer energy and delay optimization in small-scale sensor networks
 *       S. Cui, R. Madan, A. Goldsmith, et al.
 * in:
 *    IEEE Transactions on Wireless Communications 2007, vol 6, issue 10, pg 3688-3699
 *
 *  but has been modified to have the flow in units of packets (thus making the
 *  optimization an integer problem).
 */
class ConvexIntTdmaOptimizer : public TdmaOptimizerBase
{
public:

  static TypeId GetTypeId (void);

  ConvexIntTdmaOptimizer ();

  ~ConvexIntTdmaOptimizer ();

  /** Perform the initial calculations required by the optimization.
   *   @param c node container with all nodes
   *   @param propModel the propagation loss model
   */

  virtual void SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel);

  /** Solve the tdma optimization.
   * @return A matrix of timeslot flows between nodes.
   *
   */
  virtual std::vector< std::vector<int> > SolveTdma (void);

private:

  typedef std::vector<double> row_t;
  typedef std::vector<row_t> matrix_t;

};


}

#endif /* CONVEX_INT_TDMA_OPTIMIZER */
