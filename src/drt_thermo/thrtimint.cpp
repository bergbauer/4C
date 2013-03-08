/*----------------------------------------------------------------------*/
/*!
\file thrtimint.cpp
\brief Time integration for spatially discretised thermal dynamics

<pre>
Maintainer: Burkhard Bornemann
            bornemann@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15237

</pre>
*/


/*----------------------------------------------------------------------*
 | headers                                                  bborn 08/09 |
 *----------------------------------------------------------------------*/
#include <iostream>
#include "Epetra_SerialDenseMatrix.h"
#include "Epetra_SerialDenseVector.h"
#include "Teuchos_TimeMonitor.hpp"

#include "thrtimint_mstep.H"
#include "thrtimint.H"
#include "thr_resulttest.H"

#include "../drt_io/io_control.H"


/*----------------------------------------------------------------------*
 | print thermal time logo                                   dano 08/09 |
 *----------------------------------------------------------------------*/
void THR::TimInt::Logo()
{
  std::cout << "Welcome to Thermal Time Integration " << std::endl;
  std::cout << "      _______________________________" << std::endl;
  std::cout << "  ===(_________|_|_|_|_|_37°C_|_|____)" << std::endl;
  std::cout << std::endl;
}  // Logo()


/*----------------------------------------------------------------------*
 | constructor                                              bborn 08/09 |
 *----------------------------------------------------------------------*/
THR::TimInt::TimInt(
  const Teuchos::ParameterList& ioparams,
  const Teuchos::ParameterList& tdynparams,
  const Teuchos::ParameterList& xparams,
  Teuchos::RCP<DRT::Discretization> actdis,
  Teuchos::RCP<LINALG::Solver> solver,
  Teuchos::RCP<IO::DiscretizationWriter> output
  )
: discret_(actdis),
  myrank_(actdis->Comm().MyPID()),
  dofrowmap_(actdis->Filled() ? actdis->DofRowMap() : NULL),
  solver_(solver),
  solveradapttol_(DRT::INPUT::IntegralValue<int>(tdynparams,"ADAPTCONV")==1),
  solveradaptolbetter_(tdynparams.get<double>("ADAPTCONV_BETTER")),
  dbcmaps_(Teuchos::rcp(new LINALG::MapExtractor())),
  output_(output),
  printlogo_(true),  // DON'T EVEN DARE TO SET THIS TO FALSE
  printscreen_(ioparams.get<int>("STDOUTEVRY")),
  errfile_(xparams.get<FILE*>("err file")),
  printerrfile_(true and errfile_),  // ADD INPUT PARAMETER FOR 'true'
  printiter_(true),  // ADD INPUT PARAMETER
  writerestartevery_(tdynparams.get<int>("RESTARTEVRY")),
  writeglob_((bool) DRT::INPUT::IntegralValue<int>(ioparams,"THERM_TEMPERATURE")),
  writeglobevery_(tdynparams.get<int>("RESEVRYGLOB")),
  writeheatflux_(DRT::INPUT::IntegralValue<INPAR::THR::HeatFluxType>(ioparams,"THERM_HEATFLUX")),
  writetempgrad_(DRT::INPUT::IntegralValue<INPAR::THR::TempGradType>(ioparams,"THERM_TEMPGRAD")),
  writeenergyevery_(tdynparams.get<int>("RESEVRYERGY")),
  energyfile_(NULL),
  time_(Teuchos::null),
  timen_(0.0),
  dt_(Teuchos::null),
  timemax_(tdynparams.get<double>("MAXTIME")),
  stepmax_(tdynparams.get<int>("NUMSTEP")),
  step_(0),
  stepn_(0),
  firstoutputofrun_(true),
  lumpcapa_(DRT::INPUT::IntegralValue<int>(tdynparams,"LUMPCAPA")==1),
  young_temp_(DRT::INPUT::IntegralValue<int>(DRT::Problem::Instance()->StructuralDynamicParams(),"YOUNG_IS_TEMP_DEPENDENT")==1),
  zeros_(Teuchos::null),
  temp_(Teuchos::null),
  rate_(Teuchos::null),
  tempn_(Teuchos::null),
  raten_(Teuchos::null),
  disn_(Teuchos::null),  // needed for TSI
  veln_(Teuchos::null),  // needed for TSI
  fifc_(Teuchos::null),
  tang_(Teuchos::null)
