/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 The University of Calgary- FISHLAB
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
 *         Michael Herrmann <mjherrma@ucalgary.ca>
 *
 */

#include "ns3/isa100-dl-header.h"
#include "ns3/address-utils.h"


namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Isa100DlHeader);


Isa100DlHeader::Isa100DlHeader ()
{
	// 2 octet Frame Control
  m_mhrFrameControl.bothOctets = 0;
  m_mhrFrameControl.frameType = 1;    // Bit 0-2    = 0 - Beacon, 1 - Data, 2 - Ack, 3 - Command
  m_mhrFrameControl.dstAddrMode = 2;  // Bit 10-11  = 0 - No DstAddr, 2 - ShtDstAddr, 3 - ExtDstAddr
  m_mhrFrameControl.frameVer = 1;
  m_mhrFrameControl.srcAddrMode = 2;  // Bit 14-15  = 0 - No DstAddr, 2 - ShtDstAddr, 3 - ExtDstAddr

  /* Sequence Number */
  m_seqNum = 0;                     // 1 Octet

  /* Addressing fields */
  m_addrDstPanId = 0;              // 0 or 2 Octet
  m_addrSrcPanId = 0;              // 0 or 2 Octet

  /* DHDR Sub Header */
  m_dhdrFrameControl.octet = 0;

  /* DROUT Sub-Header */
  m_numRouteAddresses = 0;

  for(int iAddr=0; iAddr < ISA100_ROUTE_MAX_HOPS; iAddr++)
  	m_routeAddresses[iAddr] = Mac16Address("EE:EE");

  /* DMIC */
  m_dmic = 0;

}

Isa100DlHeader::~Isa100DlHeader ()
{
}

TypeId Isa100DlHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100DlHeader")
    .SetParent<Header> ()
    .AddConstructor<Isa100DlHeader> ();
  return tid;
}


TypeId Isa100DlHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void Isa100DlHeader::Print (std::ostream &os) const
{
  os << "Sequence Num = " << static_cast<uint16_t> (m_seqNum);
  os << ", Src Addr = " << m_addrShortSrcAddr;
  os << ", Dst Addr = " << m_addrShortDstAddr;

  os << ", DADDR Src Addr = " << m_daddrSrcAddr;
  os << ", DADDR Dst Addr = " << m_daddrDstAddr;

  os << ", DMIC-32 = " << m_dmic;

  os << ", Gen Time = " << m_timeGeneratedNS;

//  os << ", NumRouteAddresses = " << static_cast<uint16_t> (m_numRouteAddresses);

//  for(uint32_t iAddr = 0; iAddr < m_numRouteAddresses; iAddr++)
//  	os << ", HopAddr " << iAddr << " = " << m_routeAddresses[iAddr];
}

void Isa100DlHeader::PrintFrameControl (std::ostream &os) const
{
	;
}

std::string Isa100DlHeader::GetName (void) const
{
  return "Isa100 DL Header";
}

void Isa100DlHeader::SetSourceRouteHop(uint8_t hopNum, Mac16Address addr)
{
	if( hopNum > ISA100_ROUTE_MAX_HOPS)
		NS_FATAL_ERROR("hopNum cannot exceed ISA100_ROUTE_MAX_HOPS");

	m_routeAddresses[hopNum] = addr;

	if(m_numRouteAddresses == 0 || hopNum > (m_numRouteAddresses-1))
		m_numRouteAddresses = hopNum+1;

}

Mac16Address Isa100DlHeader::PopNextSourceRoutingHop()
{
	Mac16Address nextAddr = m_routeAddresses[0];

	if(m_numRouteAddresses > 1){

		nextAddr = m_routeAddresses[1];

		for(int iAddr=1; iAddr < m_numRouteAddresses; iAddr++)
			m_routeAddresses[iAddr-1] = m_routeAddresses[iAddr];

		m_numRouteAddresses--;

	}

	return nextAddr;

}




uint32_t Isa100DlHeader::GetSerializedSize (void) const
{
  /*
   * Each mac header will have
   * Frame Control      : 2 octet
   * Sequence Number    : 1 Octet
   * DHDR Frame Control : 1 Octet
   * Dst PAN Id         : 0/2 Octet
   * Dst Address        : 0/2/8 octet
   * Src PAN Id         : 0/2 octet
   * Src Address        : 0/2/8 octet
   * Aux Sec Header     : 0/5/6/10/14 octet
   * DMIC               : 4 octet
   */

  uint32_t size = 3;  // Control and sequence number.

  size += 4; // Src and dest PAN IDs.
  size += 4; // Src and dest short addresses.

  size += 1; // DHDR frame control

  size += 1; // Number of route addresses.
  size += 2*m_numRouteAddresses;  // Storage for route addresses.

  size += 2; // DADDR source address
  size += 2; // DADDR dest address

  size += 4; // DMIC

  size += 8; //gen time

  return (size);
}


