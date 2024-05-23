/*----------------------------------------------------------------------*/
/*! \file

\level 2

*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 | headers                                                  rauch 12/12 |
 *----------------------------------------------------------------------*/
#include "4C_fpsi.hpp"

#include "4C_fpsi_utils.hpp"
#include "4C_global_data.hpp"
#include "4C_lib_discret.hpp"
#include "4C_poroelast_utils.hpp"

FOUR_C_NAMESPACE_OPEN

FPSI::FpsiBase::FpsiBase(const Epetra_Comm& comm, const Teuchos::ParameterList& fpsidynparams)
    : AlgorithmBase(comm, fpsidynparams)
{
  // nothing to do ... so far
}


/*----------------------------------------------------------------------*
 | redistribute the FPSI interface                           thon 11/14 |
 *----------------------------------------------------------------------*/
void FPSI::FpsiBase::redistribute_interface()
{
  GLOBAL::Problem* problem = GLOBAL::Problem::Instance();
  const Epetra_Comm& comm = problem->GetDis("structure")->Comm();
  Teuchos::RCP<FPSI::Utils> FPSI_UTILS = FPSI::Utils::Instance();

  if (comm.NumProc() >
      1)  // if we have more than one processor, we need to redistribute at the FPSI interface
  {
    Teuchos::RCP<std::map<int, int>> Fluid_PoroFluid_InterfaceMap =
        FPSI_UTILS->get_fluid_poro_fluid_interface_map();
    Teuchos::RCP<std::map<int, int>> PoroFluid_Fluid_InterfaceMap =
        FPSI_UTILS->get_poro_fluid_fluid_interface_map();

    FPSI_UTILS->redistribute_interface(problem->GetDis("fluid"), problem->GetDis("porofluid"),
        "FPSICoupling", *PoroFluid_Fluid_InterfaceMap);
    FPSI_UTILS->redistribute_interface(problem->GetDis("ale"), problem->GetDis("porofluid"),
        "FPSICoupling", *PoroFluid_Fluid_InterfaceMap);
    FPSI_UTILS->redistribute_interface(problem->GetDis("porofluid"), problem->GetDis("fluid"),
        "FPSICoupling", *Fluid_PoroFluid_InterfaceMap);
    FPSI_UTILS->redistribute_interface(problem->GetDis("structure"), problem->GetDis("fluid"),
        "FPSICoupling", *Fluid_PoroFluid_InterfaceMap);

    // Material pointers need to be reset after redistribution.
    POROELAST::UTILS::SetMaterialPointersMatchingGrid(
        problem->GetDis("structure"), problem->GetDis("porofluid"));
  }

  return;
}

FOUR_C_NAMESPACE_CLOSE
