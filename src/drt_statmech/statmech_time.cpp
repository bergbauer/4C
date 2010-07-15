/*!----------------------------------------------------------------------
\file statmech.cpp
\brief time integration for structural problems with statistical mechanics

<pre>
Maintainer: Christian Cyron
            cyron@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15234
</pre>

*----------------------------------------------------------------------*/
#ifdef CCADISCRET

#include <Teuchos_Time.hpp>

#include "statmech_time.H"
#include "../drt_statmech/statmech_manager.H"
#include "../drt_inpar/inpar_statmech.H"
#include "../drt_io/io_control.H"
#include "../drt_constraint/constraint_manager.H"
#include "../drt_constraint/constraintsolver.H"

#include <random/normal.h>

#ifdef D_BEAM3
#include "../drt_beam3/beam3.H"
#endif  // #ifdef D_BEAM3
#ifdef D_BEAM3II
#include "../drt_beam3ii/beam3ii.H"
#endif  // #ifdef D_BEAM3II
#ifdef D_BEAM2
#include "../drt_beam2/beam2.H"
#endif  // #ifdef D_BEAM2
#ifdef D_BEAM2R
#include "../drt_beam2r/beam2r.H"
#endif  // #ifdef D_BEAM2R
#ifdef D_TRUSS3
#include "../drt_truss3/truss3.H"
#endif  // #ifdef D_TRUSS3
#ifdef D_TRUSS2
#include "../drt_truss2/truss2.H"
#endif  // #ifdef D_TRUSS2


#define MEASURETIME
/*----------------------------------------------------------------------*
 |  ctor (public)                                             cyron 08/08|
 *----------------------------------------------------------------------*/
StatMechTime::StatMechTime(ParameterList& params,
                          DRT::Discretization& dis,
                          LINALG::Solver& solver,
                          IO::DiscretizationWriter& output) :
StruGenAlpha(params,dis,solver,output),
isconverged_(0),
unconvergedsteps_(0),
isinit_(false),
deltadbc_(LINALG::CreateVector(*(discret_.DofRowMap()),true))
{
  Teuchos::RCP<LINALG::SparseMatrix> stiff = SystemMatrix();
  statmechmanager_ = rcp(new StatMechManager(params,dis));

  //maximal number of random numbers to be generated per time step for any column map element of this processor
  int randomnumbersperlocalelement = 0;

  /*check maximal number of nodes of an element with stochastic forces on this processor*/
  for (int i=0; i<  dis.NumMyColElements(); ++i)
  {
    const DRT::ElementType & eot = dis.lColElement(i)->ElementType();
    /*stochastic forces implemented so far only for the following elements:*/
#ifdef D_BEAM3
    if ( eot == DRT::ELEMENTS::Beam3Type::Instance() )
      {
        //see whether current element needs more random numbers per time step than any other before
        randomnumbersperlocalelement = max(randomnumbersperlocalelement,dynamic_cast<DRT::ELEMENTS::Beam3*>(dis.lColElement(i))->HowManyRandomNumbersINeed());

        //in case of periodic boundary conditions beam3 elements require a special initialization if they are broken by the periodic boundaries in the initial configuration
        if(statmechmanager_->statmechparams_.get<double>("PeriodLength",0.0) > 0.0)
          statmechmanager_->PeriodicBoundaryBeam3Init(dis.lColElement(i));
      }
    else
#endif  // #ifdef D_BEAM3
#ifdef D_BEAM3II
      if ( eot == DRT::ELEMENTS::Beam3iiType::Instance() )
      {
        //see whether current element needs more random numbers per time step than any other before
        randomnumbersperlocalelement = max(randomnumbersperlocalelement,dynamic_cast<DRT::ELEMENTS::Beam3ii*>(dis.lColElement(i))->HowManyRandomNumbersINeed());

        //in case of periodic boundary conditions beam3 elements require a special initialization if they are broken by the periodic boundaries in the initial configuration
        if(statmechmanager_->statmechparams_.get<double>("PeriodLength",0.0) > 0.0)
          statmechmanager_->PeriodicBoundaryBeam3iiInit(dis.lColElement(i));
      }
    else
#endif  // #ifdef D_BEAM3II
#ifdef D_BEAM2
      if ( eot == DRT::ELEMENTS::Beam2Type::Instance() )
      {
        //see whether current element needs more random numbers per time step than any other before
        randomnumbersperlocalelement = max(randomnumbersperlocalelement,dynamic_cast<DRT::ELEMENTS::Beam2*>(dis.lColElement(i))->HowManyRandomNumbersINeed());
      }
    else
#endif  // #ifdef D_BEAM2
#ifdef D_BEAM2R
      if ( eot == DRT::ELEMENTS::Beam2rType::Instance() )
      {
        //see whether current element needs more random numbers per time step than any other before
        randomnumbersperlocalelement = max(randomnumbersperlocalelement,dynamic_cast<DRT::ELEMENTS::Beam2r*>(dis.lColElement(i))->HowManyRandomNumbersINeed());
      }
    else
#endif  // #ifdef D_BEAM2R
#ifdef D_TRUSS3
      if ( eot == DRT::ELEMENTS::Truss3Type::Instance() )
      {
        //see whether current element needs more random numbers per time step than any other before
        randomnumbersperlocalelement = max(randomnumbersperlocalelement,dynamic_cast<DRT::ELEMENTS::Truss3*>(dis.lColElement(i))->HowManyRandomNumbersINeed());

        //in case of periodic boundary conditions truss3 elements require a special initialization if they are broken by the periodic boundaries in the initial configuration
        if(statmechmanager_->statmechparams_.get<double>("PeriodLength",0.0) > 0.0)
          statmechmanager_->PeriodicBoundaryTruss3Init(dis.lColElement(i));
      }
      else
#endif  // #ifdef D_TRUSS3
        continue;
  } //for (int i=0; i<dis_.NumMyColElements(); ++i)

  /*so far the maximal number of random numbers required per element has been checked only locally on this processor;
   *now we compare the results of each processor and store the maximal one in maxrandomnumbersperglobalelement_*/
  dis.Comm().MaxAll(&randomnumbersperlocalelement,&maxrandomnumbersperglobalelement_ ,1);

  return;
} // StatMechTime::StatMechTime


/*----------------------------------------------------------------------*
 |  integrate in time          (static/public)               cyron 08/08|
 *----------------------------------------------------------------------*/
