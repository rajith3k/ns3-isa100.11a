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


#ifndef GOLDSMITH_TDMA_OPTIMIZER
#define GOLDSMITH_TDMA_OPTIMIZER

#include "ns3/tdma-optimizer-base.h"

namespace ns3 {

/**
 * \class GoldsmithTdmaOptimizer
 *
 * \brief A convex optimizer for a TDMA schedule based on literature.
 *
 * This convex optimization is directly based on the Journal article:
 *   Cross-layer energy and delay optimization in small-scale sensor networks
 *       S. Cui, R. Madan, A. Goldsmith, et al.
 * in:
 *    IEEE Transactions on Wireless Communications 2007, vol 6, issue 10, pg 3688-3699
 */
class GoldsmithTdmaOptimizer : public TdmaOptimizerBase
{
public:

  static TypeId GetTypeId (void);

  GoldsmithTdmaOptimizer ();

  ~GoldsmithTdmaOptimizer ();

  /** Setup the convex optimization with the required parameters
   *   @param c node container with all nodes
   *   @param propModel the propagation loss model
   */

  virtual void SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel);

  /** Solve the tdma optimization using a convex linear programming approach.
   * @return tdmaSchedule a tdma schedule (vector of vectors where the outer vector represents timeslots and
   *                      the inner vector is active links for that timeslot)
   */
  virtual std::vector< std::vector<int> > SolveTdma (void);

private:

  typedef std::vector<double> row_t;
  typedef std::vector<row_t> matrix_t;

};


}

#endif /* GOLDSMITH_TMDA_OPTIMIZER */