//  capa_(Teuchos::null)
{
  // welcome user
  if ( (printlogo_) and (myrank_ == 0) )
  {
    Logo();
  }

  // check wether discretisation has been completed
  if (not discret_->Filled())
  {
    dserror("Discretisation is not complete!");
  }

  // time state
  time_ = Teuchos::rcp(new TimIntMStep<double>(0, 0, 0.0));
  // HERE SHOULD BE SOMETHING LIKE (tdynparams.get<double>("TIMEINIT"))
  dt_ = Teuchos::rcp(new TimIntMStep<double>(0, 0, tdynparams.get<double>("TIMESTEP")));
  step_ = 0;
  timen_ = (*time_)[0] + (*dt_)[0];  // set target time to initial time plus step size
  stepn_ = step_ + 1;

  // output file for energy
  if ( (writeenergyevery_ != 0) and (myrank_ == 0) )
    AttachEnergyFile();

  // a zero vector of full length
  zeros_ = LINALG::CreateVector(*dofrowmap_, true);

  // Map containing Dirichlet DOFs
  {
    Teuchos::ParameterList p;
    p.set("total time", timen_);
    discret_->EvaluateDirichlet(p, zeros_, Teuchos::null, Teuchos::null, Teuchos::null, dbcmaps_);
    zeros_->PutScalar(0.0); // just in case of change
  }

  // temperatures T_{n}
  temp_ = Teuchos::rcp(new TimIntMStep<Epetra_Vector>(0, 0, dofrowmap_, true));
  // temperature rates R_{n}
  rate_ = Teuchos::rcp(new TimIntMStep<Epetra_Vector>(0, 0, dofrowmap_, true));

  // temperatures T_{n+1} at t_{n+1}
  tempn_ = LINALG::CreateVector(*dofrowmap_, true);
  // temperature rates R_{n+1} at t_{n+1}
  raten_ = LINALG::CreateVector(*dofrowmap_, true);

  // create empty interface force vector
  fifc_ = LINALG::CreateVector(*dofrowmap_, true);

  // create empty matrix
  tang_ = Teuchos::rcp(new LINALG::SparseMatrix(*dofrowmap_, 81, true, true));
  // we condensed the capacity matrix out of the system

  // -------------------------------------------------------------------
  // set initial field
  // -------------------------------------------------------------------
  const int startfuncno = tdynparams.get<int>("INITFUNCNO");
  SetInitialField(
    DRT::INPUT::IntegralValue<INPAR::THR::InitialField>(tdynparams,"INITIALFIELD"),
    startfuncno
    );

  // stay with us
  return;
}  // cstr


/*----------------------------------------------------------------------*
 | equilibrate system at initial state                      bborn 08/09 |
 | and identify consistent temperature rate (only dynamic case)         |
 *----------------------------------------------------------------------*/
void THR::TimInt::DetermineCapaConsistTempRate()
{
  // temporary force vectors in this routine
  Teuchos::RCP<Epetra_Vector> fext
    = LINALG::CreateVector(*dofrowmap_, true); //!< external force
  Teuchos::RCP<Epetra_Vector> fint
    = LINALG::CreateVector(*dofrowmap_, true); //!< internal force

  // overwrite initial state vectors with DirichletBCs
  ApplyDirichletBC((*time_)[0], (*temp_)(0), (*rate_)(0), false);

  // get external force
  ApplyForceExternal((*time_)[0], (*temp_)(0), fext);
  // ApplyForceExternalConv is applied in the derived classes!

  // initialise matrices
  tang_->Zero();
//  capa_->Zero();

  // get initial internal force, tangent and capacity
  {
    // create the parameters for the discretization
    Teuchos::ParameterList p;
    // action for elements
    p.set("action", "calc_thermo_fintcapa");
    // type of calling time integrator
    p.set<int>("time integrator", MethodName());
    p.set<bool>("lump capa matrix", lumpcapa_);
    // other parameters that might be needed by the elements
    p.set("total time", (*time_)[0]);
    p.set("delta time", (*dt_)[0]);
    p.set<int>("young_temp", young_temp_);
    // set vector values needed by elements
    discret_->ClearState();
    // SetState(0,...) in case of multiple dofsets (e.g. TSI)
    discret_->SetState(0, "residual temperature", zeros_);
    discret_->SetState(0, "temperature", (*temp_)(0));
    // set displacements for the coupled TSI problem
    if(disn_!=Teuchos::null)
    {
      discret_->SetState(1,"displacement",disn_);
      discret_->SetState(1,"velocity",veln_);
    }
    // calculate the capacity matrix onto tang_, instead of buildung 2 matrices
    discret_->Evaluate(p, Teuchos::null, tang_, fint, Teuchos::null, Teuchos::null);
    discret_->ClearState();
  }

  // finish capacity matrix
//  capa_->Complete();

  // close tangent matrix
  tang_->Complete();

  // calculate consistent initial temperature rates
  {
    // rhs corresponds to residual on the rhs
    // K . DT = - R_n+1 = - R_n - (fint_n+1 - fext_n+1)
    Teuchos::RCP<Epetra_Vector> rhs = LINALG::CreateVector(*dofrowmap_, true);
    rhs->Update(-1.0, *fint, 1.0, *fext, -1.0);
    // blank RHS on DBC DOFs
    dbcmaps_->InsertCondVector(dbcmaps_->ExtractCondVector(zeros_), rhs);
    solver_->Solve(tang_->EpetraMatrix(), (*rate_)(0), rhs, true, true);
  }

  // We need to reset the tangent matrix because its graph (topology)
  // is not finished yet in case of constraints and possibly other side
  // effects (basically managers).
  // BUT: in case of explicit time integration, the conductivity matrix
  // is stored in tang_ which is needed throughout the simulation
  if(MethodName() != INPAR::THR::dyna_expleuler)
    tang_->Reset();

  // leave this hell
  return;
}  // DetermineCapaConsistTempRate()


