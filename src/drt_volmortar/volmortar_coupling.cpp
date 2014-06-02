/*!----------------------------------------------------------------------
\file volmortar_coupling.cpp

<pre>
Maintainer: Philipp Farah
            farah@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15257
</pre>

*----------------------------------------------------------------------*/

/*----------------------------------------------------------------------*
 |  headers                                                  farah 10/13|
 *----------------------------------------------------------------------*/
#include "volmortar_coupling.H"
#include "volmortar_integrator.H"
#include "volmortar_cell.H"
#include "volmortar_defines.H"
#include "volmortar_shape.H"

#include "../drt_inpar/inpar_volmortar.H"

#include "../drt_lib/drt_discret.H"
#include "../drt_lib/drt_globalproblem.H"

#include "../linalg/linalg_sparsematrix.H"
#include "../linalg/linalg_solver.H"
#include "../linalg/linalg_utils.H"

// std. interface mortar
#include "../drt_mortar/mortar_coupling3d.H"
#include "../drt_mortar/mortar_utils.H"
#include "../drt_mortar/mortar_calc_utils.H"
#include "../drt_mortar/mortar_element.H"

// thermo eles
#include "../drt_thermo/thermo_element.H"
#include "../drt_so3/so3_thermo.H"

// Cut
#include "../drt_xfem/xfem_fluidwizard.H"
#include "../drt_cut/cut_volumecell.H"
#include "../drt_cut/cut_elementhandle.H"

// search
#include "../drt_geometry/searchtree.H"
#include "../drt_geometry/searchtree_geometry_service.H"

/*----------------------------------------------------------------------*
 |  ctor (public)                                            farah 10/13|
 *----------------------------------------------------------------------*/
VOLMORTAR::VolMortarCoupl::VolMortarCoupl(int dim,
                                          Teuchos::RCP<DRT::Discretization> Adis,   // should be structure
                                          Teuchos::RCP<DRT::Discretization> Bdis) : // other field...
dim_(dim),
Adiscret_(Adis),
Bdiscret_(Bdis)
{
  //check
  if( not Adiscret_->Filled() or not Bdiscret_->Filled())
    dserror("FillComplete() has to be called on both discretizations before setup of VolMortarCoupl");
  if( (Adiscret_->NumDofSets()==1) or (Bdiscret_->NumDofSets()==1)  )
    dserror("Both discretizations need to own at least two dofsets for mortar coupling!");

  // get required parameter list
  ReadAndCheckInput();

  // init dop normals
  InitDopNormals();

  // its the same communicator for all discr.
   comm_   = Teuchos::rcp(Adis->Comm().Clone());
   myrank_ = comm_->MyPID();

  // init aux normal TODO: no fixed direction!!! ONLY FOR 2D CASE !!!
  auxn_[0]=0.0;
  auxn_[1]=0.0;
  auxn_[2]=1.0;

  // initialize the counter
  polygoncounter_= 0;
  cellcounter_   = 0;
  inteles_       = 0;
  volume_        = 0.0;

  return;
}

/*----------------------------------------------------------------------*
 |  Evaluate (public)                                        farah 10/13|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Evaluate()
{
  /***********************************************************
   * Welcome                                                 *
   ***********************************************************/
  if(myrank_==0)
  {
    std::cout << "**************************************************" << std::endl;
    std::cout << "*****     Welcome to VOLMORTAR-Coupling!     *****" << std::endl;
    std::cout << "**************************************************" << std::endl;
  }

  /***********************************************************
   * initialize global matrices                              *
   ***********************************************************/
  Initialize();

  /***********************************************************
   * Assign materials                                        *
   ***********************************************************/
  AssignMaterials();

  /***********************************************************
   * Segment-based integration                               *
   ***********************************************************/
  if(DRT::INPUT::IntegralValue<INPAR::VOLMORTAR::IntType>(Params(),"INTTYPE")
      == INPAR::VOLMORTAR::inttype_segments)
    EvaluateSegments();

  /***********************************************************
   * Element-based Integration                               *
   ***********************************************************/
  else if(DRT::INPUT::IntegralValue<INPAR::VOLMORTAR::IntType>(Params(),"INTTYPE")
      == INPAR::VOLMORTAR::inttype_elements)
    EvaluateElements();

  // no other possibility
  else
    dserror("Chosen INTTYPE not provided");

  /***********************************************************
   * complete global matrices and create projection operator *
   ***********************************************************/
  Complete();
  CreateProjectionOpterator();

  /***********************************************************
   * Check initial residuum and perform mesh init             *
   ***********************************************************/
  //CheckInitialResiduum();

  // mesh initialization procedure
  if(DRT::INPUT::IntegralValue<int>(Params(), "MESH_INIT") )
    MeshInit();

  /**************************************************
   * Bye                                            *
   **************************************************/

  if(myrank_==0)
  {
    std::cout << "**************************************************" << std::endl;
    std::cout << "*****       VOLMORTAR-Coupling Done!!!       *****" << std::endl;
    std::cout << "**************************************************" << std::endl;
  }

  //output
//  std::cout << "Polyogns/Polyhedra = " << polygoncounter_ << std::endl;
//  std::cout << "Created Cells      = " << cellcounter_    << std::endl;
//  std::cout << "Integr. Elements   = " << inteles_        << std::endl;
//  std::cout << "Integrated Volume  = " << volume_ << "\n" << std::endl;

  // reset counter
  polygoncounter_=0;
  cellcounter_   =0;
  inteles_       =0;
  volume_        =0.0;

  // coupling done
  return;
}

/*----------------------------------------------------------------------*
 |  Init normals for Dop calculation                         farah 05/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::InitDopNormals()
{
  dopnormals_.Reshape(9,3);
  dopnormals_(0,0)= 1.0; dopnormals_(0,1)= 0.0; dopnormals_(0,2)= 0.0;
  dopnormals_(1,0)= 0.0; dopnormals_(1,1)= 1.0; dopnormals_(1,2)= 0.0;
  dopnormals_(2,0)= 0.0; dopnormals_(2,1)= 0.0; dopnormals_(2,2)= 1.0;
  dopnormals_(3,0)= 1.0; dopnormals_(3,1)= 1.0; dopnormals_(3,2)= 0.0;
  dopnormals_(4,0)= 1.0; dopnormals_(4,1)= 0.0; dopnormals_(4,2)= 1.0;
  dopnormals_(5,0)= 0.0; dopnormals_(5,1)= 1.0; dopnormals_(5,2)= 1.0;
  dopnormals_(6,0)= 1.0; dopnormals_(6,1)= 0.0; dopnormals_(6,2)=-1.0;
  dopnormals_(7,0)= 1.0; dopnormals_(7,1)=-1.0; dopnormals_(7,2)= 0.0;
  dopnormals_(8,0)= 0.0; dopnormals_(8,1)= 1.0; dopnormals_(8,2)=-1.0;

  return;
}
/*----------------------------------------------------------------------*
 |  Init search tree                                         farah 05/14|
 *----------------------------------------------------------------------*/
Teuchos::RCP<GEO::SearchTree> VOLMORTAR::VolMortarCoupl::InitSearch(Teuchos::RCP<DRT::Discretization> searchdis)
{
  // init current positions
  std::map<int,LINALG::Matrix<3,1> > currentpositions;

  for (int lid = 0; lid < searchdis->NumMyColElements(); ++lid)
  {
    DRT::Element* sele = searchdis->lColElement(lid);

    // calculate slabs for every node on every element
    for (int k=0;k<sele->NumNode();k++)
    {
      DRT::Node* node= sele->Nodes()[k];
      LINALG::Matrix<3,1> currpos;

      currpos(0) = node->X()[0];
      currpos(1) = node->X()[1];
      currpos(2) = node->X()[2];

      currentpositions[node->Id()] = currpos;
    }
  }

  //init of 3D search tree
  Teuchos::RCP<GEO::SearchTree> searchTree = Teuchos::rcp(new GEO::SearchTree(8));

  // find the bounding box of the elements and initialize the search tree
  const LINALG::Matrix<3,2> rootBox = GEO::getXAABBofDis(*searchdis,currentpositions);
  searchTree->initializeTree(rootBox,*searchdis,GEO::TreeType(GEO::OCTTREE));

  return searchTree;
}


/*----------------------------------------------------------------------*
 |  Calculate Dops for background mesh                       farah 05/14|
 *----------------------------------------------------------------------*/
std::map<int,LINALG::Matrix<9,2> > VOLMORTAR::VolMortarCoupl::CalcBackgroundDops(Teuchos::RCP<DRT::Discretization> searchdis)
{
  std::map<int,LINALG::Matrix<9,2> > currentKDOPs;

  for (int lid = 0; lid < searchdis->NumMyColElements(); ++lid)
  {
    DRT::Element* sele = searchdis->lColElement(lid);

    currentKDOPs[sele->Id()] = CalcDop(*sele);
  }

  return currentKDOPs;
}

/*----------------------------------------------------------------------*
 |  Calculate Dop for one Element                            farah 05/14|
 *----------------------------------------------------------------------*/
LINALG::Matrix<9,2> VOLMORTAR::VolMortarCoupl::CalcDop(DRT::Element& ele)
{
  LINALG::Matrix<9,2>  dop;

  // calculate slabs
  for(int j=0; j<9;j++)
  {
    // initialize slabs
    dop(j,0) =  1.0e12;
    dop(j,1) = -1.0e12;
  }

  // calculate slabs for every node on every element
  for (int k=0;k<ele.NumNode();k++)
  {
    DRT::Node* node= ele.Nodes()[k];

    // get current node position
    double pos[3] = {0.0, 0.0, 0.0};
    for (int j=0;j<dim_;++j)
      pos[j] = node->X()[j];

    // calculate slabs
    for(int j=0; j<9;j++)
    {
      //= ax+by+cz=d/sqrt(aa+bb+cc)
      double num = dopnormals_(j,0)*pos[0]
                 + dopnormals_(j,1)*pos[1]
                 + dopnormals_(j,2)*pos[2];
      double denom = sqrt((dopnormals_(j,0)*dopnormals_(j,0))
                         +(dopnormals_(j,1)*dopnormals_(j,1))
                         +(dopnormals_(j,2)*dopnormals_(j,2)));
      double dcurrent = num/denom;

      if (dcurrent > dop(j,1)) dop(j,1) = dcurrent;
      if (dcurrent < dop(j,0)) dop(j,0) = dcurrent;
    }
  }

  return dop;
}

/*----------------------------------------------------------------------*
 |  Perform searching procedure                              farah 05/14|
 *----------------------------------------------------------------------*/
std::vector<int> VOLMORTAR::VolMortarCoupl::Search(DRT::Element& ele,
                                                   Teuchos::RCP<GEO::SearchTree> SearchTree,
                                                   std::map<int,LINALG::Matrix<9,2> >& currentKDOPs)
{
  // vector of global ids of found elements
  std::vector<int> gids;
  gids.clear();
  std::set<int> gid;
  gid.clear();

  LINALG::Matrix<9,2> queryKDOP;

  // calc dop for considered element
  queryKDOP = CalcDop(ele);

  //**********************************************************
  //search for near elements to the background node's coord
  SearchTree->searchMultibodyContactElements(currentKDOPs,queryKDOP,0,gid);

  for(std::set<int>::iterator iter=gid.begin();iter!=gid.end();++iter)
    gids.push_back(*iter);

  return gids;
}

/*----------------------------------------------------------------------*
 |  Assign materials for both fields                         farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::AssignMaterials()
{
  //loop over all adis elements
  for (int i=0;i<Adiscret_->NumMyRowElements();++i)
  {
    //get slave element
    DRT::Element* Aele = Adiscret_->lRowElement(i);

    //loop over all bdis elements
    for (int j=0;j<Bdiscret_->NumMyColElements();++j)
    {
      //get master element
      DRT::Element* Bele = Bdiscret_->lColElement(j);

      // exchange material pointers
      //TODO: make this more general
      Bele->AddMaterial(Aele->Material());
      Aele->AddMaterial(Bele->Material());

      // initialise kinematic type to geo_linear.
      // kintype is passed to the cloned thermo element
      GenKinematicType kintype = geo_linear;

      // if oldele is a so3_base element
      DRT::ELEMENTS::So3_Base* so3_base = dynamic_cast<DRT::ELEMENTS::So3_Base*>(Aele);
      if (so3_base != NULL)
        kintype = so3_base->GetKinematicType();
      else
        dserror("oldele is not a so3_thermo element!");

      // note: SetMaterial() was reimplemented by the thermo element!
      DRT::ELEMENTS::Thermo* therm =dynamic_cast<DRT::ELEMENTS::Thermo*>(Bele);
      if (therm != NULL)
      {
        therm->SetKinematicType(kintype);  // set kintype in cloned thermal element
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Calculate trafo matrix for quadr. elements               farah 05/14|
 *----------------------------------------------------------------------*/
