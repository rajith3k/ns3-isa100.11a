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


#ifndef TDMA_OPTIMIZER_BASE_H
#define TDMA_OPTIMIZER_BASE_H

#include "ns3/nstime.h"
#include "ns3/node-container.h"

typedef std::vector<double> row_t;
typedef std::vector<row_t> matrix_t;


namespace ns3 {
class PropagationLossModel;

struct NetworkLink {
  uint8_t txNode;
  uint8_t rxNode;
  double txPowerDbm;
  uint16_t numPkts;
};

typedef enum
{
  TDMA_MIN_HOP = 0,
  TDMA_GOLDSMITH = 1,
  TDMA_CONVEX_INT = 2,
  TDMA_CONVEX_SLOT_C = 3,
} OptimizerSelect;

typedef std::vector<std::vector<NetworkLink> > tdmaSchedule;

class TdmaOptimizerBase : public Object
{
public:

  static TypeId GetTypeId (void);

  TdmaOptimizerBase ();

  ~TdmaOptimizerBase ();

  /** Perform some initial calculations that are required by all derived class optimizers.
   * @param c the node container for all networks nodes. Some assumptions:
   *            - The sink node is the first node in the container
   *            - All nodes are using the same phy configuration
   * @param propModel the ns3 propagation loss model
   */
  virtual void SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel);

  /** Pure virtual function that triggers the optimizer solution of the routing problem.
   * @return flowMatrix A matrix of packet flows between nodes.
   */
  virtual std::vector< std::vector<int> > SolveTdma (void) ;


protected:

  uint16_t m_numNodes;        ///< Number of nodes in the network (including sink)
  Time m_slotDuration;        ///< Duration of a timeslot
  Time m_usableSlotDuration;  ///< Duration of the usable tx portion of a timeslot
  uint16_t m_numTimeslots;    ///< Total number of timeslots within a TDMA frame
  bool m_isSetup;             ///< Indicates if the optimization has been setup
  uint8_t m_currMultiFrame;   ///< The frame in a multi-frame sf which is being currently solved
  std::vector<double> m_frameInitEnergiesJ; ///< The initial energy of the current multi-frame
  double m_bitRate;           ///< The bit rate used in transmission
  double m_minRxPowerDbm;     ///< Minimum rx power required for communication to occur (dBm)
  double m_noiseFloorDbm;     ///< Noise floor (dBm), signals below this are insignificant/non-interfering
  double m_initialEnergy;   ///< The initial energy a node has (Joules).
  int m_packetsPerSlot; ///< Number of packets that can be transmitted per timeslot.
  double m_maxTxPowerDbm; ///< Maximum transmit power (dBm).


  // Attributes
  uint16_t m_sinkIndex;       ///< The index of the sink node.
  uint8_t m_numMultiFrames;  ///< Number of unique frames within a superframe
  uint16_t m_numBytesPkt;    ///< The number of bytes per packet (whole packet, not just payload)
  uint8_t m_numPktsNode;     ///< Number of packets each sensor node must send within a frame
  bool m_multiplePacketsPerSlot;   ///< Indicates if the node can transmit multiple packets per slot.
  double m_rxSensitivityDbm; ///< Receiver sensitivity (dBm).

  matrix_t m_txEnergyBit;  ///< A matrix[i][j] for the tx energy per bit (Joules/bit) for each link (i->j).
  matrix_t m_txPowerDbm;    ///< A matrix[i][j] for the tx power required to transmit on each link (i->j).
  double m_maxTxEnergyBit; ///< The maximum energy which can be used to transmit one bit (Joules/bit)
  double m_rxEnergyBit;    ///< The amount of energy to receive one bit (Joules/bit).

  matrix_t m_txEnergyByte;  ///< A matrix[i][j] for the tx energy per byte (Joules/byte) for each link (i->j).
  double m_maxTxEnergyByte; ///< The maximum energy which can be used to transmit one byte (Joules/byte)
  double m_rxEnergyByte;    ///< The amount of energy to receive one byte (Joules/byte).


};


}

#endif /* TDMA_OPTIMIZER_BASE_H */
