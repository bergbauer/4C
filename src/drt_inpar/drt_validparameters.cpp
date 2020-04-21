/*----------------------------------------------------------------------*/
/*! \file
\maintainer Martin Kronbichler

\brief Setup of the list of valid input parameters

\level 1

*/
/*----------------------------------------------------------------------*/

#include <Teuchos_Array.hpp>
#include <Teuchos_StrUtils.hpp>
#include <Teuchos_any.hpp>

#include "drt_validparameters.H"
#include "../drt_lib/drt_colors.H"
#include "../drt_lib/drt_globalproblem_enums.H"
#include "../drt_io/io_pstream.H"
#include "inpar.H"
#include "inpar_ale.H"
#include "inpar_solver.H"
#include "inpar_solver_nonlin.H"
#include "inpar_fluid.H"
#include "inpar_cut.H"
#include "inpar_twophase.H"
#include "inpar_mortar.H"
#include "inpar_contact.H"
#include "inpar_fsi.H"
#include "inpar_topopt.H"
#include "inpar_lubrication.H"
#include "inpar_scatra.H"
#include "inpar_s2i.H"
#include "inpar_sti.H"
#include "inpar_structure.H"
#include "inpar_problemtype.H"
#include "inpar_thermo.H"
#include "inpar_tsi.H"
#include "inpar_turbulence.H"
#include "inpar_elch.H"
#include "inpar_cardiac_monodomain.H"
#include "inpar_invanalysis.H"
#include "inpar_statinvanalysis.H"
#include "inpar_searchtree.H"
#include "inpar_xfem.H"
#include "inpar_mlmc.H"
#include "inpar_poroelast.H"
#include "inpar_poroscatra.H"
#include "inpar_poromultiphase.H"
#include "inpar_poromultiphase_scatra.H"
#include "inpar_porofluidmultiphase.H"
#include "inpar_immersed.H"
#include "inpar_fbi.H"
#include "inpar_fpsi.H"
#include "inpar_ehl.H"
#include "inpar_ssi.H"
#include "inpar_fs3i.H"
#include "inpar_particle.H"
#include "inpar_pasi.H"
#include "inpar_levelset.H"
#include "inpar_wear.H"
#include "inpar_beamcontact.H"
#include "inpar_beampotential.H"
#include "inpar_elemag.H"
#include "inpar_bio.H"
#include "inpar_volmortar.H"
#include "inpar_loca_continuation.H"
#include "../drt_tutorial/inpar_tutorial.H"
#include "inpar_beaminteraction.H"
#include "inpar_binningstrategy.H"
#include "inpar_browniandyn.H"
#include "inpar_cardiovascular0d.H"
#include "inpar_contact_xcontact.H"
#include "inpar_plasticity.H"
#include "inpar_mor.H"
#include "inpar_io.H"
#include "inpar_IO_monitor_structure_dbc.H"
#include "inpar_IO_runtime_vtk_output.H"
#include "inpar_IO_runtime_vtk_output_structure.H"
#include "inpar_IO_runtime_vtk_output_structure_beams.H"
#include "inpar_IO_runtime_vtp_output_structure.H"


/*----------------------------------------------------------------------*/
//! Print function
/*----------------------------------------------------------------------*/
void PrintValidParameters()
{
  Teuchos::RCP<const Teuchos::ParameterList> list = DRT::INPUT::ValidParameters();
  list->print(std::cout,
      Teuchos::ParameterList::PrintOptions().showDoc(true).showFlags(false).indent(4).showTypes(
          false));
}