void StatMechTime::Integrate()
{
  int    step    = params_.get<int>   ("step" ,0);
  int    nstep   = params_.get<int>   ("nstep",5);
  double maxtime = params_.get<double>("max time",0.0);
  // can have values "full newton" , "modified newton" , "nonlinear cg"
  string equil = params_.get<string>("equilibrium iteration","full newton");

  // can have values takes values "constant" consistent"
  string pred  = params_.get<string>("predictor","constant");
  int predictor=-1;
  if      (pred=="constant")   predictor = 1;
  else if (pred=="consistent") predictor = 2;
  else dserror("Unknown type of predictor");


  double dt = params_.get<double>("delta time" ,0.01);

  //getting number of dimensions for diffusion coefficient calculation
  const Teuchos::ParameterList& psize = DRT::Problem::Instance()->ProblemSizeParams();
  int ndim=psize.get<int>("DIM");


  for (int i=step; i<nstep; ++i)
  {
    //if input flag is set random number seed should be the same for all realizations
    if(Teuchos::getIntegralValue<int>(statmechmanager_->statmechparams_,"FIXEDSEED"))
    {
      //random generator for seeding only (necessary for thermal noise)
      ranlib::Normal<double> seedgenerator(0,1);
      //seeding random generator
      int seedvariable = i;
      seedgenerator.seed((unsigned int)seedvariable);
    }


    /*in the very first step and in case that special output for statistical mechanics is requested we have
     * to initialized the related output method*/
    if(i == 0)
      statmechmanager_->StatMechInitOutput(ndim,dt);

    //processor 0 write total number of elements at the beginning of time step i to console as well as how often a time step had to be restarted due to bad random numbers
    if(!discret_.Comm().MyPID())
    {
      std::cout<<"\nNumber of elements at the beginning of time step "<<i<<" : "<<discret_.NumGlobalElements()<<"\n";
      std::cout<<"\nNumber of unconverged steps "<<unconvergedsteps_<<"\n";
    }

			//time_ is time at the end of this time step
      double time = params_.get<double>("total time",0.0);
      if(time + statmechmanager_->statmechparams_.get<double>("DELTA_T_NEW",dt) > statmechmanager_->statmechparams_.get<double>("STARTTIME", 0.0))
      {
      	if(statmechmanager_->statmechparams_.get<double>("DELTA_T_NEW",dt)>0.0)
      	{
      		dt = statmechmanager_->statmechparams_.get<double>("DELTA_T_NEW",dt);
      		params_.set("delta time", dt);
      	}
      }

      statmechmanager_->time_ = time + dt;

      do
      {
        //assuming that iterations will converge
        isconverged_ = 1;

        /*multivector for stochastic forces evaluated by each element; the numbers of vectors in the multivector equals the maximal
         *number of random numbers required by any element in the discretization per time step; therefore this multivector is suitable
         *for synchrinisation of these random numbers in parallel computing*/
        RCP<Epetra_MultiVector> randomnumbers = rcp( new Epetra_MultiVector(*(discret_.ElementColMap()),maxrandomnumbersperglobalelement_) );

        //pay attention: for a constant predictor an incremental velocity update is necessary, which has
        //been deleted out of the code in oder to simplify it

        //generate gaussian random numbers for parallel use with mean value 0 and standard deviation (2KT / dt)0.5
        statmechmanager_->GenerateGaussianRandomNumbers(randomnumbers,0,pow(2.0 * (statmechmanager_->statmechparams_).get<double>("KT",0.0) / dt,0.5));

        /*/test: StructPolyMorphOutput in initial configuration
        if(time==0.0)
        {
					std::ostringstream tmpfilename;
					tmpfilename <<"./GmshOutput/InitStructPolymorph_"<<discret_.Comm().MyPID()<<".dat";
					statmechmanager_->StructPolymorphOutput(*dis_, tmpfilename);
        }*/

        ConsistentPredictor(randomnumbers);


        if(ndim ==3)
          PTC(randomnumbers);
        else
          FullNewton(randomnumbers);

        /*if iterations have not converged a new trial requires setting all intern element variables to
         * status at the beginning of this time step*/
        if(isconverged_ == 0)
        {
          ParameterList p;
          p.set("action","calc_struct_reset_istep");
          discret_.Evaluate(p,null,null,null,null,null);
        }

      }
      while(isconverged_ == 0);

      const double t_admin = Teuchos::Time::wallTime();

      UpdateandOutput();

      /*special update for statistical mechanics; this output has to be handled separately from the time integration scheme output
       * as it may take place independently on writing geometric output data in a specific time step or not*/
      statmechmanager_->StatMechUpdate(dt,*dis_,stiff_,ndim);

      statmechmanager_->StatMechOutput(params_,ndim,time,i,dt,*dis_,*fint_);

      if(!discret_.Comm().MyPID())
      cout << "\n***\ntotal administration time: " << Teuchos::Time::wallTime() - t_admin<< " seconds\n***\n";

      if (time>=maxtime) break;
  }


  return;
} // void StatMechTime::Integrate()


/*----------------------------------------------------------------------*
 |do consistent predictor step for Brownian dynamics (public)cyron 10/09|
 *----------------------------------------------------------------------*/
void StatMechTime::ConsistentPredictor(RCP<Epetra_MultiVector> randomnumbers)
{
  // -------------------------------------------------------------------
  // get some parameters from parameter list
  // -------------------------------------------------------------------
  double time        = params_.get<double>("total time"     ,0.0);
  double dt          = params_.get<double>("delta time"     ,0.01);
  double alphaf      = params_.get<double>("alpha f"        ,0.459);
  bool   printscreen = params_.get<bool>  ("print to screen",false);
  string convcheck   = params_.get<string>("convcheck"      ,"AbsRes_Or_AbsDis");
  bool   dynkindstat = (params_.get<string>("DYNAMICTYP") == "Static");

  // store norms of old displacements and maximum of norms of
  // internal, external and inertial forces if a relative convergence
  // check is desired
  if (!firststep_ and (convcheck != "AbsRes_And_AbsDis" or convcheck != "AbsRes_Or_AbsDis"))
    CalcRefNorms();

  // increment time and step
  double timen = time + dt;  // t_{n+1}
  //int istep = step + 1;  // n+1

  /*special part for STATMECH: initialize disn_ and veln_ with zero; this is necessary only for the following case:
   * assume that an iteration step did not converge and is repeated with new random numbers; if the failure of conver
   * gence lead to disn_ = NaN and veln_ = NaN this would affect also the next trial as e.g. disn_->Update(1.0,*dis_,0.0);
   * would set disn_ to NaN as even 0*NaN = NaN!; this would defeat the purpose of the repeated iterations with new
   * random numbers and has thus to be avoided; therefore we initialized disn_ and veln_ with zero which has no effect
   * in any other case*/
  disn_->PutScalar(0.0);
  veln_->PutScalar(0.0);
  dism_->PutScalar(0.0);
  velm_->PutScalar(0.0);
  fresm_->PutScalar(0.0);

  //consistent predictor for backward Euler time integration scheme
  disn_->Update(1.0,*dis_,0.0);
  veln_->Update(1.0/dt,*disn_,-1.0/dt,*dis_,0.0);

  //evaluate deterministic external forces
  {
    ParameterList p;
    // action for elements
    p.set("action","calc_struct_eleload");
    // other parameters needed by the elements
    p.set("total time",timen);
    p.set("delta time",dt);
    p.set("alpha f",alphaf);

    // set vector values needed by elements
    discret_.ClearState();
    discret_.SetState("displacement",disn_);
    discret_.SetState("velocity",veln_);
    // predicted dirichlet values
    // disn then also holds prescribed new dirichlet displacements

    // determine evaluation mode
    if(statmechmanager_->statmechparams_.get<double>("PeriodLength",0.0) <= 0.0 &&
    	 Teuchos::getIntegralValue<int>(statmechmanager_->statmechparams_,"PERIODICDBC"))
    	dserror("Set PeriodLength > 0.0 if periodic DBCs are to be applied");
    if(statmechmanager_->statmechparams_.get<double>("PeriodLength",0.0) > 0.0 &&
    	 Teuchos::getIntegralValue<int>(statmechmanager_->statmechparams_,"CONVENTIONALDBC"))
    	dserror("Set PeriodLength to Zero if conventional DBCs are to be applied");
    // in case of activated periodic boundary conditions
    if(Teuchos::getIntegralValue<int>(statmechmanager_->statmechparams_,"PERIODICDBC"))
    {
    	/* Reinitialize disn_ and dirichtoggle_ once.
    	 * Now, why is this done? For t==0, disn_ and dirichtoggle_ are initialized in strugenalpha.cpp.
    	 * Especially dirichtoggle_ and invtoggle_ contain information that is incorrect if DBC DOFs are
    	 * selected anew for each timestep and periodic boundary conditions are to be applied.
    	 * Also, initialize deltadbc_ with large abs. values
    	 * The "incorrect" initialization occurs due to DBCs defined in the input file.
    	 * One may interject, that they've been defined for a reason, so why cancel them here?
    	 * answer: reasons of flexibility.
    	 * If those conventional DBCs are really needed they can be readded later.
    	 */
    	if(!isinit_)
    	{
    		for(int i=0;i<disn_->MyLength();i++)
    		  (*disn_)[i] = 0.0;
    		for(int i=0;i<dirichtoggle_->MyLength();i++)
    		  (*dirichtoggle_)[i] = 0.0;
    		for(int i=0;i<invtoggle_->MyLength();i++)
    		    		  (*invtoggle_)[i] = 1.0;
    	}
    	EvaluateDirichletPeriodic(p);
    	//cout<<"disn_ post DBC Eval: \n"<<*disn_<<endl;
    }
		// "common" case without periodic boundary conditions
    if(Teuchos::getIntegralValue<int>(statmechmanager_->statmechparams_,"CONVENTIONALDBC") )
    {
    	discret_.EvaluateDirichlet(p,disn_,null,null,dirichtoggle_);
			//cout<<"Conventional: \n"<<*dirichtoggle_<<endl;
    }

    discret_.ClearState();
    discret_.SetState("displacement",disn_);
    discret_.SetState("velocity",veln_);
    fextn_->PutScalar(0.0);  // initialize external force vector (load vect)
    discret_.EvaluateNeumann(p,*fextn_);
    discret_.ClearState();
  }


#ifdef STRUGENALPHA_STRONGDBC
  // apply new velocities at DBCs
  {
    ParameterList p;
    // action for elements
    p.set("action","calc_struct_eleload");
    // other parameters needed by the elements
    p.set("total time",timen);
    p.set("delta time",dt);
    p.set("alpha f",alphaf);
    //p.set("time derivative degree",1);  // we want velocities
    // set vector values needed by elements
    discret_.ClearState();
    discret_.SetState("velocity",veln_);
    // predicted dirichlet values
    // veln_ then also holds prescribed new Dirichlet velocities
    discret_.EvaluateDirichlet(p,null,veln_,null,dirichtoggle_);
    discret_.ClearState();
  }
#endif


  //------------------------------ compute interpolated dis, vel and acc
  // consistent predictor
  // mid-displacements D_{n+1-alpha_f} (dism)
  //    D_{n+1-alpha_f} := (1.-alphaf) * D_{n+1} + alpha_f * D_{n}
  dism_->Update(1.-alphaf,*disn_,alphaf,*dis_,0.0);
  // mid-velocities V_{n+1-alpha_f} (velm)
  //    V_{n+1-alpha_f} := (1.-alphaf) * V_{n+1} + alpha_f * V_{n}
  velm_->Update(1.-alphaf,*veln_,alphaf,*vel_,0.0);


  // zerofy velocity and acceleration in case of statics
  if (dynkindstat)
  {
    velm_->PutScalar(0.0);
    veln_->PutScalar(0.0);
    vel_->PutScalar(0.0);
  }

  //------------------------------- compute interpolated external forces
  // external mid-forces F_{ext;n+1-alpha_f} (fextm)
  //    F_{ext;n+1-alpha_f} := (1.-alphaf) * F_{ext;n+1}
  //                         + alpha_f * F_{ext;n}
  fextm_->Update(1.-alphaf,*fextn_,alphaf,*fext_,0.0);

  //------------- eval fint at interpolated state, eval stiffness matrix
  {
    // zero out stiffness
    stiff_->Zero();

    // create the parameters for the discretization
    ParameterList p;
    // action for elements
    p.set("action","calc_struct_nlnstiff");
    // other parameters that might be needed by the elements
    p.set("total time",timen);
    p.set("delta time",dt);
    p.set("alpha f",alphaf);


    //passing statistical mechanics parameters to elements
    p.set("ETA",(statmechmanager_->statmechparams_).get<double>("ETA",0.0));
    p.set("THERMALBATH",Teuchos::getIntegralValue<INPAR::STATMECH::ThermalBathType>(statmechmanager_->statmechparams_,"THERMALBATH"));
    p.set("FRICTION_MODEL",Teuchos::getIntegralValue<INPAR::STATMECH::FrictionModel>(statmechmanager_->statmechparams_,"FRICTION_MODEL"));
    p.set("RandomNumbers",randomnumbers);
    p.set("SHEARAMPLITUDE",(statmechmanager_->statmechparams_).get<double>("SHEARAMPLITUDE",0.0));
    p.set("CURVENUMBER",(statmechmanager_->statmechparams_).get<int>("CURVENUMBER",-1));
    p.set("OSCILLDIR",(statmechmanager_->statmechparams_).get<int>("OSCILLDIR",-1));
    p.set("STARTTIME",(statmechmanager_->statmechparams_).get<double>("STARTTIME",0.0));
    p.set("DELTA_T_NEW",(statmechmanager_->statmechparams_).get<double>("DELTA_T_NEW",0.0));
    p.set("PeriodLength",(statmechmanager_->statmechparams_).get<double>("PeriodLength",0.0));


    // set vector values needed by elements
    discret_.ClearState();
    disi_->PutScalar(0.0);
    discret_.SetState("residual displacement",disi_);

    discret_.SetState("displacement",dism_);
    discret_.SetState("velocity",velm_);

    //discret_.SetState("velocity",velm_); // not used at the moment

    fint_->PutScalar(0.0);  // initialise internal force vector

    p.set("action","calc_struct_nlnstiff");
    discret_.Evaluate(p,stiff_,null,fint_,null,null);
    discret_.ClearState();

    // do NOT finalize the stiffness matrix, add mass and damping to it later
  }

  //-------------------------------------------- compute residual forces
  // build residual
  if (dynkindstat)
  {
    // static residual
    // Res = F_int - F_ext
    fresm_->PutScalar(0.0);
  }
  else
  {
    // dynamic residual
    // Res = M . A_{n+1-alpha_m}
    //     + C . V_{n+1-alpha_f}
    //     + F_int(D_{n+1-alpha_f})
    //     - F_{ext;n+1-alpha_f}
    // add mid-inertial force

  }
  // add static mid-balance

  fresm_->Update(-1.0,*fint_,1.0,*fextm_,0.0);



  // blank residual at DOFs on Dirichlet BC
  {
    Epetra_Vector fresmcopy(*fresm_);
    fresm_->Multiply(1.0,*invtoggle_,fresmcopy,0.0);
  }

  //------------------------------------------------ build residual norm
  double fresmnorm = 1.0;

  // store norms of displacements and maximum of norms of internal,
  // external and inertial forces if a relative convergence check
  // is desired and we are in the first time step (possibly after a
  // restart)
  if (firststep_ and (convcheck != "AbsRes_And_AbsDis" or convcheck != "AbsRes_Or_AbsDis"))
  {
    CalcRefNorms();
    firststep_=false;
  }

  if (printscreen)
    fresm_->Norm2(&fresmnorm);

  if (!myrank_ and printscreen)
  {
    PrintPredictor(convcheck, fresmnorm);
  }

  return;
} //StatMechTime::ConsistentPredictor()


