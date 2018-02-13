
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


#include "ns3/isa100-helper.h"

#include "ns3/position-allocator.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/boolean.h"

#include "ns3/zigbee-trx-current-model.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/single-model-spectrum-channel.h"
#include "ns3/isa100-application.h"
#include "ns3/isa100-routing.h"
#include "ns3/isa100-net-device.h"
#include "ns3/goldsmith-tdma-optimizer.h"
#include "ns3/minhop-tdma-optimizer.h"
#include "ns3/convex-integer-tdma-optimizer.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/packet.h"


NS_LOG_COMPONENT_DEFINE ("Isa100HelperLocations");

namespace ns3 {

void
Isa100Helper::SetDeviceConstantPosition(NetDeviceContainer dc, Ptr<ListPositionAllocator> positionAlloc)
{
	for (uint32_t i = 0; i < dc.GetN(); i++)
	{
		Ptr<ConstantPositionMobilityModel> senderMobility = CreateObject<ConstantPositionMobilityModel> ();
		Vector v=positionAlloc->GetNext();
		senderMobility->SetPosition (v); // Server

		Ptr<NetDevice> baseDevice = m_devices.Get(i);

		Ptr<Isa100NetDevice> netDevice = baseDevice->GetObject<Isa100NetDevice>();
		netDevice->GetPhy ()->SetMobility (senderMobility);
	}
}


void Isa100Helper::GenerateLocationsFixedNumNodes(Ptr<ListPositionAllocator> positionAlloc, int numNodes, double xLength, double yLength, double minNodeSpacing, Vector sinkLocation)
{
  std::vector<Vector> nodeLocations;

  Ptr<UniformRandomVariable> randX = CreateObject<UniformRandomVariable> ();
  randX->SetAttribute("Min", DoubleValue(0));
  randX->SetAttribute("Max", DoubleValue(xLength));

  Ptr<UniformRandomVariable> randY = CreateObject<UniformRandomVariable> ();
  randY->SetAttribute("Min", DoubleValue(0));
  randY->SetAttribute("Max", DoubleValue(yLength));

  // Sink node
  positionAlloc->Add(sinkLocation);
  m_locationTrace(0,sinkLocation.x, sinkLocation.y, sinkLocation.z);


  // Sensor (Tx) nodes
  double x, y;
  std::vector<Vector3D> checkDist;
  checkDist.push_back(Vector(0,0,0));
  bool conflict;
  uint16_t count;

  for (int i = 1; i < numNodes; i++)
  {
    // Generate x-coordinate
    count = 0;
    do
    {
      NS_ASSERT_MSG(count < 10000, "Could not place nodes to satisfy minimum spacing requirement.");
      x = randX->GetValue();
      y = randY->GetValue();
      conflict = false;

      // Check minimum distance requirement
      if (minNodeSpacing > 0)
      {
        for (std::vector<Vector3D>::iterator it = checkDist.begin(); it != checkDist.end(); ++it)
        {
          if (CalculateDistance(*it, Vector(x,y,0)) < minNodeSpacing)
          {
            conflict = true;
            break;
          }
        }
      }
      count++;
    } while (conflict);

    // At this point valid x,y coordinates have been generated
    checkDist.push_back(Vector(x,y,0));
    positionAlloc->Add(Vector(x,y,0));




    m_locationTrace(i,x,y,0.0);
  }







}


}

