/*---------------------------------------------------------------------------*/
/*! \file
\brief gravity acceleration handler for particle simulations
\level 2
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
#ifndef FOUR_C_PARTICLE_ALGORITHM_GRAVITY_HPP
#define FOUR_C_PARTICLE_ALGORITHM_GRAVITY_HPP

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "baci_config.hpp"

#include <Teuchos_ParameterList.hpp>

#include <memory>

BACI_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | class declarations                                                        |
 *---------------------------------------------------------------------------*/
namespace PARTICLEALGORITHM
{
  /*!
   * \brief gravity acceleration handler for particle simulations
   *
   * \author Sebastian Fuchs \date 05/2018
   */
  class GravityHandler
  {
   public:
    /*!
     * \brief constructor
     *
     * \author Sebastian Fuchs \date 05/2018
     *
     * \param[in] params particle simulation parameter list
     */
    explicit GravityHandler(const Teuchos::ParameterList& params);

    /*!
     * \brief init gravity handler
     *
     * \author Sebastian Fuchs \date 05/2018
     *
     * \param[in] gravity gravity acceleration
     */
    void Init(const std::vector<double>& gravity);

    /*!
     * \brief setup gravity handler
     *
     * \author Sebastian Fuchs \date 05/2018
     */
    void Setup();

    /*!
     * \brief get gravity acceleration
     *
     * Evaluate the gravity ramp function at the given time to get the scaled gravity acceleration.
     *
     * \author Sebastian Fuchs \date 05/2018
     *
     * \param[in]  time           evaluation time
     * \param[out] scaled_gravity scaled gravity acceleration
     */
    void GetGravityAcceleration(const double time, std::vector<double>& scaled_gravity);

   protected:
    //! particle simulation parameter list
    const Teuchos::ParameterList& params_;

    //! gravity acceleration vector
    std::vector<double> gravity_;

    //! gravity ramp function number
    const int gravityrampfctnumber_;
  };

}  // namespace PARTICLEALGORITHM

/*---------------------------------------------------------------------------*/
BACI_NAMESPACE_CLOSE

#endif