/*----------------------------------------------------------------------*
 |  do Newton iteration (public)                             mwgee 03/07|
 *----------------------------------------------------------------------*/
void StatMechTime::FullNewton(RCP<Epetra_MultiVector> randomnumbers)
{
  // -------------------------------------------------------------------
  // get some parameters from parameter list
  // -------------------------------------------------------------------
  double time      = params_.get<double>("total time"             ,0.0);
  double dt        = params_.get<double>("delta time"             ,0.01);
  double timen     = time + dt;
  int    maxiter   = params_.get<int>   ("max iterations"         ,10);
  double alphaf    = params_.get<double>("alpha f"                ,0.459);
  string convcheck = params_.get<string>("convcheck"              ,"AbsRes_Or_AbsDis");
  double toldisp   = params_.get<double>("tolerance displacements",1.0e-07);
  double tolres    = params_.get<double>("tolerance residual"     ,1.0e-07);
  bool printscreen = params_.get<bool>  ("print to screen",true);
  bool printerr    = params_.get<bool>  ("print to err",false);
  FILE* errfile    = params_.get<FILE*> ("err file",NULL);
  if (!errfile) printerr = false;

  //------------------------------ turn adaptive solver tolerance on/off
  const bool   isadapttol    = params_.get<bool>("ADAPTCONV",true);
  const double adaptolbetter = params_.get<double>("ADAPTCONV_BETTER",0.01);


#ifndef STRUGENALPHA_BE
  //double delta = beta;
#endif

  //=================================================== equilibrium loop
  int numiter=0;
  double fresmnorm = 1.0e6;
  double disinorm = 1.0e6;
  fresm_->Norm2(&fresmnorm);
  Epetra_Time timer(discret_.Comm());
  timer.ResetStartTime();
  bool print_unconv = true;

  while (!Converged(convcheck, disinorm, fresmnorm, toldisp, tolres) and numiter<=maxiter)
  {


    //------------------------------------------- effective rhs is fresm
    //---------------------------------------------- build effective lhs
    //stiff_->Add(*damp_,false,(1.-alphaf)*gamma/(delta*dt),1.0);
    //stiff_->Complete();

    //backward Euler
    stiff_->Complete();

    //----------------------- apply dirichlet BCs to system of equations
    disi_->PutScalar(0.0);  // Useful? depends on solver and more
    LINALG::ApplyDirichlettoSystem(stiff_,disi_,fresm_,zeros_,dirichtoggle_);


    //--------------------------------------------------- solve for disi
    // Solve K_Teffdyn . IncD = -R  ===>  IncD_{n+1}
    if (isadapttol && numiter)
    {
      double worst = fresmnorm;
      double wanted = tolres;
      solver_.AdaptTolerance(wanted,worst,adaptolbetter);
    }
    solver_.Solve(stiff_->EpetraOperator(),disi_,fresm_,true,numiter==0);
    solver_.ResetTolerance();


    //---------------------------------- update mid configuration values
    // displacements
    // D_{n+1-alpha_f} := D_{n+1-alpha_f} + (1-alpha_f)*IncD_{n+1}

    dism_->Update(1.-alphaf,*disi_,1.0);
    disn_->Update(1.0,*disi_,1.0);

    // velocities

    // incremental (required for constant predictor)

    //backward Euler
    velm_->Update(1.0/dt,*dism_,-1.0/dt,*dis_,0.0);

    //velm_->Update(1.0,*dism_,-1.0,*dis_,0.0);
    //velm_->Update((delta-(1.0-alphaf)*gamma)/delta,*vel_,gamma/(delta*dt));


    //---------------------------- compute internal forces and stiffness
    {
      // zero out stiffness
      stiff_->Zero();
      // create the parameters for the discretization
      ParameterList p;
      // action for elements
      p.set("action","calc_struct_nlnstiff");
      // other parameters that might be needed by the elements
      p.set("total time",timen);
      p.set("delta time",dt);
      p.set("alpha f",alphaf);

      //passing statistical mechanics parameters to elements
      p.set("ETA",(statmechmanager_->statmechparams_).get<double>("ETA",0.0));
      p.set("THERMALBATH",Teuchos::getIntegralValue<INPAR::STATMECH::ThermalBathType>(statmechmanager_->statmechparams_,"THERMALBATH"));
      p.set("FRICTION_MODEL",Teuchos::getIntegralValue<INPAR::STATMECH::FrictionModel>(statmechmanager_->statmechparams_,"FRICTION_MODEL"));
      p.set("RandomNumbers",randomnumbers);
      p.set("SHEARAMPLITUDE",(statmechmanager_->statmechparams_).get<double>("SHEARAMPLITUDE",0.0));
      p.set("CURVENUMBER",(statmechmanager_->statmechparams_).get<int>("CURVENUMBER",-1));
      p.set("STARTTIME",(statmechmanager_->statmechparams_).get<double>("STARTTIME",0.0));
      p.set("DELTA_T_NEW",(statmechmanager_->statmechparams_).get<double>("DELTA_T_NEW",0.0));
      p.set("OSCILLDIR",(statmechmanager_->statmechparams_).get<int>("OSCILLDIR",-1));
      p.set("PeriodLength",(statmechmanager_->statmechparams_).get<double>("PeriodLength",0.0));


      // set vector values needed by elements
      discret_.ClearState();

      // scale IncD_{n+1} by (1-alphaf) to obtain mid residual displacements IncD_{n+1-alphaf}
      disi_->Scale(1.-alphaf);

      discret_.SetState("residual displacement",disi_);

      discret_.SetState("displacement",dism_);
      discret_.SetState("velocity",velm_);

      //discret_.SetState("velocity",velm_); // not used at the moment

      fint_->PutScalar(0.0);  // initialise internal force vector
      discret_.Evaluate(p,stiff_,null,fint_,null,null);

      discret_.ClearState();

      // do NOT finalize the stiffness matrix to add masses to it later
    }

    //------------------------------------------ compute residual forces

    // dynamic residual
    // Res =  C . V_{n+1-alpha_f}
    //        + F_int(D_{n+1-alpha_f})
    //        - F_{ext;n+1-alpha_f}
    // add mid-inertial force


    //RefCountPtr<Epetra_Vector> fviscm = LINALG::CreateVector(*dofrowmap,true);
    fresm_->Update(-1.0,*fint_,1.0,*fextm_,0.0);


    // blank residual DOFs that are on Dirichlet BC
    {
      Epetra_Vector fresmcopy(*fresm_);
      fresm_->Multiply(1.0,*invtoggle_,fresmcopy,0.0);
    }

    //---------------------------------------------- build residual norm
    disi_->Norm2(&disinorm);
    fresm_->Norm2(&fresmnorm);



    //if code is compiled with DEBUG flag each iteration is written into file for Gmsh visualization
#ifdef DEBUG
    // first index = time step index
    std::ostringstream filename;

    //creating complete file name dependent on step number with 5 digits and leading zeros
    if (numiter<100000)
      filename << "./GmshOutput/konvergenz"<< std::setw(5) << setfill('0') << numiter <<".pos";
    else
      dserror("Gmsh output implemented for a maximum of 99999 steps");

    //statmechmanager_->GmshOutput(*dism_,filename,numiter);
#endif  // #ifdef DEBUG


    // a short message
    if (!myrank_ and (printscreen or printerr))
    {
      PrintNewton(printscreen,printerr,print_unconv,errfile,timer,numiter,maxiter,
                  fresmnorm,disinorm,convcheck);
    }

    //--------------------------------- increment equilibrium loop index
    ++numiter;

  }
  //=============================================== end equilibrium loop
  print_unconv = false;

  //-------------------------------- test whether max iterations was hit
  //if on convergence arises within maxiter iterations the time step is restarted with new random numbers
  if (numiter>=maxiter)
  {
    isconverged_ = 0;
    unconvergedsteps_++;
    std::cout<<"\n\niteration unconverged - new trial with new random numbers!\n\n";
     //dserror("PTC unconverged in %d iterations",numiter);
  }
  else if(!myrank_ and printscreen)
  {
    PrintNewton(printscreen,printerr,print_unconv,errfile,timer,numiter,maxiter,
                fresmnorm,disinorm,convcheck);
  }


  params_.set<int>("num iterations",numiter);

  return;
} // StatMechTime::FullNewton()

