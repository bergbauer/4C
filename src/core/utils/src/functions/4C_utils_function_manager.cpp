/*----------------------------------------------------------------------*/
/*! \file

\brief Managing of space- and/or time-dependent functions

\level 0

*/
/*----------------------------------------------------------------------*/

#include "4C_global_data.hpp"
#include "4C_io_dat_file_utils.hpp"
#include "4C_io_linedefinition.hpp"
#include "4C_utils_exceptions.hpp"
#include "4C_utils_function.hpp"
#include "4C_utils_function_of_time.hpp"

FOUR_C_NAMESPACE_OPEN

namespace
{
  using LineDefinitionVector = std::vector<Input::LineDefinition>;

  using TypeErasedFunctionCreator = std::function<std::any(const LineDefinitionVector&)>;

  template <typename T>
  using FunctionCreator = Teuchos::RCP<T> (*)(const LineDefinitionVector&);

  /**
   * Utility function that takes a function object returning a Teuchos::RCP<T> and erases its return
   * type via std::any. In addition, if the returned object would be Teuchos::null, discard it and
   * return an empty std::any instead.
   */
  template <typename T>
  TypeErasedFunctionCreator WrapFunction(FunctionCreator<T> fun)
  {
    return [fun](const LineDefinitionVector& linedefs) -> std::any
    {
      Teuchos::RCP<T> created = fun(linedefs);
      if (created == Teuchos::null)
        return {};
      else
        return created;
    };
  }


  template <int dim>
  std::any CreateBuiltinFunction(const std::vector<Input::LineDefinition>& function_line_defs)
  {
    // List all known TryCreate functions in a vector, so they can be called with a unified
    // syntax below. Also, erase their exact return type, since we can only store std::any.
    std::vector<TypeErasedFunctionCreator> try_create_function_vector{
        WrapFunction(Core::UTILS::TryCreateSymbolicFunctionOfAnything<dim>),
        WrapFunction(Core::UTILS::TryCreateSymbolicFunctionOfSpaceTime<dim>),
        WrapFunction(Core::UTILS::TryCreateFunctionOfTime)};

    for (const auto& try_create_function : try_create_function_vector)
    {
      auto maybe_function = try_create_function(function_line_defs);
      if (maybe_function.has_value()) return maybe_function;
    }

    FOUR_C_THROW("Internal error: could not create a function that I should be able to create.");
  }

  // add one level of indirection to dispatch on the dimension later when the global problem is
  // available.
  auto CreateBuiltinFunctionDispatch(const std::vector<Input::LineDefinition>& function_line_defs)
  {
    switch (Global::Problem::Instance()->NDim())
    {
      case 1:
        return CreateBuiltinFunction<1>(function_line_defs);
      case 2:
        return CreateBuiltinFunction<2>(function_line_defs);
      case 3:
        return CreateBuiltinFunction<3>(function_line_defs);
      default:
        FOUR_C_THROW("Unsupported dimension %d.", Global::Problem::Instance()->NDim());
    }
  }
}  // namespace


void Core::UTILS::AddValidBuiltinFunctions(Core::UTILS::FunctionManager& function_manager)
{
  using namespace Input;

  std::vector<LineDefinition> possible_lines = {
      LineDefinition::Builder().add_named_string("SYMBOLIC_FUNCTION_OF_SPACE_TIME").build(),

      LineDefinition::Builder().add_named_string("SYMBOLIC_FUNCTION_OF_TIME").build(),

      LineDefinition::Builder()
          .add_named_int("COMPONENT")
          .add_named_string("SYMBOLIC_FUNCTION_OF_SPACE_TIME")
          .build(),

      LineDefinition::Builder()
          .add_named_int("VARIABLE")
          .add_named_string("NAME")
          .add_named_string("TYPE")
          .add_optional_named_int("NUMPOINTS")
          .add_optional_named_string("BYNUM")
          .add_optional_named_double_vector("TIMERANGE", 2)
          .add_optional_named_double_vector("TIMES", LengthFromIntNamed("NUMPOINTS"))
          .add_optional_named_double_vector("VALUES", LengthFromIntNamed("NUMPOINTS"))
          .add_optional_named_string_vector("DESCRIPTION",
              // Special case where only NUMPOINTS-1 are taken
              [](const Core::IO::InputParameterContainer& already_read_line)
              {
                try
                {
                  int length = already_read_line.Get<int>("NUMPOINTS");
                  return length - 1;
                }
                catch (const Core::Exception& e)
                {
                  // When NUMPOINTS is not set, then we still allow for a single DESCRIPTION entry
                  return 1;
                }
              })
          .add_optional_named_string("PERIODIC")
          .add_optional_named_double("T1")
          .add_optional_named_double("T2")
          .build(),

      LineDefinition::Builder()
          .add_named_string("VARFUNCTION")
          .add_optional_named_int("NUMCONSTANTS")
          .add_optional_named_pair_of_string_and_double_vector(
              "CONSTANTS", LengthFromIntNamed("NUMCONSTANTS"))
          .build()};

  function_manager.add_function_definition(possible_lines, CreateBuiltinFunctionDispatch);
}


std::vector<Input::LineDefinition> Core::UTILS::FunctionManager::valid_function_lines()
{
  std::vector<Input::LineDefinition> lines;
  for (const auto& [possible_lines, _] : attached_function_data_)
  {
    for (const auto& single_line : possible_lines)
    {
      lines.emplace_back(single_line);
    }
  }
  return lines;
}


void Core::UTILS::FunctionManager::add_function_definition(
    std::vector<Input::LineDefinition> possible_lines, FunctionFactory function_factory)
{
  attached_function_data_.emplace_back(std::move(possible_lines), std::move(function_factory));
}


void Core::UTILS::FunctionManager::ReadInput(Core::IO::DatFileReader& reader)
{
  functions_.clear();

  // Read FUNCT sections starting from FUNCT1 until the first empty one is encountered.
  // This implies that the FUNCT sections must form a contiguous range in the input file.
  // Otherwise, the read fails later.
  for (int funct_suffix = 1;; ++funct_suffix)
  {
    const bool stop_parsing = std::invoke(
        [&]()
        {
          for (auto& [possible_lines, function_factory] : attached_function_data_)
          {
            auto [parsed_lines, unparsed_lines] =
                Core::IO::DatFileUtils::read_matching_lines_in_section(
                    reader, "FUNCT" + std::to_string(funct_suffix), possible_lines);

            // A convoluted way of saying that there are no lines in the section, thus, stop
            // parsing. This can only be refactored if the reading mechanism is overhauled in
            // general.
            if (parsed_lines.size() + unparsed_lines.size() == 0)
            {
              return true;
            }

            if (parsed_lines.size() > 0 && unparsed_lines.size() == 0)
            {
              functions_.emplace_back(function_factory(parsed_lines));
              return false;
            }
          }

          // If we end up here, the current sections function definition could not be parsed.
          {
            const auto section_line_defs = reader.Section("--FUNCT" + std::to_string(funct_suffix));
            std::stringstream ss;
            for (const auto& line : section_line_defs)
            {
              ss << '\n' << line;
            }

            FOUR_C_THROW("Could not parse the following lines into a Function known to 4C:\n%s",
                ss.str().c_str());
          }
        });

    // Stop reading as soon as the first FUNCT section in the input file is empty
    if (stop_parsing) break;
  }
}

FOUR_C_NAMESPACE_CLOSE