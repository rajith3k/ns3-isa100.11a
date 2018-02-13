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

#include <iomanip>

#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/pointer.h"
#include "ns3/energy-source.h"
#include "zigbee-radio-energy-model.h"
#include "zigbee-tx-current-model.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ZigbeeRadioEnergyModel");

NS_OBJECT_ENSURE_REGISTERED (ZigbeeRadioEnergyModel);

/*
 * The defaults for this model are from two devices:
 * (1) ZigBee Transceiver: AT86RF233
 *      ---> TRX_OFF: 300uA
 *      ---> RX_ON: 11.8mA
 *      ---> TX_ON: 5.2mA
 *      ---> BUSY_TX: 13.8mA (max)
 *      ---> SLEEP:20nA
 * (2) 32-bit Microcontroller: AT32UC3B
 *      ---> Active: 18.5mA
 *      ---> Sleep: 24.9uA
 */
TypeId
ZigbeeRadioEnergyModel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ZigbeeRadioEnergyModel")
    .SetParent<DeviceEnergyModel> ()
    .SetGroupName ("Energy")
    .AddConstructor<ZigbeeRadioEnergyModel> ()
    .AddAttribute ("TrxOffCurrentA",
                   "The default radio TRX_OFF current in Ampere.",
                   DoubleValue (0.0003),  // TRX_OFF mode = 300uA
                   MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetTrxOffCurrentA,
                                       &ZigbeeRadioEnergyModel::GetTrxOffCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("RxOnCurrentA",
                   "The default radio RX_ON current in Ampere.",
                   DoubleValue (0.0118),  // RX_ON mode = 11.8mA
                   MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetRxOnCurrentA,
                                       &ZigbeeRadioEnergyModel::GetRxOnCurrentA),
                   MakeDoubleChecker<double> ())
     .AddAttribute ("BusyRxCurrentA",
                    "The default radio BUSY_RX current in Ampere.",
                    DoubleValue (0.0118),  // BUSY_RX mode = 11.8mA (equal to RX_ON)
                    MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetBusyRxCurrentA,
                                        &ZigbeeRadioEnergyModel::GetBusyRxCurrentA),
                    MakeDoubleChecker<double> ())
    .AddAttribute ("TxOnCurrentA",
                   "The radio TX_ON current in Ampere.",
                   DoubleValue (0.0052),    // TX_ON mode = 5.2mA
                   MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetTxOnCurrentA,
                                       &ZigbeeRadioEnergyModel::GetTxOnCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("BusyTxCurrentA",
                   "The radio BUSY_TX current in Ampere.",
                   DoubleValue (0.0138),    // max transmit power 4dBm = 13.8mA
                   MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetBusyTxCurrentA,
                                       &ZigbeeRadioEnergyModel::GetBusyTxCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("SleepCurrentA",
                   "The radio Sleep current in Ampere.",
                   DoubleValue (0.00000002),  // sleep mode = 20nA
                   MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetSleepCurrentA,
                                       &ZigbeeRadioEnergyModel::GetSleepCurrentA),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("ProcessorSleepCurrentA",
                  "The microprocessor sleep current in Ampere.",
                  DoubleValue (0.0000249),  // sleep mode = 24.9uA
                  MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetProcessorSleepCurrentA,
                                      &ZigbeeRadioEnergyModel::GetProcessorSleepCurrentA),
                  MakeDoubleChecker<double> ())
    .AddAttribute ("ProcessorActiveCurrentA",
                  "The microprocessor active current in Ampere.",
                  DoubleValue (0.0185),  // sleep mode = 18.5mA
                  MakeDoubleAccessor (&ZigbeeRadioEnergyModel::SetProcessorActiveCurrentA,
                                      &ZigbeeRadioEnergyModel::GetProcessorActiveCurrentA),
                  MakeDoubleChecker<double> ())
    .AddAttribute ("TxCurrentModel", "A pointer to the attached tx current model.",
                   PointerValue (),
                   MakePointerAccessor (&ZigbeeRadioEnergyModel::m_txCurrentModel),
                   MakePointerChecker<ZigbeeTxCurrentModel> ())
    .AddTraceSource ("TotalEnergyConsumption",
                     "Total energy consumption of the radio device.",
                     MakeTraceSourceAccessor (&ZigbeeRadioEnergyModel::m_totalEnergyConsumption),
                     "ns3::TracedValueCallback::Double")
  ; 
  return tid;
}