/*----------------------------------------------------------------------*
 |  do Newton iteration (public)                             mwgee 03/07|
 *----------------------------------------------------------------------*/
void StatMechTime::PTC(RCP<Epetra_MultiVector> randomnumbers)
{
  // -------------------------------------------------------------------
  // get some parameters from parameter list
  // -------------------------------------------------------------------
  double time      = params_.get<double>("total time"             ,0.0);
  double dt        = params_.get<double>("delta time"             ,0.01);
  double timen     = time + dt;
  int    maxiter   = params_.get<int>   ("max iterations"         ,10);
  double alphaf    = params_.get<double>("alpha f"                ,0.459);
  string convcheck = params_.get<string>("convcheck"              ,"AbsRes_Or_AbsDis");
  double toldisp   = params_.get<double>("tolerance displacements",1.0e-07);
  double tolres    = params_.get<double>("tolerance residual"     ,1.0e-07);
  bool printscreen = params_.get<bool>  ("print to screen",true);
  bool printerr    = params_.get<bool>  ("print to err",false);
  FILE* errfile    = params_.get<FILE*> ("err file",NULL);

  #ifndef STRUGENALPHA_BE
    //double delta = beta;
  #endif


  double sumsolver     = 0;
  double sumevaluation = 0;
  double sumptc = 0;
  const double tbegin = Teuchos::Time::wallTime();

  if (!errfile) printerr = false;
  //------------------------------ turn adaptive solver tolerance on/off
  const bool   isadapttol    = params_.get<bool>("ADAPTCONV",true);
  const double adaptolbetter = params_.get<double>("ADAPTCONV_BETTER",0.01);

  const bool   dynkindstat = (params_.get<string>("DYNAMICTYP") == "Static");
  if (dynkindstat) dserror("Static case not implemented");


  // hard wired ptc parameters
  double ptcdt = 1.3e1; //1.3e1
  double nc;
  fresm_->NormInf(&nc);
  double dti = 1/ptcdt;
  double dti0 = dti;
  RCP<Epetra_Vector> x0 = rcp(new Epetra_Vector(*disi_));

  double resinit = nc;


  //=================================================== equilibrium loop
  int numiter=0;
  double fresmnorm = 1.0e6;
  double disinorm = 1.0e6;
  fresm_->Norm2(&fresmnorm);
  Epetra_Time timer(discret_.Comm());
  timer.ResetStartTime();
  bool print_unconv = true;

  while (!Converged(convcheck, disinorm, fresmnorm, toldisp, tolres) and numiter<=maxiter)
  {


#if 1 // SER
#else // TTE
    double dtim = dti0;
#endif

    dti0 = dti;
    RCP<Epetra_Vector> xm = rcp(new Epetra_Vector(*x0));
    x0->Update(1.0,*disi_,0.0);

    //backward Euler
    stiff_->Complete();

    //the following part was especially introduced for Brownian dynamics
    {
      const double t_ptc = Teuchos::Time::wallTime();

      // create the parameters for the discretization
      ParameterList p;

      p.set("action","calc_struct_ptcstiff");
      p.set("delta time",dt);
      p.set("dti",dti);

      //add statistical vector to parameter list for statistical forces and damping matrix computation
      p.set("ETA",(statmechmanager_->statmechparams_).get<double>("ETA",0.0));
      p.set("THERMALBATH",Teuchos::getIntegralValue<INPAR::STATMECH::ThermalBathType>(statmechmanager_->statmechparams_,"THERMALBATH"));
      p.set("FRICTION_MODEL",Teuchos::getIntegralValue<INPAR::STATMECH::FrictionModel>(statmechmanager_->statmechparams_,"FRICTION_MODEL"));
      p.set("SHEARAMPLITUDE",(statmechmanager_->statmechparams_).get<double>("SHEARAMPLITUDE",0.0));
      p.set("CURVENUMBER",(statmechmanager_->statmechparams_).get<int>("CURVENUMBER",-1));
      p.set("OSCILLDIR",(statmechmanager_->statmechparams_).get<int>("OSCILLDIR",-1));
      p.set("PeriodLength",(statmechmanager_->statmechparams_).get<double>("PeriodLength",0.0));

      //evaluate ptc stiffness contribution in all the elements


      discret_.Evaluate(p,stiff_,null,null,null,null);
      sumptc += Teuchos::Time::wallTime() - t_ptc;



    }


    //----------------------- apply dirichlet BCs to system of equations
    disi_->PutScalar(0.0);  // Useful? depends on solver and more
    LINALG::ApplyDirichlettoSystem(stiff_,disi_,fresm_,zeros_,dirichtoggle_);

    //--------------------------------------------------- solve for disi
    const double t_solver = Teuchos::Time::wallTime();
    // Solve K_Teffdyn . IncD = -R  ===>  IncD_{n+1}
    if (isadapttol && numiter)
    {
      double worst = fresmnorm;
      double wanted = tolres;
      solver_.AdaptTolerance(wanted,worst,adaptolbetter);
    }
    solver_.Solve(stiff_->EpetraOperator(),disi_,fresm_,true,numiter==0);
    solver_.ResetTolerance();

    sumsolver += Teuchos::Time::wallTime() - t_solver;



    //---------------------------------- update mid configuration values
    // displacements
    // D_{n+1-alpha_f} := D_{n+1-alpha_f} + (1-alpha_f)*IncD_{n+1}

    dism_->Update(1.-alphaf,*disi_,1.0);
    disn_->Update(1.0,*disi_,1.0);

    // velocities

    //backward Euler
    // incremental (required for constant predictor)
    velm_->Update(1.0/dt,*dism_,-1.0/dt,*dis_,0.0);
    //velm_->Update((delta-(1.0-alphaf)*gamma)/delta,*vel_,gamma/(delta*dt));



    //---------------------------- compute internal forces and stiffness
    {


      // zero out stiffness
      stiff_->Zero();
      // create the parameters for the discretization
      ParameterList p;
      // action for elements
      p.set("action","calc_struct_nlnstiff");
      // other parameters that might be needed by the elements
      p.set("total time",timen);
      p.set("delta time",dt);
      p.set("alpha f",alphaf);

      //passing statistical mechanics parameters to elements
      p.set("ETA",(statmechmanager_->statmechparams_).get<double>("ETA",0.0));
      p.set("THERMALBATH",Teuchos::getIntegralValue<INPAR::STATMECH::ThermalBathType>(statmechmanager_->statmechparams_,"THERMALBATH"));
      p.set("FRICTION_MODEL",Teuchos::getIntegralValue<INPAR::STATMECH::FrictionModel>(statmechmanager_->statmechparams_,"FRICTION_MODEL"));
      p.set("RandomNumbers",randomnumbers);
      p.set("SHEARAMPLITUDE",(statmechmanager_->statmechparams_).get<double>("SHEARAMPLITUDE",0.0));
      p.set("CURVENUMBER",(statmechmanager_->statmechparams_).get<int>("CURVENUMBER",-1));
      p.set("STARTTIME",(statmechmanager_->statmechparams_).get<double>("STARTTIME",0.0));
      p.set("DELTA_T_NEW",(statmechmanager_->statmechparams_).get<double>("DELTA_T_NEW",0.0));
      p.set("OSCILLDIR",(statmechmanager_->statmechparams_).get<int>("OSCILLDIR",-1));
      p.set("PeriodLength",(statmechmanager_->statmechparams_).get<double>("PeriodLength",0.0));

      // set vector values needed by elements
      discret_.ClearState();

      // scale IncD_{n+1} by (1-alphaf) to obtain mid residual displacements IncD_{n+1-alphaf}
      disi_->Scale(1.-alphaf);

      discret_.SetState("residual displacement",disi_);
      discret_.SetState("displacement",dism_);
      discret_.SetState("velocity",velm_);

      //discret_.SetState("velocity",velm_); // not used at the moment
      fint_->PutScalar(0.0);  // initialise internal force vector

      const double t_evaluate = Teuchos::Time::wallTime();

      discret_.Evaluate(p,stiff_,null,fint_,null,null);

      sumevaluation += Teuchos::Time::wallTime() - t_evaluate;

      discret_.ClearState();

      // do NOT finalize the stiffness matrix to add damping to it later
    }

    //------------------------------------------ compute residual forces

    // dynamic residual
    // Res =  C . V_{n+1-alpha_f}
    //        + F_int(D_{n+1-alpha_f})
    //        - F_{ext;n+1-alpha_f}
    // add mid-inertial force


    //RefCountPtr<Epetra_Vector> fviscm = LINALG::CreateVector(*dofrowmap,true);
    fresm_->Update(-1.0,*fint_,1.0,*fextm_,0.0);


    // blank residual DOFs that are on Dirichlet BC
    {
      Epetra_Vector fresmcopy(*fresm_);
      fresm_->Multiply(1.0,*invtoggle_,fresmcopy,0.0);
    }

    // compute inf norm of residual
    double np;
    fresm_->NormInf(&np);

    //---------------------------------------------- build residual norm
    disi_->Norm2(&disinorm);

    fresm_->Norm2(&fresmnorm);

    if (!myrank_ and (printscreen or printerr))
    {
      PrintPTC(printscreen,printerr,print_unconv,errfile,timer,numiter,maxiter,
                  fresmnorm,disinorm,convcheck,dti);
    }

    //------------------------------------ PTC update of artificial time
#if 1
    // SER step size control
    dti *= pow((np/nc),6.0);
    dti = max(dti,0.0);
    nc = np;


    //Modifikation: sobald Residuum klein, PTC ausgeschaltet
    if(np < 0.01*resinit)
      dti = 0.0;


#else
    {
      // TTE step size control
      double ttau=0.75;
      RCP<Epetra_Vector> d1 = LINALG::CreateVector(SystemMatrix()->RowMap(),false);
      d1->Update(1.0,*disi_,-1.0,*x0,0.0);
      d1->Scale(dti0);
      RCP<Epetra_Vector> d0 = LINALG::CreateVector(SystemMatrix()->RowMap(),false);
      d0->Update(1.0,*x0,-1.0,*xm,0.0);
      d0->Scale(dtim);
      double dt0 = 1/dti0;
      double dtm = 1/dtim;
      RCP<Epetra_Vector> xpp = LINALG::CreateVector(SystemMatrix()->RowMap(),false);
      xpp->Update(2.0/(dt0+dtm),*d1,-2.0/(dt0+dtm),*d0,0.0);
      RCP<Epetra_Vector> xtt = LINALG::CreateVector(SystemMatrix()->RowMap(),false);
      for (int i=0; i<xtt->MyLength(); ++i) (*xtt)[i] = abs((*xpp)[i])/(1.0+abs((*disi_)[i]));
      double ett;
      xtt->MaxValue(&ett);
      ett = ett / (2.*ttau);
      dti = sqrt(ett);
      nc = np;

      //Modifikation: sobald Residuum klein, PTC ausgeschaltet
      if(np < 0.01*resinit)
        dti = 0.0;

    }
#endif


    //--------------------------------- increment equilibrium loop index
    ++numiter;

  }
  //============================================= end equilibrium loop
  print_unconv = false;

  //-------------------------------- test whether max iterations was hit
  //if on convergence arises within maxiter iterations the time step is restarted with new random numbers
  if (numiter>=maxiter)
  {
    isconverged_ = 0;
    unconvergedsteps_++;
    std::cout<<"\n\niteration unconverged - new trial with new random numbers!\n\n";
     //dserror("FullNewton unconverged in %d iterations",numiter);
  }
  else
  {
     if (!myrank_ and printscreen)
     {
       PrintPTC(printscreen,printerr,print_unconv,errfile,timer,numiter,maxiter,
                   fresmnorm,disinorm,convcheck,dti);
     }
  }

  params_.set<int>("num iterations",numiter);

  if(!discret_.Comm().MyPID())
  std::cout << "\n***\nevaluation time: " << sumevaluation<< " seconds\nptc time: "<< sumptc <<" seconds\nsolver time: "<< sumsolver <<" seconds\ntotal solution time: "<<Teuchos::Time::wallTime() - tbegin<<" seconds\n***\n";

  return;
} // StatMechTime::PTC()


