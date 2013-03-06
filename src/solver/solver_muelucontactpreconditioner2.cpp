/*
 * solver_muelucontactpreconditioner2.cpp
 *
 *  Created on: Sep 7, 2012
 *      Author: wiesner
 */

#ifdef HAVE_MueLu
#ifdef HAVE_Trilinos_Q1_2013

#include "../drt_lib/drt_dserror.H"

#include <MueLu_ConfigDefs.hpp>

// Teuchos
#include <Teuchos_RCP.hpp>
#include <Teuchos_ArrayRCP.hpp>
#include <Teuchos_ParameterList.hpp>
#include <Teuchos_CommandLineProcessor.hpp>
#include <Teuchos_GlobalMPISession.hpp>
#include <Teuchos_DefaultComm.hpp>

// Xpetra
//#include <Xpetra_MultiVector.hpp>
#include <Xpetra_MultiVectorFactory.hpp>
#include <Xpetra_MapExtractorFactory.hpp>

// MueLu
#include <MueLu.hpp>
#include <MueLu_RAPFactory.hpp>
#include <MueLu_TrilinosSmoother.hpp>
#include <MueLu_DirectSolver.hpp>
#include <MueLu_SmootherPrototype_decl.hpp>

#include <MueLu_CoalesceDropFactory.hpp>
#include <MueLu_UncoupledAggregationFactory.hpp>
#include <MueLu_TentativePFactory.hpp>
#include <MueLu_SaPFactory.hpp>
#include <MueLu_PgPFactory.hpp>
//#include <MueLu_InjectionPFactory.hpp>
#include <MueLu_GenericRFactory.hpp>
#include <MueLu_TransPFactory.hpp>
#include <MueLu_VerbosityLevel.hpp>
#include <MueLu_SmootherFactory.hpp>
#include <MueLu_NullspaceFactory.hpp>
#include <MueLu_Aggregates.hpp>
#include <MueLu_MapTransferFactory.hpp>
#include <MueLu_AggregationExportFactory.hpp>
#include <MueLu_IfpackSmoother.hpp>

#include <MueLu_MLParameterListInterpreter.hpp>

// header files for default types, must be included after all other MueLu/Xpetra headers
#include <MueLu_UseDefaultTypes.hpp> // => Scalar=double, LocalOrdinal=GlobalOrdinal=int

#include <MueLu_UseShortNames.hpp>

#include <MueLu_EpetraOperator.hpp> // Aztec interface

#include "muelu/muelu_ContactAFilterFactory_decl.hpp"
#include "muelu/muelu_ContactTransferFactory_decl.hpp"
#include "muelu/MueLu_MyTrilinosSmoother_decl.hpp"
#include "muelu/MueLu_IterationAFactory_decl.hpp"
#include "muelu/MueLu_SelectiveSaPFactory_decl.hpp"