std::vector<int> VOLMORTAR::VolMortarCoupl::GetAdjacentNodes(DRT::Element::DiscretizationType shape,
                                                             int& lid)
{
  // vector of adjacent node ids
  std::vector<int> ids;

  switch (shape)
  {
  case DRT::Element::hex20:
  {
    if     (lid==8)  {ids.push_back(0); ids.push_back(1);}
    else if(lid==9)  {ids.push_back(1); ids.push_back(2);}
    else if(lid==10) {ids.push_back(2); ids.push_back(3);}
    else if(lid==11) {ids.push_back(3); ids.push_back(0);}
    else if(lid==12) {ids.push_back(0); ids.push_back(4);}
    else if(lid==13) {ids.push_back(1); ids.push_back(5);}
    else if(lid==14) {ids.push_back(2); ids.push_back(6);}
    else if(lid==15) {ids.push_back(3); ids.push_back(7);}
    else if(lid==16) {ids.push_back(4); ids.push_back(5);}
    else if(lid==17) {ids.push_back(5); ids.push_back(6);}
    else if(lid==18) {ids.push_back(6); ids.push_back(7);}
    else if(lid==19) {ids.push_back(4); ids.push_back(7);}
    else dserror("Given Id is wrong!!!");

    break;
  }
  case DRT::Element::tet10:
  {
    if     (lid==4) {ids.push_back(0); ids.push_back(1);}
    else if(lid==5) {ids.push_back(1); ids.push_back(2);}
    else if(lid==6) {ids.push_back(0); ids.push_back(2);}
    else if(lid==7) {ids.push_back(0); ids.push_back(3);}
    else if(lid==8) {ids.push_back(1); ids.push_back(3);}
    else if(lid==9) {ids.push_back(2); ids.push_back(3);}
    else dserror("Given Id is wrong!!!");

    break;
  }
  default:
      dserror("shape unknown\n");
  }

  return ids;
}
/*----------------------------------------------------------------------*
 |  Calculate trafo matrix for quadr. elements               farah 05/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::CreateTrafoOperator(DRT::Element& ele,
                                                    Teuchos::RCP<DRT::Discretization> searchdis,
                                                    bool dis,
                                                    std::set<int>& donebefore)
{
  // trafo parameter
  double alpha = 0.3;

  // ids for corner and edge nodes
  int corner_min = 0;
  int corner_max = 0;
  int edge_min = 0;
  int edge_max = 0;

  if(ele.Shape() == DRT::Element::hex20)
  {
    corner_min = 0;
    corner_max = 7;
    edge_min = 8;
    edge_max = 19;
  }
  else if (ele.Shape() == DRT::Element::tet10)
  {
    corner_min = 0;
    corner_max = 3;
    edge_min = 4;
    edge_max = 9;
  }
  else
    dserror("Unknown shape for trafo matrix");

  // loop over element nodes
  for(int i=0;i<ele.NumNode();++i)
  {
    DRT::Node* cnode = ele.Nodes()[i];
    if(cnode->Owner() != myrank_)
      continue;

    std::set<int>::iterator iter = donebefore.find(cnode->Id());
    if(iter != donebefore.end())
      continue;

    donebefore.insert(cnode->Id());

    //************************************************
    // corner nodes
    if(i>=corner_min and i<=corner_max)
    {
      int nsdof=searchdis->NumDof(1,cnode);

      //loop over slave dofs
      for (int jdof=0;jdof<nsdof;++jdof)
      {
        int row = searchdis->Dof(1,cnode,jdof);

        if (dis)
          TA_->Assemble(1.0,row,row);
        else
          TB_->Assemble(1.0,row,row);
      }
    }
    //************************************************
    // edge nodes
    else if (i>=edge_min and i<=edge_max)
    {
      // get ids
      std::vector<int> ids = GetAdjacentNodes(ele.Shape(),i);

      int nsdof=searchdis->NumDof(1,cnode);

      //loop over dofs
      for (int jdof=0;jdof<nsdof;++jdof)
      {
        int row = searchdis->Dof(1,cnode,jdof);

        // assemble diagonal entries
        if (dis)
          TA_->Assemble(1.0-3.0*alpha,row,row);
        else
          TB_->Assemble(1.0-3.0*alpha,row,row);

        // found ids
        for(int id=0;id<(int)ids.size();++id)
        {
          DRT::Node* fnode = ele.Nodes()[ids[id]];
          int nfdof=searchdis->NumDof(1,fnode);

          for (int fdof=0;fdof<nfdof;++fdof)
          {
            int col = searchdis->Dof(1,fnode,fdof);

            if(jdof==fdof)
            {
              // assemble off-diagonal entries
              if (dis)
                TA_->Assemble(alpha,row,col);
              else
                TB_->Assemble(alpha,row,col);
            }
          }
        }
      }
    }
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Element-based routine                                    farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::EvaluateElements()
{
  // check dimension
  if(dim_==2)
    dserror("Element-based integration only for 3D coupling!");

  // output
  if(myrank_== 0)
  {
    std::cout << "*****       Element-based Integration        *****" << std::endl;
    std::cout << "*****       Calc First Projector:            *****" << std::endl;
  }


  // init searchtrees
  Teuchos::RCP<GEO::SearchTree> SearchTreeA = InitSearch(Adiscret_);
  Teuchos::RCP<GEO::SearchTree> SearchTreeB = InitSearch(Bdiscret_);

  // calculate DOPs for search algorithm
  std::map<int,LINALG::Matrix<9,2> > CurrentDOPsA = CalcBackgroundDops(Adiscret_);
  std::map<int,LINALG::Matrix<9,2> > CurrentDOPsB = CalcBackgroundDops(Bdiscret_);

  /**************************************************
   * loop over all Adis elements                    *
   **************************************************/
  std::set<int> donebeforea;
  for (int j=0;j<Adiscret_->NumMyColElements();++j)
  {
    //PrintStatus(j,false);

    //get master element
    DRT::Element* Aele = Adiscret_->lColElement(j);

    std::vector<int> found = Search(*Aele, SearchTreeB,CurrentDOPsB);
    Integrate3DEleBased_ADis(*Aele,found);

    // create trafo operator for quadr. modification
    if(dualquad_ != INPAR::VOLMORTAR::dualquad_no_mod)
      CreateTrafoOperator(*Aele,Adiscret_,true,donebeforea);
  }

  // half-time output
  if(myrank_== 0)
  {
    std::cout << "**************************************************" << std::endl;
    std::cout << "*****       Calc Second Projector:           *****" << std::endl;
  }

  /**************************************************
   * loop over all Bdis elements                    *
   **************************************************/
  std::set<int> donebeforeb;
  for (int j=0;j<Bdiscret_->NumMyColElements();++j)
  {
    //PrintStatus(j,true);

    //get master element
    DRT::Element* Bele = Bdiscret_->lColElement(j);

    std::vector<int> found = Search(*Bele,SearchTreeA,CurrentDOPsA);
    Integrate3DEleBased_BDis(*Bele,found);

    // create trafo operator for quadr. modification
    if(dualquad_ != INPAR::VOLMORTAR::dualquad_no_mod)
      CreateTrafoOperator(*Bele,Bdiscret_,false,donebeforeb);
  }

  // stats
  inteles_ += Adiscret_->NumGlobalElements();
  inteles_ += Bdiscret_->NumGlobalElements();
}

/*----------------------------------------------------------------------*
 |  Segment-based routine                                    farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::EvaluateSegments()
{
  // create search tree and current dops
  Teuchos::RCP<GEO::SearchTree> SearchTreeB       = InitSearch(Bdiscret_);
  std::map<int,LINALG::Matrix<9,2> > CurrentDOPsB = CalcBackgroundDops(Bdiscret_);

  /**************************************************
   * loop over all slave elements                   *
   **************************************************/
  for (int i=0;i<Adiscret_->NumMyRowElements();++i)
  {
    //output of coupling status
    //PrintStatus(i);

    //get slave element
    DRT::Element* Aele = Adiscret_->lRowElement(i);

    // get found elements from other discr.
    std::vector<int> found = Search(*Aele, SearchTreeB,CurrentDOPsB);

    /**************************************************
     * loop over all master elements                  *
     **************************************************/
    for(int foundeles=0;foundeles<(int)found.size();++foundeles)
    {
       //get b element
       DRT::Element*Bele = Bdiscret_->gElement(found[foundeles]);

      /**************************************************
       *                    2D                          *
       **************************************************/
      if (dim_==2)
      {
        EvaluateSegments2D(*Aele,*Bele);
      }

      /**************************************************
       *                    3D                          *
       **************************************************/
      else if (dim_==3)
      {
        EvaluateSegments3D(Aele,Bele);
      }

      /**************************************************
       * Wrong Dimension !!!                            *
       **************************************************/
      else
        dserror("Problem dimension is not correct!");

    } // end master element loop
  } // end slave element loop
}

/*----------------------------------------------------------------------*
 |  Segment-based routine 2D                                 farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::EvaluateSegments2D(DRT::Element& Aele,
                                                   DRT::Element& Bele)
{
  // define polygon vertices
  static std::vector<MORTAR::Vertex>                  SlaveVertices;
  static std::vector<MORTAR::Vertex>                  MasterVertices;
  static std::vector<MORTAR::Vertex>                  ClippedPolygon;
  static std::vector<Teuchos::RCP<MORTAR::IntCell> >  cells;

  // clear old polygons
  SlaveVertices.clear();
  MasterVertices.clear();
  ClippedPolygon.clear();
  cells.clear();
  cells.resize(0);

  // build new polygons
  DefineVerticesMaster(Bele,MasterVertices);
  DefineVerticesSlave(Aele,SlaveVertices);

  double tol = 1e-12;
  PolygonClippingConvexHull(SlaveVertices,MasterVertices,ClippedPolygon,Aele,Bele,tol);
  int clipsize = (int)(ClippedPolygon.size());

  // proceed only if clipping polygon is at least a triangle
  if (clipsize<3)
    return;
  else
    polygoncounter_+=1;

  // triangulation
  DelaunayTriangulation(cells, ClippedPolygon,tol);
  cellcounter_+=(int)cells.size();

  //integrate cells
  Integrate2D(Aele,Bele,cells);
}

/*----------------------------------------------------------------------*
 |  Segment-based routine 3D                                 farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::EvaluateSegments3D(DRT::Element* Aele,
                                                   DRT::Element* Bele)
{
  //check need element-based integration over sele:
  bool integrateA = CheckEleIntegration(*Aele,*Bele);

  //check need element-based integration over mele:
  bool integrateB = CheckEleIntegration(*Bele,*Aele);

  //check need for cut:
  bool performcut = CheckCut(*Aele,*Bele);

  /**************************************************
   * Integrate element-based Sele                   *
   **************************************************/
  if(integrateA)
  {
    //integrate over Aele
    Integrate3D(*Aele, *Bele, 0);
  }
  /**************************************************
   * Integrate element-based Mele                   *
   **************************************************/
  else if (integrateB)
  {
    //integrate over Bele
    Integrate3D(*Aele, *Bele,1);
  }
  /**************************************************
   * Start Cut                                      *
   **************************************************/
  else if(performcut)
  {
    // call cut and create integration cells
    try
    {
      bool switched_conf = false;
      PerformCut(Aele, Bele,switched_conf);
    }
    catch ( std::runtime_error & err1 )
    {
      try
      {
        bool switched_conf = true;
        PerformCut(Bele, Aele, switched_conf);
      }
      catch ( std::runtime_error & err2 )
      {
        std::cout << "runtime error 2 = " << err2.what() << std::endl;
      }
    }
  }// end performcut
  else
  {
    // do nothing...
  }

  return;
}