ZigbeeRadioEnergyModel::ZigbeeRadioEnergyModel ()
{
  NS_LOG_FUNCTION (this);
  m_currentState = IEEE_802_15_4_PHY_RX_ON;  // initially RX_ON
  m_lastUpdateTime = Seconds (0.0);
  m_nPendingChangeState = 0;
  m_isSupersededChangeState = false;
  m_energyDepletionCallback.Nullify ();
  m_source = NULL;
  // set callback for ZigbeePhy listener
  m_listener = new ZigbeeRadioEnergyModelPhyListener;
  m_listener->SetChangeStateCallback (MakeCallback (&DeviceEnergyModel::ChangeState, this));
  // set callback for updating the tx current
  m_listener->SetUpdateTxCurrentCallback (MakeCallback (&ZigbeeRadioEnergyModel::SetTxCurrentFromModel, this));
}

ZigbeeRadioEnergyModel::~ZigbeeRadioEnergyModel ()
{
  NS_LOG_FUNCTION (this);
  delete m_listener;
}

void
ZigbeeRadioEnergyModel::SetEnergySource (Ptr<EnergySource> source)
{
  NS_LOG_FUNCTION (this << source);
  NS_ASSERT (source != NULL);
  m_source = source;
}

double
ZigbeeRadioEnergyModel::GetTotalEnergyConsumption (void) const
{
  NS_LOG_FUNCTION (this);
  return m_totalEnergyConsumption;
}

double
ZigbeeRadioEnergyModel::GetTrxOffCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_trxOffCurrentA;
}

void
ZigbeeRadioEnergyModel::SetTrxOffCurrentA (double trxOffCurrentA)
{
  NS_LOG_FUNCTION (this << trxOffCurrentA);
  m_trxOffCurrentA = trxOffCurrentA;
}

double
ZigbeeRadioEnergyModel::GetRxOnCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_rxOnCurrentA;
}

void
ZigbeeRadioEnergyModel::SetRxOnCurrentA (double rxOnCurrentA)
{
  NS_LOG_FUNCTION (this << rxOnCurrentA);
  m_rxOnCurrentA = rxOnCurrentA;
}

double
ZigbeeRadioEnergyModel::GetBusyRxCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_busyRxCurrentA;
}

void
ZigbeeRadioEnergyModel::SetBusyRxCurrentA (double busyRxCurrentA)
{
  NS_LOG_FUNCTION (this << busyRxCurrentA);
  m_busyRxCurrentA = busyRxCurrentA;
}

double
ZigbeeRadioEnergyModel::GetTxOnCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_txOnCurrentA;
}

void
ZigbeeRadioEnergyModel::SetTxOnCurrentA (double txOnCurrentA)
{
  NS_LOG_FUNCTION (this << txOnCurrentA);
  m_txOnCurrentA = txOnCurrentA;
}

double
ZigbeeRadioEnergyModel::GetBusyTxCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_busyTxCurrentA;
}

void
ZigbeeRadioEnergyModel::SetBusyTxCurrentA (double busyTxCurrentA)
{
  NS_LOG_FUNCTION (this << busyTxCurrentA);
  m_busyTxCurrentA = busyTxCurrentA;
}

double
ZigbeeRadioEnergyModel::GetSleepCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_sleepCurrentA;
}

void
ZigbeeRadioEnergyModel::SetSleepCurrentA (double sleepCurrentA)
{
  NS_LOG_FUNCTION (this << sleepCurrentA);
  m_sleepCurrentA = sleepCurrentA;
}

double
ZigbeeRadioEnergyModel::GetProcessorSleepCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_processorSleepCurrentA;
}

void
ZigbeeRadioEnergyModel::SetProcessorSleepCurrentA (double sleepCurrentA)
{
  NS_LOG_FUNCTION (this << sleepCurrentA);
  m_processorSleepCurrentA = sleepCurrentA;
}

double
ZigbeeRadioEnergyModel::GetProcessorActiveCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  return m_processorActiveCurrentA;
}

void
ZigbeeRadioEnergyModel::SetProcessorActiveCurrentA (double activeCurrentA)
{
  NS_LOG_FUNCTION (this << activeCurrentA);
  m_processorActiveCurrentA = activeCurrentA;
}

ZigbeePhyEnumeration
ZigbeeRadioEnergyModel::GetCurrentState (void) const
{
  NS_LOG_FUNCTION (this);
  return m_currentState;
}

void
ZigbeeRadioEnergyModel::SetEnergyDepletionCallback (
  ZigbeeRadioEnergyDepletionCallback callback)
{
  NS_LOG_FUNCTION (this);
  if (callback.IsNull ())
    {
      NS_LOG_DEBUG ("ZigbeeRadioEnergyModel:Setting NULL energy depletion callback!");
    }
  m_energyDepletionCallback = callback;
}

void
ZigbeeRadioEnergyModel::SetEnergyRechargedCallback (
  ZigbeeRadioEnergyRechargedCallback callback)
{
  NS_LOG_FUNCTION (this);
  if (callback.IsNull ())
    {
      NS_LOG_DEBUG ("ZigbeeRadioEnergyModel:Setting NULL energy recharged callback!");
    }
  m_energyRechargedCallback = callback;
}

