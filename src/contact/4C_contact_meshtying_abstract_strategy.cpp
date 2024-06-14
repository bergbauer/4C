/*---------------------------------------------------------------------*/
/*! \file
\brief Main abstract class for meshtying solution strategies

\level 2


*/
/*---------------------------------------------------------------------*/
#include "4C_contact_meshtying_abstract_strategy.hpp"

#include "4C_comm_parobjectfactory.hpp"
#include "4C_contact_meshtying_defines.hpp"
#include "4C_contact_meshtying_noxinterface.hpp"
#include "4C_fem_discretization.hpp"
#include "4C_inpar_contact.hpp"
#include "4C_io.hpp"
#include "4C_io_control.hpp"
#include "4C_linalg_sparsematrix.hpp"
#include "4C_linalg_utils_densematrix_communication.hpp"
#include "4C_linalg_utils_sparse_algebra_assemble.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_mortar_defines.hpp"
#include "4C_mortar_interface.hpp"
#include "4C_mortar_node.hpp"

#include <Teuchos_Time.hpp>
#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 | ctor (public)                                             popp 05/09 |
 *----------------------------------------------------------------------*/
CONTACT::MtAbstractStrategy::MtAbstractStrategy(const Epetra_Map* dof_row_map,
    const Epetra_Map* NodeRowMap, Teuchos::ParameterList params,
    std::vector<Teuchos::RCP<Mortar::Interface>> interface, const int spatialDim,
    const Teuchos::RCP<const Epetra_Comm>& comm, const double alphaf, const int maxdof)
    : Mortar::StrategyBase(Teuchos::rcp(new Mortar::StratDataContainer()), dof_row_map, NodeRowMap,
          params, spatialDim, comm, alphaf, maxdof),
      interface_(interface),
      dualquadslavetrafo_(false)
{
  // call setup method with flag redistributed=FALSE
  setup(false);

  // store interface maps with parallel distribution of underlying
  // problem discretization (i.e. interface maps before parallel
  // redistribution of slave and master sides)
  if (ParRedist())
  {
    pglmdofrowmap_ = Teuchos::rcp(new Epetra_Map(*glmdofrowmap_));
    pgsdofrowmap_ = Teuchos::rcp(new Epetra_Map(*gsdofrowmap_));
    pgmdofrowmap_ = Teuchos::rcp(new Epetra_Map(*gmdofrowmap_));
    pgsmdofrowmap_ = Teuchos::rcp(new Epetra_Map(*gsmdofrowmap_));
  }

  // build the NOX::Nln::CONSTRAINT::Interface::Required object
  noxinterface_ptr_ = Teuchos::rcp(new CONTACT::MtNoxInterface());

  return;
}

/*----------------------------------------------------------------------*
 |  << operator                                              mwgee 10/07|
 *----------------------------------------------------------------------*/
std::ostream& operator<<(std::ostream& os, const CONTACT::MtAbstractStrategy& strategy)
{
  strategy.Print(os);
  return os;
}