/*----------------------------------------------------------------------*
 | evaluate Dirichlet BC at t_{n+1}                         bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyDirichletBC(
  const double time,
  Teuchos::RCP<Epetra_Vector> temp,
  Teuchos::RCP<Epetra_Vector> rate,
  bool recreatemap
  )
{
  // apply DBCs
  // needed parameters
  Teuchos::ParameterList p;
  p.set("total time", time);  // target time

  // predicted Dirichlet values
  // \c temp then also holds prescribed new Dirichlet temperatures
  discret_->ClearState();
  if (recreatemap)
  {
    discret_->EvaluateDirichlet(p, temp, rate, Teuchos::null,
                                Teuchos::null, dbcmaps_);
  }
  else
  {
    discret_->EvaluateDirichlet(p, temp, rate, Teuchos::null,
                                Teuchos::null, Teuchos::null);
  }
  discret_->ClearState();

  // ciao
  return;
}  // ApplyDirichletBC()


/*----------------------------------------------------------------------*
 | prepare thermal contact                                   mgit 06/11 |
 *----------------------------------------------------------------------*/
void THR::TimInt:: PrepareThermoContact(
  Teuchos::RCP<MORTAR::ManagerBase> cmtman,
  Teuchos::RCP<DRT::Discretization> discretstruct
  )
{

  // thermo contact manager
  thermcontman_ = Teuchos::rcp(new THR::ThermoContactMan(cmtman,discretstruct,discret_));
  return;
}  // PrepareThermoContact()


/*----------------------------------------------------------------------*
 | update time and step counter                            bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::UpdateStepTime()
{
  // update time and step
  time_->UpdateSteps(timen_);  // t_{n} := t_{n+1}, etc
  step_ = stepn_;  // n := n+1

  timen_ += (*dt_)[0];
  stepn_ += 1;

  // new deal
  return;
}  // UpdateStepTime()


/*----------------------------------------------------------------------*
 | reset configuration after time step                      bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ResetStep()
{
  // reset state vectors
  tempn_->Update(1.0, (*temp_)[0], 0.0);
  raten_->Update(1.0, (*rate_)[0], 0.0);

  // reset anything that needs to be reset at the element level
  {
    // create the parameters for the discretization
    Teuchos::ParameterList p;
    p.set("action", "calc_thermo_reset_istep");
    // go to elements
    discret_->Evaluate(p, Teuchos::null, Teuchos::null,
                       Teuchos::null, Teuchos::null, Teuchos::null);
    discret_->ClearState();
  }

  // I am gone
  return;
}  // ResetStep()


/*----------------------------------------------------------------------*
 | read and set restart values                              bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ReadRestart(const int step)
{
  IO::DiscretizationReader reader(discret_, step);
  if (step != reader.ReadInt("step"))
    dserror("Time step on file not equal to given step");

  step_ = step;
  stepn_ = step_ + 1;
  time_ = Teuchos::rcp(new TimIntMStep<double>(0, 0, reader.ReadDouble("time")));
  timen_ = (*time_)[0] + (*dt_)[0];

  ReadRestartState();
  ReadRestartForce();

  // fix pointer to #dofrowmap_, which has not really changed, but is
  // located at different place
  dofrowmap_ = discret_->DofRowMap();
}  // ReadRestart()


/*----------------------------------------------------------------------*
 | read and set restart state                               bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ReadRestartState()
{
  IO::DiscretizationReader reader(discret_, step_);
  reader.ReadVector(tempn_, "temperature");
  temp_->UpdateSteps(*tempn_);
  reader.ReadVector(raten_, "rate");
  rate_->UpdateSteps(*raten_);
  reader.ReadMesh(step_);
  return;
}  // ReadRestartState()


/*----------------------------------------------------------------------*
 | output to file                                           mwgee 03/07 |
 *----------------------------------------------------------------------*/