/*----------------------------------------------------------------------*
 |  do output including statistical mechanics data(public)    cyron 12/08|
 *----------------------------------------------------------------------*/
void StatMechTime::Output()
{
  // -------------------------------------------------------------------
  // get some parameters from parameter list
  // -------------------------------------------------------------------
  double timen         = params_.get<double>("total time"             ,0.0);
  double dt            = params_.get<double>("delta time"             ,0.01);
  double alphaf        = params_.get<double>("alpha f"                ,0.459);
  int    istep         = params_.get<int>   ("step"                   ,0);
  int    nstep         = params_.get<int>   ("nstep"                  ,5);
  int    numiter       = params_.get<int>   ("num iterations"         ,-1);

  bool   iodisp        = params_.get<bool>  ("io structural disp"     ,true);
  int    updevrydisp   = params_.get<int>   ("io disp every nstep"    ,10);
  INPAR::STR::StressType iostress = params_.get<INPAR::STR::StressType>("io structural stress",INPAR::STR::stress_none);
  int    updevrystress = params_.get<int>   ("io stress every nstep"  ,10);
  INPAR::STR::StrainType iostrain      = params_.get<INPAR::STR::StrainType>("io structural strain",INPAR::STR::strain_none);
  bool   iosurfactant  = params_.get<bool>  ("io surfactant"          ,false);

  int    writeresevry  = params_.get<int>   ("write restart every"    ,0);

  bool   printscreen   = params_.get<bool>  ("print to screen"        ,true);
  bool   printerr      = params_.get<bool>  ("print to err"           ,true);
  FILE*  errfile       = params_.get<FILE*> ("err file"               ,NULL);
  if (!errfile) printerr = false;

  bool isdatawritten = false;

  //------------------------------------------------- write restart step
  if (writeresevry and istep%writeresevry==0)
  {
    output_.WriteMesh(istep,timen);
    output_.NewStep(istep, timen);
    output_.WriteVector("displacement",dis_);
    output_.WriteVector("velocity",vel_);
    output_.WriteVector("acceleration",acc_);
    output_.WriteVector("fexternal",fext_);

#ifdef INVERSEDESIGNCREATE // indicate that this restart is from INVERSEDESIGCREATE phase
    output_.WriteInt("InverseDesignRestartFlag",0);
#endif
#ifdef INVERSEDESIGNUSE // indicate that this restart is from INVERSEDESIGNUSE phase
    output_.WriteInt("InverseDesignRestartFlag",1);
#endif

    isdatawritten = true;

//____________________________________________________________________________________________________________
//note:the following block is the only difference to Output() in strugenalpha.cpp-----------------------------
/* write restart information for statistical mechanics problems; all the information is saved as class variables
 * of StatMechManager*/
    statmechmanager_->StatMechWriteRestart(output_);
//------------------------------------------------------------------------------------------------------------
//____________________________________________________________________________________________________________

    if (surf_stress_man_->HaveSurfStress())
      surf_stress_man_->WriteRestart(istep, timen);

    if (constrMan_->HaveConstraint())
    {
      output_.WriteDouble("uzawaparameter",constrSolv_->GetUzawaParameter());
      output_.WriteVector("lagrmultiplier",constrMan_->GetLagrMultVector());
      output_.WriteVector("refconval",constrMan_->GetRefBaseValues());
    }

    if (discret_.Comm().MyPID()==0 and printscreen)
    {
      cout << "====== Restart written in step " << istep << endl;
      fflush(stdout);
    }
    if (errfile and printerr)
    {
      fprintf(errfile,"====== Restart written in step %d\n",istep);
      fflush(errfile);
    }
  }

  //----------------------------------------------------- output results
  if (iodisp and updevrydisp and istep%updevrydisp==0 and !isdatawritten)
  {
    output_.NewStep(istep, timen);
    output_.WriteVector("displacement",dis_);
    output_.WriteVector("velocity",vel_);
    output_.WriteVector("acceleration",acc_);
    output_.WriteVector("fexternal",fext_);
    output_.WriteElementData();

    if (surf_stress_man_->HaveSurfStress() and iosurfactant)
      surf_stress_man_->WriteResults(istep,timen);

    isdatawritten = true;
  }

  //------------------------------------- do stress calculation and output
  if (updevrystress and !(istep%updevrystress) and iostress!=INPAR::STR::stress_none)
  {
    // create the parameters for the discretization
    ParameterList p;
    // action for elements
    p.set("action","calc_struct_stress");
    // other parameters that might be needed by the elements
    p.set("total time",timen);
    p.set("delta time",dt);
    p.set("alpha f",alphaf);
    Teuchos::RCP<std::vector<char> > stress = Teuchos::rcp(new std::vector<char>());
    Teuchos::RCP<std::vector<char> > strain = Teuchos::rcp(new std::vector<char>());
    p.set("stress", stress);
    p.set("iostress", iostress);
    p.set("strain", strain);
    p.set("iostrain", iostrain);
    // set vector values needed by elements
    discret_.ClearState();
    discret_.SetState("residual displacement",zeros_);
    discret_.SetState("displacement",dis_);
    discret_.SetState("velocity",vel_);
    discret_.Evaluate(p,null,null,null,null,null);
    discret_.ClearState();
    if (!isdatawritten) output_.NewStep(istep, timen);
    isdatawritten = true;

    switch (iostress)
    {
    case INPAR::STR::stress_cauchy:
      output_.WriteVector("gauss_cauchy_stresses_xyz",*stress,*discret_.ElementColMap());
      break;
    case INPAR::STR::stress_2pk:
      output_.WriteVector("gauss_2PK_stresses_xyz",*stress,*discret_.ElementColMap());
      break;
    case INPAR::STR::stress_none:
      break;
    default:
      dserror ("requested stress type not supported");
    }

    switch (iostrain)
    {
    case INPAR::STR::strain_ea:
      output_.WriteVector("gauss_EA_strains_xyz",*strain,*discret_.ElementColMap());
      break;
    case INPAR::STR::strain_gl:
      output_.WriteVector("gauss_GL_strains_xyz",*strain,*discret_.ElementColMap());
      break;
    case INPAR::STR::strain_none:
      break;
    default:
      dserror("requested strain type not supported");
    }
  }

  //---------------------------------------------------------- print out
  if (!myrank_)
  {
    if (printscreen)
    {
      printf("step %6d | nstep %6d | time %-14.8E | dt %-14.8E | numiter %3d\n",
             istep,nstep,timen,dt,numiter);
      printf("----------------------------------------------------------------------------------\n");
      fflush(stdout);
    }
    if (printerr)
    {
      fprintf(errfile,"step %6d | nstep %6d | time %-14.8E | dt %-14.8E | numiter %3d\n",
              istep,nstep,timen,dt,numiter);
      fprintf(errfile,"----------------------------------------------------------------------------------\n");
      fflush(errfile);
    }
  }
}//StatMechTime::Output()