/*----------------------------------------------------------------------*
 | parallel redistribution                                   popp 09/10 |
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::redistribute_meshtying()
{
  TEUCHOS_FUNC_TIME_MONITOR("CONTACT::MtAbstractStrategy::redistribute_meshtying");

  // Do we really want to redistribute?
  if (ParRedist() && Comm().NumProc() > 1)
  {
    // time measurement
    Comm().Barrier();
    const double t_start = Teuchos::Time::wallTime();

    // do some more stuff with interfaces
    for (int i = 0; i < (int)interface_.size(); ++i)
    {
      // print parallel distribution
      if (Comm().MyPID() == 0)
        std::cout << "\nInterface parallel distribution before rebalancing:" << std::endl;
      interface_[i]->print_parallel_distribution();

      // redistribute optimally among all procs
      interface_[i]->Redistribute();

      // call fill complete again
      interface_[i]->fill_complete(true, maxdof_);

      // print parallel distribution again
      if (Comm().MyPID() == 0)
        std::cout << "Interface parallel distribution after rebalancing:" << std::endl;
      interface_[i]->print_parallel_distribution();
    }

    // re-setup strategy with flag redistributed=TRUE
    setup(true);

    // time measurement
    Comm().Barrier();
    const double t_sum = Teuchos::Time::wallTime() - t_start;
    if (Comm().MyPID() == 0)
      std::cout << "\nTime for parallel redistribution..............." << std::scientific
                << std::setprecision(6) << t_sum << " secs\n";
  }
  else
  {
    // No parallel redistribution to be performed. Just print the current distribution to screen.
    for (int i = 0; i < (int)interface_.size(); ++i) interface_[i]->print_parallel_distribution();
  }

  return;
}

/*----------------------------------------------------------------------*
 | setup this strategy object                                popp 08/10 |
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::setup(bool redistributed)
{
  // ------------------------------------------------------------------------
  // setup global accessible Epetra_Maps
  // ------------------------------------------------------------------------

  // make sure to remove all existing maps first
  // (do NOT remove map of non-interface dofs after redistribution)
  gsdofrowmap_ = Teuchos::null;
  gmdofrowmap_ = Teuchos::null;
  gsmdofrowmap_ = Teuchos::null;
  glmdofrowmap_ = Teuchos::null;
  gdisprowmap_ = Teuchos::null;
  gsnoderowmap_ = Teuchos::null;
  gmnoderowmap_ = Teuchos::null;
  if (!redistributed) gndofrowmap_ = Teuchos::null;

  // element col. map for binning
  initial_elecolmap_.clear();
  initial_elecolmap_.resize(0);

  // make numbering of LM dofs consecutive and unique across N interfaces
  int offset_if = 0;

  // merge interface maps to global maps
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // build Lagrange multiplier dof map
    interface_[i]->UpdateLagMultSets(offset_if);

    // merge interface Lagrange multiplier dof maps to global LM dof map
    glmdofrowmap_ = Core::LinAlg::MergeMap(glmdofrowmap_, interface_[i]->LagMultDofs());
    offset_if = glmdofrowmap_->NumGlobalElements();
    if (offset_if < 0) offset_if = 0;

    // merge interface master, slave maps to global master, slave map
    gsdofrowmap_ = Core::LinAlg::MergeMap(gsdofrowmap_, interface_[i]->SlaveRowDofs());
    gmdofrowmap_ = Core::LinAlg::MergeMap(gmdofrowmap_, interface_[i]->MasterRowDofs());
    gsnoderowmap_ = Core::LinAlg::MergeMap(gsnoderowmap_, interface_[i]->SlaveRowNodes());
    gmnoderowmap_ = Core::LinAlg::MergeMap(gmnoderowmap_, interface_[i]->MasterRowNodes());

    // store initial element col map for binning strategy
    initial_elecolmap_.push_back(
        Teuchos::rcp(new Epetra_Map(*interface_[i]->Discret().ElementColMap())));
  }

  // setup global non-slave-or-master dof map
  // (this is done by splitting from the discretization dof map)
  // (no need to rebuild this map after redistribution)
  if (!redistributed)
  {
    gndofrowmap_ = Core::LinAlg::SplitMap(*ProblemDofs(), *gsdofrowmap_);
    gndofrowmap_ = Core::LinAlg::SplitMap(*gndofrowmap_, *gmdofrowmap_);
  }

  // setup combined global slave and master dof map
  // setup global displacement dof map
  gsmdofrowmap_ = Core::LinAlg::MergeMap(*gsdofrowmap_, *gmdofrowmap_, false);
  gdisprowmap_ = Core::LinAlg::MergeMap(*gndofrowmap_, *gsmdofrowmap_, false);

  // ------------------------------------------------------------------------
  // setup global accessible vectors and matrices
  // ------------------------------------------------------------------------

  // setup Lagrange multiplier vectors
  z_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  zincr_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  zold_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  zuzawa_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));

  // setup constraint rhs vector
  constrrhs_ = Teuchos::null;  // only for saddle point problem formulation

  //----------------------------------------------------------------------
  // CHECK IF WE NEED TRANSFORMATION MATRICES FOR SLAVE DISPLACEMENT DOFS
  //----------------------------------------------------------------------
  // These matrices need to be applied to the slave displacements
  // in the cases of dual LM interpolation for tet10/hex20 meshes
  // in 3D or for locally linear Lagrange multipliers for line3 meshes
  // in 2D. Here, the displacement basis functions have been modified
  // in order to assure positivity of the D matrix entries and at
  // the same time biorthogonality. Thus, to scale back the modified
  // discrete displacements \hat{d} to the nodal discrete displacements
  // {d}, we have to apply the transformation matrix T and vice versa
  // with the transformation matrix T^(-1).
  //----------------------------------------------------------------------
  Inpar::Mortar::ShapeFcn shapefcn =
      Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(Params(), "LM_SHAPEFCN");
  Inpar::Mortar::LagMultQuad lagmultquad =
      Core::UTILS::IntegralValue<Inpar::Mortar::LagMultQuad>(Params(), "LM_QUAD");
  if (shapefcn == Inpar::Mortar::shape_dual &&
      (Dim() == 3 || (Dim() == 2 && lagmultquad == Inpar::Mortar::lagmult_lin)))
    for (int i = 0; i < (int)interface_.size(); ++i)
      dualquadslavetrafo_ += (interface_[i]->Quadslave() && !(interface_[i]->IsNurbs()));

  //----------------------------------------------------------------------
  // COMPUTE TRAFO MATRIX AND ITS INVERSE
  //----------------------------------------------------------------------
  if (Dualquadslavetrafo())
  {
    // for locally linear Lagrange multipliers, consider both slave and master DOFs,
    // and otherwise, only consider slave DOFs
    if (lagmultquad == Inpar::Mortar::lagmult_lin)
    {
      trafo_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*gsmdofrowmap_, 10));
      invtrafo_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*gsmdofrowmap_, 10));
    }
    else
    {
      trafo_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*gsdofrowmap_, 10));
      invtrafo_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*gsdofrowmap_, 10));
    }

    // set of already processed nodes
    // (in order to avoid double-assembly for N interfaces)
    std::set<int> donebefore;

    // for all interfaces
    for (int i = 0; i < (int)interface_.size(); ++i)
      interface_[i]->AssembleTrafo(*trafo_, *invtrafo_, donebefore);

    // fill_complete() transformation matrices
    trafo_->Complete();
    invtrafo_->Complete();
  }

  return;
}

/*----------------------------------------------------------------------*
 | global evaluation method called from time integrator      popp 06/09 |
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::ApplyForceStiffCmt(Teuchos::RCP<Epetra_Vector> dis,
    Teuchos::RCP<Core::LinAlg::SparseOperator>& kt, Teuchos::RCP<Epetra_Vector>& f, const int step,
    const int iter, bool predictor)
{
  // set displacement state
  set_state(Mortar::state_new_displacement, *dis);

  // apply meshtying forces and stiffness
  Evaluate(kt, f, dis);

  // output interface forces
  InterfaceForces();

  return;
}


/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::set_state(
    const enum Mortar::StateType& statetype, const Epetra_Vector& vec)
{
  switch (statetype)
  {
    case Mortar::state_new_displacement:
    case Mortar::state_old_displacement:
    {
      // set state on interfaces
      for (int i = 0; i < (int)interface_.size(); ++i) interface_[i]->set_state(statetype, vec);
      break;
    }
    default:
    {
      FOUR_C_THROW(
          "Unsupported state type! (state type = %s)", Mortar::StateType2String(statetype).c_str());
      break;
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 |  do mortar coupling in reference configuration             popp 12/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::MortarCoupling(const Teuchos::RCP<const Epetra_Vector>& dis)
{
  //********************************************************************
  // initialize and evaluate interfaces
  //********************************************************************
  // for all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // initialize / reset interfaces
    interface_[i]->Initialize();

    // evaluate interfaces
    interface_[i]->Evaluate();
  }

  //********************************************************************
  // restrict mortar treatment to actual meshtying zone
  //********************************************************************
  restrict_meshtying_zone();

  //********************************************************************
  // initialize and evaluate global mortar stuff
  //********************************************************************
  // (re)setup global Mortar Core::LinAlg::SparseMatrices and Epetra_Vectors
  dmatrix_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*gsdofrowmap_, 10));
  mmatrix_ = Teuchos::rcp(new Core::LinAlg::SparseMatrix(*gsdofrowmap_, 100));
  g_ = Core::LinAlg::CreateVector(*gsdofrowmap_, true);

  // assemble D- and M-matrix on all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i) interface_[i]->AssembleDM(*dmatrix_, *mmatrix_);

  // fill_complete() global Mortar matrices
  dmatrix_->Complete();
  mmatrix_->Complete(*gmdofrowmap_, *gsdofrowmap_);

  // compute g-vector at global level
  Teuchos::RCP<Epetra_Vector> xs = Core::LinAlg::CreateVector(*gsdofrowmap_, true);
  Teuchos::RCP<Epetra_Vector> xm = Core::LinAlg::CreateVector(*gmdofrowmap_, true);
  assemble_coords("slave", true, xs);
  assemble_coords("master", true, xm);
  Teuchos::RCP<Epetra_Vector> Dxs = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  dmatrix_->Multiply(false, *xs, *Dxs);
  Teuchos::RCP<Epetra_Vector> Mxm = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  mmatrix_->Multiply(false, *xm, *Mxm);
  g_->Update(1.0, *Dxs, 1.0);
  g_->Update(-1.0, *Mxm, 1.0);

  return;
}

/*----------------------------------------------------------------------*
 |  restrict slave boundary to actual meshtying zone          popp 08/10|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::restrict_meshtying_zone()
{
  // Step 1: detect tied slave nodes on all interfaces
  int localfounduntied = 0;
  int globalfounduntied = 0;
  for (int i = 0; i < (int)interface_.size(); ++i)
    interface_[i]->detect_tied_slave_nodes(localfounduntied);
  Comm().SumAll(&localfounduntied, &globalfounduntied, 1);

  // get out of here if the whole slave surface is tied
  if (globalfounduntied == 0) return;

  // print message
  if (Comm().MyPID() == 0)
  {
    std::cout << "*restrict_meshtying_zone*...............";
    fflush(stdout);
  }

  //**********************************************************************
  // check for infeasible discretization types and LM types     popp 05/11
  // (currently RMZ only for 1st order slave elements and standard LM)
  //**********************************************************************
  // Currently, we need strictly positive LM shape functions for RMZ
  // to work properly. This is only satisfied for 1st order interpolation
  // with standard Lagrange multipliers. As soon as we have implemented
  // the modified biorthogonality (only defined on projecting slave part),
  // the 1st order interpolation case with dual Lagrange multiplier will
  // work, too. All 2nd order interpolation cases are difficult, since
  // we would require strictly positive displacement shape functions, which
  // is only possible via a proper basis transformation.
  //**********************************************************************
  bool quadratic = false;
  for (int i = 0; i < (int)interface_.size(); ++i) quadratic += interface_[i]->Quadslave();
  if (quadratic) FOUR_C_THROW("restrict_meshtying_zone only implemented for first-order elements");

  Inpar::Mortar::ShapeFcn shapefcn =
      Core::UTILS::IntegralValue<Inpar::Mortar::ShapeFcn>(Params(), "LM_SHAPEFCN");
  if ((shapefcn == Inpar::Mortar::shape_dual || shapefcn == Inpar::Mortar::shape_petrovgalerkin) &&
      Core::UTILS::IntegralValue<Inpar::Mortar::ConsistentDualType>(
          Params(), "LM_DUAL_CONSISTENT") == Inpar::Mortar::consistent_none)
    FOUR_C_THROW(
        "ERROR: restrict_meshtying_zone for dual shape functions "
        "only implemented in combination with consistent boundary modification");

  // Step 2: restrict slave node/dof sets of all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i) interface_[i]->RestrictSlaveSets();

  // Step 3: re-setup global maps and vectors with flag redistributed=FALSE
  // (this flag must be FALSE here, because the slave set has been reduced and
  // thus the non-interface set N needs to be updated / re-setup as well)
  setup(false);

  // Step 4: re-setup slave dof row map with parallel distribution of
  // underlying problem discretization (i.e. slave dof row maps before
  // parallel redistribution) -> introduce restriction!
  if (ParRedist())
  {
    // allreduce restricted slave dof row map in new distribution
    Teuchos::RCP<Epetra_Map> fullsdofs = Core::LinAlg::AllreduceEMap(*gsdofrowmap_);

    // map data to be filled
    std::vector<int> data;

    // loop over all entries of allreduced map
    const int numMyFullSlaveDofs = fullsdofs->NumMyElements();
    for (int k = 0; k < numMyFullSlaveDofs; ++k)
    {
      // get global ID of current dof
      int gid = fullsdofs->GID(k);

      /* Check if this GID is stored on this processor in the slave dof row map based on the old
       * distribution and add to data vector if so.
       */
      if (pgsdofrowmap_->MyGID(gid)) data.push_back(gid);
    }

    // re-setup old slave dof row map (with restriction now)
    pgsdofrowmap_ = Teuchos::rcp(new Epetra_Map(-1, (int)data.size(), data.data(), 0, Comm()));
  }

  // Step 5: re-setup internal dof row map (non-interface dofs)
  // (this one has been re-computed in Setup() above, but is possibly
  // incorrect due to parallel redistribution of the interfaces)
  // --> recompute based on splitting with slave and master dof row maps
  // before parallel redistribution!
  if (ParRedist())
  {
    gndofrowmap_ = Core::LinAlg::SplitMap(*ProblemDofs(), *pgsdofrowmap_);
    gndofrowmap_ = Core::LinAlg::SplitMap(*gndofrowmap_, *pgmdofrowmap_);
  }

  // Step 6: re-setup displacement dof row map with current parallel
  // distribution (possibly wrong, same argument as above)
  if (ParRedist())
  {
    gdisprowmap_ = Core::LinAlg::MergeMap(*gndofrowmap_, *gsmdofrowmap_, false);
  }

  return;
}