void THR::TimInt::OutputStep()
{
  // this flag is passed along subroutines and prevents
  // repeated initialising of output writer, printing of
  // state vectors, or similar
  bool datawritten = false;

  // output restart (try this first)
  // write restart step
  if (writerestartevery_ and (step_%writerestartevery_ == 0) )
  {
    OutputRestart(datawritten);
  }

  // output results (not necessary if restart in same step)
  if ( writeglob_
       and writeglobevery_ and (step_%writeglobevery_ == 0)
       and (not datawritten) )
  {
    OutputState(datawritten);
  }

  // output heatflux & tempgrad
  if ( writeglobevery_
       and ( (writeheatflux_ != INPAR::THR::heatflux_none)
             or (writetempgrad_ != INPAR::THR::tempgrad_none) )
       and (step_%writeglobevery_ == 0) )
  {
    OutputHeatfluxTempgrad(datawritten);
  }

  // output energy
  if ( writeenergyevery_ and (step_%writeenergyevery_ == 0) )
  {
    OutputEnergy();
  }

  // what's next?
  return;
}  // OutputStep()


/*----------------------------------------------------------------------*
 | write restart                                            mwgee 03/07 |
 *----------------------------------------------------------------------*/
void THR::TimInt::OutputRestart(bool& datawritten)
{
  // Yes, we are going to write...
  datawritten = true;

  // write restart output, please
  output_->WriteMesh(step_, (*time_)[0]);
  output_->NewStep(step_, (*time_)[0]);
  output_->WriteVector("temperature", (*temp_)(0));
  output_->WriteVector("rate", (*rate_)(0));
  output_->WriteVector("fexternal", Fext());

  // owner of elements is just written once because it does not change during simulation (so far)
  output_->WriteElementData(firstoutputofrun_);
  firstoutputofrun_ = false;

  // info dedicated to user's eyes staring at standard out
  if ( (myrank_ == 0) and printscreen_ and (GetStep()%printscreen_==0))
  {
    printf("====== Restart written in step %d\n", step_);
    // print a beautiful line made exactly of 80 dashes
    printf("--------------------------------------------------------------"
            "------------------\n");
    fflush(stdout);
  }

  // info dedicated to processor error file
  if (printerrfile_)
  {
    fprintf(errfile_, "====== Restart written in step %d\n", step_);
    fprintf(errfile_,"--------------------------------------------------------------"
            "------------------\n");
    fflush(errfile_);
  }

  // we will say what we did
  return;
}  // OutputRestart()


/*----------------------------------------------------------------------*
 | output temperature,temperature rate                      bborn 06/08 |
 | originally by mwgee 03/07                                            |
 *----------------------------------------------------------------------*/
void THR::TimInt::OutputState(bool& datawritten)
{
  // Yes, we are going to write...
  datawritten = true;

  // write now
  output_->NewStep(step_, (*time_)[0]);
  output_->WriteVector("temperature", (*temp_)(0));
  output_->WriteVector("rate", (*rate_)(0));
  // owner of elements is just written once because it does not change during simulation (so far)
  output_->WriteElementData(firstoutputofrun_);
  firstoutputofrun_ = false;

  // leave for good
  return;
}  // OutputState()


/*----------------------------------------------------------------------*
 | heatflux calculation and output                          bborn 06/08 |
 | originally by lw                                                     |
 *----------------------------------------------------------------------*/
