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


/**
 * \defgroup isa100-11a-dl-header ISA 100.11a Data Link Layer Header
 *
 *  This is a simplified version of the 802.15.4 header.
 * - Some fixed 802.15.4 fields are not present.  For example, addressing is always 16 bits so the addressing
 *   mode info is removed, 802.15.4 ACKs are not used so the ACK fields are removed, etc.
 *  @{
 */

#ifndef ISA_100_DL_HEADER_H
#define ISA_100_DL_HEADER_H

#include "ns3/header.h"
#include "ns3/mac16-address.h"

namespace ns3 {

// This should not exceed 256.
#define ISA100_ROUTE_MAX_HOPS 25

// Prevent the complier from adding padding to the unions larger than 1 byte (this may not be necessary in this case)
#pragma pack(push,1)

// 802.15.4 MHR Frame Control
typedef union
{
  struct
  {
    uint8_t frameType  : 3;    ///< Frame Control Bit 0-2    = 0 - Beacon, 1 - Data, 2 - Ack, 3 - Command
    uint8_t securityEn : 1;    ///< Frame Control Bit 3      = 0 - no AuxSecHdr ,  1 - security enabled AuxSecHdr present
    uint8_t framePend  : 1;    ///< Frame Control Bit 4
    uint8_t ackReq     : 1;    ///< Frame Control Bit 5
    uint8_t panIdComp  : 1;    ///< Frame Control Bit 6      = 0 - no compression, 1 - using only DstPanId for both Src and DstPanId
    uint8_t reserved   : 3;    ///< Frame Control Bit 7-9
    uint8_t dstAddrMode: 2;    ///< Frame Control Bit 10-11  = 0 - No DstAddr, 2 - ShtDstAddr, 3 - ExtDstAddr
    uint8_t frameVer   : 2;    ///< Frame Control Bit 12-13
    uint8_t srcAddrMode: 2;    ///< Frame Control Bit 14-15  = 0 - No DstAddr, 2 - ShtDstAddr, 3 - ExtDstAddr
  };
  uint16_t bothOctets;
}MhrFrameControl;

// Allow padding again
#pragma pack(pop)

// DHDR Frame Control Octet
typedef union
{
  struct
  {
    uint8_t dlVersion : 2;      ///< DHDR Frame Control Bit 1-0 = Always 00
    uint8_t clkRecipt : 1;      ///< DHDR Frame Control Bit 2   = Is Tx device a DL clock recipient?
    uint8_t slowHop   : 1;      ///< DHDR Frame Control Bit 3   = Should slow hopping offset be included in DMXHR?
    uint8_t dauxIncl  : 1;      ///< DHDR Frame Control Bit 4   = Is DAUX sub-header present?
    uint8_t euiReq    : 1;      ///< DHDR Frame Control Bit 5   = Should receiver include EUI-64 in ack/nack?
    uint8_t signalQ   : 1;      ///< DHDR Frame Control Bit 6   = Should receiver report signal quality in ack/nack?
    uint8_t ackReq    : 1;      ///< DHDR Frame Control Bit 7   = Is an ack/nack required?
  };
  uint8_t octet;
}DhdrFrameControl;

// DHR Frame Control
typedef union
{
  struct
  {
    uint8_t reserved  : 3;    ///< DHR Frame Control Bit 2-0 = Always 011
    uint8_t dauxIncl  : 1;    ///< DHR Frame Control Bit 3   = Is DAUX sub-header included?
    uint8_t ackType   : 2;    ///< DHR Frame Control Bit 5-4 = 0 - ACK, 1 - ACK/ECN, 2 - NACK0, 3 - NACK1
    uint8_t slowHop   : 1;    ///< DHR Frame Control Bit 6   = Include slow-hopping timeslot offset?
    uint8_t clkCorr   : 1;    ///< DHR Frame Control Bit 7   = Include clock correction?
  };
  uint8_t octet;
}DhrFrameControl;


class Isa100DlHeader : public Header
{

public:

	Isa100DlHeader (void);

  virtual ~Isa100DlHeader (void);

  static TypeId GetTypeId (void);

