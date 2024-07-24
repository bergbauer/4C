/*-----------------------------------------------------------*/
/*! \file

\brief Adapter for the new structural time integration framework.



\level 3

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_ADAPTER_STR_STRUCTURE_NEW_HPP
#define FOUR_C_ADAPTER_STR_STRUCTURE_NEW_HPP

#include "4C_config.hpp"

#include "4C_adapter_str_structure.hpp"
#include "4C_inpar_structure.hpp"

#include <set>

// forward declarations
namespace Teuchos
{
  class ParameterList;
}  // namespace Teuchos

FOUR_C_NAMESPACE_OPEN

namespace Solid
{
  namespace TimeInt
  {
    class Base;
    class BaseDataGlobalState;
    class BaseDataSDyn;
    class BaseDataIO;
  }  // namespace TimeInt
  namespace ModelEvaluator
  {
    class Generic;
  }  // namespace ModelEvaluator
}  // namespace Solid

namespace Core::LinAlg
{
  class Solver;
}  // namespace Core::LinAlg

namespace Core::FE
{
  class Discretization;
}  // namespace Core::FE

namespace Adapter
{
  class StructureNew : public Structure
  {
   public:
    /// @name General methods
    ///@{
    /// Setup the structure integrator
    void setup() override = 0;
    ///@}

    /// @name Vector access
    ///@{
    /// initial guess of Newton's method
    Teuchos::RCP<const Epetra_Vector> initial_guess() override = 0;

    /// rhs of Newton's method
    Teuchos::RCP<const Epetra_Vector> rhs() override = 0;

    /// unknown displacements at \f$t_{n+1}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> disp_np() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> dispnp() const override { return disp_np(); }
    virtual Teuchos::RCP<Epetra_Vector> write_access_disp_np() = 0;
    Teuchos::RCP<Epetra_Vector> write_access_dispnp() override { return write_access_disp_np(); }

    /// known displacements at \f$t_{n}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> disp_n() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> dispn() const override { return disp_n(); }
    virtual Teuchos::RCP<Epetra_Vector> write_access_disp_n() = 0;
    Teuchos::RCP<Epetra_Vector> write_access_dispn() override { return write_access_disp_n(); }

    /// unknown velocity at \f$t_{n+1}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> vel_np() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> velnp() const override { return vel_np(); }
    virtual Teuchos::RCP<Epetra_Vector> write_access_vel_np() = 0;
    Teuchos::RCP<Epetra_Vector> write_access_velnp() override { return write_access_vel_np(); }

    /// known velocity at \f$t_{n}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> vel_n() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> veln() const override { return vel_n(); }
    virtual Teuchos::RCP<Epetra_Vector> write_access_vel_n() = 0;
    Teuchos::RCP<Epetra_Vector> write_access_veln() override { return write_access_vel_n(); }

    /// known velocity at \f$t_{n-1}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> vel_nm() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> velnm() const override { return vel_nm(); }

    /// unknown acceleration at \f$t_{n+1}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> acc_np() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> accnp() const override { return acc_np(); }

    /// known acceleration at \f$t_{n}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual Teuchos::RCP<const Epetra_Vector> acc_n() const = 0;
    [[nodiscard]] Teuchos::RCP<const Epetra_Vector> accn() const override { return acc_n(); }

    /// resize the multi step class vector
    void resize_m_step_tim_ada() override = 0;
    ///@}

    /// @name Time step helpers
    ///@{

    /// return time integration factor
    [[nodiscard]] double tim_int_param() const override = 0;

    /// Return current time \f$t_{n}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual double get_time_n() const = 0;
    [[nodiscard]] double time_old() const override { return get_time_n(); }

    /// Sets the current time \f$t_{n}\f$
    /// ToDo Replace the deprecated version with the new version
    virtual void set_time_n(const double time_n) = 0;
    void set_time(const double time_n) override { set_time_n(time_n); }

    /// Return target time \f$t_{n+1}\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual double get_time_np() const = 0;
    [[nodiscard]] double time() const override { return get_time_np(); }

    /// Get upper limit of time range of interest
    [[nodiscard]] double get_time_end() const override = 0;

    //! Set upper limit of time range of interest
    void set_time_end(double timemax) override = 0;

    /// Sets the target time \f$t_{n+1}\f$ of this time step
    /// ToDo Replace the deprecated version with the new version
    virtual void set_time_np(const double time_np) = 0;
    void set_timen(const double time_np) override { set_time_np(time_np); }

    /// Get time step size \f$\Delta t_n\f$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual double get_delta_time() const = 0;
    [[nodiscard]] double dt() const override { return get_delta_time(); }

    /// set time step size
    /// ToDo Replace the deprecated version with the new version
    virtual void set_delta_time(const double dt) = 0;
    void set_dt(const double dt) override { set_delta_time(dt); }

    /// Return current step number $n$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual int get_step_n() const = 0;
    [[nodiscard]] int step_old() const override { return get_step_n(); }

    /// Sets the current step \f$n\f$
    /// ToDo Replace the deprecated version with the new version
    virtual void set_step_n(int step_n) = 0;
    void set_step(int step_n) override { set_step_n(step_n); }

    /// Return current step number $n+1$
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual int get_step_np() const = 0;
    [[nodiscard]] int step() const override { return get_step_np(); }

    /// Sets the current step \f$n+1\f$
    /// ToDo Replace the deprecated version with the new version
    virtual void set_step_np(int step_np) = 0;
    void set_stepn(int step_np) override { set_step_np(step_np); }

    /// Get number of time steps
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual int get_step_end() const = 0;
    [[nodiscard]] int num_step() const override { return get_step_end(); }

    /// Sets number of time steps (in case of time adaptivity)
    virtual void set_step_end(int step_end) = 0;

    /// Take the time and integrate (time loop)
    /// \date 11/08
    int integrate() override = 0;

    /// fixme: this can go when the old structure time integration is gone and PerformErrorAction is
    /// only called in Solid::TimeInt::Implicit::Solve() and not on the structure in the adapter
    /// time loop
    Inpar::Solid::ConvergenceStatus perform_error_action(
        Inpar::Solid::ConvergenceStatus nonlinsoldiv) override
    {
      FOUR_C_THROW("You should not be here");
      return nonlinsoldiv;
    };

    /// tests if there are more time steps to do
    [[nodiscard]] bool not_finished() const override = 0;

    /// start new time step
    void prepare_time_step() override = 0;

    /*!
     \brief update displacement
     There are two displacement increments possible

     \f$x^n+1_i+1 = x^n+1_i + disiterinc\f$  (sometimes referred to as residual increment), and

     \f$x^n+1_i+1 = x^n     + disstepinc\f$

     with \f$n\f$ and \f$i\f$ being time and Newton iteration step

     Note: The structure expects an iteration increment.
     In case the StructureNOXCorrectionWrapper is applied, the step increment is expected
     which is then transformed into an iteration increment
     */
    void update_state_incrementally(Teuchos::RCP<const Epetra_Vector>
            disiterinc  ///< displacement increment between Newton iteration i and i+1
        ) override = 0;

    /*!
    \brief update displacement and evaluate elements

    There are two displacement increments possible

    \f$x^n+1_i+1 = x^n+1_i + disiterinc\f$  (sometimes referred to as residual increment), and

    \f$x^n+1_i+1 = x^n     + disstepinc\f$

    with \f$n\f$ and \f$i\f$ being time and Newton iteration step

    Note: The structure expects an iteration increment.
    In case the StructureNOXCorrectionWrapper is applied, the step increment is expected
    which is then transformed into an iteration increment
    */
    void evaluate(Teuchos::RCP<const Epetra_Vector>
            disiterinc  ///< displacement increment between Newton iteration i and i+1
        ) override = 0;

    /// don't update displacement but evaluate elements (implicit only)
    void evaluate() override = 0;

    /// update at time step end
    void update() override = 0;

    /// update at time step end in case of FSI time adaptivity
    void update(double endtime) override = 0;

    /// Update iteration
    /// Add residual increment to Lagrange multipliers stored in Constraint manager
    void update_iter_incr_constr(Teuchos::RCP<Epetra_Vector> lagrincr) override = 0;

    /// Update iteration
    /// Add residual increment to pressures stored in Cardiovascular0D manager
    void update_iter_incr_cardiovascular0_d(Teuchos::RCP<Epetra_Vector> presincr) override = 0;

    /// Access to output object
    Teuchos::RCP<Core::IO::DiscretizationWriter> disc_writer() override = 0;

    /// prepare output (i.e. calculate stresses, strains, energies)
    void prepare_output(bool force_prepare_timestep) override = 0;

    // Get restart data
    void get_restart_data(Teuchos::RCP<int> step, Teuchos::RCP<double> time,
        Teuchos::RCP<Epetra_Vector> disn, Teuchos::RCP<Epetra_Vector> veln,
        Teuchos::RCP<Epetra_Vector> accn, Teuchos::RCP<std::vector<char>> elementdata,
        Teuchos::RCP<std::vector<char>> nodedata) override = 0;

    /// output results
    void output(bool forced_writerestart = false) override = 0;

    /// output results to screen
    void print_step() override = 0;

    /// read restart information for given time step
    void read_restart(const int step) override = 0;

    /*!
    \brief Reset time step

    In case of time step size adaptivity, time steps might have to be repeated.
    Therefore, we need to reset the solution back to the initial solution of the
    time step.

    \author mayr.mt
    \date 08/2013
    */
    void reset_step() override = 0;

    /// set restart information for parameter continuation
    void set_restart(int step, double time, Teuchos::RCP<Epetra_Vector> disn,
        Teuchos::RCP<Epetra_Vector> veln, Teuchos::RCP<Epetra_Vector> accn,
        Teuchos::RCP<std::vector<char>> elementdata,
        Teuchos::RCP<std::vector<char>> nodedata) override = 0;

    /// wrapper for things that should be done before prepare_time_step is called
    void pre_predict() override = 0;

    /// wrapper for things that should be done before solving the nonlinear iterations
    void pre_solve() override = 0;

    /// wrapper for things that should be done before updating
    void pre_update() override = 0;

    /// wrapper for things that should be done after solving the update
    void post_update() override = 0;

    /// wrapper for things that should be done after the output
    void post_output() override = 0;

    /// wrapper for things that should be done after the actual time loop is finished
    void post_time_loop() override = 0;

    ///@}

    //! @name Solver calls

    /*!
    \brief nonlinear solve

    Do the nonlinear solve, i.e. (multiple) corrector,
    for the time step. All boundary conditions have
    been set.
    */
    Inpar::Solid::ConvergenceStatus solve() override = 0;

    /*!
    \brief linear structure solve with just a interface load

    The very special solve done in steepest descent relaxation
    calculation (and matrix free Newton Krylov).

    \note Can only be called after a valid structural solve.
    */
    Teuchos::RCP<Epetra_Vector> solve_relaxation_linear() override
    {
      FOUR_C_THROW(
          "In the new structural timeintegration this method is"
          "no longer needed inside the structure. Since this is"
          "FSI specific, the functionality is shifted to the"
          "Solid::ModelEvaluator::PartitionedFSI.");
      return Teuchos::null;
    };

    /// get the linear solver object used for this field
    Teuchos::RCP<Core::LinAlg::Solver> linear_solver() override = 0;

    //@}

    /// extract rhs (used to calculate reaction force for post-processing)
    Teuchos::RCP<Epetra_Vector> freact() override = 0;


    //! @name volume coupled specific methods
    //@{

    /// Set forces due to interface with fluid, the force is expected external-force-like
    void set_force_interface(Teuchos::RCP<Epetra_MultiVector> iforce) override
    {
      FOUR_C_THROW(
          "This method is deprecated. In the new structural time integration"
          "this functionality is taken over by the problem specific model "
          "evaluators. Remove this method as soon as possible.");
    };

    //! specific method for iterative staggered partitioned TSI

    /// Identify residual
    /// This method does not predict the target solution but
    /// evaluates the residual and the stiffness matrix.
    /// In partitioned solution schemes, it is better to keep the current
    /// solution instead of evaluating the initial guess (as the predictor)
    /// does.
    void prepare_partition_step() override = 0;

    //@}

    /// @name Structure with ale specific methods
    ///@{
    /// unknown material displacements at \f$t_{n+1}\f$
    /// ToDo Replace the deprecated version with the new version
    virtual Teuchos::RCP<Epetra_Vector> write_access_disp_mat_np() = 0;
    Teuchos::RCP<Epetra_Vector> disp_mat() override { return write_access_disp_mat_np(); }

    /// set/apply material displacements to structure field (structure with ale)
    virtual void set_disp_mat_np(Teuchos::RCP<Epetra_Vector> dispmatnp) = 0;
    void apply_dis_mat(Teuchos::RCP<Epetra_Vector> dismat) override { set_disp_mat_np(dismat); };
    ///@}

    /// @name Misc
    ///@{
    /// dof map of vector of unknowns
    Teuchos::RCP<const Epetra_Map> dof_row_map() override = 0;

    /// DOF map of vector of unknowns for multiple dofsets
    Teuchos::RCP<const Epetra_Map> dof_row_map(unsigned nds) override = 0;

    /// DOF map view of vector of unknowns
    const Epetra_Map* dof_row_map_view() override = 0;

    /// domain map of system matrix (do we really need this?)
    /// ToDo Replace the deprecated version with the new version
    [[nodiscard]] virtual const Epetra_Map& get_mass_domain_map() const = 0;
    [[nodiscard]] const Epetra_Map& domain_map() const override { return get_mass_domain_map(); }

    /// direct access to system matrix
    Teuchos::RCP<Core::LinAlg::SparseMatrix> system_matrix() override = 0;

    /// direct access to system matrix
    Teuchos::RCP<Core::LinAlg::BlockSparseMatrixBase> block_system_matrix() override = 0;

    /// switch structure field to block matrix
    void use_block_matrix(Teuchos::RCP<const Core::LinAlg::MultiMapExtractor> domainmaps,
        Teuchos::RCP<const Core::LinAlg::MultiMapExtractor> rangemaps) override = 0;

    /// return contact/meshtying bridge
    Teuchos::RCP<CONTACT::MeshtyingContactBridge> meshtying_contact_bridge() override = 0;

    /// access to locsys manager
    Teuchos::RCP<Core::Conditions::LocsysManager> locsys_manager() override = 0;

    /// access the desired model evaluator (read-only)
    [[nodiscard]] virtual const Solid::ModelEvaluator::Generic& model_evaluator(
        Inpar::Solid::ModelType mtype) const = 0;

    /// access the desired model evaluator (read and write)
    Solid::ModelEvaluator::Generic& model_evaluator(Inpar::Solid::ModelType mtype) override = 0;

    /// direct access to discretization
    Teuchos::RCP<Core::FE::Discretization> discretization() override = 0;

    /// are there any algebraic constraints?
    bool have_constraint() override = 0;

    /// get constraint manager defined in the structure
    Teuchos::RCP<CONSTRAINTS::ConstrManager> get_constraint_manager() override = 0;

    /// Get type of thickness scaling for thin shell structures
    Inpar::Solid::StcScale get_stc_algo() override = 0;

    /// Access to scaling matrix for STC
    Teuchos::RCP<Core::LinAlg::SparseMatrix> get_stc_mat() override = 0;

    /// Return MapExtractor for Dirichlet boundary conditions
    Teuchos::RCP<const Core::LinAlg::MapExtractor> get_dbc_map_extractor() override = 0;

    /// create result test for encapsulated structure algorithm
    Teuchos::RCP<Core::UTILS::ResultTest> create_field_test() override = 0;

    /// reset time and state vectors (needed for biofilm growth simulations)
    void reset() override = 0;

    /// set structure displacement vector due to biofilm growth
    void set_str_gr_disp(Teuchos::RCP<Epetra_Vector> struct_growth_disp) override = 0;

    /// bool indicating if micro material is used
    bool have_micro_mat() override = 0;

    ///@}

    /// @name Currently unused functions, which will be deleted in the near future,
    /// if they stay unnecessary.
    ///@{
    /// are there any spring dashpot bcs?
    bool have_spring_dashpot() override
    {
      FOUR_C_THROW("This function seems to be unused!");
      return false;
    }

    /// get SpringDashpot manager defined in the structure
    Teuchos::RCP<CONSTRAINTS::SpringDashpotManager> get_spring_dashpot_manager() override
    {
      FOUR_C_THROW("This function seems to be unused!");
      return Teuchos::null;
    }

    ///@}

    /// @name Multiphysics related stuff
    ///@{

    /** \brief Set the state of the NOX group and the global state data container.
     *
     * This method is needed because there are two parallel ways to handle the
     * global state in the 'new' structural time integration.
     *
     * 1) The current state is held in the global state data container:
     *    \ref Solid::TimeInt::BaseDataGlobalState
     *
     * 2) Also the NOX group (that means the nonlinear solver) has its
     *    own state vector (called 'X').
     *
     * This method sets the provided state consistently in both objects.
     *
     * This is useful for multiphysics in case a manipulated state needs
     * to be set from outside:
     *
     *   For example, see the class:
     *   \ref Immersed::ImmersedPartitionedAdhesionTraction
     *
     *   There, see the method
     *   \ref Immersed::ImmersedPartitionedAdhesionTraction::ImmersedOp
     *
     *   First, the displacement state with write access is request from
     *   the time integrator via the corresponding adapter.
     *
     *   Then, the displacement state is manipulated.
     *
     *   Finally, the displacement state is handed over to this method in
     *   order to apply the new state to both (i) the global state object;
     *   and (ii) the NOX group.
     *
     *   \note velocities and accelerations are recalculated inside by invoking
     *   set_state(x) on the concrete time integrator (e.g. OST, GenAlpha, etc.)
     *   It never makes any sense to call velocities or displacements as WriteAccess
     *   variant from outside, because these vectors should always be consistent with
     *   our primary variable (i.e. the displacements).
     *
     *  \author rauch
     *  \date 10/17
     */
    void set_state(const Teuchos::RCP<Epetra_Vector>& x) override = 0;

    ///@}

  };  // class StructureNew

  /// structure field solver
  class StructureBaseAlgorithmNew
  {
   public:
    /// constructor
    StructureBaseAlgorithmNew();

    /// virtual destructor to support polymorph destruction
    virtual ~StructureBaseAlgorithmNew() = default;

    /// initialize all class internal variables
    virtual void init(const Teuchos::ParameterList& prbdyn, Teuchos::ParameterList& sdyn,
        Teuchos::RCP<Core::FE::Discretization> actdis);

    /// setup
    virtual void setup();

    /** \brief Register an externally created model evaluator.
     *
     *  This can be used e.g. by coupled problems.
     *
     *  \date 11/16 */
    void register_model_evaluator(
        const std::string name, Teuchos::RCP<Solid::ModelEvaluator::Generic> me);

    /// structural field solver
    Teuchos::RCP<Structure> structure_field() { return str_wrapper_; }

   public:
    [[nodiscard]] inline const bool& is_init() const { return isinit_; };

    [[nodiscard]] inline const bool& is_setup() const { return issetup_; };

   protected:
    /// setup structure algorithm of Solid::TimInt::Implicit or Solid::TimInt::Explicit type
    void setup_tim_int();

    /** \brief Set all model types. This is necessary for the model evaluation.
     *
     *  The inherent structural models are identified by the corresponding conditions and/or
     *  other unique criteria. If your intention is to solve a partitioned coupled problem and
     *  you need to modify the structural right-hand-side in any way, then you have to implement
     *  your own concrete implementation of a Solid::ModelEvaluator::Generic class and register it
     *  as a Teuchos::RCP<Solid::ModelEvaluator::Generic> pointer in your problem dynamic parameter-
     *  list. For partitioned problems you have to use the parameter-name
     *  \"Partitioned Coupling Model\".
     *
     *  For example: To create and use a coupling model evaluator for the partitioned FSI you
     *  have to insert the object as follows:
     *
     *  <ol>
     *
     *  <li> Create a model evaluator that derives from
     *  Solid::ModelEvaluator::Generic. For example, the model evaluator
     *  \c FSI_Partitioned might be defined as shown below.
     *
     *  \code
     *  class FSI_Partitioned : public Solid::ModelEvaluator::Generic
     *  {
     *  // Insert class definition here
     *  }
     *  \endcode
     *
     *  <li> Create the appropriate entries in your problem dynamic parameter-list \c prbdyn
     *  and initialize member variables, if desired (optional):
     *
     *  \code
     *  Teuchos::RCP<FSI_Partitioned> fsi_model_ptr = Teuchos::rcp(new FSI_Partitioned());
     *  // optional: call of your own 2-nd init() method
     *  fsi_model_ptr->init(stuff_you_need_inside_the_model_evaluator);
     *  prbdyn.set<Teuchos::RCP<Solid::ModelEvaluator::Generic> >("Partitioned Coupling Model",
     *      fsi_model_ptr);
     *  \endcode
     *
     *  </ol>
     *
     *  \remark Please keep in mind, that the prescribed Generic::init() and Generic::setup()
     *  methods will be called automatically in the Solid::ModelEvaluatorManager::setup() routine.
     * If you need a different init() method, just define a second init() function with different
     *  input variables in your concrete class implementation and call it somewhere in your code
     *  (see upper example code).
     *  The constructor is supposed to stay empty. If you need a safety check, you can overload
     *  the Generic::check_init() and Generic::check_init_setup() routines, instead.
     *
     *  \author hiermeier
     *  \date 09/16 */
    void set_model_types(std::set<enum Inpar::Solid::ModelType>& modeltypes) const;

    /// Set all found model types.
    void detect_element_technologies(std::set<enum Inpar::Solid::EleTech>& eletechs) const;

    /// Set different time integrator specific parameters in the different parameter lists
    virtual void set_params(Teuchos::ParameterList& ioflags, Teuchos::ParameterList& xparams,
        Teuchos::ParameterList& time_adaptivity_params);

    /// Create, initialize and setup the global state data container
    virtual void set_global_state(
        Teuchos::RCP<Solid::TimeInt::BaseDataGlobalState>& dataglobalstate,
        const Teuchos::RCP<const Solid::TimeInt::BaseDataSDyn>& datasdyn_ptr);

    /// Create, initialize and setup the time integration strategy object
    virtual void set_time_integration_strategy(Teuchos::RCP<Solid::TimeInt::Base>& ti_strategy,
        const Teuchos::RCP<Solid::TimeInt::BaseDataIO>& dataio,
        const Teuchos::RCP<Solid::TimeInt::BaseDataSDyn>& datasdyn,
        const Teuchos::RCP<Solid::TimeInt::BaseDataGlobalState>& dataglobalstate,
        const int& restart);

    /// set the final structure time integrator object
    virtual void set_structure_wrapper(const Teuchos::ParameterList& ioflags,
        const Teuchos::ParameterList& sdyn, const Teuchos::ParameterList& xparams,
        const Teuchos::ParameterList& time_adaptivity_params,
        Teuchos::RCP<Solid::TimeInt::Base> ti_strategy);

    /// create the time integrator wrapper
    void create_wrapper(Teuchos::RCP<Solid::TimeInt::Base> ti_strategy);

   protected:
    /// structural field solver
    Teuchos::RCP<Structure> str_wrapper_;

    /// parameter list of the problem dynamics (read only)
    Teuchos::RCP<const Teuchos::ParameterList> prbdyn_;

    /// parameter list of the structural dynamics (mutable)
    Teuchos::RCP<Teuchos::ParameterList> sdyn_;

    /// current discretization
    Teuchos::RCP<Core::FE::Discretization> actdis_;

    /// init flag
    bool isinit_;

    /// setup flag
    bool issetup_;
  };  // class StructureBaseAlgorithmNew
}  // namespace Adapter

FOUR_C_NAMESPACE_CLOSE

#endif