/*----------------------------------------------------------------------*
 |  read restart (public)                                    cyron 12/08|
 *----------------------------------------------------------------------*/
void StatMechTime::ReadRestart(int step)
{
  RefCountPtr<DRT::Discretization> rcpdiscret = rcp(&discret_);
  rcpdiscret.release();
  IO::DiscretizationReader reader(rcpdiscret,step);
  double time  = reader.ReadDouble("time");
  int    rstep = reader.ReadInt("step");
  if (rstep != step) dserror("Time step on file not equal to given step");

  reader.ReadVector(dis_, "displacement");
  reader.ReadVector(vel_, "velocity");
  reader.ReadVector(acc_, "acceleration");
  reader.ReadVector(fext_,"fexternal");
  reader.ReadMesh(step);

  // read restart information for contact
  statmechmanager_->StatMechReadRestart(reader);

#ifdef INVERSEDESIGNUSE
  int idrestart = -1;
  idrestart = reader.ReadInt("InverseDesignRestartFlag");
  if (idrestart==-1) dserror("expected inverse design restart flag not on file");
  // if idrestart==0 then the file is from a INVERSEDESIGCREATE phase
  // and we have to zero out the inverse design displacements.
  // The stored reference configuration is on record at the element level
  if (!idrestart)
  {
    dis_->PutScalar(0.0);
    vel_->PutScalar(0.0);
    acc_->PutScalar(0.0);
  }
#endif

  // override current time and step with values from file
  params_.set<double>("total time",time);
  params_.set<int>   ("step",rstep);

  if (surf_stress_man_->HaveSurfStress())
    surf_stress_man_->ReadRestart(rstep, DRT::Problem::Instance()->InputControlFile()->FileName());

  if (constrMan_->HaveConstraint())
  {
    double uzawatemp = reader.ReadDouble("uzawaparameter");
    constrSolv_->SetUzawaParameter(uzawatemp);
    RCP<Epetra_Map> constrmap=constrMan_->GetConstraintMap();
    RCP<Epetra_Vector> tempvec = LINALG::CreateVector(*constrmap,true);
    reader.ReadVector(tempvec, "lagrmultiplier");
    constrMan_->SetLagrMultVector(tempvec);
    reader.ReadVector(tempvec, "refconval");
    constrMan_->SetRefBaseValues(tempvec,time);
  }

  return;
}//StatMechTime::ReadRestart()

