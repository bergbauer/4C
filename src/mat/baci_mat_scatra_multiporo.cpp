/*----------------------------------------------------------------------*/
/*! \file
 \brief scatra material for transport within multiphase porous medium

   \level 3

 *----------------------------------------------------------------------*/



#include "baci_mat_scatra_multiporo.hpp"

#include "baci_comm_utils.hpp"
#include "baci_global_data.hpp"
#include "baci_mat_par_bundle.hpp"

#include <vector>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::PAR::ScatraMatMultiPoroFluid::ScatraMatMultiPoroFluid(Teuchos::RCP<MAT::PAR::Material> matdata)
    : ScatraMat(matdata),
      phaseID_(*matdata->Get<int>("PHASEID")),
      delta_(*matdata->Get<double>("DELTA")),
      min_sat_(*matdata->Get<double>("MIN_SAT")),
      relative_mobility_funct_id_(*matdata->Get<int>("RELATIVE_MOBILITY_FUNCTION_ID"))
{
}

Teuchos::RCP<MAT::Material> MAT::PAR::ScatraMatMultiPoroFluid::CreateMaterial()
{
  return Teuchos::rcp(new MAT::ScatraMatMultiPoroFluid(this));
}


MAT::ScatraMatMultiPoroFluidType MAT::ScatraMatMultiPoroFluidType::instance_;

