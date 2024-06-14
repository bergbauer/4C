/*---------------------------------------------------------------------------*/
/*! \file
\brief interface to provide restricted access to rigid body handler
\level 2
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
#ifndef FOUR_C_PARTICLE_RIGIDBODY_INTERFACE_HPP
#define FOUR_C_PARTICLE_RIGIDBODY_INTERFACE_HPP

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "4C_config.hpp"

#include <memory>
#include <vector>

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | forward declarations                                                      |
 *---------------------------------------------------------------------------*/
namespace ParticleRigidBody
{
  class RigidBodyDataState;
}

/*---------------------------------------------------------------------------*
 | class declarations                                                        |
 *---------------------------------------------------------------------------*/
namespace ParticleRigidBody
{
  /*!
   * \brief interface to provide restricted access to rigid body handler
   *
   * \author Sebastian Fuchs \date 09/2020
   */
  class RigidBodyHandlerInterface
  {
   public:
    //! virtual destructor
    virtual ~RigidBodyHandlerInterface() = default;

    /*!
     * \brief update positions with given time increment
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \param[in] timeincrement time increment
     */
    virtual void UpdatePositions(const double timeincrement) = 0;

    /*!
     * \brief update velocities with given time increment
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \param[in] timeincrement time increment
     */
    virtual void UpdateVelocities(const double timeincrement) = 0;

    /*!
     * \brief clear accelerations
     *
     * \author Sebastian Fuchs \date 09/2020
     */
    virtual void ClearAccelerations() = 0;

    /*!
     * \brief get rigid body data state container
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \return rigid body data state container
     */
    virtual std::shared_ptr<ParticleRigidBody::RigidBodyDataState> get_rigid_body_data_state()
        const = 0;

    /*!
     * \brief get owned rigid bodies by this processor
     *
     * \author Sebastian Fuchs \date 09/2020
     *
     * \return owned rigid bodies by this processor
     */
    virtual const std::vector<int>& GetOwnedRigidBodies() const = 0;
  };

}  // namespace ParticleRigidBody

/*---------------------------------------------------------------------------*/
FOUR_C_NAMESPACE_CLOSE

#endif