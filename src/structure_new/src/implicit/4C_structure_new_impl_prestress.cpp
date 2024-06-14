/*----------------------------------------------------------------------*/
/*! \file
\brief Definition of a time integrator for prestressing

\level 3

*/
/*----------------------------------------------------------------------*/

#include "4C_structure_new_impl_prestress.hpp"

#include "4C_global_data.hpp"
#include "4C_io.hpp"
#include "4C_io_pstream.hpp"
#include "4C_linalg_utils_sparse_algebra_create.hpp"
#include "4C_structure_new_model_evaluator.hpp"
#include "4C_structure_new_timint_basedataglobalstate.hpp"
#include "4C_structure_new_timint_basedatasdyn.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  inline bool IsMaterialIterative()
  {
    return Teuchos::getIntegralValue<Inpar::STR::PreStress>(
               Global::Problem::Instance()->structural_dynamic_params(), "PRESTRESS") ==
           Inpar::STR::PreStress::material_iterative;
  }

  inline bool IsMaterialIterativeActive(const double currentTime)
  {
    Inpar::STR::PreStress pstype = Teuchos::getIntegralValue<Inpar::STR::PreStress>(
        Global::Problem::Instance()->structural_dynamic_params(), "PRESTRESS");
    double pstime =
        Global::Problem::Instance()->structural_dynamic_params().get<double>("PRESTRESSTIME");
    return pstype == Inpar::STR::PreStress::material_iterative && currentTime <= pstime + 1.0e-15;
  }

  static inline bool IsMulfActive(const double currentTime)
  {
    Inpar::STR::PreStress pstype = Teuchos::getIntegralValue<Inpar::STR::PreStress>(
        Global::Problem::Instance()->structural_dynamic_params(), "PRESTRESS");
    double pstime =
        Global::Problem::Instance()->structural_dynamic_params().get<double>("PRESTRESSTIME");
    return pstype == Inpar::STR::PreStress::mulf && currentTime <= pstime + 1.0e-15;
  }
}  // namespace

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
STR::IMPLICIT::PreStress::PreStress() : absolute_displacement_norm_(1e9)
{
  // empty constructor
}


void STR::IMPLICIT::PreStress::write_restart(
    Core::IO::DiscretizationWriter& iowriter, const bool& forced_writerestart) const
{
  check_init_setup();

  const auto zeros = Teuchos::rcp(new Epetra_Vector(*global_state().dof_row_map_view(), true));

  // write zero dynamic forces (for dynamic restart after  static prestressing)
  iowriter.write_vector("finert", zeros);
  iowriter.write_vector("fvisco", zeros);

  model_eval().write_restart(iowriter, forced_writerestart);
}

void STR::IMPLICIT::PreStress::update_step_state()
{
  check_init_setup();

  // Compute norm of the displacements
  global_state().get_dis_np()->NormInf(&absolute_displacement_norm_);

  if (!is_material_iterative_prestress_converged())
  {
    // Only update prestress if the material iterative prestress is not converged
    // update model specific variables
    model_eval().update_step_state(0.0);
  }
}

void STR::IMPLICIT::PreStress::update_step_element()
{
  check_init_setup();

  if (!is_material_iterative_prestress_converged())
  {
    // Only update prestress if the material iterative prestress is not converged
    model_eval().update_step_element();
  }
}

void STR::IMPLICIT::PreStress::post_update()
{
  // Check for prestressing
  if (IsMulfActive(global_state().get_time_n()))

  {
    if (global_state().get_my_rank() == 0)
      Core::IO::cout << "====== Resetting Displacements" << Core::IO::endl;
    // This is a MULF step, hence we do not update the displacements at the end of the
    // timestep. This is achieved by resetting the displacements, velocities and
    // accelerations.
    global_state().get_dis_n()->PutScalar(0.0);
    global_state().get_vel_n()->PutScalar(0.0);
    global_state().get_acc_n()->PutScalar(0.0);
  }
  else if (IsMaterialIterativeActive(global_state().get_time_n()))
  {
    // Print prestress status update
    if (global_state().get_my_rank() == 0)
    {
      Core::IO::cout << "====== Iterative Prestress Status" << Core::IO::endl;
      Core::IO::cout << "abs-dis-inf-norm:                    " << absolute_displacement_norm_
                     << Core::IO::endl;
    }
  }
}

bool STR::IMPLICIT::PreStress::is_material_iterative_prestress_converged() const
{
  return IsMaterialIterative() &&
         global_state().get_step_n() >= s_dyn().get_pre_stress_minimum_number_of_load_steps() &&
         absolute_displacement_norm_ < s_dyn().get_pre_stress_displacement_tolerance();
}

bool STR::IMPLICIT::PreStress::early_stopping() const
{
  check_init_setup();

  if (is_material_iterative_prestress_converged())
  {
    if (global_state().get_my_rank() == 0)
    {
      Core::IO::cout << "Prestress is converged. Stopping simulation." << Core::IO::endl;
      Core::IO::cout << "abs-dis-inf-norm:                    " << absolute_displacement_norm_
                     << Core::IO::endl;
    }
    return true;
  }

  return false;
}

void STR::IMPLICIT::PreStress::post_time_loop()
{
  if (IsMaterialIterative())
  {
    if (absolute_displacement_norm_ > sdyn().get_pre_stress_displacement_tolerance())
    {
      FOUR_C_THROW(
          "Prestress algorithm did not converged within the given timesteps. "
          "abs-dis-inf-norm is "
          "%f",
          absolute_displacement_norm_);
    }
  }
}

FOUR_C_NAMESPACE_CLOSE