  TypeId GetInstanceTypeId (void) const;

  /** Get header name.
   *
   * \return A string containing the header name.
   */
  std::string GetName (void) const;

  /** Print frame control information.
   */
  void PrintFrameControl (std::ostream &os) const;

  /** Print header contents.
   */
  void Print (std::ostream &os) const;

  /** Calculate header size.
   *
   * \return Header size in bytes.
   */
  uint32_t GetSerializedSize (void) const;

  /** Write the header to a byte buffer.
   *
   * @param start Iterator for the buffer.
   */
  void Serialize (Buffer::Iterator start) const;

  /** Read header contents from a byte buffer.
   *
   * @param start Iterator for the buffer.
   * \return Size of the buffer.
   */
  uint32_t Deserialize (Buffer::Iterator start);

  /** Get frame control fields.
   *
   * \return A 16 bit integer with frame control bits set.
   */
  MhrFrameControl GetMhrFrameControl (void) const;

  /** Set frame control fields.
   *
   * @param A 16 bit integer with frame control bits set.
   */
  void SetMhrFrameControl (MhrFrameControl frameControl);

  /** Get frame control fields.
   *
   * \return A 8 bit integer with frame control bits set.
   */
  DhdrFrameControl GetDhdrFrameControl (void) const;

  /** Set frame control fields.
   *
   * @param A 8 bit integer with frame control bits set.
   */
  void SetDhdrFrameControl (DhdrFrameControl dhdrFrameControl);

  /** Get frame sequence number.
   *
   * \return 8 bit sequence number.
   */
  uint8_t GetSeqNum (void) const;

  /** Set the frame sequence number.
   *
   * @param seqNum The sequence number value.
   */
  void SetSeqNum (uint8_t seqNum);

  /** Get destination PAN ID.
   *
   * \return PAN ID.
   */
  uint16_t GetDstPanId (void) const;

  /** Get source PAN ID.
   *
   * \return PAN ID.
   */
  uint16_t GetSrcPanId (void) const;

  /** Get destination address.
   *
   * \return 16 bit address.
   */
  Mac16Address GetShortDstAddr (void) const;

  /** Get source address.
   *
   * \return 16 bit address.
   */
  Mac16Address GetShortSrcAddr (void) const;

  /** Set source address and PAN ID.
   *
   * @param panId PAN ID.
   * @param add Source address.
   */
  void SetSrcAddrFields (uint16_t panId,
                         Mac16Address addr);

  /** Set destination address and PAN ID.
   *
   * @param panId PAN ID.
   * @param add Destination address.
   */
  void SetDstAddrFields (uint16_t panId,
                         Mac16Address addr);

  /** Set DADDR source address.
   *
   * \param addr Source address.
   */
  void SetDaddrSrcAddress(Mac16Address addr);

  /** Get DADDR source address.
   *
   * \return Source address.
   */
  Mac16Address GetDaddrSrcAddress();

  /** Set DADDR destination address.
   *
   * \param addr Destination address.
   */
  void SetDaddrDestAddress(Mac16Address addr);

  /** Get DADDR destination address.
   *
   * \return Destination address.
   */
  Mac16Address GetDaddrDestAddress();

  /* Set the DMIC for this header
   *
   * \param dmic the DMIC for this header
   */
  void SetDmic(uint32_t dmic);

  /* Get the DMIC
   *
   * \return DMIC
   */
  uint32_t GetDmic(void) const;





  /** Set network hop.
   *
   * @param hopNum  Index indicating the hop position in the path.
   * @param addr Destination node for the hop.
   */
  void SetSourceRouteHop(uint8_t hopNum, Mac16Address addr);


  /** Get next address along a multi-hop path and remove from header.
   * - Will return but won't remove the final destination address from the header.
   *
   * \return Address.
   */
  Mac16Address PopNextSourceRoutingHop();

  /* Set the time when the packet was generated
   *
   * @param timeGen time in nanoseconds of packet's origin
   */
  void SetTimeGeneratedNS (uint64_t timeGen);