CORE::COMM::ParObject* MAT::ScatraMatMultiPoroFluidType::Create(const std::vector<char>& data)
{
  MAT::ScatraMatMultiPoroFluid* scatra_mat = new MAT::ScatraMatMultiPoroFluid();
  scatra_mat->Unpack(data);
  return scatra_mat;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroFluid::ScatraMatMultiPoroFluid() : params_(nullptr) {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroFluid::ScatraMatMultiPoroFluid(MAT::PAR::ScatraMatMultiPoroFluid* params)
    : ScatraMat(params), params_(params)
{
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroFluid::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data, matid);

  // add base class material
  ScatraMat::Pack(data);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroFluid::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid
  int matid;
  ExtractfromPack(position, data, matid);
  params_ = nullptr;
  if (GLOBAL::Problem::Instance()->Materials() != Teuchos::null)
    if (GLOBAL::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::ScatraMatMultiPoroFluid*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }

  // extract base class material
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  ScatraMat::Unpack(basedata);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::PAR::ScatraMatMultiPoroVolFrac::ScatraMatMultiPoroVolFrac(
    Teuchos::RCP<MAT::PAR::Material> matdata)
    : ScatraMat(matdata),
      phaseID_(*matdata->Get<int>("PHASEID")),
      delta_(*matdata->Get<double>("DELTA")),
      relative_mobility_funct_id_(*matdata->Get<int>("RELATIVE_MOBILITY_FUNCTION_ID"))
{
}

Teuchos::RCP<MAT::Material> MAT::PAR::ScatraMatMultiPoroVolFrac::CreateMaterial()
{
  return Teuchos::rcp(new MAT::ScatraMatMultiPoroVolFrac(this));
}


MAT::ScatraMatMultiPoroVolFracType MAT::ScatraMatMultiPoroVolFracType::instance_;

CORE::COMM::ParObject* MAT::ScatraMatMultiPoroVolFracType::Create(const std::vector<char>& data)
{
  MAT::ScatraMatMultiPoroVolFrac* scatra_mat = new MAT::ScatraMatMultiPoroVolFrac();
  scatra_mat->Unpack(data);
  return scatra_mat;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroVolFrac::ScatraMatMultiPoroVolFrac() : params_(nullptr) {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroVolFrac::ScatraMatMultiPoroVolFrac(
    MAT::PAR::ScatraMatMultiPoroVolFrac* params)
    : ScatraMat(params), params_(params)
{
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroVolFrac::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data, matid);

  // add base class material
  ScatraMat::Pack(data);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroVolFrac::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid
  int matid;
  ExtractfromPack(position, data, matid);
  params_ = nullptr;
  if (GLOBAL::Problem::Instance()->Materials() != Teuchos::null)
    if (GLOBAL::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::ScatraMatMultiPoroVolFrac*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }

  // extract base class material
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  ScatraMat::Unpack(basedata);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

MAT::PAR::ScatraMatMultiPoroSolid::ScatraMatMultiPoroSolid(Teuchos::RCP<MAT::PAR::Material> matdata)
    : ScatraMat(matdata), delta_(*matdata->Get<double>("DELTA"))
{
}

Teuchos::RCP<MAT::Material> MAT::PAR::ScatraMatMultiPoroSolid::CreateMaterial()
{
  return Teuchos::rcp(new MAT::ScatraMatMultiPoroSolid(this));
}

MAT::ScatraMatMultiPoroSolidType MAT::ScatraMatMultiPoroSolidType::instance_;

CORE::COMM::ParObject* MAT::ScatraMatMultiPoroSolidType::Create(const std::vector<char>& data)
{
  MAT::ScatraMatMultiPoroSolid* scatra_mat = new MAT::ScatraMatMultiPoroSolid();
  scatra_mat->Unpack(data);
  return scatra_mat;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroSolid::ScatraMatMultiPoroSolid() : params_(nullptr) {}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroSolid::ScatraMatMultiPoroSolid(MAT::PAR::ScatraMatMultiPoroSolid* params)
    : ScatraMat(params), params_(params)
{
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroSolid::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data, matid);

  // add base class material
  ScatraMat::Pack(data);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroSolid::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid
  int matid;
  ExtractfromPack(position, data, matid);
  params_ = nullptr;
  if (GLOBAL::Problem::Instance()->Materials() != Teuchos::null)
    if (GLOBAL::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::ScatraMatMultiPoroSolid*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }

  // extract base class material
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  ScatraMat::Unpack(basedata);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/

MAT::PAR::ScatraMatMultiPoroTemperature::ScatraMatMultiPoroTemperature(
    Teuchos::RCP<MAT::PAR::Material> matdata)
    : ScatraMat(matdata),
      numfluidphases_(*matdata->Get<int>("NUMFLUIDPHASES_IN_MULTIPHASEPORESPACE")),
      numvolfrac_(*matdata->Get<int>("NUMVOLFRAC")),
      cp_fluid_(*(matdata->Get<std::vector<double>>("CP_FLUID"))),
      cp_volfrac_(*(matdata->Get<std::vector<double>>("CP_VOLFRAC"))),
      cp_solid_(*matdata->Get<double>("CP_SOLID")),
      kappa_fluid_(*(matdata->Get<std::vector<double>>("KAPPA_FLUID"))),
      kappa_volfrac_(*(matdata->Get<std::vector<double>>("KAPPA_VOLFRAC"))),
      kappa_solid_(*matdata->Get<double>("KAPPA_SOLID"))
{
}

Teuchos::RCP<MAT::Material> MAT::PAR::ScatraMatMultiPoroTemperature::CreateMaterial()
{
  return Teuchos::rcp(new MAT::ScatraMatMultiPoroTemperature(this));
}

MAT::ScatraMatMultiPoroTemperatureType MAT::ScatraMatMultiPoroTemperatureType::instance_;

CORE::COMM::ParObject* MAT::ScatraMatMultiPoroTemperatureType::Create(const std::vector<char>& data)
{
  MAT::ScatraMatMultiPoroTemperature* scatra_mat = new MAT::ScatraMatMultiPoroTemperature();
  scatra_mat->Unpack(data);
  return scatra_mat;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroTemperature::ScatraMatMultiPoroTemperature() : params_(nullptr) {}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
MAT::ScatraMatMultiPoroTemperature::ScatraMatMultiPoroTemperature(
    MAT::PAR::ScatraMatMultiPoroTemperature* params)
    : ScatraMat(params), params_(params)
{
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroTemperature::Pack(CORE::COMM::PackBuffer& data) const
{
  CORE::COMM::PackBuffer::SizeMarker sm(data);
  sm.Insert();

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  AddtoPack(data, type);

  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  AddtoPack(data, matid);

  // add base class material
  ScatraMat::Pack(data);
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void MAT::ScatraMatMultiPoroTemperature::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  CORE::COMM::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid
  int matid;
  ExtractfromPack(position, data, matid);
  params_ = nullptr;
  if (GLOBAL::Problem::Instance()->Materials() != Teuchos::null)
    if (GLOBAL::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = GLOBAL::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          GLOBAL::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<MAT::PAR::ScatraMatMultiPoroTemperature*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }

  // extract base class material
  std::vector<char> basedata(0);
  ExtractfromPack(position, data, basedata);
  ScatraMat::Unpack(basedata);
}

FOUR_C_NAMESPACE_CLOSE