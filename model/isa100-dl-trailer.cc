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
 * Author:   Michael Herrmann <mjherrma@ucalgary.ca>
 */

#include "ns3/isa100-dl-trailer.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Isa100DlTrailer);


Isa100DlTrailer::Isa100DlTrailer ()
{
  m_cost = std::numeric_limits<uint32_t>::max();
  m_remainingJoules = std::numeric_limits<uint32_t>::max();
  m_txPowDbm.sign = std::numeric_limits<int8_t>::min();
}

Isa100DlTrailer::~Isa100DlTrailer ()
{
}

TypeId Isa100DlTrailer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100DlTrailer")
    .SetParent<Trailer> ()
    .AddConstructor<Isa100DlTrailer> ();
  return tid;
}


TypeId Isa100DlTrailer::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void Isa100DlTrailer::Print (std::ostream &os) const
{
  os << "Cost = " << m_cost;
  os << " Resdl_J = " << m_remainingJoules;
  os << " TxPow = " << (int16_t)m_txPowDbm.sign;
}

uint32_t Isa100DlTrailer::GetSerializedSize (void) const
{
  /*
   * Each mac trailer will have
   * cost                : 4 octet
   * remaining joules    : 4 Octet
   * tx power            : 1 Octet
   */

  uint32_t size = 9;  // distributed routing data

  return (size);
}


void Isa100DlTrailer::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;
  i.Prev (GetSerializedSize());

  // Distributed Routing data
  i.WriteHtolsbU32 (m_cost);
  i.WriteHtolsbU32 (m_remainingJoules);
  i.WriteU8 (m_txPowDbm.unsign);
}


uint32_t Isa100DlTrailer::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  i.Prev (GetSerializedSize());

  // Distributed Routing data
  m_cost = i.ReadLsbtohU32();
  m_remainingJoules = i.ReadLsbtohU32();
  m_txPowDbm.unsign = i.ReadU8();

  return GetSerializedSize();
}

uint32_t Isa100DlTrailer::GetDistrRoutingCost (void) const
{
  return m_cost;
}

void Isa100DlTrailer::SetDistrRoutingCost (uint32_t cost)
{
  m_cost = cost;
}

uint32_t Isa100DlTrailer::GetDistrRoutingEnergy (void) const
{
  return m_remainingJoules;
}

void Isa100DlTrailer::SetDistrRoutingEnergy (uint32_t energy)
{
  m_remainingJoules = energy;
}

int8_t Isa100DlTrailer::GetDistrRoutingTxPower (void) const
{
  return m_txPowDbm.sign;
}

void Isa100DlTrailer::SetDistrRoutingTxPower (int8_t txPower)
{
  m_txPowDbm.sign = txPower;
}
}