void
ZigbeeRadioEnergyModel::SetTxCurrentModel (Ptr<ZigbeeTxCurrentModel> model)
{
  m_txCurrentModel = model;
}

void
ZigbeeRadioEnergyModel::SetTxCurrentFromModel (double txPowerDbm)
{
  if (m_txCurrentModel)
    {
      m_busyTxCurrentA = m_txCurrentModel->CalcTxCurrent (txPowerDbm);
    }
}

void
ZigbeeRadioEnergyModel::ChangeState (int newState)
{
  NS_LOG_FUNCTION (this << newState);

  Time duration = Simulator::Now () - m_lastUpdateTime;
  NS_ASSERT (duration.GetNanoSeconds () >= 0); // check if duration is valid

  // calculate energy consumption = current * voltage * time
  double totalCurrent = 0.0;
  double supplyVoltage = m_source->GetSupplyVoltage ();
  switch (m_currentState)
    {
    case IEEE_802_15_4_PHY_BUSY_RX:
      totalCurrent = m_busyRxCurrentA + m_processorActiveCurrentA;
      break;
    case IEEE_802_15_4_PHY_RX_ON:
      totalCurrent = m_rxOnCurrentA + m_processorActiveCurrentA;
      break;
    case IEEE_802_15_4_PHY_BUSY_TX:
      totalCurrent = m_busyTxCurrentA + m_processorActiveCurrentA;
      break;
    case IEEE_802_15_4_PHY_TX_ON:
      totalCurrent = m_txOnCurrentA + m_processorActiveCurrentA;
      break;
    case IEEE_802_15_4_PHY_TRX_OFF:
      totalCurrent = m_trxOffCurrentA + m_processorActiveCurrentA;
      break;
 /* case SLEEP_MODE:
  *   totalCurrent = m_sleepCurrentA + m_processorSleepCurrentA;
  *   break; */
    default:
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModel:Invalid radio state: " << m_currentState);
    }

  // update total energy consumption
  m_totalEnergyConsumption += duration.GetSeconds () * totalCurrent * supplyVoltage;;

  // update last update time stamp
  m_lastUpdateTime = Simulator::Now ();

  m_nPendingChangeState++;

  // notify energy source
  m_source->UpdateEnergySource ();

  // in case the energy source is found to be depleted during the last update, a callback might be
  // invoked that might cause a change in the Zigbee PHY state (e.g., the PHY is put into SLEEP mode).
  // This in turn causes a new call to this member function, with the consequence that the previous
  // instance is resumed after the termination of the new instance. In particular, the state set
  // by the previous instance is erroneously the final state stored in m_currentState. The check below
  // ensures that previous instances do not change m_currentState.

  if (!m_isSupersededChangeState)
    {
      // update current state & last update time stamp
      SetZigbeeRadioState ((ZigbeePhyEnumeration) newState);

      // some debug message
      NS_LOG_DEBUG ("ZigbeeRadioEnergyModel:Total energy consumption is " <<
                    m_totalEnergyConsumption << "J");
    }

  m_isSupersededChangeState = (m_nPendingChangeState > 1);

  m_nPendingChangeState--;
}

void
ZigbeeRadioEnergyModel::HandleEnergyDepletion (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_WARN (std::fixed << std::setprecision (2) << Simulator::Now().GetSeconds()
      << "s ZigbeeRadioEnergyModel:Energy is depleted on Node "
      << m_source->GetNode ()->GetId () <<"!");
  // invoke energy depletion callback, if set.
  if (!m_energyDepletionCallback.IsNull ())
    {
      m_energyDepletionCallback ();
    }
}

void
ZigbeeRadioEnergyModel::HandleEnergyRecharged (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("ZigbeeRadioEnergyModel:Energy is recharged!");
  // invoke energy recharged callback, if set.
  if (!m_energyRechargedCallback.IsNull ())
    {
      m_energyRechargedCallback ();
    }
}

ZigbeeRadioEnergyModelPhyListener *
ZigbeeRadioEnergyModel::GetPhyListener (void)
{
  NS_LOG_FUNCTION (this);
  return m_listener;
}

/*
 * Private functions start here.
 */

void
ZigbeeRadioEnergyModel::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_source = NULL;
  m_energyDepletionCallback.Nullify ();
}

