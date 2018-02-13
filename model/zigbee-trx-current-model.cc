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
 * Author:   Michael Herrmann <mjherrma@ucalgary.ca>
 */

#include "zigbee-trx-current-model.h"
#include "ns3/log.h"
#include "ns3/double.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ZigbeeTrxCurrentModel");

NS_OBJECT_ENSURE_REGISTERED (ZigbeeTrxCurrentModel);

TypeId 
ZigbeeTrxCurrentModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ZigbeeTrxCurrentModel")
    .SetParent<Object> ()
    .AddConstructor<ZigbeeTrxCurrentModel> ()
    .AddAttribute ("ProcessorSleepCurrentA",
                  "The microprocessor sleep current in Ampere.",
                  DoubleValue (0.0000249),  // sleep mode = 24.9uA
                  MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetProcessorSleepCurrentA,
                                      &ZigbeeTrxCurrentModel::GetProcessorSleepCurrentA),
                  MakeDoubleChecker<double> (0))
    .AddAttribute ("ProcessorActiveCurrentA",
                  "The microprocessor active current in Ampere.",
                  DoubleValue (0.0185),  // active mode = 18.5mA
                  MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetProcessorActiveCurrentA,
                                      &ZigbeeTrxCurrentModel::GetProcessorActiveCurrentA),
                      MakeDoubleChecker<double> (0))
    .AddAttribute ("ProcessorIdleCurrentA",
                  "The microprocessor idle current in Ampere.",
                  DoubleValue (0.0029),  // idle mode = 2.9mA
                  MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetProcessorIdleCurrentA,
                  &ZigbeeTrxCurrentModel::GetProcessorIdleCurrentA),
                MakeDoubleChecker<double> (0))
    .AddAttribute ("TrxOffCurrentA",
                   "The default radio TRX_OFF current in Ampere.",
                   DoubleValue (0.0003),  // TRX_OFF mode = 300uA
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetTrxOffCurrentA,
                                       &ZigbeeTrxCurrentModel::GetTrxOffCurrentA),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("RxOnCurrentA",
                   "The default radio RX_ON current in Ampere.",
                   DoubleValue (0.0118),  // RX_ON mode = 11.8mA
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetRxOnCurrentA,
                                       &ZigbeeTrxCurrentModel::GetRxOnCurrentA),
                   MakeDoubleChecker<double> ())
     .AddAttribute ("BusyRxCurrentA",
                    "The default radio BUSY_RX current in Ampere.",
                    DoubleValue (0.0118),  // BUSY_RX mode = 11.8mA (equal to RX_ON)
                    MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetBusyRxCurrentA,
                                        &ZigbeeTrxCurrentModel::GetBusyRxCurrentA),
                    MakeDoubleChecker<double> (0))
    .AddAttribute ("TxOnCurrentA",
                   "The radio TX_ON current in Ampere.",
                   DoubleValue (0.0052),    // TX_ON mode = 5.2mA
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetTxOnCurrentA,
                                       &ZigbeeTrxCurrentModel::GetTxOnCurrentA),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("BusyTxCurrentA",
                   "The radio BUSY_TX current in Ampere.",
                   DoubleValue (0.0138),    // max transmit power 4dBm = 13.8mA
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::m_busyTxCurrentA),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("SleepCurrentA",
                   "The radio Sleep current in Ampere.",
                   DoubleValue (0.00000002),  // sleep mode = 20nA
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetSleepCurrentA,
                                       &ZigbeeTrxCurrentModel::GetSleepCurrentA),
                   MakeDoubleChecker<double> (0))
    .AddAttribute ("Slope", "The slope of the Tx Current vs Tx Power relationship (in A/dBm).",
                   DoubleValue (0.0003013),
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetTxCurrentPowerSlope,
                                       &ZigbeeTrxCurrentModel::GetTxCurrentPowerSlope),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("Offset", "The offset of the Tx Current vs Tx Power relationship (in A/dBm).",
                   DoubleValue (0.01224),
                   MakeDoubleAccessor (&ZigbeeTrxCurrentModel::SetTxCurrentPowerOffset,
                                       &ZigbeeTrxCurrentModel::GetTxCurrentPowerOffset),
                   MakeDoubleChecker<double> ())
  ;
  return tid;
}

ZigbeeTrxCurrentModel::ZigbeeTrxCurrentModel ()
{
  ;
}

ZigbeeTrxCurrentModel::~ZigbeeTrxCurrentModel()
{
  ;
}
double
ZigbeeTrxCurrentModel::GetTrxOffCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_trxOffCurrentA;
}

