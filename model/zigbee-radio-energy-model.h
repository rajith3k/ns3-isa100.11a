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

#ifndef ZIGBEE_RADIO_ENERGY_MODEL_H
#define ZIGBEE_RADIO_ENERGY_MODEL_H

#include "ns3/device-energy-model.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/zigbee-phy.h"

namespace ns3 {

class ZigbeeTxCurrentModel;

/**
 * \ingroup energy
 * A ZigbeePhy listener class for notifying the ZigbeeRadioEnergyModel of Zigbee radio
 * state change.
 *
 */
class ZigbeeRadioEnergyModelPhyListener : public ZigbeePhyListener
{
public:
  /**
   * Callback type for updating the transmit current based on the nominal tx power.
   */
  typedef Callback<void, double> UpdateTxCurrentCallback;

  ZigbeeRadioEnergyModelPhyListener ();
  virtual ~ZigbeeRadioEnergyModelPhyListener ();

  /**
   * \brief Sets the change state callback. Used by helper class.
   *
   * \param callback Change state callback.
   */
  void SetChangeStateCallback (DeviceEnergyModel::ChangeStateCallback callback);

  /**
   * \brief Sets the update tx current callback.
   *
   * \param callback Update tx current callback.
   */
  void SetUpdateTxCurrentCallback (UpdateTxCurrentCallback callback);

  /**
   * \brief Switches the ZigbeeRadioEnergyModel to RX state.
   *
   * \param duration the expected duration of the packet reception.
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifyRxStart (Time duration);

  /**
   * \brief Switches the ZigbeeRadioEnergyModel to the state following the packet reception.
   *
   * \param nextState The state that the transceiver switches into following packet reception
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifyRxEnd (ZigbeePhyEnumeration nextState);

  /**
   * \brief Switches the ZigbeeRadioEnergyModel to TX state
   *
   * \param duration the expected transmission duration.
   * \param txPowerDbm the nominal tx power in dBm
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifyTxStart (Time duration, double txPowerDbm);

  /**
   * \brief Switches the ZigbeeRadioEnergyModel to the state following the packet transmission.
   *
   * \param nextState The state that the transceiver switches into following transmission
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifyTxEnd (ZigbeePhyEnumeration nextState);

  /**
   * \brief Switches the ZigbeeRadioEnergy Model to the sleeping state
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifySleep (void);

  /**
   * \brief Switches the ZigbeeRadioEnergy Model from sleep to transceiver off (TRX_OFF)
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifyWakeup (void);

  /**
   * \brief Switches the ZigbeeRadioEnergy Model into the specified nextState
   *
   * \param nextState The state that the transceiver is switching into
   *
   * Defined in ns3::ZigbeePhyListener
   */
  virtual void NotifyChangeState (ZigbeePhyEnumeration nextState);

private:
  /**
   * Change state callback used to notify the ZigbeeRadioEnergyModel of a state
   * change.
   */
  DeviceEnergyModel::ChangeStateCallback m_changeStateCallback;

  /**
   * Callback used to update the tx current stored in ZigbeeRadioEnergyModel based on
   * the nominal tx power used to transmit the current frame.
   */
  UpdateTxCurrentCallback m_updateTxCurrentCallback;
};

// -------------------------------------------------------------------------- //

/**
 * \ingroup energy
 * \brief A ZigBee radio energy model.
 * 
 * 5 states are defined for the radio: TX_ON, BUSY_TX, RX_ON, BUSY_RX, TRX_OFF
 * (and SLEEP is not yet fully supported).
 * The different types of states that are defined are:
 *  1. TX_ON:   Transmitter circuitry is enabled and radio is ready to transmit.
 *  2. BUSY_TX: Radio is transmitting for TX_duration, then state goes back to
 *                  either TX_ON or a pending state.
 *  3. RX_ON:   Receiver circuitry is enabled and radio is ready to receive.
 *  2. BUSY_RX: Radio is receiving for RX_duration, then state goes back to
 *                  either RX_ON or a pending state.
 *  5. TRX_OFF: Transmitting and receiving circuits are disabled.
 * The class keeps track of what state the radio is currently in.
 *
 * Energy calculation: For each transaction, this model notifies EnergySource
 * object. The EnergySource object will query this model for the total current.
 * Then the EnergySource object uses the total current to calculate energy.
 *
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
 *
 * The dependence of the power consumption in transmission mode on the nominal
 * transmit power can also be achieved through a wifi tx current model.
 *
 */
class ZigbeeRadioEnergyModel : public DeviceEnergyModel
{
public:
  /**
   * Callback type for energy depletion handling.
   */
  typedef Callback<void> ZigbeeRadioEnergyDepletionCallback;

  /**
   * Callback type for energy recharged handling.
   */
  typedef Callback<void> ZigbeeRadioEnergyRechargedCallback;

public:
  static TypeId GetTypeId (void);
  ZigbeeRadioEnergyModel ();
  virtual ~ZigbeeRadioEnergyModel ();

