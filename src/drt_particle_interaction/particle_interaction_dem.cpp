/*---------------------------------------------------------------------------*/
/*!
\file particle_interaction_dem.cpp

\brief discrete element method (DEM) interaction handler

\level 3

\maintainer  Sebastian Fuchs
             fuchs@lnm.mw.tum.de
             http://www.lnm.mw.tum.de
             089 - 289 -15262

*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | headers                                                    sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
#include "particle_interaction_dem.H"

#include "particle_interaction_material_handler.H"
#include "particle_interaction_utils.H"

#include "particle_interaction_dem_neighbor_pairs.H"
#include "particle_interaction_dem_contact.H"

#include "../drt_particle_engine/particle_engine_interface.H"
#include "../drt_particle_engine/particle_container.H"

#include <Teuchos_TimeMonitor.hpp>

/*---------------------------------------------------------------------------*
 | constructor                                                sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
PARTICLEINTERACTION::ParticleInteractionDEM::ParticleInteractionDEM(
    const Epetra_Comm& comm, const Teuchos::ParameterList& params)
    : PARTICLEINTERACTION::ParticleInteractionBase(comm, params), params_dem_(params.sublist("DEM"))
{
  // empty constructor
}

/*---------------------------------------------------------------------------*
 | destructor                                                 sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
PARTICLEINTERACTION::ParticleInteractionDEM::~ParticleInteractionDEM()
{
  // note: destructor declaration here since at compile-time a complete type
  // of class T as used in class member std::unique_ptr<T> ptr_T_ is required
}

/*---------------------------------------------------------------------------*
 | init particle interaction handler                          sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::Init()
{
  // call base class init
  ParticleInteractionBase::Init();

  // init neighbor pair handler
  InitNeighborPairHandler();

  // init contact handler
  InitContactHandler();
}

/*---------------------------------------------------------------------------*
 | setup particle interaction handler                         sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::Setup(
    const std::shared_ptr<PARTICLEENGINE::ParticleEngineInterface> particleengineinterface)
{
  // call base class setup
  ParticleInteractionBase::Setup(particleengineinterface);

  // setup neighbor pair handler
  neighborpairs_->Setup(particleengineinterface);

  // setup contact handler
  contact_->Setup(particleengineinterface, particlematerial_, neighborpairs_);
}

/*---------------------------------------------------------------------------*
 | write restart of particle interaction handler              sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::WriteRestart(
    const int step, const double time) const
{
  // call base class function
  ParticleInteractionBase::WriteRestart(step, time);

  // write restart of neighbor pair handler
  neighborpairs_->WriteRestart(step, time);

  // write restart of contact handler
  contact_->WriteRestart(step, time);
}

/*---------------------------------------------------------------------------*
 | read restart of particle interaction handler               sfuchs 05/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::ReadRestart(
    const std::shared_ptr<IO::DiscretizationReader> reader)
{
  // call base class function
  ParticleInteractionBase::ReadRestart(reader);

  // read restart of neighbor pair handler
  neighborpairs_->ReadRestart(reader);

  // read restart of contact handler
  contact_->ReadRestart(reader);
}

/*---------------------------------------------------------------------------*
 | insert interaction dependent states of all particle types  sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::InsertParticleStatesOfParticleTypes(
    std::map<PARTICLEENGINE::TypeEnum, std::set<PARTICLEENGINE::StateEnum>>& particlestatestotypes)
{
  // iterate over particle types
  for (auto& typeIt : particlestatestotypes)
  {
    // set of particle states for current particle type
    std::set<PARTICLEENGINE::StateEnum>& particlestates = typeIt.second;

    // insert states of regular phase particles
    particlestates.insert({PARTICLEENGINE::Force, PARTICLEENGINE::Mass, PARTICLEENGINE::Radius});
  }
}

/*---------------------------------------------------------------------------*
 | set initial states                                         sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::SetInitialStates()
{
  // iterate over particle types
  for (const auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainerShrdPtr container =
        particlecontainerbundle_->GetSpecificContainer(typeEnum, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // get material for current particle type
    const MAT::PAR::ParticleMaterialBase* material =
        particlematerial_->GetPtrToParticleMatParameter(typeEnum);

    // (initial) radius of current phase
    std::vector<double> initradius(1);
    initradius[0] = material->initRadius_;

    // (initial) mass of current phase
    std::vector<double> initmass(1);
    initmass[0] = material->initDensity_ * 4.0 / 3.0 * M_PI * UTILS::pow<3>(material->initRadius_);

    // set initial mass and radius for all particles of current type
    particlecontainerbundle_->SetStateSpecificContainer(initmass, PARTICLEENGINE::Mass, typeEnum);
    particlecontainerbundle_->SetStateSpecificContainer(
        initradius, PARTICLEENGINE::Radius, typeEnum);
  }
}

/*---------------------------------------------------------------------------*
 | evaluate particle interactions                             sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::EvaluateInteractions()
{
  TEUCHOS_FUNC_TIME_MONITOR("PARTICLEINTERACTION::ParticleInteractionDEM::EvaluateInteractions");

  // clear force state of particles
  ClearForceState();

  // evaluate particle neighbor pairs
  neighborpairs_->EvaluateNeighborPairs();

  // check critical time step
  contact_->CheckCriticalTimeStep();

  // add contact contribution to force field
  contact_->AddForceContribution();

  // compute acceleration from force
  ComputeAcceleration();
}

/*---------------------------------------------------------------------------*
 | maximum interaction distance (on this processor)           sfuchs 06/2018 |
 *---------------------------------------------------------------------------*/
