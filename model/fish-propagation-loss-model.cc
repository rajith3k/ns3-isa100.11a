/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006,2007 INRIA
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
 */

#include <cmath>
#include "fish-propagation-loss-model.h"

#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FishPropagationLossModel");



// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FishFixedLossModel);

TypeId
FishFixedLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FishFixedLossModel")
    .SetParent<PropagationLossModel> ()
    .SetGroupName ("Propagation")
    .AddConstructor<FishFixedLossModel> ()
    .AddAttribute ("Loss", "The loss value in dB.",
                   DoubleValue (100.0),
                   MakeDoubleAccessor (&FishFixedLossModel::m_loss),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}
FishFixedLossModel::FishFixedLossModel ()
  : PropagationLossModel ()
{
	m_loss = 100;
}

FishFixedLossModel::~FishFixedLossModel ()
{
}

void
FishFixedLossModel::SetLoss (double loss)
{
  m_loss = loss;
}

double
FishFixedLossModel::DoCalcRxPower (double txPowerDbm,
                                  Ptr<MobilityModel> a,
                                  Ptr<MobilityModel> b) const
{
  return txPowerDbm - m_loss;
}

int64_t
FishFixedLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FishCustomLossModel);

TypeId
FishCustomLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FishCustomLossModel")
    .SetParent<PropagationLossModel> ()
    .SetGroupName ("Propagation")
    .AddConstructor<FishCustomLossModel> ()
  ;
  return tid;

}

FishCustomLossModel::FishCustomLossModel ()
{
  m_lookupTableDb = 0;
}

FishCustomLossModel::FishCustomLossModel (std::vector<Vector> mapPosToIndex, double **lookupTable)
{
  m_mapPosToIndex = mapPosToIndex;
  uint16_t numNodes = m_mapPosToIndex.size();

  m_lookupTableDb = new double*[numNodes];
  for(uint16_t i = 0; i < numNodes; i++)
  {
    m_lookupTableDb[i] = new double[numNodes];
    memcpy(m_lookupTableDb[i], lookupTable[i], numNodes * sizeof(double));
  }
}

FishCustomLossModel::~FishCustomLossModel ()
{
  for(uint16_t i = 0; i < m_mapPosToIndex.size(); i++){
    delete[] m_lookupTableDb[i];
  }
}


double
FishCustomLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  if (m_lookupTableDb == 0){
    NS_FATAL_ERROR("CustomPropogationLossModel: Lookup table was not initialized!");
  }

  // Have to determine the indices from the positions
  uint16_t indexA = 0xFFFF;
  uint16_t indexB = 0xFFFF;
  std::vector<Vector>::const_iterator iter;

  // Find indices
  for (iter = m_mapPosToIndex.begin(); iter != m_mapPosToIndex.end(); ++iter)
  {
    if (CalculateDistance(*iter, a->GetPosition()) == 0)
    {
      indexA = iter - m_mapPosToIndex.begin();
    }

    if (CalculateDistance(*iter, b->GetPosition()) == 0)
    {
      indexB = iter - m_mapPosToIndex.begin();
    }
  }

  // Check if valid indices were found
  if (indexA == 0xFFFF || indexB == 0xFFFF){
    NS_FATAL_ERROR("CustomPropogationLossModel: Valid indices used for lookup could not be found!");
  }

  // Look up path loss
  double pathlossDb = m_lookupTableDb[indexA][indexB];

  // Calculate and return receive power
  return txPowerDbm - pathlossDb;
}

int64_t
FishCustomLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

// ------------------------------------------------------------------------- //

NS_OBJECT_ENSURE_REGISTERED (FishLogDistanceLossModel);

TypeId
FishLogDistanceLossModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::FishLogDistanceLossModel")
    .SetParent<PropagationLossModel> ()
    .SetGroupName ("Propagation")
    .AddConstructor<FishLogDistanceLossModel> ()
    .AddAttribute ("PathLossExponent",
                   "The exponent of the Path Loss propagation model",
                   DoubleValue (3.0),
                   MakeDoubleAccessor (&FishLogDistanceLossModel::m_exponent),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ShadowingStdDev",
                   "The standard deviation of the shadowing",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&FishLogDistanceLossModel::m_shadowingStD),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceDistance",
                   "The distance at which the reference loss is calculated (m)",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&FishLogDistanceLossModel::m_referenceDistance),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ReferenceLoss",
                   "The reference loss at reference distance (dB). (Default is Friis at 1m with 2.44 GHz)",
                   DoubleValue (40.1956),
                   MakeDoubleAccessor (&FishLogDistanceLossModel::m_referenceLoss),
                   MakeDoubleChecker<double> ())

		.AddAttribute ("IsStationaryNetwork",
								   "Indicates whether new shadowing values should be generated for each call due to node mobility.",
								   BooleanValue (true),
								   MakeBooleanAccessor (&FishLogDistanceLossModel::m_isStationary),
								   MakeBooleanChecker())
