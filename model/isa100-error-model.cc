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
 */

#include <math.h>

#include "ns3/object.h"
#include "ns3/log.h"

#include "ns3/isa100-error-model.h"


NS_LOG_COMPONENT_DEFINE ("Isa100ErrorModel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Isa100ErrorModel);

TypeId
Isa100ErrorModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Isa100ErrorModel")
    .SetParent<Object> ()
    .AddConstructor<Isa100ErrorModel> ()
  ;
  return tid;
}

Isa100ErrorModel::Isa100ErrorModel ()
{
}

double
Isa100ErrorModel::GetChunkSuccessRate (double snr, uint32_t nbits) const
{
	// Q(x) = 0.5erfc(x/sqrt(2));

  double ber = 0.5*erfc(sqrt(snr/2));

  return pow(1.0-ber,nbits);
}


} // namespace ns3
