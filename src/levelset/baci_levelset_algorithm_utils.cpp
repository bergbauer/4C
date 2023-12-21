/*----------------------------------------------------------------------*/
/*! \file

\brief base level-set algorithm: collection of useful helper functions

    detailed description in header file levelset_algorithm.H

\level 2

 *------------------------------------------------------------------------------------------------*/


#include "baci_io_control.H"
#include "baci_io_pstream.H"
#include "baci_levelset_algorithm.H"
#include "baci_levelset_intersection_utils.H"
#include "baci_lib_globalproblem.H"
#include "baci_lib_utils_parameter_list.H"
#include "baci_linalg_utils_densematrix_communication.H"
#include "baci_linalg_utils_sparse_algebra_create.H"
#include "baci_linalg_utils_sparse_algebra_manipulation.H"
#include "baci_linear_solver_method_linalg.H"
#include "baci_scatra_ele_action.H"
#include "baci_scatra_ele_calc_utils.H"
#include "baci_utils_function.H"

BACI_NAMESPACE_OPEN


/*----------------------------------------------------------------------*
 | initialize or update velocity field                  rasthofer 03/14 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::SetVelocityField(bool init)
{
  // call function of base class
  ScaTraTimIntImpl::SetVelocityField();

  // note: This function is only called from the level-set dyn. This is ok, since
  //       we only want to initialize conveln_ at the beginning of the simulation.
  //       for the remainder, it is updated as usual. For the dependent velocity fields
  //       the base class function is called in PrepareTimeStep().
}


/*----------------------------------------------------------------------*
 | set convective velocity field (+ pressure and acceleration field as  |
 | well as fine-scale velocity field, if required)      rasthofer 11/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::SetVelocityField(Teuchos::RCP<const Epetra_Vector> convvel,
    Teuchos::RCP<const Epetra_Vector> acc, Teuchos::RCP<const Epetra_Vector> vel,
    Teuchos::RCP<const Epetra_Vector> fsvel, bool setpressure, bool init)
{
  // call routine of base class
  ScaTraTimIntImpl::SetVelocityField(convvel, acc, vel, fsvel, setpressure);

  // manipulate velocity field away from the interface
  if (extract_interface_vel_) ManipulateFluidFieldForGfunc();

  // estimate velocity at contact points, i.e., intersection points of interface and (no-slip) walls
  if (cpbc_) ApplyContactPointBoundaryCondition();

  return;
}


/*----------------------------------------------------------------------*
 | add problem depended params for AssembleMatAndRHS    rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::AddProblemSpecificParametersAndVectors(
    Teuchos::ParameterList& params)
{
  // set only special parameters of the solution of the reinitialization equation
  // otherwise we take the standard parameters only
  if (switchreinit_)
  {
    // action for elements
    params.set<bool>("solve reinit eq", true);

    if (reinitaction_ == INPAR::SCATRA::reinitaction_sussman)
    {
      // set initial phi, i.e., solution of level-set equation
      discret_->SetState("phizero", initialphireinit_);
      // TODO: RM if not needed
      discret_->SetState("phin", phin_);

#ifndef USE_PHIN_FOR_VEL
      if (useprojectedreinitvel_ == INPAR::SCATRA::vel_reinit_node_based) CalcNodeBasedReinitVel();
#endif

      // add nodal velocity field, if required
      if (useprojectedreinitvel_ == INPAR::SCATRA::vel_reinit_node_based)
        discret_->AddMultiVectorToParameterList(
            params, "reinitialization velocity field", nb_grad_val_);
    }
    else if (reinitaction_ == INPAR::SCATRA::reinitaction_ellipticeq)
    {
      // add node-based gradient, if required
      if (projection_ == true)
        discret_->AddMultiVectorToParameterList(params, "gradphi", nb_grad_val_);

      // add interface integration cells
      params.set<Teuchos::RCP<std::map<int, CORE::GEO::BoundaryIntCells>>>(
          "boundary cells", interface_eleq_);
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 | capture interface                                    rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::CaptureInterface(
    std::map<int, CORE::GEO::BoundaryIntCells>& interface, const bool writetofile)
{
  double volminus = 0.0;
  double volplus = 0.0;
  double surf = 0.0;
  // reconstruct interface and calculate volumes, etc ...
  SCATRA::LEVELSET::Intersection intersect;
  intersect.CaptureZeroLevelSet(phinp_, discret_, volminus, volplus, surf, interface);

  // do mass conservation check
  MassConservationCheck(volminus, writetofile);

  return;
}


/*----------------------------------------------------------------------*
 | mass conservation check                              rasthofer 09/13 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::MassConservationCheck(
    const double actvolminus, const bool writetofile)
{
  if (myrank_ == 0)
  {
    // compute mass loss
    if (initvolminus_ != 0.0)
    {
      double massloss = -(1. - actvolminus / initvolminus_) * 100.0;

      // 'isnan' seems to work not reliably; error occurs in line above
      if (std::isnan(massloss)) dserror("NaN detected in mass conservation check");

      IO::cout << "---------------------------------------" << IO::endl;
      IO::cout << "           mass conservation" << IO::endl;
      IO::cout << " initial mass: " << std::setprecision(5) << initvolminus_ << IO::endl;
      IO::cout << " final mass:   " << std::setprecision(5) << actvolminus << IO::endl;
      IO::cout << " mass loss:    " << std::setprecision(5) << massloss << "%" << IO::endl;
      IO::cout << "---------------------------------------" << IO::endl;

      if (writetofile)
      {
        const std::string simulation = problem_->OutputControlFile()->FileName();
        const std::string fname = simulation + "_massconservation.relerror";

        if (step_ == 0)
        {
          std::ofstream f;
          f.open(fname.c_str());
          f << "#| Step | Time | mass loss w.r.t. minus domain |\n";
          f << "  " << std::setw(2) << std::setprecision(10) << step_ << "    " << std::setw(3)
            << std::setprecision(5) << time_ << std::setw(8) << std::setprecision(10) << "    "
            << massloss << " "
            << "\n";

          f.flush();
          f.close();
        }
        else
        {
          std::ofstream f;
          f.open(fname.c_str(), std::fstream::ate | std::fstream::app);
          f << "  " << std::setw(2) << std::setprecision(10) << step_ << "    " << std::setw(3)
            << std::setprecision(5) << time_ << std::setw(8) << std::setprecision(10) << "    "
            << massloss << " "
            << "\n";

          f.flush();
          f.close();
        }
      }
    }
    else
    {
      if (step_ > 0)
        IO::cout << " there is no 'minus domain'! -> division by zero checking mass conservation"
                 << IO::endl;
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 | calculate error compared to analytical solution      rasthofer 04/14 |
 *----------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::EvaluateErrorComparedToAnalyticalSol()
{
  const INPAR::SCATRA::CalcErrorLevelSet calcerr =
      INPUT::IntegralValue<INPAR::SCATRA::CalcErrorLevelSet>(*levelsetparams_, "CALCERROR");

  switch (calcerr)
  {
    case INPAR::SCATRA::calcerror_no_ls:  // do nothing (the usual case)
      break;
    case INPAR::SCATRA::calcerror_initial_field:
    {
      if (myrank_ == 0)
      {
        if (step_ == 0)
        {
          const std::string simulation = problem_->OutputControlFile()->FileName();
          const std::string fname = simulation + "_shape.error";

          std::ofstream f;
          f.open(fname.c_str());
          f << "#| Step | Time | L1-err        | Linf-err        |\n";

          f.flush();
          f.close();
        }
      }

      if (step_ == stepmax_)  // do only at the end of the simulation
      {
        // create the parameters for the error calculation
        Teuchos::ParameterList eleparams;
        DRT::UTILS::AddEnumClassToParameterList<SCATRA::Action>(
            "action", SCATRA::Action::calc_error, eleparams);
        eleparams.set<int>("calcerrorflag", calcerr);

        // get initial field
        const Epetra_Map* dofrowmap = discret_->DofRowMap();
        Teuchos::RCP<Epetra_Vector> phiref = Teuchos::rcp(new Epetra_Vector(*dofrowmap, true));

        // get function
        int startfuncno = params_->get<int>("INITFUNCNO");
        if (startfuncno < 1) dserror("No initial field defined!");

        // loop all nodes on the processor
        for (int lnodeid = 0; lnodeid < discret_->NumMyRowNodes(); lnodeid++)
        {
          // get the processor local node
          DRT::Node* lnode = discret_->lRowNode(lnodeid);
          // the set of degrees of freedom associated with the node
          std::vector<int> nodedofset = discret_->Dof(0, lnode);

          int numdofs = nodedofset.size();
          for (int k = 0; k < numdofs; ++k)
          {
            const int dofgid = nodedofset[k];
            int doflid = dofrowmap->LID(dofgid);
            // evaluate component k of spatial function
            double initialval =
                problem_->FunctionById<CORE::UTILS::FunctionOfSpaceTime>(startfuncno - 1)
                    .Evaluate(lnode->X().data(), time_, k);
            int err = phiref->ReplaceMyValues(1, &initialval, &doflid);
            if (err != 0) dserror("dof not on proc");
          }
        }

        // set vector values needed by elements
        discret_->ClearState();
        discret_->SetState("phinp", phinp_);
        discret_->SetState("phiref", phiref);

        // get error and volume
        Teuchos::RCP<CORE::LINALG::SerialDenseVector> errors =
            Teuchos::rcp(new CORE::LINALG::SerialDenseVector(2));
        discret_->EvaluateScalars(eleparams, errors);
        discret_->ClearState();

        double errL1 = (*errors)[0] / (*errors)[1];  // division by thickness of element layer for
                                                     // 2D problems with domain size 1
        Teuchos::RCP<Epetra_Vector> phidiff = Teuchos::rcp(new Epetra_Vector(*phinp_));
        phidiff->Update(-1.0, *phiref, 1.0);
        double errLinf = 0.0;
        phidiff->NormInf(&errLinf);

        const std::string simulation = problem_->OutputControlFile()->FileName();
        const std::string fname = simulation + "_shape.error";

        if (myrank_ == 0)
        {
          std::ofstream f;
          f.open(fname.c_str(), std::fstream::ate | std::fstream::app);
          f << "  " << std::setw(2) << std::setprecision(10) << step_ << "    " << std::setw(3)
            << std::setprecision(5) << time_ << std::setw(8) << std::setprecision(10) << "    "
            << errL1 << std::setw(8) << std::setprecision(10) << "    " << errLinf << " "
            << "\n";

          f.flush();
          f.close();
        }
      }
    }
    break;
    default:
      dserror("Cannot calculate error. Unknown type of analytical test problem");
      break;
  }

  return;
}


/*------------------------------------------------------------------------------------------------*
 | compute convective velocity for contact points no-slip wall and interface      rasthofer 12/13 |
 *------------------------------------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::ApplyContactPointBoundaryCondition()
{
  // get condition
  std::vector<DRT::Condition*> lscontactpoint;
  discret_->GetCondition("LsContact", lscontactpoint);

  // map to store node gid and corrected values
  std::map<int, std::vector<double>> nodal_correction;

  // extract convective velocity field
  Teuchos::RCP<const Epetra_Vector> convel =
      discret_->GetState(NdsVel(), "convective velocity field");
  if (convel == Teuchos::null) dserror("Cannot get state vector convective velocity");

  Teuchos::RCP<Epetra_Vector> convel_new = Teuchos::rcp(new Epetra_Vector(*convel));

  // loop all conditions
  for (std::size_t icond = 0; icond < lscontactpoint.size(); icond++)
  {
    DRT::Condition* mycondition = lscontactpoint[icond];

    // loop all nodes belonging to this condition
    // for these nodes, new values have to be set
    const std::vector<int>* mynodes = mycondition->Nodes();
    for (std::size_t inode = 0; inode < mynodes->size(); inode++)
    {
      // for all nodes on this proc: is this check really necessary here
      if (discret_->HaveGlobalNode((*mynodes)[inode]))
      {
        // get node
        DRT::Node* actnode = discret_->gNode((*mynodes)[inode]);

        // exclude ghosted nodes, since we need all adjacent elements here,
        // which are only available for nodes belonging to the this proc
        if (actnode->Owner() == myrank_)
        {
          // get adjacent elements
          const DRT::Element* const* adjelements = actnode->Elements();

          // initialize vector for averaged center velocity
          // note: velocity in scatra algorithm has three components (see also basic constructor)
          std::vector<double> averagedvel(3);
          for (int rr = 0; rr < 3; rr++) averagedvel[rr] = 0.0;

          // loop all adjacent elements
          for (int iele = 0; iele < actnode->NumElement(); iele++)
          {
            // get discretization type
            if ((adjelements[iele])->Shape() != CORE::FE::CellType::hex8)
              dserror("Currently only hex8 supported");
            const CORE::FE::CellType distype = CORE::FE::CellType::hex8;

            // in case of further distypes, move the following block to a templated function
            {
              // get number of element nodes
              const int nen = CORE::FE::num_nodes<distype>;
              // get number of space dimensions
              const int nsd = CORE::FE::dim<distype>;

              // get nodal values of velocity field from secondary dofset
              DRT::Element::LocationArray la(discret_->NumDofSets());
              adjelements[iele]->LocationVector(*discret_, la, false);
              const std::vector<int>& lmvel = la[NdsVel()].lm_;
              std::vector<double> myconvel(lmvel.size());

              // extract local values from global vector
              DRT::UTILS::ExtractMyValues(*convel, myconvel, lmvel);

              // determine number of velocity related dofs per node
              const int numveldofpernode = lmvel.size() / nen;

              CORE::LINALG::Matrix<nsd, nen> evel(true);

              // loop over number of nodes
              for (int inode = 0; inode < nen; ++inode)
                // loop over number of dimensions
                for (int idim = 0; idim < nsd; ++idim)
                  evel(idim, inode) = myconvel[idim + (inode * numveldofpernode)];

              // use one-point Gauss rule to do calculations at the element center
              // used here to get center coordinates
              CORE::FE::IntPointsAndWeights<nsd> centercoord(
                  SCATRA::DisTypeToStabGaussRule<distype>::rule);
              CORE::LINALG::Matrix<nsd, 1> xsi(true);
              const double* gpcoord = (centercoord.IP().qxg)[0];
              for (int idim = 0; idim < nsd; idim++) xsi(idim, 0) = gpcoord[idim];

              // compute shape functions at element center
              CORE::LINALG::Matrix<nen, 1> funct(true);
              CORE::FE::shape_function<distype>(xsi, funct);

              // get velocity at integration point
              CORE::LINALG::Matrix<nsd, 1> velint(true);
              velint.Multiply(evel, funct);

              // add to averaged velocity vector
              for (int idim = 0; idim < nsd; idim++) averagedvel[idim] += velint(idim, 0);
            }
          }  // end loop elements

          // computed averaged value
          for (int rr = 0; rr < 3; rr++) averagedvel[rr] /= (double)(actnode->NumElement());

          // store value in map, if not yet stored (i.e., multiple conditions intersect at one
          // point)
          if (nodal_correction.find(actnode->Id()) == nodal_correction.end())
            nodal_correction.insert(
                std::pair<int, std::vector<double>>(actnode->Id(), averagedvel));
        }
      }
    }  // end loop nodes
  }    // end loop conditions

  // replace values in velocity vector
  const Epetra_Map* noderowmap = discret_->NodeRowMap();
  for (std::map<int, std::vector<double>>::iterator iter = nodal_correction.begin();
       iter != nodal_correction.end(); iter++)
  {
    const int gnodeid = iter->first;
    const int lnodeid = noderowmap->LID(gnodeid);
    DRT::Node* lnode = discret_->lRowNode(lnodeid);

    std::vector<int> nodedofs = discret_->Dof(NdsVel(), lnode);

    std::vector<double> myvel = iter->second;
    for (int index = 0; index < 3; ++index)
    {
      // get global and local dof IDs
      const int gid = nodedofs[index];
      const int lid = convel_new->Map().LID(gid);
      if (lid < 0) dserror("Local ID not found in map for given global ID!");
      const double convelocity = myvel[index];
      int err = convel_new->ReplaceMyValue(lid, 0, convelocity);
      if (err != 0) dserror("Error while inserting value into vector convel!");
    }
  }

  // update velocity vectors
  discret_->SetState(NdsVel(), "convective velocity field", convel_new);
  discret_->SetState(NdsVel(), "velocity field", convel_new);

  return;
}


/*------------------------------------------------------------------------------------------------*
 | manipulate velocity field away from the interface                              rasthofer 08/11 |
 |                                                                                    DA wichmann |
 *------------------------------------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::ManipulateFluidFieldForGfunc()
{
  // idea: Velocity field at no-slip walls is zero fixing the level-set contours here.
  //       This may result in strong deformations of the level-set field, which may then
  //       convect into the domain crashing the level-set algorithm.
  //       There for the convective velocity field around the interface is extended to the wall.
  if (myrank_ == 0)
    IO::cout << "--- extension of flow field in interface region to entire domain" << IO::endl;

  Teuchos::RCP<const Epetra_Vector> convel_col =
      discret_->GetState(NdsVel(), "convective velocity field");
  if (convel_col == Teuchos::null) dserror("Cannot get state vector convective velocity");
  Teuchos::RCP<Epetra_Vector> convel =
      Teuchos::rcp(new Epetra_Vector(*discret_->DofRowMap(NdsVel()), true));
  CORE::LINALG::Export(*convel_col, *convel);

  // temporary vector for convective velocity (based on dofrowmap of standard (non-XFEM) dofset)
  // remark: operations must not be performed on 'convel', because the vector is accessed by both
  //         master and slave nodes, if periodic bounday conditions are present
  Teuchos::RCP<Epetra_Vector> conveltmp =
      Teuchos::rcp(new Epetra_Vector(*discret_->DofRowMap(NdsVel()), true));

  const int numproc = discret_->Comm().NumProc();
  std::vector<int> allproc(numproc);
  for (int i = 0; i < numproc; ++i) allproc[i] = i;

  //--------------------------------------------------------------------------------------------------
  // due to the PBCs we need to get some info here in order to properly handle it later
  //--------------------------------------------------------------------------------------------------

  // get the following information about the pbc
  // - planenormaldirection e.g. (1,0,0)
  // - minimum in planenormaldirection
  // - maximum in planenormaldirection
  // std::vector<DRT::Condition*>* surfacepbcs = pbc_->ReturnSurfacePBCs();
  // get periodic surface boundary conditions
  std::vector<DRT::Condition*> surfacepbcs;
  discret_->GetCondition("SurfacePeriodic", surfacepbcs);
  if (surfacepbcs.empty()) discret_->GetCondition("LinePeriodic", surfacepbcs);
  std::vector<int> planenormal(0);
  std::vector<double> globalmins(0);
  std::vector<double> globalmaxs(0);
  for (size_t i = 0; i < surfacepbcs.size(); ++i)
  {
    const std::string* ismaster =
        surfacepbcs[i]->Get<std::string>("Is slave periodic boundary condition");
    if (*ismaster == "Master")
    {
      const int masterid = surfacepbcs[i]->GetInt("Id of periodic boundary condition");
      std::vector<int> nodeids(*(surfacepbcs[i]->Nodes()));
      for (size_t j = 0; j < surfacepbcs.size(); ++j)
      {
        const int slaveid = surfacepbcs[j]->GetInt("Id of periodic boundary condition");
        if (masterid == slaveid)
        {
          const std::string* isslave =
              surfacepbcs[j]->Get<std::string>("Is slave periodic boundary condition");
          if (*isslave == "Slave")
          {
            const std::vector<int>* slavenodeids = surfacepbcs[j]->Nodes();
            // append slave node Ids to node Ids for the complete condition
            for (size_t k = 0; k < slavenodeids->size(); ++k)
              nodeids.push_back(slavenodeids->at(k));
          }
        }
      }

      // Get normal direction of pbc plane
      const std::string* pbcplane =
          surfacepbcs[i]->Get<std::string>("degrees of freedom for the pbc plane");
      if (*pbcplane == "yz")
        planenormal.push_back(0);
      else if (*pbcplane == "xz")
        planenormal.push_back(1);
      else if (*pbcplane == "xy")
        planenormal.push_back(2);
      else
        dserror("A PBC condition could not provide a plane normal.");

      double min = +10e19;
      double max = -10e19;
      for (size_t j = 0; j < nodeids.size(); ++j)
      {
        const int gid = nodeids[j];
        const int lid = discret_->NodeRowMap()->LID(gid);
        if (lid < 0) continue;
        const DRT::Node* lnode = discret_->lRowNode(lid);
        const auto& coord = lnode->X();
        if (coord[planenormal.back()] < min) min = coord[planenormal.back()];
        if (coord[planenormal.back()] > max) max = coord[planenormal.back()];
      }
      globalmins.resize(planenormal.size());
      globalmaxs.resize(planenormal.size());
      discret_->Comm().MinAll(&min, &(globalmins.back()), 1);
      discret_->Comm().MaxAll(&max, &(globalmaxs.back()), 1);
    }
  }  // end loop over all surfacepbcs


  // these sets contain the element/node GIDs that have been collected
  Teuchos::RCP<std::set<int>> allcollectednodes = Teuchos::rcp(new std::set<int>);
  Teuchos::RCP<std::set<int>> allcollectedelements = Teuchos::rcp(new std::set<int>);

  // export phinp to column map
  const Teuchos::RCP<Epetra_Vector> phinpcol =
      Teuchos::rcp(new Epetra_Vector(*discret_->DofColMap()));
  CORE::LINALG::Export(*phinp_, *phinpcol);

  // this loop determines how many layers around the cut elements will be collected
  for (int loopcounter = 0; loopcounter < convel_layers_; ++loopcounter)
  {
    if (loopcounter == 0)
    {
      //-------------------------------------------------------------------------------------------------
      // loop over row elements an check whether it has positive and negative phi-values. If it does
      // add the element to the allcollectedelements set.
      //-------------------------------------------------------------------------------------------------
      for (int lroweleid = 0; lroweleid < discret_->NumMyRowElements(); lroweleid++)
      {
        DRT::Element* ele = discret_->lRowElement(lroweleid);
        const int* nodeids = ele->NodeIds();
        bool gotpositivephi = false;
        bool gotnegativephi = false;

        for (int inode = 0; inode < ele->NumNode(); ++inode)
        {
          const int nodegid = nodeids[inode];
          DRT::Node* node = discret_->gNode(nodegid);
          const int dofgid = discret_->Dof(0, node, 0);
          const int doflid = phinpcol->Map().LID(dofgid);
          if (doflid < 0) dserror("Proc %d: Cannot find gid=%d in Epetra_Vector", myrank_, dofgid);

          if (plusDomain((*phinpcol)[doflid]) == false)
            gotnegativephi = true;
          else
            gotpositivephi = true;
        }

        if (gotpositivephi and gotnegativephi) allcollectedelements->insert(ele->Id());
      }
    }
    else
    {
      //------------------------------------------------------------------------------------------
      // now, that we have collected all row nodes for this proc. Its time to get the adjacent
      // elements
      //------------------------------------------------------------------------------------------
      std::set<int>::const_iterator nodeit;
      for (nodeit = allcollectednodes->begin(); nodeit != allcollectednodes->end(); ++nodeit)
      {
        const int nodelid = discret_->NodeRowMap()->LID(*nodeit);
        DRT::Node* node = discret_->lRowNode(nodelid);
        DRT::Element** elements = node->Elements();
        for (int iele = 0; iele < node->NumElement(); ++iele)
        {
          DRT::Element* ele = elements[iele];
          allcollectedelements->insert(ele->Id());
        }
      }  // loop over elements
    }

    //------------------------------------------------------------------------------------------------
    // now that we have collected all elements on this proc, its time to get the adjacent nodes.
    // Afterwards the nodes are communicated in order to obtain the collected row nodes on every
    // proc.
    //------------------------------------------------------------------------------------------------
    std::set<int>::const_iterator eleit;
    Teuchos::RCP<std::map<int, std::vector<int>>> col_pbcmapmastertoslave =
        discret_->GetAllPBCCoupledColNodes();
    for (eleit = allcollectedelements->begin(); eleit != allcollectedelements->end(); ++eleit)
    {
      const int elelid = discret_->ElementColMap()->LID(*eleit);
      DRT::Element* ele = discret_->lColElement(elelid);
      DRT::Node** nodes = ele->Nodes();
      for (int inode = 0; inode < ele->NumNode(); ++inode)
      {
        DRT::Node* node = nodes[inode];

        // now check whether we have a pbc condition on this node
        std::vector<DRT::Condition*> mypbc;
        node->GetCondition("SurfacePeriodic", mypbc);

        if (mypbc.size() == 0)
        {
          allcollectednodes->insert(node->Id());
        }
        else
        {
          // obtain a vector of master and slaves
          const int nodeid = node->Id();
          std::vector<int> pbcnodes;
          for (size_t numcond = 0; numcond < mypbc.size(); ++numcond)
          {
            std::map<int, std::vector<int>>::iterator mapit;
            for (mapit = col_pbcmapmastertoslave->begin(); mapit != col_pbcmapmastertoslave->end();
                 ++mapit)
            {
              if (mapit->first == nodeid)
              {
                pbcnodes.push_back(mapit->first);
                for (size_t i = 0; i < mapit->second.size(); ++i)
                  pbcnodes.push_back(mapit->second[i]);
                break;
              }
              for (size_t isec = 0; isec < mapit->second.size(); ++isec)
              {
                if (mapit->second[isec] == nodeid)
                {
                  pbcnodes.push_back(mapit->first);
                  for (size_t i = 0; i < mapit->second.size(); ++i)
                    pbcnodes.push_back(mapit->second[i]);
                  break;
                }
              }
            }
          }

          for (size_t i = 0; i < pbcnodes.size(); ++i) allcollectednodes->insert(pbcnodes[i]);
        }
      }  // loop over elements' nodes
    }    // loop over elements

    // with all nodes collected it is time to communicate them to all other procs
    // which then eliminate all but their row nodes
    {
      Teuchos::RCP<std::set<int>> globalcollectednodes = Teuchos::rcp(new std::set<int>);
      CORE::LINALG::Gather<int>(
          *allcollectednodes, *globalcollectednodes, numproc, allproc.data(), discret_->Comm());

      allcollectednodes->clear();
      std::set<int>::const_iterator gnodesit;
      for (gnodesit = globalcollectednodes->begin(); gnodesit != globalcollectednodes->end();
           ++gnodesit)
      {
        const int nodegid = (*gnodesit);
        const int nodelid = discret_->NodeRowMap()->LID(nodegid);
        if (nodelid >= 0) allcollectednodes->insert(nodegid);
      }
    }
  }  // loop over layers of elements

  //-----------------------------------------------------------------------------------------------
  // If a node does not have 8 elements in the allcollected elements it must be a the surface
  // and therefore gets added to the surfacenodes set. This set is redundantly available and
  // mereley knows a node's position and velocities
  //-----------------------------------------------------------------------------------------------
  Teuchos::RCP<std::vector<CORE::LINALG::Matrix<3, 2>>> surfacenodes =
      Teuchos::rcp(new std::vector<CORE::LINALG::Matrix<3, 2>>);

  std::set<int>::const_iterator nodeit;
  for (nodeit = allcollectednodes->begin(); nodeit != allcollectednodes->end(); ++nodeit)
  {
    const int nodelid = discret_->NodeRowMap()->LID(*nodeit);
    DRT::Node* node = discret_->lRowNode(nodelid);
    DRT::Element** eles = node->Elements();
    int elementcount = 0;
    for (int iele = 0; iele < node->NumElement(); ++iele)
    {
      DRT::Element* ele = eles[iele];
      std::set<int>::const_iterator foundit = allcollectedelements->find(ele->Id());
      if (foundit != allcollectedelements->end()) elementcount++;
    }

    if (elementcount < 8)
    {
      std::vector<int> nodedofs = discret_->Dof(NdsVel(), node);
      CORE::LINALG::Matrix<3, 2> coordandvel;
      const auto& coord = node->X();
      for (int i = 0; i < 3; ++i)
      {
        // get global and local dof IDs
        const int gid = nodedofs[i];
        const int lid = convel->Map().LID(gid);
        if (lid < 0) dserror("Local ID not found in map for given global ID!");
        coordandvel(i, 0) = coord[i];
        coordandvel(i, 1) = (*convel)[lid];
      }
      surfacenodes->push_back(coordandvel);
    }
  }

  // Now the surfacenodes must be gathered to all procs
  {
    Teuchos::RCP<std::vector<CORE::LINALG::Matrix<3, 2>>> mysurfacenodes = surfacenodes;
    surfacenodes = Teuchos::rcp(new std::vector<CORE::LINALG::Matrix<3, 2>>);

    CORE::LINALG::Gather<CORE::LINALG::Matrix<3, 2>>(
        *mysurfacenodes, *surfacenodes, numproc, allproc.data(), discret_->Comm());
  }

  //----------------------------------------------------------------------------------------------
  // Here we manipulate the velocity vector. If a node is not in allnodes we find the nearest node
  // in surface nodes and use its velocity instead.
  //----------------------------------------------------------------------------------------------
  for (int lnodeid = 0; lnodeid < discret_->NumMyRowNodes(); ++lnodeid)
  {
    DRT::Node* lnode = discret_->lRowNode(lnodeid);
    std::vector<int> nodedofs = discret_->Dof(NdsVel(), lnode);

    CORE::LINALG::Matrix<3, 1> fluidvel(true);

    // extract velocity values (no pressure!) from global velocity vector
    for (int i = 0; i < 3; ++i)
    {
      // get global and local dof IDs
      const int gid = nodedofs[i];
      const int lid = convel->Map().LID(gid);
      if (lid < 0) dserror("Local ID not found in map for given global ID!");

      fluidvel(i) = (*convel)[lid];
    }

    std::set<int>::const_iterator foundit = allcollectednodes->find(lnode->Id());
    if (foundit == allcollectednodes->end())
    {
      // find closest node in surfacenodes
      CORE::LINALG::Matrix<3, 2> closestnodedata(true);
      {
        CORE::LINALG::Matrix<3, 1> nodecoord;
        auto& coord = lnode->X();
        for (int i = 0; i < 3; ++i) nodecoord(i) = coord[i];
        double mindist = 1.0e19;

        //--------------------------------
        // due to the PBCs the node might actually be closer to the
        // surfacenodes then would be calculated if one only considered
        // the actual position of the node. In order to find the
        // smallest distance the node is copied along all PBC directions
        //
        //   +------------------+ - - - - - - - - - -+
        //   +             II   +
        //   +   x        I  I  +    y               +
        //   +             II   +
        //   +------------------+ - - - - - - - - - -+
        //         original           copy
        //
        //   x: current node
        //   y: copy of current node
        //   I: interface
        //   +: pbc

        if (planenormal.size() > 3)
          dserror(
              "Sorry, but currently a maximum of three periodic boundary conditions are supported "
              "by the combustion reinitializer.");

        // since there is no stl pow(INT, INT) function, we calculate it manually
        size_t looplimit = 1;
        for (size_t i = 0; i < planenormal.size(); ++i) looplimit *= 2;

        for (size_t ipbc = 0; ipbc < looplimit; ++ipbc)
        {
          CORE::LINALG::Matrix<3, 1> tmpcoord(nodecoord);

          // determine which pbcs have to be applied
          //
          // loopcounter | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
          // ------------+---+---+---+---+---+---+---+---+
          //  first PBC  |     x       x       x       x
          // second PBC  |         x   x           x   x
          //  third PBC  |                 x   x   x   x
          //
          // this is equivalent to the binary representation
          // of the size_t
          if (ipbc & 0x01)
          {
            const double pbclength = globalmaxs[0] - globalmins[0];
            if (nodecoord(planenormal[0]) > globalmins[0] + pbclength / 2.0)
              tmpcoord(planenormal[0]) = nodecoord(planenormal[0]) - pbclength;
            else
              tmpcoord(planenormal[0]) = nodecoord(planenormal[0]) + pbclength;
          }
          if (ipbc & 0x02)
          {
            const double pbclength = globalmaxs[1] - globalmins[1];
            if (nodecoord(planenormal[1]) > globalmins[1] + pbclength / 2.0)
              tmpcoord(planenormal[1]) = nodecoord(planenormal[1]) - pbclength;
            else
              tmpcoord(planenormal[1]) = nodecoord(planenormal[1]) + pbclength;
          }
          if (ipbc & 0x04)
          {
            const double pbclength = globalmaxs[2] - globalmins[2];
            if (nodecoord(planenormal[2]) > globalmins[2] + pbclength / 2.0)
              tmpcoord(planenormal[2]) = nodecoord(planenormal[2]) - pbclength;
            else
              tmpcoord(planenormal[2]) = nodecoord(planenormal[2]) + pbclength;
          }

          for (size_t i = 0; i < surfacenodes->size(); ++i)
          {
            const double dist = sqrt((tmpcoord(0) - (*surfacenodes)[i](0, 0)) *
                                         (tmpcoord(0) - (*surfacenodes)[i](0, 0)) +
                                     (tmpcoord(1) - (*surfacenodes)[i](1, 0)) *
                                         (tmpcoord(1) - (*surfacenodes)[i](1, 0)) +
                                     (tmpcoord(2) - (*surfacenodes)[i](2, 0)) *
                                         (tmpcoord(2) - (*surfacenodes)[i](2, 0)));
            if (dist < mindist)
            {
              mindist = dist;
              closestnodedata = (*surfacenodes)[i];
            }
          }
        }
      }

      // write new velocities to the current node's dofs
      for (int icomp = 0; icomp < 3; ++icomp)
      {
        // get global and local dof IDs
        const int gid = nodedofs[icomp];
        const int lid = convel->Map().LID(gid);
        if (lid < 0) dserror("Local ID not found in map for given global ID!");

        int err = conveltmp->ReplaceMyValue(lid, 0, closestnodedata(icomp, 1));
        if (err) dserror("could not replace values for convective velocity");
      }
    }
    else
    {
      for (int icomp = 0; icomp < 3; ++icomp)
      {
        // get global and local dof IDs
        const int gid = nodedofs[icomp];
        const int lid = convel->Map().LID(gid);
        if (lid < 0) dserror("Local ID not found in map for given global ID!");

        int err = conveltmp->ReplaceMyValue(lid, 0, fluidvel(icomp));
        if (err) dserror("could not replace values for convective velocity");
      }
    }
  }

  // update velocity vectors
  discret_->SetState(NdsVel(), "convective velocity field", conveltmp);
  discret_->SetState(NdsVel(), "velocity field", conveltmp);


  return;
}


/*----------------------------------------------------------------------------*
 | Get Mass Center, using the smoothing function                 winter 06/14 |
 *----------------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::MassCenterUsingSmoothing()
{
  // set vector values needed by elements
  discret_->ClearState();
  discret_->SetState("phinp", phinp_);

  // create the parameters for the error calculation
  Teuchos::ParameterList eleparams;

  // action for elements
  DRT::UTILS::AddEnumClassToParameterList<SCATRA::Action>(
      "action", SCATRA::Action::calc_mass_center_smoothingfunct, eleparams);

  // give access to interface thickness from smoothing function (TPF module) in element calculations
  eleparams.set<double>(
      "INTERFACE_THICKNESS_TPF", levelsetparams_->get<double>("INTERFACE_THICKNESS_TPF"));

  // get masscenter and volume, last entry of vector is total volume of minus domain.
  Teuchos::RCP<CORE::LINALG::SerialDenseVector> masscenter_and_volume =
      Teuchos::rcp(new CORE::LINALG::SerialDenseVector(nsd_ + 1));
  discret_->EvaluateScalars(eleparams, masscenter_and_volume);
  discret_->ClearState();

  std::vector<double> center(nsd_);

  for (int idim = 0; idim < nsd_; idim++)
  {
    center[idim] = masscenter_and_volume->values()[idim] / (masscenter_and_volume->values()[nsd_]);
  }

  if (nsd_ != 3)
    dserror("Writing the mass center only available for 3 dimensional problems currently.");

  if (discret_->Comm().MyPID() == 0)
  {
    // write to file
    const std::string simulation = problem_->OutputControlFile()->FileName();
    const std::string fname = simulation + "_center_of_mass.txt";

    if (Step() == 0)
    {
      std::ofstream f;
      f.open(fname.c_str());
      f << "#| Step | Time |       x       |       y       |       z       |\n";
      f << "  " << std::setw(2) << std::setprecision(10) << Step() << "    " << std::setw(3)
        << std::setprecision(5) << Time() << std::setw(4) << std::setprecision(8) << "  "
        << center[0] << "    " << std::setprecision(8) << center[1] << "    "
        << std::setprecision(8) << center[2] << " "
        << "\n";

      f.flush();
      f.close();
    }
    else
    {
      std::ofstream f;
      f.open(fname.c_str(), std::fstream::ate | std::fstream::app);
      f << "  " << std::setw(2) << std::setprecision(10) << Step() << "    " << std::setw(3)
        << std::setprecision(5) << Time() << std::setw(4) << std::setprecision(8) << "  "
        << center[0] << "    " << std::setprecision(8) << center[1] << "    "
        << std::setprecision(8) << center[2] << " "
        << "\n";

      f.flush();
      f.close();
    }
  }

  return;

}  // SCATRA::LevelSetAlgorithm::MassCenterUsingSmoothing


/*----------------------------------------------------------------------------*
 | redistribute the scatra discretization and vectors         rasthofer 07/11 |
 | according to nodegraph according to nodegraph              DA wichmann     |
 *----------------------------------------------------------------------------*/
