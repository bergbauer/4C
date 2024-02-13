/*---------------------------------------------------------------------------*/
/*! \file
\brief one way coupled partitioned algorithm for particle structure interaction
\level 3
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
#ifndef BACI_PASI_PARTITIONED_ONEWAYCOUP_HPP
#define BACI_PASI_PARTITIONED_ONEWAYCOUP_HPP

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "baci_config.hpp"

#include "baci_pasi_partitioned.hpp"

BACI_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | class declarations                                                        |
 *---------------------------------------------------------------------------*/
namespace PASI
{
  /*!
   * \brief one way coupled partitioned algorithm
   *
   * One way coupled partitioned particle structure interaction algorithm with structure to particle
   * coupling of the interface states.
   *
   * \author Sebastian Fuchs \date 02/2017
   */
  class PASI_PartOneWayCoup : public PartitionedAlgo
  {
   public:
    /*!
     * \brief constructor
     *
     * \author Sebastian Fuchs \date 02/2017
     *
     * \param[in] comm   communicator
     * \param[in] params particle structure interaction parameter list
     */
    explicit PASI_PartOneWayCoup(const Epetra_Comm& comm, const Teuchos::ParameterList& params);

    /*!
     * \brief setup pasi algorithm
     *
     * \author Sebastian Fuchs \date 02/2017
     */
    void Setup() override;

    /*!
     * \brief partitioned one way coupled timeloop
     *
     * \author Sebastian Fuchs \date 02/2017
     */
    void Timeloop() override;

   private:
    /*!
     * \brief output of fields
     *
     * \author Sebastian Fuchs \date 09/2019
     */
    void Output() override;
  };

}  // namespace PASI

/*---------------------------------------------------------------------------*/
BACI_NAMESPACE_CLOSE

#endif