/*----------------------------------------------------------------------*/
//! Print help message
/*----------------------------------------------------------------------*/
void PrintHelpMessage()
{
#ifdef DEBUG
  char baci_build[] = "baci-debug";
#else
  char baci_build[] = "baci-release";
#endif

  std::cout << "NAME\n"
            << "\t" << baci_build << " - simulate just about anything\n"
            << "\n"
            << "SYNOPSIS\n"
            << "\t" << baci_build
            << " [-h] [--help] [-p] [--parameters] [-d] [--datfile] [-ngroup=x] "
               "[-glayout=a,b,c,...] [-nptype=parallelism_type]\n"
            << "\t\tdat_name output_name [restart=y] [restartfrom=restart_file_name] [ dat_name0 "
               "output_name0 [restart=y] [restartfrom=restart_file_name] ... ] [--interactive]\n"
            << "\n"
            << "DESCRIPTION\n"
            << "\tThe am besten simulation tool in the world.\n"
            << "\n"
            << "OPTIONS\n"
            << "\t--help or -h\n"
            << "\t\tPrint this message.\n"
            << "\n"
            << "\t--parameters or -p\n"
            << "\t\tPrint a list of all available parameters for use in a dat_file.\n"
            << "\n"
            << "\t--datfile or -d\n"
            << "\t\tPrint example dat_file with all available parameters.\n"
            << "\n"
            << "\t-ngroup=x\n"
            << "\t\tSpecify the number of groups for nested parallelism. (default: 1)\n"
            << "\n"
            << "\t-glayout=a,b,c,...\n"
            << "\t\tSpecify the number of processors per group. Argument \"-ngroup\" is mandatory "
               "and must be preceding. (default: equal distribution)\n"
            << "\n"
            << "\t-nptype=parallelism_type\n"
            << "\t\tAvailable options: \"separateDatFiles\", \"everyGroupReadDatFile\" and "
               "\"copyDatFile\"; Must be set if \"-ngroup\" > 1.\n"
            << "\t\t\"diffgroupx\" can be used to compare results from separate but parallel baci "
               "runs; x must be 0 and 1 for the respective run"
            << "\n"
            << "\tdat_name\n"
            << "\t\tName of the input file (Usually *.dat)\n"
            << "\n"
            << "\toutput_name\n"
            << "\t\tPrefix of your output files.\n"
            << "\n"
            << "\trestart=y\n"
            << "\t\tRestart the simulation from step y. It always refers to the previously defined "
               "dat_name and output_name. (default: 0 or from dat_name)\n"
            << "\n"
            << "\trestartfrom=restart_file_name\n"
            << "\t\tRestart the simulation from the files prefixed with restart_file_name. "
               "(default: output_name)\n"
            << "\n"
            << "\t--interactive\n"
            << "\t\tBaci waits at the beginning for keyboard input. Helpful for parallel debugging "
               "when attaching to a single job. Must be specified at the end in the command line.\n"
            << "\n"
            << "SEE ALSO\n"
            << "\tguides/reports/global_report.pdf\n"
            << "\n"
            << "BUGS\n"
            << "\t100% bug free since 1964.\n"
            << "\n"
            << "TIPS\n"
            << "\tCan be obtain from a friendly colleague.\n"
            << "\n"
            << "\tAlso, espresso may be donated to room MW1236.\n";

  return;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::PrintDatHeader(std::ostream& stream, const Teuchos::ParameterList& list,
    std::string parentname, bool color, bool comment)
{
  std::string blue2light = "";
  std::string bluelight = "";
  std::string redlight = "";
  std::string yellowlight = "";
  std::string greenlight = "";
  std::string magentalight = "";
  std::string endcolor = "";
  if (color)
  {
    blue2light = BLUE2_LIGHT;
    bluelight = BLUE_LIGHT;
    redlight = RED_LIGHT;
    yellowlight = YELLOW_LIGHT;
    greenlight = GREEN_LIGHT;
    magentalight = MAGENTA_LIGHT;
    endcolor = END_COLOR;
  }

  // prevent invalid ordering of parameters caused by alphabetical output:
  // in the first run, print out all list elements that are not a sublist
  // in the second run, do the recursive call for all the sublists in the list
  for (int j = 0; j < 2; ++j)
  {
    for (Teuchos::ParameterList::ConstIterator i = list.begin(); i != list.end(); ++i)
    {
      const Teuchos::ParameterEntry& entry = list.entry(i);
      if (entry.isList() && j == 0) continue;
      if ((!entry.isList()) && j == 1) continue;
      const std::string& name = list.name(i);
      if (name == PrintEqualSign()) continue;
      Teuchos::RCP<const Teuchos::ParameterEntryValidator> validator = entry.validator();

      if (comment)
      {
        stream << blue2light << "//" << endcolor << '\n';

        std::string doc = entry.docString();
        if (doc != "")
        {
          Teuchos::StrUtils::printLines(stream, blue2light + "// ", doc);
          stream << endcolor;
        }
      }

      if (entry.isList())
      {
        std::string secname = parentname;
        if (secname != "") secname += "/";
        secname += name;
        unsigned l = secname.length();
        stream << redlight << "--";
        for (int i = 0; i < std::max<int>(65 - l, 0); ++i) stream << '-';
        stream << greenlight << secname << endcolor << '\n';
        PrintDatHeader(stream, list.sublist(name), secname, color, comment);
      }
      else
      {
        if (comment)
          if (validator != Teuchos::null)
          {
            Teuchos::RCP<const Teuchos::Array<std::string>> values = validator->validStringValues();
            if (values != Teuchos::null)
            {
              unsigned len = 0;
              for (int i = 0; i < (int)values->size(); ++i)
              {
                len += (*values)[i].length() + 1;
              }
              if (len < 74)
              {
                stream << blue2light << "//     ";
                for (int i = 0; i < static_cast<int>(values->size()) - 1; ++i)
                {
                  stream << magentalight << (*values)[i] << blue2light << ",";
                }
                stream << magentalight << (*values)[values->size() - 1] << endcolor << '\n';
              }
              else
              {
                for (int i = 0; i < (int)values->size(); ++i)
                {
                  stream << blue2light << "//     " << magentalight << (*values)[i] << endcolor
                         << '\n';
                }
              }
            }
          }
        const Teuchos::any& v = entry.getAny(false);
        stream << bluelight << name << endcolor;
        unsigned l = name.length();
        for (int i = 0; i < std::max<int>(31 - l, 0); ++i) stream << ' ';
        if (NeedToPrintEqualSign(list)) stream << " =";
        stream << ' ' << yellowlight << v << endcolor << '\n';
      }
    }
  }
}


/*----------------------------------------------------------------------*/
//! Print function
/*----------------------------------------------------------------------*/
void PrintDefaultDatHeader()
{
  Teuchos::RCP<const Teuchos::ParameterList> list = DRT::INPUT::ValidParameters();
  DRT::INPUT::PrintDatHeader(std::cout, *list, "", true);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::PrintDefaultParameters(IO::Pstream& stream, const Teuchos::ParameterList& list)
{
  bool hasDefault = false;
  for (Teuchos::ParameterList::ConstIterator i = list.begin(); i != list.end(); ++i)
  {
    const Teuchos::ParameterEntry& entry = list.entry(i);
    if (entry.isDefault())
    {
      if (not hasDefault)
      {
        hasDefault = true;
        stream << "default parameters in list '" << list.name() << "':\n";
      }
      const Teuchos::any& v = entry.getAny(false);
      int l = list.name(i).length();
      stream << "    " << list.name(i);
      for (int i = 0; i < std::max<int>(31 - l, 0); ++i) stream << ' ';
      stream << ' ' << v << '\n';
    }
  }
  if (hasDefault) stream << "\n";
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::BoolParameter(std::string const& paramName, std::string const& value,
    std::string const& docString, Teuchos::ParameterList* paramList)
{
  Teuchos::Array<std::string> yesnotuple =
      Teuchos::tuple<std::string>("Yes", "No", "yes", "no", "YES", "NO");
  Teuchos::Array<int> yesnovalue = Teuchos::tuple<int>(true, false, true, false, true, false);
  Teuchos::setStringToIntegralParameter<int>(
      paramName, value, docString, yesnotuple, yesnovalue, paramList);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::IntParameter(std::string const& paramName, int const value,
    std::string const& docString, Teuchos::ParameterList* paramList)
{
  Teuchos::AnyNumberParameterEntryValidator::AcceptedTypes validator(false);
  validator.allowInt(true);
  Teuchos::setIntParameter(paramName, value, docString, paramList, validator);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::DoubleParameter(std::string const& paramName, double const& value,
    std::string const& docString, Teuchos::ParameterList* paramList)
{
  Teuchos::AnyNumberParameterEntryValidator::AcceptedTypes validator(false);
  validator.allowDouble(true);
  validator.allowInt(true);
  Teuchos::setDoubleParameter(paramName, value, docString, paramList, validator);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::StringParameter(std::string const& paramName, std::string const& value,
    std::string const& docString, Teuchos::ParameterList* paramList)
{
  Teuchos::RCP<Teuchos::StringValidator> validator = Teuchos::rcp(new Teuchos::StringValidator());
  paramList->set(paramName, value, docString, validator);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Teuchos::RCP<const Teuchos::ParameterList> DRT::INPUT::ValidParameters()
{
  using Teuchos::setStringToIntegralParameter;
  using Teuchos::tuple;

  // define some tuples that are often used to account for different writing of certain key words
  Teuchos::Array<std::string> yesnotuple =
      tuple<std::string>("Yes", "No", "yes", "no", "YES", "NO");
  Teuchos::Array<int> yesnovalue = tuple<int>(true, false, true, false, true, false);

  Teuchos::RCP<Teuchos::ParameterList> list = Teuchos::rcp(new Teuchos::ParameterList);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& discret = list->sublist("DISCRETISATION", false, "");

  IntParameter("NUMFLUIDDIS", 1, "Number of meshes in fluid field", &discret);
  IntParameter("NUMSTRUCDIS", 1, "Number of meshes in structural field", &discret);
  IntParameter("NUMALEDIS", 1, "Number of meshes in ale field", &discret);
  IntParameter("NUMARTNETDIS", 1, "Number of meshes in arterial network field", &discret);
  IntParameter("NUMTHERMDIS", 1, "Number of meshes in thermal field", &discret);
  IntParameter("NUMAIRWAYSDIS", 1, "Number of meshes in reduced dimensional airways network field",
      &discret);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& size = list->sublist("PROBLEM SIZE", false, "");

  IntParameter("DIM", 3, "2d or 3d problem", &size);

  // deactivate all the follwing (unused) parameters one day
  // they are nice as general info in the input file but should not
  // read into a parameter list. Misuse is possible
  IntParameter("ELEMENTS", 0, "Total number of elements", &size);
  IntParameter("NODES", 0, "Total number of nodes", &size);
  IntParameter("NPATCHES", 0, "number of nurbs patches", &size);
  IntParameter("MATERIALS", 0, "number of materials", &size);
  IntParameter("NUMDF", 3, "maximum number of degrees of freedom", &size);

  INPAR::PROBLEMTYPE::SetValidParameters(list);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& meshpartitioning = list->sublist("MESH PARTITIONING", false, "");

  DoubleParameter("IMBALANCE_TOL", 1.1,
      "Tolerance for relative imbalance of subdomain sizes for graph partitioning of unstructured "
      "meshes read from input files.",
      &meshpartitioning);

  /*----------------------------------------------------------------------*/
  Teuchos::ParameterList& design =
      list->sublist("DESIGN DESCRIPTION", false, "number of nodal clouds");

  IntParameter("NDPOINT", 0, "number of points", &design);
  IntParameter("NDLINE", 0, "number of line clouds", &design);
  IntParameter("NDSURF", 0, "number of surface clouds", &design);
  IntParameter("NDVOL", 0, "number of volume clouds", &design);


  /*----------------------------------------------------------------------*/
  /* Finally call the problem-specific SetValidParameter functions        */
  /*----------------------------------------------------------------------*/

  INPAR::STR::SetValidParameters(list);
  INPAR::IO::SetValidParameters(list);
  INPAR::IO_MONITOR_STRUCTURE_DBC::SetValidParameters(list);
  INPAR::IO_RUNTIME_VTK::SetValidParameters(list);
  INPAR::IO_RUNTIME_VTP_STRUCTURE::SetValidParameters(list);
  INPAR::INVANA::SetValidParameters(list);
  INPAR::MLMC::SetValidParameters(list);
  INPAR::MORTAR::SetValidParameters(list);
  INPAR::CONTACT::SetValidParameters(list);
  INPAR::XCONTACT::SetValidParameters(list);
  INPAR::VOLMORTAR::SetValidParameters(list);
  INPAR::WEAR::SetValidParameters(list);
  INPAR::IO_RUNTIME_VTK::STRUCTURE::SetValidParameters(list);
  INPAR::IO_RUNTIME_VTK::BEAMS::SetValidParameters(list);
  INPAR::BEAMCONTACT::SetValidParameters(list);
  INPAR::BEAMPOTENTIAL::SetValidParameters(list);
  INPAR::BEAMINTERACTION::SetValidParameters(list);
  INPAR::BROWNIANDYN::SetValidParameters(list);

  INPAR::LOCA::SetValidParameters(list);
  INPAR::PLASTICITY::SetValidParameters(list);

  INPAR::THR::SetValidParameters(list);
  INPAR::TSI::SetValidParameters(list);

  INPAR::FLUID::SetValidParameters(list);
  INPAR::TWOPHASE::SetValidParameters(list);
  INPAR::LOMA::SetValidParameters(list);
  INPAR::TOPOPT::SetValidParameters(list);
  INPAR::CUT::SetValidParameters(list);
  INPAR::XFEM::SetValidParameters(list);

  INPAR::LUBRICATION::SetValidParameters(list);
  INPAR::SCATRA::SetValidParameters(list);
  INPAR::LEVELSET::SetValidParameters(list);
  INPAR::ELCH::SetValidParameters(list);
  INPAR::EP::SetValidParameters(list);
  INPAR::STI::SetValidParameters(list);

  INPAR::S2I::SetValidParameters(list);
  INPAR::FS3I::SetValidParameters(list);
  INPAR::POROELAST::SetValidParameters(list);
  INPAR::PORO_SCATRA::SetValidParameters(list);
  INPAR::POROMULTIPHASE::SetValidParameters(list);
  INPAR::POROMULTIPHASESCATRA::SetValidParameters(list);
  INPAR::POROFLUIDMULTIPHASE::SetValidParameters(list);
  INPAR::EHL::SetValidParameters(list);
  INPAR::SSI::SetValidParameters(list);
  INPAR::ALE::SetValidParameters(list);
  INPAR::FSI::SetValidParameters(list);

  INPAR::ARTDYN::SetValidParameters(list);
  INPAR::ARTNET::SetValidParameters(list);
  INPAR::BIOFILM::SetValidParameters(list);
  INPAR::PATSPEC::SetValidParameters(list);
  INPAR::REDAIRWAYS::SetValidParameters(list);
  INPAR::CARDIOVASCULAR0D::SetValidParameters(list);
  INPAR::IMMERSED::SetValidParameters(list);
  INPAR::FPSI::SetValidParameters(list);
  INPAR::FBI::SetValidParameters(list);

  INPAR::PARTICLE::SetValidParameters(list);

  INPAR::MOR::SetValidParameters(list);

  INPAR::ELEMAG::SetValidParameters(list);

  INPAR::GEO::SetValidParameters(list);
  INPAR::BINSTRATEGY::SetValidParameters(list);
  INPAR::PASI::SetValidParameters(list);

  INPAR::SOLVER::SetValidParameters(list);
  INPAR::NLNSOL::SetValidParameters(list);

  INPAR::TUTORIAL::SetValidParameters(list);

  return list;
}



/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
std::string DRT::INPUT::PrintEqualSign() { return "*PrintEqualSign*"; }

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void DRT::INPUT::SetPrintEqualSign(Teuchos::ParameterList& list, const bool& pes)
{
  std::string printequalsign = PrintEqualSign();
  list.set<bool>(printequalsign, pes);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
bool DRT::INPUT::NeedToPrintEqualSign(const Teuchos::ParameterList& list)
{
  const std::string printequalsign = PrintEqualSign();
  bool pes = false;
  try
  {
    pes = list.get<bool>(printequalsign);
  }
  catch (Teuchos::Exceptions::InvalidParameter)
  {
    pes = false;
  }
  return pes;
}