/*----------------------------------------------------------------------*
 |  get parameters and check for validity                    farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::ReadAndCheckInput()
{
  // read input parameters
  const Teuchos::ParameterList& volmortar   = DRT::Problem::Instance()->VolmortarParams();

  // check validity
  if (DRT::INPUT::IntegralValue<INPAR::VOLMORTAR::IntType>(volmortar,"INTTYPE") == INPAR::VOLMORTAR::inttype_segments)
  {
    std::cout<<"WARNING: The chosen integration type for volmortar coupling requires cut procedure !"<< std::endl;
    std::cout<<"WARNING: The cut is up to now not able to exactly calculate the required segments!"<< std::endl;
  }

  if(DRT::INPUT::IntegralValue<int>(volmortar, "MESH_INIT")  and
     comm_->NumProc()!=1)
  {
    dserror("ERROR: MeshInit only for serial calculations!!!");
  }

  // merge to global parameter list
  params_.setParameters(volmortar);

  // get specific and frequently reused parameters
  dualquad_ = DRT::INPUT::IntegralValue<INPAR::VOLMORTAR::DualQuad>(params_,"DUALQUAD");

  return;
}

/*----------------------------------------------------------------------*
 |  check initial residuum                                   farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::CheckInitialResiduum()
{
  // create vectors of initial primary variables
  Teuchos::RCP<Epetra_Vector> var_A = LINALG::CreateVector(*ADiscret()->DofRowMap(0),true);
  Teuchos::RCP<Epetra_Vector> var_B = LINALG::CreateVector(*BDiscret()->DofRowMap(1),true);

  // solution
  Teuchos::RCP<Epetra_Vector> result_A = LINALG::CreateVector(*BDiscret()->DofRowMap(1),true);
  Teuchos::RCP<Epetra_Vector> result_B = LINALG::CreateVector(*BDiscret()->DofRowMap(1),true);

  // node positions for Discr A
  for (int i=0;i<ADiscret()->NumMyRowElements();++i)
  {
    DRT::Element* Aele = ADiscret()->lRowElement(i);
    for(int j=0;j<Aele->NumNode();++j)
    {
      DRT::Node* cnode = Aele->Nodes()[j];
      int nsdof=ADiscret()->NumDof(0,cnode);

      //loop over slave dofs
      for (int jdof=0;jdof<nsdof;++jdof)
      {
        int id  = ADiscret()->Dof(0,cnode,jdof);
        double val = cnode->X()[jdof];

        var_A->ReplaceGlobalValue(id,0,val);
      }
    }
  }

  // node positions for Discr B
  for (int i=0;i<BDiscret()->NumMyRowElements();++i)
  {
    DRT::Element* Bele = BDiscret()->lRowElement(i);
    for(int j=0;j<Bele->NumNode();++j)
    {
      DRT::Node* cnode = Bele->Nodes()[j];
      int nsdof=BDiscret()->NumDof(1,cnode);

      //loop over slave dofs
      for (int jdof=0;jdof<nsdof;++jdof)
      {
        int id  = BDiscret()->Dof(1,cnode,jdof);
        double val = cnode->X()[jdof];

        var_B->ReplaceGlobalValue(id,0,val);
      }
    }
  }

  //std::cout << "vector of slave positions " << *var_B << std::endl;

  // do: D*db - M*da
  //int err1 = dmatrixB_->Multiply(false, *var_B, *result_B);
  //int err2 = mmatrixB_->Multiply(false, *var_A, *result_A);

  //if(err1!=0 or err2!=0)
  //  dserror("error");

  int err = pmatrixB_->Multiply(false,*var_A,*result_A);
  if(err!=0)
    dserror("error");


  // substract both results
  result_A->Update(-1.0,*var_B,1.0);

  std::cout << "Result of init check= " << *result_A << std::endl;

  return;
}

/*----------------------------------------------------------------------*
 |  check initial residuum                                   farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::MeshInit()
{
  // create vectors of initial primary variables
  Teuchos::RCP<Epetra_Vector> var_A = LINALG::CreateVector(*ADiscret()->DofRowMap(0),true);

  // solution
  Teuchos::RCP<Epetra_Vector> result_A = LINALG::CreateVector(*BDiscret()->DofRowMap(1),true);

  // node positions for Discr A
  for (int i=0;i<ADiscret()->NumMyRowElements();++i)
  {
    DRT::Element* Aele = ADiscret()->lRowElement(i);
    for(int j=0;j<Aele->NumNode();++j)
    {
      DRT::Node* cnode = Aele->Nodes()[j];
      int nsdof=ADiscret()->NumDof(0,cnode);

      //loop over slave dofs
      for (int jdof=0;jdof<nsdof;++jdof)
      {
        int id  = ADiscret()->Dof(0,cnode,jdof);
        double val = cnode->X()[jdof];

        var_A->ReplaceGlobalValue(id,0,val);
      }
    }
  }

  //-----------------------------------------------------
  // mesh init
  int err = pmatrixB_->Multiply(false,*var_A,*result_A);
  if(err!=0)
    dserror("error");

  //std::cout << "New slave positions " << *result_A << std::endl;

  // node positions for Discr B
  for (int i=0;i<BDiscret()->NumMyRowElements();++i)
  {
    DRT::Element* Bele = BDiscret()->lRowElement(i);
    for(int j=0;j<Bele->NumNode();++j)
    {
      DRT::Node* cnode = Bele->Nodes()[j];
      int nsdof=BDiscret()->NumDof(1,cnode);
      std::vector<double> nvector(3);

      //loop over slave dofs
      for (int jdof=0;jdof<nsdof;++jdof)
      {
        const int lid = result_A->Map().LID(BDiscret()->Dof(1,cnode,jdof));
        nvector[jdof] = (*result_A)[lid]-cnode->X()[jdof];
      }

      cnode->ChangePos(nvector);
    }
  }

  BDiscret()->FillComplete(false,true,true);

  return;
}
/*----------------------------------------------------------------------*
 |  PrintStatus (public)                                     farah 02/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::PrintStatus(int& i, bool dis_switch)
{
  static int percent_counter = 0;
  static int EleSum = 0;

  if(i==0)
  {
    if(dis_switch)
      EleSum = Bdiscret_->NumGlobalElements();
    else
      EleSum = Adiscret_->NumGlobalElements();

    percent_counter = 0;
  }

  if( (int)((i*100)/EleSum) > (int)(10*percent_counter))
  {
    std::cout << "---------------------------" << std::endl;
    std::cout << (int)((i*100)/EleSum) -1 << "% of Coupling Evaluations are done!" << std::endl;
    std::cout << "---------------------------" << std::endl;

    percent_counter++;
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Start Cut routine                                         farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::PerformCut(DRT::Element* sele,
                                           DRT::Element* mele,
                                           bool switched_conf)
{
  // create empty vector of integration cells
  std::vector<Teuchos::RCP<Cell> > IntCells;

  // the cut wizard wants discretizations to perform the cut. One is supposed to be the background discretization and the
  // other the interface discretization.
  // As we want to cut only two 3D elements, we build two auxiliary discretizations. The first holds only a copy of the master element,
  // acting as background mesh, and the other one is build of the surface elements of the slave element, being the interface
  // discretization. We use temporary copies of all elements and nodes, as we only need the geometrie to perform the cut, but 
  // want to make sure that the gids and dofs of the original elements are kept untouched.

  Teuchos::RCP<DRT::Discretization> sauxdis = Teuchos::rcp(new DRT::Discretization((std::string)"slaveauxdis",comm_));
  Teuchos::RCP<DRT::Discretization> mauxdis = Teuchos::rcp(new DRT::Discretization((std::string)"masterauxdis",comm_));

  // build surface elements for all surfaces of slave element 
  std::vector<Teuchos::RCP<DRT::Element> > sele_surfs = sele->Surfaces();
  const int numsurf = sele_surfs.size();
  for(int isurf=0; isurf<numsurf ;++isurf)
  {
    //add all surface elements to auxiliary slave discretization (no need for cloning, as surface elements are
    // rebuild every time when calling Surfaces(), anyway)
    sauxdis->AddElement(sele_surfs[isurf]);
  }

  // add clone of element to auxiliary discretization
  mauxdis->AddElement(Teuchos::rcp(mele->Clone()));

  // add clones of nodes to auxiliary discretizations
  for(int node=0;node<sele->NumNode();++node)
    sauxdis->AddNode(Teuchos::rcp(sele->Nodes()[node]->Clone()));
  for(int node=0;node<mele->NumNode();++node)
    mauxdis->AddNode(Teuchos::rcp(mele->Nodes()[node]->Clone()));

  // complete dis
  sauxdis->FillComplete(true, false, false);
  mauxdis->FillComplete(true, false, false);

  // create cut wizard
  Teuchos::RCP<XFEM::FluidWizard> wizard = Teuchos::rcp( new XFEM::FluidWizard(*mauxdis, *sauxdis ));

  //dummy displacement vector --> zero due to coupling in reference configuration
  Teuchos::RCP<Epetra_Vector> idispcol = LINALG::CreateVector(*sauxdis->DofRowMap(0),true);

  // *************************************
  // TESSELATION *************************
  if(DRT::INPUT::IntegralValue<INPAR::VOLMORTAR::CutType>(Params(),"CUTTYPE")
      == INPAR::VOLMORTAR::cuttype_tessellation)
  {
     wizard->Cut(  true,            // include_inner
                   *idispcol,       // interface displacements
                   "Tessellation",  // how to create volume cell Gauss points?
                   "Tessellation",  // how to create boundary cell Gauss points?
                   true,            // parallel cut framework
                   false,           // gmsh output for cut library
                   true,            // find point positions
                   true,            // create tet cells only
                   false,            // screen output
                   true             // cut in reference coordinates
                   );

     GEO::CUT::plain_volumecell_set mcells_out;
     GEO::CUT::plain_volumecell_set mcells_in;
     GEO::CUT::ElementHandle * em = wizard->GetElement( mele );

      //is mele in cut involved?
     if (em!=NULL)
     {
       em->CollectVolumeCells(true, mcells_in, mcells_out);

       int count = 0;

       for(int u=0;u<(int)mcells_in.size();++u)
         for(int z=0;z<(int)mcells_in[u]->IntegrationCells().size();++z)
         {
           IntCells.push_back(Teuchos::rcp(new VOLMORTAR::Cell(count,4,mcells_in[u]->IntegrationCells()[z]->Coordinates(),mcells_in[u]->IntegrationCells()[z]->Shape()) ));
           volume_ += IntCells[count]->Vol();

           count++;
         }

       //integrate found cells - tesselation
       if (!switched_conf)
         Integrate3DCell(*sele, *mele, IntCells);
       else
         Integrate3DCell(*mele, *sele, IntCells);

       // count the cells and the polygons/polyhedra
       polygoncounter_+= mcells_in.size();
       cellcounter_   += count;
    }
  }

  // *******************************************
  // DIRECT DIVERGENCE *************************
  else if (DRT::INPUT::IntegralValue<INPAR::VOLMORTAR::CutType>(Params(),"CUTTYPE")
      == INPAR::VOLMORTAR::cuttype_directdivergence)
  {

    wizard->Cut(  true,                // include_inner
                  *idispcol,           // interface displacements
                  "DirectDivergence",  // how to create volume cell Gauss points?
                  "DirectDivergence",  // how to create boundary cell Gauss points?
                  true,                // parallel cut framework
                  false,               // gmsh output for cut library
                  true,                // find point positions
                  false,                // create tet cells only
                  false,               // suppress screen output
                  true                 // cut in reference coordinates
                  );

    GEO::CUT::plain_volumecell_set mcells_out;
    GEO::CUT::plain_volumecell_set mcells_in;
    GEO::CUT::ElementHandle * em = wizard->GetElement( mele );

    // for safety
    volcell_.clear();
    if (em!=NULL)
    {
      em->CollectVolumeCells(true,volcell_,mcells_out);

      // start integration
      if(switched_conf)
        Integrate3DCell_DirectDivergence(*mele, *sele, switched_conf);
      else
        Integrate3DCell_DirectDivergence(*sele, *mele, switched_conf);

      polygoncounter_+= volcell_.size();
    }
  }

  // *******************************************
  // DEFAULT           *************************
  else
    dserror("Chosen Cuttype for volmortar not supported!");

  return;
}

/*----------------------------------------------------------------------*
 |  Check need for element-based integration                 farah 01/14|
 *----------------------------------------------------------------------*/
bool VOLMORTAR::VolMortarCoupl::CheckEleIntegration(DRT::Element& sele,
                                                    DRT::Element& mele)
{
  bool integrateele = true;
  bool converged    = false;

  double xi[3]  = {0.0 , 0.0, 0.0};
  double xgl[3] = {0.0 , 0.0, 0.0};

  //--------------------------------------------------------
  // 1. all slave nodes within with master ele ?
  for(int u=0;u<sele.NumNode();++u)
  {
    xgl[0] = sele.Nodes()[u]->X()[0];
    xgl[1] = sele.Nodes()[u]->X()[1];
    xgl[2] = sele.Nodes()[u]->X()[2];

    // global to local:
    if (mele.Shape()==DRT::Element::hex8)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::hex8>(mele,xgl,xi,converged);
    else if (mele.Shape()==DRT::Element::tet4)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::tet4>(mele,xgl,xi,converged);
    else
      dserror("Shape function not supported!");

    if(converged==true)
    {
      if(mele.Shape()==DRT::Element::hex8)
      {
        if(xi[0]>-1.0-VOLMORTARELETOL and xi[0]<1.0+VOLMORTARELETOL and
           xi[1]>-1.0-VOLMORTARELETOL and xi[1]<1.0+VOLMORTARELETOL and
           xi[2]>-1.0-VOLMORTARELETOL and xi[2]<1.0+VOLMORTARELETOL )
          integrateele = true;
        else
          return false;
      }

      if(mele.Shape()==DRT::Element::tet4)
      {
        if( xi[0]>0.0-VOLMORTARELETOL and xi[0]<1.0+VOLMORTARELETOL and
            xi[1]>0.0-VOLMORTARELETOL and xi[1]<1.0+VOLMORTARELETOL and
            xi[2]>0.0-VOLMORTARELETOL and xi[2]<1.0+VOLMORTARELETOL and
            (xi[0]+xi[1]+xi[2])<1.0+3*VOLMORTARELETOL)
          integrateele = true;
        else
          return false;
      }
    }
    else
    {
      std::cout << "!!! GLOBAL TO LOCAL NOT CONVERGED !!!" << std::endl;
      return false;
    }
  }

  return integrateele;
}

/*----------------------------------------------------------------------*
 |  Check need for cut and element-based integration         farah 01/14|
 *----------------------------------------------------------------------*/
