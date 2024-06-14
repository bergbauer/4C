/*---------------------------------------------------------------------*/
/*! \file
\brief A collection of helper functions for Teuchos::ParameterLists

\level 0

*/
/*---------------------------------------------------------------------*/

#ifndef FOUR_C_UTILS_PARAMETER_LIST_HPP
#define FOUR_C_UTILS_PARAMETER_LIST_HPP

#include "4C_config.hpp"

#include <Teuchos_StandardParameterEntryValidators.hpp>

FOUR_C_NAMESPACE_OPEN

namespace Core
{
  namespace UTILS
  {
    //! add entry as item of enum class @p value to @p list with name @p parameter_name
    template <class EnumType>
    void AddEnumClassToParameterList(
        const std::string& parameter_name, const EnumType value, Teuchos::ParameterList& list)
    {
      const std::string docu = "";
      const std::string value_name = "val";
      Teuchos::setStringToIntegralParameter<EnumType>(parameter_name, value_name, docu,
          Teuchos::tuple<std::string>(value_name), Teuchos::tuple<EnumType>(value), &list);
    }

    /// local wrapper to test multiple versions of "Yes", "YES", etc
    void BoolParameter(std::string const& paramName, std::string const& value,
        std::string const& docString, Teuchos::ParameterList* paramList);

    /// local wrapper for Teuchos::setIntParameter() that allows only integers
    void IntParameter(std::string const& paramName, int const value, std::string const& docString,
        Teuchos::ParameterList* paramList);

    /// local wrapper for Teuchos::setDoubleParameter() that allows only doubles
    void DoubleParameter(std::string const& paramName, double const& value,
        std::string const& docString, Teuchos::ParameterList* paramList);

    /*!
    \brief Special implementation for a parameter being an arbitrary std::string

    The method Teuchos::setNumericStringParameter() cannot be used for arbitrary
    std::string parameters, since the validate() method of the underlying
    AnyNumberParameterEntryValidator always tries to convert a given std::string to DOUBLE(s)!
    This may cause error messages in valgrind.
    Thus, for arbitrary std::strings, such as needed for specifying a file or solver name, for
    instance, this method which uses a StringValidator has to be used!

    @param[in] paramName Name of parameter to be put into the parameter list
    @param[in] value Value of the parameter
    @param[in] docString Documentation of the parameter
    @param[in/out] paramList Parameter list (to be filled with <paramName,Value,docString>)
    */
    void StringParameter(std::string const& paramName, std::string const& value,
        std::string const& docString, Teuchos::ParameterList* paramList);

    template <class T>
    T IntegralValue(const Teuchos::ParameterList& params, const std::string& name)
    {
      int value = Teuchos::getIntegralValue<int>(params, name);
      return static_cast<T>(value);
    }

    template <class T>
    T GetAsEnum(const Teuchos::ParameterList& params, const std::string& name)
    {
      static_assert(std::is_enum_v<T>, "This function may only be used for enum constants.");
      int value = params.get<int>(name);
      return static_cast<T>(value);
    }

    template <class T>
    T GetAsEnum(const Teuchos::ParameterList& params, const std::string& name, T default_value)
    {
      static_assert(std::is_enum_v<T>, "This function may only be used for enum constants.");
      if (params.isParameter(name))
        return static_cast<T>(params.get<int>(name));
      else
        return default_value;
    }
  }  // namespace UTILS
}  // namespace Core


FOUR_C_NAMESPACE_CLOSE

#endif