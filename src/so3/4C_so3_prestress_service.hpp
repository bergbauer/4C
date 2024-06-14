/*! \file

\level 1

\brief Common service function for prestress


*/

#ifndef FOUR_C_SO3_PRESTRESS_SERVICE_HPP
#define FOUR_C_SO3_PRESTRESS_SERVICE_HPP


#include "4C_config.hpp"

#include "4C_global_data.hpp"
#include "4C_inpar_structure.hpp"

#include <Teuchos_ParameterList.hpp>

FOUR_C_NAMESPACE_OPEN

namespace Prestress
{
  /*!
   * \brief Returns the type of the prestress algorithm stored in the parameters of structural
   * dynamics
   *
   * \return Inpar::STR::PreStress
   */
  static inline Inpar::STR::PreStress GetType()
  {
    static Inpar::STR::PreStress pstype = Teuchos::getIntegralValue<Inpar::STR::PreStress>(
        Global::Problem::Instance()->structural_dynamic_params(), "PRESTRESS");

    return pstype;
  }

  /*!
   * \brief Returns the prestress time stored in the parameters of structural dynamics
   *
   * \return double
   */
  static inline double GetPrestressTime()
  {
    static double pstime =
        Global::Problem::Instance()->structural_dynamic_params().get<double>("PRESTRESSTIME");

    return pstime;
  }

  /*!
   * \brief Returns whether MULF is set for prestressing in the parameters of structural dynamics.
   * This method does not ensure that MULF is actually active
   *
   * \return true MULF is set in input file
   * \return false MULF is not set in input file
   */
  static inline bool IsMulf() { return GetType() == Inpar::STR::PreStress::mulf; }

  /*!
   * \brief Returns whether material iterative prestressing is set in the parameters of structural
   * dynamics. This method does not ensure that prestressing is actually active
   *
   * \return true material iterative prestressing is set in input file
   * \return false material iterative prestressing is not set in input file
   */
  static inline bool IsMaterialIterative()
  {
    return GetType() == Inpar::STR::PreStress::material_iterative;
  }

  /*!
   * \brief Returns whether MULF is set for prestressing as the given prestress type.
   * This method does not ensure that MULF is actually active
   *
   * \param pstype Prestress type that is used
   * \return true MULF is set in input file
   * \return false MULF is not set in input file
   */
  static inline bool IsMulf(Inpar::STR::PreStress pstype)
  {
    return pstype == Inpar::STR::PreStress::mulf;
  }

  /*!
   * \brief Returns whether material iterative prestressing is set as the given prestress type.
   * This method does not ensure that prestressing is actually active
   *
   * \param pstype Prestress type that is used
   * \return true material iterative prestressing is set in input file
   * \return false material iterative prestressing is not set in input file
   */
  static inline bool IsMaterialIterative(Inpar::STR::PreStress pstype)
  {
    return pstype == Inpar::STR::PreStress::material_iterative;
  }


  /*!
   * \brief Returns whether no prestressing is set in the parameters of
   * structural dynamics.
   *
   * \return true No prestressing is set in the input file
   * \return false Prestressing is set in the input file
   */
  static inline bool IsNone() { return GetType() == Inpar::STR::PreStress::none; }


  /*!
   * \brief Returns whether no prestressing is set in the given parameter.
   *
   * \param pstype Prestress type that is used
   * \return true No prestressing is set in the input parameter
   * \return false Prestressing is set in the input parameter
   */
  static inline bool IsNone(Inpar::STR::PreStress pstype)
  {
    return pstype == Inpar::STR::PreStress::none;
  }

  /*!
   * \brief Returns whether any prestressing is set in the parameters of
   * structural dynamics.
   *
   * \return true Prestressing is set in the input file
   * \return false No prestressing is set in the input file
   */
  static inline bool IsAny()
  {
    return Teuchos::getIntegralValue<Inpar::STR::PreStress>(
               Global::Problem::Instance()->structural_dynamic_params(), "PRESTRESS") !=
           Inpar::STR::PreStress::none;
  }

  /*!
   * \brief Returns whether prestressing is set in the given parameter.
   *
   * \param pstype Prestress type that is used
   * \return true Prestressing is set in the input parameter
   * \return false No prestressing is set in the input parameter
   */
  static inline bool IsAny(Inpar::STR::PreStress pstype)
  {
    return pstype != Inpar::STR::PreStress::none;
  }

  /*!
   * \brief Returns true if any prestressing method is currently active with the parameters of
   * strtuctural dynamics.
   *
   * \param currentTime Current time of the simulation
   * \return true Any prestressing method is active
   * \return false No prestressing method is active
   */
  static inline bool is_active(const double currentTime)
  {
    Inpar::STR::PreStress pstype = Teuchos::getIntegralValue<Inpar::STR::PreStress>(
        Global::Problem::Instance()->structural_dynamic_params(), "PRESTRESS");
    const double pstime =
        Global::Problem::Instance()->structural_dynamic_params().get<double>("PRESTRESSTIME");
    return pstype != Inpar::STR::PreStress::none && currentTime <= pstime + 1.0e-15;
  }

  /*!
   * \brief Returns true if any prestressing method is currently active with the given parameters.
   *
   * \param currentTimeCurrent time of the simulation
   * \param pstype Prestress type that is used
   * \param pstime Prestress time that is used
   * \return true Any prestressing method is active
   * \return false No prestressing method is active
   */
  static inline bool is_active(
      const double currentTime, Inpar::STR::PreStress pstype, const double pstime)
  {
    return pstype != Inpar::STR::PreStress::none && currentTime <= pstime + 1.0e-15;
  }

  /*!
   * \brief Returns true if MULF prestressing method is currently active with the parameters of
   * strtuctural dynamics.
   *
   * \param currentTimeCurrent time of the simulation
   * \return true MULF prestressing method is active
   * \return false MULF prestressing method is active
   */
  static inline bool IsMulfActive(const double currentTime)
  {
    return IsMulf() && currentTime <= GetPrestressTime() + 1.0e-15;
  }

  /*!
   * \brief Returns true if MULF prestressing method is currently active with the given
   * parameters.
   *
   * \param currentTimeCurrent time of the simulation
   * \param pstype Prestress type that is used
   * \param pstime Prestress time that is used
   * \return true MULF prestressing method is active
   * \return false MULF prestressing method is active
   */
  static inline bool IsMulfActive(
      const double currentTime, Inpar::STR::PreStress pstype, const double pstime)
  {
    return IsMulf(pstype) && currentTime <= pstime + 1.0e-15;
  }

}  // namespace Prestress

FOUR_C_NAMESPACE_CLOSE

#endif