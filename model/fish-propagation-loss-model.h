#ifndef FISH_PROPAGATION_LOSS_MODEL_H
#define FISH_PROPAGATION_LOSS_MODEL_H

#include "ns3/propagation-loss-model.h"
#include "ns3/position-allocator.h"
#include "ns3/boolean.h"

namespace ns3 {


/**
 * \ingroup propagation
 *
 * \brief Attenuate signals by a constant amount.
 *
 * The loss must be set through a public SetLoss() method or the
 * Loss attribute.
 */
class FishFixedLossModel : public PropagationLossModel
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  FishFixedLossModel ();

  virtual ~FishFixedLossModel ();

  /**
   * \param loss (dB) the link loss
   *
   * Set the loss in dB.
   */
  void SetLoss (double loss);

private:
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  FishFixedLossModel (const FishFixedLossModel &);

  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   * \returns
   */
  FishFixedLossModel & operator = (const FishFixedLossModel &);

  virtual double DoCalcRxPower (double txPowerDbm,
                                Ptr<MobilityModel> a,
                                Ptr<MobilityModel> b) const;

  virtual int64_t DoAssignStreams (int64_t stream);

  double m_loss; //!< the loss
};

/**
 * \ingroup propagation
 *
 * \brief a custom propagation model.
 *
 * This model lets the attenuation of each link to be
 * specified manually.
 *
 * There are no additional loss calculations done, the
 * loss in the lookupTable is the total link loss
 */
class FishCustomLossModel : public PropagationLossModel
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  FishCustomLossModel ();
  FishCustomLossModel (std::vector<Vector> mapPosToIndex, double **lookupTableDb);

  ~FishCustomLossModel ();

private:
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  FishCustomLossModel (const FishCustomLossModel &);
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   * \returns
   */
  FishCustomLossModel & operator = (const FishCustomLossModel &);

  virtual double DoCalcRxPower (double txPowerDbm,
                                Ptr<MobilityModel> a,
                                Ptr<MobilityModel> b) const;

  virtual int64_t DoAssignStreams (int64_t stream);

  std::vector<Vector> m_mapPosToIndex;    //!< Maps the node positions to an index for the lookup table
  double  **m_lookupTableDb;              //!< Lookup table for all path loss exponents between nodes
};

/**
 * \ingroup propagation
 *
 * \brief a log distance propagation model.
 *
 * This model calculates the reception power with a so-called
 * log-distance propagation model:
 * \f$ L = L_0 + 10 n log_{10}(\frac{d}{d_0})\f$
 *
 * where:
 *  - \f$ n \f$ : the path loss distance exponent
 *  - \f$ d_0 \f$ : reference distance (m)
 *  - \f$ L_0 \f$ : path loss at reference distance (dB)
 *  - \f$ d \f$ : distance (m)
 *  - \f$ L \f$ : path loss (dB)
 *
 * When the path loss is requested at a distance smaller than
 * the reference distance, the tx power is returned.
 *
 * The model also allows for shadowing to be included in the path loss.
 *
 */
class FishLogDistanceLossModel : public PropagationLossModel
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  FishLogDistanceLossModel ();

  // Using this constructor implies a stationary network (shadowing doesn't change)
  FishLogDistanceLossModel (Ptr<ListPositionAllocator> positionAlloc, uint16_t numNodes, double shadowingStd);

  /** Set the node positions and generate shadowing values for them.
   *
   * @param positionAlloc Node positions.
   * @param numNodes Number of nodes.
   * @param shadowingStd Shadowing standard deviation.
   */
  void GenerateNewShadowingValues (Ptr<ListPositionAllocator> positionAlloc, uint16_t numNodes, double shadowingStd);




private:
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  FishLogDistanceLossModel (const FishLogDistanceLossModel &);
  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   * \returns
   */
  FishLogDistanceLossModel & operator = (const FishLogDistanceLossModel &);

  virtual double DoCalcRxPower (double txPowerDbm,
                                Ptr<MobilityModel> a,
                                Ptr<MobilityModel> b) const;
  virtual int64_t DoAssignStreams (int64_t stream);

  /**
   *  Creates a default reference loss model
   * \return a default reference loss model
   */
  static Ptr<PropagationLossModel> CreateDefaultReference (void);

  double m_exponent; //!< model exponent
  double m_referenceDistance; //!< reference distance
  double m_referenceLoss; //!< reference loss
  double m_shadowingStD; //!< shadowing standard deviation
  bool m_isStationary;  ///< Indicates if the network is stationary or not

  Ptr<NormalRandomVariable> m_normDist; ///< the normal distribution used for shadowing
  std::map<std::string, double> m_shadowingLookup; ///< Used for stationary networks to store the shadowing gain
};


}
#endif
