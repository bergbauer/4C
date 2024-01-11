/*----------------------------------------------------------------------*/
/*! \file


\brief Main routine

\author scheider

\level 0
*/

#include "baci_config_revision.H"

#include "baci_create_rtdfiles_wrapper.H"

#include "baci_comm_utils.H"
#include "baci_create_rtdfiles_utils.H"
#include "baci_inpar_validconditions.H"
#include "baci_inpar_validcontactconstitutivelaw.H"
#include "baci_inpar_validmaterials.H"
#include "baci_inpar_validparameters.H"
#include "baci_lib_elementdefinition.H"
#include "baci_lib_globalproblem.H"
#include "baci_lib_utils_createdis.H"
#include "baci_utils_exceptions.H"
#include "baci_utils_function.H"

#include <iostream>

BACI_NAMESPACE_OPEN

namespace RTD
{
  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  void WriteReadTheDocsHeader(const std::string& headerdocumentationfilename)
  {
    // open ascii file for writing all header parameters
    std::ofstream headerdocumentationfile(headerdocumentationfilename.c_str());
    if (!headerdocumentationfile)
      dserror("failed to open file: %s", headerdocumentationfilename.c_str());
    headerdocumentationfile << "..\n   Created using baci version (git SHA1):\n";
    headerdocumentationfile << "   " << BaciGitHash.c_str() << "\n\n";

    headerdocumentationfile << ".. _headerparameters:\n\n";
    headerdocumentationfile << "Header parameters\n";
    headerdocumentationfile << "=================\n\n";
    WriteHeaderReference(headerdocumentationfile, *INPUT::ValidParameters(), "");
  }

  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  void WriteReadTheDocsCelltypes(const std::string& celltypedocumentationfilename)
  {
    // open ascii file for writing all header parameters
    std::ofstream celltypeocumentationfile(celltypedocumentationfilename.c_str());
    if (!celltypeocumentationfile)
      dserror("failed to open file: %s", celltypedocumentationfilename.c_str());
    celltypeocumentationfile << "..\n   Created using baci version (git SHA1):\n";
    celltypeocumentationfile << "   " << BaciGitHash.c_str() << "\n\n";

    WriteCelltypeReference(celltypeocumentationfile);
  }

  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  void WriteReadTheDocsMaterial(const std::string& materialdocumentationfilename)
  {
    //
    // open ascii file for writing all material parameters
    std::ofstream materialdocumentationfile(materialdocumentationfilename.c_str());
    if (!materialdocumentationfile)
      dserror("failed to open file: %s", materialdocumentationfilename.c_str());
    materialdocumentationfile << "..\n   Created using baci version (git SHA1):\n";
    materialdocumentationfile << "   " << BaciGitHash.c_str() << "\n\n";
    WriteMaterialReference(materialdocumentationfile, *INPUT::ValidMaterials());
  }

  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  void WriteReadTheDocsCondition(const std::string& conditiondocumentationfilename)
  {
    //
    // open ascii file for writing all constrains / conditions parameters
    std::ofstream conditiondocumentationfile(conditiondocumentationfilename.c_str());
    if (!conditiondocumentationfile)
      dserror("failed to open file: %s", conditiondocumentationfilename.c_str());
    conditiondocumentationfile << "..\n   Created using baci version (git SHA1):\n";
    conditiondocumentationfile << "   " << BaciGitHash.c_str() << "\n\n";
    WriteConditionsReference(conditiondocumentationfile, *INPUT::ValidConditions());

    WriteContactLawReference(conditiondocumentationfile, *INPUT::ValidContactConstitutiveLaws());
  }

  /*----------------------------------------------------------------------*/
  /*----------------------------------------------------------------------*/
  void WriteReadTheDocsVarious(const std::string& variousdocumentationfilename)
  {
    //
    // open ascii file for writing all constrains / conditions parameters
    std::ofstream variousdocumentationfile(variousdocumentationfilename.c_str());
    if (!variousdocumentationfile)
      dserror("failed to open file: %s", variousdocumentationfilename.c_str());
    variousdocumentationfile << "..\n   Created using baci version (git SHA1):\n";
    variousdocumentationfile << "   " << BaciGitHash.c_str() << "\n\n";
    WriteVariousReference(variousdocumentationfile);
  }

  void PrintHelpMessage()
  {
    std::cout << "This program writes all necessary reference files for readthedocs";
  }

}  // namespace RTD

BACI_NAMESPACE_CLOSE
