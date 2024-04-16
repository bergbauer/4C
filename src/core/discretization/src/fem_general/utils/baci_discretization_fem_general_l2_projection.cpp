/*---------------------------------------------------------------------*/
/*! \file

\brief L2 projection on nodes

\level 0


*/
/*---------------------------------------------------------------------*/

#include "baci_discretization_fem_general_l2_projection.hpp"

#include "baci_io_control.hpp"
#include "baci_lib_discret.hpp"
#include "baci_linalg_utils_sparse_algebra_assemble.hpp"
#include "baci_linalg_utils_sparse_algebra_manipulation.hpp"
#include "baci_linear_solver_method_linalg.hpp"
#include "baci_utils_exceptions.hpp"
#include "baci_utils_parameter_list.hpp"

BACI_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Epetra_MultiVector> CORE::FE::evaluate_and_solve_nodal_l2_projection(
    DRT::Discretization& dis, const Epetra_Map& noderowmap, const std::string& statename,
    const int& numvec, Teuchos::ParameterList& params, const Teuchos::ParameterList& solverparams,
    const Epetra_Map& fullnoderowmap, const std::map<int, int>& slavetomastercolnodesmap)
{
  // create empty matrix
  auto massmatrix = Teuchos::rcp(new CORE::LINALG::SparseMatrix(noderowmap, 108, false, true));
  // create empty right hand side
  auto rhs = Teuchos::rcp(new Epetra_MultiVector(noderowmap, numvec));

  std::vector<int> lm;
  std::vector<int> lmowner;
  std::vector<int> lmstride;
  DRT::Element::LocationArray la(dis.NumDofSets());

  // define element matrices and vectors
  CORE::LINALG::SerialDenseMatrix elematrix1;
  CORE::LINALG::SerialDenseMatrix elematrix2;
  CORE::LINALG::SerialDenseVector elevector1;
  CORE::LINALG::SerialDenseVector elevector2;
  CORE::LINALG::SerialDenseVector elevector3;

  // loop column elements

  for (auto* actele : dis.MyColElementRange())
  {
    const int numnode = actele->NumNode();

    actele->LocationVector(dis, la, false);
    lmowner = la[0].lmowner_;
    lmstride = la[0].stride_;
    lm = la[0].lm_;

    // Reshape element matrices and vectors and initialize to zero
    elevector1.size(numnode);
    elematrix1.shape(numnode, numnode);
    elematrix2.shape(numnode, numvec);

    // call the element specific evaluate method (elemat1 = mass matrix, elemat2 = rhs)
    int err = actele->Evaluate(
        params, dis, la, elematrix1, elematrix2, elevector1, elevector2, elevector3);
    if (err) dserror("Element %d returned err=%d", actele->Id(), err);


    // get element location vector for nodes
    lm.resize(numnode);
    lmowner.resize(numnode);

    DRT::Node** nodes = actele->Nodes();
    for (int n = 0; n < numnode; ++n)
    {
      const int nodeid = nodes[n]->Id();
      if (!slavetomastercolnodesmap.empty())
      {
        auto slavemasterpair = slavetomastercolnodesmap.find(nodeid);
        if (slavemasterpair != slavetomastercolnodesmap.end())
          lm[n] = slavemasterpair->second;
        else
          lm[n] = nodeid;
      }
      else
        lm[n] = nodeid;

      // owner of pbc master and slave nodes are identical
      lmowner[n] = nodes[n]->Owner();
    }

    // mass matrix assembling into node map
    massmatrix->Assemble(actele->Id(), elematrix1, lm, lmowner);

    // assemble numvec entries sequentially
    for (int n = 0; n < numvec; ++n)
    {
      // copy results into Serial_DenseVector for assembling
      for (int inode = 0; inode < numnode; ++inode) elevector1(inode) = elematrix2(inode, n);
      // assemble into nth vector of MultiVector
      CORE::LINALG::Assemble(*rhs, n, elevector1, lm, lmowner);
    }
  }  // end element loop

  // finalize the matrix
  massmatrix->Complete();

  return solve_nodal_l2_projection(*massmatrix, *rhs, dis.Comm(), numvec, solverparams, noderowmap,
      fullnoderowmap, slavetomastercolnodesmap);
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
Teuchos::RCP<Epetra_MultiVector> CORE::FE::compute_nodal_l2_projection(
    Teuchos::RCP<DRT::Discretization> dis, const std::string& statename, const int& numvec,
    Teuchos::ParameterList& params, const Teuchos::ParameterList& solverparams)
{
  // check if the statename has been set
  if (!dis->HasState(statename))
  {
    dserror(
        "The discretization does not know about this statename. Please "
        "review how you call this function.");
  }

  // check whether action type is set
  if (params.getEntryRCP("action") == Teuchos::null) dserror("action type for element is missing");

  // handle pbcs if existing
  // build inverse map from slave to master nodes
  std::map<int, int> slavetomastercolnodesmap;

  std::map<int, std::vector<int>>* allcoupledcolnodes = dis->GetAllPBCCoupledColNodes();
  if (allcoupledcolnodes)
  {
    for (auto [master_gid, slave_gids] : *allcoupledcolnodes)
    {
      for (const auto slave_gid : slave_gids)
      {
        slavetomastercolnodesmap[slave_gid] = master_gid;
      }
    }
  }

  // get reduced node row map of fluid field --> will be used for setting up linear system
  const auto* fullnoderowmap = dis->NodeRowMap();
  // remove pbc slave nodes from full noderowmap
  std::vector<int> reducednoderowmap;
  // a little more memory than necessary is possibly reserved here
  reducednoderowmap.reserve(fullnoderowmap->NumMyElements());
  for (int i = 0; i < fullnoderowmap->NumMyElements(); ++i)
  {
    const int nodeid = fullnoderowmap->GID(i);
    // do not add slave pbc nodes here
    if (slavetomastercolnodesmap.empty() or slavetomastercolnodesmap.count(nodeid) == 0)
    {
      reducednoderowmap.push_back(nodeid);
    }
  }

  // build node row map which does not include slave pbc nodes
  Epetra_Map noderowmap(-1, static_cast<int>(reducednoderowmap.size()), reducednoderowmap.data(), 0,
      fullnoderowmap->Comm());

  auto nodevec = evaluate_and_solve_nodal_l2_projection(*dis, noderowmap, statename, numvec, params,
      solverparams, *fullnoderowmap, slavetomastercolnodesmap);

  // if no pbc are involved leave here
  if (slavetomastercolnodesmap.empty() or noderowmap.PointSameAs(*fullnoderowmap)) return nodevec;

  // solution vector based on full row map in which the solution of the master node is inserted into
  // slave nodes
  auto fullnodevec = Teuchos::rcp(new Epetra_MultiVector(*fullnoderowmap, numvec));

  for (int i = 0; i < fullnoderowmap->NumMyElements(); ++i)
  {
    const int nodeid = fullnoderowmap->GID(i);

    auto slavemasterpair = slavetomastercolnodesmap.find(nodeid);
    if (slavemasterpair != slavetomastercolnodesmap.end())
    {
      const int mastergid = slavemasterpair->second;
      const int masterlid = noderowmap.LID(mastergid);
      for (int j = 0; j < numvec; ++j)
        fullnodevec->ReplaceMyValue(i, j, ((*(*nodevec)(j))[masterlid]));
    }
    else
    {
      const int lid = noderowmap.LID(nodeid);
      for (int j = 0; j < numvec; ++j) fullnodevec->ReplaceMyValue(i, j, ((*(*nodevec)(j))[lid]));
    }
  }

  return fullnodevec;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Epetra_MultiVector> CORE::FE::solve_nodal_l2_projection(
    CORE::LINALG::SparseMatrix& massmatrix, Epetra_MultiVector& rhs, const Epetra_Comm& comm,
    const int& numvec, const Teuchos::ParameterList& solverparams, const Epetra_Map& noderowmap,
    const Epetra_Map& fullnoderowmap, const std::map<int, int>& slavetomastercolnodesmap)
{
  // get solver parameter list of linear solver
  const auto solvertype =
      Teuchos::getIntegralValue<INPAR::SOLVER::SolverType>(solverparams, "SOLVER");

  auto solver = Teuchos::rcp(new CORE::LINALG::Solver(solverparams, comm));

  // skip setup of preconditioner in case of a direct solver
  if (solvertype != INPAR::SOLVER::SolverType::umfpack and
      solvertype != INPAR::SOLVER::SolverType::superlu)
  {
    const auto prectyp =
        Teuchos::getIntegralValue<INPAR::SOLVER::PreconditionerType>(solverparams, "AZPREC");
    switch (prectyp)
    {
      case INPAR::SOLVER::PreconditionerType::multigrid_ml:
      case INPAR::SOLVER::PreconditionerType::multigrid_ml_fluid:
      case INPAR::SOLVER::PreconditionerType::multigrid_ml_fluid2:
      case INPAR::SOLVER::PreconditionerType::multigrid_muelu:
      {
        Teuchos::ParameterList* preclist_ptr = nullptr;
        // switch here between ML and MueLu cases
        if (prectyp == INPAR::SOLVER::PreconditionerType::multigrid_ml or
            prectyp == INPAR::SOLVER::PreconditionerType::multigrid_ml_fluid or
            prectyp == INPAR::SOLVER::PreconditionerType::multigrid_ml_fluid2)
          preclist_ptr = &((solver->Params()).sublist("ML Parameters"));
        else if (prectyp == INPAR::SOLVER::PreconditionerType::multigrid_muelu)
          preclist_ptr = &((solver->Params()).sublist("MueLu Parameters"));
        else
          dserror("please add correct parameter list");

        Teuchos::ParameterList& preclist = *preclist_ptr;
        preclist.set("PDE equations", 1);
        preclist.set("null space: dimension", 1);
        preclist.set("null space: type", "pre-computed");
        preclist.set("null space: add default vectors", false);

        Teuchos::RCP<Epetra_MultiVector> nullspace =
            Teuchos::rcp(new Epetra_MultiVector(noderowmap, 1, true));
        nullspace->PutScalar(1.0);

        preclist.set<Teuchos::RCP<Epetra_MultiVector>>("nullspace", nullspace);
        preclist.set("null space: vectors", nullspace->Values());
        preclist.set("ML validate parameter list", false);
      }
      break;
      case INPAR::SOLVER::PreconditionerType::ilu:
        // do nothing
        break;
      default:
        dserror("You have to choose ML, MueLu or ILU preconditioning");
        break;
    }
  }

  // solution vector based on reduced node row map
  auto nodevec = Teuchos::rcp(new Epetra_MultiVector(noderowmap, numvec));

  switch (solvertype)
  {
    case INPAR::SOLVER::SolverType::belos:
    {
      // solve for numvec rhs at the same time using Belos solver
      CORE::LINALG::SolverParams solver_params;
      solver_params.refactor = true;
      solver_params.reset = true;
      solver->Solve(massmatrix.EpetraOperator(), nodevec, Teuchos::rcpFromRef(rhs), solver_params);
      break;
    }
    default:
    {
      if (numvec != 1 and comm.MyPID() == 0)
      {
        std::cout << "Think about using a Belos solver which can handle several rhs vectors at the "
                     "same time\n";
      }

      // solve for numvec rhs iteratively
      for (int i = 0; i < numvec; i++)
      {
        CORE::LINALG::SolverParams solver_params;
        solver_params.refactor = true;
        solver_params.reset = true;
        solver->Solve(massmatrix.EpetraOperator(), Teuchos::rcp(((*nodevec)(i)), false),
            Teuchos::rcp((rhs(i)), false), solver_params);
      }
      break;
    }
  }

  return nodevec;
}

BACI_NAMESPACE_CLOSE