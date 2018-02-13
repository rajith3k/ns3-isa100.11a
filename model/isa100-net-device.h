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
 * Author: Geoffrey Messier <gmessier@ucalgary.ca>
 *         Hazem Gomaa <gomaa.hazem@gmail.com>
 *         Michael Herrmann <mjherrma@gmail.com>
 */

#ifndef ISA100_NET_DEVICE_H
#define ISA100_NET_DEVICE_H

#include "ns3/pointer.h"
#include "ns3/traced-callback.h"
#include "ns3/net-device.h"
#include "ns3/isa100-processor.h"
#include "ns3/isa100-sensor.h"
#include "ns3/isa100-battery.h"
#include "ns3/zigbee-phy.h"
#include "ns3/isa100-dl.h"

namespace ns3 {

class SpectrumChannel;

/**
 * \defgroup isa100-11a-net-device ISA 100.11a Net Device Implementation
 *
 * - This documents the code that implements the 100.11a net device class.
 * - Similar to LrWpan, the inherited IP API functions (Send(), SendTo(), etc.)
 *   are not implemented.  Instead, we use an Isa100Application object with a
 *   callback function connected directly to the DL-DATA.request implementation.
 * - Need to look at the higher layer 6LoWPan functionality of ISA to see how much
 *   of the socket API can be salvaged.
 *  @{
 */


class Isa100NetDevice : public NetDevice
{
public:

  static TypeId GetTypeId (void);

  Isa100NetDevice();

  virtual ~Isa100NetDevice ();

  /** Set DL object used by net device.
   *
   * @param dl Pointer to DL object.
   */
  void SetDl (Ptr<Isa100Dl> dl);

  /** Set PHY object used by the net device.
   *
   * @param phy Pointer to PHY object.
   */
  void SetPhy (Ptr<ZigbeePhy> phy);

  /** Set battery object used by the net device.
   *
   * @param battery Pointer to battery object.
   */
  void SetBattery (Ptr<Isa100Battery> battery);

  /** Set processor object used by the net device.
   *
   * @param processor Pointer to processor object.
   */
  void SetProcessor (Ptr<Isa100Processor> processor);

  /** Set sensor object used by the net device.
   *
   * @param sensor Pointer to sensor object.
   */
  void SetSensor (Ptr<Isa100Sensor> sensor);


  /** Link to the node using this net device.
   *
   * @param phy Pointer to Node object.
   */
  virtual void SetNode (Ptr<Node> node);

  /** Connect to the channel object used to communicate with other net devices.
   *
   * @param channel Pointer to the channel object.
   */
  void SetChannel (Ptr<SpectrumChannel> channel);

  /** Get the DL object.
   *
   * \return Pointer to DL object.
   */
  Ptr<Isa100Dl> GetDl (void) const;

  /** Get the PHY object.
   *
   * \return Pointer to the PHY object.
   */
  Ptr<ZigbeePhy> GetPhy (void) const;

  /** Get the battery object.
   *
   * \return Pointer to the battery object.
   */
  Ptr<Isa100Battery> GetBattery (void) const;

  /** Get the processor object.
   *
   * \return Pointer to the processor object.
   */
  Ptr<Isa100Processor> GetProcessor (void) const;

  /** Get the sensor object.
   *
   * \return Pointer to the sensor object.
   */
  Ptr<Isa100Sensor> GetSensor (void) const;

  /** Get the node using this net device.
   *
   * \return Pointer to the node object.
   */
  virtual Ptr<Node> GetNode (void) const;


  /** Get the channel object.
   *
   * \return Pointer to the channel object.
   */
  virtual Ptr<Channel> GetChannel (void) const;

  /** Set address.
   * - Configures the 16 bit MAC address used by the DL object.
   *
   * @param address Address value.
   */
  virtual void SetAddress (Address address);


  /** Inherited from net device base class.
   *
   */
  virtual void SetIfIndex (const uint32_t index);

  /** Inherited from net device base class.
   *
   */
  virtual uint32_t GetIfIndex (void) const;


  /** Get the network interface address.
   *
   * \return Address of the net device.
   */
  virtual Address GetAddress (void) const;

  /// Not implemented.
  virtual bool SetMtu (const uint16_t mtu);

  /// Not implemented.
  virtual uint16_t GetMtu (void) const;

  /// Not implemented.
  virtual bool IsLinkUp (void) const;

  /// Not implemented.
  virtual void AddLinkChangeCallback (Callback<void> callback);

  /// Not implemented.
  virtual bool IsBroadcast (void) const;

  /// Not implemented.
  virtual Address GetBroadcast (void) const;

  /// Not implemented.
  virtual bool IsMulticast (void) const;

  /// Not implemented.
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;

  /// Not implemented.
  virtual Address GetMulticast (Ipv6Address addr) const;

  /// Not implemented.
  virtual bool IsBridge (void) const;

  /// Not implemented.
  virtual bool IsPointToPoint (void) const;

  /// Not implemented.
  virtual bool Send (Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);

  /// Not implemented.
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);

  /// Not implemented.
  virtual bool NeedsArp (void) const;

  /// Not implemented.
  virtual void SetReceiveCallback (ReceiveCallback cb);

  /// Not implemented.
  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);

  /// Not implemented.
  virtual bool SupportsSendFrom (void) const;

  /**}@*/

private:

  /** Called during net device disposal.
   */
  virtual void DoDispose (void);

  /** Called during net device creation.
   */
  virtual void DoInitialize (void);

  /** Called when bringing link up.
   */
  void LinkUp (void);

  /** Called when tearing link down.
   *
   */
  void LinkDown (void);

  /** Gets the channel object.
   *
   * \return Pointer to the channel object.
   */
  Ptr<SpectrumChannel> DoGetChannel (void) const;

  /** Called to complete the configuration of the net device protocol objects once they're all created.
   *
   */
  void CompleteConfig (void);

  Ptr<Isa100Dl> m_dl;   ///< The data link layer object.
  Ptr<ZigbeePhy> m_phy; ///< The PHY layer object.
  Ptr<Isa100Battery> m_battery; ///< The battery object.
  Ptr<Isa100Processor> m_processor; ///< The processor object.
  Ptr<Isa100Sensor> m_sensor; ///< The sensor object.
  Ptr<Node> m_node;    ///< The node hosting the net device.

  bool m_configComplete;  ///< Indicates the protocol objects have been configured.
  bool m_linkUp;         ///< Indicates the link is up.
  uint32_t m_ifIndex;   ///< Index number for this interface.
 // mutable uint16_t m_mtu;   ///< ?

  TracedCallback<> m_linkChanges; ///< ?
};



} // namespace ns3

#endif /* NET_DEVICE_H */