bool VOLMORTAR::VolMortarCoupl::CheckCut(DRT::Element& sele,
                                         DRT::Element& mele)
{
  double xi[3]  = {0.0 , 0.0, 0.0};
  double xgl[3] = {0.0 , 0.0, 0.0};
  bool converged=false;

  {
  //--------------------------------------------------------
  // 1. check if all nodes are outside a parameter space surface
  // bools for tet
  bool xi0 =false;
  bool xi1 =false;
  bool xi2 =false;
  bool all =false;

  //bools for hex
  bool xi0n =false;
  bool xi1n =false;
  bool xi2n =false;

  for(int u=0;u<mele.NumNode();++u)
  {
    xgl[0] = mele.Nodes()[u]->X()[0];
    xgl[1] = mele.Nodes()[u]->X()[1];
    xgl[2] = mele.Nodes()[u]->X()[2];

    // global to local:
    if (sele.Shape()==DRT::Element::hex8)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::hex8>(sele,xgl,xi,converged);
    else if (sele.Shape()==DRT::Element::tet4)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::tet4>(sele,xgl,xi,converged);
    else
      dserror("Shape function not supported!");

    if(converged==true)
    {
      if(sele.Shape()==DRT::Element::hex8)
      {
        if(xi[0]>-1.0+VOLMORTARCUTTOL) xi0=true;
        if(xi[1]>-1.0+VOLMORTARCUTTOL) xi1=true;
        if(xi[2]>-1.0+VOLMORTARCUTTOL) xi2=true;

        if(xi[0]<1.0-VOLMORTARCUTTOL) xi0n=true;
        if(xi[1]<1.0-VOLMORTARCUTTOL) xi1n=true;
        if(xi[2]<1.0-VOLMORTARCUTTOL) xi2n=true;
      }
      else if(sele.Shape()==DRT::Element::tet4)
      {
        if(xi[0]>0.0+VOLMORTARCUTTOL) xi0=true;
        if(xi[1]>0.0+VOLMORTARCUTTOL) xi1=true;
        if(xi[2]>0.0+VOLMORTARCUTTOL) xi2=true;
        if((xi[0]+xi[1]+xi[2])<1.0-3.0*VOLMORTARCUTTOL) all=true;
      }
    }
  } //end node loop

  if(sele.Shape()==DRT::Element::tet4)
  {
    if(!xi0 or !xi1 or !xi2 or !all)
      return false;
  }
  if(sele.Shape()==DRT::Element::hex8)
  {
    if(!xi0 or !xi1 or !xi2 or !xi0n or !xi1n or !xi2n)
      return false;
  }

  }

  {
  //--------------------------------------------------------
  // 2. check if all nodes are outside a parameter space surface (changed elements)
  bool xi0 =false;
  bool xi1 =false;
  bool xi2 =false;
  bool all =false;

  //bools for hex
  bool xi0n =false;
  bool xi1n =false;
  bool xi2n =false;

  for(int u=0;u<sele.NumNode();++u)
  {
    xgl[0] = sele.Nodes()[u]->X()[0];
    xgl[1] = sele.Nodes()[u]->X()[1];
    xgl[2] = sele.Nodes()[u]->X()[2];

    // global to local:
    if (mele.Shape()==DRT::Element::hex8)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::hex8>(mele,xgl,xi,converged);
    else if (mele.Shape()==DRT::Element::tet4)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::tet4>(mele,xgl,xi,converged);
    else
      dserror("Shape function not supported!");

    if(converged==true)
    {
      if(mele.Shape()==DRT::Element::hex8)
      {
        if(xi[0]>-1.0+VOLMORTARCUTTOL) xi0=true;
        if(xi[1]>-1.0+VOLMORTARCUTTOL) xi1=true;
        if(xi[2]>-1.0+VOLMORTARCUTTOL) xi2=true;

        if(xi[0]<1.0-VOLMORTARCUTTOL) xi0n=true;
        if(xi[1]<1.0-VOLMORTARCUTTOL) xi1n=true;
        if(xi[2]<1.0-VOLMORTARCUTTOL) xi2n=true;
      }

      else if(mele.Shape()==DRT::Element::tet4)
      {
        if(xi[0]>0.0+VOLMORTARCUTTOL) xi0=true;
        if(xi[1]>0.0+VOLMORTARCUTTOL) xi1=true;
        if(xi[2]>0.0+VOLMORTARCUTTOL) xi2=true;
        if((xi[0]+xi[1]+xi[2])<1.0-3.0*VOLMORTARCUTTOL) all=true;
      }
    }
  }

  if(mele.Shape()==DRT::Element::tet4)
  {
    if(!xi0 or !xi1 or !xi2 or !all)
      return false;
  }
  if(mele.Shape()==DRT::Element::hex8)
  {
    if(!xi0 or !xi1 or !xi2 or !xi0n or !xi1n or !xi2n)
      return false;
  }

  }


  //--------------------------------------------------------
  // 3. master nodes within slave parameter space?
  for(int u=0;u<mele.NumNode();++u)
  {
    xgl[0] = mele.Nodes()[u]->X()[0];
    xgl[1] = mele.Nodes()[u]->X()[1];
    xgl[2] = mele.Nodes()[u]->X()[2];

    // global to local:
    if (sele.Shape()==DRT::Element::hex8)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::hex8>(sele,xgl,xi,converged);
    else if (sele.Shape()==DRT::Element::tet4)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::tet4>(sele,xgl,xi,converged);
    else
      dserror("Shape function not supported!");

    if(converged==true)
    {
      if(sele.Shape()==DRT::Element::hex8)
        if( abs(xi[0])<1.0-VOLMORTARCUT2TOL and
            abs(xi[1])<1.0-VOLMORTARCUT2TOL and
            abs(xi[2])<1.0-VOLMORTARCUT2TOL )
          return true;

      if(sele.Shape()==DRT::Element::tet4)
        if( xi[0]>0.0+VOLMORTARCUT2TOL and xi[0]<1.0-VOLMORTARCUT2TOL and
            xi[1]>0.0+VOLMORTARCUT2TOL and xi[1]<1.0-VOLMORTARCUT2TOL and
            xi[2]>0.0+VOLMORTARCUT2TOL and xi[2]<1.0-VOLMORTARCUT2TOL and
            (xi[0]+xi[1]+xi[2])<1.0-3.0*VOLMORTARCUT2TOL)
          return true;
    }
  }

  //--------------------------------------------------------
  // 4. slave nodes within master parameter space?
  for(int u=0;u<sele.NumNode();++u)
  {
    xgl[0] = sele.Nodes()[u]->X()[0];
    xgl[1] = sele.Nodes()[u]->X()[1];
    xgl[2] = sele.Nodes()[u]->X()[2];

    // global to local:
    if (mele.Shape()==DRT::Element::hex8)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::hex8>(mele,xgl,xi,converged);
    else if (mele.Shape()==DRT::Element::tet4)
      MORTAR::UTILS::GlobalToLocal<DRT::Element::tet4>(mele,xgl,xi,converged);
    else
      dserror("Shape function not supported!");

    if(converged==true)
    {
      if(mele.Shape()==DRT::Element::hex8)
        if( abs(xi[0])<1.0-VOLMORTARCUT2TOL and
            abs(xi[1])<1.0-VOLMORTARCUT2TOL and
            abs(xi[2])<1.0-VOLMORTARCUT2TOL )
          return true;

      if(mele.Shape()==DRT::Element::tet4)
        if( xi[0]>0.0+VOLMORTARCUT2TOL and xi[0]<1.0-VOLMORTARCUT2TOL and
            xi[1]>0.0+VOLMORTARCUT2TOL and xi[1]<1.0-VOLMORTARCUT2TOL and
            xi[2]>0.0+VOLMORTARCUT2TOL and xi[2]<1.0-VOLMORTARCUT2TOL and
            (xi[0]+xi[1]+xi[2])<1.0-3.0*VOLMORTARCUT2TOL)
          return true;
    }
  }

  return false;
}

/*----------------------------------------------------------------------*
 |  Integrate2D Cells                                        farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Integrate2D(DRT::Element& sele,
                                            DRT::Element& mele,
                                            std::vector<Teuchos::RCP<MORTAR::IntCell> >& cells)
{
  //--------------------------------------------------------------------
  // loop over cells for A Field
  // loop over cells
  for(int q=0;q<(int)cells.size();++q)
  {
    switch (sele.Shape())
    {
    // 2D surface elements
    case DRT::Element::quad4:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::quad4:
      {
        static VolMortarIntegrator<DRT::Element::quad4,DRT::Element::quad4> integrator(Params());
        integrator.IntegrateCells2D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,Adiscret_,Bdiscret_);
        break;
      }
      case DRT::Element::tri3:
      {
        static VolMortarIntegrator<DRT::Element::quad4,DRT::Element::tri3> integrator(Params());
        integrator.IntegrateCells2D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,Adiscret_,Bdiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    case DRT::Element::tri3:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::quad4:
      {
        static VolMortarIntegrator<DRT::Element::tri3,DRT::Element::quad4> integrator(Params());
        integrator.IntegrateCells2D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,Adiscret_,Bdiscret_);
        break;
      }
      case DRT::Element::tri3:
      {
        static VolMortarIntegrator<DRT::Element::tri3,DRT::Element::quad4> integrator(Params());
        integrator.IntegrateCells2D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,Adiscret_,Bdiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }

    //--------------------------------------------------------------------
    // loop over cells for A Field
    switch (mele.Shape())
    {
    // 2D surface elements
    case DRT::Element::quad4:
    {
      switch (sele.Shape())
      {
      // 2D surface elements
      case DRT::Element::quad4:
      {
        static VolMortarIntegrator<DRT::Element::quad4,DRT::Element::quad4> integrator(Params());
        integrator.IntegrateCells2D(mele,sele,cells[q],*dmatrixB_,*mmatrixB_,Bdiscret_,Adiscret_);
        break;
      }
      case DRT::Element::tri3:
      {
        static VolMortarIntegrator<DRT::Element::quad4,DRT::Element::tri3> integrator(Params());
        integrator.IntegrateCells2D(mele,sele,cells[q],*dmatrixB_,*mmatrixB_,Bdiscret_,Adiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    case DRT::Element::tri3:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::quad4:
      {
        static VolMortarIntegrator<DRT::Element::tri3,DRT::Element::quad4> integrator(Params());
        integrator.IntegrateCells2D(mele,sele,cells[q],*dmatrixB_,*mmatrixB_,Bdiscret_,Adiscret_);
        break;
      }
      case DRT::Element::tri3:
      {
        static VolMortarIntegrator<DRT::Element::tri3,DRT::Element::quad4> integrator(Params());
        integrator.IntegrateCells2D(mele,sele,cells[q],*dmatrixB_,*mmatrixB_,Bdiscret_,Adiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
  } // end loop

  return;
}

/*----------------------------------------------------------------------*
 |  Integrate3D Cells                                        farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Integrate3DCell(DRT::Element& sele,
                                                DRT::Element& mele,
                                                std::vector<Teuchos::RCP<Cell> >& cells)
{
  //--------------------------------------------------------------------
  // loop over cells for A Field
  for(int q=0;q<(int)cells.size();++q)
  {
    switch (sele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::hex8:
      {
        static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex8> integrator(Params());
        integrator.InitializeGP(false,0,cells[q]->Shape());
        integrator.IntegrateCells3D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      case DRT::Element::tet4:
      {
        static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet4> integrator(Params());
        integrator.InitializeGP(false,0,cells[q]->Shape());
        integrator.IntegrateCells3D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    case DRT::Element::tet4:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::hex8:
      {
        static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex8> integrator(Params());
        integrator.InitializeGP(false,0,cells[q]->Shape());
        integrator.IntegrateCells3D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      case DRT::Element::tet4:
      {
        static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet4> integrator(Params());
        integrator.InitializeGP(false,0,cells[q]->Shape());
        integrator.IntegrateCells3D(sele,mele,cells[q],*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
  } // end cell loop
  return;
}


/*----------------------------------------------------------------------*
 |  Integrate3D Cells                                        farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Integrate3DEleBased_ADis(DRT::Element& Aele,
                                                         std::vector<int>& foundeles)
{
  // assume that all BDis element types are equal
  DRT::Element::DiscretizationType btype = Bdiscret_->lColElement(0)->Shape();

  switch (Aele.Shape())
  {
  // 2D surface elements
  case DRT::Element::hex8:
  {
    switch (btype)
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::tet4:
  {
    switch (btype)
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::hex27:
  {
    switch (btype)
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::hex20:
  {
    switch (btype)
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::tet10:
  {
    switch (btype)
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,0);
      integrator.IntegrateEleBased3D_ADis(Aele,foundeles,*dmatrixA_,*mmatrixA_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  default:
  {
    dserror("unknown shape!");
    break;
  }
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Integrate3D Cells                                        farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Integrate3DEleBased_BDis(DRT::Element& Bele,
                                                         std::vector<int>& foundeles)
{
  // assume that all BDis element types are equal
  DRT::Element::DiscretizationType atype = Adiscret_->lColElement(0)->Shape();

  switch (atype)
  {
  // 2D surface elements
  case DRT::Element::hex8:
  {
    switch (Bele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::tet4:
  {
    switch (Bele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::hex27:
  {
    switch (Bele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::hex27,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::hex20:
  {
    switch (Bele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::hex20,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::tet10:
  {
    switch (Bele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex27:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::hex27> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::hex20:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::hex20> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet10:
    {
      static VolMortarIntegrator<DRT::Element::tet10,DRT::Element::tet10> integrator(Params());
      integrator.InitializeGP(true,1);
      integrator.IntegrateEleBased3D_BDis(Bele,foundeles,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  default:
  {
    dserror("unknown shape!");
    break;
  }
  }

  return;
}
/*----------------------------------------------------------------------*
 |  Integrate3D Cells for direct divergence approach         farah 04/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Integrate3DCell_DirectDivergence(DRT::Element& sele,
                                                                 DRT::Element& mele,
                                                                 bool switched_conf)
{
  if((int)(volcell_.size())>1)
    std::cout << "****************************   CELL SIZE > 1 ***************************" << std::endl;

  for(int i=0; i<(int)(volcell_.size());++i)
  {

    if(volcell_[i]==NULL)
      continue;

    GEO::CUT::VolumeCell * vc = volcell_[i];

    if(vc->IsNegligiblySmall())
      continue;

    // main gp rule
    Teuchos::RCP<DRT::UTILS::GaussPoints> intpoints = vc->GetGaussRule();

    //--------------------------------------------------------------------
    // loop over cells for A Field

    switch (sele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::hex8:
      {
        static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex8> integrator(Params());
        integrator.IntegrateCells3D_DirectDiveregence(sele,mele,*vc,intpoints,switched_conf,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      case DRT::Element::tet4:
      {
        static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet4> integrator(Params());
        integrator.IntegrateCells3D_DirectDiveregence(sele,mele,*vc,intpoints,switched_conf,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    case DRT::Element::tet4:
    {
      switch (mele.Shape())
      {
      // 2D surface elements
      case DRT::Element::hex8:
      {
        static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex8> integrator(Params());
        integrator.IntegrateCells3D_DirectDiveregence(sele,mele,*vc,intpoints,switched_conf,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      case DRT::Element::tet4:
      {
        static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet4> integrator(Params());
        integrator.IntegrateCells3D_DirectDiveregence(sele,mele,*vc,intpoints,switched_conf,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
            Adiscret_,Bdiscret_);
        break;
      }
      default:
      {
        dserror("unknown shape!");
        break;
      }
      }
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
  }

  return;
}


/*----------------------------------------------------------------------*
 |  Integrate over A-element domain                          farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Integrate3D(DRT::Element& sele,
                                            DRT::Element& mele,
                                            int domain)
{
  //--------------------------------------------------------------------
  // loop over cells for A Field
  switch (sele.Shape())
  {
  // 2D surface elements
  case DRT::Element::hex8:
  {
    switch (mele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,domain);
      integrator.IntegrateEle3D(domain,sele,mele,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::hex8,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,domain);
      integrator.IntegrateEle3D(domain,sele,mele,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  case DRT::Element::tet4:
  {
    switch (mele.Shape())
    {
    // 2D surface elements
    case DRT::Element::hex8:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::hex8> integrator(Params());
      integrator.InitializeGP(true,domain);
      integrator.IntegrateEle3D(domain,sele,mele,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    case DRT::Element::tet4:
    {
      static VolMortarIntegrator<DRT::Element::tet4,DRT::Element::tet4> integrator(Params());
      integrator.InitializeGP(true,domain);
      integrator.IntegrateEle3D(domain,sele,mele,*dmatrixA_,*mmatrixA_,*dmatrixB_,*mmatrixB_,
          Adiscret_,Bdiscret_);
      break;
    }
    default:
    {
      dserror("unknown shape!");
      break;
    }
    }
    break;
  }
  default:
  {
    dserror("unknown shape!");
    break;
  }
  }

  // integration element counter
  inteles_++;

  return;
}


/*----------------------------------------------------------------------*
 |  init (public)                                            farah 10/13|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Initialize()
{
  /* ******************************************************************
   * (re)setup global Mortar LINALG::SparseMatrices                   *
   * unknowns which are going to be condensed are defined on the slave*
   * side. Therefore, the rows are the auxiliary variables on the     *
   * slave side!                                                      *
   * ******************************************************************/

  dmatrixA_ = Teuchos::rcp(new LINALG::SparseMatrix(*ADiscret()->DofRowMap(1),10));
  mmatrixA_ = Teuchos::rcp(new LINALG::SparseMatrix(*ADiscret()->DofRowMap(1),100));

  dmatrixB_ = Teuchos::rcp(new LINALG::SparseMatrix(*BDiscret()->DofRowMap(1),10));
  mmatrixB_ = Teuchos::rcp(new LINALG::SparseMatrix(*BDiscret()->DofRowMap(1),100));

  // initialize trafo operator for quadr. modification
  if(dualquad_ != INPAR::VOLMORTAR::dualquad_no_mod)
  {
    TA_    = Teuchos::rcp(new LINALG::SparseMatrix(*ADiscret()->DofRowMap(1),10));
    TB_    = Teuchos::rcp(new LINALG::SparseMatrix(*BDiscret()->DofRowMap(1),10));
  }

  return;
}