void THR::TimInt::OutputHeatfluxTempgrad(bool& datawritten)
{
  // create the parameters for the discretization
  Teuchos::ParameterList p;
  // action for elements
  p.set("action", "proc_thermo_heatflux");
  // other parameters that might be needed by the elements
  p.set("total time", (*time_)[0]);
  p.set("delta time", (*dt_)[0]);
  p.set<int>("young_temp", young_temp_);

  Teuchos::RCP<std::vector<char> > heatfluxdata
    = Teuchos::rcp(new std::vector<char>());
  p.set("heatflux", heatfluxdata);
  p.set<int>("ioheatflux", writeheatflux_);

  Teuchos::RCP<std::vector<char> > tempgraddata
    = Teuchos::rcp(new std::vector<char>());
  p.set("tempgrad", tempgraddata);
  p.set<int>("iotempgrad", writetempgrad_);

  // set vector values needed by elements
  discret_->ClearState();
  // SetState(0,...) in case of multiple dofsets (e.g. TSI)
  discret_->SetState(0, "residual temperature", zeros_);
  discret_->SetState(0, "temperature", (*temp_)(0));
  // set displacements and velocities for the coupled TSI problem
  if(disn_!=Teuchos::null)
  {
    discret_->SetState(1,"displacement",disn_);
  }
  if(veln_!=Teuchos::null)
  {
    discret_->SetState(1,"velocity",veln_);
  }
  discret_->Evaluate(p, Teuchos::null, Teuchos::null,
                     Teuchos::null, Teuchos::null, Teuchos::null);
  discret_->ClearState();

  // Make new step
  if (not datawritten)
  {
    output_->NewStep(step_, (*time_)[0]);
  }
  datawritten = true;

  // write heatflux
  if (writeheatflux_ != INPAR::THR::heatflux_none)
  {
    std::string heatfluxtext = "";
    if (writeheatflux_ == INPAR::THR::heatflux_current)
    {
     heatfluxtext = "gauss_current_heatfluxes_xyz";
    }
    else if (writeheatflux_ == INPAR::THR::heatflux_initial)
    {
      heatfluxtext = "gauss_initial_heatfluxes_xyz";
    }
    else
    {
      dserror("requested heatflux type not supported");
    }
    output_->WriteVector(heatfluxtext, *heatfluxdata,
                         *(discret_->ElementColMap()));
  }

  // write temperature gradient
  if (writetempgrad_ != INPAR::THR::tempgrad_none)
  {
    std::string tempgradtext = "";
    if (writetempgrad_ == INPAR::THR::tempgrad_current)
    {
      tempgradtext = "gauss_current_tempgrad_xyz";
    }
    else if (writetempgrad_ == INPAR::THR::tempgrad_initial)
    {
      tempgradtext = "gauss_initial_tempgrad_xyz";
    }
    else
    {
      dserror("requested tempgrad type not supported");
    }
    output_->WriteVector(tempgradtext, *tempgraddata,
                         *(discret_->ElementColMap()));
  }

  // leave me alone
  return;
}  // OutputHeatfluxTempgrad()


/*----------------------------------------------------------------------*
 | output system energies                                   bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::OutputEnergy()
{
  // internal/tempgrad energy
  double intergy = 0.0;  // total internal energy
  {
    Teuchos::ParameterList p;
    // other parameters needed by the elements
    p.set("action", "calc_thermo_energy");

    // set vector values needed by elements
    discret_->ClearState();
    // SetState(0,...) in case of multiple dofsets (e.g. TSI)
    discret_->SetState(0, "temperature", (*temp_)(0));
    // get energies
    Teuchos::RCP<Epetra_SerialDenseVector> energies
      = Teuchos::rcp(new Epetra_SerialDenseVector(1));
    discret_->EvaluateScalars(p, energies);
    discret_->ClearState();
    intergy = (*energies)(0);
  }

//  // global calculation of kinetic energy
//  double kinergy = 0.0;  // total kinetic energy
//  {
//    Teuchos::RCP<Epetra_Vector> linmom
//      = LINALG::CreateVector(*dofrowmap_, true);
//    capa_->Multiply(false, (*rate_)[0], *linmom);
//    linmom->Dot((*rate_)[0], &kinergy);
//    kinergy *= 0.5;
//  }

//  // external energy
//  //double extergy = 0.0;  // total external energy
//  {
//    // WARNING: This will only work with dead loads!!!
//    Teuchos::RCP<Epetra_Vector> fext = Fext();
//    fext->Dot((*temp_)[0], &extergy);
//  }

  // total energy
  //double totergy = kinergy + intergy - extergy;

  // the output
  if (myrank_ == 0)
  {
    *energyfile_ << " " << std::setw(9) << step_
                 << std::scientific  << std::setprecision(16)
                 << " " << (*time_)[0]
                 << " " << intergy
                 << std::endl;
  }

  // in God we trust
  return;
}  // OutputEnergy()


/*----------------------------------------------------------------------*
 | thermal result test                                       dano 01/12 |
 *----------------------------------------------------------------------*/
Teuchos::RCP<DRT::ResultTest> THR::TimInt::CreateFieldTest()
{
  return Teuchos::rcp(new THR::ResultTest(*this));
}  // CreateFieldTest()