/*----------------------------------------------------------------------*
 |  mesh initialization for rotational invariance              popp 12/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::MeshInitialization(Teuchos::RCP<Epetra_Vector> Xslavemod)
{
  //**********************************************************************
  // (1) perform mesh initialization node by node
  //**********************************************************************
  // IMPORTANT NOTE:
  // We have to be very careful on which nodes on which processor to
  // relocate! Basically, every processor needs to know about relocation
  // of all its column nodes in the standard column map with overlap=1,
  // because all these nodes participate in the processor's element
  // evaluation! Thus, the modified slave positions are first exported
  // to the column map of the respective interface and the modification
  // loop is then also done with respect to this node column map!
  // A second concern is that we are dealing with a special interface
  // discretization (including special meshtying nodes, too) here, This
  // interface discretization has been set up for dealing with meshtying
  // ONLY, and there is still the underlying problem discretization
  // dealing with the classical finite element evaluation. Thus, it is
  // very important that we apply the nodal relocation to BOTH the
  // Mortar::Node(s) in the meshtying interface discretization AND to the
  // Core::Nodes::Node(s) in the underlying problem discretization. However, the
  // second aspect needs to be done by the respective control routine,
  // i.e. in the case of 4C in strtimint.cpp and NOT here.
  //**********************************************************************

  // loop over all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // export Xslavemod to column map for current interface
    Epetra_Vector Xslavemodcol(*(interface_[i]->SlaveColDofs()), false);
    Core::LinAlg::Export(*Xslavemod, Xslavemodcol);

    // loop over all slave column nodes on the current interface
    for (int j = 0; j < interface_[i]->SlaveColNodes()->NumMyElements(); ++j)
    {
      // get global ID of current node
      int gid = interface_[i]->SlaveColNodes()->GID(j);

      // get the mortar node
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      // new nodal position and problem dimension
      std::vector<double> Xnew(3, 0.0);
      int dim = Dim();

      //******************************************************************
      // compute new nodal position
      //******************************************************************
      // get corresponding entries from Xslavemodcol
      int numdof = mtnode->NumDof();
      if (dim != numdof) FOUR_C_THROW("Inconsisteny Dim <-> NumDof");

      // find DOFs of current node in Xslavemodcol and extract this node's position
      std::vector<int> locindex(numdof);

      for (int dof = 0; dof < numdof; ++dof)
      {
        locindex[dof] = (Xslavemodcol.Map()).LID(mtnode->Dofs()[dof]);
        if (locindex[dof] < 0) FOUR_C_THROW("Did not find dof in map");
        Xnew[dof] = Xslavemodcol[locindex[dof]];
      }

      // check is mesh distortion is still OK
      // (throw a FOUR_C_THROW if length of relocation is larger than 80%
      // of an adjacent element edge -> see Puso, IJNME, 2004)
      const double limit = 0.8;
      double relocation = 0.0;
      if (dim == 2)
      {
        relocation = sqrt((Xnew[0] - mtnode->X()[0]) * (Xnew[0] - mtnode->X()[0]) +
                          (Xnew[1] - mtnode->X()[1]) * (Xnew[1] - mtnode->X()[1]));
      }
      else if (dim == 3)
      {
        relocation = sqrt((Xnew[0] - mtnode->X()[0]) * (Xnew[0] - mtnode->X()[0]) +
                          (Xnew[1] - mtnode->X()[1]) * (Xnew[1] - mtnode->X()[1]) +
                          (Xnew[2] - mtnode->X()[2]) * (Xnew[2] - mtnode->X()[2]));
      }
      else
      {
        FOUR_C_THROW("Problem dimension must be either 2 or 3!");
      }

      // check is only done once per node (by owing processor)
      if (Comm().MyPID() == mtnode->Owner())
      {
        bool isok = mtnode->CheckMeshDistortion(relocation, limit);
        if (!isok) FOUR_C_THROW("Mesh distortion generated by relocation is too large!");
      }

      // modification of xspatial (spatial coordinates)
      for (int k = 0; k < dim; ++k) mtnode->xspatial()[k] = Xnew[k];

      // modification of xref (reference coordinates)
      mtnode->SetPos(Xnew);
    }
  }

  //**********************************************************************
  // (2) re-evaluate constraints in reference configuration
  //**********************************************************************
  // initialize
  g_ = Core::LinAlg::CreateVector(*gsdofrowmap_, true);

  // compute g-vector at global level
  Teuchos::RCP<Epetra_Vector> xs = Core::LinAlg::CreateVector(*gsdofrowmap_, true);
  Teuchos::RCP<Epetra_Vector> xm = Core::LinAlg::CreateVector(*gmdofrowmap_, true);
  assemble_coords("slave", true, xs);
  assemble_coords("master", true, xm);
  Teuchos::RCP<Epetra_Vector> Dxs = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  dmatrix_->Multiply(false, *xs, *Dxs);
  Teuchos::RCP<Epetra_Vector> Mxm = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  mmatrix_->Multiply(false, *xm, *Mxm);
  g_->Update(1.0, *Dxs, 1.0);
  g_->Update(-1.0, *Mxm, 1.0);

  return;
}

/*----------------------------------------------------------------------*
 | call appropriate evaluate for contact evaluation           popp 06/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::Evaluate(Teuchos::RCP<Core::LinAlg::SparseOperator>& kteff,
    Teuchos::RCP<Epetra_Vector>& feff, Teuchos::RCP<Epetra_Vector> dis)
{
  // trivial (no choice as for contact)
  EvaluateMeshtying(kteff, feff, dis);
  return;
}

/*----------------------------------------------------------------------*
 |  Store Lagrange multipliers into Mortar::Node                popp 06/08|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::store_nodal_quantities(Mortar::StrategyBase::QuantityType type)
{
  // loop over all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // get global quantity to be stored in nodes
    Teuchos::RCP<Epetra_Vector> vectorglobal = Teuchos::null;
    switch (type)
    {
      case Mortar::StrategyBase::lmcurrent:
      {
        vectorglobal = LagrMult();
        break;
      }
      case Mortar::StrategyBase::lmold:
      {
        vectorglobal = LagrMultOld();
        break;
      }
      case Mortar::StrategyBase::lmupdate:
      {
        vectorglobal = LagrMult();
        break;
      }
      case Mortar::StrategyBase::lmuzawa:
      {
        vectorglobal = LagrMultUzawa();
        break;
      }
      default:
      {
        FOUR_C_THROW("store_nodal_quantities: Unknown state std::string variable!");
        break;
      }
    }  // switch

    // export global quantity to current interface slave dof row map
    Teuchos::RCP<Epetra_Map> sdofrowmap = interface_[i]->SlaveRowDofs();
    Teuchos::RCP<Epetra_Vector> vectorinterface = Teuchos::rcp(new Epetra_Vector(*sdofrowmap));

    if (vectorglobal != Teuchos::null)
      Core::LinAlg::Export(*vectorglobal, *vectorinterface);
    else
      FOUR_C_THROW("store_nodal_quantities: Null vector handed in!");

    // loop over all slave row nodes on the current interface
    for (int j = 0; j < interface_[i]->SlaveRowNodes()->NumMyElements(); ++j)
    {
      int gid = interface_[i]->SlaveRowNodes()->GID(j);
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      // be aware of problem dimension
      int dim = Dim();
      int numdof = mtnode->NumDof();
      if (dim != numdof) FOUR_C_THROW("Inconsisteny Dim <-> NumDof");

      // find indices for DOFs of current node in Epetra_Vector
      // and extract this node's quantity from vectorinterface
      std::vector<int> locindex(dim);

      for (int dof = 0; dof < dim; ++dof)
      {
        locindex[dof] = (vectorinterface->Map()).LID(mtnode->Dofs()[dof]);
        if (locindex[dof] < 0) FOUR_C_THROW("StoreNodalQuantites: Did not find dof in map");

        switch (type)
        {
          case Mortar::StrategyBase::lmcurrent:
          {
            mtnode->MoData().lm()[dof] = (*vectorinterface)[locindex[dof]];
            break;
          }
          case Mortar::StrategyBase::lmold:
          {
            mtnode->MoData().lmold()[dof] = (*vectorinterface)[locindex[dof]];
            break;
          }
          case Mortar::StrategyBase::lmuzawa:
          {
            mtnode->MoData().lmuzawa()[dof] = (*vectorinterface)[locindex[dof]];
            break;
          }
          case Mortar::StrategyBase::lmupdate:
          {
            // throw a FOUR_C_THROW if node is Active and DBC
            if (mtnode->IsDbc())
              FOUR_C_THROW(
                  "Slave Node %i is active and at the same time carries D.B.C.s!", mtnode->Id());

            // store updated LM into node
            mtnode->MoData().lm()[dof] = (*vectorinterface)[locindex[dof]];
            break;
          }
          default:
          {
            FOUR_C_THROW("store_nodal_quantities: Unknown state std::string variable!");
            break;
          }
        }  // switch
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Store dirichlet B.C. status into Mortar::Node               popp 06/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::store_dirichlet_status(
    Teuchos::RCP<const Core::LinAlg::MapExtractor> dbcmaps)
{
  // loop over all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // loop over all slave row nodes on the current interface
    for (int j = 0; j < interface_[i]->SlaveRowNodes()->NumMyElements(); ++j)
    {
      int gid = interface_[i]->SlaveRowNodes()->GID(j);
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      // check if this node's dofs are in dbcmap
      for (int k = 0; k < mtnode->NumDof(); ++k)
      {
        int currdof = mtnode->Dofs()[k];
        int lid = (dbcmaps->CondMap())->LID(currdof);

        // store dbc status if found
        if (lid >= 0 && mtnode->DbcDofs()[k] == false)
        {
          mtnode->SetDbc() = true;

          // check compatibility of contact symmetry condition and displacement dirichlet conditions
          if (lid < 0 && mtnode->DbcDofs()[k] == true)
            FOUR_C_THROW(
                "Inconsistency in structure Dirichlet conditions and Mortar symmetry conditions");
        }
      }
    }
  }

  // create old style dirichtoggle vector (supposed to go away)
  pgsdirichtoggle_ = Core::LinAlg::CreateVector(*gsdofrowmap_, true);
  Teuchos::RCP<Epetra_Vector> temp = Teuchos::rcp(new Epetra_Vector(*(dbcmaps->CondMap())));
  temp->PutScalar(1.0);
  Core::LinAlg::Export(*temp, *pgsdirichtoggle_);

  return;
}

/*----------------------------------------------------------------------*
 | Update meshtying at end of time step                       popp 06/09|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::Update(Teuchos::RCP<const Epetra_Vector> dis)
{
  // store Lagrange multipliers
  // (we need this for interpolation of the next generalized mid-point)
  zold_->Update(1.0, *z_, 0.0);
  store_nodal_quantities(Mortar::StrategyBase::lmold);

  // old displacements in nodes
  // (this is needed for calculating the auxiliary positions in
  // binarytree contact search)
  set_state(Mortar::state_old_displacement, *dis);

  return;
}

/*----------------------------------------------------------------------*
 |  read restart information for meshtying                    popp 03/08|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::DoReadRestart(
    Core::IO::DiscretizationReader& reader, Teuchos::RCP<const Epetra_Vector> dis)
{
  // check whether this is a restart with meshtying of a previously
  // non-meshtying simulation run
  bool restartwithmeshtying = Core::UTILS::IntegralValue<int>(Params(), "RESTART_WITH_MESHTYING");

  // set displacement state
  set_state(Mortar::state_new_displacement, *dis);

  // read restart information on Lagrange multipliers
  z_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  zincr_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  if (!restartwithmeshtying) reader.read_vector(LagrMult(), "mt_lagrmultold");
  store_nodal_quantities(Mortar::StrategyBase::lmcurrent);
  zold_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
  if (!restartwithmeshtying) reader.read_vector(LagrMultOld(), "mt_lagrmultold");
  store_nodal_quantities(Mortar::StrategyBase::lmold);

  // only for Uzawa strategy
  Inpar::CONTACT::SolvingStrategy st =
      Core::UTILS::IntegralValue<Inpar::CONTACT::SolvingStrategy>(Params(), "STRATEGY");
  if (st == Inpar::CONTACT::solution_uzawa)
  {
    zuzawa_ = Teuchos::rcp(new Epetra_Vector(*gsdofrowmap_));
    if (!restartwithmeshtying) reader.read_vector(LagrMultUzawa(), "mt_lagrmultold");
    store_nodal_quantities(Mortar::StrategyBase::lmuzawa);
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Compute interface forces (for debugging only)             popp 02/08|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::InterfaceForces(bool output)
{
  // check chosen output option
  Inpar::CONTACT::EmOutputType emtype =
      Core::UTILS::IntegralValue<Inpar::CONTACT::EmOutputType>(Params(), "EMOUTPUT");

  // get out of here if no output wanted
  if (emtype == Inpar::CONTACT::output_none) return;

  // compute discrete slave and master interface forces
  Teuchos::RCP<Epetra_Vector> fcslavetemp = Teuchos::rcp(new Epetra_Vector(dmatrix_->RowMap()));
  Teuchos::RCP<Epetra_Vector> fcmastertemp = Teuchos::rcp(new Epetra_Vector(mmatrix_->DomainMap()));
  dmatrix_->Multiply(true, *z_, *fcslavetemp);
  mmatrix_->Multiply(true, *z_, *fcmastertemp);

  // export the interface forces to full dof layout
  Teuchos::RCP<Epetra_Vector> fcslave = Teuchos::rcp(new Epetra_Vector(*ProblemDofs()));
  Teuchos::RCP<Epetra_Vector> fcmaster = Teuchos::rcp(new Epetra_Vector(*ProblemDofs()));
  Core::LinAlg::Export(*fcslavetemp, *fcslave);
  Core::LinAlg::Export(*fcmastertemp, *fcmaster);

  // interface forces and moments
  std::vector<double> gfcs(3);
  std::vector<double> ggfcs(3);
  std::vector<double> gfcm(3);
  std::vector<double> ggfcm(3);
  std::vector<double> gmcs(3);
  std::vector<double> ggmcs(3);
  std::vector<double> gmcm(3);
  std::vector<double> ggmcm(3);

  std::vector<double> gmcsnew(3);
  std::vector<double> ggmcsnew(3);
  std::vector<double> gmcmnew(3);
  std::vector<double> ggmcmnew(3);

  // weighted gap vector
  Teuchos::RCP<Epetra_Vector> gapslave = Teuchos::rcp(new Epetra_Vector(dmatrix_->RowMap()));
  Teuchos::RCP<Epetra_Vector> gapmaster = Teuchos::rcp(new Epetra_Vector(mmatrix_->DomainMap()));

  // loop over all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // loop over all slave nodes on the current interface
    for (int j = 0; j < interface_[i]->SlaveRowNodes()->NumMyElements(); ++j)
    {
      int gid = interface_[i]->SlaveRowNodes()->GID(j);
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      std::vector<double> nodeforce(3);
      std::vector<double> position(3);

      // forces and positions
      for (int d = 0; d < Dim(); ++d)
      {
        int dofid = (fcslavetemp->Map()).LID(mtnode->Dofs()[d]);
        if (dofid < 0) FOUR_C_THROW("InterfaceForces: Did not find slave dof in map");
        nodeforce[d] = (*fcslavetemp)[dofid];
        gfcs[d] += nodeforce[d];
        position[d] = mtnode->xspatial()[d];
      }

      // moments
      std::vector<double> nodemoment(3);
      nodemoment[0] = position[1] * nodeforce[2] - position[2] * nodeforce[1];
      nodemoment[1] = position[2] * nodeforce[0] - position[0] * nodeforce[2];
      nodemoment[2] = position[0] * nodeforce[1] - position[1] * nodeforce[0];
      for (int d = 0; d < 3; ++d) gmcs[d] += nodemoment[d];

      // weighted gap
      Core::LinAlg::SerialDenseVector posnode(Dim());
      std::vector<int> lm(Dim());
      std::vector<int> lmowner(Dim());
      for (int d = 0; d < Dim(); ++d)
      {
        posnode[d] = mtnode->xspatial()[d];
        lm[d] = mtnode->Dofs()[d];
        lmowner[d] = mtnode->Owner();
      }
      Core::LinAlg::Assemble(*gapslave, posnode, lm, lmowner);
    }

    // loop over all master nodes on the current interface
    for (int j = 0; j < interface_[i]->MasterRowNodes()->NumMyElements(); ++j)
    {
      int gid = interface_[i]->MasterRowNodes()->GID(j);
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      std::vector<double> nodeforce(3);
      std::vector<double> position(3);

      // forces and positions
      for (int d = 0; d < Dim(); ++d)
      {
        int dofid = (fcmastertemp->Map()).LID(mtnode->Dofs()[d]);
        if (dofid < 0) FOUR_C_THROW("InterfaceForces: Did not find master dof in map");
        nodeforce[d] = -(*fcmastertemp)[dofid];
        gfcm[d] += nodeforce[d];
        position[d] = mtnode->xspatial()[d];
      }

      // moments
      std::vector<double> nodemoment(3);
      nodemoment[0] = position[1] * nodeforce[2] - position[2] * nodeforce[1];
      nodemoment[1] = position[2] * nodeforce[0] - position[0] * nodeforce[2];
      nodemoment[2] = position[0] * nodeforce[1] - position[1] * nodeforce[0];
      for (int d = 0; d < 3; ++d) gmcm[d] += nodemoment[d];

      // weighted gap
      Core::LinAlg::SerialDenseVector posnode(Dim());
      std::vector<int> lm(Dim());
      std::vector<int> lmowner(Dim());
      for (int d = 0; d < Dim(); ++d)
      {
        posnode[d] = mtnode->xspatial()[d];
        lm[d] = mtnode->Dofs()[d];
        lmowner[d] = mtnode->Owner();
      }
      Core::LinAlg::Assemble(*gapmaster, posnode, lm, lmowner);
    }
  }

  // weighted gap
  Teuchos::RCP<Epetra_Vector> gapslavefinal = Teuchos::rcp(new Epetra_Vector(dmatrix_->RowMap()));
  Teuchos::RCP<Epetra_Vector> gapmasterfinal = Teuchos::rcp(new Epetra_Vector(mmatrix_->RowMap()));
  dmatrix_->Multiply(false, *gapslave, *gapslavefinal);
  mmatrix_->Multiply(false, *gapmaster, *gapmasterfinal);
  Teuchos::RCP<Epetra_Vector> gapfinal = Teuchos::rcp(new Epetra_Vector(dmatrix_->RowMap()));
  gapfinal->Update(1.0, *gapslavefinal, 0.0);
  gapfinal->Update(-1.0, *gapmasterfinal, 1.0);

  // again, for alternative moment: lambda x gap
  // loop over all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // loop over all slave nodes on the current interface
    for (int j = 0; j < interface_[i]->SlaveRowNodes()->NumMyElements(); ++j)
    {
      int gid = interface_[i]->SlaveRowNodes()->GID(j);
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      std::vector<double> lm(3);
      std::vector<double> nodegaps(3);
      std::vector<double> nodegapm(3);

      // LMs and gaps
      for (int d = 0; d < Dim(); ++d)
      {
        int dofid = (fcslavetemp->Map()).LID(mtnode->Dofs()[d]);
        if (dofid < 0) FOUR_C_THROW("InterfaceForces: Did not find slave dof in map");
        nodegaps[d] = (*gapslavefinal)[dofid];
        nodegapm[d] = (*gapmasterfinal)[dofid];
        lm[d] = mtnode->MoData().lm()[d];
      }

      // moments
      std::vector<double> nodemoments(3);
      std::vector<double> nodemomentm(3);
      nodemoments[0] = nodegaps[1] * lm[2] - nodegaps[2] * lm[1];
      nodemoments[1] = nodegaps[2] * lm[0] - nodegaps[0] * lm[2];
      nodemoments[2] = nodegaps[0] * lm[1] - nodegaps[1] * lm[0];
      nodemomentm[0] = nodegapm[1] * lm[2] - nodegapm[2] * lm[1];
      nodemomentm[1] = nodegapm[2] * lm[0] - nodegapm[0] * lm[2];
      nodemomentm[2] = nodegapm[0] * lm[1] - nodegapm[1] * lm[0];
      for (int d = 0; d < 3; ++d)
      {
        gmcsnew[d] += nodemoments[d];
        gmcmnew[d] -= nodemomentm[d];
      }
    }
  }

  // summing up over all processors
  for (int i = 0; i < 3; ++i)
  {
    Comm().SumAll(&gfcs[i], &ggfcs[i], 1);
    Comm().SumAll(&gfcm[i], &ggfcm[i], 1);
    Comm().SumAll(&gmcs[i], &ggmcs[i], 1);
    Comm().SumAll(&gmcm[i], &ggmcm[i], 1);
    Comm().SumAll(&gmcsnew[i], &ggmcsnew[i], 1);
    Comm().SumAll(&gmcmnew[i], &ggmcmnew[i], 1);
  }

  // print interface results to file
  if (emtype == Inpar::CONTACT::output_file || emtype == Inpar::CONTACT::output_both)
  {
    // do this at end of time step only (output==true)!
    // processor 0 does all the work
    if (output && Comm().MyPID() == 0)
    {
      FILE* MyFile = nullptr;
      std::ostringstream filename;
      filename << "interface.txt";
      MyFile = fopen(filename.str().c_str(), "at+");

      if (MyFile)
      {
        for (int i = 0; i < 3; i++) fprintf(MyFile, "%g\t", ggfcs[i]);
        for (int i = 0; i < 3; i++) fprintf(MyFile, "%g\t", ggfcm[i]);
        for (int i = 0; i < 3; i++) fprintf(MyFile, "%g\t", ggmcs[i]);
        for (int i = 0; i < 3; i++) fprintf(MyFile, "%g\t", ggmcm[i]);
        fprintf(MyFile, "\n");
        fclose(MyFile);
      }
      else
        FOUR_C_THROW("File for writing meshtying forces could not be opened.");
    }
  }

  // print interface results to screen
  if (emtype == Inpar::CONTACT::output_screen || emtype == Inpar::CONTACT::output_both)
  {
    // do this during Newton steps only (output==false)!
    // processor 0 does all the work
    if (!output && Comm().MyPID() == 0)
    {
      double snorm = sqrt(ggfcs[0] * ggfcs[0] + ggfcs[1] * ggfcs[1] + ggfcs[2] * ggfcs[2]);
      double mnorm = sqrt(ggfcm[0] * ggfcm[0] + ggfcm[1] * ggfcm[1] + ggfcm[2] * ggfcm[2]);
      printf("Slave Contact Force:   % e  % e  % e \tNorm: % e\n", ggfcs[0], ggfcs[1], ggfcs[2],
          snorm);
      printf("Master Contact Force:  % e  % e  % e \tNorm: % e\n", ggfcm[0], ggfcm[1], ggfcm[2],
          mnorm);
      printf("Slave Meshtying Moment:  % e  % e  % e\n", ggmcs[0], ggmcs[1], ggmcs[2]);
      // printf("Slave Meshtying Moment:  % e  % e  % e\n",ggmcsnew[0],ggmcsnew[1],ggmcsnew[2]);
      printf("Master Meshtying Moment: % e  % e  % e\n", ggmcm[0], ggmcm[1], ggmcm[2]);
      // printf("Master Meshtying Moment: % e  % e  % e\n",ggmcmnew[0],ggmcmnew[1],ggmcmnew[2]);
      fflush(stdout);
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 |  print interfaces (public)                                mwgee 10/07|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::Print(std::ostream& os) const
{
  if (Comm().MyPID() == 0)
  {
    os << "--------------------------------- CONTACT::MtAbstractStrategy\n"
       << "Meshtying interfaces: " << (int)interface_.size() << std::endl
       << "-------------------------------------------------------------\n";
  }
  Comm().Barrier();
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    std::cout << *(interface_[i]);
  }
  Comm().Barrier();

  return;
}

/*----------------------------------------------------------------------*
 | print active set information                               popp 06/08|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::PrintActiveSet() const
{
  //**********************************************************************
  // only do this if corresponding output option is chosen
  //**********************************************************************
#ifdef MESHTYINGASOUTPUT

  // output message
  Comm().Barrier();
  if (Comm().MyPID() == 0)
  {
    printf("\nMeshtying Interface--------------------------------------------------------------\n");
    fflush(stdout);
  }

  // create storage for local and global data
  std::vector<int> lnid, gnid;
  std::vector<double> llmx, glmx;
  std::vector<double> llmy, glmy;
  std::vector<double> llmz, glmz;

  std::vector<double> Xposl, Xposg;
  std::vector<double> Yposl, Yposg;
  std::vector<double> Zposl, Zposg;

  std::vector<double> xposl, xposg;
  std::vector<double> yposl, yposg;
  std::vector<double> zposl, zposg;

  // loop over all interfaces
  for (int i = 0; i < (int)interface_.size(); ++i)
  {
    // loop over all slave row nodes on the current interface
    for (int j = 0; j < interface_[i]->SlaveRowNodes()->NumMyElements(); ++j)
    {
      // gid of current node
      int gid = interface_[i]->SlaveRowNodes()->GID(j);
      Core::Nodes::Node* node = interface_[i]->Discret().gNode(gid);
      if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);

      // cast to Mortar::Node
      Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

      // store node id
      lnid.push_back(gid);

      // store Lagrange multiplier
      llmx.push_back(mtnode->MoData().lm()[0]);
      llmy.push_back(mtnode->MoData().lm()[1]);
      llmz.push_back(mtnode->MoData().lm()[2]);

      double Xpos = mtnode->X()[0];
      double Ypos = mtnode->X()[1];
      double Zpos = mtnode->X()[2];

      double xpos = mtnode->xspatial()[0];
      double ypos = mtnode->xspatial()[1];
      double zpos = mtnode->xspatial()[2];

      Xposl.push_back(Xpos);
      Yposl.push_back(Ypos);
      Zposl.push_back(Zpos);
      xposl.push_back(xpos);
      yposl.push_back(ypos);
      zposl.push_back(zpos);
    }
  }

  // we want to gather data from on all procs
  std::vector<int> allproc(Comm().NumProc());
  for (int i = 0; i < Comm().NumProc(); ++i) allproc[i] = i;

  // communicate all data to proc 0
  Core::LinAlg::Gather<int>(lnid, gnid, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(llmx, glmx, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(llmy, glmy, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(llmz, glmz, (int)allproc.size(), allproc.data(), Comm());

  Core::LinAlg::Gather<double>(Xposl, Xposg, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(Yposl, Yposg, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(Zposl, Zposg, (int)allproc.size(), allproc.data(), Comm());

  Core::LinAlg::Gather<double>(xposl, xposg, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(yposl, yposg, (int)allproc.size(), allproc.data(), Comm());
  Core::LinAlg::Gather<double>(zposl, zposg, (int)allproc.size(), allproc.data(), Comm());

  // output is solely done by proc 0
  if (Comm().MyPID() == 0)
  {
    // loop over all nodes
    for (int k = 0; k < (int)gnid.size(); ++k)
    {
      // print nodes of active set *************************************
      // printf("ACTIVE: %d \t lm[0]: % e \t lm[1]: % e \t lm[2]: % e
      // \n",gnid[k],glmx[k],glmy[k],glmz[k]);

      //      if (Xposg[k]==2.5 && Yposg[k]==-2.5)
      //      {
      //        printf("1  %d \t lm: % e \n",gnid[k],glmz[k]);
      //      }
      //      if (Xposg[k]==(10.0/12.0) && Yposg[k]==(-10.0/12.0))
      //      {
      //        printf("2  %d \t lm: % e \n",gnid[k],glmz[k]);
      //      }
      //      if (Xposg[k]==(-10.0/12.0) && Yposg[k]==(10.0/12.0))
      //      {
      //        printf("3  %d \t lm: % e \n",gnid[k],glmz[k]);
      //      }
      //      if (Xposg[k]==-2.5 && Yposg[k]==2.5)
      //      {
      //        printf("4  %d \t lm: % e \n",gnid[k],glmz[k]);
      //      }
      // alternative output: with additional slave node coordinates in reference configuration
      printf(
          "ACTIVE: %d \t lm[0]: % e \t lm[1]: % e \t lm[2]: % e \t Xref: % e \t Yref: % e \t Zref: "
          "% e \n",
          gnid[k], glmx[k], glmy[k], glmz[k], Xposg[k], Yposg[k], Zposg[k]);
    }
    fflush(stdout);
  }

  // output line
  Comm().Barrier();
  if (Comm().MyPID() == 0)
  {
    printf("--------------------------------------------------------------------------------\n\n");
    fflush(stdout);
  }

#endif

  return;
}

/*----------------------------------------------------------------------*
 | Visualization of meshtying segments with gmsh              popp 08/08|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::VisualizeGmsh(const int step, const int iter)
{
  // visualization with gmsh
  for (int i = 0; i < (int)interface_.size(); ++i) interface_[i]->VisualizeGmsh(step, iter);
}

/*----------------------------------------------------------------------*
 | Visualization of meshtying segments with gmsh              popp 08/08|
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::assemble_coords(
    const std::string& sidename, bool ref, Teuchos::RCP<Epetra_Vector> vec)
{
  // NOTE:
  // An alternative way of doing this would be to loop over
  // all interfaces and to assemble the coordinates there.
  // In that case, one would have to be very careful with
  // edge nodes / crosspoints, which must not be assembled
  // twice. The solution would be to overwrite the corresponding
  // entries in the Epetra_Vector instead of using Assemble().

  // decide which side (slave or master)
  Teuchos::RCP<Epetra_Map> sidemap = Teuchos::null;
  if (sidename == "slave")
    sidemap = gsnoderowmap_;
  else if (sidename == "master")
    sidemap = gmnoderowmap_;
  else
    FOUR_C_THROW("Unknown sidename");

  // loop over all row nodes of this side (at the global level)
  for (int j = 0; j < sidemap->NumMyElements(); ++j)
  {
    int gid = sidemap->GID(j);

    // find this node in interface discretizations
    bool found = false;
    Core::Nodes::Node* node = nullptr;
    for (int k = 0; k < (int)interface_.size(); ++k)
    {
      found = interface_[k]->Discret().HaveGlobalNode(gid);
      if (found)
      {
        node = interface_[k]->Discret().gNode(gid);
        break;
      }
    }
    if (!node) FOUR_C_THROW("Cannot find node with gid %", gid);
    Mortar::Node* mtnode = dynamic_cast<Mortar::Node*>(node);

    // prepare assembly
    Core::LinAlg::SerialDenseVector val(Dim());
    std::vector<int> lm(Dim());
    std::vector<int> lmowner(Dim());

    for (int k = 0; k < Dim(); ++k)
    {
      // reference (true) or current (false) configuration
      if (ref)
        val[k] = mtnode->X()[k];
      else
        val[k] = mtnode->xspatial()[k];
      lm[k] = mtnode->Dofs()[k];
      lmowner[k] = mtnode->Owner();
    }

    // do assembly
    Core::LinAlg::Assemble(*vec, val, lm, lmowner);
  }

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::collect_maps_for_preconditioner(
    Teuchos::RCP<Epetra_Map>& MasterDofMap, Teuchos::RCP<Epetra_Map>& SlaveDofMap,
    Teuchos::RCP<Epetra_Map>& InnerDofMap, Teuchos::RCP<Epetra_Map>& ActiveDofMap) const
{
  InnerDofMap = gndofrowmap_;  // global internal dof row map

  // global active slave dof row map (all slave dofs are active  in meshtying)
  if (pgsdofrowmap_ != Teuchos::null)
    ActiveDofMap = pgsdofrowmap_;
  else
    ActiveDofMap = gsdofrowmap_;

  // check if parallel redistribution is used
  // if parallel redistribution is activated, then use (original) maps before redistribution
  // otherwise we use just the standard master/slave maps
  if (pgsdofrowmap_ != Teuchos::null)
    SlaveDofMap = pgsdofrowmap_;
  else
    SlaveDofMap = gsdofrowmap_;
  if (pgmdofrowmap_ != Teuchos::null)
    MasterDofMap = pgmdofrowmap_;
  else
    MasterDofMap = gmdofrowmap_;

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool CONTACT::MtAbstractStrategy::IsSaddlePointSystem() const
{
  if (SystemType() == Inpar::CONTACT::system_saddlepoint) return true;

  return false;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool CONTACT::MtAbstractStrategy::IsCondensedSystem() const
{
  if (SystemType() != Inpar::CONTACT::system_saddlepoint) return true;

  return false;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::fill_maps_for_preconditioner(
    std::vector<Teuchos::RCP<Epetra_Map>>& maps) const
{
  /* FixMe This function replaces the deprecated collect_maps_for_preconditioner(),
   * the old version can be deleted, as soon as the contact uses the new
   * structure framework. */

  if (maps.size() != 4) FOUR_C_THROW("The vector size has to be 4!");
  /* check if parallel redistribution is used
   * if parallel redistribution is activated, then use (original) maps
   * before redistribution otherwise we use just the standard master/slave
   * maps */
  // (0) masterDofMap
  if (pgmdofrowmap_ != Teuchos::null)
    maps[0] = pgmdofrowmap_;
  else
    maps[0] = gmdofrowmap_;
  // (1) slaveDofMap
  // (3) activeDofMap (all slave nodes are active!)
  if (pgsdofrowmap_ != Teuchos::null)
  {
    maps[1] = pgsdofrowmap_;
    maps[3] = pgsdofrowmap_;
  }
  else
  {
    maps[1] = gsdofrowmap_;
    maps[3] = gsdofrowmap_;
  }
  // (2) innerDofMap
  maps[2] = gndofrowmap_;

  return;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
