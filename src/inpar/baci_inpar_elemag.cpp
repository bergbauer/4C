/*----------------------------------------------------------------------*/
/*! \file
\brief Input parameters for electromagnetic simulations

\level 3


*/
/*----------------------------------------------------------------------*/

#include "baci_inpar_elemag.H"

#include "baci_inpar_validparameters.H"
#include "baci_lib_conditiondefinition.H"
#include "baci_linalg_equilibrate.H"

void INPAR::ELEMAG::SetValidParameters(Teuchos::RCP<Teuchos::ParameterList> list)
{
  using namespace DRT::INPUT;
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  Teuchos::ParameterList& electromagneticdyn = list->sublist(
      "ELECTROMAGNETIC DYNAMIC", false, "control parameters for electromagnetic problems\n");

  // general settings for time-integration scheme
  DoubleParameter("TIMESTEP", 0.01, "Time-step length dt", &electromagneticdyn);
  DoubleParameter("TAU", 1, "Stabilization parameter", &electromagneticdyn);
  IntParameter("NUMSTEP", 100, "Number of time steps", &electromagneticdyn);
  DoubleParameter("MAXTIME", 1.0, "Total simulation time", &electromagneticdyn);

  // additional parameters
  IntParameter("RESULTSEVRY", 1, "Increment for writing solution", &electromagneticdyn);
  IntParameter("RESTARTEVRY", 1, "Increment for writing restart", &electromagneticdyn);
  IntParameter("LINEAR_SOLVER", -1, "Number of linear solver used for electromagnetic problem",
      &electromagneticdyn);
  IntParameter("STARTFUNCNO", -1, "Function for initial field", &electromagneticdyn);
  IntParameter("SOURCEFUNCNO", -1, "Function for source term in volume", &electromagneticdyn);
  // BoolParameter("DOUBLEORFLOAT","Yes","Yes, if evaluation with double, no if with
  // float",&electromagneticdyn); BoolParameter("ALLELESEQUAL","No","Yes, if all elements have same
  // shape and material",&electromagneticdyn);

  {
    // time integration

    Teuchos::Tuple<std::string, 8> name;
    Teuchos::Tuple<int, 8> label;
    name[0] = "One_Step_Theta";
    label[0] = elemag_ost;
    name[1] = "BDF1";
    label[1] = elemag_bdf1;
    name[2] = "BDF2";
    label[2] = elemag_bdf2;
    name[3] = "BDF4";
    label[3] = elemag_bdf4;
    name[4] = "GenAlpha";
    label[4] = elemag_genAlpha;
    name[5] = "Explicit_Euler";
    label[5] = elemag_explicit_euler;
    name[6] = "Runge_Kutta";
    label[6] = elemag_rk;
    name[7] = "Crank_Nicolson";
    label[7] = elemag_cn;

    setStringToIntegralParameter<int>("TIMEINT", "One_Step_Theta",
        "Type of time integration scheme", name, label, &electromagneticdyn);
  }

  {
    // a standard Teuchos::tuple can have at maximum 10 entries! We have to circumvent this here.
    Teuchos::Tuple<std::string, 4> name;
    Teuchos::Tuple<int, 4> label;
    name[0] = "zero_field";
    label[0] = initfield_zero_field;
    name[1] = "field_by_function";
    label[1] = initfield_field_by_function;
    name[2] = "field_by_steady_state";
    label[2] = initfield_scatra;
    name[3] = "field_by_steady_state_hdg";
    label[3] = initfield_scatra_hdg;

    setStringToIntegralParameter<int>("INITIALFIELD", "zero_field", "Initial field for ele problem",
        name, label, &electromagneticdyn);

    // Error calculation
    BoolParameter("CALCERR", "No", "Calc the error wrt ERRORFUNCNO?", &electromagneticdyn);

    // Post process solution?
    BoolParameter("POSTPROCESS", "No", "Postprocess solution? (very slow)", &electromagneticdyn);
  }

  IntParameter("ERRORFUNCNO", -1, "Function for error calculation", &electromagneticdyn);

  // flag for equilibration of global system of equations
  setStringToIntegralParameter<CORE::LINALG::EquilibrationMethod>("EQUILIBRATION", "none",
      "flag for equilibration of global system of equations",
      tuple<std::string>("none", "rows_full", "rows_maindiag", "columns_full", "columns_maindiag",
          "rowsandcolumns_full", "rowsandcolumns_maindiag"),
      tuple<CORE::LINALG::EquilibrationMethod>(CORE::LINALG::EquilibrationMethod::none,
          CORE::LINALG::EquilibrationMethod::rows_full,
          CORE::LINALG::EquilibrationMethod::rows_maindiag,
          CORE::LINALG::EquilibrationMethod::columns_full,
          CORE::LINALG::EquilibrationMethod::columns_maindiag,
          CORE::LINALG::EquilibrationMethod::rowsandcolumns_full,
          CORE::LINALG::EquilibrationMethod::rowsandcolumns_maindiag),
      &electromagneticdyn);

  // PML
  // StringParameter("PML_DEFINITION_FILE","none.txt","Filename of file containing the pml
  // definition",&electromagneticdyn);
}