  /* Get the time when the packet was generated
   *
   * @return time in nanoseconds of packet's origin
   */
  uint64_t GetTimeGeneratedNS (void);



  /**}@*/

private:

  // MHR Sub Header
  MhrFrameControl m_mhrFrameControl;    ///< Frame control
  uint8_t m_seqNum;                     ///< Sequence number.
  uint16_t m_addrDstPanId;              ///< Destination PAN ID 0 or 2 Octet
  Mac16Address m_addrShortDstAddr;      ///< 16 bit MAC destination 0 or 8 Octet
  uint16_t m_addrSrcPanId;              ///< Source PAN ID 0 or 2 Octet
  Mac16Address m_addrShortSrcAddr;      ///< 16 bit MAC source 0 or 8 Octet

  // DHDR Sub Header
  DhdrFrameControl m_dhdrFrameControl;

  // DROUT Sub Header
  uint8_t m_numRouteAddresses; ///< Number of source routing addresses in DROUT sub-header.
  Mac16Address m_routeAddresses[ISA100_ROUTE_MAX_HOPS]; ///< Source routing address list.

  // DADDR Sub Header
  Mac16Address m_daddrSrcAddr; ///< Source address specified by DD-Data.Request
  Mac16Address m_daddrDstAddr; ///< Destination address specified by DD-Data.Request

  // DMIC -- technically a footer, but for simplicity put it in the header
  uint32_t m_dmic;

  // Tracing identifier -- used for processing some simulation statistics
  uint64_t m_timeGeneratedNS; ///< the simulation time (ns) when the original packet was generated


};

//*********************************************************************************************
//******************************* ACK HEADER CLASS ********************************************
//*********************************************************************************************

class Isa100DlAckHeader : public Header
{

public:

  Isa100DlAckHeader (void);

  virtual ~Isa100DlAckHeader (void);

  static TypeId GetTypeId (void);

  TypeId GetInstanceTypeId (void) const;

  /** Print header contents.
   */
  void Print (std::ostream &os) const;

  /** Calculate header size.
   *
   * \return Header size in bytes.
   */
  uint32_t GetSerializedSize (void) const;

  /** Write the header to a byte buffer.
   *
   * @param start Iterator for the buffer.
   */
  void Serialize (Buffer::Iterator start) const;

  /** Read header contents from a byte buffer.
   *
   * @param start Iterator for the buffer.
   * \return Size of the buffer.
   */
  uint32_t Deserialize (Buffer::Iterator start);

  /** Get MHR frame control fields.
   *
   * \return A 16 bit integer with MHR frame control bits set.
   */
  MhrFrameControl GetMhrFrameControl (void) const;

  /** Set MHR frame control fields.
   *
   * @param A 16 bit integer with MHR frame control bits set.
   */
  void SetMhrFrameControl (MhrFrameControl frameControl);

  /** Get DHR frame control fields.
   *
   * \return A 8 bit integer with DHR frame control bits set.
   */
  DhrFrameControl GetDhrFrameControl (void) const;

  /** Set DHR frame control fields.
   *
   * @param A 8 bit integer with DHR frame control bits set.
   */
  void SetDhrFrameControl (DhrFrameControl dhrFrameControl);

  /** Get the DMIC.
   *
   * \return The 32-bit DMIC.
   */
  uint32_t GetDmic(void) const;

  /** Set the DMIC.
   *
   * @param The 32-bit DMIC to set.
   */
  void SetDmic (uint32_t dmic);

  /* Set the destination address
   *
   * @param The 16 bit destination address
   */
  void SetShortDstAddr (Mac16Address addr);

  /** Get destination address.
   *
   * \return 16 bit address.
   */
  Mac16Address GetShortDstAddr (void) const;


  /**}@*/

private:

  // MHR sub-header
  MhrFrameControl m_mhrFrameControl;

  // DHR sub-header
  DhrFrameControl m_dhrFrameControl;

  // Destination Address (not in isa100-11a standard)
  Mac16Address m_addrShortDstAddr;

  // DMIC-32 of the packet being ack'd
  uint32_t m_dmic;

};
}; // namespace ns-3
#endif