double
ZigbeeRadioEnergyModel::DoGetCurrentA (void) const
{
  NS_LOG_FUNCTION (this);
  switch (m_currentState)
    {
    case IEEE_802_15_4_PHY_BUSY_RX:
      return m_busyRxCurrentA + m_processorActiveCurrentA;
    case IEEE_802_15_4_PHY_RX_ON:
      return m_rxOnCurrentA + m_processorActiveCurrentA;
    case IEEE_802_15_4_PHY_BUSY_TX:
      return m_busyTxCurrentA + m_processorActiveCurrentA;
    case IEEE_802_15_4_PHY_TX_ON:
      return m_txOnCurrentA + m_processorActiveCurrentA;
    case IEEE_802_15_4_PHY_TRX_OFF:
      return m_trxOffCurrentA + m_processorActiveCurrentA;
 /* case SLEEP_MODE:
  *   return m_sleepCurrentA + m_processorSleepCurrentA;
  *   break; */
    default:
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModel:Invalid radio state:" << m_currentState);
    }
}

void
ZigbeeRadioEnergyModel::SetZigbeeRadioState (const ZigbeePhyEnumeration state)
{
  NS_LOG_FUNCTION (this << state);
  m_currentState = state;
  std::string stateName;
  switch (state)
    {
    case IEEE_802_15_4_PHY_BUSY_RX:
      stateName = "BUSY_RX";
      break;
    case IEEE_802_15_4_PHY_RX_ON:
      stateName = "RX_ON";
      break;
    case IEEE_802_15_4_PHY_BUSY_TX:
      stateName = "BUSY_TX";
      break;
    case IEEE_802_15_4_PHY_TX_ON:
      stateName = "TX_ON";
      break;
    case IEEE_802_15_4_PHY_TRX_OFF:
      stateName = "TRX_OFF";
      break;
/*  case SLEEP_MODE:
 *    stateName = "SLEEP";
 *    break; */
    default:
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModel:Invalid radio state:" << state);
    }
  NS_LOG_DEBUG ("ZigbeeRadioEnergyModel:Switching to state: " << stateName <<
                " at time = " << Simulator::Now ());
}


// -------------------------------------------------------------------------- //
// Class ZigbeeRadioEnergyModelPhyListener

ZigbeeRadioEnergyModelPhyListener::ZigbeeRadioEnergyModelPhyListener ()
{
  NS_LOG_FUNCTION (this);
  m_changeStateCallback.Nullify ();
  m_updateTxCurrentCallback.Nullify ();
}

ZigbeeRadioEnergyModelPhyListener::~ZigbeeRadioEnergyModelPhyListener ()
{
  NS_LOG_FUNCTION (this);
}

void
ZigbeeRadioEnergyModelPhyListener::SetChangeStateCallback (DeviceEnergyModel::ChangeStateCallback callback)
{
  NS_LOG_FUNCTION (this << &callback);
  NS_ASSERT (!callback.IsNull ());
  m_changeStateCallback = callback;
}

void
ZigbeeRadioEnergyModelPhyListener::SetUpdateTxCurrentCallback (UpdateTxCurrentCallback callback)
{
  NS_LOG_FUNCTION (this << &callback);
  NS_ASSERT (!callback.IsNull ());
  m_updateTxCurrentCallback = callback;
}

void
ZigbeeRadioEnergyModelPhyListener::NotifyRxStart (Time duration)
{
  NS_LOG_FUNCTION (this << duration);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (IEEE_802_15_4_PHY_BUSY_RX);
}

void
ZigbeeRadioEnergyModelPhyListener::NotifyRxEnd (ZigbeePhyEnumeration nextState)
{
  NS_LOG_FUNCTION (this << nextState);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (nextState);
}

void
ZigbeeRadioEnergyModelPhyListener::NotifyTxStart (Time duration, double txPowerDbm)
{
  NS_LOG_FUNCTION (this << duration << txPowerDbm);
  if (m_updateTxCurrentCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Update tx current callback not set!");
    }
  m_updateTxCurrentCallback (txPowerDbm);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (IEEE_802_15_4_PHY_BUSY_TX);
}

void
ZigbeeRadioEnergyModelPhyListener::NotifyTxEnd (ZigbeePhyEnumeration nextState)
{
  NS_LOG_FUNCTION (this << nextState);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (nextState);
}

void
ZigbeeRadioEnergyModelPhyListener::NotifySleep (void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  NS_LOG_INFO("Notify a change into SLEEP mode, but PHY doesn't support sleeping.");
  //m_changeStateCallback (SLEEP_MODE);
}

void
ZigbeeRadioEnergyModelPhyListener::NotifyWakeup(void)
{
  NS_LOG_FUNCTION (this);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (IEEE_802_15_4_PHY_TRX_OFF);
}

void
ZigbeeRadioEnergyModelPhyListener::NotifyChangeState (ZigbeePhyEnumeration nextState)
{
  NS_LOG_FUNCTION (this << nextState);
  if (m_changeStateCallback.IsNull ())
    {
      NS_FATAL_ERROR ("ZigbeeRadioEnergyModelPhyListener:Change state callback not set!");
    }
  m_changeStateCallback (nextState);
}

} // namespace ns3
