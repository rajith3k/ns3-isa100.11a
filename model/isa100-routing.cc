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
 * Authors: Geoffrey Messier <gmessier@ucalgary.ca>
 *          Michael Herrmann <mjherrma@ucalgary.ca>
 */

#include "ns3/isa100-routing.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"
#include "ns3/packet.h"
#include "ns3/node-container.h"

#include "ns3/isa100-dl-header.h"
#include "ns3/isa100-dl-trailer.h"
#include "ns3/isa100-net-device.h"
#include "ns3/isa100-dl.h"
#include "ns3/zigbee-trx-current-model.h"
#include "ns3/isa100-battery.h"

#include <iomanip>

NS_LOG_COMPONENT_DEFINE ("Isa100Routing");

namespace ns3 {


// --- Isa100RoutingAlgorithm Base Class ---

NS_OBJECT_ENSURE_REGISTERED (Isa100RoutingAlgorithm);

TypeId Isa100RoutingAlgorithm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100RoutingAlgorithm")
    .SetParent<Object> ()

    .AddAttribute ("Address","16 bit DL address of node.",
        Mac16AddressValue(),
        MakeMac16AddressAccessor(&Isa100RoutingAlgorithm::m_address),
        MakeMac16AddressChecker())


    ;

  return tid;
}


Isa100RoutingAlgorithm::Isa100RoutingAlgorithm()
{
  ;
}

Isa100RoutingAlgorithm::~Isa100RoutingAlgorithm()
{
  ;
}

Mac16Address
Isa100RoutingAlgorithm::AttemptAnotherLink(uint8_t destInd, std::vector<Mac16Address> attemptedLinks)
{
  return Mac16Address("ff:ff");
}

NS_OBJECT_ENSURE_REGISTERED (Isa100SourceRoutingAlgorithm);

// --- Isa100SourceRoutingAlgorithm ---
TypeId Isa100SourceRoutingAlgorithm::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100SourceRoutingAlgorithm")
    .SetParent<Isa100RoutingAlgorithm> ()
    .AddConstructor<Isa100SourceRoutingAlgorithm> ()

    ;

  return tid;
}



Isa100SourceRoutingAlgorithm::Isa100SourceRoutingAlgorithm(uint32_t numDests, std::string *initTable)
:Isa100RoutingAlgorithm(),m_numDests(numDests)
{
  NS_LOG_FUNCTION(this);

  m_numHops = new uint32_t[m_numDests];
  m_table = new Mac16Address*[m_numDests];

  std::string addressStr;

  for(uint32_t iDest=0; iDest < m_numDests; iDest++){

    NS_LOG_DEBUG(" Dest: " << iDest);

  	// Determine the number of routing table entries by counting the semicolons
  	int numEntries = 0;
  	int startSearch = 0;
  	while( ( startSearch = initTable[iDest].find(":",startSearch+1) ) != std::string::npos)
  		numEntries++;

  	NS_LOG_DEBUG(" Num Hop Entries: " << numEntries);

    // Each address string is 6 chars (address plus space) except the last one which is just address.
    m_table[iDest] = new Mac16Address[numEntries];

    uint32_t iEnd, iHop = 0, iStart = 0;
    for(iEnd=0; iEnd < initTable[iDest].length(); iEnd++){

      if(initTable[iDest][iEnd] == ' '){

        addressStr = initTable[iDest].substr(iStart,iEnd-iStart);
        NS_LOG_DEBUG("  Hop: " << addressStr);

        m_table[iDest][iHop++] = Mac16Address( addressStr.c_str() );
        iStart = iEnd+1;
      }
    }

    addressStr = initTable[iDest].substr(iStart,std::string::npos);

    NS_LOG_DEBUG("  Hop: " << addressStr);
    m_table[iDest][iHop++] = Mac16Address( addressStr.c_str() );

    m_numHops[iDest] = iHop;

    NS_LOG_DEBUG("  Total Hops: " << m_numHops[iDest]);
  }
}

Isa100SourceRoutingAlgorithm::Isa100SourceRoutingAlgorithm()
:Isa100RoutingAlgorithm()
{
	m_table = 0;
	m_numDests = 0;
	m_numHops = 0;
}

Isa100SourceRoutingAlgorithm::~Isa100SourceRoutingAlgorithm()
{
	for(uint32_t iDel=0; iDel < m_numDests; iDel++)
		delete[] m_table[iDel];
	delete[] m_table;
	delete[] m_numHops;
}


void Isa100SourceRoutingAlgorithm::PrepTxPacketHeader(Isa100DlHeader &header)
{
	NS_LOG_FUNCTION(this);

	uint8_t buffer[4];
	Mac16Address addr = header.GetDaddrDestAddress();
	addr.CopyTo(buffer);

	// Populate DROUT sub-header.
	uint8_t destNodeInd = buffer[1];
	NS_LOG_DEBUG(" Sending to node " << static_cast<uint16_t>(destNodeInd));

	for(uint32_t iHop=0; iHop < m_numHops[destNodeInd]; iHop++)
		header.SetSourceRouteHop(iHop,m_table[destNodeInd][iHop]);

	// Set header for first hop.
	header.SetSrcAddrFields(0,m_address);
	header.SetDstAddrFields(0,m_table[destNodeInd][0]);


}

void Isa100SourceRoutingAlgorithm::ProcessRxPacket(Ptr<Packet> packet, bool &forwardPacketOn)
{
	NS_LOG_FUNCTION(this << m_address);

	NS_LOG_DEBUG(" Input packet " << *packet);

	// Remove the header so that it can be modified.
	Isa100DlHeader header;
	packet->RemoveHeader(header);

	Mac16Address finalDestAddr = header.GetDaddrDestAddress();
	Mac16Address nextHopAddr = header.PopNextSourceRoutingHop();

	NS_LOG_DEBUG(" Final Dest Addr: " << finalDestAddr << ", Next Hop Addr: " << nextHopAddr);

	forwardPacketOn = (m_address != finalDestAddr);

	// Set MHR source and destination addresses for next hop.
	if(forwardPacketOn){
		header.SetSrcAddrFields(0,m_address);
		header.SetDstAddrFields(0,nextHopAddr);
	}

	// Return the modified header to the packet.
	packet->AddHeader(header);

	NS_LOG_DEBUG(" Output packet " << *packet);

}




} // namespace ns3