bool CONTACT::MtAbstractStrategy::computePreconditioner(
    const Epetra_Vector& x, Epetra_Operator& M, Teuchos::ParameterList* precParams)
{
  FOUR_C_THROW("Not implemented!");
  return false;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void CONTACT::MtAbstractStrategy::postprocess_quantities_per_interface(
    Teuchos::RCP<Teuchos::ParameterList> outputParams)
{
  using Teuchos::RCP;

  // Evaluate slave and master forces
  RCP<Epetra_Vector> fcslave = Teuchos::rcp(new Epetra_Vector(DMatrix()->RowMap()));
  RCP<Epetra_Vector> fcmaster = Teuchos::rcp(new Epetra_Vector(MMatrix()->DomainMap()));
  DMatrix()->Multiply(true, *zold_, *fcslave);
  MMatrix()->Multiply(true, *zold_, *fcmaster);

  // Append data to parameter list
  outputParams->set<RCP<const Epetra_Vector>>("interface traction", zold_);
  outputParams->set<RCP<const Epetra_Vector>>("slave forces", fcslave);
  outputParams->set<RCP<const Epetra_Vector>>("master forces", fcmaster);

  for (std::vector<Teuchos::RCP<Mortar::Interface>>::iterator it = interface_.begin();
       it < interface_.end(); ++it)
    (*it)->postprocess_quantities(*outputParams);
}

FOUR_C_NAMESPACE_CLOSE