;
  return tid;

}

FishLogDistanceLossModel::FishLogDistanceLossModel ()
{
  m_normDist = CreateObject<NormalRandomVariable> ();
  m_normDist->SetAttribute("Mean", DoubleValue (0.0));
  m_isStationary = false;
}

FishLogDistanceLossModel::FishLogDistanceLossModel (Ptr<ListPositionAllocator> positionAlloc, uint16_t numNodes, double shadowingStd)
{
	GenerateNewShadowingValues(positionAlloc,numNodes,shadowingStd);
}

void FishLogDistanceLossModel::GenerateNewShadowingValues (Ptr<ListPositionAllocator> positionAlloc, uint16_t numNodes, double shadowingStd)
{
	NS_LOG_FUNCTION(this);

  m_normDist = CreateObject<NormalRandomVariable> ();
  m_normDist->SetAttribute("Mean", DoubleValue (0.0));
  m_shadowingStD = shadowingStd;
  m_isStationary = true;

  // Create position vector to index map
  std::vector<Vector> posVect;
  for (uint16_t i = 0; i < numNodes; i++)
  {
    posVect.push_back(positionAlloc->GetNext());
  }

  m_shadowingLookup.clear();

  // Generate the lookup map for shadowing values
  for (uint16_t i = 0; i < numNodes; i++)
  {
    for (uint16_t j = i + 1; j < numNodes; j++)
    {
      // Create lookup keys (fading is the same for both directions of a link)
      std::stringstream key1, key2;
      key1 << (int32_t)posVect[i].x << (int32_t)posVect[i].y << (int32_t)posVect[i].z
          << (int32_t)posVect[j].x << (int32_t)posVect[j].y << (int32_t)posVect[j].z;
      key2 << (int32_t)posVect[j].x << (int32_t)posVect[j].y << (int32_t)posVect[j].z
          << (int32_t)posVect[i].x << (int32_t)posVect[i].y << (int32_t)posVect[i].z;

      // Shadowing Value
      double shadowingDb = m_normDist->GetValue();

      NS_LOG_DEBUG(" " << key1.str() << " to " << key2.str() << ": " << shadowingDb << "dB");

      // Record values
      m_shadowingLookup[key1.str()] = shadowingDb;
      m_shadowingLookup[key2.str()] = shadowingDb;
    }
  }
}





double
FishLogDistanceLossModel::DoCalcRxPower (double txPowerDbm,
                                                Ptr<MobilityModel> a,
                                                Ptr<MobilityModel> b) const
{
  double distance = a->GetDistanceFrom (b);
  if (distance <= m_referenceDistance)
    {
      return txPowerDbm;
    }
  /**
   * The formula is:
   * rx = 10 * log (Pr0(tx)) - n * 10 * log (d/d0)
   *
   * Pr0: rx power at reference distance d0 (W)
   * d0: reference distance: 1.0 (m)
   * d: distance (m)
   * tx: tx power (dB)
   * rx: dB
   *
   * Which, in our case is:
   *
   * rx = rx0(tx) - 10 * n * log (d/d0)
   */
  double pathLossDb = 10 * m_exponent * std::log10 (distance / m_referenceDistance);

  // Determine shadowing. If stationary network keep using the same value.
  double shadowingDb;
  if (m_isStationary)
  {
    std::stringstream key;
    key << (int32_t)a->GetPosition().x << (int32_t)a->GetPosition().y << (int32_t)a->GetPosition().z
        << (int32_t)b->GetPosition().x << (int32_t)b->GetPosition().y << (int32_t)b->GetPosition().z;


    std::map<std::string, double>::const_iterator it = m_shadowingLookup.find(key.str());
    NS_ASSERT_MSG(it != m_shadowingLookup.end(), "Prop Model could not find shadowing value!");
    shadowingDb = it->second;
  }

  else {
    shadowingDb = m_normDist->GetValue();
  }


  double rxc = -m_referenceLoss - pathLossDb + shadowingDb;
  NS_LOG_DEBUG ("distance="<<distance<<"m, reference-attenuation="<< -m_referenceLoss<<"dB, "<<
                "attenuation coefficient="<<rxc<<"db, txPower: " << txPowerDbm << "dBm, rxPower: " << txPowerDbm+rxc << "dBm");

  return txPowerDbm + rxc;
}

int64_t
FishLogDistanceLossModel::DoAssignStreams (int64_t stream)
{
  return 0;
}

}
