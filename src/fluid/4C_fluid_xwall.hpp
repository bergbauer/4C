/*-----------------------------------------------------------*/
/*! \file

\brief Implementation of enrichment-based wall model


\level 2

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_FLUID_XWALL_HPP
#define FOUR_C_FLUID_XWALL_HPP

#include "4C_config.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_inpar_fluid.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"

#include <Epetra_Vector.h>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_RCP.hpp>

FOUR_C_NAMESPACE_OPEN

// forward declarations
namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace Core::LinAlg
{
  class SparseMatrix;
  class Solver;
}  // namespace Core::LinAlg

namespace Core::IO
{
  class DiscretizationReader;
}

namespace FLD
{
  class FluidImplicitTimeInt;
  class TransferTurbulentInflowConditionNodal;
  namespace UTILS
  {
    class StressManager;
  }

  class XWall
  {
   public:
    /// Standard Constructor
    XWall(Teuchos::RCP<Core::FE::Discretization> dis, int nsd,
        Teuchos::RCP<Teuchos::ParameterList>& params,
        Teuchos::RCP<Core::LinAlg::MapExtractor> dbcmaps,
        Teuchos::RCP<FLD::UTILS::StressManager> wssmanager);

    /// Destructor
    virtual ~XWall() = default;

    // set element params for xwall EnrichmentType
    virtual void SetXWallParams(Teuchos::ParameterList& eleparams);

    // adapt ml nullspace for aggregation for scale separation (mfs)
    void AdaptMLNullspace(const Teuchos::RCP<Core::LinAlg::Solver>& solver);

    // get output vector of enriched dofs
    Teuchos::RCP<Epetra_Vector> GetOutputVector(Teuchos::RCP<Epetra_Vector> vel);

    // returns, if Properties for GenAlpha have to be updated
    virtual void UpdateTauW(int step, Teuchos::RCP<Epetra_Vector> trueresidual, int itnum,
        Teuchos::RCP<Epetra_Vector> accn, Teuchos::RCP<Epetra_Vector> velnp,
        Teuchos::RCP<Epetra_Vector> veln);

    // returns tauw of discret_
    Teuchos::RCP<Epetra_Vector> GetTauw();

    Teuchos::RCP<Epetra_Vector> GetTauwVector()
    {
      Teuchos::RCP<Epetra_Vector> tauw =
          Teuchos::rcp(new Epetra_Vector(*(discret_->NodeRowMap()), true));
      Core::LinAlg::Export(*tauw_, *tauw);
      return tauw;
    }

    // read restart including wall stresses
    void read_restart(Core::IO::DiscretizationReader& reader);

    // fix residual at Dirichlet-inflow nodes such that the wss can be calculated
    Teuchos::RCP<Epetra_Vector> FixDirichletInflow(Teuchos::RCP<Epetra_Vector> trueresidual);

    // set current non-linear iteration number
    void SetIter(int iter)
    {
      iter_ = iter;
      return;
    }

   protected:
    // set element params for xwall EnrichmentType, distributed for xwall discretization
    virtual void set_x_wall_params_xw_dis(Teuchos::ParameterList& eleparams);

    // setup XWall
    void setup();

    // initialize some maps
    void init_x_wall_maps();

    // initialize a toggle vector for the element
    void init_toggle_vector();

    // initialize wall distance
    void init_wall_dist();

    // setup xwall discretization
    void setup_x_wall_dis();

    // setup l2 projection
    void setup_l2_projection();

    // calculate wall shear stress
    void calc_tau_w(int step, Teuchos::RCP<Epetra_Vector> velnp, Teuchos::RCP<Epetra_Vector> wss);

    // l2 project vectors
    void l2_project_vector(Teuchos::RCP<Epetra_Vector> veln, Teuchos::RCP<Epetra_Vector> velnp,
        Teuchos::RCP<Epetra_Vector> accn);

    // calculate parameter for stabilization parameter mk
    void calc_mk();

    // for inflowchannel
    void transfer_and_save_tauw();

    // for inflowchannel
    void overwrite_transferred_values();

    //! discretisation
    Teuchos::RCP<Core::FE::Discretization> discret_;

    //! fluid params
    Teuchos::RCP<Teuchos::ParameterList> params_;

    //! manager for wall shear stress
    Teuchos::RCP<FLD::UTILS::StressManager> mystressmanager_;

    //! the processor ID from the communicator
    int myrank_;

    //! map including all wall nodes, redundant map
    Teuchos::RCP<Epetra_Map> dircolnodemap_;

    //! xwall node row map
    Teuchos::RCP<Epetra_Map> xwallrownodemap_;

    //! xwall node col map
    Teuchos::RCP<Epetra_Map> xwallcolnodemap_;

    //! map including the enriched dofs, row map
    Teuchos::RCP<Epetra_Map> enrdofrowmap_;

    //! map including the unused pressure dofs, row map
    Teuchos::RCP<Epetra_Map> lagrdofrowmap_;

    //! map including all enriched dofs plus unused pressure dofs
    Teuchos::RCP<Epetra_Map> mergedmap_;

    //! wall distance, local vector
    Teuchos::RCP<Epetra_Vector> walldist_;

    //! wall distance, standard node row map
    Teuchos::RCP<Epetra_Vector> wdist_;

    //! wall distance, row map of redistributed discretization
    Teuchos::RCP<Epetra_Vector> wdistxwdis_;

    //! vector on same dofs for tauw
    Teuchos::RCP<Epetra_Vector> tauw_;

    //! vector on same dofs for tauw
    Teuchos::RCP<Epetra_Vector> tauwxwdis_;

    //! vector on same dofs for deltatauw (increment)
    Teuchos::RCP<Epetra_Vector> inctauw_;

    //! vector on same dofs for deltatauw (increment)
    Teuchos::RCP<Epetra_Vector> inctauwxwdis_;

    //! vector on same dofs for old tauw
    Teuchos::RCP<Epetra_Vector> oldtauw_;

    //! vector on same dofs for old tauw
    Teuchos::RCP<Epetra_Vector> oldinctauw_;

    //! matrix projecting the wall shear stress to off-wall nodes
    Teuchos::RCP<Core::LinAlg::SparseMatrix> tauwcouplingmattrans_;

    //! toggle vector, standard node row map
    Teuchos::RCP<Epetra_Vector> xwalltoggle_;

    //! toggle vector, xwall discretization
    Teuchos::RCP<Epetra_Vector> xwalltogglexwdis_;

    //! toggle vector, local vector
    Teuchos::RCP<Epetra_Vector> xtoggleloc_;

    //! redistributed xwall discretization
    Teuchos::RCP<Core::FE::Discretization> xwdiscret_;

    //! mass matrix for projection
    Teuchos::RCP<Core::LinAlg::SparseMatrix> massmatrix_;

    //! solver for projection
    Teuchos::RCP<Core::LinAlg::Solver> solver_;

    //! increment of veln during projection
    Teuchos::RCP<Epetra_Vector> incveln_;

    //! increment of velnp during projection
    Teuchos::RCP<Epetra_Vector> incvelnp_;

    //! increment of accn during projection
    Teuchos::RCP<Epetra_Vector> incaccn_;

    //! veln for state of xwall discretization during projection
    Teuchos::RCP<Epetra_Vector> stateveln_;

    //! velnp for state of xwall discretization during projection
    Teuchos::RCP<Epetra_Vector> statevelnp_;

    //! accn for state of xwall discretization during projection
    Teuchos::RCP<Epetra_Vector> stateaccn_;

    //! MK for standard discretization
    Teuchos::RCP<Epetra_Vector> mkstate_;

    //! MK for xwall discretization
    Teuchos::RCP<Epetra_Vector> mkxwstate_;

    Teuchos::RCP<Epetra_Vector> restart_wss_;

    Teuchos::RCP<TransferTurbulentInflowConditionNodal> turbulent_inflow_condition_;

    //! increment factor of tauw
    double fac_;

    //! increment of tauw
    double inctauwnorm_;

    //! constant tauw from input file
    double constant_tauw_;

    //! minimum tauw from input file
    double min_tauw_;

    //! number of gauss points in wall-parallel and normal direction
    int gp_norm_;
    int gp_norm_ow_;
    int gp_par_;

    //! viscosity
    double visc_;

    //! density
    double dens_;

    //! when and how to update tauw
    enum Inpar::FLUID::XWallTauwType tauwtype_;

    //! how to calculate tauw
    enum Inpar::FLUID::XWallTauwCalcType tauwcalctype_;

    //! how to blend
    enum Inpar::FLUID::XWallBlendingType blendingtype_;

    //! projection
    bool proj_;

    //! smoothing through aggregation of residual
    bool smooth_res_aggregation_;

    //! mfs on fine scales
    bool fix_residual_on_inflow_;

    //! switch from gradient-based to residual-based calculation of tauw
    int switch_step_;

    int iter_;

  };  // class XWall

  class XWallAleFSI : public XWall
  {
   public:
    /// Standard Constructor
    XWallAleFSI(Teuchos::RCP<Core::FE::Discretization> dis, int nsd,
        Teuchos::RCP<Teuchos::ParameterList>& params,
        Teuchos::RCP<Core::LinAlg::MapExtractor> dbcmaps,
        Teuchos::RCP<FLD::UTILS::StressManager> wssmanager, Teuchos::RCP<Epetra_Vector> dispnp,
        Teuchos::RCP<Epetra_Vector> gridv);

    void UpdateWDistWALE();

    // set element params for xwall EnrichmentType
    void SetXWallParams(Teuchos::ParameterList& eleparams) override;

    // returns, if Properties for GenAlpha have to be updated
    void UpdateTauW(int step, Teuchos::RCP<Epetra_Vector> trueresidual, int itnum,
        Teuchos::RCP<Epetra_Vector> accn, Teuchos::RCP<Epetra_Vector> velnp,
        Teuchos::RCP<Epetra_Vector> veln) override;

   private:
    // set element params for xwall EnrichmentType, distributed for xwall discretization
    void set_x_wall_params_xw_dis(Teuchos::ParameterList& eleparams) override;
    Teuchos::RCP<Epetra_Vector> mydispnp_;
    Teuchos::RCP<Epetra_Vector> mygridv_;

    //! wall distance, row map of redistributed discretization
    Teuchos::RCP<Epetra_Vector> incwdistxwdis_;
  };

}  // namespace FLD


FOUR_C_NAMESPACE_CLOSE

#endif