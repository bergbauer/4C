/*----------------------------------------------------------------------*/
/*! \file

\brief

\level 3


*/
/*----------------------------------------------------------------------*/
#include "4C_mat_anisotropy_extension_default.hpp"

#include "4C_comm_parobject.hpp"
#include "4C_linalg_fixedsizematrix_generators.hpp"
#include "4C_mat_anisotropy_extension.hpp"
#include "4C_mat_service.hpp"

#include <algorithm>

FOUR_C_NAMESPACE_OPEN

template <unsigned int numfib>
MAT::DefaultAnisotropyExtension<numfib>::DefaultAnisotropyExtension(const int init_mode,
    const double gamma, const bool adapt_angle,
    const Teuchos::RCP<ELASTIC::StructuralTensorStrategyBase>& stucturalTensorStrategy,
    std::array<int, numfib> fiber_ids)
    : FiberAnisotropyExtension<numfib>(stucturalTensorStrategy),
      init_mode_(init_mode),
      gamma_(gamma),
      adapt_angle_(adapt_angle),
      fiber_ids_(fiber_ids)
{
  if (init_mode_ == INIT_MODE_NODAL_FIBERS || init_mode_ == INIT_MODE_NODAL_EXTERNAL)
  {
    this->SetFiberLocation(FiberLocation::GPFibers);
  }
  else
  {
    this->SetFiberLocation(FiberLocation::ElementFibers);
  }
}

template <unsigned int numfib>
void MAT::DefaultAnisotropyExtension<numfib>::PackAnisotropy(CORE::COMM::PackBuffer& data) const
{
  // Call base packing
  MAT::FiberAnisotropyExtension<numfib>::PackAnisotropy(data);

  CORE::COMM::ParObject::AddtoPack(data, static_cast<int>(initialized_));
}

template <unsigned int numfib>
void MAT::DefaultAnisotropyExtension<numfib>::UnpackAnisotropy(
    const std::vector<char>& data, std::vector<char>::size_type& position)
{
  // Call base unpacking
  MAT::FiberAnisotropyExtension<numfib>::UnpackAnisotropy(data, position);

  initialized_ = static_cast<bool>(CORE::COMM::ParObject::ExtractInt(position, data));
}

template <unsigned int numfib>
void MAT::DefaultAnisotropyExtension<numfib>::SetFiberVecs(const double newgamma,
    const CORE::LINALG::Matrix<3, 3>& locsys, const CORE::LINALG::Matrix<3, 3>& defgrd)
{
  CORE::LINALG::Matrix<3, 1> ca1(true);
  CORE::LINALG::Matrix<3, 1> ca2(true);

  // Fiber direction derived from local cosy
  if (init_mode_ == INIT_MODE_ELEMENT_EXTERNAL || init_mode_ == INIT_MODE_ELEMENT_FIBERS)
  {
    // alignment angles gamma_i are read from first entry of then unnecessary vectors a1 and a2
    if ((gamma_ < -90) || (gamma_ > 90)) FOUR_C_THROW("Fiber angle not in [-90,90]");
    // convert
    double gamma = (gamma_ * M_PI) / 180.;

    if (adapt_angle_ && newgamma != -1.0)
    {
      if (gamma * newgamma < 0.0)
      {
        gamma = -1.0 * newgamma;
      }
      else
      {
        gamma = newgamma;
      }
    }

    for (int i = 0; i < 3; ++i)
    {
      // a1 = cos gamma e3 + sin gamma e2
      ca1(i) = std::cos(gamma) * locsys(i, 2) + std::sin(gamma) * locsys(i, 1);
      // a2 = cos gamma e3 - sin gamma e2
      ca2(i) = std::cos(gamma) * locsys(i, 2) - std::sin(gamma) * locsys(i, 1);
    }
  }
  else
  {
    FOUR_C_THROW(
        "Setting the fiber vectors is only possible for external element fibers mode or using a "
        "coordinate system.");
  }

  // pull back in reference configuration
  CORE::LINALG::Matrix<3, 1> a1_0(true);
  CORE::LINALG::Matrix<3, 1> a2_0(true);
  CORE::LINALG::Matrix<3, 3> idefgrd(true);
  idefgrd.Invert(defgrd);


  std::array<CORE::LINALG::Matrix<3, 1>, numfib> fibers;

  if (numfib >= 1)
  {
    fibers[0].Multiply(idefgrd, ca1);
    fibers[0].Scale(1.0 / fibers[0].Norm2());
  }
  if (numfib >= 2)
  {
    fibers[1].Multiply(idefgrd, ca2);
    fibers[1].Scale(1.0 / fibers[1].Norm2());
  }
  if (numfib >= 3)
  {
    FOUR_C_THROW(
        "This kind of initialization method is not implemented for materials that need more than 2 "
        "fibers.");
  }

  this->SetFibers(BaseAnisotropyExtension::GPDEFAULT, fibers);
}