void Isa100DlHeader::Serialize (Buffer::Iterator start) const
{
  Buffer::Iterator i = start;

  i.WriteHtolsbU16 (m_mhrFrameControl.bothOctets);
  i.WriteU8 (m_seqNum);

  i.WriteHtolsbU16 (m_addrDstPanId);
  WriteTo (i, m_addrShortDstAddr);

  i.WriteHtolsbU16 (m_addrSrcPanId);
  WriteTo (i, m_addrShortSrcAddr);

  i.WriteU8(m_dhdrFrameControl.octet);

  i.WriteU8(m_numRouteAddresses);

  for(uint8_t iAddr=0; iAddr < m_numRouteAddresses; iAddr++)
  	WriteTo (i, m_routeAddresses[iAddr]);

  WriteTo (i, m_daddrSrcAddr);
  WriteTo (i, m_daddrDstAddr);

  i.WriteHtolsbU32(m_dmic);

  i.WriteHtolsbU64(m_timeGeneratedNS);

}


uint32_t Isa100DlHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_mhrFrameControl.bothOctets = i.ReadLsbtohU16 ();

  m_seqNum =  i.ReadU8 ();

  m_addrDstPanId = i.ReadLsbtohU16 ();
  ReadFrom (i, m_addrShortDstAddr);

  m_addrSrcPanId = i.ReadLsbtohU16 ();
  ReadFrom (i, m_addrShortSrcAddr);

  m_dhdrFrameControl.octet = i.ReadU8();

  m_numRouteAddresses = i.ReadU8();

  for(uint8_t iAddr=0; iAddr < m_numRouteAddresses; iAddr++)
  	ReadFrom(i, m_routeAddresses[iAddr]);

  ReadFrom (i, m_daddrSrcAddr);
  ReadFrom (i, m_daddrDstAddr);

  m_dmic = i.ReadLsbtohU32();

  m_timeGeneratedNS = i.ReadLsbtohU64();

  return i.GetDistanceFrom (start);
}

void Isa100DlHeader::SetMhrFrameControl (MhrFrameControl frameControl)
{
  m_mhrFrameControl = frameControl;
}

MhrFrameControl Isa100DlHeader::GetMhrFrameControl (void) const
{
  return m_mhrFrameControl;
}

void Isa100DlHeader::SetDhdrFrameControl (DhdrFrameControl dhdrFrameCtrl)
{
  m_dhdrFrameControl = dhdrFrameCtrl;
}

DhdrFrameControl Isa100DlHeader::GetDhdrFrameControl (void) const
{
  return m_dhdrFrameControl;
}


void Isa100DlHeader::SetDaddrSrcAddress(Mac16Address addr)
{
	m_daddrSrcAddr = addr;
}


Mac16Address Isa100DlHeader::GetDaddrSrcAddress()
{
	return m_daddrSrcAddr;
}


void Isa100DlHeader::SetDaddrDestAddress(Mac16Address addr)
{
	m_daddrDstAddr = addr;
}

Mac16Address Isa100DlHeader::GetDaddrDestAddress()
{
	return m_daddrDstAddr;
}


uint8_t Isa100DlHeader::GetSeqNum (void) const
{
  return(m_seqNum);
}

void Isa100DlHeader::SetSeqNum (uint8_t seqNum)
{
  m_seqNum = seqNum;
}

uint16_t Isa100DlHeader::GetDstPanId (void) const
{
  return(m_addrDstPanId);
}

uint16_t Isa100DlHeader::GetSrcPanId (void) const
{
  return(m_addrSrcPanId);
}

Mac16Address Isa100DlHeader::GetShortDstAddr (void) const
{
  return(m_addrShortDstAddr);
}

Mac16Address Isa100DlHeader::GetShortSrcAddr (void) const
{
  return(m_addrShortSrcAddr);
}


void Isa100DlHeader::SetSrcAddrFields (
		uint16_t panId,
		Mac16Address addr)
{
  m_addrSrcPanId = panId;
  m_addrShortSrcAddr = addr;
}


