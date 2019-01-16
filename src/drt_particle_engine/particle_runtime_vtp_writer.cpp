/*---------------------------------------------------------------------------*/
/*!
\file particle_runtime_vtp_writer.cpp

\brief write visualization output for particles in vtk/vtp format at runtime

\level 3

\maintainer  Sebastian Fuchs
             fuchs@lnm.mw.tum.de
             http://www.lnm.mw.tum.de
             089 - 289 -15262

*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | headers                                                    sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
#include "particle_runtime_vtp_writer.H"

#include "../drt_io/runtime_vtp_writer.H"
#include "../drt_io/io.H"

#include "../drt_lib/drt_globalproblem.H"
#include "../drt_lib/drt_dserror.H"

/*---------------------------------------------------------------------------*
 | constructor                                                sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
PARTICLEENGINE::ParticleRuntimeVtpWriter::ParticleRuntimeVtpWriter(const Epetra_Comm& comm)
    : comm_(comm), setuptime_(0.0)
{
  // empty constructor
}

/*---------------------------------------------------------------------------*
 | destructor                                                 sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
PARTICLEENGINE::ParticleRuntimeVtpWriter::~ParticleRuntimeVtpWriter()
{
  // note: destructor declaration here since at compile-time a complete type
  // of class T as used in class member std::unique_ptr<T> ptr_T_ is required
}

/*---------------------------------------------------------------------------*
 | init particle runtime vtp writer                           sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::Init(
    const ParticleContainerBundleShrdPtr particlecontainerbundle)
{
  // set particle container bundle
  particlecontainerbundle_ = particlecontainerbundle;

  // insert specific particle states in black list
  blackliststates_.insert({PARTICLEENGINE::DensitySum, PARTICLEENGINE::DensityDot});
  blackliststates_.insert(PARTICLEENGINE::TemperatureDot);
  blackliststates_.insert(
      {PARTICLEENGINE::LastTransferPosition, PARTICLEENGINE::ReferencePosition});
  blackliststates_.insert({PARTICLEENGINE::ModifiedVelocity, PARTICLEENGINE::ModifiedAcceleration});
  blackliststates_.insert({PARTICLEENGINE::InterfaceNormal, PARTICLEENGINE::UnitWallNormal,
      PARTICLEENGINE::WallDistance});
}

/*---------------------------------------------------------------------------*
 | setup particle runtime vtp writer                          sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::Setup(
    bool write_binary_output, bool write_ghosted_particles)
{
  // determine size of vector indexed by particle types
  const int typevectorsize = *(--particlecontainerbundle_->GetParticleTypes().end()) + 1;

  // allocate memory to hold particle types
  runtime_vtpwriters_.resize(typevectorsize);

  // determine path of output directory
  const std::string outputfilename(DRT::Problem::Instance()->OutputControlFile()->FileName());
  size_t pos = outputfilename.find_last_of("/");
  if (pos == outputfilename.npos)
    pos = 0ul;
  else
    pos++;
  const std::string output_directory_path(outputfilename.substr(0ul, pos));

  // we need a better upper bound for total number of time steps here
  // however, this 'only' affects the number of leading zeros in the vtk file names
  const unsigned int max_number_timesteps_to_be_written = 1.0e+6;

  // construct and initialize all vtp writer objects
  std::shared_ptr<RuntimeVtpWriter> runtime_vtpwriter;

  // iterate over particle types
  for (auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // allocate memory for vtp writer objects of owned and ghosted states
    (runtime_vtpwriters_[typeEnum]).resize(2);

    // iterate over particle statuses
    for (auto& statusEnum : {PARTICLEENGINE::Owned, PARTICLEENGINE::Ghosted})
    {
      if (statusEnum == PARTICLEENGINE::Ghosted and write_ghosted_particles == false) continue;

      // construct vtp writer object for current particle type and status
      runtime_vtpwriter = std::make_shared<RuntimeVtpWriter>();

      // particle field name
      std::ostringstream particlefieldname;
      particlefieldname << "particle-" << PARTICLEENGINE::EnumToTypeName(typeEnum) << "-"
                        << PARTICLEENGINE::EnumToStatusName(statusEnum);

      // initialize vtp writer object
      runtime_vtpwriter->Initialize(comm_.MyPID(), comm_.NumProc(),
          max_number_timesteps_to_be_written, output_directory_path,
          DRT::Problem::Instance()->OutputControlFile()->FileNameOnlyPrefix(),
          particlefieldname.str(), DRT::Problem::Instance()->OutputControlFile()->RestartName(),
          setuptime_, write_binary_output);

      // insert into data structure holding all vtp writer objects for each particle type and status
      (runtime_vtpwriters_[typeEnum])[statusEnum] = runtime_vtpwriter;
    }
  }
}

/*---------------------------------------------------------------------------*
 | write restart of runtime vtp writer                        sfuchs 07/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::WriteRestart(const int step, const double time) const
{
  // nothing to do
}

/*---------------------------------------------------------------------------*
 | read restart of runtime vtp writer                         sfuchs 07/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::ReadRestart(
    const std::shared_ptr<IO::DiscretizationReader> reader)
{
  // get restart time
  setuptime_ = reader->ReadDouble("time");
}

/*---------------------------------------------------------------------------*
 | reset current simulation time and time step number         sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::ResetTimeAndTimeStep(
    double time, unsigned int timestep)
{
  // iterate over particle types
  for (auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // iterate over particle statuses
    for (auto& statusEnum : {PARTICLEENGINE::Owned, PARTICLEENGINE::Ghosted})
    {
      // get runtime vtp writer for current particle type and status
      std::shared_ptr<RuntimeVtpWriter> runtime_vtpwriter =
          (runtime_vtpwriters_[typeEnum])[statusEnum];

      // check for runtime vtp writer for current particle type and status
      if (not runtime_vtpwriter) continue;

      // particle field name
      std::ostringstream particlefieldname;
      particlefieldname << "particle-" << PARTICLEENGINE::EnumToTypeName(typeEnum) << "-"
                        << PARTICLEENGINE::EnumToStatusName(statusEnum);

      runtime_vtpwriter->SetupForNewTimeStepAndGeometry(time, timestep, particlefieldname.str());
    }
  }
}

/*---------------------------------------------------------------------------*
 | set positions and states of particles                      sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::SetParticlePositionsAndStates()
{
  // iterate over particle types
  for (auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // iterate over particle statuses
    for (auto& statusEnum : {PARTICLEENGINE::Owned, PARTICLEENGINE::Ghosted})
    {
      // get runtime vtp writer for current particle type and status
      std::shared_ptr<RuntimeVtpWriter> runtime_vtpwriter =
          (runtime_vtpwriters_[typeEnum])[statusEnum];

      // check for runtime vtp writer for current particle type and status
      if (not runtime_vtpwriter) continue;

      // get container of current particle type and status
      ParticleContainerShrdPtr container =
          particlecontainerbundle_->GetSpecificContainer(typeEnum, statusEnum);

      // get number of particles stored in container
      int particlestored = container->ParticlesStored();

      // get particle states stored in container
      const std::set<StateEnum>& particlestates = container->GetStoredStates();

      // safety check
      if (not particlestates.count(PARTICLEENGINE::Position))
        dserror("particle state '%s' not found!",
            PARTICLEENGINE::EnumToStateName(PARTICLEENGINE::Position).c_str());

      // iterate over particle states
      for (auto& particleState : particlestates)
      {
        // get particle state dimension
        int statedim = container->GetParticleStateDim(particleState);

        // get name of particle state
        std::string statename = PARTICLEENGINE::EnumToStateName(particleState);

        // get pointer to particle state
        double* state_ptr = nullptr;
        if (particlestored > 0) state_ptr = container->GetPtrToParticleState(particleState, 0);

        if (particleState == PARTICLEENGINE::Position)
        {
          // get and prepare storage for position data
          std::vector<double>& positiondata = runtime_vtpwriter->GetMutablePointCoordinateVector();
          positiondata.clear();
          positiondata.reserve(statedim * particlestored);

          // copy particle position data
          if (particlestored > 0)
            for (int i = 0; i < (statedim * particlestored); ++i)
              positiondata.push_back(state_ptr[i]);

          // safety check
          if ((int)positiondata.size() != statedim * particlestored)
            dserror("ParticleRuntimeVtpWriter expected %d coordinate values, but got %d!",
                statedim * particlestored, positiondata.size());
        }
        else if (not blackliststates_.count(particleState))
        {
          // prepare particle state data
          std::vector<double> statedata;
          statedata.reserve(statedim * particlestored);

          // copy particle state data
          if (particlestored > 0)
            for (int i = 0; i < (statedim * particlestored); ++i) statedata.push_back(state_ptr[i]);

          // append particle state data to vtp writer
          runtime_vtpwriter->AppendVisualizationPointDataVector(statedata, statedim, statename);
        }
      }

      // prepare particle global id data
      std::vector<double> globaliddata;
      globaliddata.reserve(particlestored);

      // copy particle global id data
      if (particlestored > 0)
      {
        // get pointer to global id of particles
        int* globalids = container->GetPtrToParticleGlobalID(0);

        for (int i = 0; i < particlestored; ++i) globaliddata.push_back(globalids[i]);
      }

      // append global id of particles to vtp writer
      runtime_vtpwriter->AppendVisualizationPointDataVector(globaliddata, 1, "globalid");

      // append owner of particles to vtp writer
      std::vector<double> ownerdata(particlestored, comm_.MyPID());
      runtime_vtpwriter->AppendVisualizationPointDataVector(ownerdata, 1, "owner");
    }
  }
}

/*---------------------------------------------------------------------------*
 | write all required vtp files to filesystem                 sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::WriteFiles()
{
  // iterate over particle types
  for (auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // iterate over particle statuses
    for (auto& statusEnum : {PARTICLEENGINE::Owned, PARTICLEENGINE::Ghosted})
    {
      // get runtime vtp writer for current particle type and status
      std::shared_ptr<RuntimeVtpWriter> runtime_vtpwriter =
          (runtime_vtpwriters_[typeEnum])[statusEnum];

      // check for runtime vtp writer for current particle type and status
      if (not runtime_vtpwriter) continue;

      runtime_vtpwriter->WriteFiles();
    }
  }
}

/*---------------------------------------------------------------------------*
 | write a vtp collection file to filesystem                  sfuchs 03/2018 |
 *---------------------------------------------------------------------------*/
void PARTICLEENGINE::ParticleRuntimeVtpWriter::WriteCollectionFileOfAllWrittenFiles()
{
  // iterate over particle types
  for (auto& typeEnum : particlecontainerbundle_->GetParticleTypes())
  {
    // iterate over particle statuses
    for (auto& statusEnum : {PARTICLEENGINE::Owned, PARTICLEENGINE::Ghosted})
    {
      // get runtime vtp writer for current particle type and status
      std::shared_ptr<RuntimeVtpWriter> runtime_vtpwriter =
          (runtime_vtpwriters_[typeEnum])[statusEnum];

      // check for runtime vtp writer for current particle type and status
      if (not runtime_vtpwriter) continue;

      // particle field name
      std::ostringstream particlefieldname;
      particlefieldname << "particle-" << PARTICLEENGINE::EnumToTypeName(typeEnum) << "-"
                        << PARTICLEENGINE::EnumToStatusName(statusEnum);

      runtime_vtpwriter->WriteCollectionFileOfAllWrittenFiles(
          DRT::Problem::Instance()->OutputControlFile()->FileNameOnlyPrefix() + "-" +
          particlefieldname.str());
    }
  }
}
