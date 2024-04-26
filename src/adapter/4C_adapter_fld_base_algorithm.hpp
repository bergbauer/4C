/*----------------------------------------------------------------------*/
/*! \file

\brief Fluid Base Algorithm

\level 1


*/
/*----------------------------------------------------------------------*/



#ifndef FOUR_C_ADAPTER_FLD_BASE_ALGORITHM_HPP
#define FOUR_C_ADAPTER_FLD_BASE_ALGORITHM_HPP

#include "4C_config.hpp"

#include "4C_adapter_fld_fluid.hpp"

#include <Epetra_Map.h>
#include <Epetra_Operator.h>
#include <Epetra_Vector.h>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

namespace ADAPTER
{
  /// fluid field solver
  class FluidBaseAlgorithm
  {
   public:
    /// constructor which distinguishes different discretizations for different fluids in
    /// multi-fluid field problems // rauch 09/13
    explicit FluidBaseAlgorithm(const Teuchos::ParameterList& prbdyn,
        const Teuchos::ParameterList& fdyn, const std::string& disname, bool isale,
        bool init = true);  // initialize time-integration scheme immediately
    // remark: parameter init allows for distinguishing an immediate initialization of all members
    // and state vectors and a
    //         later initialization which enables a later modifications of the maps

    /// second constructor (special version for turbulent flows with separate inflow
    /// section for generation of turbulent inflow profiles)
    explicit FluidBaseAlgorithm(
        const Teuchos::ParameterList& prbdyn, const Teuchos::RCP<DRT::Discretization> discret);

    /// virtual destructor to support polymorph destruction
    virtual ~FluidBaseAlgorithm() = default;

    /// access to fluid field solver
    const Teuchos::RCP<Fluid>& FluidField() { return fluid_; }

    /// set the initial flow field in the fluid
    void SetInitialFlowField(const Teuchos::ParameterList& fdyn);

   private:
    //! setup fluid algorithm (overriding some fluid parameters with
    //! values specified in given problem-dependent ParameterList)
    /**
     * \note In this function the linear solver object is generated. For pure fluid problems or
     * fluid meshtying (no block matrix) the FLUID SOLVER block from the 4C dat file is used.
     * For fluid meshtying (block matrix) the MESHTYING SOLVER block is used as main solver object
     * with a block preconditioner (BGS or SIMPLE type). The block preconditioners use the
     * information form the FLUID SOLVER and the FLUID PRESSURE SOLVER block for the velocity and
     * pressure dofs.
     */
    void SetupFluid(const Teuchos::ParameterList& prbdyn, const Teuchos::ParameterList& fdyn,
        const std::string& disname, bool isale, bool init);

    /// set the initial turbulent inflow field in the fluid
    void SetInitialInflowField(const Teuchos::ParameterList& fdyn);

    /// setup second fluid algorithm (overriding some fluid parameters with
    /// values specified in given problem-dependent Turbulent Inflow ParameterList)
    /// separate discretization for inflow generation
    void SetupInflowFluid(
        const Teuchos::ParameterList& prbdyn, const Teuchos::RCP<DRT::Discretization> discret);

    //! set parameters in list required for all schemes
    void SetGeneralParameters(const Teuchos::RCP<Teuchos::ParameterList> fluidtimeparams,
        const Teuchos::ParameterList& prbdyn, const Teuchos::ParameterList& fdyn);

    //! create a second solver
    void CreateSecondSolver(
        const Teuchos::RCP<CORE::LINALG::Solver> solver, const Teuchos::ParameterList& fdyn);

    /// fluid field solver
    Teuchos::RCP<Fluid> fluid_;
  };

}  // namespace ADAPTER

FOUR_C_NAMESPACE_CLOSE

#endif