  /**
   * \brief Sets pointer to EnergySouce installed on node.
   *
   * \param source Pointer to EnergySource installed on node.
   *
   * Implements DeviceEnergyModel::SetEnergySource.
   */
  virtual void SetEnergySource (Ptr<EnergySource> source);

  /**
   * \returns Total energy consumption of the isa100 device.
   *
   * Implements DeviceEnergyModel::GetTotalEnergyConsumption.
   */
  virtual double GetTotalEnergyConsumption (void) const;

  // Setter & getters for state power consumption.
  double GetTrxOffCurrentA (void) const;
  void SetTrxOffCurrentA (double trxOffCurrentA);
  double GetRxOnCurrentA (void) const;
  void SetRxOnCurrentA (double rxOnCurrentA);
  double GetBusyRxCurrentA (void) const;
  void SetBusyRxCurrentA (double rxOnCurrentA);
  double GetTxOnCurrentA (void) const;
  void SetTxOnCurrentA (double txOnCurrentA);
  double GetBusyTxCurrentA (void) const;
  void SetBusyTxCurrentA (double busyTxCurrentA);
  double GetSleepCurrentA (void) const;
  void SetSleepCurrentA (double sleepCurrentA);

  double GetProcessorSleepCurrentA (void) const;
  void SetProcessorSleepCurrentA (double sleepCurrentA);
  double GetProcessorActiveCurrentA (void) const;
  void SetProcessorActiveCurrentA (double activeCurrentA);

  /**
   * \returns Current state.
   */
  ZigbeePhyEnumeration GetCurrentState (void) const;

  /**
   * \param callback Callback function.
   *
   * Sets callback for energy depletion handling.
   */
  void SetEnergyDepletionCallback (ZigbeeRadioEnergyDepletionCallback callback);

  /**
   * \param callback Callback function.
   *
   * Sets callback for energy recharged handling.
   */
  void SetEnergyRechargedCallback (ZigbeeRadioEnergyRechargedCallback callback);

  /**
   * \param model the model used to compute the Zigbee tx current.
   */
  void SetTxCurrentModel (Ptr<ZigbeeTxCurrentModel> model);

  /**
   * \brief Calls the CalcTxCurrent method of the tx current model to
   *        compute the tx current based on such model
   * 
   * \param txPowerDbm the nominal tx power in dBm
   */
  void SetTxCurrentFromModel (double txPowerDbm);

  /**
   * \brief Changes state of the ZigbeeRadioEnergyMode.
   *
   * \param newState New state the Zigbee radio is in.
   *
   * Implements DeviceEnergyModel::ChangeState.
   */
  virtual void ChangeState (int newState);

  /**
   * \brief Handles energy depletion.
   *
   * Implements DeviceEnergyModel::HandleEnergyDepletion
   */
  virtual void HandleEnergyDepletion (void);

  /**
   * \brief Handles energy recharged.
   *
   * Implements DeviceEnergyModel::HandleEnergyRecharged
   */
  virtual void HandleEnergyRecharged (void);

  /**
   * \returns Pointer to the PHY listener.
   */
  ZigbeeRadioEnergyModelPhyListener * GetPhyListener (void);


private:
  void DoDispose (void);

  /**
   * \returns Current draw of device, at current state.
   *
   * Implements DeviceEnergyModel::GetCurrentA.
   */
  virtual double DoGetCurrentA (void) const;

  /**
   * \param state New state the radio device is currently in.
   *
   * Sets current state. This function is private so that only the energy model
   * can change its own state.
   */
  void SetZigbeeRadioState (const ZigbeePhyEnumeration state);

private:
  Ptr<EnergySource> m_source;

  // Member variables for current draw in different radio modes.
  double m_trxOffCurrentA;
  double m_rxOnCurrentA;
  double m_busyRxCurrentA;
  double m_txOnCurrentA;
  double m_busyTxCurrentA;
  double m_sleepCurrentA;
  Ptr<ZigbeeTxCurrentModel> m_txCurrentModel;

  // Member variables for current draw of microprocessor in different modes.
  double m_processorSleepCurrentA;
  double m_processorActiveCurrentA;

  // This variable keeps track of the total energy consumed by this model.
  TracedValue<double> m_totalEnergyConsumption;

  // State variables.
  ZigbeePhyEnumeration m_currentState;  // current state the radio is in
  Time m_lastUpdateTime;                // time stamp of previous energy update

  uint8_t m_nPendingChangeState;
  bool m_isSupersededChangeState;

  // Energy depletion callback
  ZigbeeRadioEnergyDepletionCallback m_energyDepletionCallback;

  // Energy recharged callback
  ZigbeeRadioEnergyRechargedCallback m_energyRechargedCallback;

  // ZigbeePhy listener
  ZigbeeRadioEnergyModelPhyListener *m_listener;
};

} // namespace ns3

#endif /* ZIGBEE_RADIO_ENERGY_MODEL_H */