/*----------------------------------------------------------------------*
 |  Complete (public)                                        farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::Complete()
{
  //complete...
  dmatrixA_->Complete(*ADiscret()->DofRowMap(1),*ADiscret()->DofRowMap(1));
  mmatrixA_->Complete(*BDiscret()->DofRowMap(0),*ADiscret()->DofRowMap(1));

  dmatrixB_->Complete(*BDiscret()->DofRowMap(1),*BDiscret()->DofRowMap(1));
  mmatrixB_->Complete(*ADiscret()->DofRowMap(0),*BDiscret()->DofRowMap(1));

  // complete trafo operator for quadr. modification
  if(dualquad_ != INPAR::VOLMORTAR::dualquad_no_mod)
  {
    TA_->Complete(*ADiscret()->DofRowMap(1),*ADiscret()->DofRowMap(1));
    TB_->Complete(*BDiscret()->DofRowMap(1),*BDiscret()->DofRowMap(1));
  }

  return;
}

/*----------------------------------------------------------------------*
 |  compute projection operator P                            farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::CreateProjectionOpterator()
{
  /********************************************************************/
  /* Multiply Mortar matrices: P = inv(D) * M         A               */
  /********************************************************************/
  Teuchos::RCP<LINALG::SparseMatrix> invdA = Teuchos::rcp(new LINALG::SparseMatrix(*dmatrixA_));
  Teuchos::RCP<Epetra_Vector> diagA = LINALG::CreateVector(*ADiscret()->DofRowMap(1),true);
  int err = 0;

  // extract diagonal of invd into diag
  invdA->ExtractDiagonalCopy(*diagA);

  // set zero diagonal values to dummy 1.0
  for (int i=0;i<diagA->MyLength();++i)
    if ( abs((*diagA)[i])<1e-12)
      (*diagA)[i]=1.0;

  // scalar inversion of diagonal values
  err = diagA->Reciprocal(*diagA);
  if (err>0) dserror("ERROR: Reciprocal: Zero diagonal entry!");

  // re-insert inverted diagonal into invd
  err = invdA->ReplaceDiagonalValues(*diagA);

  // do the multiplication P = inv(D) * M
  //pmatrixA_
  Teuchos::RCP<LINALG::SparseMatrix> auxa = LINALG::MLMultiply(*invdA,false,*mmatrixA_,false,false,false,true);

  /********************************************************************/
  /* Multiply Mortar matrices: P = inv(D) * M         B               */
  /********************************************************************/
  Teuchos::RCP<LINALG::SparseMatrix> invdB = Teuchos::rcp(new LINALG::SparseMatrix(*dmatrixB_));
  Teuchos::RCP<Epetra_Vector> diagB = LINALG::CreateVector(*BDiscret()->DofRowMap(1),true);

  // extract diagonal of invd into diag
  invdB->ExtractDiagonalCopy(*diagB);

  // set zero diagonal values to dummy 1.0
  for (int i=0;i<diagB->MyLength();++i)
    if ( abs((*diagB)[i])<1e-12) (*diagB)[i]=1.0;

  // scalar inversion of diagonal values
  err = diagB->Reciprocal(*diagB);
  if (err>0) dserror("ERROR: Reciprocal: Zero diagonal entry!");

  // re-insert inverted diagonal into invd
  err = invdB->ReplaceDiagonalValues(*diagB);

  // do the multiplication P = inv(D) * M
  //pmatrixB_
  Teuchos::RCP<LINALG::SparseMatrix> auxb = LINALG::MLMultiply(*invdB,false,*mmatrixB_,false,false,false,true);

  // initialize trafo operator for quadr. modification
  if(dualquad_ != INPAR::VOLMORTAR::dualquad_no_mod)
  {
    pmatrixA_ = LINALG::MLMultiply(*TA_,false,*auxa,false,false,false,true);
    pmatrixB_ = LINALG::MLMultiply(*TB_,false,*auxb,false,false,false,true);
  }
  else
  {
    pmatrixA_ = auxa;
    pmatrixB_ = auxb;
  }

//  std::cout << "auxa= " << *auxa << std::endl;
//  std::cout << "pmatrixA_= " << *pmatrixA_ << std::endl;
//  std::cout << "auxb= " << *auxb << std::endl;
//  std::cout << "pmatrixB_= " << *pmatrixB_ << std::endl;
//  std::cout << "TB_= " << *TB_ << std::endl;


  return;
}

/*----------------------------------------------------------------------*
 |  Define polygon of mortar vertices                        farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::DefineVerticesSlave(DRT::Element& ele,
    std::vector<MORTAR::Vertex>& SlaveVertices)
{
  // project slave nodes onto auxiliary plane
  int nnodes = ele.NumNode();
  DRT::Node** mynodes = ele.Nodes();
  if (!mynodes) dserror("ERROR: ProjectSlave: Null pointer!");

  // initialize storage for slave coords + their ids
  std::vector<double> vertices(3);
  std::vector<int> snodeids(1);

  for (int i=0;i<nnodes;++i)
  {
    // compute projection
    for (int k=0;k<3;++k)
      vertices[k] = mynodes[i]->X()[k];

    // get node id, too
    snodeids[0] = mynodes[i]->Id();

    // store into vertex data structure
    SlaveVertices.push_back(MORTAR::Vertex(vertices,MORTAR::Vertex::slave,snodeids,NULL,NULL,false,false,NULL,-1.0));
  }
  return;
}

/*----------------------------------------------------------------------*
 |  Define polygon of mortar vertices                        farah 01/14|
 *----------------------------------------------------------------------*/
void VOLMORTAR::VolMortarCoupl::DefineVerticesMaster(DRT::Element& ele,
    std::vector<MORTAR::Vertex>& SlaveVertices)
{
  // project slave nodes onto auxiliary plane
  int nnodes = ele.NumNode();
  DRT::Node** mynodes = ele.Nodes();
  if (!mynodes) dserror("ERROR: ProjectSlave: Null pointer!");

  // initialize storage for slave coords + their ids
  std::vector<double> vertices(3);
  std::vector<int> snodeids(1);

  for (int i=0;i<nnodes;++i)
  {
    // compute projection
    for (int k=0;k<3;++k)
      vertices[k] = mynodes[i]->X()[k];

    // get node id, too
    snodeids[0] = mynodes[i]->Id();

    // store into vertex data structure
    SlaveVertices.push_back(MORTAR::Vertex(vertices,MORTAR::Vertex::projmaster,snodeids,NULL,NULL,false,false,NULL,-1.0));
  }
  return;
}

/*----------------------------------------------------------------------*
 |  Clipping of two polygons (NEW version)                    popp 11/09|
 *----------------------------------------------------------------------*/
