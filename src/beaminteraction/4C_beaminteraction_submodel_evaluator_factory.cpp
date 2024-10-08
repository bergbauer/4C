/*-----------------------------------------------------------*/
/*! \file

\brief Factory to create the desired submodel evaluators.


\level 3

*/
/*-----------------------------------------------------------*/


#include "4C_beaminteraction_submodel_evaluator_factory.hpp"

#include "4C_beaminteraction_submodel_evaluator_beamcontact.hpp"
#include "4C_beaminteraction_submodel_evaluator_crosslinking.hpp"
#include "4C_beaminteraction_submodel_evaluator_potential.hpp"
#include "4C_beaminteraction_submodel_evaluator_spherebeamlinking.hpp"
#include "4C_global_data.hpp"
#include "4C_inpar_beaminteraction.hpp"

FOUR_C_NAMESPACE_OPEN

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
BEAMINTERACTION::SUBMODELEVALUATOR::Factory::Factory()
{
  // empty constructor
}


/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Solid::ModelEvaluator::BeamInteraction::Map>
BEAMINTERACTION::SUBMODELEVALUATOR::Factory::build_model_evaluators(
    const std::set<enum Inpar::BEAMINTERACTION::SubModelType>& submodeltypes) const
{
  // create a new standard map
  Teuchos::RCP<Solid::ModelEvaluator::BeamInteraction::Map> model_map =
      Teuchos::make_rcp<Solid::ModelEvaluator::BeamInteraction::Map>();

  std::set<enum Inpar::BEAMINTERACTION::SubModelType>::const_iterator mt_iter;
  for (mt_iter = submodeltypes.begin(); mt_iter != submodeltypes.end(); ++mt_iter)
  {
    switch (*mt_iter)
    {
      case Inpar::BEAMINTERACTION::submodel_beamcontact:
        (*model_map)[*mt_iter] =
            Teuchos::make_rcp<BEAMINTERACTION::SUBMODELEVALUATOR::BeamContact>();
        break;
      case Inpar::BEAMINTERACTION::submodel_crosslinking:
        (*model_map)[*mt_iter] =
            Teuchos::make_rcp<BEAMINTERACTION::SUBMODELEVALUATOR::Crosslinking>();
        break;
      case Inpar::BEAMINTERACTION::submodel_spherebeamlink:
        (*model_map)[*mt_iter] =
            Teuchos::make_rcp<BEAMINTERACTION::SUBMODELEVALUATOR::SphereBeamLinking>();
        break;
      case Inpar::BEAMINTERACTION::submodel_potential:
        (*model_map)[*mt_iter] =
            Teuchos::make_rcp<BEAMINTERACTION::SUBMODELEVALUATOR::BeamPotential>();
        break;
      default:
        FOUR_C_THROW("Not yet implemented!");
        break;
    }
  }

  return model_map;
}

/*----------------------------------------------------------------------------*
 *----------------------------------------------------------------------------*/
Teuchos::RCP<Solid::ModelEvaluator::BeamInteraction::Map>
BEAMINTERACTION::SUBMODELEVALUATOR::build_model_evaluators(
    const std::set<enum Inpar::BEAMINTERACTION::SubModelType>& submodeltypes)
{
  Factory factory;
  return factory.build_model_evaluators(submodeltypes);
}

FOUR_C_NAMESPACE_CLOSE