/*----------------------------------------------------------------------*
 |  Evaluate DBCs in case of periodic BCs (public)         mueller  2/10|
 *----------------------------------------------------------------------*/
void StatMechTime::EvaluateDirichletPeriodic(ParameterList& params)
/*The idea behind EvaluateDirichletPeriodic() is simple:
 * Give Dirichlet values to nodes of an element that is broken in
 * z-direction due to the application of periodic boundary conditions.
 * The motion of the node close to z=0.0 in the cubic volume of edge
 * length l (==PeriodLength in this case) is inhibited in direction of the
 * oscillatory motion. The oscillation is imposed on the node close to z=l.
 * This method is triggered in case of PeriodLength>0.0 (i.e. Periodic BCs
 * exist). Since the DBC setup happens dynamically by checking element
 * positions with each new time step, the static definition of DBCs in the input
 * file is only used to get the direction of the oscillatory motion as well as
 * the time curve. Therefore, only one DBC needs to be specified.
 *
 * How it works:
 * Each time this method is called, the system vector and the toggle vector are
 * modified to fit the current geometric situation.
 * DOFs holdings Dirichlet values are marked by setting the corresponding toggle
 * vector component to 1.0. In case of an element which was broken the step before
 * and is now whole again,just the toggle vector components in question are
 * reset to 0.0.
 * A position vector deltadbc_ is needed in order to calculate the correct Dirichlet
 * values to be imposed on nodes of an element which has drifted over the boundaries
 * and thus has been broken.
 * These positions are used to calculate the zero position of the oscillation which then can
 * be added to the time curve value in DoDirichletConditionPeriodic().
 */
{
#ifdef MEASURETIME
  const double t_start = Teuchos::Time::wallTime();
#endif // #ifdef MEASURETIME

	if (!(discret_.Filled())) dserror("FillComplete() was not called");
	if (!(discret_.HaveDofs())) dserror("AssignDegreesOfFreedom() was not called");

  //----------------------------------- some variables
	// indicates broken element
	bool 											broken;
	// time trigger
	bool 											usetime = true;
	// to avoid redundant or wrong actions when filling vectors or deleting the last element of the free nodes vector
	bool											alreadydone = false;
	// store node Id of previously handled node
  int 											tmpid = -1;
  // store LIDs of element nodes
  vector<int>								lids;
  // vectors to manipulate DBC properties
  vector<int> 							oscillnodes;
  vector<int> 							fixednodes;
  vector<int> 							freenodes;

  // init
  if(!isinit_)
  {
  	// get the apmlitude of the oscillation
  	amp_= statmechmanager_->statmechparams_.get<double>("SHEARAMPLITUDE",0.0);
		// retrieve direction of oscillatory motion
		oscdir_ = statmechmanager_->statmechparams_.get<int>("OSCILLDIR",-1);
		// retrieve number of time curve that is to be applied
		curvenumber_ = statmechmanager_->statmechparams_.get<int>("CURVENUMBER",0)-1;
		// initialize deltadbc with large values
		for(int i=0; i<deltadbc_->MyLength(); i++)
			(*deltadbc_)[i] = 0.0;

		//error message
		if(oscdir_!=0 && oscdir_!=1 && oscdir_!=2)
			dserror("Please define the StatMech Parameter OSCILLDIR correctly");

		isinit_=true;
	} // init

  // get the current time
	const double time = statmechmanager_->time_;
	// check if start time for DBC evaluation has been reached. If not, do nothing and just return!
	if(time < (statmechmanager_->statmechparams_.get<double>("STARTTIME",-1.0)+statmechmanager_->statmechparams_.get<double>("DELTA_T_NEW", 0.01)))
		return;
	if (time<0.0)
		usetime = false;

//---------------------------------------------------------- loop through elements
  // this step is advantageous because GIDs of original elements are defined in numerical order
  int numevalelements = statmechmanager_->statmechparams_.get("NUM_EVAL_ELEMENTS", -1);
  if(numevalelements == -1)
  	dserror("Check NUM_EVAL_ELEMENTS, the number of evaluated elements, in you StatMech Parameters block");
  // loop over the original elements that are to be evaluated (beam or truss)
	for(int gid=0;gid<numevalelements; gid++)
	{
		// check if element is on current proc. If not, continue
		bool havenode = discret_.HaveGlobalElement(gid);
		if(!havenode)
			continue;
		// get the LID of the current element
		int lid = discret_.gElement(gid)->LID();
		// An element used to browse through local Row Elements
	  DRT::Element* 						element = discret_.lRowElement(lid);
	  // number of DOFs per node
	  int numdof = (int)discret_.Dof(0, element->Nodes()[0]).size();
	  // positions of nodes of an element with n nodes
	  LINALG::SerialDenseMatrix coord(numdof,(int)discret_.lRowElement(lid)->NumNode(), true);
	  // indicates location, direction and component of a broken element with n nodes->n-1 possible cuts
	  LINALG::SerialDenseMatrix cut(numdof,(int)discret_.lRowElement(lid)->NumNode()-1,true);
//-------------------------------- obtain nodal coordinates of the current element
	  // get nodal coordinates and LIDs of the nodal DOFs
	  statmechmanager_->GetElementNodeCoords(element, disn_, coord, &lids);
//-----------------------detect broken/fixed/free elements and fill position vector
	  // determine existence and location of broken element
	  statmechmanager_->CheckForBrokenElement(coord, cut, &broken);

	  // loop over number of cuts (columns)
	  for(int n=0; n<cut.N(); n++)
	  {
	  	// case 1: broken element (in z-dir); node_n+1 oscillates, node_n is fixed in dir. of oscillation
			if(broken && cut(2,n)==1.0)
			{
				// indicates beginning of a new filament (in the very special case that this is needed)
				bool newfilament = false;
				// check for case: last element of filament I as well as first element of filament I+1 broken
				if(tmpid!=element->Nodes()[n]->Id() && alreadydone)
				{
					// in this case, reset alreadydone...
					alreadydone = false;
					// ...and set newfilament to true. Otherwise the last free nodes vector element will be deleted
					newfilament = true;
				}

				// add GID of fixed node to fixed-nodes-vector (to be added to condition later)
				if(!alreadydone)
					fixednodes.push_back(element->Nodes()[n]->Id());
				// add GID of oscillating node to osc.-nodes-vector
				oscillnodes.push_back(element->Nodes()[n+1]->Id());

				/* When an element is cut, there are always two nodes involved: one that is subjected to a fixed
				 * displacement in one particular direction (oscdir_), another which oscillates in the same direction.
				 * The following code section calculates increments for both node types and stores this increment in
				 * a vector deltadbc_. This increment is later added to the nodes' displacement of the preceding time step.
				 */
				// the new displacement increments
				// incremental displacement for a fixed node...(all DOFs = 0.0)
				for(int i=0; i<numdof; i++)
					if(i==oscdir_)
						(*deltadbc_)[lids.at(numdof*n+i)] = 0.0;

				// incremental Dirichlet displacement for an oscillating node (all DOFs except oscdir_ = 0.0)
				//for(int i=0; i<numdof; i++)
					//if(i!=oscdir_)
						//(*deltadbc_)[lids.at(numdof*(n+1)+i)] = 0.0;
					//else
					//{
						// time step size
						double dt = params_.get<double>("delta time" ,-1.0);
						// time curve increment
						double tcincrement = 0.0;
						if(curvenumber_>-1)
							tcincrement = DRT::Problem::Instance()->Curve(curvenumber_).f(time) -
														DRT::Problem::Instance()->Curve(curvenumber_).f(time-dt);
						(*deltadbc_)[lids.at(numdof*(n+1)+oscdir_)] = amp_*tcincrement;
					//}

				// delete last Id of freenodes if it was previously and falsely added
				if(element->Nodes()[n]->Id()==tmpid && !alreadydone && !newfilament)
					freenodes.pop_back();
				// store gid of the "n+1"-node to avoid overwriting during the following iteration,
				// e.g. oscillating node becomes free if the following CheckForBrokenElement() call yields "!broken".
				tmpid = element->Nodes()[n+1]->Id();
				// Set to true to initiate certain actions if the following element is also broken.
				// If the following element isn't broken, alreadydone will be reset to false (see case: !broken)
				alreadydone=true;
			}// end of case 1

			// case 2: broken element (in z-dir); node_n oscillates, node_n+1 is fixed in dir. of oscillation
			if(broken && cut(2,n)==2.0)
			{
				bool newfilament = false;

				if(tmpid!=element->Nodes()[n]->Id() && alreadydone)
				{
					alreadydone = false;
					newfilament = true;
				}

				if(!alreadydone)
					oscillnodes.push_back(element->Nodes()[n]->Id());
				fixednodes.push_back(element->Nodes()[n+1]->Id());

				// oscillating node
				//for(int i=0; i<numdof; i++)
					//if(i!=oscdir_)
						//(*deltadbc_)[lids.at(numdof*n+i)] = 0.0;
					//else
					//{
						double dt = params_.get<double>("delta time" ,-1.0);
						double tcincrement = 0.0;
						if(curvenumber_>-1)
							tcincrement = DRT::Problem::Instance()->Curve(curvenumber_).f(time) -
														DRT::Problem::Instance()->Curve(curvenumber_).f(time-dt);
						(*deltadbc_)[lids.at(numdof*n+oscdir_)] = amp_*tcincrement;
					//}
				// fixed node
				for(int i=0; i<numdof; i++)
					if(i==oscdir_)
						(*deltadbc_)[lids.at(numdof*(n+1)+i)] = 0.0;

				if(element->Nodes()[n]->Id()==tmpid && !alreadydone && !newfilament)
					freenodes.pop_back();

				tmpid = element->Nodes()[n+1]->Id();
				alreadydone = true;
			} // end of case 2

			// case 3: unbroken element or broken in another than z-direction
			if(cut(2,n)==0.0)
			{
				if(element->Nodes()[n]->Id()!=tmpid)
				{
					freenodes.push_back(element->Nodes()[n]->Id());
					freenodes.push_back(element->Nodes()[n+1]->Id());
				}
				else
					freenodes.push_back(element->Nodes()[n+1]->Id());
				tmpid=element->Nodes()[n+1]->Id();
				// set to false to handle annoying special cases
				alreadydone = false;
			}
	  }
	}

//---------check/set force sensors anew for each time step
  if(Teuchos::getIntegralValue<int>(DRT::Problem::Instance()->StatisticalMechanicsParams(),"DYN_CROSSLINKERS"))
  	// add DOF LID where a force sensor is to be set
  	statmechmanager_->UpdateForceSensors(oscillnodes, oscdir_);
  cout<<"\n=========================================="<<endl;
  cout<<"UpdateForceSensors: "<<oscillnodes.size()<< " nodes @ t="<<time<<endl;
  cout<<"==========================================\n"<<endl;

//------------------------------------set Dirichlet values
	// preliminary
  DRT::Node* node = discret_.gNode(discret_.NodeRowMap()->GID(0));
	int numdof = (int)discret_.Dof(node).size();
	vector<int>  	 addonoff(numdof, 0);

  // set condition for oscillating nodes
	// inhibit all DOFs (for now, testing)
	for(int i=0; i<numdof; i++)
		if(i==oscdir_)
			addonoff.at(i) = 1;
	//cout<<"deltadbc_ = \n"<<*deltadbc_<<endl;
  // do not do anything if vector is empty
  if(!oscillnodes.empty())
  {
  	//cout<<"OSCILLATING NODES"<<endl;
  	DoDirichletConditionPeriodic(&oscillnodes, &addonoff);
  }

  // set condition for fixed nodes

	if(!fixednodes.empty())
	{
		//cout<<"FIXED NODES"<<endl;
  	DoDirichletConditionPeriodic(&fixednodes, &addonoff);
	}

  // set condition for free or recently set free nodes
  for(int i=0; i<numdof; i++)
  	if(i==oscdir_)
  		addonoff.at(i) = 0;

	if(!freenodes.empty())
	{
		//cout<<"FREE NODES"<<endl;
  	DoDirichletConditionPeriodic(&freenodes, &addonoff);
	}
	//cout<<"deltadbc_:\n"<<*deltadbc_<<endl;

#ifdef MEASURETIME
  const double t_end = Teuchos::Time::wallTime();
  cout<<"DBC Evaluation time: "<<t_end-t_start<<endl;
#endif // #ifdef MEASURETIME
	return;
}

