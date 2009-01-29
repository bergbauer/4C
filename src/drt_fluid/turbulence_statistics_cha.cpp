/*!----------------------------------------------------------------------
\file turbulence_statistics_cha.cpp

\brief calculate mean values and fluctuations for turbulent channel
flows.


*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include "turbulence_statistics_cha.H"

/*----------------------------------------------------------------------

                  Standard Constructor (public)

  ---------------------------------------------------------------------*/
FLD::TurbulenceStatisticsCha::TurbulenceStatisticsCha(
  RefCountPtr<DRT::Discretization> actdis             ,
  bool                             alefluid           ,
  RefCountPtr<Epetra_Vector>       dispnp             ,
  ParameterList&                   params             ,
  bool                             smagorinsky        ,
  bool                             subgrid_dissipation
  )
  :
  discret_            (actdis             ),
  alefluid_           (alefluid           ),
  dispnp_             (dispnp             ),
  params_             (params             ),
  smagorinsky_        (smagorinsky        ),
  subgrid_dissipation_(subgrid_dissipation)
{
  //----------------------------------------------------------------------
  // plausibility check
  int numdim = params_.get<int>("number of velocity degrees of freedom");
  if (numdim!=3)
  {
    dserror("Evaluation of turbulence statistics only for 3d channel flow!");
  }

  //----------------------------------------------------------------------
  // switches, control parameters, material parameters

  // type of solver: low-Mach-number or incompressible solver
  loma_ = params_.get<string>("low-Mach-number solver","No");

  // get the plane normal direction from the parameterlist
  {
    string planestring = params_.sublist("TURBULENCE MODEL").get<string>("HOMDIR","not_specified");

    if(planestring == "xz")
    {
      dim_ = 1;
    }
    else if(planestring == "yz")
    {
      dim_ = 0;
    }
    else if(planestring == "xy")
    {
      dim_ = 2;
    }
    else
    {
      dserror("homogeneuous plane for channel flow was specified incorrectly.");
    }
  }

  // get fluid viscosity from material definition --- for computation
  // of ltau
  visc_ = mat->m.fluid->viscosity;


  // ---------------------------------------------------------------------
  // up to now, there are no records written
  countrecord_ = 0;

  //----------------------------------------------------------------------
  // allocate some (toggle) vectors
  const Epetra_Map* dofrowmap = discret_->DofRowMap();

  meanvelnp_    = LINALG::CreateVector(*dofrowmap,true);
  // this vector is only necessary for low-Mach-number flow
  if (loma_ != "No") meanvedenp_ = LINALG::CreateVector(*dofrowmap,true);

  toggleu_      = LINALG::CreateVector(*dofrowmap,true);
  togglev_      = LINALG::CreateVector(*dofrowmap,true);
  togglew_      = LINALG::CreateVector(*dofrowmap,true);
  togglep_      = LINALG::CreateVector(*dofrowmap,true);

  // ---------------------------------------------------------------------
  // compute all planes for sampling

  // available planes of element nodes (polynomial)/corners
  // (Nurbs) of elements
  nodeplanes_ = rcp(new vector<double> );


  // available homogeneous (sampling) planes --- there are
  // numsubdivisions layers per element layer between two
  // nodes (Polynomial)/per element layer (Nurbs)
  planecoordinates_ = rcp(new vector<double> );

  const int numsubdivisions=5;

  // try to cast discretisation to nurbs variant
  // this tells you what kind of computation of
  // samples is required
  DRT::NURBS::NurbsDiscretization* nurbsdis
    =
    dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(*actdis));

  // allocate array for bounding box
  //
  //          |  x  |  y  |  z
  //    ------+-----+-----+-----
  //      min |     |     |
  //    ------+-----+-----+-----
  //      max |     |     |
  //
  //
  boundingbox_ = rcp(new Epetra_SerialDenseMatrix(2,3));
  for (int row = 0;row<3;++row)
  {
    (*boundingbox_)(0,row) = +10e+19;
    (*boundingbox_)(1,row) = -10e+19;
  }

  if(nurbsdis == NULL)
  {
    // create set of available homogeneous planes. The normal direction
    // is read from the parameter list
    planecoordinates_ = rcp(new vector<double> );

    // the criterion allows differences in coordinates by 1e-9
    set<double,PlaneSortCriterion> availablecoords;

    // loop nodes, build set of planes accessible on this proc and
    // calculate bounding box
    for (int i=0; i<discret_->NumMyRowNodes(); ++i)
    {
      DRT::Node* node = discret_->lRowNode(i);
      availablecoords.insert(node->X()[dim_]);

      for (int row = 0;row<3;++row)
      {
	if ((*boundingbox_)(0,row)>node->X()[row])
	{
	  (*boundingbox_)(0,row)=node->X()[row];
	}
	if ((*boundingbox_)(1,row)<node->X()[row])
	{
	  (*boundingbox_)(1,row)=node->X()[row];
	}
      }
    }

    // communicate mins
    for (int row = 0;row<3;++row)
    {
      double min;

      discret_->Comm().MinAll(&((*boundingbox_)(0,row)),&min,1);
      (*boundingbox_)(0,row)=min;
    }

    // communicate maxs
    for (int row = 0;row<3;++row)
    {
      double max;

      discret_->Comm().MaxAll(&((*boundingbox_)(1,row)),&max,1);
      (*boundingbox_)(1,row)=max;
    }

    //--------------------------------------------------------------------
    // round robin loop to communicate coordinates to all procs

    {
#ifdef PARALLEL
      int myrank  =discret_->Comm().MyPID();
#endif
      int numprocs=discret_->Comm().NumProc();

      vector<char> sblock;
      vector<char> rblock;


#ifdef PARALLEL
      // create an exporter for point to point comunication
      DRT::Exporter exporter(discret_->Comm());
#endif

      for (int np=0;np<numprocs;++np)
      {
	// export set to sendbuffer
	sblock.clear();

	for (set<double,PlaneSortCriterion>::iterator plane=availablecoords.begin();
	     plane!=availablecoords.end();
	     ++plane)
	{
	  DRT::ParObject::AddtoPack(sblock,*plane);
	}
#ifdef PARALLEL
	MPI_Request request;
	int         tag    =myrank;

	int         frompid=myrank;
	int         topid  =(myrank+1)%numprocs;

	int         length=sblock.size();

	exporter.ISend(frompid,topid,
		       &(sblock[0]),sblock.size(),
		       tag,request);

	rblock.clear();

	// receive from predecessor
	frompid=(myrank+numprocs-1)%numprocs;
	exporter.ReceiveAny(frompid,tag,rblock,length);

	if(tag!=(myrank+numprocs-1)%numprocs)
	{
	  dserror("received wrong message (ReceiveAny)");
	}

	exporter.Wait(request);

	{
	  // for safety
	  exporter.Comm().Barrier();
	}
#else
	// dummy communication
	rblock.clear();
	rblock=sblock;
#endif

	// Unpack received block into set of all planes.
	{
	  vector<double> coordsvec;

	  coordsvec.clear();

	  int index = 0;
	  while (index < (int)rblock.size())
	  {
	    double onecoord;
	    DRT::ParObject::ExtractfromPack(index,rblock,onecoord);
	    availablecoords.insert(onecoord);
	  }
	}
      }
    }

    //----------------------------------------------------------------------
    // push coordinates of planes in a vector

    {
      nodeplanes_ = rcp(new vector<double> );


      for(set<double,PlaneSortCriterion>::iterator coord=availablecoords.begin();
	  coord!=availablecoords.end();
	  ++coord)
      {
	nodeplanes_->push_back(*coord);
      }

      // insert additional sampling planes (to show influence of quadratic
      // shape functions)

      for(unsigned rr =0; rr < nodeplanes_->size()-1; ++rr)
      {
	double delta = ((*nodeplanes_)[rr+1]-(*nodeplanes_)[rr])/((double) numsubdivisions);

	for (int mm =0; mm < numsubdivisions; ++mm)
	{
	  planecoordinates_->push_back((*nodeplanes_)[rr]+delta*mm);
	}
      }
      planecoordinates_->push_back((*nodeplanes_)[(*nodeplanes_).size()-1]);
    }
  }
  else
  {

    // pointwise sampling does not make any sense for Nurbs
    // discretisations since shape functions are not interpolating


    // planecoordinates are determined by the element (cartesian) number
    // in y direction and the number of sampling planes in between
    // and nodeplanes are kept as the corners of elements
    // (to be able to visualise stuff on the element center later on)

    // for nurbs discretisations, all vector sizes are already determined
    // by the knotvector size
    if(dim_!=1)
    {
      dserror("For the nurbs stuff, we require that xz is the hom. plane\n");
    }

    // get nurbs dis' knotvector sizes
    vector<int> n_x_m_x_l(nurbsdis->Return_n_x_m_x_l(0));

    // get nurbs dis' element numbers
    vector<int> nele_x_mele_x_lele(nurbsdis->Return_nele_x_mele_x_lele(0));

    // get the knotvector itself
    RefCountPtr<DRT::NURBS::Knotvector> knots=nurbsdis->GetKnotVector();

    // resize and initialise to 0
    {
      (*nodeplanes_      ).resize(nele_x_mele_x_lele[1]+1);
      (*planecoordinates_).resize(nele_x_mele_x_lele[1]*(numsubdivisions-1)+1);

      vector<double>::iterator coord;

      for (coord  = (*nodeplanes_).begin();
	   coord != (*nodeplanes_).end()  ;
	   ++coord)
      {
	*coord=0;
      }
      for (coord  = planecoordinates_->begin();
	   coord != planecoordinates_->end()  ;
	   ++coord)
      {
	*coord=0;
      }
    }

    // get element map
    const Epetra_Map* elementmap = nurbsdis->ElementRowMap();

    // loop all available elements
    for (int iele=0; iele<elementmap->NumMyElements(); ++iele)
    {
      DRT::Element* const actele = nurbsdis->gElement(elementmap->GID(iele));
      DRT::Node**   nodes = actele->Nodes();

      // get gid, location in the patch
      int gid = actele->Id();

      int patchid=0;

      vector<int> ele_cart_id;
      knots->ConvertEleGidToKnotIds(patchid,gid,ele_cart_id);

      // want to loop all control points of the element,
      // so get the number of points
      const int numnp = actele->NumNode();

	// access elements knot span
      std::vector<blitz::Array<double,1> > knots(3);
      (*((*nurbsdis).GetKnotVector())).GetEleKnots(knots,actele->Id());

      // aquire weights from nodes
      blitz::Array<double,1> weights(numnp);

      for (int inode=0; inode<numnp; ++inode)
      {
	DRT::NURBS::ControlPoint* cp
	  =
	  dynamic_cast<DRT::NURBS::ControlPoint* > (nodes[inode]);

	weights(inode) = cp->W();
      }

      // get shapefunctions, compute all visualisation point positions
      blitz::Array<double,1> nurbs_shape_funct(numnp);

      switch (actele->Shape())
      {
      case DRT::Element::nurbs8:
      case DRT::Element::nurbs27:
      {
	// element local point position
	blitz::Array<double,1> uv(3);

	{
	  // standard

	  //               v
	  //              /
          //  w  7       /   8
	  //  ^   +---------+
	  //  |  /         /|
	  //  | /         / |
	  // 5|/        6/  |
	  //  +---------+   |
	  //  |         |   |
	  //  |         |   +
	  //  |         |  / 4
	  //  |         | /
	  //  |         |/
	  //  +---------+ ----->u
	  // 1           2
	  // use v-coordinate of point 1 and 8
	  // temporary x vector
	  std::vector<double> x(3);

	  // point 1
	  uv(0)= -1.0;
	  uv(1)= -1.0;
	  uv(2)= -1.0;
	  DRT::NURBS::UTILS::nurbs_get_3D_funct(nurbs_shape_funct,
						uv               ,
						knots            ,
						weights          ,
						actele->Shape()  );
	  for (int isd=0; isd<3; ++isd)
	  {
	    double val = 0;
	    for (int inode=0; inode<numnp; ++inode)
	    {
	      val+=(((nodes[inode])->X())[isd])*nurbs_shape_funct(inode);
	    }
	    x[isd]=val;
	  }

	  (*nodeplanes_      )[ele_cart_id[1]]                    +=x[1];
	  (*planecoordinates_)[ele_cart_id[1]*(numsubdivisions-1)]+=x[1];

	  for (int isd=0; isd<3; ++isd)
	  {
	    if ((*boundingbox_)(0,isd)>x[isd])
	    {
	      (*boundingbox_)(0,isd)=x[isd];
	    }
	    if ((*boundingbox_)(1,isd)<x[isd])
	    {
	      (*boundingbox_)(1,isd)=x[isd];
	    }
	  }

	  for(int rr=1;rr<numsubdivisions-1;++rr)
	  {
	    uv(1) += 2.0/(numsubdivisions-1);

	    DRT::NURBS::UTILS::nurbs_get_3D_funct(nurbs_shape_funct,
						  uv               ,
						  knots            ,
						  weights          ,
						  actele->Shape()  );
	    for (int isd=0; isd<3; ++isd)
	    {
	      double val = 0;
	      for (int inode=0; inode<numnp; ++inode)
	      {
		val+=(((nodes[inode])->X())[isd])*nurbs_shape_funct(inode);
	      }
	      x[isd]=val;
	    }
	    (*planecoordinates_)[ele_cart_id[1]*(numsubdivisions-1)+rr]+=x[1];
	  }


	  // set upper point of element, too (only for last layer)
	  if(ele_cart_id[1]+1 == nele_x_mele_x_lele[1])
	  {
	    // point 8
	    uv(0)=  1.0;
	    uv(1)=  1.0;
	    uv(2)=  1.0;
	    DRT::NURBS::UTILS::nurbs_get_3D_funct(nurbs_shape_funct,
						  uv               ,
						  knots            ,
						  weights          ,
						  actele->Shape()  );
	    for (int isd=0; isd<3; ++isd)
	    {
	      double val = 0;
	      for (int inode=0; inode<numnp; ++inode)
	      {
		val+=(((nodes[inode])->X())[isd])*nurbs_shape_funct(inode);
	      }
	      x[isd]=val;
	    }

	    (*nodeplanes_)      [ele_cart_id[1]                   +1]+=x[1];
	    (*planecoordinates_)[(ele_cart_id[1]+1)*(numsubdivisions-1)]+=x[1];

            for (int isd=0; isd<3; ++isd)
            {
              if ((*boundingbox_)(0,isd)>x[isd])
              {
                (*boundingbox_)(0,isd)=x[isd];
              }
              if ((*boundingbox_)(1,isd)<x[isd])
              {
                (*boundingbox_)(1,isd)=x[isd];
              }
            }
	  }

	}
	break;
      }
      default:
	dserror("Unknown element shape for a nurbs element or nurbs type not valid for turbulence calculation\n");
      }
    }

    //----------------------------------------------------------------------
    // add contributions from all processors, normalize

    vector<double> lnodeplanes      (*nodeplanes_      );
    vector<double> lplanecoordinates(*planecoordinates_);

    discret_->Comm().SumAll(&(lnodeplanes[0]      ),&((*nodeplanes_      )[0]),nodeplanes_->size()      );
    discret_->Comm().SumAll(&(lplanecoordinates[0]),&((*planecoordinates_)[0]),planecoordinates_->size());

    {
      (*nodeplanes_      ).resize(nele_x_mele_x_lele[1]+1);
      (*planecoordinates_).resize(nele_x_mele_x_lele[1]*(numsubdivisions-1)+1);

      vector<double>::iterator coord;

      int nelelayer = (nele_x_mele_x_lele[0])*(nele_x_mele_x_lele[2]);

      for (coord  = (*nodeplanes_).begin();
	   coord != (*nodeplanes_).end()  ;
	   ++coord)
      {
	*coord/=(double)(nelelayer);
      }
      for (coord  = planecoordinates_->begin();
	   coord != planecoordinates_->end()  ;
	   ++coord)
      {
	*coord/=(double)(nelelayer);
      }
    }

    // communicate mins
    for (int row = 0;row<3;++row)
    {
      double min;

      discret_->Comm().MinAll(&((*boundingbox_)(0,row)),&min,1);
      (*boundingbox_)(0,row)=min;
    }

    // communicate maxs
    for (int row = 0;row<3;++row)
    {
      double max;

      discret_->Comm().MaxAll(&((*boundingbox_)(1,row)),&max,1);
      (*boundingbox_)(1,row)=max;
    }

  }

  //----------------------------------------------------------------------
  // allocate arrays for sums of in plane mean values

  int size = planecoordinates_->size();

  // arrays for integration based averaging
  // --------------------------------------

  // first order moments
  sumu_ =  rcp(new vector<double> );
  sumu_->resize(size,0.0);

  sumv_ =  rcp(new vector<double> );
  sumv_->resize(size,0.0);

  sumw_ =  rcp(new vector<double> );
  sumw_->resize(size,0.0);

  sump_ =  rcp(new vector<double> );
  sump_->resize(size,0.0);

  // now the second order moments
  sumsqu_ =  rcp(new vector<double> );
  sumsqu_->resize(size,0.0);

  sumsqv_ =  rcp(new vector<double> );
  sumsqv_->resize(size,0.0);

  sumsqw_ =  rcp(new vector<double> );
  sumsqw_->resize(size,0.0);

  sumuv_ =  rcp(new vector<double> );
  sumuv_->resize(size,0.0);

  sumuw_ =  rcp(new vector<double> );
  sumuw_->resize(size,0.0);

  sumvw_ =  rcp(new vector<double> );
  sumvw_->resize(size,0.0);

  sumsqp_ =  rcp(new vector<double> );
  sumsqp_->resize(size,0.0);

  // arrays for point based averaging
  // --------------------------------

  pointsquaredvelnp_  = LINALG::CreateVector(*dofrowmap,true);

  // this vector is only necessary for low-Mach-number flow
  if (loma_ != "No") pointsquaredvedenp_  = LINALG::CreateVector(*dofrowmap,true);

  // first order moments
  pointsumu_ =  rcp(new vector<double> );
  pointsumu_->resize(size,0.0);

  pointsumv_ =  rcp(new vector<double> );
  pointsumv_->resize(size,0.0);

  pointsumw_ =  rcp(new vector<double> );
  pointsumw_->resize(size,0.0);

  pointsump_ =  rcp(new vector<double> );
  pointsump_->resize(size,0.0);

  pointsumT_ =  rcp(new vector<double> );
  pointsumT_->resize(size,0.0);

  // now the second order moments
  pointsumsqu_ =  rcp(new vector<double> );
  pointsumsqu_->resize(size,0.0);

  pointsumsqv_ =  rcp(new vector<double> );
  pointsumsqv_->resize(size,0.0);

  pointsumsqw_ =  rcp(new vector<double> );
  pointsumsqw_->resize(size,0.0);

  pointsumsqp_ =  rcp(new vector<double> );
  pointsumsqp_->resize(size,0.0);

  pointsumsqT_ =  rcp(new vector<double> );
  pointsumsqT_->resize(size,0.0);

  //----------------------------------------------------------------------
  // arrays for averaging of Smagorinsky constant etc.
  //

  // check if we want to compute averages of Smagorinsky
  // constants, effective viscosities etc
  if (smagorinsky_)
  {
    // extended statistics (plane average of Cs, (Cs_delta)^2, visceff)
    // for dynamic Smagorinsky model
    
    // vectors for element -> statistics communication
    // -----------------------------------------------

    // vectors containing processor local values to sum element 
    // contributions on this proc
    RefCountPtr<vector<double> > local_Cs_sum          =  rcp(new vector<double> (nodeplanes_->size()-1,0.0));
    RefCountPtr<vector<double> > local_Cs_delta_sq_sum =  rcp(new vector<double> (nodeplanes_->size()-1,0.0));
    RefCountPtr<vector<double> > local_visceff_sum     =  rcp(new vector<double> (nodeplanes_->size()-1,0.0));
          
    // store them in parameterlist for access on the element
    ParameterList *  modelparams =&(params_.sublist("TURBULENCE MODEL"));


    modelparams->set<RefCountPtr<vector<double> > >("planecoords_"         ,nodeplanes_          );
    modelparams->set<RefCountPtr<vector<double> > >("local_Cs_sum"         ,local_Cs_sum         );
    modelparams->set<RefCountPtr<vector<double> > >("local_Cs_delta_sq_sum",local_Cs_delta_sq_sum);
    modelparams->set<RefCountPtr<vector<double> > >("local_visceff_sum"    ,local_visceff_sum    );

    // vectors for statistics computation (sums and increments)
    // ----------------------------------
      
    // means for the Smagorinsky constant
    sumCs_  =  rcp(new vector<double> );
    sumCs_->resize(nodeplanes_->size()-1,0.0);
      
    incrsumCs_  =  rcp(new vector<double> );
    incrsumCs_->resize(nodeplanes_->size()-1,0.0);
      
    // means for (Cs*delta)^2
    sumCs_delta_sq_  =  rcp(new vector<double> );
    sumCs_delta_sq_->resize(nodeplanes_->size()-1,0.0);
      
    incrsumCs_delta_sq_  =  rcp(new vector<double> );
    incrsumCs_delta_sq_->resize(nodeplanes_->size()-1,0.0);
        
    // means for the effective viscosity
    sumvisceff_  =  rcp(new vector<double> );
    sumvisceff_->resize(nodeplanes_->size()-1,0.0);
      
    incrsumvisceff_  =  rcp(new vector<double> );
    incrsumvisceff_->resize(nodeplanes_->size()-1,0.0);
  }

  //----------------------------------------------------------------------
  // arrays for averaging of residual, subscales etc.

  // prepare time averaging for subscales and residual 
  if(subgrid_dissipation_)
  {
    //--------------------------------------------------
    // local_incrtauC            (in plane) averaged values of stabilisation parameter tauC
    // local_incrtauM            (in plane) averaged values of stabilisation parameter tauM
    // local_incrres(_sq)        (in plane) averaged values of resM (^2)
    // local_incrsacc(_sq)       (in plane) averaged values of sacc (^2)
    // local_incrsvelaf(_sq)     (in plane) averaged values of svelaf (^2)
    // local_incrresC(_sq)       (in plane) averaged values of resC (^2)
    // local_incrspressnp(_sq)   (in plane) averaged values of spressnp (^2)
    //--------------------------------------------------
    RefCountPtr<vector<double> > local_incrvol           = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));

    RefCountPtr<vector<double> > local_incrtauC          = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incrtauM          = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    
    RefCountPtr<vector<double> > local_incrres           = rcp(new vector<double> (3*(nodeplanes_->size()-1),0.0));
    RefCountPtr<vector<double> > local_incrres_sq        = rcp(new vector<double> (3*(nodeplanes_->size()-1),0.0));
    RefCountPtr<vector<double> > local_incrsacc          = rcp(new vector<double> (3*(nodeplanes_->size()-1),0.0));
    RefCountPtr<vector<double> > local_incrsacc_sq       = rcp(new vector<double> (3*(nodeplanes_->size()-1),0.0));
    RefCountPtr<vector<double> > local_incrsvelaf        = rcp(new vector<double> (3*(nodeplanes_->size()-1),0.0));
    RefCountPtr<vector<double> > local_incrsvelaf_sq     = rcp(new vector<double> (3*(nodeplanes_->size()-1),0.0));
    RefCountPtr<vector<double> > local_incrresC          = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incrresC_sq       = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incrspressnp      = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incrspressnp_sq   = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));


    RefCountPtr<vector<double> > local_incr_eps_sacc     = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_pspg     = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_supg     = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_cross    = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_rey      = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_cstab    = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_vstab    = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_eddyvisc = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_visc     = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));
    RefCountPtr<vector<double> > local_incr_eps_conv     = rcp(new vector<double> ((nodeplanes_->size()-1)  ,0.0));

    // pass pointers to local sum vectors to the element
    eleparams_.set<RefCountPtr<vector<double> > >("incrvol"          ,local_incrvol         );

    eleparams_.set<RefCountPtr<vector<double> > >("planecoords_"     ,nodeplanes_           );
    eleparams_.set<RefCountPtr<vector<double> > >("incrtauC"         ,local_incrtauC        );
    eleparams_.set<RefCountPtr<vector<double> > >("incrtauM"         ,local_incrtauM        );
    eleparams_.set<RefCountPtr<vector<double> > >("incrres"          ,local_incrres         );
    eleparams_.set<RefCountPtr<vector<double> > >("incrres_sq"       ,local_incrres_sq      );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsacc"         ,local_incrsacc        );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsacc_sq"      ,local_incrsacc_sq     );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsvelaf"       ,local_incrsvelaf      );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsvelaf_sq"    ,local_incrsvelaf_sq   );
    eleparams_.set<RefCountPtr<vector<double> > >("incrresC"         ,local_incrresC        );
    eleparams_.set<RefCountPtr<vector<double> > >("incrresC_sq"      ,local_incrresC_sq     );
    eleparams_.set<RefCountPtr<vector<double> > >("incrspressnp"     ,local_incrspressnp    );
    eleparams_.set<RefCountPtr<vector<double> > >("incrspressnp_sq"  ,local_incrspressnp_sq );

    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_sacc"    ,local_incr_eps_sacc    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_pspg"    ,local_incr_eps_pspg    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_supg"    ,local_incr_eps_supg    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_cross"   ,local_incr_eps_cross   );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_rey"     ,local_incr_eps_rey     );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_cstab"   ,local_incr_eps_cstab   );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_vstab"   ,local_incr_eps_vstab   );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_eddyvisc",local_incr_eps_eddyvisc);
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_visc"    ,local_incr_eps_visc    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_conv"    ,local_incr_eps_conv    );

    // means for comparison of of residual and subscale acceleration

    sumres_    =  rcp(new vector<double> );
    sumres_->resize(3*(nodeplanes_->size()-1),0.0);
    sumres_sq_ =  rcp(new vector<double> );
    sumres_sq_->resize(3*(nodeplanes_->size()-1),0.0);
    
    sumsacc_   =  rcp(new vector<double> );
    sumsacc_->resize(3*(nodeplanes_->size()-1),0.0);
    sumsacc_sq_=  rcp(new vector<double> );
    sumsacc_sq_->resize(3*(nodeplanes_->size()-1),0.0);
    
    sumsvelaf_=  rcp(new vector<double> );
    sumsvelaf_->resize(3*(nodeplanes_->size()-1),0.0);
    sumsvelaf_sq_=  rcp(new vector<double> );
    sumsvelaf_sq_->resize(3*(nodeplanes_->size()-1),0.0);
    
    sumresC_       =  rcp(new vector<double> );
    sumresC_->resize(nodeplanes_->size()-1,0.0);
    sumresC_sq_    =  rcp(new vector<double> );
    sumresC_sq_->resize(nodeplanes_->size()-1,0.0);

    sumspressnp_   =  rcp(new vector<double> );
    sumspressnp_->resize(nodeplanes_->size()-1,0.0);
    sumspressnp_sq_=  rcp(new vector<double> );
    sumspressnp_sq_->resize(nodeplanes_->size()-1,0.0);
    

    sumtauM_=  rcp(new vector<double> );
    sumtauM_->resize(nodeplanes_->size()-1,0.0);
    sumtauC_=  rcp(new vector<double> );
    sumtauC_->resize(nodeplanes_->size()-1,0.0);

    sum_eps_sacc_   =  rcp(new vector<double> );
    sum_eps_sacc_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_pspg_=  rcp(new vector<double> );
    sum_eps_pspg_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_supg_=  rcp(new vector<double> );
    sum_eps_supg_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_cross_=  rcp(new vector<double> );
    sum_eps_cross_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_rey_=  rcp(new vector<double> );
    sum_eps_rey_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_cstab_=  rcp(new vector<double> );
    sum_eps_cstab_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_vstab_=  rcp(new vector<double> );
    sum_eps_vstab_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_eddyvisc_=  rcp(new vector<double> );
    sum_eps_eddyvisc_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_visc_=  rcp(new vector<double> );
    sum_eps_visc_->resize(nodeplanes_->size()-1,0.0);
    sum_eps_conv_=  rcp(new vector<double> );
    sum_eps_conv_->resize(nodeplanes_->size()-1,0.0);
 
  }



  //----------------------------------------------------------------------
  // initialise output

  Teuchos::RefCountPtr<std::ofstream> log;
  Teuchos::RefCountPtr<std::ofstream> log_Cs;
  Teuchos::RefCountPtr<std::ofstream> log_res;

  if (discret_->Comm().MyPID()==0)
  {
    std::string s = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
    s.append(".flow_statistic");

    log = Teuchos::rcp(new std::ofstream(s.c_str(),ios::out));
    (*log) << "# Flow statistics for turbulent channel flow (first- and second-order moments)\n\n";

    log->flush();

    // additional output for dynamic Smagorinsky model
    if(smagorinsky_)
    {
      std::string s_smag = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
      s_smag.append(".Cs_statistic");
      
      log_Cs = Teuchos::rcp(new std::ofstream(s_smag.c_str(),ios::out));
      (*log_Cs) << "# Statistics for turbulent channel flow (Smagorinsky constant)\n\n";
    }

    // output of residuals and subscale quantities
    std::string s_res = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
    s_res.append(".res_statistic");

    log_res = Teuchos::rcp(new std::ofstream(s_res.c_str(),ios::out));
    (*log_res) << "# Statistics for turbulent channel flow (residuals and subscale quantities)\n";
    (*log_res) << "# All values are first averaged over the integration points in an element \n";
    (*log_res) << "# and after that averaged over a whole element layer in the homogeneous plane\n\n";
  }

  // clear statistics
  this->ClearStatistics();

  return;
}// TurbulenceStatisticsCha::TurbulenceStatisticsCha

