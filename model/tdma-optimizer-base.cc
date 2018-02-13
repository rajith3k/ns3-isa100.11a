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

#include "ns3/tdma-optimizer-base.h"
#include "ns3/isa100-dl.h"
#include "ns3/isa100-net-device.h"
#include "ns3/isa100-battery.h"
#include "ns3/mobility-model.h"
#include "ns3/propagation-loss-model.h"

#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/log.h"
#include "ns3/boolean.h"

NS_LOG_COMPONENT_DEFINE ("TdmaOptimizerBase");

namespace ns3 {


NS_OBJECT_ENSURE_REGISTERED (TdmaOptimizerBase);

TypeId TdmaOptimizerBase::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TdmaOptimizerBase")
    .SetParent<Object> ()
    .AddConstructor<TdmaOptimizerBase> ()

    .AddAttribute ("SinkIndex", "The index of the sink node.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&TdmaOptimizerBase::m_sinkIndex),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NumMultiFrames", "The number of frames within a multi-frame superframe.",
                   UintegerValue(1),
                   MakeUintegerAccessor(&TdmaOptimizerBase::m_numMultiFrames),
                   MakeUintegerChecker<uint8_t>())
    .AddAttribute ("NumBytesPkt", "The number of bytes within a packet.",
                   UintegerValue(0),
                   MakeUintegerAccessor(&TdmaOptimizerBase::m_numBytesPkt),
                   MakeUintegerChecker<uint16_t>())
    .AddAttribute ("NumPktsNode", "The number of packets a node must send within a frame.",
                   UintegerValue(0),
                   MakeUintegerAccessor(&TdmaOptimizerBase::m_numPktsNode),
                   MakeUintegerChecker<uint8_t>())
    .AddAttribute ("MultiplePacketsPerSlot", "Whether multiple packets can be sent during a single timeslot.",
    		BooleanValue(false),
				MakeBooleanAccessor(&TdmaOptimizerBase::m_multiplePacketsPerSlot),
				MakeBooleanChecker())
   .AddAttribute ("PacketsPerSlot", "Number of packets sent per slot.",
  		 IntegerValue(1),
			 MakeIntegerAccessor(&TdmaOptimizerBase::m_packetsPerSlot),
			 MakeIntegerChecker<int32_t>(0,10000))


	 .AddAttribute ("SensitivityDbm",
			 "The sensitivity of the receiver (dBm)",
			 DoubleValue (-101.0), // Sensitivity for the RF233 is -101 dBm for 250kbit/s.
			 MakeDoubleAccessor (&TdmaOptimizerBase::m_rxSensitivityDbm),
			 MakeDoubleChecker<double>())




    ;

  return tid;
}

TdmaOptimizerBase::TdmaOptimizerBase ()
{
  NS_LOG_FUNCTION (this);

  m_numNodes = 0;
  m_isSetup = false;
  m_slotDuration = Seconds(0.0);
  m_numTimeslots = 0;
  m_currMultiFrame = 0;
  m_frameInitEnergiesJ.clear();
}

TdmaOptimizerBase::~TdmaOptimizerBase ()
{
  NS_LOG_FUNCTION (this);
}

std::vector< std::vector<int> > TdmaOptimizerBase::SolveTdma (void)
{
	NS_FATAL_ERROR("SolveTdma needs to be redefined in a derived class");

}