void Isa100DlHeader::SetDstAddrFields (
		uint16_t panId,
		Mac16Address addr)
{
  m_addrDstPanId = panId;
  m_addrShortDstAddr = addr;
}

void Isa100DlHeader::SetDmic(uint32_t dmic)
{
  m_dmic = dmic;
}

uint32_t Isa100DlHeader::GetDmic(void) const
{
  return(m_dmic);
}

void Isa100DlHeader::SetTimeGeneratedNS(uint64_t timeGen)
{
  m_timeGeneratedNS = timeGen;
}

uint64_t Isa100DlHeader::GetTimeGeneratedNS(void)
{
  return m_timeGeneratedNS;
}

//*********************************************************************************************
//******************************* ACK HEADER CLASS ********************************************
//*********************************************************************************************

NS_OBJECT_ENSURE_REGISTERED (Isa100DlAckHeader);

Isa100DlAckHeader::Isa100DlAckHeader ()
{
  m_mhrFrameControl.bothOctets = 0;
  m_mhrFrameControl.frameType = 1;
  m_mhrFrameControl.frameVer = 1;

  m_dhrFrameControl.octet = 0;
  m_dhrFrameControl.reserved = 3;

  m_dmic = 0;
}

Isa100DlAckHeader::~Isa100DlAckHeader ()
{
}

TypeId Isa100DlAckHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100DlAckHeader")
    .SetParent<Header> ()
    .AddConstructor<Isa100DlAckHeader> ();
  return tid;
}

TypeId Isa100DlAckHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void Isa100DlAckHeader::Print (std::ostream &os) const
{
   os << "MHR Frame Control = " << (m_mhrFrameControl.bothOctets);
   os << ", DHR Frame Control = " << static_cast<uint16_t> (m_dhrFrameControl.octet);
   os << ", Dst Addr = " << m_addrShortDstAddr;
   os << ", DMIC-32 = " << (m_dmic);
}

uint32_t Isa100DlAckHeader::GetSerializedSize (void) const
{
   /*
    * Each ack header will have
    * MHR Frame Control  : 2 octets
    * DHR Frame Control  : 1 Octet
    * DST Address        : 2 Octets
    * DMIC-32            : 4 Octets
    */

   uint32_t size = 2;  // MHR Frame Control
   size += 1;          // DHR Frame Control
   size += 2;          // DST address
   size += 4;          // DMIC-32

   return (size);
}

void Isa100DlAckHeader::Serialize (Buffer::Iterator start) const
{
   Buffer::Iterator i = start;

   i.WriteHtolsbU16 (m_mhrFrameControl.bothOctets);
   i.WriteU8 (m_dhrFrameControl.octet);
   WriteTo (i, m_addrShortDstAddr);
   i.WriteHtolsbU32(m_dmic);
}

uint32_t Isa100DlAckHeader::Deserialize (Buffer::Iterator start)
{
   Buffer::Iterator i = start;

   m_mhrFrameControl.bothOctets = i.ReadLsbtohU16 ();
   m_dhrFrameControl.octet =  i.ReadU8 ();
   ReadFrom (i, m_addrShortDstAddr);
   m_dmic = i.ReadLsbtohU32 ();

   return i.GetDistanceFrom (start);
}

void Isa100DlAckHeader::SetMhrFrameControl (MhrFrameControl frameControl)
{
   m_mhrFrameControl = frameControl;
}

MhrFrameControl Isa100DlAckHeader::GetMhrFrameControl (void) const
{
   return m_mhrFrameControl;
}

void Isa100DlAckHeader::SetDhrFrameControl (DhrFrameControl dhrFrameCtrl)
{
   m_dhrFrameControl = dhrFrameCtrl;
}

DhrFrameControl Isa100DlAckHeader::GetDhrFrameControl (void) const
{
   return m_dhrFrameControl;
}

void Isa100DlAckHeader::SetDmic(uint32_t dmic)
{
  m_dmic = dmic;
}

uint32_t Isa100DlAckHeader::GetDmic (void) const
{
  return m_dmic;
}

void Isa100DlAckHeader::SetShortDstAddr (Mac16Address addr)
{
  m_addrShortDstAddr = addr;
}

Mac16Address Isa100DlAckHeader::GetShortDstAddr (void) const
{
  return(m_addrShortDstAddr);
}
} //namespace ns3