/*----------------------------------------------------------------------*

                           Destructor

 -----------------------------------------------------------------------*/
FLD::TurbulenceStatisticsCha::~TurbulenceStatisticsCha()
{
  return;
}// TurbulenceStatisticsCha::~TurbulenceStatisticsCha()

/*----------------------------------------------------------------------*
 
       Compute the in-plane mean values of first and second order
       moments for velocities, pressure and Cs are added to global
                            'sum' vectors.

 -----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::DoTimeSample(
  Teuchos::RefCountPtr<Epetra_Vector> velnp,
  Epetra_Vector & force
  )
{
  // we have an additional sample
  numsamp_++;

  // meanvelnp is a refcount copy of velnp

  meanvelnp_->Update(1.0,*velnp,0.0);

  //----------------------------------------------------------------------
  // loop planes and calculate integral means in each plane

  this->EvaluateIntegralMeanValuesInPlanes();

  //----------------------------------------------------------------------
  // loop planes and calculate pointwise means in each plane

  // try to cast discretisation to nurbs variant
  // this tells you whether pointwise computation of
  // samples is allowed
  DRT::NURBS::NurbsDiscretization* nurbsdis
    =
    dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(*discret_));

  if(nurbsdis == NULL)
  {
    this->EvaluatePointwiseMeanValuesInPlanes();
  }

  //----------------------------------------------------------------------
  // compute forces on top and bottom plate for normalization purposes

  for(vector<double>::iterator plane=planecoordinates_->begin();
      plane!=planecoordinates_->end();
      ++plane)
  {
    // only true for top and bottom plane
    if ((*plane-2e-9 < (*planecoordinates_)[0]
         &&
         *plane+2e-9 > (*planecoordinates_)[0])
        ||
        (*plane-2e-9 < (*planecoordinates_)[planecoordinates_->size()-1]
         &&
         *plane+2e-9 > (*planecoordinates_)[planecoordinates_->size()-1])
      )
    {
      // toggle vectors are one in the position of a dof in this plane,
      // else 0
      toggleu_->PutScalar(0.0);
      togglev_->PutScalar(0.0);
      togglew_->PutScalar(0.0);

      // activate toggles for in plane dofs
      for (int nn=0; nn<discret_->NumMyRowNodes(); ++nn)
      {
        DRT::Node* node = discret_->lRowNode(nn);

        // this node belongs to the plane under consideration
        if (node->X()[dim_]<*plane+2e-9 && node->X()[dim_]>*plane-2e-9)
        {
          vector<int> dof = discret_->Dof(node);
          double      one = 1.0;

          toggleu_->ReplaceGlobalValues(1,&one,&(dof[0]));
          togglev_->ReplaceGlobalValues(1,&one,&(dof[1]));
          togglew_->ReplaceGlobalValues(1,&one,&(dof[2]));
        }
      }

      // compute forces by dot product
      double inc=0.0;
      force.Dot(*toggleu_,&inc);
      sumforceu_+=inc;
      inc=0.0;
      force.Dot(*togglev_,&inc);
      sumforcev_+=inc;
      inc=0.0;
      force.Dot(*togglew_,&inc);
      sumforcew_+=inc;
    }
  }

  //----------------------------------------------------------------------
  // add increment of last iteration to the sum of Cs values
  // (statistics for dynamic Smagorinsky model)

  if (smagorinsky_)
  {
    for (unsigned rr=0;rr<(*incrsumCs_).size();++rr)
    {
      (*sumCs_         )[rr]+=(*incrsumCs_         )[rr];
      (*sumCs_delta_sq_)[rr]+=(*incrsumCs_delta_sq_)[rr];
      (*sumvisceff_    )[rr]+=(*incrsumvisceff_    )[rr];
    }
  }

  return;
}// TurbulenceStatisticsCha::DoTimeSample

/*----------------------------------------------------------------------*
 
                 pointwise means for low machnumber flow

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::DoLomaTimeSample(
  Teuchos::RefCountPtr<Epetra_Vector> velnp,
  Teuchos::RefCountPtr<Epetra_Vector> vedenp,
  Epetra_Vector & force
  )
{
  //----------------------------------------------------------------------
  // we have an additional sample

  numsamp_++;

  //----------------------------------------------------------------------
  // meanvelnp and meanvedenp are refcount copies of velnp and vedenp

  meanvelnp_->Update(1.0,*velnp,0.0);
  meanvedenp_->Update(1.0,*vedenp,0.0);

  //----------------------------------------------------------------------
  // loop planes and calculate pointwise means in each plane

  this->EvaluateLomaPointwiseMeanValuesInPlanes();

  //----------------------------------------------------------------------
  // compute forces on top and bottom plate for normalization purposes

  for(vector<double>::iterator plane=planecoordinates_->begin();
      plane!=planecoordinates_->end();
      ++plane)
  {
    // only true for top and bottom plane
    if ((*plane-2e-9 < (*planecoordinates_)[0]
         &&
         *plane+2e-9 > (*planecoordinates_)[0])
        ||
        (*plane-2e-9 < (*planecoordinates_)[planecoordinates_->size()-1]
         &&
         *plane+2e-9 > (*planecoordinates_)[planecoordinates_->size()-1])
      )
    {
      // toggle vectors are one in the position of a dof in this plane,
      // else 0
      toggleu_->PutScalar(0.0);
      togglev_->PutScalar(0.0);
      togglew_->PutScalar(0.0);

      // activate toggles for in plane dofs
      for (int nn=0; nn<discret_->NumMyRowNodes(); ++nn)
      {
        DRT::Node* node = discret_->lRowNode(nn);

        // this node belongs to the plane under consideration
        if (node->X()[dim_]<*plane+2e-9 && node->X()[dim_]>*plane-2e-9)
        {
          vector<int> dof = discret_->Dof(node);
          double      one = 1.0;

          toggleu_->ReplaceGlobalValues(1,&one,&(dof[0]));
          togglev_->ReplaceGlobalValues(1,&one,&(dof[1]));
          togglew_->ReplaceGlobalValues(1,&one,&(dof[2]));
        }
      }

      double inc=0.0;
      force.Dot(*toggleu_,&inc);
      sumforceu_+=inc;
      inc=0.0;
      force.Dot(*togglev_,&inc);
      sumforcev_+=inc;
      inc=0.0;
      force.Dot(*togglew_,&inc);
      sumforcew_+=inc;
    }
  }

  return;
}// TurbulenceStatisticsCha::DoLomaTimeSample

/*----------------------------------------------------------------------*
 
          Compute in plane means of u,u^2 etc. (integral version)

 -----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::EvaluateIntegralMeanValuesInPlanes()
{

  //----------------------------------------------------------------------
  // loop elements and perform integration over homogeneous plane

  // create the parameters for the discretization
  ParameterList eleparams;

  // action for elements
  eleparams.set("action","calc_turbulence_statistics");

  // choose what to assemble
  eleparams.set("assemble matrix 1",false);
  eleparams.set("assemble matrix 2",false);
  eleparams.set("assemble vector 1",false);
  eleparams.set("assemble vector 2",false);
  eleparams.set("assemble vector 3",false);

  // set parameter list
  eleparams.set("normal direction to homogeneous plane",dim_);
  eleparams.set("coordinate vector for hom. planes",planecoordinates_);

  // set size of vectors
  int size = sumu_->size();

  // generate processor local result vectors
  RefCountPtr<vector<double> > locarea =  rcp(new vector<double> );
  locarea->resize(size,0.0);

  RefCountPtr<vector<double> > locsumu =  rcp(new vector<double> );
  locsumu->resize(size,0.0);

  RefCountPtr<vector<double> > locsumv =  rcp(new vector<double> );
  locsumv->resize(size,0.0);

  RefCountPtr<vector<double> > locsumw =  rcp(new vector<double> );
  locsumw->resize(size,0.0);

  RefCountPtr<vector<double> > locsump =  rcp(new vector<double> );
  locsump->resize(size,0.0);

  RefCountPtr<vector<double> > locsumsqu =  rcp(new vector<double> );
  locsumsqu->resize(size,0.0);

  RefCountPtr<vector<double> > locsumsqv =  rcp(new vector<double> );
  locsumsqv->resize(size,0.0);

  RefCountPtr<vector<double> > locsumsqw =  rcp(new vector<double> );
  locsumsqw->resize(size,0.0);

  RefCountPtr<vector<double> > locsumuv  =  rcp(new vector<double> );
  locsumuv->resize(size,0.0);

  RefCountPtr<vector<double> > locsumuw  =  rcp(new vector<double> );
  locsumuw->resize(size,0.0);

  RefCountPtr<vector<double> > locsumvw  =  rcp(new vector<double> );
  locsumvw->resize(size,0.0);

  RefCountPtr<vector<double> > locsumsqp =  rcp(new vector<double> );
  locsumsqp->resize(size,0.0);

  RefCountPtr<vector<double> > globarea =  rcp(new vector<double> );
  globarea->resize(size,0.0);

  RefCountPtr<vector<double> > globsumu =  rcp(new vector<double> );
  globsumu->resize(size,0.0);

  RefCountPtr<vector<double> > globsumv =  rcp(new vector<double> );
  globsumv->resize(size,0.0);

  RefCountPtr<vector<double> > globsumw =  rcp(new vector<double> );
  globsumw->resize(size,0.0);

  RefCountPtr<vector<double> > globsump =  rcp(new vector<double> );
  globsump->resize(size,0.0);

  RefCountPtr<vector<double> > globsumsqu =  rcp(new vector<double> );
  globsumsqu->resize(size,0.0);

  RefCountPtr<vector<double> > globsumsqv =  rcp(new vector<double> );
  globsumsqv->resize(size,0.0);

  RefCountPtr<vector<double> > globsumsqw =  rcp(new vector<double> );
  globsumsqw->resize(size,0.0);

  RefCountPtr<vector<double> > globsumuv  =  rcp(new vector<double> );
  globsumuv->resize(size,0.0);

  RefCountPtr<vector<double> > globsumuw  =  rcp(new vector<double> );
  globsumuw->resize(size,0.0);

  RefCountPtr<vector<double> > globsumvw  =  rcp(new vector<double> );
  globsumvw->resize(size,0.0);

  RefCountPtr<vector<double> > globsumsqp =  rcp(new vector<double> );
  globsumsqp->resize(size,0.0);

  // communicate pointers to the result vectors to the element
  eleparams.set("element layer area"  ,locarea );
  eleparams.set("mean velocity u"     ,locsumu);
  eleparams.set("mean velocity v"     ,locsumv);
  eleparams.set("mean velocity w"     ,locsumw);
  eleparams.set("mean pressure p"     ,locsump);

  eleparams.set("mean value u^2",locsumsqu);
  eleparams.set("mean value v^2",locsumsqv);
  eleparams.set("mean value w^2",locsumsqw);
  eleparams.set("mean value uv" ,locsumuv );
  eleparams.set("mean value uw" ,locsumuw );
  eleparams.set("mean value vw" ,locsumvw );
  eleparams.set("mean value p^2",locsumsqp);

  // counts the number of elements in the lowest homogeneous plane
  // (the number is the same for all planes, since we use a structured
  //  cartesian mesh)
  int locprocessedeles=0;

  eleparams.set("count processed elements",&locprocessedeles);

  // set vector values needed by elements
  discret_->ClearState();
  discret_->SetState("u and p (n+1,converged)"    ,meanvelnp_   );
  if(alefluid_)
  {
    discret_->SetState("dispnp"                   ,dispnp_      );
  }

  // call loop over elements
  discret_->Evaluate(eleparams,null,null,null,null,null);
  discret_->ClearState();


  //----------------------------------------------------------------------
  // add contributions from all processors
  discret_->Comm().SumAll(&((*locarea)[0]),&((*globarea)[0]),size);

  discret_->Comm().SumAll(&((*locsumu)[0]),&((*globsumu)[0]),size);
  discret_->Comm().SumAll(&((*locsumv)[0]),&((*globsumv)[0]),size);
  discret_->Comm().SumAll(&((*locsumw)[0]),&((*globsumw)[0]),size);
  discret_->Comm().SumAll(&((*locsump)[0]),&((*globsump)[0]),size);

  discret_->Comm().SumAll(&((*locsumsqu)[0]),&((*globsumsqu)[0]),size);
  discret_->Comm().SumAll(&((*locsumsqv)[0]),&((*globsumsqv)[0]),size);
  discret_->Comm().SumAll(&((*locsumsqw)[0]),&((*globsumsqw)[0]),size);
  discret_->Comm().SumAll(&((*locsumuv )[0]),&((*globsumuv )[0]),size);
  discret_->Comm().SumAll(&((*locsumuw )[0]),&((*globsumuw )[0]),size);
  discret_->Comm().SumAll(&((*locsumvw )[0]),&((*globsumvw )[0]),size);
  discret_->Comm().SumAll(&((*locsumsqp)[0]),&((*globsumsqp)[0]),size);


  //----------------------------------------------------------------------
  // the sums are divided by the layers area to get the area average

  DRT::NURBS::NurbsDiscretization* nurbsdis
    =
    dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(*discret_));

  if(nurbsdis == NULL)
  {
    discret_->Comm().SumAll(&locprocessedeles,&numele_,1);
  }
  else
  {
    // get nurbs dis' element numbers
    vector<int> nele_x_mele_x_lele(nurbsdis->Return_nele_x_mele_x_lele(0));

    numele_ = nele_x_mele_x_lele[0]*nele_x_mele_x_lele[2];
  }


  for(unsigned i=0; i<planecoordinates_->size(); ++i)
  {
    // get averege element size
    (*globarea)[i]/=numele_;

    (*sumu_)[i]  +=(*globsumu)[i]/(*globarea)[i];
    (*sumv_)[i]  +=(*globsumv)[i]/(*globarea)[i];
    (*sumw_)[i]  +=(*globsumw)[i]/(*globarea)[i];
    (*sump_)[i]  +=(*globsump)[i]/(*globarea)[i];

    (*sumsqu_)[i]+=(*globsumsqu)[i]/(*globarea)[i];
    (*sumsqv_)[i]+=(*globsumsqv)[i]/(*globarea)[i];
    (*sumsqw_)[i]+=(*globsumsqw)[i]/(*globarea)[i];
    (*sumuv_ )[i]+=(*globsumuv )[i]/(*globarea)[i];
    (*sumuw_ )[i]+=(*globsumuw )[i]/(*globarea)[i];
    (*sumvw_ )[i]+=(*globsumvw )[i]/(*globarea)[i];
    (*sumsqp_)[i]+=(*globsumsqp)[i]/(*globarea)[i];
  }

  return;

}// TurbulenceStatisticsCha::EvaluateIntegralMeanValuesInPlanes()

/*----------------------------------------------------------------------*

          Compute in plane means of u,u^2 etc. (nodal quantities)

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::EvaluatePointwiseMeanValuesInPlanes()
{
  int planenum = 0;

  //----------------------------------------------------------------------
  // pointwise multiplication to get squared values

  pointsquaredvelnp_->Multiply(1.0,*meanvelnp_,*meanvelnp_,0.0);


  //----------------------------------------------------------------------
  // loop planes and calculate pointwise means in each plane

  for(vector<double>::iterator plane=planecoordinates_->begin();
      plane!=planecoordinates_->end();
      ++plane)
  {

    // toggle vectors are one in the position of a dof in this plane,
    // else 0
    toggleu_->PutScalar(0.0);
    togglev_->PutScalar(0.0);
    togglew_->PutScalar(0.0);
    togglep_->PutScalar(0.0);

    // count the number of nodes in plane (required to calc. in plane mean)
    int countnodesinplane=0;

    //----------------------------------------------------------------------
    // activate toggles for in plane dofs

    for (int nn=0; nn<discret_->NumMyRowNodes(); ++nn)
    {
      DRT::Node* node = discret_->lRowNode(nn);

      // this node belongs to the plane under consideration
      if (node->X()[dim_]<*plane+2e-9 && node->X()[dim_]>*plane-2e-9)
      {
        vector<int> dof = discret_->Dof(node);
        double      one = 1.0;

        toggleu_->ReplaceGlobalValues(1,&one,&(dof[0]));
        togglev_->ReplaceGlobalValues(1,&one,&(dof[1]));
        togglew_->ReplaceGlobalValues(1,&one,&(dof[2]));
        togglep_->ReplaceGlobalValues(1,&one,&(dof[3]));

        // now check whether we have a pbc condition on this node
        vector<DRT::Condition*> mypbc;

        node->GetCondition("SurfacePeriodic",mypbc);

        // yes, we have a pbc
        if (mypbc.size()>0)
        {
          // loop them and check, whether this is a pbc pure master node
          // for all previous conditions
          unsigned ntimesmaster = 0;
          for (unsigned numcond=0;numcond<mypbc.size();++numcond)
          {
            const string* mymasterslavetoggle
              = mypbc[numcond]->Get<string>("Is slave periodic boundary condition");

            if(*mymasterslavetoggle=="Master")
            {
              ++ntimesmaster;
            } // end is slave?
          } // end loop this conditions

          if(ntimesmaster!=mypbc.size())
          {
            continue;
          }
          // we have a master. Remember this cause we have to extend the patch
          // to the other side...
        }
        countnodesinplane++;
      }
    }

    int countnodesinplaneonallprocs=0;

    discret_->Comm().SumAll(&countnodesinplane,&countnodesinplaneonallprocs,1);

    if (countnodesinplaneonallprocs)
    {
      //----------------------------------------------------------------------
      // compute scalar products from velnp and toggle vec to sum up
      // values in this plane

      double inc=0.0;
      meanvelnp_->Dot(*toggleu_,&inc);
      (*pointsumu_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvelnp_->Dot(*togglev_,&inc);
      (*pointsumv_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvelnp_->Dot(*togglew_,&inc);
      (*pointsumw_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvelnp_->Dot(*togglep_,&inc);
      (*pointsump_)[planenum]+=inc/countnodesinplaneonallprocs;

      //----------------------------------------------------------------------
      // compute scalar products from squaredvelnp and toggle vec to
      // sum up values for second order moments in this plane

      inc=0.0;
      pointsquaredvelnp_->Dot(*toggleu_,&inc);
      (*pointsumsqu_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvelnp_->Dot(*togglev_,&inc);
      (*pointsumsqv_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvelnp_->Dot(*togglew_,&inc);
      (*pointsumsqw_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvelnp_->Dot(*togglep_,&inc);
      (*pointsumsqp_)[planenum]+=inc/countnodesinplaneonallprocs;
    }
    planenum++;
  }

  return;
}// TurbulenceStatisticsCha::EvaluatePointwiseMeanValuesInPlanes()


/*----------------------------------------------------------------------*

        Add computed dynamic Smagorinsky quantities (Smagorinsky 
           constant, effective viscosity and (Cs_delta)^2 used 
                      during the computation)

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::AddDynamicSmagorinskyQuantities()
{
  // get sublist of turbulence parameters from the fluid dynamic 
  // parameter list --- it is used to transfer data between element
  // and statistics method
  ParameterList *  modelparams =&(params_.sublist("TURBULENCE MODEL"));

  // extract values for Cs, Cs_delta_sq_ and visceff from parameterlist
  // the values are stored in vectors --- each component corresponds to
  // one element layer
  RefCountPtr<vector<double> > global_incr_Cs_sum;
  RefCountPtr<vector<double> > local_Cs_sum;
  global_incr_Cs_sum          = rcp(new vector<double> (nodeplanes_->size()-1,0.0));
  local_Cs_sum                = modelparams->get<RefCountPtr<vector<double> > >("local_Cs_sum"         ,Teuchos::null);
  if(local_Cs_sum==Teuchos::null)
    dserror("local_Cs_sum==null from parameterlist");
        
  RefCountPtr<vector<double> > global_incr_Cs_delta_sq_sum;
  RefCountPtr<vector<double> > local_Cs_delta_sq_sum;
  global_incr_Cs_delta_sq_sum = rcp(new vector<double> (nodeplanes_->size()-1,0.0));
  local_Cs_delta_sq_sum       = modelparams->get<RefCountPtr<vector<double> > >("local_Cs_delta_sq_sum",Teuchos::null);
  if(local_Cs_delta_sq_sum==Teuchos::null)
    dserror("local_Cs_delta_sq_sum==null from parameterlist");
  
  RefCountPtr<vector<double> > global_incr_visceff_sum;
  RefCountPtr<vector<double> > local_visceff_sum;
  global_incr_visceff_sum     = rcp(new vector<double> (nodeplanes_->size()-1,0.0));
  local_visceff_sum           = modelparams->get<RefCountPtr<vector<double> > >("local_visceff_sum"    ,Teuchos::null);
  if(local_visceff_sum==Teuchos::null)
      dserror("local_visceff_sum==null from parameterlist");

  // now add all the stuff from the different processors
  discret_->Comm().SumAll(&((*local_Cs_sum               )[0]),
                          &((*global_incr_Cs_sum         )[0]),
                          local_Cs_sum->size());
  discret_->Comm().SumAll(&((*local_Cs_delta_sq_sum      )[0]),
                          &((*global_incr_Cs_delta_sq_sum)[0]),
                          local_Cs_delta_sq_sum->size());
  discret_->Comm().SumAll(&((*local_visceff_sum          )[0]),
                          &((*global_incr_visceff_sum    )[0]),
                          local_visceff_sum->size());
 
  // Replace increment to compute average of Smagorinsky Constant, effective
  // viscosity and (Cs_delta)^2
  for (unsigned rr=0;rr<global_incr_Cs_sum->size();++rr)
  {
    (*incrsumCs_         )[rr] =(*global_incr_Cs_sum         )[rr];
    (*incrsumCs_delta_sq_)[rr] =(*global_incr_Cs_delta_sq_sum)[rr];
    (*incrsumvisceff_    )[rr] =(*global_incr_visceff_sum    )[rr];
  }
        
  // reinitialise to zero for next element call
  local_Cs_sum          =  rcp(new vector<double> (nodeplanes_->size()-1,0.0));
  local_Cs_delta_sq_sum =  rcp(new vector<double> (nodeplanes_->size()-1,0.0));
  local_visceff_sum     =  rcp(new vector<double> (nodeplanes_->size()-1,0.0));
          
  modelparams->set<RefCountPtr<vector<double> > >("local_Cs_sum"         ,local_Cs_sum         );
  modelparams->set<RefCountPtr<vector<double> > >("local_Cs_delta_sq_sum",local_Cs_delta_sq_sum);
  modelparams->set<RefCountPtr<vector<double> > >("local_visceff_sum"    ,local_visceff_sum    );
  
  return;
} // FLD::TurbulenceStatisticsCha::AddDynamicSmagorinskyQuantities


/*----------------------------------------------------------------------*


  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::EvaluateResiduals(
  map<string,RCP<Epetra_Vector> > statevecs,
  double                          time
  )
{

  if(subgrid_dissipation_)
  {
    //--------------------------------------------------------------------
    // set parameter list (time integration)
    
    // action for elements
    eleparams_.set("action","time average for subscales and residual");

    // other parameters that might be needed by the elements
    {
      ParameterList& timelist = eleparams_.sublist("time integration parameters");
      
      timelist.set("alpha_M",params_.get<double>("alpha_M"       ));
      timelist.set("alpha_F",params_.get<double>("alpha_F"       ));
      timelist.set("gamma"  ,params_.get<double>("gamma"         ));
      timelist.set("dt"     ,params_.get<double>("time step size"));
      timelist.set("time"   ,time                                 );
    }
  
    // parameters for stabilisation
    {
      eleparams_.sublist("STABILIZATION") = params_.sublist("STABILIZATION");
    }
    
    // parameters for a turbulence model
    {
      eleparams_.sublist("TURBULENCE MODEL") = params_.sublist("TURBULENCE MODEL");
    }
    
    // set state vectors for element call
    for(map<string,RCP<Epetra_Vector> >::iterator state =statevecs.begin();
                                                  state!=statevecs.end()  ;
                                                  ++state                 )
    {
      discret_->SetState(state->first,state->second);
    }

    // call loop over elements to compute means
    discret_->Evaluate(eleparams_,null,null,null,null,null);
  
    discret_->ClearState();
 
    // ------------------------------------------------
    // get results from element call via parameter list
    RefCountPtr<vector<double> > local_vol               =eleparams_.get<RefCountPtr<vector<double> > >("incrvol"         );

    RefCountPtr<vector<double> > local_incrtauC          =eleparams_.get<RefCountPtr<vector<double> > >("incrtauC"        );
    RefCountPtr<vector<double> > local_incrtauM          =eleparams_.get<RefCountPtr<vector<double> > >("incrtauM"        );

    RefCountPtr<vector<double> > local_incrres           =eleparams_.get<RefCountPtr<vector<double> > >("incrres"         );
    RefCountPtr<vector<double> > local_incrres_sq        =eleparams_.get<RefCountPtr<vector<double> > >("incrres_sq"      );
    RefCountPtr<vector<double> > local_incrsacc          =eleparams_.get<RefCountPtr<vector<double> > >("incrsacc"        );
    RefCountPtr<vector<double> > local_incrsacc_sq       =eleparams_.get<RefCountPtr<vector<double> > >("incrsacc_sq"     );
    RefCountPtr<vector<double> > local_incrsvelaf        =eleparams_.get<RefCountPtr<vector<double> > >("incrsvelaf"      );
    RefCountPtr<vector<double> > local_incrsvelaf_sq     =eleparams_.get<RefCountPtr<vector<double> > >("incrsvelaf_sq"   );
    RefCountPtr<vector<double> > local_incrresC          =eleparams_.get<RefCountPtr<vector<double> > >("incrresC"        );
    RefCountPtr<vector<double> > local_incrresC_sq       =eleparams_.get<RefCountPtr<vector<double> > >("incrresC_sq"     );
    RefCountPtr<vector<double> > local_incrspressnp      =eleparams_.get<RefCountPtr<vector<double> > >("incrspressnp"    );
    RefCountPtr<vector<double> > local_incrspressnp_sq   =eleparams_.get<RefCountPtr<vector<double> > >("incrspressnp_sq" );

    RefCountPtr<vector<double> > local_incr_eps_sacc     = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_sacc"    );
    RefCountPtr<vector<double> > local_incr_eps_pspg     = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_pspg"    );
    RefCountPtr<vector<double> > local_incr_eps_supg     = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_supg"    );
    RefCountPtr<vector<double> > local_incr_eps_cross    = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_cross"   );
    RefCountPtr<vector<double> > local_incr_eps_rey      = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_rey"     );
    RefCountPtr<vector<double> > local_incr_eps_cstab    = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_cstab"   );
    RefCountPtr<vector<double> > local_incr_eps_vstab    = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_vstab"   );
    RefCountPtr<vector<double> > local_incr_eps_eddyvisc = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_eddyvisc");
    RefCountPtr<vector<double> > local_incr_eps_visc     = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_visc"    );
    RefCountPtr<vector<double> > local_incr_eps_conv     = eleparams_.get<RefCountPtr<vector<double> > >("incr_eps_conv"    );

    int presize = local_incrresC->size();
    int velsize = local_incrres ->size();

    //--------------------------------------------------
    // vectors to sum over all procs

    // volume of element layers
    RefCountPtr<vector<double> > global_vol;
    global_vol     =  rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of tauM/tauC

    RefCountPtr<vector<double> > global_incrtauM;
    global_incrtauM=  rcp(new vector<double> (presize,0.0));
    
    RefCountPtr<vector<double> > global_incrtauC;
    global_incrtauC=  rcp(new vector<double> (presize,0.0));
    
    // (in plane) averaged values of resM (^2)

    RefCountPtr<vector<double> > global_incrres;
    global_incrres=  rcp(new vector<double> (velsize,0.0));
    
    RefCountPtr<vector<double> > global_incrres_sq;
    global_incrres_sq=  rcp(new vector<double> (velsize,0.0));
  
    // (in plane) averaged values of sacc (^2)

    RefCountPtr<vector<double> > global_incrsacc;
    global_incrsacc=  rcp(new vector<double> (velsize,0.0));
      
    RefCountPtr<vector<double> > global_incrsacc_sq;
    global_incrsacc_sq=  rcp(new vector<double> (velsize,0.0));
      
    // (in plane) averaged values of svelaf (^2)

    RefCountPtr<vector<double> > global_incrsvelaf;
    global_incrsvelaf=  rcp(new vector<double> (velsize,0.0));

    RefCountPtr<vector<double> > global_incrsvelaf_sq;
    global_incrsvelaf_sq=  rcp(new vector<double> (velsize,0.0));

    // (in plane) averaged values of resC (^2)

    RefCountPtr<vector<double> > global_incrresC;
    global_incrresC=  rcp(new vector<double> (presize,0.0));

    RefCountPtr<vector<double> > global_incrresC_sq;
    global_incrresC_sq=  rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of spressnp (^2)

    RefCountPtr<vector<double> > global_incrspressnp;
    global_incrspressnp=  rcp(new vector<double> (presize,0.0));

    RefCountPtr<vector<double> > global_incrspressnp_sq;
    global_incrspressnp_sq=  rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by subscale acceleration

    RefCountPtr<vector<double> > global_incr_eps_sacc;
    global_incr_eps_sacc  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_pspg;
    global_incr_eps_pspg  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_supg;
    global_incr_eps_supg  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_cross;
    global_incr_eps_cross  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_rey;
    global_incr_eps_rey  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_cstab;
    global_incr_eps_cstab  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_vstab;
    global_incr_eps_vstab  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_eddyvisc;
    global_incr_eps_eddyvisc  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_visc;
    global_incr_eps_visc  = rcp(new vector<double> (presize,0.0));

    // (in plane) averaged values of dissipation by 

    RefCountPtr<vector<double> > global_incr_eps_conv;
    global_incr_eps_conv  = rcp(new vector<double> (presize,0.0));

    //--------------------------------------------------
    // global sums

    // compute global sum, volume
    discret_->Comm().SumAll(&((*local_vol )[0]),
                            &((*global_vol)[0]),
                            presize);

    // compute global sums, stabilisation parameters
    discret_->Comm().SumAll(&((*local_incrtauM )[0]),
                            &((*global_incrtauM)[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incrtauC )[0]),
                            &((*global_incrtauC)[0]),
                            presize);

    // compute global sums, momentum equation residuals
    discret_->Comm().SumAll(&((*local_incrres       )[0]),
                            &((*global_incrres      )[0]),
                            velsize);
    discret_->Comm().SumAll(&((*local_incrres_sq    )[0]),
                            &((*global_incrres_sq   )[0]),
                            velsize);
     
    discret_->Comm().SumAll(&((*local_incrsacc      )[0]),
                            &((*global_incrsacc     )[0]),
                            velsize);
    discret_->Comm().SumAll(&((*local_incrsacc_sq   )[0]),
                            &((*global_incrsacc_sq  )[0]),
                            velsize);
      
    discret_->Comm().SumAll(&((*local_incrsvelaf    )[0]),
                            &((*global_incrsvelaf   )[0]),
                            velsize);
    discret_->Comm().SumAll(&((*local_incrsvelaf_sq )[0]),
                            &((*global_incrsvelaf_sq)[0]),
                            velsize);

    // compute global sums, incompressibility residuals
    discret_->Comm().SumAll(&((*local_incrresC      )[0]),
                            &((*global_incrresC     )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incrresC_sq   )[0]),
                            &((*global_incrresC_sq  )[0]),
                            presize);
      
    discret_->Comm().SumAll(&((*local_incrspressnp     )[0]),
                            &((*global_incrspressnp    )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incrspressnp_sq  )[0]),
                            &((*global_incrspressnp_sq )[0]),
                            presize);
    
    // compute global sums, disspiation rates

    discret_->Comm().SumAll(&((*local_incr_eps_sacc  )[0]),
                            &((*global_incr_eps_sacc )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_pspg  )[0]),
                            &((*global_incr_eps_pspg )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_supg  )[0]),
                            &((*global_incr_eps_supg )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_cross  )[0]),
                            &((*global_incr_eps_cross )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_rey  )[0]),
                            &((*global_incr_eps_rey )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_cstab  )[0]),
                            &((*global_incr_eps_cstab )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_vstab  )[0]),
                            &((*global_incr_eps_vstab )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_eddyvisc  )[0]),
                            &((*global_incr_eps_eddyvisc )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_visc  )[0]),
                            &((*global_incr_eps_visc )[0]),
                            presize);
    discret_->Comm().SumAll(&((*local_incr_eps_conv  )[0]),
                            &((*global_incr_eps_conv )[0]),
                            presize);

    for (int rr=0;rr<velsize;++rr)
    {
      (*sumres_          )[rr]+=(*global_incrres          )[rr];
      (*sumres_sq_       )[rr]+=(*global_incrres_sq       )[rr];
      (*sumsacc_         )[rr]+=(*global_incrsacc         )[rr];
      (*sumsacc_sq_      )[rr]+=(*global_incrsacc_sq      )[rr];
      (*sumsvelaf_       )[rr]+=(*global_incrsvelaf       )[rr];
      (*sumsvelaf_sq_    )[rr]+=(*global_incrsvelaf_sq    )[rr];
    }
    for (int rr=0;rr<presize;++rr)
    {
      (*sumtauM_         )[rr]+=(*global_incrtauM         )[rr];
      (*sumtauC_         )[rr]+=(*global_incrtauC         )[rr];

      (*sumresC_         )[rr]+=(*global_incrresC         )[rr];
      (*sumresC_sq_      )[rr]+=(*global_incrresC_sq      )[rr];
      (*sumspressnp_     )[rr]+=(*global_incrspressnp     )[rr];
      (*sumspressnp_sq_  )[rr]+=(*global_incrspressnp_sq  )[rr];

      (*sum_eps_sacc_    )[rr]+=(*global_incr_eps_sacc    )[rr];
      (*sum_eps_pspg_    )[rr]+=(*global_incr_eps_pspg    )[rr];
      (*sum_eps_supg_    )[rr]+=(*global_incr_eps_supg    )[rr];
      (*sum_eps_cross_   )[rr]+=(*global_incr_eps_cross   )[rr];
      (*sum_eps_rey_     )[rr]+=(*global_incr_eps_rey     )[rr];
      (*sum_eps_cstab_   )[rr]+=(*global_incr_eps_cstab   )[rr];
      (*sum_eps_vstab_   )[rr]+=(*global_incr_eps_vstab   )[rr];
      (*sum_eps_eddyvisc_)[rr]+=(*global_incr_eps_eddyvisc)[rr];
      (*sum_eps_visc_    )[rr]+=(*global_incr_eps_visc    )[rr];
      (*sum_eps_conv_    )[rr]+=(*global_incr_eps_conv    )[rr];
    }
 
    // reset working arrays
    local_vol               = rcp(new vector<double> (presize,0.0));

    local_incrtauC          = rcp(new vector<double> (presize,0.0));
    local_incrtauM          = rcp(new vector<double> (presize,0.0));

    local_incrres           = rcp(new vector<double> (velsize,0.0));
    local_incrres_sq        = rcp(new vector<double> (velsize,0.0));
    local_incrsacc          = rcp(new vector<double> (velsize,0.0));
    local_incrsacc_sq       = rcp(new vector<double> (velsize,0.0));
    local_incrsvelaf        = rcp(new vector<double> (velsize,0.0));
    local_incrsvelaf_sq     = rcp(new vector<double> (velsize,0.0));

    local_incrresC          = rcp(new vector<double> (presize,0.0));
    local_incrresC_sq       = rcp(new vector<double> (presize,0.0));
    local_incrspressnp      = rcp(new vector<double> (presize,0.0));
    local_incrspressnp_sq   = rcp(new vector<double> (presize,0.0));

    local_incr_eps_sacc     = rcp(new vector<double> (presize,0.0));
    local_incr_eps_pspg     = rcp(new vector<double> (presize,0.0));
    local_incr_eps_supg     = rcp(new vector<double> (presize,0.0));
    local_incr_eps_cross    = rcp(new vector<double> (presize,0.0));
    local_incr_eps_rey      = rcp(new vector<double> (presize,0.0));
    local_incr_eps_cstab    = rcp(new vector<double> (presize,0.0));
    local_incr_eps_vstab    = rcp(new vector<double> (presize,0.0));
    local_incr_eps_eddyvisc = rcp(new vector<double> (presize,0.0));
    local_incr_eps_visc     = rcp(new vector<double> (presize,0.0));
    local_incr_eps_conv     = rcp(new vector<double> (presize,0.0));


    eleparams_.set<RefCountPtr<vector<double> > >("incrvol"          ,local_vol              );

    eleparams_.set<RefCountPtr<vector<double> > >("incrtauC"         ,local_incrtauC         );
    eleparams_.set<RefCountPtr<vector<double> > >("incrtauM"         ,local_incrtauM         );
    
    eleparams_.set<RefCountPtr<vector<double> > >("incrres"          ,local_incrres          );
    eleparams_.set<RefCountPtr<vector<double> > >("incrres_sq"       ,local_incrres_sq       );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsacc"         ,local_incrsacc         );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsacc_sq"      ,local_incrsacc_sq      );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsvelaf"       ,local_incrsvelaf       );
    eleparams_.set<RefCountPtr<vector<double> > >("incrsvelaf_sq"    ,local_incrsvelaf_sq    );

    eleparams_.set<RefCountPtr<vector<double> > >("incrresC"         ,local_incrresC         );
    eleparams_.set<RefCountPtr<vector<double> > >("incrresC_sq"      ,local_incrresC_sq      );
    eleparams_.set<RefCountPtr<vector<double> > >("incrspressnp"     ,local_incrspressnp     );
    eleparams_.set<RefCountPtr<vector<double> > >("incrspressnp_sq"  ,local_incrspressnp_sq  );

    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_sacc"    ,local_incr_eps_sacc    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_pspg"    ,local_incr_eps_pspg    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_supg"    ,local_incr_eps_supg    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_cross"   ,local_incr_eps_cross   );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_rey"     ,local_incr_eps_rey     );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_cstab"   ,local_incr_eps_cstab   );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_vstab"   ,local_incr_eps_vstab   );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_eddyvisc",local_incr_eps_eddyvisc);
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_visc"    ,local_incr_eps_visc    );
    eleparams_.set<RefCountPtr<vector<double> > >("incr_eps_conv"    ,local_incr_eps_conv    );
  }

  return;
} // FLD::TurbulenceStatisticsCha::EvaluateResiduals




/*----------------------------------------------------------------------*
  
               The same as above for low-Mach-number flows

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::EvaluateLomaPointwiseMeanValuesInPlanes()
{
  int planenum = 0;

  //----------------------------------------------------------------------
  // pointwise multiplication to get squared values

  pointsquaredvelnp_->Multiply(1.0,*meanvelnp_,*meanvelnp_,0.0);
  pointsquaredvedenp_->Multiply(1.0,*meanvedenp_,*meanvedenp_,0.0);

  //----------------------------------------------------------------------
  // loop planes and calculate pointwise means in each plane

  for(vector<double>::iterator plane=planecoordinates_->begin();
      plane!=planecoordinates_->end();
      ++plane)
  {

    // toggle vectors are one in the position of a dof in this plane,
    // else 0
    toggleu_->PutScalar(0.0);
    togglev_->PutScalar(0.0);
    togglew_->PutScalar(0.0);
    togglep_->PutScalar(0.0);

    // count the number of nodes in plane (required to calc. in plane mean)
    int countnodesinplane=0;

    //----------------------------------------------------------------------
    // activate toggles for in plane dofs

    for (int nn=0; nn<discret_->NumMyRowNodes(); ++nn)
    {
      DRT::Node* node = discret_->lRowNode(nn);

      // this node belongs to the plane under consideration
      if (node->X()[dim_]<*plane+2e-9 && node->X()[dim_]>*plane-2e-9)
      {
        vector<int> dof = discret_->Dof(node);
        double      one = 1.0;

        toggleu_->ReplaceGlobalValues(1,&one,&(dof[0]));
        togglev_->ReplaceGlobalValues(1,&one,&(dof[1]));
        togglew_->ReplaceGlobalValues(1,&one,&(dof[2]));
        togglep_->ReplaceGlobalValues(1,&one,&(dof[3]));

        // now check whether we have a pbc condition on this node
        vector<DRT::Condition*> mypbc;

        node->GetCondition("SurfacePeriodic",mypbc);

        // yes, we have a pbc
        if (mypbc.size()>0)
        {
          // loop them and check, whether this is a pbc pure master node
          // for all previous conditions
          unsigned ntimesmaster = 0;
          for (unsigned numcond=0;numcond<mypbc.size();++numcond)
          {
            const string* mymasterslavetoggle
              = mypbc[numcond]->Get<string>("Is slave periodic boundary condition");

            if(*mymasterslavetoggle=="Master")
            {
              ++ntimesmaster;
            } // end is slave?
          } // end loop this conditions

          if(ntimesmaster!=mypbc.size())
          {
            continue;
          }
          // we have a master. Remember this cause we have to extend the patch
          // to the other side...
        }
        countnodesinplane++;
      }
    }

    int countnodesinplaneonallprocs=0;

    discret_->Comm().SumAll(&countnodesinplane,&countnodesinplaneonallprocs,1);

    if (countnodesinplaneonallprocs)
    {
      //----------------------------------------------------------------------
      // compute scalar products from velnp and toggle vec to sum up
      // values in this plane

      double inc=0.0;
      meanvelnp_->Dot(*toggleu_,&inc);
      (*pointsumu_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvelnp_->Dot(*togglev_,&inc);
      (*pointsumv_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvelnp_->Dot(*togglew_,&inc);
      (*pointsumw_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvelnp_->Dot(*togglep_,&inc);
      (*pointsump_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      meanvedenp_->Dot(*togglep_,&inc);
      (*pointsumT_)[planenum]+=inc/countnodesinplaneonallprocs;

      //----------------------------------------------------------------------
      // compute scalar products from squaredvelnp and toggle vec to
      // sum up values for second order moments in this plane

      inc=0.0;
      pointsquaredvelnp_->Dot(*toggleu_,&inc);
      (*pointsumsqu_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvelnp_->Dot(*togglev_,&inc);
      (*pointsumsqv_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvelnp_->Dot(*togglew_,&inc);
      (*pointsumsqw_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvelnp_->Dot(*togglep_,&inc);
      (*pointsumsqp_)[planenum]+=inc/countnodesinplaneonallprocs;
      inc=0.0;
      pointsquaredvedenp_->Dot(*togglep_,&inc);
      (*pointsumsqT_)[planenum]+=inc/countnodesinplaneonallprocs;
    }
    planenum++;
  }

  return;
}// TurbulenceStatisticsCha::EvaluateLomaPointwiseMeanValuesInPlanes()


/*----------------------------------------------------------------------*
  
       Compute a time average of the mean values over all steps
          since the last output. Dump the result to file.

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::TimeAverageMeansAndOutputOfStatistics(int step)
{
  if (numsamp_ == 0)
  {
    dserror("No samples to do time average");
  }

  //----------------------------------------------------------------------
  // the sums are divided by the number of samples to get the time average
  int aux = numele_*numsamp_;

  for(unsigned i=0; i<planecoordinates_->size(); ++i)
  {

    (*sumu_  )[i] /=aux;
    (*sumv_  )[i] /=aux;
    (*sumw_  )[i] /=aux;
    (*sump_  )[i] /=aux;

    (*sumuv_ )[i] /=aux;
    (*sumuw_ )[i] /=aux;
    (*sumvw_ )[i] /=aux;

    (*sumsqu_)[i] /=aux;
    (*sumsqv_)[i] /=aux;
    (*sumsqw_)[i] /=aux;
    (*sumsqp_)[i] /=aux;
  }

  DRT::NURBS::NurbsDiscretization* nurbsdis
    =
    dynamic_cast<DRT::NURBS::NurbsDiscretization*>(&(*discret_));

  if(nurbsdis == NULL)
  {
    for(unsigned i=0; i<planecoordinates_->size(); ++i)
    {
      // the pointwise values have already been normalised by
      // "countnodesinplaneonallprocs", so we just divide by
      // the number of time samples
      (*pointsumu_)[i]   /=numsamp_;
      (*pointsumv_)[i]   /=numsamp_;
      (*pointsumw_)[i]   /=numsamp_;
      (*pointsump_)[i]   /=numsamp_;

      (*pointsumsqu_)[i] /=numsamp_;
      (*pointsumsqv_)[i] /=numsamp_;
      (*pointsumsqw_)[i] /=numsamp_;
      (*pointsumsqp_)[i] /=numsamp_;
    }
  }

  sumforceu_/=numsamp_;
  sumforcev_/=numsamp_;
  sumforcew_/=numsamp_;

  //----------------------------------------------------------------------
  // evaluate area to calculate u_tau, l_tau (and tau_W)
  double area = 1.0;
  for (int i=0;i<3;i++)
  {
    if(i!=dim_)
    {
      area*=((*boundingbox_)(1,i)-(*boundingbox_)(0,i));
    }
  }
  // there are two Dirichlet boundaries
  area*=2;

  //----------------------------------------------------------------------
  // we expect nonzero forces (tractions) only in flow direction
  int flowdirection =0;

  // ltau is used to compute y+
  double ltau = 0;
  if      (sumforceu_>sumforcev_ && sumforceu_>sumforcew_)
  {
    flowdirection=0;
    ltau = visc_/sqrt(sumforceu_/area);
  }
  else if (sumforcev_>sumforceu_ && sumforcev_>sumforcew_)
  {
    flowdirection=1;
    ltau = visc_/sqrt(sumforcev_/area);
  }
  else if (sumforcew_>sumforceu_ && sumforcew_>sumforcev_)
  {
    flowdirection=2;
    ltau = visc_/sqrt(sumforcew_/area);
  }
  else
  {
    dserror("Cannot determine flow direction by traction (seems to be not unique)");
  }

  //----------------------------------------------------------------------
  // output to log-file
  Teuchos::RefCountPtr<std::ofstream> log;
  if (discret_->Comm().MyPID()==0)
  {
    std::string s = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
    s.append(".flow_statistic");

    log = Teuchos::rcp(new std::ofstream(s.c_str(),ios::app));
    (*log) << "\n\n\n";
    (*log) << "# Statistics record " << countrecord_;
    (*log) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";

    (*log) << "# (u_tau)^2 = tau_W/rho : ";
    (*log) << "   " << setw(11) << setprecision(4) << sumforceu_/area;
    (*log) << "   " << setw(11) << setprecision(4) << sumforcev_/area;
    (*log) << "   " << setw(11) << setprecision(4) << sumforcew_/area;
    (*log) << &endl;


    (*log) << "#|-------------------";
    (*log) << "----------------------------------------------------------";
    (*log) << "--integration based-------------------------";
    (*log) << "----------------------------------------------------------|";
    (*log) << "-------------------------------------------------point";
    (*log) << "wise---------------------------------------";
    (*log) << "------------|\n";

    (*log) << "#     y            y+";
    (*log) << "           umean         vmean         wmean         pmean";
    (*log) << "        mean u^2      mean v^2      mean w^2";
    (*log) << "      mean u*v      mean u*w      mean v*w      mean p^2";
    (*log) << "       umean         vmean         wmean         pmean";
    (*log) << "        mean u^2      mean v^2      mean w^2";
    (*log) << "       mean p^2 \n";
    (*log) << scientific;
    for(unsigned i=0; i<planecoordinates_->size(); ++i)
    {
      // y and y+
      (*log) <<  " "  << setw(11) << setprecision(4) << (*planecoordinates_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*planecoordinates_)[i]/ltau;

      // integral mean values
      (*log) << "   " << setw(11) << setprecision(4) << (*sumu_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumv_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumw_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sump_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqu_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqv_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqw_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumuv_ )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumuw_ )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumvw_ )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqp_)[i];

      // pointwise means
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumu_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumv_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumw_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsump_  )[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqu_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqv_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqw_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqp_)[i];
      (*log) << "   \n";
    }
    log->flush();

    // ------------------------------------------------------------------
    // additional output for dynamic Smagorinsky model
    if (smagorinsky_)
    {
      // get the outfile
      Teuchos::RefCountPtr<std::ofstream> log_Cs;

      std::string s_smag = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
      s_smag.append(".Cs_statistic");
      
      log_Cs = Teuchos::rcp(new std::ofstream(s_smag.c_str(),ios::app));
      
      (*log_Cs) << "\n\n\n";
      (*log_Cs) << "# Statistics record " << countrecord_;
      (*log_Cs) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";


      (*log_Cs) << "#     y      ";
      (*log_Cs) << "     Cs     ";
      (*log_Cs) << "   (Cs*hk)^2 ";
      (*log_Cs) << "    visceff  ";
      (*log_Cs) << &endl;
      (*log_Cs) << scientific;
      for (unsigned rr=0;rr<sumCs_->size();++rr)
      {
        // we associate the value with the midpoint of the element layer
        (*log_Cs) << setw(11) << setprecision(4) << 0.5*((*nodeplanes_)[rr+1]+(*nodeplanes_)[rr]) << "  " ;

        // the three values to be visualised
        (*log_Cs) << setw(11) << setprecision(4) << ((*sumCs_         )[rr])/(numele_*numsamp_)   << "  ";
        (*log_Cs) << setw(11) << setprecision(4) << ((*sumCs_delta_sq_)[rr])/(numele_*numsamp_)   << "  " ;
        (*log_Cs) << setw(11) << setprecision(4) << ((*sumvisceff_    )[rr])/(numele_*numsamp_)   << &endl;
      }
      log_Cs->flush();
    } // end smagorinsky_

    if(subgrid_dissipation_)
    {
      Teuchos::RefCountPtr<std::ofstream> log_res;

      // output of residuals and subscale quantities
      std::string s_res = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
      s_res.append(".res_statistic");

      log_res = Teuchos::rcp(new std::ofstream(s_res.c_str(),ios::app));

      (*log_res) << "\n\n\n";
      (*log_res) << "# Statistics record " << countrecord_;
      (*log_res) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";
      (*log_res) << "#       y    ";

      (*log_res) << "    res_x   ";
      (*log_res) << "      res_y  ";
      (*log_res) << "      res_z  ";
      (*log_res) << "     sacc_x  ";
      (*log_res) << "     sacc_y  ";
      (*log_res) << "     sacc_z  ";
      (*log_res) << "     svel_x  ";
      (*log_res) << "     svel_y  ";
      (*log_res) << "     svel_z  ";
      
      (*log_res) << "   res_sq_x  ";
      (*log_res) << "   res_sq_y  ";
      (*log_res) << "   res_sq_z  ";
      (*log_res) << "   sacc_sq_x ";
      (*log_res) << "   sacc_sq_y ";
      (*log_res) << "   sacc_sq_z ";
      (*log_res) << "   svel_sq_x ";
      (*log_res) << "   svel_sq_y ";
      (*log_res) << "   svel_sq_z ";

      (*log_res) << "      resC   ";
      (*log_res) << "    spresnp  ";

      (*log_res) << "    resC_sq  ";
      (*log_res) << "  spresnp_sq ";

      (*log_res) << "    tauM     ";
      (*log_res) << "    tauC     ";

      (*log_res) << "  eps_sacc   ";
      (*log_res) << "  eps_pspg   ";
      (*log_res) << "  eps_supg   ";
      (*log_res) << "  eps_cross  ";
      (*log_res) << "   eps_rey   ";
      (*log_res) << "  eps_cstab  ";
      (*log_res) << "  eps_vstab  ";
      (*log_res) << " eps_eddyvisc";
      (*log_res) << "   eps_visc  ";
      (*log_res) << "   eps_conv  "<<&endl;

      (*log_res) << scientific;
      for (unsigned rr=0;rr<nodeplanes_->size()-1;++rr)
      {
        (*log_res)  << setw(11) << setprecision(4) << 0.5*((*nodeplanes_)[rr+1]+(*nodeplanes_)[rr]) << "  " ;
        
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_      )[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_      )[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_      )[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_     )[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_     )[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_     )[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumsvelaf_   )[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsvelaf_   )[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsvelaf_   )[3*rr+2]/(numele_*numsamp_) << "  ";
        
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_sq_   )[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_sq_   )[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_sq_   )[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_sq_  )[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_sq_  )[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_sq_  )[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumsvelaf_sq_)[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsvelaf_sq_)[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsvelaf_sq_)[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumresC_         )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumspressnp_     )[rr]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumresC_sq_      )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumspressnp_sq_  )[rr]/(numele_*numsamp_) << "  ";
        
        (*log_res)  << setw(11) << setprecision(4) << (*sumtauM_         )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumtauC_         )[rr]/(numele_*numsamp_) << "  ";
      
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_sacc_    )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_pspg_    )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_supg_    )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_cross_   )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_rey_     )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_cstab_   )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_vstab_   )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_eddyvisc_)[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_visc_    )[rr]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sum_eps_conv_    )[rr]/(numele_*numsamp_) << "  ";

        (*log_res)  << &endl;
      }
      log_res->flush();
    } // end subgrid_dissipation_
  } // end myrank 0


  // log was written, so increase counter for records
  countrecord_++;

  return;

}// TurbulenceStatisticsCha::TimeAverageMeansAndOutputOfStatistics


/*----------------------------------------------------------------------*

      Compute a time average of the mean values over all steps
       of the sampling period so far. Dump the result to file.  

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::DumpStatistics(int step)
{
  if (numsamp_ == 0)
  {
    dserror("No samples to do time average");
  }

  //----------------------------------------------------------------------
  // the sums are divided by the number of samples to get the time average
  int aux = numele_*numsamp_;

  //----------------------------------------------------------------------
  // evaluate area to calculate u_tau, l_tau (and tau_W)
  double area = 1.0;
  for (int i=0;i<3;i++)
  {
    if(i!=dim_)
    {
      area*=((*boundingbox_)(1,i)-(*boundingbox_)(0,i));
    }
  }
  // there are two Dirichlet boundaries
  area*=2;

  //----------------------------------------------------------------------
  // we expect nonzero forces (tractions) only in flow direction
  int flowdirection =0;

  // ltau is used to compute y+
  double ltau = 0;
  if      (sumforceu_>sumforcev_ && sumforceu_>sumforcew_)
  {
    flowdirection=0;
    ltau = visc_/sqrt(sumforceu_/(area*numsamp_));
  }
  else if (sumforcev_>sumforceu_ && sumforcev_>sumforcew_)
  {
    flowdirection=1;
    ltau = visc_/sqrt(sumforcev_/(area*numsamp_));
  }
  else if (sumforcew_>sumforceu_ && sumforcew_>sumforcev_)
  {
    flowdirection=2;
    ltau = visc_/sqrt(sumforcew_/(area*numsamp_));
  }
  else
  {
    dserror("Cannot determine flow direction by traction (seems to be not unique)");
  }

  //----------------------------------------------------------------------
  // output to log-file
  Teuchos::RefCountPtr<std::ofstream> log;
  if (discret_->Comm().MyPID()==0)
  {
    std::string s = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
    s.append(".flow_statistic");

    log = Teuchos::rcp(new std::ofstream(s.c_str(),ios::out));
    (*log) << "# Flow statistics for turbulent flow in a channel (first- and second-order moments)";
    (*log) << "\n\n\n";
    (*log) << "# Statistics record ";
    (*log) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";

    (*log) << "# (u_tau)^2 = tau_W/rho : ";
    (*log) << "   " << setw(11) << setprecision(4) << sumforceu_/(area*numsamp_);
    (*log) << "   " << setw(11) << setprecision(4) << sumforcev_/(area*numsamp_);
    (*log) << "   " << setw(11) << setprecision(4) << sumforcew_/(area*numsamp_);
    (*log) << &endl;

    (*log) << "#     y            y+";
    (*log) << "           umean         vmean         wmean         pmean";
    (*log) << "        mean u^2      mean v^2      mean w^2";
    (*log) << "      mean u*v      mean u*w      mean v*w        Varp   \n";

    (*log) << scientific;
    for(unsigned i=0; i<planecoordinates_->size(); ++i)
    {
      (*log) <<  " "  << setw(11) << setprecision(4) << (*planecoordinates_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*planecoordinates_)[i]/ltau;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumu_       )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumv_       )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumw_       )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sump_       )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqu_     )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqv_     )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqw_     )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumuv_      )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumuw_      )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumvw_      )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqp_     )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumu_  )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumv_  )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumw_  )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsump_  )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqu_)[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqv_)[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqw_)[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqp_)[i]/numsamp_;
      (*log) << "   \n";
    }
    log->flush();
  }

  if (discret_->Comm().MyPID()==0)
  {

    // additional output for dynamic Smagorinsky model
    if (smagorinsky_)
    {
      // get the outfile
      Teuchos::RefCountPtr<std::ofstream> log_Cs;
      
      std::string s_smag = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
      s_smag.append(".Cs_statistic");

      log_Cs = Teuchos::rcp(new std::ofstream(s_smag.c_str(),ios::out));
      (*log_Cs) << "# Smagorinsky parameter statistics for turbulent flow in a channel";
      (*log_Cs) << "\n\n\n";
      (*log_Cs) << "# Statistics record ";
      (*log_Cs) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";

      
      (*log_Cs) << "#     y      ";
      (*log_Cs) << "     Cs     ";
      (*log_Cs) << "   (Cs*hk)^2 ";
      (*log_Cs) << "    visceff  ";
      (*log_Cs) << &endl;
      (*log_Cs) << scientific;
      for (unsigned rr=0;rr<sumCs_->size();++rr)
      {
        (*log_Cs) << setw(11) << setprecision(4) << 0.5*((*nodeplanes_)[rr+1]+(*nodeplanes_)[rr]) << "  " ;
        (*log_Cs) << setw(11) << setprecision(4) << ((*sumCs_)[rr])/(numele_*numsamp_) << "  ";
        (*log_Cs) << setw(11) << setprecision(4) << ((*sumCs_delta_sq_)[rr])/(numele_*numsamp_)<< "  " ;
        (*log_Cs) << setw(11) << setprecision(4) << ((*sumvisceff_)[rr])/(numele_*numsamp_) << &endl;
      }
      log_Cs->flush();
    }

    if(subgrid_dissipation_)
    {
      Teuchos::RefCountPtr<std::ofstream> log_res;

      // output of residuals and subscale quantities
      std::string s_res = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
      s_res.append(".res_statistic");

      log_res = Teuchos::rcp(new std::ofstream(s_res.c_str(),ios::out));
      (*log_res) << "# Residual statistics for turbulent flow in a channel";
      (*log_res) << "\n\n\n";
      (*log_res) << "# Statistics record ";
      (*log_res) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";
      (*log_res) << "#       y    ";
      (*log_res) << "    res_x  ";
      (*log_res) << "      res_y  ";
      (*log_res) << "      res_z  ";
      (*log_res) << "     sacc_x  ";
      (*log_res) << "     sacc_y  ";
      (*log_res) << "     sacc_z   ";
      (*log_res) << "   res_sq_x  ";
      (*log_res) << "   res_sq_y  ";
      (*log_res) << "   res_sq_z  ";
      (*log_res) << "   sacc_sq_x ";
      (*log_res) << "   sacc_sq_y ";
      (*log_res) << "   sacc_sq_z " <<&endl;

      (*log_res) << scientific;
      for (unsigned rr=0;rr<nodeplanes_->size()-1;++rr)
      {
        (*log_res)  << setw(11) << setprecision(4) << 0.5*((*nodeplanes_)[rr+1]+(*nodeplanes_)[rr]) << "  " ;
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_)[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_)[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_)[3*rr+2]/(numele_*numsamp_) << "  ";
        
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_)[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_)[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_)[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumres_sq_)[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_sq_)[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumres_sq_)[3*rr+2]/(numele_*numsamp_) << "  ";

        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_sq_)[3*rr  ]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_sq_)[3*rr+1]/(numele_*numsamp_) << "  ";
        (*log_res)  << setw(11) << setprecision(4) << (*sumsacc_sq_)[3*rr+2]/(numele_*numsamp_) << "  ";
        
        (*log_res)  << &endl;
      }
      log_res->flush();
    } // end subgrid_dissipation_
  } // end myrank 0

  return;

}// TurbulenceStatisticsCha::DumpStatistics


/*----------------------------------------------------------------------*
 
     Compute a time average of the mean values for low-Mach-number
          flow over all steps of the sampling period so far. 
                      Dump the result to file.

 -----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::DumpLomaStatistics(int step)
{
  if (numsamp_ == 0) dserror("No samples to do time average");

  //----------------------------------------------------------------------
  // the sums are divided by the number of samples to get the time average
  int aux = numele_*numsamp_;

  //----------------------------------------------------------------------
  // evaluate area to calculate u_tau, l_tau (and tau_W)
  double area = 1.0;
  for (int i=0;i<3;i++)
  {
    if(i!=dim_) area*=((*boundingbox_)(1,i)-(*boundingbox_)(0,i));
  }
  // there are two Dirichlet boundaries
  area*=2;

  //----------------------------------------------------------------------
  // we expect nonzero forces (tractions) only in flow direction
  int flowdirection =0;

  // ltau is used to compute y+
  double ltau = 0;
  if      (sumforceu_>sumforcev_ && sumforceu_>sumforcew_)
  {
    flowdirection=0;
    ltau = visc_/sqrt(sumforceu_/(area*numsamp_));
  }
  else if (sumforcev_>sumforceu_ && sumforcev_>sumforcew_)
  {
    flowdirection=1;
    ltau = visc_/sqrt(sumforcev_/(area*numsamp_));
  }
  else if (sumforcew_>sumforceu_ && sumforcew_>sumforcev_)
  {
    flowdirection=2;
    ltau = visc_/sqrt(sumforcew_/(area*numsamp_));
  }
  else
    dserror("Cannot determine flow direction by traction (seems to be not unique)");

  //----------------------------------------------------------------------
  // output to log-file
  Teuchos::RefCountPtr<std::ofstream> log;
  if (discret_->Comm().MyPID()==0)
  {
    std::string s = params_.sublist("TURBULENCE MODEL").get<string>("statistics outfile");
    s.append(".loma_statistic");

    log = Teuchos::rcp(new std::ofstream(s.c_str(),ios::out));
    (*log) << "# Flow statistics for turbulent variable-density channel flow at low Mach number (first- and second-order moments)";
    (*log) << "\n\n\n";
    (*log) << "# Statistics record ";
    (*log) << " (Steps " << step-numsamp_+1 << "--" << step <<")\n";

    (*log) << "# (u_tau)^2 = tau_W/rho : ";
    (*log) << "   " << setw(11) << setprecision(4) << sumforceu_/(area*numsamp_);
    (*log) << "   " << setw(11) << setprecision(4) << sumforcev_/(area*numsamp_);
    (*log) << "   " << setw(11) << setprecision(4) << sumforcew_/(area*numsamp_);
    (*log) << &endl;

    (*log) << "#     y            y+";
    (*log) << "           umean         vmean         wmean         pmean         Tmean";
    (*log) << "        mean u^2      mean v^2      mean w^2";
    (*log) << "      mean u*v      mean u*w      mean v*w        Varp   \n";

    (*log) << scientific;
    for(unsigned i=0; i<planecoordinates_->size(); ++i)
    {
      (*log) <<  " "  << setw(11) << setprecision(4) << (*planecoordinates_)[i];
      (*log) << "   " << setw(11) << setprecision(4) << (*planecoordinates_)[i]/ltau;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumu_            )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumv_            )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumw_            )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sump_            )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqu_          )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqv_          )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqw_          )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumuv_           )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumuw_           )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumvw_           )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*sumsqp_          )[i]/aux;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumu_       )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumv_       )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumw_       )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsump_       )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumT_       )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqu_     )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqv_     )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqw_     )[i]/numsamp_;
      (*log) << "   " << setw(11) << setprecision(4) << (*pointsumsqp_     )[i]/numsamp_;
      (*log) << "   \n";
    }
    log->flush();
  }

  return;

}// TurbulenceStatisticsCha::DumpLomaStatistics


/*----------------------------------------------------------------------*

                  Reset sums and number of samples to 0

  ----------------------------------------------------------------------*/