bool VOLMORTAR::VolMortarCoupl::PolygonClippingConvexHull(std::vector<MORTAR::Vertex>& poly1,
                                                    std::vector<MORTAR::Vertex>& poly2,
                                                    std::vector<MORTAR::Vertex>& respoly,
                                                    DRT::Element& sele,
                                                    DRT::Element& mele,
                                                    double& tol)
{
  //**********************************************************************
  // STEP1: Input check
  // - input polygons must consist of min. 3 vertices each
  // - rotation of poly1 must be c-clockwise w.r.t. (0,0,1) or Auxn()
  // - rotation of poly 2 changed to c-clockwise w.r.t. (0,0,1) or Auxn()
  // - both input polygons must be convex
  //**********************************************************************

  // check input variables
  if ((int)poly1.size()<3 || (int)poly2.size()<3)
    dserror("ERROR: Input Polygons must consist of min. 3 vertices each");

  // check for rotation of polygon1 (slave) and polgon 2 (master)
  // note that we implicitly already rely on convexity here!
  // first get geometric centers of polygon1 and polygon2
  double center1[3] = {0.0, 0.0, 0.0};
  double center2[3] = {0.0, 0.0, 0.0};

  for (int i=0;i<(int)poly1.size();++i)
    for (int k=0;k<3;++k)
      center1[k] += poly1[i].Coord()[k]/((int)poly1.size());

  for (int i=0;i<(int)poly2.size();++i)
    for (int k=0;k<3;++k)
      center2[k] += poly2[i].Coord()[k]/((int)poly2.size());

  // then we compute the counter-clockwise plane normal
  double diff1[3] = {0.0, 0.0, 0.0};
  double edge1[3] = {0.0, 0.0, 0.0};
  double diff2[3] = {0.0, 0.0, 0.0};
  double edge2[3] = {0.0, 0.0, 0.0};

  for (int k=0;k<3;++k)
  {
    diff1[k] = poly1[0].Coord()[k]-center1[k];
    edge1[k] = poly1[1].Coord()[k]-poly1[0].Coord()[k];
    diff2[k] = poly2[0].Coord()[k]-center2[k];
    edge2[k] = poly2[1].Coord()[k]-poly2[0].Coord()[k];
  }

  double cross1[3] = {0.0, 0.0, 0.0};
  double cross2[3] = {0.0, 0.0, 0.0};

  cross1[0] = diff1[1]*edge1[2]-diff1[2]*edge1[1];
  cross1[1] = diff1[2]*edge1[0]-diff1[0]*edge1[2];
  cross1[2] = diff1[0]*edge1[1]-diff1[1]*edge1[0];

  cross2[0] = diff2[1]*edge2[2]-diff2[2]*edge2[1];
  cross2[1] = diff2[2]*edge2[0]-diff2[0]*edge2[2];
  cross2[2] = diff2[0]*edge2[1]-diff2[1]*edge2[0];

  // check against auxiliary plane normal
  double check1 = cross1[0]*Auxn()[0]+cross1[1]*Auxn()[1]+cross1[2]*Auxn()[2];
  double check2 = cross2[0]*Auxn()[0]+cross2[1]*Auxn()[1]+cross2[2]*Auxn()[2];

  // check polygon 1 and throw dserror if not c-clockwise
  if (check1<=0) dserror("ERROR: Polygon 1 (slave) not ordered counter-clockwise!");

  // check polygon 2 and reorder in c-clockwise direction
  if (check2<0)
    std::reverse(poly2.begin(), poly2.end());

  // check if the two input polygons are convex
  // a polygon is convex if the scalar product of an edge normal and the
  // next edge direction is negative for all edges
  for (int i=0;i<(int)poly1.size();++i)
  {
    // we need the edge vector first
    double edge[3] = {0.0, 0.0, 0.0};
    for (int k=0;k<3;++k)
    {
      if (i!=(int)poly1.size()-1) edge[k] = poly1[i+1].Coord()[k] - poly1[i].Coord()[k];
      else edge[k] = poly1[0].Coord()[k] - poly1[i].Coord()[k];
    }

    // edge normal is result of cross product
    double n[3] = {0.0, 0.0, 0.0};
    n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
    n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
    n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];

    // we need the next edge vector now
    double nextedge[3] = {0.0, 0.0, 0.0};
    for (int k=0;k<3;++k)
    {
      if (i<(int)poly1.size()-2) nextedge[k] = poly1[i+2].Coord()[k] - poly1[i+1].Coord()[k];
      else if (i==(int)poly1.size()-2) nextedge[k] = poly1[0].Coord()[k] - poly1[i+1].Coord()[k];
      else nextedge[k] = poly1[1].Coord()[k] - poly1[0].Coord()[k];
    }

    // check scalar product
    double check = n[0]*nextedge[0]+n[1]*nextedge[1]+n[2]*nextedge[2];
    if (check>0) dserror("ERROR: Input polygon 1 not convex");
  }

  for (int i=0;i<(int)poly2.size();++i)
  {
    // we need the edge vector first
    double edge[3] = {0.0, 0.0, 0.0};
    for (int k=0;k<3;++k)
    {
      if (i!=(int)poly2.size()-1) edge[k] = poly2[i+1].Coord()[k] - poly2[i].Coord()[k];
      else edge[k] = poly2[0].Coord()[k] - poly2[i].Coord()[k];
    }

    // edge normal is result of cross product
    double n[3] = {0.0, 0.0, 0.0};
    n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
    n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
    n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];

    // we need the next edge vector now
    double nextedge[3] = {0.0, 0.0, 0.0};
    for (int k=0;k<3;++k)
    {
      if (i<(int)poly2.size()-2) nextedge[k] = poly2[i+2].Coord()[k] - poly2[i+1].Coord()[k];
      else if (i==(int)poly2.size()-2) nextedge[k] = poly2[0].Coord()[k] - poly2[i+1].Coord()[k];
      else nextedge[k] = poly2[1].Coord()[k] - poly2[0].Coord()[k];
    }

    // check scalar product
    double check = n[0]*nextedge[0]+n[1]*nextedge[1]+n[2]*nextedge[2];
    if (check>0)
    {
      // this may happen, so do NOT throw an error immediately
      // but instead check if the two elements to be clipped are
      // close to each other at all. If so, then really throw the
      // dserror, if not, simply continue with the next pair!
      int sid = sele.Id();
      int mid = mele.Id();
      bool nearcheck = true;//RoughCheckNodes();
      if (nearcheck)
      {
        std::cout << "***WARNING*** Input polygon 2 not convex! (S/M-pair: " << sid << "/" << mid << ")" << std::endl;
      }

      return false;
    }
  }

  //**********************************************************************
  // STEP2: Extend Vertex data structures
  // - note that poly1 is the slave element and poly2 the master element
  // - assign Next() and Prev() pointers to initialize linked structure
  //**********************************************************************
  // set previous and next Vertex pointer for all elements in lists
  for (int i=0;i<(int)poly1.size();++i)
  {
    // standard case
    if (i!=0 && i!=(int)poly1.size()-1)
    {
      poly1[i].AssignNext(&poly1[i+1]);
      poly1[i].AssignPrev(&poly1[i-1]);
    }
    // first element in list
    else if (i==0)
    {
      poly1[i].AssignNext(&poly1[i+1]);
      poly1[i].AssignPrev(&poly1[(int)poly1.size()-1]);
    }
    // last element in list
    else
    {
      poly1[i].AssignNext(&poly1[0]);
      poly1[i].AssignPrev(&poly1[i-1]);
    }
  }
  for (int i=0;i<(int)poly2.size();++i)
  {
    // standard case
    if (i!=0 && i!=(int)poly2.size()-1)
    {
      poly2[i].AssignNext(&poly2[i+1]);
      poly2[i].AssignPrev(&poly2[i-1]);
    }
    // first element in list
    else if (i==0)
    {
      poly2[i].AssignNext(&poly2[i+1]);
      poly2[i].AssignPrev(&poly2[(int)poly2.size()-1]);
    }
    // last element in list
    else
    {
      poly2[i].AssignNext(&poly2[0]);
      poly2[i].AssignPrev(&poly2[i-1]);
    }
  }

  //**********************************************************************
  // STEP3: Perform line intersection of all edge pairs
  // - this yields a new vector of intersection vertices
  // - by default the respective edge end vertices are assumed to be
  //   the next/prev vertices and connectivity is set up accordingly
  //**********************************************************************
  std::vector<MORTAR::Vertex> intersec;

  for (int i=0;i<(int)poly1.size();++i)
  {
    for (int j=0;j<(int)poly2.size();++j)
    {
      // we need two edges first
      double edge1[3] = {0.0, 0.0, 0.0};
      double edge2[3] = {0.0, 0.0, 0.0};
      for (int k=0;k<3;++k)
      {
        edge1[k] = (poly1[i].Next())->Coord()[k] - poly1[i].Coord()[k];
        edge2[k] = (poly2[j].Next())->Coord()[k] - poly2[j].Coord()[k];
      }

      // outward edge normals of polygon 1 and 2 edges
      double n1[3] = {0.0, 0.0, 0.0};
      double n2[3] = {0.0, 0.0, 0.0};
      n1[0] = edge1[1]*Auxn()[2]-edge1[2]*Auxn()[1];
      n1[1] = edge1[2]*Auxn()[0]-edge1[0]*Auxn()[2];
      n1[2] = edge1[0]*Auxn()[1]-edge1[1]*Auxn()[0];
      n2[0] = edge2[1]*Auxn()[2]-edge2[2]*Auxn()[1];
      n2[1] = edge2[2]*Auxn()[0]-edge2[0]*Auxn()[2];
      n2[2] = edge2[0]*Auxn()[1]-edge2[1]*Auxn()[0];

      // check for parallelity of edges
      double parallel = edge1[0]*n2[0]+edge1[1]*n2[1]+edge1[2]*n2[2];
      if (abs(parallel)<tol)
        continue;

      // check for intersection of non-parallel edges
      double wec_p1 = 0.0;
      double wec_p2 = 0.0;
      for (int k=0;k<3;++k)
      {
        wec_p1 += (poly1[i].Coord()[k] - poly2[j].Coord()[k]) * n2[k];
        wec_p2 += ((poly1[i].Next())->Coord()[k] - poly2[j].Coord()[k]) * n2[k];
      }

      if (wec_p1*wec_p2<=0)
      {
        double wec_q1 = 0.0;
        double wec_q2 = 0.0;
        for (int k=0;k<3;++k)
        {
          wec_q1 += (poly2[j].Coord()[k] - poly1[i].Coord()[k]) * n1[k];
          wec_q2 += ((poly2[j].Next())->Coord()[k] - poly1[i].Coord()[k]) * n1[k];
        }

        if (wec_q1*wec_q2<=0)
        {
          double alphap = wec_p1/(wec_p1-wec_p2);
          double alphaq = wec_q1/(wec_q1-wec_q2);
          std::vector<double> ip(3);
          std::vector<double> iq(3);
          for (int k=0;k<3;++k)
          {
            ip[k] = (1-alphap) * poly1[i].Coord()[k] + alphap * (poly1[i].Next())->Coord()[k];
            iq[k] = (1-alphaq) * poly2[j].Coord()[k] + alphaq * (poly2[j].Next())->Coord()[k];
            if (abs(ip[k]) < tol) ip[k] = 0.0;
            if (abs(iq[k]) < tol) iq[k] = 0.0;
          }

          // generate vectors of underlying node ids for lineclip (2x slave, 2x master)
          std::vector<int> lcids(4);
          lcids[0] = (int)(poly1[i].Nodeids()[0]);
          lcids[1] = (int)((poly1[i].Next())->Nodeids()[0]);
          lcids[2] = (int)(poly2[j].Nodeids()[0]);
          lcids[3] = (int)((poly2[j].Next())->Nodeids()[0]);

          // store intersection points
          intersec.push_back(MORTAR::Vertex(ip,MORTAR::Vertex::lineclip,lcids,poly1[i].Next(),&poly1[i],true,false,NULL,alphap));
        }
      }
    }
  }

  //**********************************************************************
  // STEP4: Collapse line intersections
  // - this yields a collapsed vector of intersection vertices
  // - those intersection points close to poly1/poly2 vertices are deleted
  //**********************************************************************
  std::vector<MORTAR::Vertex> collintersec;

  for (int i=0;i<(int)intersec.size();++i)
  {
    // keep track of comparisons
    bool close = false;

    // check against all poly1 (slave) points
    for (int j=0;j<(int)poly1.size();++j)
    {
      // distance vector
      double diff[3] = {0.0, 0.0, 0.0};
      for (int k=0;k<3;++k) diff[k] = intersec[i].Coord()[k] - poly1[j].Coord()[k];
      double dist = sqrt(diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2]);

      // only keep intersection point if not close
      if (dist <= tol)
      {
        close = true;
        break;
      }
    }

    // do only if no close poly1 (slave) point found
    if (!close)
    {
      // check against all poly2 (master) points
      for (int j=0;j<(int)poly2.size();++j)
      {
        // distance vector
        double diff[3] = {0.0, 0.0, 0.0};
        for (int k=0;k<3;++k) diff[k] = intersec[i].Coord()[k] - poly2[j].Coord()[k];
        double dist = sqrt(diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2]);

        // only keep intersection point if not close
        if (dist <= tol)
        {
          close = true;
          break;
        }
      }
    }

    // keep intersection point only if not close to any poly1/poly2 point
    if (!close) collintersec.push_back(intersec[i]);
  }

  //**********************************************************************
  // STEP5: Create points of convex hull
  // - check all poly1 points against all poly1/poly2 edges
  // - check all poly2 points against all poly1/poly2 edges
  // - check all collintersec points against all poly1/poly2 edges
  // - points outside any poly1/poly2 edge are NOT in the convex hull
  // - as a result we obtain all points forming the convex hull
  //**********************************************************************
  std::vector<MORTAR::Vertex> convexhull;

  //----------------------------------------------------check poly1 points
  for (int i=0;i<(int)poly1.size();++i)
  {
    // keep track of inside / outside status
    bool outside = false;

    // check against all poly1 (slave) edges
    for (int j=0;j<(int)poly1.size();++j)
    {
      // we need diff vector and edge2 first
      double diff[3] = {0.0, 0.0, 0.0};
      double edge[3] = {0.0, 0.0, 0.0};
      for (int k=0;k<3;++k)
      {
        diff[k] = poly1[i].Coord()[k] - poly1[j].Coord()[k];
        edge[k] = (poly1[j].Next())->Coord()[k] - poly1[j].Coord()[k];
      }

      // compute distance from point on poly1 to edge
      double n[3] = {0.0, 0.0, 0.0};
      n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
      n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
      n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];
      double ln = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
      for (int k=0;k<3;++k) n[k] /= ln;

      double dist = diff[0]*n[0]+diff[1]*n[1]+diff[2]*n[2];

      // only keep point if not in outside halfspace
      if (dist > tol)
      {
        outside = true;
        break;
      }
    }

    // do only if not already outside w.r.t. to a poly1 (slave) edge
    if (!outside)
    {
      // check against all poly2 (master) edges
      for (int j=0;j<(int)poly2.size();++j)
      {
        // we need diff vector and edge2 first
        double diff[3] = {0.0, 0.0, 0.0};
        double edge[3] = {0.0, 0.0, 0.0};
        for (int k=0;k<3;++k)
        {
          diff[k] = poly1[i].Coord()[k] - poly2[j].Coord()[k];
          edge[k] = (poly2[j].Next())->Coord()[k] - poly2[j].Coord()[k];
        }

        // compute distance from point on poly1 to edge
        double n[3] = {0.0, 0.0, 0.0};
        n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
        n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
        n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];
        double ln = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        for (int k=0;k<3;++k) n[k] /= ln;

        double dist = diff[0]*n[0]+diff[1]*n[1]+diff[2]*n[2];

        // only keep point if not in outside halfspace
        if (dist > tol)
        {
          outside = true;
          break;
        }
      }
    }

    // only keep point if never in outside halfspace
    if (!outside) convexhull.push_back(poly1[i]);
  }

  //----------------------------------------------------check poly2 points
  for (int i=0;i<(int)poly2.size();++i)
  {
    // keep track of inside / outside status
    bool outside = false;

    // check against all poly1 (slave) edges
    for (int j=0;j<(int)poly1.size();++j)
    {
      // we need diff vector and edge2 first
      double diff[3] = {0.0, 0.0, 0.0};
      double edge[3] = {0.0, 0.0, 0.0};
      for (int k=0;k<3;++k)
      {
        diff[k] = poly2[i].Coord()[k] - poly1[j].Coord()[k];
        edge[k] = (poly1[j].Next())->Coord()[k] - poly1[j].Coord()[k];
      }

      // compute distance from point on poly1 to edge
      double n[3] = {0.0, 0.0, 0.0};
      n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
      n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
      n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];
      double ln = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
      for (int k=0;k<3;++k) n[k] /= ln;

      double dist = diff[0]*n[0]+diff[1]*n[1]+diff[2]*n[2];

      // only keep point if not in outside halfspace
      if (dist > tol)
      {
        outside = true;
        break;
      }
    }

    // do only if not already outside w.r.t. to a poly1 (slave) edge
    if (!outside)
    {
      // check against all poly2 (master) edges
      for (int j=0;j<(int)poly2.size();++j)
      {
        // we need diff vector and edge2 first
        double diff[3] = {0.0, 0.0, 0.0};
        double edge[3] = {0.0, 0.0, 0.0};
        for (int k=0;k<3;++k)
        {
          diff[k] = poly2[i].Coord()[k] - poly2[j].Coord()[k];
          edge[k] = (poly2[j].Next())->Coord()[k] - poly2[j].Coord()[k];
        }

        // compute distance from point on poly1 to edge
        double n[3] = {0.0, 0.0, 0.0};
        n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
        n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
        n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];
        double ln = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        for (int k=0;k<3;++k) n[k] /= ln;

        double dist = diff[0]*n[0]+diff[1]*n[1]+diff[2]*n[2];

        // only keep point if not in outside halfspace
        if (dist > tol)
        {
          outside = true;
          break;
        }
      }
    }

    // only keep point if never in outside halfspace
    if (!outside) convexhull.push_back(poly2[i]);
  }

  //---------------------------------------------check collintersec points
  for (int i=0;i<(int)collintersec.size();++i)
  {
    // keep track of inside / outside status
    bool outside = false;

    // check against all poly1 (slave) edges
    for (int j=0;j<(int)poly1.size();++j)
    {
      // we need diff vector and edge2 first
      double diff[3] = {0.0, 0.0, 0.0};
      double edge[3] = {0.0, 0.0, 0.0};
      for (int k=0;k<3;++k)
      {
        diff[k] = collintersec[i].Coord()[k] - poly1[j].Coord()[k];
        edge[k] = (poly1[j].Next())->Coord()[k] - poly1[j].Coord()[k];
      }

      // compute distance from point on poly1 to edge
      double n[3] = {0.0, 0.0, 0.0};
      n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
      n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
      n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];
      double ln = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
      for (int k=0;k<3;++k) n[k] /= ln;

      double dist = diff[0]*n[0]+diff[1]*n[1]+diff[2]*n[2];

      // only keep point if not in outside halfspace
      if (dist > tol)
      {
        outside = true;
        break;
      }
    }

    // do only if not already outside w.r.t. to a poly1 (slave) edge
    if (!outside)
    {
      // check against all poly2 (master) edges
      for (int j=0;j<(int)poly2.size();++j)
      {
        // we need diff vector and edge2 first
        double diff[3] = {0.0, 0.0, 0.0};
        double edge[3] = {0.0, 0.0, 0.0};
        for (int k=0;k<3;++k)
        {
          diff[k] = collintersec[i].Coord()[k] - poly2[j].Coord()[k];
          edge[k] = (poly2[j].Next())->Coord()[k] - poly2[j].Coord()[k];
        }

        // compute distance from point on poly1 to edge
        double n[3] = {0.0, 0.0, 0.0};
        n[0] = edge[1]*Auxn()[2]-edge[2]*Auxn()[1];
        n[1] = edge[2]*Auxn()[0]-edge[0]*Auxn()[2];
        n[2] = edge[0]*Auxn()[1]-edge[1]*Auxn()[0];
        double ln = sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
        for (int k=0;k<3;++k) n[k] /= ln;

        double dist = diff[0]*n[0]+diff[1]*n[1]+diff[2]*n[2];

        // only keep point if not in outside halfspace
        if (dist > tol)
        {
          outside = true;
          break;
        }
      }
    }

    // only keep point if never in outside halfspace
    if (!outside) convexhull.push_back(collintersec[i]);
  }

  //**********************************************************************
  // STEP6: Collapse convex hull points
  // - this yields a collapsed vector of convex hull vertices
  // - up to now only duplicate intersection points have been eliminated
  // - this operation now removes ALL kinds of duplicate points
  // - intersection points close to poly2/poly1 points are deleted
  // - poly2 points close to poly1 vertices are deleted
  //**********************************************************************
  std::vector<MORTAR::Vertex> collconvexhull;

  for (int i=0;i<(int)convexhull.size();++i)
  {
    // keep track of comparisons
    bool close = false;

    // do not collapse poly1 (slave) points
    if (convexhull[i].VType()==MORTAR::Vertex::slave)
    {
      collconvexhull.push_back(convexhull[i]);
      continue;
    }

    // check remaining poly2 (master) and intersec points against poly1 (slave) points
    for (int j=0;j<(int)convexhull.size();++j)
    {
      // only collapse with poly1 (slave) points
      if (convexhull[j].VType()!=MORTAR::Vertex::slave) continue;

      // distance vector
      double diff[3] = {0.0, 0.0, 0.0};
      for (int k=0;k<3;++k) diff[k] = convexhull[i].Coord()[k] - convexhull[j].Coord()[k];
      double dist = sqrt(diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2]);

      // only keep point if not close
      if (dist <= tol)
      {
        close = true;
        break;
      }
    }

    // do not check poly2 (master) points
    if (convexhull[i].VType()==MORTAR::Vertex::projmaster)
    {
      if (!close) collconvexhull.push_back(convexhull[i]);
      continue;
    }

    // check intersec points against poly2 (master) points
    if (!close && convexhull[i].VType()==MORTAR::Vertex::lineclip)
    {
      for (int j=0;j<(int)convexhull.size();++j)
      {
        // only collapse with poly2 (master) points
        if (convexhull[j].VType()!=MORTAR::Vertex::projmaster) continue;

        // distance vector
        double diff[3] = {0.0, 0.0, 0.0};
        for (int k=0;k<3;++k) diff[k] = convexhull[i].Coord()[k] - convexhull[j].Coord()[k];
        double dist = sqrt(diff[0]*diff[0]+diff[1]*diff[1]+diff[2]*diff[2]);

        // only keep intersection point if not close
        if (dist <= tol)
        {
          close = true;
          break;
        }
      }
    }

    // keep intersection point only if not collapsed
    if (!close) collconvexhull.push_back(convexhull[i]);
  }

  //**********************************************************************
  // STEP7: Transform convex hull points to auxiliary plane
  // - x* = A * (x - p1) where p1 = translation, A = rotation
  // - this yields transformed points with coords (x_bar,y_bar,0)
  //**********************************************************************
  // only continue if more than two points remaining
  if ((int)collconvexhull.size() < 3)
  {
    // no clip polygon if less than 3 points
    std::vector<MORTAR::Vertex> empty;
    respoly = empty;
  }
  else if ((int)collconvexhull.size()==3)
  {
    // 3 points already ARE the convex hull
    respoly = collconvexhull;
  }
  else
  {
    // do transformation to auxiliary plane
    double newzero[3] = {collconvexhull[0].Coord()[0],collconvexhull[0].Coord()[1],collconvexhull[0].Coord()[2]};
    double newxaxis[3] = {collconvexhull[1].Coord()[0]-collconvexhull[0].Coord()[0],
                          collconvexhull[1].Coord()[1]-collconvexhull[0].Coord()[1],
                          collconvexhull[1].Coord()[2]-collconvexhull[0].Coord()[2]};
    double newyaxis[3] = {Auxn()[1]*newxaxis[2]-Auxn()[2]*newxaxis[1],
                          Auxn()[2]*newxaxis[0]-Auxn()[0]*newxaxis[2],
                          Auxn()[0]*newxaxis[1]-Auxn()[1]*newxaxis[0]};
    double lnewxaxis = sqrt(newxaxis[0]*newxaxis[0]+newxaxis[1]*newxaxis[1]+newxaxis[2]*newxaxis[2]);
    double lnewyaxis = sqrt(newyaxis[0]*newyaxis[0]+newyaxis[1]*newyaxis[1]+newyaxis[2]*newyaxis[2]);

//    for(int h=0;h<(int)collconvexhull.size();++h)
//      std::cout << "Coord "<< h <<"= " << collconvexhull[h].Coord()[0] << " " << collconvexhull[h].Coord()[1] << " " << collconvexhull[h].Coord()[2] << std::endl;

    // normalize
    for (int k=0;k<3;++k)
    {
      newxaxis[k] /= lnewxaxis;
      newyaxis[k] /= lnewyaxis;
    }


    // trafo matrix
    LINALG::Matrix<3,3> trafo;
    for (int k=0;k<3;++k)
    {
      trafo(0,k) = newxaxis[k];
      trafo(1,k) = newyaxis[k];
      trafo(2,k) = Auxn()[k];
    }

    // temporary storage for transformed points
    int np = (int)collconvexhull.size();
    Epetra_SerialDenseMatrix transformed(2,np);

    // transform each convex hull point
    for (int i=0;i<np;++i)
    {
      double newpoint[3] = {0.0, 0.0, 0.0};

      for (int j=0;j<3;++j)
        for (int k=0;k<3;++k) newpoint[j] += trafo(j,k) * (collconvexhull[i].Coord()[k] - newzero[k]);

      if (abs(newpoint[2]) > tol) dserror("ERROR: Transformation to aux. plane failed: z!=0 !");
      transformed(0,i) = newpoint[0];
      transformed(1,i) = newpoint[1];
    }

    //**********************************************************************
    // STEP8: Sort convex hull points to obtain final clip polygon
    // - this yields the final clip polygon
    // - sanity of the generated output is checked
    //**********************************************************************
    MORTAR::SortConvexHullPoints(false, transformed, collconvexhull, respoly, tol);
  }

  return true;
}