template <unsigned int numfib>
void MAT::DefaultAnisotropyExtension<numfib>::SetFiberVecs(
    const CORE::LINALG::Matrix<3, 1>& fibervec)
{
  std::array<CORE::LINALG::Matrix<3, 1>, numfib> fibers;
  fibers[0].Update(fibervec);

  if (numfib >= 2)
  {
    FOUR_C_THROW("This method can only be called for materials with one fiber!");
  }

  this->SetFibers(BaseAnisotropyExtension::GPDEFAULT, fibers);
}

template <unsigned int numfib>
bool MAT::DefaultAnisotropyExtension<numfib>::do_element_fiber_initialization()
{
  switch (init_mode_)
  {
    case INIT_MODE_ELEMENT_EXTERNAL:
      do_external_fiber_initialization();
      return true;
    case INIT_MODE_ELEMENT_FIBERS:

      // check, whether a coordinate system is given
      if (this->GetAnisotropy()->has_element_cylinder_coordinate_system())
      {
        // initialize fiber vector with local coordinate system
        CORE::LINALG::Matrix<3, 3> locsys(true);
        const CORE::LINALG::Matrix<3, 3> Id = CORE::LINALG::IdentityMatrix<3>();
        this->GetAnisotropy()
            ->get_element_cylinder_coordinate_system()
            .evaluate_local_coordinate_system(locsys);

        this->SetFiberVecs(-1.0, locsys, Id);
      }
      else if (this->GetAnisotropy()->get_number_of_element_fibers() > 0)
      {
        // initialize fibers from global given fibers
        std::array<CORE::LINALG::Matrix<3, 1>, numfib> fibers;
        for (unsigned int i = 0; i < numfib; ++i)
        {
          fibers.at(i) = this->GetAnisotropy()->GetElementFibers().at(fiber_ids_.at(i));
        }
        this->SetFibers(BaseAnisotropyExtension::GPDEFAULT, fibers);
      }
      else
      {
        FOUR_C_THROW("Could not find element coordinate system or element fibers!");
      }

      return true;
    default:
      return false;
  }
}

template <unsigned int numfib>
bool MAT::DefaultAnisotropyExtension<numfib>::do_gp_fiber_initialization()
{
  switch (init_mode_)
  {
    case INIT_MODE_NODAL_EXTERNAL:
      do_external_fiber_initialization();
      return true;
    case INIT_MODE_NODAL_FIBERS:

      // check, whether a coordinate system is given
      if (this->GetAnisotropy()->has_gp_cylinder_coordinate_system())
      {
        FOUR_C_THROW(
            "Gauss-point fibers defined via Gauss-point cylinder coordinate systems is not yet "
            "defined");
      }
      else if (this->GetAnisotropy()->GetNumberOfGPFibers() > 0)
      {
        // initialize fibers from global given fibers
        int gp = 0;
        for (const auto& fiberList : this->GetAnisotropy()->GetGPFibers())
        {
          std::array<CORE::LINALG::Matrix<3, 1>, numfib> fibers;

          int i = 0;
          for (int id : fiber_ids_)
          {
            fibers.at(i) = fiberList.at(id);
            ++i;
          }
          this->SetFibers(gp, fibers);
          ++gp;
        }
      }
      else
      {
        FOUR_C_THROW("Could not find Gauss-point coordinate systems or Gauss-point fibers!");
      }

      return true;
    default:
      return false;
  }
}

template <unsigned int numfib>
void MAT::DefaultAnisotropyExtension<numfib>::do_external_fiber_initialization()
{
  const CORE::LINALG::Matrix<3, 3> Id = CORE::LINALG::IdentityMatrix<3>();
  SetFiberVecs(-1.0, Id, Id);
}


// explicit instatiations of template classes
template class MAT::DefaultAnisotropyExtension<1u>;
template class MAT::DefaultAnisotropyExtension<2u>;
FOUR_C_NAMESPACE_CLOSE