void TdmaOptimizerBase::SetupOptimization (NodeContainer c, Ptr<PropagationLossModel> propModel)
{
  NS_LOG_FUNCTION (this);

  m_numNodes = c.GetN();

  // Initialize some member variables needed for the optimization
  TimeValue slotDurationV;
  TimeValue txEarliestV;
  Ptr<Isa100NetDevice> devPtr = c.Get(1)->GetDevice(0)->GetObject<Isa100NetDevice>();
  devPtr->GetDl()->GetAttribute("SuperFrameSlotDuration", slotDurationV);
  devPtr->GetDl()->GetAttribute("TxEarliest", txEarliestV);
  m_slotDuration = slotDurationV.Get();
  m_usableSlotDuration = m_slotDuration - txEarliestV.Get();

  UintegerValue numSlotsV;
  devPtr->GetDl()->GetAttribute("SuperFramePeriod", numSlotsV);
  m_numTimeslots = numSlotsV.Get() / m_numMultiFrames;
  NS_ASSERT_MSG( numSlotsV.Get() % m_numMultiFrames == 0,
      "The number of timeslots in the super frame cannot evenly divide for each frame.");

  DoubleValue doubleV;
  devPtr->GetPhy()->GetAttribute("PhyBitRate", doubleV);
  m_bitRate = doubleV.Get();
  devPtr->GetPhy()->GetAttribute("SensitivityDbm", doubleV);
  m_minRxPowerDbm = doubleV.Get();
  devPtr->GetPhy()->GetAttribute("NoiseFloorDbm", doubleV);
  m_noiseFloorDbm = doubleV.Get();

  // Get initial energy at a node
  m_initialEnergy =  devPtr->GetBattery()->GetInitialEnergy();

  // Processor currents
  double procActiveCurr = devPtr->GetProcessor()->GetActiveCurrent();
  double procIdleCurr = devPtr->GetProcessor()->GetActiveCurrent();


  // Get a reference to the phy layer (all based on node 1)
  devPtr = c.Get(1)->GetDevice(0)->GetObject<Isa100NetDevice>();
  Ptr<ZigbeePhy> zigbeePhy = devPtr->GetPhy();

  // Calculate tx energies
  IntegerValue TxPowerV;
  devPtr->GetDl()->GetAttribute("MaxTxPowerDbm", TxPowerV);
  m_maxTxPowerDbm = TxPowerV.Get();

  devPtr->GetDl()->GetAttribute("MinTxPowerDbm", TxPowerV);
  double minTxPowerDbm = TxPowerV.Get();

  // Obtain all node locations
  std::vector<Ptr<MobilityModel> > positions;
  for (uint8_t i = 0; i < m_numNodes; i++)
  {
    Ptr<NetDevice> baseDevice = c.Get(i)->GetDevice(0);
    devPtr = baseDevice->GetObject<Isa100NetDevice>();
    positions.push_back(devPtr->GetPhy()->GetMobility());
  }

  // Iterate over all links
  for (uint16_t i = 0; i < m_numNodes; i++)
  {
    row_t txPowRw;
    row_t txEnergiesRwBit;
    row_t txEnergiesRwByte;

  	std::stringstream ss;
  	ss << "Node " << i << ": ";

    for (uint16_t j = 0; j < m_numNodes; j++)
    {
      double txEnergyBit = 0;
      double txEnergyByte = 0;
      double txPow = -999;

      // Only calculate bit energies for links between nodes
      if (i != j)
      {
        // Assuming no antenna gains
        double chnGainDbm = propModel->CalcRxPower(0, positions[i], positions[j]);
        txPow = ceil(m_minRxPowerDbm - chnGainDbm);

        if (txPow < minTxPowerDbm){
          txPow = minTxPowerDbm;
        }

    		ss << "(" << i << "->" << j << "," << txPow << "," << (m_minRxPowerDbm - chnGainDbm) << ") ";

        // Calculate the tx energy required per bit (uJ)

        txEnergyBit = (zigbeePhy->GetTrxCurrents()->GetBusyTxCurrentA(txPow) + procActiveCurr) *
            zigbeePhy->GetSupplyVoltage() / m_bitRate * 1e6;

        txEnergyByte = (zigbeePhy->GetTrxCurrents()->GetBusyTxCurrentA(txPow) + procActiveCurr) *
            zigbeePhy->GetSupplyVoltage() / m_bitRate * 8 * 1e6;

      }

      // Store tx energy and power for this tx/rx node pair
      txPowRw.push_back(txPow);
      txEnergiesRwBit.push_back(txEnergyBit);
      txEnergiesRwByte.push_back(txEnergyByte);
    }

    NS_LOG_DEBUG(ss.str());

    // Store all tx energies and powers for this tx node
    m_txPowerDbm.push_back(txPowRw);
    m_txEnergyBit.push_back(txEnergiesRwBit);
    m_txEnergyByte.push_back(txEnergiesRwByte);
  }

  // Calculate max tx energy per bit
  m_maxTxEnergyBit = (zigbeePhy->GetTrxCurrents()->GetBusyTxCurrentA(m_maxTxPowerDbm) + procActiveCurr) *
      zigbeePhy->GetSupplyVoltage() / m_bitRate * 1e6;

  // Calculate rx energy per bit
  m_rxEnergyBit = (zigbeePhy->GetTrxCurrents()->GetBusyRxCurrentA() + procActiveCurr) *
      zigbeePhy->GetSupplyVoltage() / m_bitRate * 1e6;


  // Calculate max tx energy per byte
  m_maxTxEnergyByte = (zigbeePhy->GetTrxCurrents()->GetBusyTxCurrentA(m_maxTxPowerDbm) + procActiveCurr) *
      zigbeePhy->GetSupplyVoltage() / m_bitRate * 8 * 1e6;

  // Calculate rx energy per byte
  m_rxEnergyByte = (zigbeePhy->GetTrxCurrents()->GetBusyRxCurrentA() + procActiveCurr) *
      zigbeePhy->GetSupplyVoltage() / m_bitRate * 8 * 1e6;

  // Get initial energy at a node
  m_initialEnergy = devPtr->GetBattery()->GetInitialEnergy();

}


} // namespace ns3