/*----------------------------------------------------------------------*
 | evaluate external forces at t_{n+1}                      bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyForceExternal(
  const double time,  //!< evaluation time
  const Teuchos::RCP<Epetra_Vector> temp,  //!< temperature state
  Teuchos::RCP<Epetra_Vector>& fext  //!< external force
  )
{
  Teuchos::ParameterList p;
  // action for elements
  const std::string action = "calc_thermo_fext";
  p.set("action", action);
  // type of calling time integrator
  p.set<int>("time integrator", MethodName());
  // other parameters needed by the elements
  p.set("total time", time);

  // set vector values needed by elements
  discret_->ClearState();
  // SetState(0,...) in case of multiple dofsets (e.g. TSI)
  discret_->SetState(0, "temperature", temp);
  // get load vector
  discret_->EvaluateNeumann(p, *fext);
  discret_->ClearState();

  // go away
  return;
}  // ApplyForceExternal()


/*----------------------------------------------------------------------*
 | evaluate convection boundary conditions at t_{n+1}        dano 01/11 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyForceExternalConv(
  Teuchos::ParameterList& p,
  const double time,  //!< evaluation time
  const Teuchos::RCP<Epetra_Vector> tempn,  //!< temperature state T_n
  const Teuchos::RCP<Epetra_Vector> temp,  //!< temperature state T_n+1
  Teuchos::RCP<Epetra_Vector>& fext,  //!< external force
  Teuchos::RCP<LINALG::SparseOperator> tang  //!< tangent at time n+1
  )
{
  // for heat convection von Neumann boundary conditions, i.e. q_c^, the
  // calculation depends on the deformation, i.e. differentiation between
  // geo_linear and geo_nonlinear is required
  // - geo_linear:
  //   - use CalculateFindCondCapa(), contribution to linearisation for k_TT
  // geo_nonlinear:
  //   - use CalculateNlnFindCondCapa() considering deformation d_{n+1}
  //   - contribution due to linearisation for k_TT AND k_Td

  // action for elements
  const std::string action = "calc_thermo_fextconvection";
  p.set("action", action);
  // type of calling time integrator
  p.set<int>("time integrator", MethodName());
  // other parameters needed by the elements
  p.set("total time", time);

  // set vector values needed by elements
  discret_->ClearState();
  discret_->SetState(0,"old temperature", tempn);  // T_n (*temp_)(0)
  discret_->SetState(0,"temperature", temp);  // T_{n+1} tempn_
  // for geometrically nonlinear analysis the displacements are required
  if(disn_!=Teuchos::null)
  {
    discret_->SetState(1,"displacement", disn_);  // d_{n+1}
  }
  // get load vector
  // use general version of EvaluateCondition(), following the example set by ScaTra::EvaluateElectrodeKinetics()
  std::string condstring("ThermoConvections");
  discret_->EvaluateCondition(p,tang,Teuchos::null,fext,Teuchos::null,Teuchos::null,condstring);
  discret_->ClearState();

  // go away
  return;
}  // ApplyForceExternalConv()


/*----------------------------------------------------------------------*
 | evaluate ordinary internal force, its tangent at state   bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyForceTangInternal(
  Teuchos::ParameterList& p,
  const double time,
  const double dt,
  const Teuchos::RCP<Epetra_Vector> temp,  //!< temperature state
  const Teuchos::RCP<Epetra_Vector> tempi,  //!< residual temperature
  Teuchos::RCP<Epetra_Vector> fint,  //!< internal force
  Teuchos::RCP<LINALG::SparseMatrix> tang  //!< tangent matrix
  )
{
  // type of calling time integrator
  p.set<int>("time integrator", MethodName());
  // action for elements
  const std::string action = "calc_thermo_fintcond";
  p.set("action", action);
  // other parameters that might be needed by the elements
  p.set("total time", time);
  p.set("delta time", dt);
  p.set<int>("young_temp", young_temp_);
  // set vector values needed by elements
  discret_->ClearState();
  // SetState(0,...) in case of multiple dofsets (e.g. TSI)
  discret_->SetState(0, "residual temperature", tempi);
  discret_->SetState(0, "temperature", temp);
  // set displacements and velocities for the coupled TSI problem
  if(disn_!=Teuchos::null)
  {
    discret_->SetState(1,"displacement",disn_);
  }
  if(veln_!=Teuchos::null)
  {
    discret_->SetState(1,"velocity",veln_);
  }
  discret_->Evaluate(p, tang, Teuchos::null, fint, Teuchos::null, Teuchos::null);
  discret_->ClearState();

  // that's it
  return;
}  // ApplyForceTangInternal()


/*----------------------------------------------------------------------*
 | evaluate ordinary internal force, its tangent at state   bborn 10/09 |
 | overloaded function specified for ost time integration               |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyForceTangInternal(
  Teuchos::ParameterList& p,
  const double time,
  const double dt,
  const Teuchos::RCP<Epetra_Vector> temp,  //!< temperature state
  const Teuchos::RCP<Epetra_Vector> tempi,  //!< residual temperature
  Teuchos::RCP<Epetra_Vector> fcap,  //!< stored force
  Teuchos::RCP<Epetra_Vector> fint,  //!< internal force
  Teuchos::RCP<LINALG::SparseMatrix> tang  //!< tangent matrix
  )
{
  // type of calling time integrator
  p.set<int>("time integrator", MethodName());
  // action for elements
  const std::string action = "calc_thermo_finttang";
  p.set("action", action);
  // other parameters that might be needed by the elements
  p.set("total time", time);
  p.set("delta time", dt);
  p.set<int>("young_temp", young_temp_);
  // set vector values needed by elements
  discret_->ClearState();
  // SetState(0,...) in case of multiple dofsets (e.g. TSI)
  discret_->SetState(0,"residual temperature", tempi);
  discret_->SetState(0,"temperature", temp);
  // set displacements and velocities for the coupled TSI problem
  if(disn_!=Teuchos::null)
  {
    discret_->SetState(1,"displacement", disn_);
  }
  if(veln_!=Teuchos::null)
  {
    discret_->SetState(1,"velocity", veln_);
  }
  // call the element Evaluate()
  discret_->Evaluate(p, tang, Teuchos::null, fint, Teuchos::null, fcap);
  discret_->ClearState();

  // that's it
  return;
}  // ApplyForceTangInternal()


/*----------------------------------------------------------------------*
 | evaluate ordinary internal force                         bborn 06/08 |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyForceInternal(
  Teuchos::ParameterList& p,
  const double time,
  const double dt,
  const Teuchos::RCP<Epetra_Vector> temp,  //!< temperature state
  const Teuchos::RCP<Epetra_Vector> tempi,  //!< incremental temperature
  Teuchos::RCP<Epetra_Vector> fint  //!< internal force
  )
{
  // type of calling time integrator
  p.set<int>("time integrator", MethodName());
  // action for elements
  const std::string action = "calc_thermo_fint";
  p.set("action", action);
  // other parameters that might be needed by the elements
  p.set("total time", time);
  p.set("delta time", dt);
  p.set<int>("young_temp", young_temp_);
  // set vector values needed by elements
  discret_->ClearState();
  // SetState(0,...) in case of multiple dofsets (e.g. TSI)
  discret_->SetState(0, "residual temperature", tempi);  // these are incremental
  discret_->SetState(0, "temperature", temp);
  // set displacements and velocities for the coupled TSI problem
  if(disn_!=Teuchos::null)
  {
    discret_->SetState(1,"displacement", disn_);
  }
  if(veln_!=Teuchos::null)
  {
    discret_->SetState(1,"velocity", veln_);
  }
  // call the element Evaluate()
  discret_->Evaluate(p, Teuchos::null, Teuchos::null,
                     fint, Teuchos::null, Teuchos::null);
  discret_->ClearState();

  // where the fun starts
  return;
}  // ApplyForceTangInternal()


/*----------------------------------------------------------------------*
 | get current displacements and  velocities from the        dano 05/10 |
 | structure discretization                                             |
 *----------------------------------------------------------------------*/