void FLD::TurbulenceStatisticsCha::ClearStatistics()
{
  // reset the number of samples
  numsamp_ =0;

  // reset forces
  sumforceu_=0;
  sumforcev_=0;
  sumforcew_=0;

  // reset integral and pointwise averages
  for(unsigned i=0; i<planecoordinates_->size(); ++i)
  {
    (*sumu_)[i]  =0;
    (*sumv_)[i]  =0;
    (*sumw_)[i]  =0;
    (*sump_)[i]  =0;

    (*sumuv_ )[i]=0;
    (*sumuw_ )[i]=0;
    (*sumvw_ )[i]=0;
    (*sumsqu_)[i]=0;
    (*sumsqv_)[i]=0;
    (*sumsqw_)[i]=0;
    (*sumsqp_)[i]=0;

    (*pointsumu_)[i]  =0;
    (*pointsumv_)[i]  =0;
    (*pointsumw_)[i]  =0;
    (*pointsump_)[i]  =0;
    (*pointsumT_)[i]  =0;

    (*pointsumsqu_)[i]=0;
    (*pointsumsqv_)[i]=0;
    (*pointsumsqw_)[i]=0;
    (*pointsumsqp_)[i]=0;
    (*pointsumsqT_)[i]=0;

  }

  meanvelnp_->PutScalar(0.0);
  if (loma_ != "No") meanvedenp_->PutScalar(0.0);

  // reset smapling for dynamic Smagorinsky model
  if (smagorinsky_)
  {
    for (unsigned rr=0;rr<sumCs_->size();++rr)
    {
      // reset value
      (*sumCs_)         [rr]=0;
      (*sumCs_delta_sq_)[rr]=0;
      (*sumvisceff_)    [rr]=0;
    }
  } // end smagorinsky_

  // reset residuals and subscale averages
  if(subgrid_dissipation_)
  {
    for (unsigned rr=0;rr<sumres_->size()/3;++rr)
    {
      (*sumres_      )[3*rr  ]=0.0;
      (*sumres_      )[3*rr+1]=0.0;
      (*sumres_      )[3*rr+2]=0.0;

      (*sumsacc_     )[3*rr  ]=0.0;
      (*sumsacc_     )[3*rr+1]=0.0;
      (*sumsacc_     )[3*rr+2]=0.0;
      
      (*sumsvelaf_   )[3*rr  ]=0.0;
      (*sumsvelaf_   )[3*rr+1]=0.0;
      (*sumsvelaf_   )[3*rr+2]=0.0;

      (*sumres_sq_   )[3*rr  ]=0.0;
      (*sumres_sq_   )[3*rr+1]=0.0;
      (*sumres_sq_   )[3*rr+2]=0.0;

      (*sumsacc_sq_  )[3*rr  ]=0.0;
      (*sumsacc_sq_  )[3*rr+1]=0.0;
      (*sumsacc_sq_  )[3*rr+2]=0.0;

      (*sumsvelaf_sq_)[3*rr  ]=0.0;
      (*sumsvelaf_sq_)[3*rr+1]=0.0;
      (*sumsvelaf_sq_)[3*rr+2]=0.0;
    }
    for (unsigned rr=0;rr<sumresC_->size();++rr)
    {
      (*sumtauM_         )[rr]=0.0;
      (*sumtauC_         )[rr]=0.0;
      
      (*sum_eps_sacc_    )[rr]=0.0;
      (*sum_eps_pspg_    )[rr]=0.0;
      (*sum_eps_supg_    )[rr]=0.0;
      (*sum_eps_cross_   )[rr]=0.0;
      (*sum_eps_rey_     )[rr]=0.0;
      (*sum_eps_cstab_   )[rr]=0.0;
      (*sum_eps_vstab_   )[rr]=0.0;
      (*sum_eps_eddyvisc_)[rr]=0.0;
      (*sum_eps_visc_    )[rr]=0.0;
      (*sum_eps_conv_    )[rr]=0.0;

      (*sumresC_         )[rr]=0.0;
      (*sumspressnp_     )[rr]=0.0;
      
      (*sumresC_sq_      )[rr]=0.0;
      (*sumspressnp_sq_  )[rr]=0.0;
    }
  } // end subgrid_dissipation_

  return;
}// TurbulenceStatisticsCha::ClearStatistics



#endif /* CCADISCRET       */