/*----------------------------------------------------------------------*
 |  Triangulation of clip polygon (3D) - DELAUNAY             popp 08/11|
 *----------------------------------------------------------------------*/
bool VOLMORTAR::VolMortarCoupl::DelaunayTriangulation(std::vector<Teuchos::RCP<MORTAR::IntCell> >&  cells,
                                               std::vector<MORTAR::Vertex> & clip,
                                               double tol)
{
  // preparations
  cells.resize(0);
  int clipsize = (int)(clip.size());

  std::vector<std::map<int,double> > derivauxn;

  std::vector<std::vector<std::map<int,double> > > linvertex(clipsize,std::vector<std::map<int,double> >(3));
  //**********************************************************************
  // (1) Trivial clipping polygon -> IntCells
  //**********************************************************************
  // clip polygon = triangle
  // no triangulation necessary -> 1 IntCell
  if (clipsize==3)
  {
    // IntCell vertices = clip polygon vertices
    Epetra_SerialDenseMatrix coords(3,clipsize);
    for (int i=0;i<clipsize;++i)
      for (int k=0;k<3;++k)
        coords(k,i) = clip[i].Coord()[k];

    // create IntCell object and push back
    cells.push_back(Teuchos::rcp(new MORTAR::IntCell(0,3,coords,Auxn(),DRT::Element::tri3,
      linvertex[0],linvertex[1],linvertex[2],derivauxn)));

    // get out of here
    return true;
  }

  //**********************************************************************
  // (2) General clipping polygon: Triangulation -> IntCells
  //**********************************************************************
  // store Delaunay triangles here
  std::vector<std::vector<int> > triangles(0,std::vector<int>(3));

  // start with first triangle v0,v1,v2
  std::vector<int> currtriangle(3);
  currtriangle[0] = 0;
  currtriangle[1] = 1;
  currtriangle[2] = 2;
  triangles.push_back(currtriangle);

  // build Delaunay triangulation recursively (starting from a triangle
  // and then adding all remaining nodes of the clipping polygon 1-by-1)
  // loop over clip vertices v3,..,vN
  for (int c=3;c<clipsize;++c)
  {
    // current size of triangulated polygon
    int currsize = c+1;

    // add next triangle v(c-1),vc,v0
    std::vector<int> nexttriangle(3);
    nexttriangle[0] = c-1;
    nexttriangle[1] = c;
    nexttriangle[2] = 0;
    triangles.push_back(nexttriangle);

    // check Delaunay criterion for all triangles and sort
    // triangles accordingly (good / bad)
    int numt = (int)triangles.size();
    std::vector<bool> bad(numt);
    std::vector<double> close(numt);
    for (int t=0;t<numt;++t)
    {
      // initialize values indicating a close decision
      // these are needed to later introduce some tolerance into
      // the Delaunay criterion decision (which is needed in the
      // cases where this decision becomes non-unique / arbitrary).
      close[t]= 1.0e12;

      // indices of current triangle
      int idx0 = triangles[t][0];
      int idx1 = triangles[t][1];
      int idx2 = triangles[t][2];

      // coordinates of current triangle
      Epetra_SerialDenseMatrix coords(3,3);
      for (int k=0;k<3;++k)
      {
        coords(k,0) = clip[idx0].Coord()[k];
        coords(k,1) = clip[idx1].Coord()[k];
        coords(k,2) = clip[idx2].Coord()[k];
      }

      // define center of circumcircle of current triangle
      double x1 = coords(0,0); double y1 = coords(1,0); double z1 = coords(2,0);
      double x2 = coords(0,1); double y2 = coords(1,1); double z2 = coords(2,1);
      double x3 = coords(0,2); double y3 = coords(1,2); double z3 = coords(2,2);

      // a=vector P1P2, b=vector P2P3
      double a1 = x2-x1;
      double a2 = y2-y1;
      double a3 = z2-z1;
      double b1 = x3-x2;
      double b2 = y3-y2;
      double b3 = z3-z2;

      // normal vector of plane P1P2P3 via cross product
      double no1=a2*b3-b2*a3;
      double no2=a3*b1-b3*a1;
      double no3=a1*b2-b1*a2;

      // perpendicular bisector of P1P2 via cross product
      double c1=a2*no3-no2*a3;
      double c2=a3*no1-no3*a1;
      double c3=a1*no2-no1*a2;

      // perpendicular bisector of P2P3 via cross product
      double d1=b2*no3-no2*b3;
      double d2=b3*no1-no3*b1;
      double d3=b1*no2-no1*b2;

      // mid-points of P1P2 and P2P3
      double m1=(x1+x2)/2.0;
      double m2=(y1+y2)/2.0;
      double m3=(z1+z2)/2.0;
      double n1=(x2+x3)/2.0;
      double n2=(y2+y3)/2.0;
      double n3=(z2+z3)/2.0;

      // try to minimize error
      int direction = 0;
      if (abs(Auxn()[0])>=abs(Auxn()[1]) && abs(Auxn()[0])>=abs(Auxn()[2])) direction=1;
      if (abs(Auxn()[1])>=abs(Auxn()[0]) && abs(Auxn()[1])>=abs(Auxn()[2])) direction=2;
      if (abs(Auxn()[2])>=abs(Auxn()[0]) && abs(Auxn()[2])>=abs(Auxn()[1])) direction=3;
      if (direction==0) dserror("ERROR: Did not find best direction");

      // intersection of the two perpendicular bisections
      // (solution of m1+s*c1 = n1+t*d1 and m2+s*c2 = n2+t*d2)
      double s=0.0;
      if (direction==1)
      {
        // minimize error in yz-plane by solving
        // m2+s*c2 = n2+t*d2 and m3+s*c3 = n3+t*d3
        s=(m3*d2-n3*d2-d3*m2+d3*n2)/(c2*d3-c3*d2);
      }
      else if (direction==2)
      {
        // minimize error in xz-plane by solving
        // m1+s*c1 = n1+t*d1 and m3+s*c3 = n3+t*d3
        s=(m3*d1-n3*d1-d3*m1+d3*n1)/(c1*d3-c3*d1);
      }
      else /* (direction==3)*/
      {
        // minimize error in xy-plane by solving
        // m1+s*c1 = n1+t*d1 and m2+s*c2 = n2+t*d2
        s=(m2*d1-n2*d1-d2*m1+d2*n1)/(c1*d2-c2*d1);
      }

      // center of the circumcircle
      double xcenter = m1+s*c1;
      double ycenter = m2+s*c2;
      double zcenter = m3+s*c3;

      // radius of the circumcircle
      double radius1 = sqrt((xcenter-x1)*(xcenter-x1)+(ycenter-y1)*(ycenter-y1)+(zcenter-z1)*(zcenter-z1));
      double radius2 = sqrt((xcenter-x2)*(xcenter-x2)+(ycenter-y2)*(ycenter-y2)+(zcenter-z2)*(zcenter-z2));
      double radius3 = sqrt((xcenter-x3)*(xcenter-x3)+(ycenter-y3)*(ycenter-y3)+(zcenter-z3)*(zcenter-z3));

      // check radius computation
      if (abs(radius2-radius1) > tol || abs(radius3-radius1) > tol)
      {
        std::cout << "***WARNING*** Delaunay triangulation failed (no well-defined circumcircles)"
             << " -> using backup" << std::endl;

        // if Delaunay triangulation failed, use old center-based
        // triangulation as backup (therefore return false)
        return false;
      }

      // check Delaunay criterion for all other vertices
      // (of current polygon, NOT the full clipping polygon)
      for (int k=0;k<currsize;++k)
      {
        // no check needed for triangle vertices
        if (k==idx0 || k==idx1 || k==idx2) continue;

        // compute distance
        double dist = sqrt((xcenter-clip[k].Coord()[0])*(xcenter-clip[k].Coord()[0])
                          +(ycenter-clip[k].Coord()[1])*(ycenter-clip[k].Coord()[1])
                          +(zcenter-clip[k].Coord()[2])*(zcenter-clip[k].Coord()[2]));

        // monitor critical Delaunay criterion decision
        // (necessary to avoid inconsistent good/bad grouping later)
        double diff = abs(dist-radius1);
        if (diff < close[t]) close[t] = diff;

        // check for bad triangle (without tolerance)
        if (dist < radius1) bad[t]=true;
      }
    }

    // make good/bad decision consistent (with tolerance)
    // (problems might occur if more than 3 vertices on circumcircle)
    for (int t=0;t<numt;++t)
    {
      // check if this good decision was really close
      if (!bad[t] && close[t]<tol)
      {
        // check if any bad decision was really close, too
        bool foundpartner = false;
        for (int u=0;u<numt;++u)
        {
          if (bad[u] && close[u]<tol)
            foundpartner = true;
        }

        // set good->bad if partner found
        if (foundpartner) bad[t] = true;
      }
    }

    // now we build vector of all good / bad triangles
    std::vector<std::vector<int> > goodtriangles(0,std::vector<int>(3));
    std::vector<std::vector<int> > badtriangles(0,std::vector<int>(3));
    for (int t=0;t<numt;++t)
    {
      if (bad[t]) badtriangles.push_back(triangles[t]);
      else        goodtriangles.push_back(triangles[t]);
    }

    // find vertices in bad triangles: ALL vertices
    // find vertices in bad triangles: NOT connected with current vertex
    std::vector<int> badv(0);
    std::vector<int> ncv(0);
    for (int t=0;t<numt;++t)
    {
      if (bad[t])
      {
        // indices of current bad triangle
        int idx0 = triangles[t][0];
        int idx1 = triangles[t][1];
        int idx2 = triangles[t][2];

        // collect ALL vertices
        bool foundbefore0 = false;
        for(int k=0;k<(int)badv.size();++k)
        {
          if (badv[k]==idx0)
            foundbefore0 = true;
        }
        if (!foundbefore0) badv.push_back(idx0);

        bool foundbefore1 = false;
        for(int k=0;k<(int)badv.size();++k)
        {
          if (badv[k]==idx1)
            foundbefore1 = true;
        }
        if (!foundbefore1) badv.push_back(idx1);

        bool foundbefore2 = false;
        for(int k=0;k<(int)badv.size();++k)
        {
          if (badv[k]==idx2)
            foundbefore2 = true;
        }
        if (!foundbefore2) badv.push_back(idx2);

        // indices of current vertex neighbors
        int neighbor0 = c-1;
        int neighbor1 = 0;

        // collect NOT connected vertices
        if (idx0 != c && idx0 != neighbor0 && idx0 != neighbor1)
        {
          bool foundbefore = false;
          for(int k=0;k<(int)ncv.size();++k)
          {
            if (ncv[k]==idx0)
              foundbefore = true;
          }
          if (!foundbefore) ncv.push_back(idx0);
        }
        if (idx1 != c && idx1 != neighbor0 && idx1 != neighbor1)
        {
          bool foundbefore = false;
          for(int k=0;k<(int)ncv.size();++k)
          {
            if (ncv[k]==idx1)
              foundbefore = true;
          }
          if (!foundbefore) ncv.push_back(idx1);
        }
        if (idx2 != c && idx2 != neighbor0 && idx2 != neighbor1)
        {
          bool foundbefore = false;
          for(int k=0;k<(int)ncv.size();++k)
          {
            if (ncv[k]==idx2)
              foundbefore = true;
          }
          if (!foundbefore) ncv.push_back(idx2);
        }
      }
    }

    // build triangles formed by current vertex and ncv vertices
    std::vector<std::vector<int> > addtriangles(0,std::vector<int>(3));
    for (int k=0;k<(int)ncv.size();++k)
    {
      // find ncv vertex neighbor0
      bool validneighbor0 = false;
      int off0 = 0;
      int neighbor0 = 0;
      do
      {
        // set neighbor
        neighbor0 = ncv[k]-1-off0;
        if ((ncv[k]-off0)==0) neighbor0 = currsize-1-off0;

        // check if neighbor is in bad vertices
        for(int k=0;k<(int)badv.size();++k)
        {
          if (badv[k]==neighbor0)
            validneighbor0 = true;
        }

        // increase counter
        ++off0;

      } while (!validneighbor0);

      // find ncv vertex neighbor1
      bool validneighbor1 = false;
      int off1=0;
      int neighbor1=0;
      do
      {
        // set neighbor
        neighbor1 = ncv[k]+1+off1;
        if ((ncv[k]+off1)==currsize-1) neighbor1 = 0+off1;

        // check if neighbor is in bad vertices
        for(int k=0;k<(int)badv.size();++k)
        {
          if (badv[k]==neighbor1)
            validneighbor1 = true;
        }

        // increase counter
        ++off1;

      } while (!validneighbor1);

      // plausibility check
      if (neighbor0 == c || neighbor1 == c)
        dserror("ERROR: Connected nodes not possible here");

      // add triangles
      std::vector<int> add1(3);
      add1[0] = c; add1[1]=ncv[k]; add1[2]=neighbor0;
      addtriangles.push_back(add1);
      std::vector<int> add2(3);
      add2[0] = c; add2[1]=ncv[k]; add2[2]=neighbor1;
      addtriangles.push_back(add2);
    }

    // collapse addtriangles (remove double entries)
    int nadd = 0;
    for (int k=0;k<(int)addtriangles.size();++k)
    {
      bool addbefore = false;
      int idx0 = addtriangles[k][0];
      int idx1 = addtriangles[k][1];
      int idx2 = addtriangles[k][2];

      // check against all other goodtriangles
      for (int l=0;l<(int)goodtriangles.size();++l)
      {
        // do not check against itself
        int lidx0 = goodtriangles[l][0];
        int lidx1 = goodtriangles[l][1];
        int lidx2 = goodtriangles[l][2];

        if (idx0==lidx0 && idx1==lidx1 && idx2==lidx2) addbefore=true;
        if (idx0==lidx0 && idx1==lidx2 && idx2==lidx1) addbefore=true;
        if (idx0==lidx1 && idx1==lidx0 && idx2==lidx2) addbefore=true;
        if (idx0==lidx1 && idx1==lidx2 && idx2==lidx0) addbefore=true;
        if (idx0==lidx2 && idx1==lidx0 && idx2==lidx1) addbefore=true;
        if (idx0==lidx2 && idx1==lidx1 && idx2==lidx0) addbefore=true;
      }

      // add to good triangles
      if (!addbefore)
      {
        nadd++;
        goodtriangles.push_back(addtriangles[k]);
      }
    }

    // store final triangulation
    triangles.resize(0);
    for (int k=0;k<(int)goodtriangles.size();++k)
      triangles.push_back(goodtriangles[k]);
  }

  // create intcells for all triangle
  int numt = (int)triangles.size();
  for (int t=0;t<numt;++t)
  {
    // indices of current triangle
    int idx0 = triangles[t][0];
    int idx1 = triangles[t][1];
    int idx2 = triangles[t][2];

    // coordinates of current triangle
    Epetra_SerialDenseMatrix coords(3,3);
    for (int k=0;k<3;++k)
    {
      coords(k,0) = clip[idx0].Coord()[k];
      coords(k,1) = clip[idx1].Coord()[k];
      coords(k,2) = clip[idx2].Coord()[k];
    }

    // create IntCell object and push back
    cells.push_back(Teuchos::rcp(new MORTAR::IntCell(t,3,coords,Auxn(),DRT::Element::tri3,
      linvertex[idx0],linvertex[idx1],linvertex[idx2],derivauxn)));
  }

  // double check number of triangles
  if (numt != clipsize-2)
  {
    std::cout << "***WARNING*** Delaunay triangulation failed (" << clipsize << " vertices, "
         << numt << " triangles)" << " -> using backup" << std::endl;

    // if Delaunay triangulation failed, use old center-based
    // triangulation as backup (therefore return false)
    return false;
  }

  // triangulation successful
  return true;
}