double PARTICLEINTERACTION::ParticleInteractionDEM::MaxInteractionDistance() const
{
  return (2.0 * MaxParticleRadius());
}

/*---------------------------------------------------------------------------*
 | set current step size                                      sfuchs 08/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::SetCurrentStepSize(const double currentstepsize)
{
  // call base class method
  ParticleInteractionBase::SetCurrentStepSize(currentstepsize);

  // set current step size
  contact_->SetCurrentStepSize(currentstepsize);
}

/*---------------------------------------------------------------------------*
 | init neighbor pair handler                                 sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::InitNeighborPairHandler()
{
  // create neighbor pair handler
  neighborpairs_ = std::make_shared<PARTICLEINTERACTION::DEMNeighborPairs>();

  // init neighbor pair handler
  neighborpairs_->Init();
}

/*---------------------------------------------------------------------------*
 | init contact handler                                       sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::InitContactHandler()
{
  // create contact handler
  contact_ = std::unique_ptr<PARTICLEINTERACTION::DEMContact>(
      new PARTICLEINTERACTION::DEMContact(params_dem_));

  // init contact handler
  contact_->Init();
}

/*---------------------------------------------------------------------------*
 | clear force state of particles                             sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::ClearForceState() const
{
  // iterate over particle types
  for (const auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainerShrdPtr container =
        particlecontainerbundle_->GetSpecificContainer(typeEnum, PARTICLEENGINE::Owned);

    // clear force of all particles
    container->ClearState(PARTICLEENGINE::Force);
  }
}

/*---------------------------------------------------------------------------*
 | compute acceleration from force                            sfuchs 11/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEINTERACTION::ParticleInteractionDEM::ComputeAcceleration() const
{
  TEUCHOS_FUNC_TIME_MONITOR("PARTICLEINTERACTION::ParticleInteractionDEM::ComputeAcceleration");

  // iterate over particle types
  for (const auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // get container of owned particles of current particle type
    PARTICLEENGINE::ParticleContainerShrdPtr container =
        particlecontainerbundle_->GetSpecificContainer(typeEnum, PARTICLEENGINE::Owned);

    // get number of particles stored in container
    const int particlestored = container->ParticlesStored();

    // no owned particles of current particle type
    if (particlestored <= 0) continue;

    // get particle state dimension
    const int statedim = container->GetParticleStateDim(PARTICLEENGINE::Acceleration);

    // get pointer to particle state
    const double* mass = container->GetPtrToParticleState(PARTICLEENGINE::Mass, 0);
    const double* force = container->GetPtrToParticleState(PARTICLEENGINE::Force, 0);
    double* acc = container->GetPtrToParticleState(PARTICLEENGINE::Acceleration, 0);

    // iterate over owned particles of current type
    for (int i = 0; i < particlestored; ++i)
      UTILS::vec_addscale(&acc[statedim * i], (1.0 / mass[i]), &force[statedim * i]);
  }
}
