/*---------------------------------------------------------------------*/
/*! \file

\brief Concrete implementation of the beam parameter interface


\level 3
*/
/*---------------------------------------------------------------------*/

#include "structure_new_model_evaluator_data.H"

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
STR::MODELEVALUATOR::BeamData::BeamData()
    : isinit_(false), issetup_(false), beta_(-1.0), gamma_(-1.0), alphaf_(-1.0), alpham_(-1.0)
{
  // empty constructor
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::BeamData::Init()
{
  issetup_ = false;
  isinit_ = true;
}

/*----------------------------------------------------------------------*
 *----------------------------------------------------------------------*/
void STR::MODELEVALUATOR::BeamData::Setup()
{
  CheckInit();

  issetup_ = true;
}