#include "solver_muelucontactpreconditioner2.H"

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
LINALG::SOLVER::MueLuContactPreconditioner2::MueLuContactPreconditioner2( FILE * outfile, Teuchos::ParameterList & mllist )
  : PreconditionerType( outfile ),
    mllist_( mllist )
{
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
void LINALG::SOLVER::MueLuContactPreconditioner2::Setup( bool create,
                                              Epetra_Operator * matrix,
                                              Epetra_MultiVector * x,
                                              Epetra_MultiVector * b )
{
  SetupLinearProblem( matrix, x, b );

  if ( create )
  {
    Epetra_CrsMatrix* A = dynamic_cast<Epetra_CrsMatrix*>( matrix );
    if ( A==NULL )
      dserror( "CrsMatrix expected" );

    // free old matrix first
    P_       = Teuchos::null;
    Pmatrix_ = Teuchos::null;

    // create a copy of the scaled matrix
    // so we can reuse the preconditioner
    Pmatrix_ = Teuchos::rcp(new Epetra_CrsMatrix(*A));

    // wrap Epetra_CrsMatrix to Xpetra::Operator for use in MueLu
    Teuchos::RCP<Xpetra::CrsMatrix<SC,LO,GO,NO,LMO > > mueluA  = Teuchos::rcp(new Xpetra::EpetraCrsMatrix(Pmatrix_));
    Teuchos::RCP<Xpetra::Matrix<SC,LO,GO,NO,LMO> >   mueluOp = Teuchos::rcp(new Xpetra::CrsMatrixWrap<SC,LO,GO,NO,LMO>(mueluA));

    // prepare nullspace vector for MueLu
    int numdf = mllist_.get<int>("PDE equations",-1);
    int dimns = mllist_.get<int>("null space: dimension",-1);
    if(dimns == -1 || numdf == -1) dserror("Error: PDE equations or null space dimension wrong.");
    Teuchos::RCP<const Xpetra::Map<LO,GO,NO> > rowMap = mueluA->getRowMap();

    Teuchos::RCP<MultiVector> nspVector = Xpetra::MultiVectorFactory<Scalar, LocalOrdinal, GlobalOrdinal, Node>::Build(rowMap,dimns,true);
    Teuchos::RCP<std::vector<double> > nsdata = mllist_.get<Teuchos::RCP<std::vector<double> > >("nullspace",Teuchos::null);

    for ( size_t i=0; i < Teuchos::as<size_t>(dimns); i++) {
        Teuchos::ArrayRCP<Scalar> nspVectori = nspVector->getDataNonConst(i);
        const size_t myLength = nspVector->getLocalLength();
        for(size_t j=0; j<myLength; j++) {
                nspVectori[j] = (*nsdata)[i*myLength+j];
        }
    }

    // remove unsupported flags
    mllist_.remove("aggregation: threshold",false); // no support for aggregation: threshold TODO

    // Setup MueLu Hierarchy
    //Teuchos::RCP<Hierarchy> H = MLInterpreter::Setup(mllist_, mueluOp, nspVector);
    Teuchos::RCP<Hierarchy> H = SetupHierarchy(mllist_, mueluOp, nspVector);

    // set preconditioner
    P_ = Teuchos::rcp(new MueLu::EpetraOperator(H));

  }
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
Teuchos::RCP<Hierarchy> LINALG::SOLVER::MueLuContactPreconditioner2::SetupHierarchy(
    const Teuchos::ParameterList & params,
    const Teuchos::RCP<Matrix> & A,
    const Teuchos::RCP<MultiVector> nsp)
{
  //Teuchos::RCP<Teuchos::FancyOStream> out = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));

  //Teuchos::RCP<Teuchos::FancyOStream> out = Teuchos::fancyOStream(Teuchos::rcpFromRef(std::cout));

  // read in common parameters
  int maxLevels = 10;       // multigrid prameters
  int verbosityLevel = 10;  // verbosity level
  int maxCoarseSize = 50;
  int nDofsPerNode = 1;         // coalesce and drop parameters
  //double agg_threshold = 0.0;   // aggregation parameters
  double agg_damping = 4/3;
  //int    agg_smoothingsweeps = 1;
  int    minPerAgg = 3;       // optimal for 2d
  int    maxNbrAlreadySelected = 0;
  std::string agg_type = "Uncoupled";
  //bool   bEnergyMinimization = false; // PGAMG
  if(params.isParameter("max levels")) maxLevels = params.get<int>("max levels");
  if(params.isParameter("ML output"))  verbosityLevel = params.get<int>("ML output");
  if(params.isParameter("coarse: max size")) maxCoarseSize = params.get<int>("coarse: max size");
  if(params.isParameter("PDE equations")) nDofsPerNode = params.get<int>("PDE equations");
  //if(params.isParameter("aggregation: threshold"))          agg_threshold       = params.get<double>("aggregation: threshold");
  if(params.isParameter("aggregation: damping factor"))     agg_damping         = params.get<double>("aggregation: damping factor");
  //if(params.isParameter("aggregation: smoothing sweeps"))   agg_smoothingsweeps = params.get<int>   ("aggregation: smoothing sweeps");
  if(params.isParameter("aggregation: type"))               agg_type            = params.get<std::string> ("aggregation: type");
  if(params.isParameter("aggregation: nodes per aggregate"))minPerAgg           = params.get<int>("aggregation: nodes per aggregate");
  //if(params.isParameter("energy minimization: enable"))  bEnergyMinimization = params.get<bool>("energy minimization: enable");

  // set DofsPerNode in A operator
  A->SetFixedBlockSize(nDofsPerNode);

  // translate verbosity parameter
  Teuchos::EVerbosityLevel eVerbLevel = Teuchos::VERB_NONE;
  if(verbosityLevel == 0)  eVerbLevel = Teuchos::VERB_NONE;
  if(verbosityLevel > 0 )  eVerbLevel = Teuchos::VERB_LOW;
  if(verbosityLevel > 4 )  eVerbLevel = Teuchos::VERB_MEDIUM;
  if(verbosityLevel > 7 )  eVerbLevel = Teuchos::VERB_HIGH;
  if(verbosityLevel > 9 )  eVerbLevel = Teuchos::VERB_EXTREME;


  // extract additional maps from parameter list
  // these maps are provided by the STR::TimInt::PrepareContactMeshtying routine, that
  // has access to the contact manager class
  Teuchos::RCP<Epetra_Map> epMasterDofMap = Teuchos::null;
  Teuchos::RCP<Epetra_Map> epSlaveDofMap = Teuchos::null;
  Teuchos::RCP<Epetra_Map> epActiveDofMap = Teuchos::null;
  Teuchos::RCP<Epetra_Map> epInnerDofMap = Teuchos::null;
  Teuchos::RCP<Map> xSingleNodeAggMap = Teuchos::null;
  Teuchos::RCP<Map> xNearZeroDiagMap = Teuchos::null;
  if(params.isSublist("Linear System properties")) {
    const Teuchos::ParameterList & linSystemProps = params.sublist("Linear System properties");
    // extract information provided by solver (e.g. PermutedAztecSolver)
    epMasterDofMap = linSystemProps.get<Teuchos::RCP<Epetra_Map> > ("contact masterDofMap");
    epSlaveDofMap =  linSystemProps.get<Teuchos::RCP<Epetra_Map> > ("contact slaveDofMap");
    epActiveDofMap = linSystemProps.get<Teuchos::RCP<Epetra_Map> > ("contact activeDofMap");
    epInnerDofMap  = linSystemProps.get<Teuchos::RCP<Epetra_Map> > ("contact innerDofMap");
    if(linSystemProps.isParameter("non diagonal-dominant row map"))
      xSingleNodeAggMap = linSystemProps.get<Teuchos::RCP<Map> > ("non diagonal-dominant row map");
    if(linSystemProps.isParameter("near-zero diagonal row map"))
      xNearZeroDiagMap  = linSystemProps.get<Teuchos::RCP<Map> > ("near-zero diagonal row map");
  }

  std::cout << "masterDofMap" << std::endl;
  std::cout << *epMasterDofMap << std::endl;
  std::cout << "slaveDofMap" << std::endl;
  std::cout << *epSlaveDofMap << std::endl;
  std::cout << "activeDofMap" << std::endl;
  std::cout << *epActiveDofMap << std::endl;

  // transform Epetra maps to Xpetra maps (if necessary)
  Teuchos::RCP<const Map> xfullmap = A->getRowMap(); // full map (MasterDofMap + SalveDofMap + InnerDofMap)
  Teuchos::RCP<Xpetra::EpetraMap> xMasterDofMap  = Teuchos::rcp(new Xpetra::EpetraMap( epMasterDofMap ));
  Teuchos::RCP<Xpetra::EpetraMap> xSlaveDofMap   = Teuchos::rcp(new Xpetra::EpetraMap( epSlaveDofMap  ));
  //Teuchos::RCP<Xpetra::EpetraMap> xActiveDofMap  = Teuchos::rcp(new Xpetra::EpetraMap( epActiveDofMap ));
  //Teuchos::RCP<Xpetra::EpetraMap> xInnerDofMap   = Teuchos::rcp(new Xpetra::EpetraMap( epInnerDofMap  )); // TODO check me


  ///////////////////////////////////////////////////////////

  // fill hierarchy
  Teuchos::RCP<Hierarchy> hierarchy = Teuchos::rcp(new Hierarchy(A));
  hierarchy->SetDefaultVerbLevel(MueLu::toMueLuVerbLevel(eVerbLevel));
  hierarchy->SetMaxCoarseSize(Teuchos::as<Xpetra::global_size_t>(maxCoarseSize)/*+nSlaveDofs*/);
  //hierarchy->SetDebug(true);

  /*int timestep = mllist_.get<int>("time-step");
  int newtoniter = mllist_.get<int>("newton-iter");
  std::stringstream str; str << "t" << timestep << "_n" << newtoniter;
  hierarchy->SetDebugPrefix(str.str());*/

  ///////////////////////////////////////////////////////////

  // set fine level nullspace
  // use given fine level null space or extract pre-computed nullspace from ML parameter list
 Teuchos::RCP<MueLu::Level> Finest = hierarchy->GetLevel();  // get finest level

 Finest->Set("A",A); // not necessary


  if (nsp != Teuchos::null) {
    Finest->Set("Nullspace",nsp);                       // set user given null space
  } else {
    std::string type = "";
    if(params.isParameter("null space: type")) type = params.get<std::string>("null space: type");
    if(type != "pre-computed") dserror("MueLu::Interpreter: no valid nullspace (no pre-computed null space). error.");
    int dimns = -1;
    if(params.isParameter("null space: dimension")) dimns = params.get<int>("null space: dimension");
    if(dimns == -1) dserror( "MueLu::Interpreter: no valid nullspace (nullspace dim = -1). error.");

    const Teuchos::RCP<const Map> rowMap = A->getRowMap();
    Teuchos::RCP<MultiVector> nspVector = MultiVectorFactory::Build(rowMap,dimns,true);
    double* nsdata = NULL;
    if(params.isParameter("null space: vectors")) nsdata = params.get<double*>("null space: vectors");
    if(nsdata == NULL) dserror("MueLu::Interpreter: no valid nullspace (nsdata = NULL). error.");

    for ( size_t i=0; i < Teuchos::as<size_t>(dimns); i++) {
      Teuchos::ArrayRCP<Scalar> nspVectori = nspVector->getDataNonConst(i);
      const size_t myLength = nspVector->getLocalLength();
      for(size_t j=0; j<myLength; j++) {
        nspVectori[j] = nsdata[i*myLength+j];
      }

    }
    Finest->Set("Nullspace",nspVector);                       // set user given null space
  }


  ///////////////////////////////////////////////////////////////////////
  // declare "SingleNodeAggDofMap" on finest level
  // this map marks the problematic rows of the input matrix
  // these problematic rows are skipped within the multigrid smoothers
  // -> MyTrilinosSmoother, IterationAFactory
  if(xSingleNodeAggMap != Teuchos::null)
   Finest->Set("SingleNodeAggDofMap", Teuchos::rcp_dynamic_cast<const Xpetra::Map<LO,GO,Node> >(xSingleNodeAggMap));

  // for the Jacobi/SGS smoother we wanna change the input matrix A and set Dirichlet bcs for the (active?) slave dofs
  // TODO what if xSingleNodeAggMap == Teuchos::null?
  Teuchos::RCP<FactoryBase> singleNodeAFact =
       Teuchos::rcp(new MueLu::IterationAFactory<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap",MueLu::NoFactory::getRCP()));

  ///////////////////////////////////////////////////////////////////////
  // declare "SlaveDofMap" on finest level
  // this map marks the slave DOFs (ignoring permutations) which shall
  // be excluded from transfer operator smoothing
  // -> SelectiveSaPFactory
  if(xSlaveDofMap != Teuchos::null)
    Finest->Set("SlaveDofMap", Teuchos::rcp_dynamic_cast<const Xpetra::Map<LO,GO,Node> >(xSlaveDofMap));

  ///////////////////////////////////////////////////////////////////////
  // declare "MasterDofMap" on finest level
  // this map marks the master DOFs (ignoring permutations)
  // needed together with SlaveDofMap to define a segregated matrix, used for
  // building aggregates
  // -> ContactAFilterFactory
  if(xMasterDofMap != Teuchos::null)
    Finest->Set("MasterDofMap", Teuchos::rcp_dynamic_cast<const Xpetra::Map<LO,GO,Node> >(xMasterDofMap));

  // for the Jacobi/SGS smoother we wanna change the input matrix A and set Dirichlet bcs for the (active?) slave dofs
  Teuchos::RCP<Factory> segregatedAFact =
       Teuchos::rcp(new MueLu::ContactAFilterFactory<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>());
  segregatedAFact->SetParameter("Input matrix name",Teuchos::ParameterEntry(std::string("A")));
  segregatedAFact->SetParameter("Map block 1 name",Teuchos::ParameterEntry(std::string("SlaveDofMap")));
  segregatedAFact->SetParameter("Map block 2 name",Teuchos::ParameterEntry(std::string("MasterDofMap")));

  ///////////////////////////////////////////////////////////////////////
  // declare "NearZeroDiagMap" on finest level
  // this map marks rows of A with a near zero diagonal entry (usually the
  // tolerance is 1e-5). This is a sign of a badly behaving matrix. Usually
  // one should not used smoothed aggregation for these type of problems but
  // rather try plain aggregation (PA-AMG)
  // -> SelectiveSaPFactory
  if(xNearZeroDiagMap != Teuchos::null)
    Finest->Set("NearZeroDiagMap", Teuchos::rcp_dynamic_cast<const Xpetra::Map<LO,GO,Node> >(xNearZeroDiagMap));

  ///////////////////////////////////////////////////////////////////////
  // keep factories

  // keep singleNodeAFact since it's needed in the solution phase by MyTrilinosSmoother
  if(xSingleNodeAggMap != Teuchos::null)
    Finest->Keep("A",singleNodeAFact.get());

  // Coalesce and drop factory with constant number of Dofs per freedom
  // note: coalescing based on original matrix A
  Teuchos::RCP<CoalesceDropFactory> dropFact = Teuchos::rcp(new CoalesceDropFactory());
  dropFact->SetFactory("A",segregatedAFact);

  // aggregation factory
  Teuchos::RCP<UncoupledAggregationFactory> UCAggFact = Teuchos::rcp(new UncoupledAggregationFactory(/*dropFact*/));
  UCAggFact->SetFactory("Graph", dropFact);
  UCAggFact->SetFactory("DofsPerNode", dropFact);
  UCAggFact->SetParameter("MaxNeighAlreadySelected",Teuchos::ParameterEntry(maxNbrAlreadySelected));
  UCAggFact->SetParameter("MinNodesPerAggregate",Teuchos::ParameterEntry(minPerAgg));
  UCAggFact->SetParameter("Ordering",Teuchos::ParameterEntry(MueLu::AggOptions::GRAPH));

  if(xSingleNodeAggMap != Teuchos::null) { // declare single node aggregates
    //UCAggFact->SetOnePtMapName("SingleNodeAggDofMap", MueLu::NoFactory::getRCP());
    UCAggFact->SetParameter("OnePt aggregate map name", Teuchos::ParameterEntry("SingleNodeAggDofMap"));
    UCAggFact->SetFactory("OnePt aggregate map factory", MueLu::NoFactory::getRCP());
  }

  Teuchos::RCP<PFactory> PFact;
  Teuchos::RCP<TwoLevelFactoryBase> RFact;

  Teuchos::RCP<PFactory> PtentFact = Teuchos::rcp(new TentativePFactory());

  // choose either nonsmoothed transfer operators or
  // PG-AMG smoothed aggregation transfer operators
  // note:
  //  - SA-AMG is not working properly (probably due to problematic Dinv scaling with zeros on diagonal) TODO handling of zeros on diagonal in SaPFactory
  //  - PG-AMG has some special handling for zeros on diagonal (avoid NaNs)
  //    avoid local damping factory omega==1 -> oversmoothing, leads to zero rows in P
  //    use matrix A with artificial Dirichlet bcs for prolongator smoothing
  // if agg_damping == 0.0 -> PA-AMG else PG-AMG
#if 1
  if (agg_damping == 0.0) {
    // tentative prolongation operator (PA-AMG)
    PFact = PtentFact;
    RFact = Teuchos::rcp( new TransPFactory() );
  } else if (agg_damping > 0.0) {
    // smoothed aggregation (SA-AMG)
    PFact  = Teuchos::rcp( new MueLu::SelectiveSaPFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps>() );

    // feed SelectiveSAPFactory with information
    PFact->SetFactory("P",PtentFact);
    // use user-given damping parameter
    PFact->SetParameter("Damping factor", Teuchos::ParameterEntry(agg_damping));
    PFact->SetParameter("Damping strategy", Teuchos::ParameterEntry(std::string("User")));
    // only prolongator smoothing for transfer operator basis functions which
    // correspond to non-slave rows in (permuted) matrix A
    // We use the tentative prolongator to detect the corresponding prolongator basis functions for given row gids.
    // Note: this ignores the permutations in A. In case, the matrix A has been permuted it can happen
    //       that problematic columns in Ptent are not corresponding to columns that belong to the
    //       with nonzero entries in slave rows. // TODO think more about this -> aggregation
    PFact->SetParameter("NonSmoothRowMapName", Teuchos::ParameterEntry(std::string("SlaveDofMap")));
    //PFact->SetParameter("NonSmoothRowMapName", Teuchos::ParameterEntry(std::string("")));
    PFact->SetFactory("NonSmoothRowMapFactory", MueLu::NoFactory::getRCP());

    // provide diagnostics of diagonal entries of current matrix A
    // if the solver object detects some significantly small entries on diagonal the contact
    // preconditioner can decide to skip transfer operator smoothing to increase robustness
    PFact->SetParameter("NearZeroDiagMapName", Teuchos::ParameterEntry(std::string("NearZeroDiagMap")));
    //PFact->SetParameter("NearZeroDiagMapName", Teuchos::ParameterEntry(std::string("")));
    PFact->SetFactory("NearZeroDiagMapFactory", MueLu::NoFactory::getRCP());

    PFact->SetFactory("A",segregatedAFact);

    RFact  = Teuchos::rcp( new GenericRFactory() );
  } else {
    // Petrov Galerkin PG-AMG smoothed aggregation (energy minimization in ML)
    PFact  = Teuchos::rcp( new PgPFactory() );
    PFact->SetFactory("P",PtentFact);
    PFact->SetFactory("A",segregatedAFact);
    //PFact->SetFactory("A",singleNodeAFact);
    //PFact->SetFactory("A",slaveTransferAFactory);  // produces nans
    RFact  = Teuchos::rcp( new GenericRFactory() );
  }
#else
  PFact = PtentFact;
  Teuchos::RCP<InjectionPFactory> Pinj = Teuchos::rcp(new InjectionPFactory());
  RFact = Teuchos::rcp( new TransPFactory() );
  RFact->SetFactory("P", Pinj);
#endif

  // define nullspace factory AFTER tentative PFactory (that generates the nullspace for the coarser levels)
  // use same nullspace factory for all multigrid levels
  // therefor we have to create one instance of NullspaceFactory and use it
  // for all FactoryManager objects (note: here, we have one FactoryManager object per level)
  Teuchos::RCP<NullspaceFactory> nspFact = Teuchos::rcp(new NullspaceFactory("Nullspace"));
  nspFact->SetFactory("Nullspace", PtentFact);

  // RAP factory with inter-level transfer of segregation block information (map extractor)
  Teuchos::RCP<RAPFactory> AcFact = Teuchos::rcp( new RAPFactory() );
  AcFact->SetFactory("P",PFact);
  AcFact->SetFactory("R",RFact);
  AcFact->SetRepairZeroDiagonal(true); // repair zero diagonal entries in Ac, that are resulting from Ptent with nullspacedim > ndofspernode

  // write out aggregates
  Teuchos::RCP<MueLu::AggregationExportFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps> > aggExpFact = Teuchos::rcp(new MueLu::AggregationExportFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps>());
  aggExpFact->SetParameter("Output filename",Teuchos::ParameterEntry(std::string("aggs_level%LEVELID_proc%PROCID.out")));
  aggExpFact->SetFactory("Aggregates",UCAggFact);
  aggExpFact->SetFactory("DofsPerNode",dropFact);
  AcFact->AddTransferFactory(aggExpFact);

  // transfer maps to coarser grids
  if(xSingleNodeAggMap != Teuchos::null) {
    Teuchos::RCP<MapTransferFactory> cmTransFact = Teuchos::rcp(new MapTransferFactory("SingleNodeAggDofMap", MueLu::NoFactory::getRCP()));
    cmTransFact->SetFactory("P", PtentFact);
    AcFact->AddTransferFactory(cmTransFact);
  }
  if(xSlaveDofMap != Teuchos::null) { // needed for ContactAFilterFactory, IterationAFactory
    Teuchos::RCP<MapTransferFactory> cmTransFact = Teuchos::rcp(new MapTransferFactory("SlaveDofMap", MueLu::NoFactory::getRCP()));
    cmTransFact->SetFactory("P", PtentFact);
    AcFact->AddTransferFactory(cmTransFact);
  }
  if(xMasterDofMap != Teuchos::null) { // needed for ContactAFilterFactory
    Teuchos::RCP<MapTransferFactory> cmTransFact = Teuchos::rcp(new MapTransferFactory("MasterDofMap", MueLu::NoFactory::getRCP()));
    cmTransFact->SetFactory("P", PtentFact);
    AcFact->AddTransferFactory(cmTransFact);
  }
  if(xNearZeroDiagMap != Teuchos::null) {
    Teuchos::RCP<MapTransferFactory> cmTransFact = Teuchos::rcp(new MapTransferFactory("NearZeroDiagMap", MueLu::NoFactory::getRCP()));
    cmTransFact->SetFactory("P", PtentFact);
    AcFact->AddTransferFactory(cmTransFact);
  }
  ///////////////////////////////////////////////////////////////////////
  // setup coarse level smoothers/solvers
  ///////////////////////////////////////////////////////////////////////

  // coarse level smoother/solver
  Teuchos::RCP<SmootherFactory> coarsestSmooFact;
  coarsestSmooFact = GetContactCoarsestSolverFactory(params, Teuchos::null ); // use full matrix A on coarsest level (direct solver)

  ///////////////////////////////////////////////////////////////////////
  // prepare factory managers
  ///////////////////////////////////////////////////////////////////////


  bool bIsLastLevel = false;
  std::vector<Teuchos::RCP<FactoryManager> > vecManager(maxLevels);
  for(int i=0; i < maxLevels; i++) {
    Teuchos::ParameterList pp(params);
    vecManager[i] = Teuchos::rcp(new FactoryManager());

    // fine/intermedium level smoother
    Teuchos::RCP<SmootherFactory> SmooFactFine = Teuchos::null;
    if(xSingleNodeAggMap != Teuchos::null)
      SmooFactFine = GetContactSmootherFactory(pp, i, singleNodeAFact); // use filtered matrix on fine and intermedium levels
    else
      SmooFactFine = GetContactSmootherFactory(pp, i, Teuchos::null); // use filtered matrix on fine and intermedium levels

    if(SmooFactFine != Teuchos::null)
      vecManager[i]->SetFactory("Smoother" ,  SmooFactFine);    // Hierarchy.Setup uses TOPSmootherFactory, that only needs "Smoother"
    vecManager[i]->SetFactory("CoarseSolver", coarsestSmooFact);
    vecManager[i]->SetFactory("Aggregates", UCAggFact);
    vecManager[i]->SetFactory("Graph", dropFact);
    vecManager[i]->SetFactory("DofsPerNode", dropFact);
    vecManager[i]->SetFactory("A", AcFact);       // same RAP factory for all levels
    vecManager[i]->SetFactory("P", PFact);        // same prolongator and restrictor factories for all levels
    vecManager[i]->SetFactory("Ptent", PtentFact);// same prolongator and restrictor factories for all levels
    vecManager[i]->SetFactory("R", RFact);        // same prolongator and restrictor factories for all levels
    vecManager[i]->SetFactory("Nullspace", nspFact); // use same nullspace factory throughout all multigrid levels
  }

  // use new Hierarchy::Setup routine
  if(maxLevels == 1) {
    bIsLastLevel = hierarchy->Setup(0, Teuchos::null, vecManager[0].ptr(), Teuchos::null); // 1 level "multigrid" method
  }
  else
  {
    bIsLastLevel = hierarchy->Setup(0, Teuchos::null, vecManager[0].ptr(), vecManager[1].ptr()); // first (finest) level
    for(int i=1; i < maxLevels-1; i++) { // intermedium levels
      if(bIsLastLevel == true) break;
      bIsLastLevel = hierarchy->Setup(i, vecManager[i-1].ptr(), vecManager[i].ptr(), vecManager[i+1].ptr());
    }
    if(bIsLastLevel == false) { // coarsest level
        bIsLastLevel = hierarchy->Setup(maxLevels-1, vecManager[maxLevels-2].ptr(), vecManager[maxLevels-1].ptr(), Teuchos::null);
     }
  }

  return hierarchy;
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
Teuchos::RCP<MueLu::SmootherFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps> > LINALG::SOLVER::MueLuContactPreconditioner2::GetContactSmootherFactory(const Teuchos::ParameterList & paramList, int level, const Teuchos::RCP<FactoryBase> & AFact) {

  char levelchar[11];
  sprintf(levelchar,"(level %d)",level);
  std::string levelstr(levelchar);

  if(paramList.isSublist("smoother: list " + levelstr)==false)
    return Teuchos::null;
  TEUCHOS_TEST_FOR_EXCEPTION(paramList.isSublist("smoother: list " + levelstr)==false, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: no ML smoother parameter list for level. error.");

  std::string type = paramList.sublist("smoother: list " + levelstr).get<std::string>("smoother: type");
  TEUCHOS_TEST_FOR_EXCEPTION(type.empty(), MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: no ML smoother type for level. error.");

  const Teuchos::ParameterList smolevelsublist = paramList.sublist("smoother: list " + levelstr);

  Teuchos::RCP<SmootherPrototype> smooProto;
  std::string ifpackType;
  Teuchos::ParameterList ifpackList;
  Teuchos::RCP<SmootherFactory> SmooFact;

  if(type == "Jacobi") {
    if(smolevelsublist.isParameter("smoother: sweeps"))
      ifpackList.set<int>("relaxation: sweeps", smolevelsublist.get<int>("smoother: sweeps"));
    if(smolevelsublist.get<double>("smoother: damping factor"))
      ifpackList.set("relaxation: damping factor", smolevelsublist.get<double>("smoother: damping factor"));
    ifpackType = "RELAXATION";
    ifpackList.set("relaxation: type", "Jacobi");
    smooProto = Teuchos::rcp( new MueLu::MyTrilinosSmoother<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap", MueLu::NoFactory::getRCP(), ifpackType, ifpackList, 0, AFact) );
  } else if(type == "Gauss-Seidel") {
    if(smolevelsublist.isParameter("smoother: sweeps"))
      ifpackList.set<int>("relaxation: sweeps", smolevelsublist.get<int>("smoother: sweeps"));
    if(smolevelsublist.get<double>("smoother: damping factor"))
      ifpackList.set("relaxation: damping factor", smolevelsublist.get<double>("smoother: damping factor"));
    ifpackType = "RELAXATION";
    ifpackList.set("relaxation: type", "Gauss-Seidel");
    smooProto = Teuchos::rcp( new MueLu::MyTrilinosSmoother<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap", MueLu::NoFactory::getRCP(), ifpackType, ifpackList, 0, AFact) );
  } else if (type == "symmetric Gauss-Seidel") {
    if(smolevelsublist.isParameter("smoother: sweeps"))
      ifpackList.set<int>("relaxation: sweeps", smolevelsublist.get<int>("smoother: sweeps"));
    if(smolevelsublist.get<double>("smoother: damping factor"))
      ifpackList.set("relaxation: damping factor", smolevelsublist.get<double>("smoother: damping factor"));
    ifpackType = "RELAXATION";
    ifpackList.set("relaxation: type", "Symmetric Gauss-Seidel");
    smooProto = Teuchos::rcp( new MueLu::MyTrilinosSmoother<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap", MueLu::NoFactory::getRCP(), ifpackType, ifpackList, 0, AFact) );
    //std::cout << "built symm GS: " << smooProto << std::endl;
  } else if (type == "Chebyshev") {
    ifpackType = "CHEBYSHEV";
    if(smolevelsublist.isParameter("smoother: sweeps"))
      ifpackList.set("chebyshev: degree", smolevelsublist.get<int>("smoother: sweeps"));
    smooProto = Teuchos::rcp( new MueLu::MyTrilinosSmoother<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap", MueLu::NoFactory::getRCP(), ifpackType, ifpackList, 0, AFact) );
    // TODO what about the other parameters
  } else if(type == "IFPACK") {
#ifdef HAVE_MUELU_IFPACK
    // TODO change to TrilinosSmoother as soon as Ifpack2 supports all preconditioners from Ifpack
    ifpackType = paramList.sublist("smoother: list " + levelstr).get<std::string>("smoother: ifpack type");
    if(ifpackType == "ILU") {
      ifpackList.set<int>("fact: level-of-fill", (int)smolevelsublist.get<double>("smoother: ifpack level-of-fill"));
      ifpackList.set("partitioner: overlap", smolevelsublist.get<int>("smoother: ifpack overlap"));
      int overlap = smolevelsublist.get<int>("smoother: ifpack overlap");
      //smooProto = MueLu::GetIfpackSmoother<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps>(ifpackType, ifpackList,smolevelsublist.get<int>("smoother: ifpack overlap"),AFact);
      smooProto = Teuchos::rcp( new MueLu::MyTrilinosSmoother<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap", MueLu::NoFactory::getRCP(), ifpackType, ifpackList, overlap, AFact) );
    }
    else
      TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: unknown ML smoother type " + type + " (IFPACK) not supported by MueLu. Only ILU is supported.");
#else // HAVE_MUELU_IFPACK
    TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: MueLu compiled without Ifpack support");
#endif // HAVE_MUELU_IFPACK
  } else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: unknown ML smoother type " + type + " not supported by MueLu.");
  }

  // create smoother factory
  SmooFact = Teuchos::rcp( new SmootherFactory(smooProto) );

  // check if pre- and postsmoothing is set
  std::string preorpost = "both";
  if(smolevelsublist.isParameter("smoother: pre or post")) preorpost = smolevelsublist.get<std::string>("smoother: pre or post");

  if (preorpost == "pre") {
    SmooFact->SetSmootherPrototypes(smooProto, Teuchos::null);
  } else if(preorpost == "post") {
    SmooFact->SetSmootherPrototypes(Teuchos::null, smooProto);
  }

  return SmooFact;
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
Teuchos::RCP<MueLu::SmootherFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps> > LINALG::SOLVER::MueLuContactPreconditioner2::GetContactCoarsestSolverFactory(const Teuchos::ParameterList & paramList, const Teuchos::RCP<FactoryBase> & AFact) {

  std::string type = ""; // use default defined by AmesosSmoother or Amesos2Smoother

  if(paramList.isParameter("coarse: type")) type = paramList.get<std::string>("coarse: type");

  Teuchos::RCP<SmootherPrototype> smooProto;
  std::string ifpackType;
  Teuchos::ParameterList ifpackList;
  Teuchos::RCP<SmootherFactory> SmooFact;

  if(type == "Jacobi") {
    if(paramList.isParameter("coarse: sweeps"))
      ifpackList.set<int>("relaxation: sweeps", paramList.get<int>("coarse: sweeps"));
    else ifpackList.set<int>("relaxation: sweeps", 1);
    if(paramList.isParameter("coarse: damping factor"))
      ifpackList.set("relaxation: damping factor", paramList.get<double>("coarse: damping factor"));
    else ifpackList.set("relaxation: damping factor", 1.0);
    ifpackType = "RELAXATION";
    ifpackList.set("relaxation: type", "Jacobi");
    smooProto = rcp( new TrilinosSmoother(ifpackType, ifpackList, 0, AFact) );
  } else if(type == "Gauss-Seidel") {
    if(paramList.isParameter("coarse: sweeps"))
      ifpackList.set<int>("relaxation: sweeps", paramList.get<int>("coarse: sweeps"));
    else ifpackList.set<int>("relaxation: sweeps", 1);
    if(paramList.isParameter("coarse: damping factor"))
      ifpackList.set("relaxation: damping factor", paramList.get<double>("coarse: damping factor"));
    else ifpackList.set("relaxation: damping factor", 1.0);
    ifpackType = "RELAXATION";
    ifpackList.set("relaxation: type", "Gauss-Seidel");
    smooProto = rcp( new TrilinosSmoother(ifpackType, ifpackList, 0, AFact) );
  } else if (type == "symmetric Gauss-Seidel") {
    if(paramList.isParameter("coarse: sweeps"))
      ifpackList.set<int>("relaxation: sweeps", paramList.get<int>("coarse: sweeps"));
    else ifpackList.set<int>("relaxation: sweeps", 1);
    if(paramList.isParameter("coarse: damping factor"))
      ifpackList.set("relaxation: damping factor", paramList.get<double>("coarse: damping factor"));
    else ifpackList.set("relaxation: damping factor", 1.0);
    ifpackType = "RELAXATION";
    ifpackList.set("relaxation: type", "Symmetric Gauss-Seidel");
    smooProto = rcp( new TrilinosSmoother(ifpackType, ifpackList, 0, AFact) );
  } else if (type == "Chebyshev") {
    ifpackType = "CHEBYSHEV";
    if(paramList.isParameter("coarse: sweeps"))
      ifpackList.set("chebyshev: degree", paramList.get<int>("coarse: sweeps"));
    if(paramList.isParameter("coarse: Chebyshev alpha"))
      ifpackList.set("chebyshev: alpha", paramList.get<double>("coarse: Chebyshev alpha"));
    smooProto = rcp( new TrilinosSmoother(ifpackType, ifpackList, 0, AFact) );
  } else if(type == "IFPACK") {
#ifdef HAVE_MUELU_IFPACK
    // TODO change to TrilinosSmoother as soon as Ifpack2 supports all preconditioners from Ifpack
   /* ifpackType = paramList.get<std::string>("coarse: ifpack type");
    if(ifpackType == "ILU") {
      ifpackList.set<int>("fact: level-of-fill", (int)paramList.get<double>("coarse: ifpack level-of-fill"));
      ifpackList.set("partitioner: overlap", paramList.get<int>("coarse: ifpack overlap"));
      smooProto = MueLu::GetIfpackSmoother<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps>(ifpackType, ifpackList, paramList.get<int>("coarse: ifpack overlap"), AFact);
    }
    else*/
    //  TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: unknown ML smoother type " + type + " (IFPACK) not supported by MueLu. Only ILU is supported.");
    ifpackType =paramList.get<std::string>("coarse: ifpack type");
    if(ifpackType == "ILU") {
      ifpackList.set<int>("fact: level-of-fill", (int)paramList.get<double>("coarse: ifpack level-of-fill"));
      ifpackList.set("partitioner: overlap", paramList.get<int>("coarse: ifpack overlap"));
      //int overlap = paramList.get<int>("coarse: ifpack overlap");
      //smooProto = Teuchos::rcp( new MueLu::PermutingSmoother<Scalar,LocalOrdinal, GlobalOrdinal, Node, LocalMatOps>("SingleNodeAggDofMap", MueLu::NoFactory::getRCP(), ifpackType, ifpackList, overlap, AFact) );
      smooProto = MueLu::GetIfpackSmoother<Scalar,LocalOrdinal,GlobalOrdinal,Node,LocalMatOps>(ifpackType, ifpackList, paramList.get<int>("coarse: ifpack overlap"), AFact);
    }
    else
      TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: unknown ML smoother type " + type + " (IFPACK) not supported by MueLu. Only ILU is supported.");

#else // HAVE_MUELU_IFPACK
    TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: MueLu compiled without Ifpack support");
#endif // HAVE_MUELU_IFPACK
  } else if(type == "Amesos-Superlu") {
    smooProto = Teuchos::rcp( new DirectSolver("Superlu",Teuchos::ParameterList()) );
    smooProto->SetFactory("A", AFact);
  } else if(type == "Amesos-Superludist") {
    smooProto = Teuchos::rcp( new DirectSolver("Superludist",Teuchos::ParameterList()) );
    smooProto->SetFactory("A", AFact);
  } else if(type == "Amesos-KLU") {
    smooProto = Teuchos::rcp( new DirectSolver("Klu",Teuchos::ParameterList()) );
    smooProto->SetFactory("A", AFact);
  } else if(type == "Amesos-UMFPACK") {
    smooProto = Teuchos::rcp( new DirectSolver("Umfpack",Teuchos::ParameterList()) );
    smooProto->SetFactory("A", AFact);
  } else if(type == "") {
    smooProto = Teuchos::rcp( new DirectSolver("",Teuchos::ParameterList()) );
    smooProto->SetFactory("A", AFact);
  } else {
    TEUCHOS_TEST_FOR_EXCEPTION(true, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: unknown coarsest solver type. '" << type << "' not supported by MueLu.");
  }

  // create smoother factory
  TEUCHOS_TEST_FOR_EXCEPTION(smooProto == Teuchos::null, MueLu::Exceptions::RuntimeError, "MueLu::Interpreter: no smoother prototype. fatal error.");
  SmooFact = rcp( new SmootherFactory(smooProto) );

  // check if pre- and postsmoothing is set
  std::string preorpost = "both";
  if(paramList.isParameter("coarse: pre or post")) preorpost = paramList.get<std::string>("coarse: pre or post");

  if (preorpost == "pre") {
    SmooFact->SetSmootherPrototypes(smooProto, Teuchos::null);
  } else if(preorpost == "post") {
    SmooFact->SetSmootherPrototypes(Teuchos::null, smooProto);
  }

  return SmooFact;
}

#endif // #ifdef HAVE_Trilinos_Q1_2013
#endif // #ifdef HAVE_MueLu

