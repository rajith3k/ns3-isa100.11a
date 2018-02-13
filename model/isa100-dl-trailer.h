/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 The University of Calgary- FISHLAB
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
 * Author: Michael Herrmann <mjherrma@ucalgary.ca>
 *
 */


/**
 * \defgroup isa100-11a-dl-trailer
 *
 *  This trailer is used to tag on additional data at the end of a packet.
 *    - ex for the distributed routing to piggyback information on packets
 */

#ifndef ISA_100_DL_TRAILER_H
#define ISA_100_DL_TRAILER_H

#include "ns3/trailer.h"


namespace ns3 {

// Used to convert between signed and unsigned while preserving bit pattern
union oneByte_t
{
  int8_t sign;
  uint8_t unsign;
};

class Isa100DlTrailer : public Trailer
{

public:

  Isa100DlTrailer (void);

  virtual ~Isa100DlTrailer (void);

  static TypeId GetTypeId (void);

  TypeId GetInstanceTypeId (void) const;

  /** Calculate trailer size.
   *
   * \return Trailer size in bytes.
   */
  uint32_t GetSerializedSize (void) const;

  /** Write the trailer to a byte buffer.
   *
   * @param start Iterator for the buffer.
   */
  void Serialize (Buffer::Iterator start) const;

  /** Read trailer contents from a byte buffer.
   *
   * @param start Iterator for the buffer.
   * \return Size of the buffer.
   */
  uint32_t Deserialize (Buffer::Iterator start);

  /** Print trailer contents.
   */
  void Print (std::ostream &os) const;

  /** Get cost to sink from node tx'ing this packet
   *  @return distributed routing cost
   */
  uint32_t GetDistrRoutingCost (void) const;

  /** Set the cost to sink from node tx'ing this packet
   *  @param cost Distributed routing cost
   */
  void SetDistrRoutingCost (uint32_t cost);

  /** Get the remaining energy of the node tx'ing this packet
   *   @return Node's remaining energy
   */
  uint32_t GetDistrRoutingEnergy (void) const;

  /** Set the remaining energy of the node tx'ing this packet
   *   @param energy Node's remaining energy
   */
  void SetDistrRoutingEnergy (uint32_t energy);

  /** Get the tx power with which this packet was sent
   *   @return The tx power
   */
  int8_t GetDistrRoutingTxPower (void) const;

  /** Set the tx power with which this packet will be sent
   *    @param txPower The tx power
   */
  void SetDistrRoutingTxPower (int8_t txPower);

  /**}@*/

private:

  // Data required for distributed routing
  uint32_t m_cost;                    ///< Cost to transmit to the sink from the node transmitting this packet
  uint32_t m_remainingJoules;         ///< The remaining energy of the node transmitting this packet
  oneByte_t m_txPowDbm;               ///< The tx power with which this packet was sent

};
}; // namespace ns-3
#endif


