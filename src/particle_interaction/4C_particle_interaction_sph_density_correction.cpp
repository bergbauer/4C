/*---------------------------------------------------------------------------*/
/*! \file
\brief density correction handler in smoothed particle hydrodynamics (SPH)
\level 3
*/
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*
 | headers                                                                   |
 *---------------------------------------------------------------------------*/
#include "4C_particle_interaction_sph_density_correction.hpp"

#include "4C_inpar_particle.hpp"
#include "4C_utils_exceptions.hpp"

FOUR_C_NAMESPACE_OPEN

/*---------------------------------------------------------------------------*
 | definitions                                                               |
 *---------------------------------------------------------------------------*/
PARTICLEINTERACTION::SPHDensityCorrectionBase::SPHDensityCorrectionBase()
{
  // empty constructor
}

void PARTICLEINTERACTION::SPHDensityCorrectionBase::Init()
{
  // nothing to do
}

void PARTICLEINTERACTION::SPHDensityCorrectionBase::Setup()
{
  // nothing to do
}

void PARTICLEINTERACTION::SPHDensityCorrectionBase::corrected_density_interior(
    const double* denssum, double* dens) const
{
  dens[0] = denssum[0];
}

PARTICLEINTERACTION::SPHDensityCorrectionInterior::SPHDensityCorrectionInterior()
    : PARTICLEINTERACTION::SPHDensityCorrectionBase()
{
  // empty constructor
}

bool PARTICLEINTERACTION::SPHDensityCorrectionInterior::ComputeDensityBC() const { return false; }

void PARTICLEINTERACTION::SPHDensityCorrectionInterior::corrected_density_free_surface(
    const double* denssum, const double* colorfield, const double* dens_bc, double* dens) const
{
  // density of free surface particles is not corrected
}

PARTICLEINTERACTION::SPHDensityCorrectionNormalized::SPHDensityCorrectionNormalized()
    : PARTICLEINTERACTION::SPHDensityCorrectionBase()
{
  // empty constructor
}

bool PARTICLEINTERACTION::SPHDensityCorrectionNormalized::ComputeDensityBC() const { return false; }

void PARTICLEINTERACTION::SPHDensityCorrectionNormalized::corrected_density_free_surface(
    const double* denssum, const double* colorfield, const double* dens_bc, double* dens) const
{
  dens[0] = denssum[0] / colorfield[0];
}

PARTICLEINTERACTION::SPHDensityCorrectionRandles::SPHDensityCorrectionRandles()
    : PARTICLEINTERACTION::SPHDensityCorrectionBase()
{
  // empty constructor
}

bool PARTICLEINTERACTION::SPHDensityCorrectionRandles::ComputeDensityBC() const { return true; }

void PARTICLEINTERACTION::SPHDensityCorrectionRandles::corrected_density_free_surface(
    const double* denssum, const double* colorfield, const double* dens_bc, double* dens) const
{
  dens[0] = denssum[0] + dens_bc[0] * (1.0 - colorfield[0]);
}

FOUR_C_NAMESPACE_CLOSE
