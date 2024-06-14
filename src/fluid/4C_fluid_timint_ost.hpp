/*-----------------------------------------------------------*/
/*! \file

\brief One-step theta time integration for fluids


\level 2

*/
/*-----------------------------------------------------------*/

#ifndef FOUR_C_FLUID_TIMINT_OST_HPP
#define FOUR_C_FLUID_TIMINT_OST_HPP


#include "4C_config.hpp"

#include "4C_fluid_implicit_integration.hpp"

FOUR_C_NAMESPACE_OPEN


namespace FLD
{
  class TimIntOneStepTheta : public virtual FluidImplicitTimeInt
  {
   public:
    /// Standard Constructor
    TimIntOneStepTheta(const Teuchos::RCP<Core::FE::Discretization>& actdis,
        const Teuchos::RCP<Core::LinAlg::Solver>& solver,
        const Teuchos::RCP<Teuchos::ParameterList>& params,
        const Teuchos::RCP<Core::IO::DiscretizationWriter>& output, bool alefluid = false);


    /*!
    \brief initialization

    */
    void Init() override;

    /*!
    \brief Print information about current time step to screen (reimplementation)

    */
    void print_time_step_info() override;

    /*!
    \brief Set the part of the righthandside belonging to the last
           timestep for incompressible or low-Mach-number flow

       for low-Mach-number flow: distinguish momentum and continuity part
       (continuity part only meaningful for low-Mach-number flow)

       Stationary/af-generalized-alpha:

                     mom: hist_ = 0.0
                    (con: hist_ = 0.0)

       One-step-Theta:

                     mom: hist_ = veln_  + dt*(1-Theta)*accn_
                    (con: hist_ = densn_ + dt*(1-Theta)*densdtn_)

       BDF2: for constant time step:

                     mom: hist_ = 4/3 veln_  - 1/3 velnm_
                    (con: hist_ = 4/3 densn_ - 1/3 densnm_)


    */
    void set_old_part_of_righthandside() override;

    /*!
    \brief Set states in the time integration schemes: differs between GenAlpha and the others

    */
    void SetStateTimInt() override;

    /*!
    \brief Calculate time derivatives for
           stationary/one-step-theta/BDF2/af-generalized-alpha time integration
           for incompressible and low-Mach-number flow
    */
    void calculate_acceleration(const Teuchos::RCP<const Epetra_Vector> velnp,  ///< velocity at n+1
        const Teuchos::RCP<const Epetra_Vector> veln,   ///< velocity at     n
        const Teuchos::RCP<const Epetra_Vector> velnm,  ///< velocity at     n-1
        const Teuchos::RCP<const Epetra_Vector> accn,   ///< acceleration at n-1
        const Teuchos::RCP<Epetra_Vector> accnp         ///< acceleration at n+1
        ) override;

    /*!
    \brief Set gamma to a value

    */
    void SetGamma(Teuchos::ParameterList& eleparams) override;

    /*!
    \brief Scale separation

    */
    void Sep_Multiply() override;

    /*!
    \brief Output of filtered velocity

    */
    void OutputofFilteredVel(
        Teuchos::RCP<Epetra_Vector> outvec, Teuchos::RCP<Epetra_Vector> fsoutvec) override;

    /*!

    \brief parameter (fix over a time step) are set in this method.
           Therefore, these parameter are accessible in the fluid element
           and in the fluid boundary element

    */
    void set_element_time_parameter() override;

    /*!
    \brief set theta if starting algorithm is chosen.

    */
    void SetTheta() override;

    /*!
    \brief return scheme-specific time integration parameter

    */
    double TimIntParam() const override { return 0.0; }

    /*!
    \brief return scaling factor for the residual

    */
    double residual_scaling() const override
    {
      if (params_->get<bool>("ost new"))
        return 1.0 / (dta_);
      else
        return 1.0 / (theta_ * dta_);
    }

    /*!
    \brief velocity required for evaluation of related quantites required on element level

    */
    Teuchos::RCP<const Epetra_Vector> evaluation_vel() override { return velnp_; };

    /*!
    \ apply external forces to the fluid

    */
    void ApplyExternalForces(Teuchos::RCP<Epetra_MultiVector> fext) override;

    /*!
    \output of external forces for restart

    */
    void output_external_forces() override;

    /*!
    \brief read restart data

    */
    void read_restart(int step) override;

    /*!
    \ Update of external forces

    */
    void time_update_external_forces() override;

    /*!
    \brief treat turbulence models in assemble_mat_and_rhs

    */
    void treat_turbulence_models(Teuchos::ParameterList& eleparams) override;

    //! @name Time Step Size Adaptivity
    //@{

    //! Give local order of accuracy of velocity part
    int method_order_of_accuracy_vel() const override;

    //! Give local order of accuracy of pressure part
    int method_order_of_accuracy_pres() const override;

    /*! \brief Return linear error coefficient of velocity
     *
     *  The linear discretization error reads
     *  \f[
     *  e \approx \Delta t_n^2\left(\frac{1}{2}-\theta\right)\ddot{u}(t_n)
     *    + \Delta t_n^3\left(\frac{1}{6}-\frac{\theta}{2}\right)\dddot{u}(t_n)
     *    + HOT\left(\Delta t_n^4\right)
     *  \f]
     *
     *  \author mayr.mt \date 04/2015
     */
    double method_lin_err_coeff_vel() const override;

    //@}

   protected:
    /// copy constructor
    TimIntOneStepTheta(const TimIntOneStepTheta& old);

    //! @name time stepping variables
    bool startalgo_;  ///< flag for starting algorithm

    /// the vector containing external loads at t_n
    Teuchos::RCP<Epetra_Vector> external_loadsn_;

    /// the vector containing external loads at t_{n+1}
    Teuchos::RCP<Epetra_Vector> external_loadsnp_;

   private:
  };  // class TimIntOneStepTheta

}  // namespace FLD

FOUR_C_NAMESPACE_CLOSE

#endif