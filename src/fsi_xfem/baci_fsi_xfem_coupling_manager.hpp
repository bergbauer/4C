/*----------------------------------------------------------------------*/
/*! \file
\brief  Pure Virtual Coupling Manager, to define the basic functionality of all specified coupling
managers

\level 2


*----------------------------------------------------------------------*/

#include "baci_config.hpp"

#include <Epetra_Vector.h>
#include <Teuchos_ParameterList.hpp>

#ifndef BACI_FSI_XFEM_COUPLING_MANAGER_HPP
#define BACI_FSI_XFEM_COUPLING_MANAGER_HPP

BACI_NAMESPACE_OPEN

// forward declarations
namespace CORE::LINALG
{
  class BlockSparseMatrixBase;
  class MultiMapExtractor;
}  // namespace CORE::LINALG

namespace IO
{
  class DiscretizationWriter;
  class DiscretizationReader;
}  // namespace IO

namespace XFEM
{
  class Coupling_Manager
  {
   public:
    //! @name Destruction
    //@{

    /// virtual to get polymorph destruction
    virtual ~Coupling_Manager() = default;
    //! predict states in the coupling object
    virtual void PredictCouplingStates() = 0;

    //! Set required states in the coupling object
    virtual void SetCouplingStates() = 0;

    //! Initializes the couplings (done at the beginning of the algorithm after fields have their
    //! state for timestep n)
    virtual void InitCouplingStates() = 0;

    //! Add the coupling matrixes to the global systemmatrix
    // in ... idx[0] first discretization index , idx[1] second discretization index, ... in the
    // blockmatrix in ... scaling between xfluid evaluated coupling matrixes and coupled
    // systemmatrix
    virtual void AddCouplingMatrix(
        CORE::LINALG::BlockSparseMatrixBase& systemmatrix, double scaling) = 0;

    //! Add the coupling rhs
    // in ... idx[0] first discretization index , idx[1] second discretization index, ... in the
    // blockmatrix in ... scaling between xfluid evaluated coupling matrixes and coupled
    // systemmatrix
    virtual void AddCouplingRHS(Teuchos::RCP<Epetra_Vector> rhs,
        const CORE::LINALG::MultiMapExtractor& me, double scaling) = 0;

    //! Update (Perform after Each Timestep)
    virtual void Update(double scaling) = 0;

    //! Write Output (For restart or write results on the interface)
    virtual void Output(IO::DiscretizationWriter& writer) = 0;

    //! Read Restart (For quantities stored on the interface)
    virtual void ReadRestart(IO::DiscretizationReader& reader) = 0;
  };
}  // namespace XFEM

BACI_NAMESPACE_CLOSE

#endif