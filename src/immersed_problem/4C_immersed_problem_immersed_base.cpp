/*----------------------------------------------------------------------*/
/*! \file

\brief base class for all immersed algorithms

\level 2


*----------------------------------------------------------------------*/
#include "4C_immersed_problem_immersed_base.hpp"

#include "4C_adapter_fld_wrapper.hpp"
#include "4C_adapter_str_fsiwrapper_immersed.hpp"
#include "4C_io_control.hpp"
#include "4C_linalg_utils_sparse_algebra_math.hpp"
#include "4C_utils_function_of_time.hpp"

FOUR_C_NAMESPACE_OPEN


Immersed::ImmersedBase::ImmersedBase() : issetup_(false), isinit_(false)
{
  // empty
  return;
}  // ImmersedBase


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::create_volume_condition(
    const Teuchos::RCP<Core::FE::Discretization>& dis, const std::vector<int> dvol_fenode,
    const Core::Conditions::ConditionType condtype, const std::string condname, bool buildgeometry)
{
  // determine id of condition
  std::multimap<std::string, Teuchos::RCP<Core::Conditions::Condition>> allconditions;
  allconditions = dis->GetAllConditions();
  int id = (int)allconditions.size();
  id += 1;

  // build condition
  Teuchos::RCP<Core::Conditions::Condition> condition =
      Teuchos::rcp(new Core::Conditions::Condition(
          id, condtype, buildgeometry, Core::Conditions::geometry_type_volume));

  // add nodes to conditions
  condition->SetNodes(dvol_fenode);

  // add condition to discretization
  dis->SetCondition(condname, condition);

  // fill complete if necessary
  if (!dis->Filled()) dis->fill_complete(false, false, buildgeometry);

  std::map<int, Teuchos::RCP<Core::Elements::Element>>& geom =
      dis->GetCondition(condname)->Geometry();
  std::map<int, Teuchos::RCP<Core::Elements::Element>>::iterator it;
  for (it = geom.begin(); it != geom.end(); it++)
  {
    int id = it->second->Id();
    dis->gElement(id)->SetCondition(condname, condition);
  }

  return;
}  // create_volume_condition


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::build_condition_dof_map(
    const Teuchos::RCP<const Core::FE::Discretization>& dis, const std::string condname,
    const Teuchos::RCP<const Epetra_Map>& cond_dofmap_orig, const int numdof,
    Teuchos::RCP<Epetra_Map>& cond_dofmap)
{
  // declare dof vector
  std::vector<int> mydirichdofs(0);

  // get condition and conditioned nodes
  Core::Conditions::Condition* condition = dis->GetCondition(condname);
  const std::vector<int>* cond_nodes = condition->GetNodes();
  int cond_nodes_size = cond_nodes->size();

  if (cond_nodes_size == 0)
    FOUR_C_THROW("No nodes in nodal cloud of condition %s", condname.c_str());

  // loop over all conditioned nodes
  for (int node = 0; node < cond_nodes_size; node++)
  {
    // get node id
    int nodeid = cond_nodes->at(node);
    // get node pointer
    Core::Nodes::Node* node_ptr = dis->gNode(nodeid);
    if (node_ptr == nullptr) FOUR_C_THROW("Could not get node with id %d", nodeid);

    if (dis->NodeRowMap()->LID(nodeid) != -1)
    {
      // get dofs
      std::vector<int> dofs = dis->Dof(0, node_ptr);

      for (int dim = 0; dim < numdof; ++dim)
      {
        // if not already in original dirich map
        if (cond_dofmap_orig->LID(dofs[dim]) == -1) mydirichdofs.push_back(dofs[dim]);
      }
    }  // if my node
  }    // loop over all conditioned nodes

  int nummydirichvals = mydirichdofs.size();
  cond_dofmap =
      Teuchos::rcp(new Epetra_Map(-1, nummydirichvals, mydirichdofs.data(), 0, dis->Comm()));

  return;
}  // build_condition_dof_row_map


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::DoDirichletCond(const Teuchos::RCP<Epetra_Vector>& statevector,
    const Teuchos::RCP<const Epetra_Vector>& dirichvals,
    const Teuchos::RCP<const Epetra_Map>& dbcmap_new)
{
  int mynumvals = dbcmap_new->NumMyElements();
  double* myvals = dirichvals->Values();

  for (int i = 0; i < mynumvals; ++i)
  {
    int gid = dbcmap_new->GID(i);

#ifdef FOUR_C_ENABLE_ASSERTIONS
    if (mynumvals == 0) FOUR_C_THROW("dbcmap empty!");
    int err = -2;
    int lid = dirichvals->Map().LID(gid);
    err = statevector->ReplaceGlobalValue(gid, 0, myvals[lid]);
    if (err == -1)
      FOUR_C_THROW("VectorIndex >= NumVectors()");
    else if (err == 1)
      FOUR_C_THROW("GlobalRow not associated with calling processor");
    else if (err != -1 and err != 1 and err != 0)
      FOUR_C_THROW("Trouble using ReplaceGlobalValue on fluid state vector. ErrorCode = %d", err);
#else
    int lid = dirichvals->Map().LID(gid);
    statevector->ReplaceGlobalValue(gid, 0, myvals[lid]);
#endif
  }
  return;
}  // DoDirichletCond


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::DoDirichletCond(const Teuchos::RCP<Epetra_Vector>& statevector,
    const Teuchos::RCP<const Epetra_Vector>& dirichvals,
    const Teuchos::RCP<const Epetra_Map>& dbcmap_new,
    const Teuchos::RCP<const Epetra_Map>& dbcmap_orig)
{
  int mynumvals = dbcmap_new->NumMyElements();
  double* myvals = dirichvals->Values();

  for (int i = 0; i < mynumvals; ++i)
  {
    int gid = dbcmap_new->GID(i);

#ifdef FOUR_C_ENABLE_ASSERTIONS
    if (mynumvals == 0) FOUR_C_THROW("dbcmap empty!");
    int err = -2;
    int lid = dirichvals->Map().LID(gid);
    err = statevector->ReplaceGlobalValue(gid, 0, myvals[lid]);
    if (err == -1)
      FOUR_C_THROW("VectorIndex >= NumVectors()");
    else if (err == 1)
      FOUR_C_THROW("GlobalRow not associated with calling processor");
    else if (err != -1 and err != 1 and err != 0)
      FOUR_C_THROW("Trouble using ReplaceGlobalValue on fluid state vector. ErrorCode = %d", err);
#else
    int lid = dirichvals->Map().LID(gid);

    // we do not want to overwrite original values
    if (dbcmap_orig->LID(gid) == -1) statevector->ReplaceGlobalValue(gid, 0, myvals[lid]);
#endif
  }
  return;
}  // DoDirichletCond


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::ApplyDirichlet(
    const Teuchos::RCP<Adapter::StructureWrapper>& field_wrapper,
    const Teuchos::RCP<Core::FE::Discretization>& dis, const std::string condname,
    Teuchos::RCP<Epetra_Map>& cond_dofrowmap, const int numdof,
    const Teuchos::RCP<const Epetra_Vector>& dirichvals)
{
  const Teuchos::RCP<const Epetra_Map> condmap_orig =
      field_wrapper->get_dbc_map_extractor()->cond_map();

  // build map of dofs subjected to Dirichlet condition
  build_condition_dof_map(dis, condname, condmap_orig, numdof, cond_dofrowmap);

  // add adhesion dofs to dbc map
  field_wrapper->AddDirichDofs(cond_dofrowmap);

  // write Dirichlet values to systemvector
  DoDirichletCond(field_wrapper->WriteAccessDispnp(), dirichvals,
      field_wrapper->get_dbc_map_extractor()->cond_map());

  return;
}  // ApplyDirichlet


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::apply_dirichlet_to_fluid(
    const Teuchos::RCP<Adapter::FluidWrapper>& field_wrapper,
    const Teuchos::RCP<Core::FE::Discretization>& dis, const std::string condname,
    Teuchos::RCP<Epetra_Map>& cond_dofrowmap, const int numdof,
    const Teuchos::RCP<const Epetra_Vector>& dirichvals)
{
  // save the original condition map
  const Teuchos::RCP<const Epetra_Map> condmap_orig =
      field_wrapper->get_dbc_map_extractor()->cond_map();

  // build map of dofs subjected to Dirichlet condition
  build_condition_dof_map(dis, condname, condmap_orig, numdof, cond_dofrowmap);

  // add adhesion dofs to dbc map
  field_wrapper->add_dirich_cond(cond_dofrowmap);

  // write Dirichlet values to systemvector
  DoDirichletCond(field_wrapper->WriteAccessVelnp(), dirichvals,
      field_wrapper->get_dbc_map_extractor()->cond_map());

  return;
}  // ApplyDirichlet

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::RemoveDirichlet(const Teuchos::RCP<const Epetra_Map>& cond_dofmap,
    const Teuchos::RCP<Adapter::StructureWrapper>& field_wrapper)
{
  field_wrapper->RemoveDirichDofs(cond_dofmap);
  return;
}  // RemoveDirichlet


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::remove_dirichlet_from_fluid(
    const Teuchos::RCP<const Epetra_Map>& cond_dofmap,
    const Teuchos::RCP<Adapter::FluidWrapper>& field_wrapper)
{
  field_wrapper->remove_dirich_cond(cond_dofmap);
  return;
}  // RemoveDirichlet


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::EvaluateImmersed(Teuchos::ParameterList& params,
    Teuchos::RCP<Core::FE::Discretization> dis, Core::FE::AssembleStrategy* strategy,
    std::map<int, std::set<int>>* elementstoeval,
    Teuchos::RCP<Core::Geo::SearchTree> structsearchtree,
    std::map<int, Core::LinAlg::Matrix<3, 1>>* currpositions_struct, int action,
    bool evaluateonlyboundary)
{
  // pointer to element
  Core::Elements::Element* ele;

  for (std::map<int, std::set<int>>::const_iterator closele = elementstoeval->begin();
       closele != elementstoeval->end(); closele++)
  {
    for (std::set<int>::const_iterator eleIter = (closele->second).begin();
         eleIter != (closele->second).end(); eleIter++)
    {
      ele = dis->gElement(*eleIter);

      Discret::ELEMENTS::FluidImmersedBase* immersedelebase =
          dynamic_cast<Discret::ELEMENTS::FluidImmersedBase*>(ele);
      if (immersedelebase == nullptr)
        FOUR_C_THROW(
            "dynamic cast from Core::Elements::Element* to Discret::ELEMENTS::FluidImmersedBase* "
            "failed");

      // evaluate this element and fill vector with immersed dirichlets
      int row = strategy->FirstDofSet();
      int col = strategy->SecondDofSet();

      params.set<int>("action", action);
      params.set<Teuchos::RCP<Core::Geo::SearchTree>>("structsearchtree_rcp", structsearchtree);
      params.set<std::map<int, Core::LinAlg::Matrix<3, 1>>*>(
          "currpositions_struct", currpositions_struct);
      params.set<int>("Physical Type", Inpar::FLUID::poro_p1);

      Core::Elements::Element::LocationArray la(1);
      immersedelebase->LocationVector(*dis, la, false);
      strategy->ClearElementStorage(la[row].Size(), la[col].Size());

      if (!evaluateonlyboundary)
        immersedelebase->evaluate(params, *dis, la[0].lm_, strategy->Elematrix1(),
            strategy->Elematrix2(), strategy->Elevector1(), strategy->Elevector2(),
            strategy->Elevector3());
      else
      {
        if (immersedelebase->IsBoundaryImmersed())
          immersedelebase->evaluate(params, *dis, la[0].lm_, strategy->Elematrix1(),
              strategy->Elematrix2(), strategy->Elevector1(), strategy->Elevector2(),
              strategy->Elevector3());
      }

      strategy->AssembleVector1(la[row].lm_, la[row].lmowner_);
    }
  }
  return;
}  // EvaluateImmersed

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::evaluate_immersed_no_assembly(Teuchos::ParameterList& params,
    Teuchos::RCP<Core::FE::Discretization> dis, std::map<int, std::set<int>>* elementstoeval,
    Teuchos::RCP<Core::Geo::SearchTree> structsearchtree,
    std::map<int, Core::LinAlg::Matrix<3, 1>>* currpositions_struct, int action)
{
  // pointer to element
  Core::Elements::Element* ele;

  for (std::map<int, std::set<int>>::const_iterator closele = elementstoeval->begin();
       closele != elementstoeval->end(); closele++)
  {
    for (std::set<int>::const_iterator eleIter = (closele->second).begin();
         eleIter != (closele->second).end(); eleIter++)
    {
      ele = dis->gElement(*eleIter);

      Discret::ELEMENTS::FluidImmersedBase* immersedelebase =
          dynamic_cast<Discret::ELEMENTS::FluidImmersedBase*>(ele);
      if (immersedelebase == nullptr)
        FOUR_C_THROW(
            "dynamic cast from Core::Elements::Element* to Discret::ELEMENTS::FluidImmersedBase* "
            "failed");

      // provide important objects to ParameterList
      params.set<int>("action", action);
      params.set<Teuchos::RCP<Core::Geo::SearchTree>>("structsearchtree_rcp", structsearchtree);
      params.set<std::map<int, Core::LinAlg::Matrix<3, 1>>*>(
          "currpositions_struct", currpositions_struct);
      params.set<int>("Physical Type", Inpar::FLUID::poro_p1);
      if (dis->Name() == "fluid")
        params.set<std::string>("immerseddisname", "structure");
      else if (dis->Name() == "porofluid")
        params.set<std::string>("immerseddisname", "cell");
      else
        FOUR_C_THROW("no corresponding immerseddisname set for this type of backgrounddis!");

      // evaluate the element
      Core::LinAlg::SerialDenseMatrix dummymat;
      Core::LinAlg::SerialDenseVector dummyvec;

      Core::Elements::Element::LocationArray la(1);
      immersedelebase->LocationVector(*dis, la, false);

      immersedelebase->evaluate(
          params, *dis, la[0].lm_, dummymat, dummymat, dummyvec, dummyvec, dummyvec);
    }
  }
  return;
}  // evaluate_immersed_no_assembly

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::evaluate_sca_tra_with_internal_communication(
    Teuchos::RCP<Core::FE::Discretization> dis,
    const Teuchos::RCP<const Core::FE::Discretization> idis, Core::FE::AssembleStrategy* strategy,
    std::map<int, std::set<int>>* elementstoeval,
    Teuchos::RCP<Core::Geo::SearchTree> structsearchtree,
    std::map<int, Core::LinAlg::Matrix<3, 1>>* currpositions_struct, Teuchos::ParameterList& params,
    bool evaluateonlyboundary)
{
  // pointer to element
  Core::Elements::Element* ele;
  Core::Elements::Element* iele;

  for (std::map<int, std::set<int>>::const_iterator closele = elementstoeval->begin();
       closele != elementstoeval->end(); closele++)
  {
    for (std::set<int>::const_iterator eleIter = (closele->second).begin();
         eleIter != (closele->second).end(); eleIter++)
    {
      ele = dis->gElement(*eleIter);
      iele = idis->gElement(*eleIter);

      Discret::ELEMENTS::FluidImmersedBase* immersedelebase =
          dynamic_cast<Discret::ELEMENTS::FluidImmersedBase*>(iele);
      if (immersedelebase == nullptr)
        FOUR_C_THROW(
            "dynamic cast from Core::Elements::Element* to Discret::ELEMENTS::FluidImmersedBase* "
            "failed");

      // evaluate this element and fill vector with immersed dirichlets
      int row = strategy->FirstDofSet();
      int col = strategy->SecondDofSet();

      params.set<Teuchos::RCP<Core::Geo::SearchTree>>("structsearchtree_rcp", structsearchtree);
      params.set<std::map<int, Core::LinAlg::Matrix<3, 1>>*>(
          "currpositions_struct", currpositions_struct);
      params.set<int>("Physical Type", Inpar::FLUID::poro_p1);

      Core::Elements::Element::LocationArray la(dis->NumDofSets());
      ele->LocationVector(*dis, la, false);
      strategy->ClearElementStorage(la[row].Size(), la[col].Size());

      if (!evaluateonlyboundary)
        ele->evaluate(params, *dis, la, strategy->Elematrix1(), strategy->Elematrix2(),
            strategy->Elevector1(), strategy->Elevector2(), strategy->Elevector3());
      else
      {
        if (immersedelebase->IsBoundaryImmersed())
          ele->evaluate(params, *dis, la, strategy->Elematrix1(), strategy->Elematrix2(),
              strategy->Elevector1(), strategy->Elevector2(), strategy->Elevector3());
      }

      strategy->AssembleVector1(la[row].lm_, la[row].lmowner_);
    }
  }
}  // EvaluateWithInternalCommunication

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
/// Reduces to standard evaluate_condition on one proc.
/// Evaluate a specific condition using assemble strategy allowing communication at element level
/// until every conditioned element is evaluated. Needed especially during interpolation from an
/// other discretization to the conditioned elements (e.g. in immersed method).
/// The integration point of a conditioned element requesting a quantity may be owned by another
/// proc as the interpolating element providing this quantity.  rauch 05/14
void Immersed::ImmersedBase::evaluate_interpolation_condition(
    Teuchos::RCP<Core::FE::Discretization> evaldis, Teuchos::ParameterList& params,
    Core::FE::AssembleStrategy& strategy, const std::string& condstring, const int condid)
{
#ifdef FOUR_C_ENABLE_ASSERTIONS
  if (!(evaldis->Filled())) FOUR_C_THROW("fill_complete() was not called");
  if (!(evaldis->HaveDofs())) FOUR_C_THROW("assign_degrees_of_freedom() was not called");
#endif

  int row = strategy.FirstDofSet();
  int col = strategy.SecondDofSet();

  // get the current time
  bool usetime = true;
  const double time = params.get("total time", -1.0);
  if (time < 0.0) usetime = false;

  params.set<int>("dummy_call", 0);

  Core::Elements::Element::LocationArray la(evaldis->NumDofSets());

  std::multimap<std::string, Teuchos::RCP<Core::Conditions::Condition>>::iterator fool;

  //----------------------------------------------------------------------
  // loop through conditions and evaluate them if they match the criterion
  //----------------------------------------------------------------------
  for (fool = evaldis->GetAllConditions().begin(); fool != evaldis->GetAllConditions().end();
       ++fool)
  {
    if (fool->first == condstring)
    {
      Core::Conditions::Condition& cond = *(fool->second);
      if (condid == -1 || condid == cond.parameters().get<int>("ConditionID"))
      {
        std::map<int, Teuchos::RCP<Core::Elements::Element>>& geom = cond.Geometry();
        if (geom.empty())
          FOUR_C_THROW(
              "evaluation of condition with empty geometry on proc %d", evaldis->Comm().MyPID());

        std::map<int, Teuchos::RCP<Core::Elements::Element>>::iterator curr;

        // Evaluate Loadcurve if defined. Put current load factor in parameterlist
        const auto* curve = cond.parameters().GetIf<int>("curve");
        int curvenum = -1;
        if (curve) curvenum = *curve;
        double curvefac = 1.0;
        if (curvenum >= 0 && usetime)
        {
          curvefac = Global::Problem::Instance()
                         ->FunctionById<Core::UTILS::FunctionOfTime>(curvenum)
                         .evaluate(time);
        }

        // Get ConditionID of current condition if defined and write value in parameterlist
        const auto* CondID = cond.parameters().GetIf<int>("ConditionID");
        if (CondID)
        {
          params.set("ConditionID", *CondID);
          char factorname[30];
          sprintf(factorname, "LoadCurveFactor %d", *CondID);
          params.set(factorname, curvefac);
        }
        else
        {
          params.set("LoadCurveFactor", curvefac);
        }
        params.set<Teuchos::RCP<Core::Conditions::Condition>>("condition", fool->second);

        int mygeometrysize = -1234;
        if (geom.empty() == true)
          mygeometrysize = 0;
        else
          mygeometrysize = geom.size();
        int maxgeometrysize = -1234;
        evaldis->Comm().MaxAll(&mygeometrysize, &maxgeometrysize, 1);
        curr = geom.begin();

#ifdef FOUR_C_ENABLE_ASSERTIONS
        std::cout << "PROC " << evaldis->Comm().MyPID() << ": mygeometrysize = " << mygeometrysize
                  << " maxgeometrysize = " << maxgeometrysize << std::endl;
#endif


        // enter loop on every proc until the last proc evaluated his last geometry element
        // because there is communication happening inside
        for (int i = 0; i < maxgeometrysize; ++i)
        {
          if (i >= mygeometrysize) params.set<int>("dummy_call", 1);

          // get element location vector and ownerships
          // the LocationVector method will return the the location vector
          // of the dofs this condition is meant to assemble into.
          // These dofs do not need to be the same as the dofs of the element
          // (this is the standard case, though). Special boundary conditions,
          // like weak dirichlet conditions, assemble into the dofs of the parent element.
          curr->second->LocationVector(*evaldis, la, false, condstring, params);

          // get dimension of element matrices and vectors
          // Reshape element matrices and vectors and init to zero

          strategy.ClearElementStorage(la[row].Size(), la[col].Size());

          // call the element specific evaluate method
          int err = curr->second->evaluate(params, *evaldis, la[0].lm_, strategy.Elematrix1(),
              strategy.Elematrix2(), strategy.Elevector1(), strategy.Elevector2(),
              strategy.Elevector3());
          if (err) FOUR_C_THROW("error while evaluating elements");

          // assemble every element contribution only once
          // do not assemble after dummy call for internal communication
          if (i < mygeometrysize)
          {
            // assembly
            int eid = curr->second->Id();
            strategy.AssembleMatrix1(
                eid, la[row].lm_, la[col].lm_, la[row].lmowner_, la[col].stride_);
            strategy.AssembleMatrix2(
                eid, la[row].lm_, la[col].lm_, la[row].lmowner_, la[col].stride_);
            strategy.AssembleVector1(la[row].lm_, la[row].lmowner_);
            strategy.AssembleVector2(la[row].lm_, la[row].lmowner_);
            strategy.AssembleVector3(la[row].lm_, la[row].lmowner_);
          }

          // go to next element
          if (i < (mygeometrysize - 1)) ++curr;

        }  // for 0 to max. geometrysize over all procs
      }    // if check of condid successful
    }      // if condstring found
  }        // for (fool=condition_.begin(); fool!=condition_.end(); ++fool)
  return;
}  // evaluate_interpolation_condition

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::search_potentially_covered_backgrd_elements(
    std::map<int, std::set<int>>* current_subset_tofill,
    Teuchos::RCP<Core::Geo::SearchTree> backgrd_SearchTree, const Core::FE::Discretization& dis,
    const std::map<int, Core::LinAlg::Matrix<3, 1>>& currentpositions,
    const Core::LinAlg::Matrix<3, 1>& point, const double radius, const int label)
{
  *current_subset_tofill =
      backgrd_SearchTree->search_elements_in_radius(dis, currentpositions, point, radius, label);
  return;
}  // search_potentially_covered_backgrd_elements


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::evaluate_subset_elements(Teuchos::ParameterList& params,
    Teuchos::RCP<Core::FE::Discretization> dis, std::map<int, std::set<int>>& elementstoeval,
    int action)
{
  // pointer to element
  Core::Elements::Element* ele;

  // initialize location array
  Core::Elements::Element::LocationArray la(1);

  for (std::map<int, std::set<int>>::const_iterator closele = elementstoeval.begin();
       closele != elementstoeval.end(); closele++)
  {
    for (std::set<int>::const_iterator eleIter = (closele->second).begin();
         eleIter != (closele->second).end(); eleIter++)
    {
      ele = dis->gElement(*eleIter);

      Core::LinAlg::SerialDenseMatrix dummymatrix;
      Core::LinAlg::SerialDenseVector dummyvector;
      ele->evaluate(
          params, *dis, la, dummymatrix, dummymatrix, dummyvector, dummyvector, dummyvector);
    }
  }

  return;
}  // evaluate_subset_elements


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Immersed::ImmersedBase::WriteExtraOutput(const Epetra_Comm& comm, const double time,
    const std::string filenameending, const std::vector<double> valuetowrite,
    const std::vector<double> valuetowrite2, const std::vector<double> valuetowrite3)
{
  // append values to output file
  if (comm.MyPID() == 0)
  {
    const std::string fname1 =
        Global::Problem::Instance()->OutputControlFile()->file_name() + "." + filenameending;

    std::ofstream f1;
    f1.open(fname1.c_str(), std::fstream::ate | std::fstream::app);

    f1 << time << " " << valuetowrite[0] << " " << valuetowrite[1] << " " << valuetowrite[2] << " "
       << valuetowrite[3] << " " << valuetowrite2[0] << " " << valuetowrite2[1] << " "
       << valuetowrite2[2] << " " << valuetowrite2[3] << " " << valuetowrite3[0] << " "
       << valuetowrite3[1] << " " << valuetowrite3[2] << " " << valuetowrite3[3] << "   ";

    f1 << "\n";
    f1.flush();
    f1.close();

  }  // only proc 0 writes
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
std::vector<double> Immersed::ImmersedBase::calc_global_resultantfrom_epetra_vector(
    const Epetra_Comm& comm, const Teuchos::RCP<const Core::FE::Discretization>& dis,
    const Teuchos::RCP<const Epetra_Vector>& vec_epetra)
{
  double summyrowentriesx = 0.0;
  double summyrowentriesy = 0.0;
  double summyrowentriesz = 0.0;
  double result_globalx = 0.0;
  double result_globaly = 0.0;
  double result_globalz = 0.0;
  double result_L2norm = 0.0;

  std::vector<double> result;

  const int nummyrownodes = dis->NumMyRowNodes();
  const int myveclength = vec_epetra->MyLength();

  if (myveclength != nummyrownodes * 3) FOUR_C_THROW("local vector length is invalid!");

  for (int i = 0; i < nummyrownodes; i++)
  {
    summyrowentriesx += vec_epetra->Values()[i * 3 + 0];
    summyrowentriesy += vec_epetra->Values()[i * 3 + 1];
    summyrowentriesz += vec_epetra->Values()[i * 3 + 2];
  }

  comm.Barrier();
  comm.SumAll(&summyrowentriesx, &result_globalx, 1);
  comm.SumAll(&summyrowentriesy, &result_globaly, 1);
  comm.SumAll(&summyrowentriesz, &result_globalz, 1);

  result_L2norm = sqrt(pow(result_globalx, 2) + pow(result_globaly, 2) + pow(result_globalz, 2));

  result.push_back(result_globalx);
  result.push_back(result_globaly);
  result.push_back(result_globalz);
  result.push_back(result_L2norm);

  return result;
}

FOUR_C_NAMESPACE_CLOSE