void SCATRA::LevelSetAlgorithm::Redistribute(const Teuchos::RCP<Epetra_CrsGraph>& nodegraph)
{
  // TODO: works if and only if discretization has already been redistributed
  //      change this and use unused nodegraph
  // TODO: does not work for gen-alpha, since time-integration dependent vectors have
  //      to be redistributed, too
  dserror("Fix Redistribution!");
  //--------------------------------------------------------------------
  // Now update all Epetra_Vectors and Epetra_Matrix to the new dofmap
  //--------------------------------------------------------------------

  discret_->ComputeNullSpaceIfNecessary(solver_->Params(), true);

  // -------------------------------------------------------------------
  // get a vector layout from the discretization to construct matching
  // vectors and matrices: local <-> global dof numbering
  // -------------------------------------------------------------------
  const Epetra_Map* dofrowmap = discret_->DofRowMap();

  // initialize standard (stabilized) system matrix (and save its graph!)
  // in standard case, but do not save the graph if fine-scale subgrid
  // diffusivity is used in non-incremental case
  if (fssgd_ != INPAR::SCATRA::fssugrdiff_no and not incremental_)
    // sysmat_ = Teuchos::rcp(new CORE::LINALG::SparseMatrix(*dofrowmap,27));
    // cf constructor
    sysmat_ = Teuchos::rcp(new CORE::LINALG::SparseMatrix(*dofrowmap, 27, false, true));
  else
    sysmat_ = Teuchos::rcp(new CORE::LINALG::SparseMatrix(*dofrowmap, 27, false, true));

  // -------------------------------------------------------------------
  // create vectors containing problem variables
  // -------------------------------------------------------------------

  // solutions at time n+1 and n
  Teuchos::RCP<Epetra_Vector> old;
  Teuchos::RCP<Epetra_MultiVector> oldMulti;

  if (phinp_ != Teuchos::null)
  {
    old = phinp_;
    phinp_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *phinp_);
  }

  if (phin_ != Teuchos::null)
  {
    old = phin_;
    phin_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *phin_);
  }

  // temporal solution derivative at time n+1
  if (phidtnp_ != Teuchos::null)
  {
    old = phidtnp_;
    phidtnp_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *phidtnp_);
  }

  // temporal solution derivative at time n
  if (phidtn_ != Teuchos::null)
  {
    old = phidtn_;
    phidtn_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *phidtn_);
  }

  // history vector (a linear combination of phinm, phin (BDF)
  // or phin, phidtn (One-Step-Theta, Generalized-alpha))
  if (hist_ != Teuchos::null)
  {
    old = hist_;
    hist_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *hist_);
  }

  // -------------------------------------------------------------------
  // create vectors associated to boundary conditions
  // -------------------------------------------------------------------
  // a vector of zeros to be used to enforce zero dirichlet boundary conditions
  if (zeros_ != Teuchos::null)
  {
    old = zeros_;
    zeros_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *zeros_);
  }

  // -------------------------------------------------------------------
  // create vectors associated to solution process
  // -------------------------------------------------------------------
  // the vector containing body and surface forces
  if (neumann_loads_ != Teuchos::null)
  {
    old = neumann_loads_;
    neumann_loads_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *neumann_loads_);
  }

  // the residual vector --- more or less the rhs
  if (residual_ != Teuchos::null)
  {
    old = residual_;
    residual_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *residual_);
  }

  // residual vector containing the normal boundary fluxes
  if (trueresidual_ != Teuchos::null)
  {
    old = trueresidual_;
    trueresidual_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *trueresidual_);
  }

  // incremental solution vector
  if (increment_ != Teuchos::null)
  {
    old = increment_;
    increment_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *increment_);
  }

  // subgrid-diffusivity(-scaling) vector
  // (used either for AVM3 approach or temperature equation
  //  with all-scale subgrid-diffusivity model)
  if (subgrdiff_ != Teuchos::null)
  {
    old = subgrdiff_;
    subgrdiff_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *subgrdiff_);
  }

  if (initialphireinit_ != Teuchos::null)
  {
    old = initialphireinit_;
    initialphireinit_ = CORE::LINALG::CreateVector(*dofrowmap, true);
    CORE::LINALG::Export(*old, *initialphireinit_);
  }

  if (fssgd_ != INPAR::SCATRA::fssugrdiff_no)
  {
    dserror("No redistribution for AVM3 subgrid stuff.");
  }

  if (discret_->Comm().MyPID() == 0) std::cout << "done" << std::endl;

  return;
}  // SCATRA::ScaTraTimIntImpl::Redistribute

BACI_NAMESPACE_CLOSE
