/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 The Boeing Company
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
 * Author: Gary Pei <guangyu.pei@boeing.com>
 */
#include "ns3/log.h"

#include "fish-wpan-spectrum-value-helper.h"
NS_LOG_COMPONENT_DEFINE ("FishWpanSpectrumValueHelper");

namespace ns3 {

Ptr<SpectrumModel> g_FishWpanSpectrumModel;

class FishWpanSpectrumModelInitializer
{
public:
  FishWpanSpectrumModelInitializer ()
  {
    NS_LOG_FUNCTION (this);

    Bands bands;
    // Use center frequencies only for the 11 channels supported by ISA100.
    // - Channels are 2MHz wide and separated by 5MHz
    for(int i=0; i < 11; i++){
    	BandInfo bi;
    	bi.fl = 2404e6 + i * 5.0e6;
    	bi.fh = 2406e6 + i * 5.0e6;
      bi.fc = (bi.fl +  bi.fh) / 2;
      bands.push_back (bi);
    }

    g_FishWpanSpectrumModel = Create<SpectrumModel> (bands);
  }

} g_FishWpanSpectrumModelInitializerInstance;

FishWpanSpectrumValueHelper::FishWpanSpectrumValueHelper ()
{
  NS_LOG_FUNCTION (this);
  m_noiseFactor = 1.0;
}

FishWpanSpectrumValueHelper::~FishWpanSpectrumValueHelper ()
{
}

Ptr<SpectrumValue>
FishWpanSpectrumValueHelper::CreateTxPowerSpectralDensity (double txPower, uint32_t channel)
{
  NS_LOG_FUNCTION (this);
  Ptr<SpectrumValue> txPsd = Create <SpectrumValue> (g_FishWpanSpectrumModel);
  *txPsd = 0;

  // txPower is expressed in dBm. We must convert it into natural unit.
  txPower = pow (10., (txPower - 30) / 10);

  // The RF bandwidth of the signal is 2 MHz after spreading.
  // This gives the energy (W/Hz) of the signal.
  double txPowerDensity = txPower / 2.0e6;

  NS_ASSERT_MSG ((channel >= 11 && channel <= 26), "Invalid channel numbers");

  (*txPsd)[channel - 11] = txPowerDensity;

  return txPsd;
}

Ptr<SpectrumValue>
FishWpanSpectrumValueHelper::CreateNoisePowerSpectralDensity (uint32_t channel)
{
  NS_LOG_FUNCTION (this);
  Ptr<SpectrumValue> noisePsd = Create <SpectrumValue> (g_FishWpanSpectrumModel);
  *noisePsd = 0;

  static const double BOLTZMANN = 1.3803e-23;
  // Nt  is the power of thermal noise in W
  double Nt = BOLTZMANN * 290.0;

  // noise Floor (W) which accounts for thermal noise and non-idealities of the receiver
  double noisePowerDensity = m_noiseFactor * Nt;

  NS_ASSERT_MSG ((channel >= 11 && channel <= 26), "Invalid channel numbers");

  (*noisePsd)[channel - 11] = noisePowerDensity;

  return noisePsd;
}

double
FishWpanSpectrumValueHelper::TotalAvgPower (const SpectrumValue &psd)
{
  NS_LOG_FUNCTION (this);
  double totalAvgPower = 0.0;

  // numerically integrate to get area under psd using the 2MHz band resolution.

  totalAvgPower = Sum (psd * 2.0e6);

  return totalAvgPower;
}

} // namespace ns3
