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
 * Authors:
 * Geoffrey Messier <gmessier@ucalgary.ca>
 * Michael Herrmann <mjherrma@ucalgary.ca>
 */


#ifndef ISA100_ROUTING_H
#define ISA100_ROUTING_H

#include "ns3/object.h"
#include "ns3/mac16-address.h"

namespace ns3 {
class NodeContainer;
class Packet;
class Isa100NetDevice;
class Isa100DlHeader;


class Isa100RoutingAlgorithm : public Object{
public:

  static TypeId GetTypeId (void);

  Isa100RoutingAlgorithm();

  virtual ~Isa100RoutingAlgorithm();

  /** Populate header at source with any information required by routing algorithm.
   *
   * @param header The header object to be worked on.
   */
  virtual void PrepTxPacketHeader(Isa100DlHeader &header) = 0;

  /** Process a received packet to determine if it needs to be forwarded.
   * - If it does need to be forwarded on, this function will also make the
   *   necessary modifications to the packet header.
   *
   * @param packet Pointer to the received packet.
   * @param forwardPacketOn Reference to boolean that is true if the packet must be sent onward.
   */
  virtual void ProcessRxPacket(Ptr<Packet> packet, bool &forwardPacketOn) = 0;

  /** Determine if it's possible to attempt another link, given that a transmission has failed
   *   - This function should be overridden if used
   *
   * @param destInd The index of the final destination node
   * @param attempedLinks A list of already attempted links
   *
   * @return The base function always returns ff:ff
   */
  virtual Mac16Address AttemptAnotherLink(uint8_t destInd, std::vector<Mac16Address> attemptedLinks);


protected:

	Mac16Address m_address; ///< Address of this node.

};

class Isa100SourceRoutingAlgorithm : public Isa100RoutingAlgorithm
{
public:

  static TypeId GetTypeId (void);

  Isa100SourceRoutingAlgorithm();

  /** Constructor.
   *
   * @param initNumDests The number of destinations the node can reach using source routing.
   * @param initTable Array of strings containing the multi-hop paths to reach each destination.  Each address is a string in XX:XX format.
   */
  Isa100SourceRoutingAlgorithm(uint32_t initNumDests, std::string *initTable);

  ~Isa100SourceRoutingAlgorithm();

  /** Populate header at source with any information required by routing algorithm.
   * - Only works if the header contains a valid destination address.
   *
   * @param header The header object to be worked on.
   */
  void PrepTxPacketHeader(Isa100DlHeader &header);

  /** Process a received packet to determine if it needs to be forwarded.
   * - If it does need to be forwarded on, this function will also make the
   *   necessary modifications to the packet header.
   *
   * @param packet Pointer to the received packet.
   * @param forwardPacketOn Reference to boolean that is true if the packet must be sent onward.
   */
  void ProcessRxPacket(Ptr<Packet> packet, bool &forwardPacketOn);

private:

  Mac16Address **m_table;
  uint32_t m_numDests;
  uint32_t *m_numHops;

};


}

#endif /* ISA100_ROUTING_H */
