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
#ifndef ISA100_ERROR_MODEL_H
#define ISA100_ERROR_MODEL_H

#include "ns3/net-device.h"

namespace ns3 {

/**
 * \ingroup isa100-error-model
 *
 * Model the error rate for IEEE 802.15.4 in the AWGN channel.
 */
class Isa100ErrorModel : public Object
{
public:
  static TypeId GetTypeId (void);

  Isa100ErrorModel ();

  /**
   * return chunk success rate for given SNR
   * \return success rate (i.e. 1 - chunk error rate)
   * \param snr SNR expressed as a power ratio (i.e. not in dB)
   * \param nbits number of bits in the chunk
   */
  double GetChunkSuccessRate (double snr, uint32_t nbits) const;

private:
};


} // namespace ns3

#endif /* ISA100_ERROR_MODEL_H */