/*----------------------------------------------------------------------*
 |  fill system vector and toggle vector (public)          mueller  3/10|
 *----------------------------------------------------------------------*/
void StatMechTime::DoDirichletConditionPeriodic(vector<int>* nodeids,
																								vector<int>* onoff)
/*
 * This basically does the same thing as DoDirichletCondition() (to be found in drt_discret_evaluate.cpp),
 * but with the slight difference of taking current displacements into account.
 * Time curve values aren't added to the reference position(s) of the discretization as usual,
 * but to the latest known 0-position(s). These positions are calculated using the deltadbc_
 * vector holding the latest incremental Dirichlet displacement. It is added to the displacement
 * at the end of the preceding time step.
 */
{
	/*/ "condition output"
	cout<<"Node Ids: ";
	for(int i=0; i<(int)nodeids->size(); i++)
		cout<<nodeids->at(i)<<" ";
	cout<<"onoff: ";
	for(int i=0; i<(int)discret_.Dof(0,discret_.gNode(nodeids->at(0))).size(); i++)
		cout<<onoff->at(i)<<" ";
	cout<<endl;*/

	// some checks for errors
	if (!nodeids) dserror("No Node IDs were handed over!");
	if(disn_==Teuchos::null) dserror("Displacement vector must be !=null");
	if(deltadbc_==Teuchos::null || dirichtoggle_==Teuchos::null)
		dserror("deltadbc_ and dirichtoggle_ must be non-empty");

	// get the condition properties
	const int nnode = nodeids->size();

	// loop over all nodes in condition
	for (int i=0; i<nnode; ++i)
	{
		// do only nodes in my row map
		if (!discret_.NodeRowMap()->MyGID(nodeids->at(i))) continue;
		DRT::Node* actnode = discret_.gNode(nodeids->at(i));
		if (!actnode) dserror("Cannot find global node %d",nodeids->at(i));
		// call explicitly the main dofset, i.e. the first column
		vector<int> dofs = discret_.Dof(0,actnode);
		const unsigned numdf = dofs.size();

		// loop over DOFs
		for (unsigned j=0; j<numdf; ++j)
		{
			// get the LID of the currently handled DOF
			const int lid = (*disn_).Map().LID(dofs[j]);

			// if DOF in question is not subject to DBCs (anymore)
      if (onoff->at(j)==0)
      {
        if (lid<0) dserror("Global id %d not on this proc in system vector",dofs[j]);
        if (dirichtoggle_!=Teuchos::null)
        {
        	// turn off application of Dirichlet value
          (*dirichtoggle_)[lid] = 0.0;
          // in addition, modify the inverse vector (needed for manipulation of the residual vector)
          (*invtoggle_)[lid] = 1.0;
        }
        // if formerly subject to Dirichlet values, reset deltadbc_ for this lid
        if((*deltadbc_)[lid]!= 9e99)
        	(*deltadbc_)[lid] = 9e99;
        continue;
      }
//---------------------------------------------Dirichlet Value Assignment
	    // assign value
	    if (lid<0) dserror("Global id %d not on this proc in system vector", dofs[j]);
	    if (disn_ != Teuchos::null)
	    	(*disn_)[lid] += (*deltadbc_)[lid];
	    // set toggle vector and the inverse vector
	    if (dirichtoggle_ != Teuchos::null)
	    {
	      (*dirichtoggle_)[lid] = 1.0;
	      (*invtoggle_)[lid] = 0.0;
	    }
	  }  // loop over nodal DOFs
	}  // loop over nodes
	return;
}

#endif  // #ifdef CCADISCRET