/// set specific electromagnetic conditions
void INPAR::ELEMAG::SetValidConditions(
    std::vector<Teuchos::RCP<DRT::INPUT::ConditionDefinition>>& condlist)
{
  using namespace DRT::INPUT;

  std::vector<Teuchos::RCP<::INPUT::LineComponent>> abcbundcomponents;

  abcbundcomponents.emplace_back(Teuchos::rcp(new ::INPUT::SeparatorComponent("NUMDOF")));
  abcbundcomponents.emplace_back(Teuchos::rcp(new ::INPUT::IntComponent("numdof")));
  abcbundcomponents.emplace_back(Teuchos::rcp(new ::INPUT::SeparatorComponent("ONOFF")));
  abcbundcomponents.emplace_back(
      Teuchos::rcp(new ::INPUT::IntVectorComponent("onoff", INPUT::LengthFromInt("numdof"))));
  abcbundcomponents.emplace_back(Teuchos::rcp(new ::INPUT::SeparatorComponent("FUNCT")));
  abcbundcomponents.emplace_back(Teuchos::rcp(new ::INPUT::IntVectorComponent(
      "funct", INPUT::LengthFromInt("numdof"), {0, false, true, false})));
  abcbundcomponents.emplace_back(Teuchos::rcp(new ::INPUT::SeparatorComponent("VAL")));
  abcbundcomponents.emplace_back(
      Teuchos::rcp(new ::INPUT::RealVectorComponent("val", INPUT::LengthFromInt("numdof"))));

  //*--------------------------------------------------------------------* /
  // absorbing boundary condition for electromagnetic problems
  // line
  Teuchos::RCP<ConditionDefinition> silvermueller_line =
      Teuchos::rcp(new ConditionDefinition("DESIGN LINE SILVER-MUELLER CONDITIONS",
          "Silver-Mueller", "Absorbing-emitting line for electromagnetics",
          DRT::Condition::SilverMueller, true, DRT::Condition::Line));

  // surface
  Teuchos::RCP<ConditionDefinition> silvermueller_surface =
      Teuchos::rcp(new ConditionDefinition("DESIGN SURF SILVER-MUELLER CONDITIONS",
          "Silver-Mueller", "Absorbing-emitting surface for electromagnetics",
          DRT::Condition::SilverMueller, true, DRT::Condition::Surface));

  for (unsigned i = 0; i < abcbundcomponents.size(); ++i)
  {
    silvermueller_line->AddComponent(abcbundcomponents[i]);
    silvermueller_surface->AddComponent(abcbundcomponents[i]);
  }

  condlist.push_back(silvermueller_line);
  condlist.push_back(silvermueller_surface);
}