void
ZigbeeTrxCurrentModel::SetTrxOffCurrentA (double trxOffCurrentA)
{
  NS_LOG_FUNCTION (this << trxOffCurrentA);
  m_trxOffCurrentA = trxOffCurrentA;
}

double
ZigbeeTrxCurrentModel::GetRxOnCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_rxOnCurrentA;
}

void
ZigbeeTrxCurrentModel::SetRxOnCurrentA (double rxOnCurrentA)
{
  NS_LOG_FUNCTION (this << rxOnCurrentA);
  m_rxOnCurrentA = rxOnCurrentA;
}

double
ZigbeeTrxCurrentModel::GetBusyRxCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_busyRxCurrentA;
}

void
ZigbeeTrxCurrentModel::SetBusyRxCurrentA (double busyRxCurrentA)
{
  NS_LOG_FUNCTION (this << busyRxCurrentA);
  m_busyRxCurrentA = busyRxCurrentA;
}

double
ZigbeeTrxCurrentModel::GetTxOnCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_txOnCurrentA;
}

void
ZigbeeTrxCurrentModel::SetTxOnCurrentA (double txOnCurrentA)
{
  NS_LOG_FUNCTION (this << txOnCurrentA);
  m_txOnCurrentA = txOnCurrentA;
}

double
ZigbeeTrxCurrentModel::GetBusyTxCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_busyTxCurrentA;
}

double
ZigbeeTrxCurrentModel::GetBusyTxCurrentA (double txPowerDbm) const
{
  NS_LOG_FUNCTION (this << txPowerDbm);

  double ret = txPowerDbm*m_slope + m_offset;

  if(ret < 0)
    ret = 0;

  return ret;
}

void
ZigbeeTrxCurrentModel::SetBusyTxCurrentA (double busyTxCurrentA)
{
  NS_LOG_FUNCTION (this << busyTxCurrentA);
  m_busyTxCurrentA = busyTxCurrentA;
}

double
ZigbeeTrxCurrentModel::GetSleepCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_sleepCurrentA;
}

void
ZigbeeTrxCurrentModel::SetSleepCurrentA (double sleepCurrentA)
{
  NS_LOG_FUNCTION (this << sleepCurrentA);
  m_sleepCurrentA = sleepCurrentA;
}

double
ZigbeeTrxCurrentModel::GetProcessorSleepCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_processorSleepCurrentA;
}

void
ZigbeeTrxCurrentModel::SetProcessorSleepCurrentA (double sleepCurrentA)
{
  NS_LOG_FUNCTION (this << sleepCurrentA);
  m_processorSleepCurrentA = sleepCurrentA;
}

double
ZigbeeTrxCurrentModel::GetProcessorActiveCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_processorActiveCurrentA;
}

void
ZigbeeTrxCurrentModel::SetProcessorActiveCurrentA (double activeCurrentA)
{
  NS_LOG_FUNCTION (this << activeCurrentA);
  m_processorActiveCurrentA = activeCurrentA;
}

double
ZigbeeTrxCurrentModel::GetProcessorIdleCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_processorIdleCurrentA;
}

void
ZigbeeTrxCurrentModel::SetProcessorIdleCurrentA (double idleCurrentA)
{
  NS_LOG_FUNCTION (this << idleCurrentA);
  m_processorIdleCurrentA = idleCurrentA;
}

void
ZigbeeTrxCurrentModel::SetTxCurrentPowerSlope (double slope)
{
  NS_LOG_FUNCTION (this << slope);
  m_slope = slope;
}

void
ZigbeeTrxCurrentModel::SetTxCurrentPowerOffset (double offset)
{
  NS_LOG_FUNCTION (this << offset);
  m_offset = offset;
}


double
ZigbeeTrxCurrentModel::GetTxCurrentPowerSlope (void) const
{
  NS_LOG_FUNCTION (this);
  return m_slope;
}

double
ZigbeeTrxCurrentModel::GetTxCurrentPowerOffset (void) const
{
  NS_LOG_FUNCTION (this);
  return m_offset;
}

void
ZigbeeTrxCurrentModel::UpdateTxCurrent (double txPowerDbm)
{
  NS_LOG_FUNCTION (this << txPowerDbm);

  m_busyTxCurrentA = txPowerDbm*m_slope + m_offset;

  if(m_busyTxCurrentA < 0)
    NS_FATAL_ERROR("Transmit current cannot be negative.");
}


} // namespace ns3
