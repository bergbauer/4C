/*----------------------------------------------------------------------------*/
/*! \file
\brief Nitsche ssi contact solving strategy including electrochemistry

\level 3

*/
/*----------------------------------------------------------------------------*/
#ifndef BACI_CONTACT_NITSCHE_STRATEGY_SSI_ELCH_HPP
#define BACI_CONTACT_NITSCHE_STRATEGY_SSI_ELCH_HPP

#include "baci_config.hpp"

#include "baci_contact_nitsche_strategy_ssi.hpp"

BACI_NAMESPACE_OPEN

namespace CONTACT
{
  /*!
   * @brief Contact solving strategy with Nitsche's method.
   *
   * This is a specialization of the abstract contact algorithm as defined in AbstractStrategy.
   * For a more general documentation of the involved functions refer to CONTACT::AbstractStrategy.
   */
  class NitscheStrategySsiElch : public NitscheStrategySsi
  {
   public:
    //! Shared data constructor
    NitscheStrategySsiElch(const Teuchos::RCP<CONTACT::AbstractStratDataContainer>& data_ptr,
        const Epetra_Map* DofRowMap, const Epetra_Map* NodeRowMap,
        const Teuchos::ParameterList& params,
        std::vector<Teuchos::RCP<CONTACT::Interface>> interface, int dim,
        const Teuchos::RCP<const Epetra_Comm>& comm, double alphaf, int maxdof)
        : NitscheStrategySsi(data_ptr, DofRowMap, NodeRowMap, params, std::move(interface), dim,
              comm, alphaf, maxdof)
    {
    }

    void ApplyForceStiffCmt(Teuchos::RCP<Epetra_Vector> dis,
        Teuchos::RCP<CORE::LINALG::SparseOperator>& kt, Teuchos::RCP<Epetra_Vector>& f,
        const int step, const int iter, bool predictor) override
    {
      dserror("not implemented");
    }

    void Integrate(const CONTACT::ParamsInterface& cparams) override;

    //! don't want = operator
    NitscheStrategySsiElch operator=(const NitscheStrategySsiElch& old) = delete;

    //! don't want copy constructor
    NitscheStrategySsiElch(const NitscheStrategySsiElch& old) = delete;
  };
}  // namespace CONTACT
BACI_NAMESPACE_CLOSE

#endif  // CONTACT_NITSCHE_STRATEGY_SSI_ELCH_H