void THR::TimInt::ApplyStructVariables(
  Teuchos::RCP<const Epetra_Vector> disp,  ///< the current displacements
  Teuchos::RCP<const Epetra_Vector> vel  ///< the current velocities
  )
{
  // displacements
  if (disn_ == Teuchos::null)
    disn_ = LINALG::CreateVector(*(discret_->DofRowMap(1)), true);

  if( (disp != Teuchos::null) && (disn_->Map().SameAs(disp->Map())) )
  {
    // displacements D at chosen time t dependent on call in coupled algorithm
    disn_->Update(1.0, *disp, 0.0);
  }
  else dserror("no displacements available for TSI or maps not equal");

  // velocities
  if (veln_ == Teuchos::null)
    veln_ = LINALG::CreateVector(*(discret_->DofRowMap(1)), true);

  if( (vel != Teuchos::null) && (veln_->Map().SameAs(disp->Map())) )
  {
    // velocities V at chosen time t dependent on call in coupled algorithm
    veln_->Update(1.0, *vel, 0.0);
  }
  else dserror("no velocities available for TSI or maps not equal");

  // where the fun starts
  return;

}  // ApplyStructVariables()


/*----------------------------------------------------------------------*
 | set initial field for temperature (according to ScaTra)   dano 06/10 |
 *----------------------------------------------------------------------*/
void THR::TimInt::SetInitialField(
  const INPAR::THR::InitialField init,
  const int startfuncno
  )
{
  switch(init)
  {
  case INPAR::THR::initfield_zero_field:
  {
    // extract temperature vector at time t_n (temp_ contains various vectors of
    // old(er) temperatures and is of type TimIntMStep<Epetra_Vector>)
    (*temp_)(0)->PutScalar(0.0);
    tempn_->PutScalar(0.0);
    break;
  }  // initfield_zero_field

  case INPAR::THR::initfield_field_by_function:
  {
    const Epetra_Map* dofrowmap = discret_->DofRowMap();

    // loop all nodes on the processor
    for(int lnodeid=0;lnodeid<discret_->NumMyRowNodes();lnodeid++)
    {
      // get the processor local node
      DRT::Node* lnode = discret_->lRowNode(lnodeid);
      // the set of degrees of freedom associated with the node
      std::vector<int> nodedofset = discret_->Dof(0,lnode);

      int numdofs = nodedofset.size();
      for (int k=0;k< numdofs;++k)
      {
        const int dofgid = nodedofset[k];
        int doflid = dofrowmap->LID(dofgid);
        // evaluate component k of spatial function
        double initialval
          = DRT::Problem::Instance()->Funct(startfuncno-1).Evaluate(k,lnode->X(),0.0,NULL);
        // extract temperature vector at time t_n (temp_ contains various vectors of
        // old(er) temperatures and is of type TimIntMStep<Epetra_Vector>)
        int err1 = (*temp_)(0)->ReplaceMyValues(1,&initialval,&doflid);
        if (err1 != 0) dserror("dof not on proc");
        // initialize also the solution vector. These values are a pretty good
        // guess for the solution after the first time step (much better than
        // starting with a zero vector)
        int err2 = tempn_->ReplaceMyValues(1,&initialval,&doflid);
        if (err2 != 0) dserror("dof not on proc");
      }
    }
    break;
  }  // initfield_field_by_function

  case INPAR::THR::initfield_field_by_condition:
  {
    // access the initial field condition
    std::vector<DRT::Condition*> cond;
    discret_->GetCondition("InitialField", cond);

    const Epetra_Map* dofrowmap = discret_->DofRowMap();

    for (unsigned i=0; i<cond.size(); ++i)
    {
      cout<<"Applied InitialField Condition "<<i<<endl;

      // loop all nodes on the processor
      for(int lnodeid=0;lnodeid<discret_->NumMyRowNodes();lnodeid++)
      {
        // get the processor local node
        DRT::Node* lnode = discret_->lRowNode(lnodeid);

        std::vector<DRT::Condition*> mycond;
        lnode->GetCondition("InitialField",mycond);

        if (mycond.size()>0)
        {
          // the set of degrees of freedom associated with the node
          std::vector<int> nodedofset = discret_->Dof(lnode);

          int numdofs = nodedofset.size();
          for (int k=0;k< numdofs;++k)
          {
            // set 1.0 as initial value if node belongs to condition
            double temp0 = 1.0;
            // set initial value
            const int dofgid = nodedofset[k];
            int doflid = dofrowmap->LID(dofgid);
            // extract temperature vector at time t_n (temp_ contains various vectors of
            // old(er) temperatures and is of type TimIntMStep<Epetra_Vector>)
            Teuchos::RCP<Epetra_Vector> vec = (*temp_)(0);
            int err1 = vec->ReplaceMyValues(1,&temp0,&doflid);
            if (err1 != 0) dserror("dof not on proc");
            // initialize also the solution vector. These values are a pretty good guess for the
            // solution after the first time step (much better than starting with a zero vector)
            int err2 = tempn_->ReplaceMyValues(1,&temp0,&doflid);
            if (err2 != 0) dserror("dof not on proc");
          }
        }
      }
    }
    break;
  }  // initfield_field_by_condition

  default:
    dserror("Unknown option for initial field: %d", init);
  } // switch(init)

  return;
}  // SetInitialField()


/*----------------------------------------------------------------------*
 | apply interface loads to the thermo field                ghamm 12/10 |
 *----------------------------------------------------------------------*/
void THR::TimInt::SetForceInterface(Teuchos::RCP<Epetra_Vector> ithermoload)
{
  fifc_->Update(1.0, *ithermoload, 0.0);
}  // SetForceInterface()


/*----------------------------------------------------------------------*/
