/*----------------------------------------------------------------------*/
/*! \file
\brief temperature-dependent gas according to Sutherland law

\level 2

*/
/*----------------------------------------------------------------------*/


#include "4C_mat_sutherland.hpp"

#include "4C_global_data.hpp"
#include "4C_mat_par_bundle.hpp"

#include <vector>

FOUR_C_NAMESPACE_OPEN


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::PAR::Sutherland::Sutherland(Teuchos::RCP<Core::Mat::PAR::Material> matdata)
    : Parameter(matdata),
      refvisc_(matdata->Get<double>("REFVISC")),
      reftemp_(matdata->Get<double>("REFTEMP")),
      suthtemp_(matdata->Get<double>("SUTHTEMP")),
      shc_(matdata->Get<double>("SHC")),
      pranum_(matdata->Get<double>("PRANUM")),
      thermpress_(matdata->Get<double>("THERMPRESS")),
      gasconst_(matdata->Get<double>("GASCON"))
{
}

Teuchos::RCP<Core::Mat::Material> Mat::PAR::Sutherland::create_material()
{
  return Teuchos::rcp(new Mat::Sutherland(this));
}


Mat::SutherlandType Mat::SutherlandType::instance_;


Core::Communication::ParObject* Mat::SutherlandType::Create(const std::vector<char>& data)
{
  Mat::Sutherland* sutherland = new Mat::Sutherland();
  sutherland->Unpack(data);
  return sutherland;
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::Sutherland::Sutherland() : params_(nullptr) {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
Mat::Sutherland::Sutherland(Mat::PAR::Sutherland* params) : params_(params) {}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::Sutherland::Pack(Core::Communication::PackBuffer& data) const
{
  Core::Communication::PackBuffer::SizeMarker sm(data);

  // pack type of this instance of ParObject
  int type = UniqueParObjectId();
  add_to_pack(data, type);
  // matid
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  add_to_pack(data, matid);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
void Mat::Sutherland::Unpack(const std::vector<char>& data)
{
  std::vector<char>::size_type position = 0;

  Core::Communication::ExtractAndAssertId(position, data, UniqueParObjectId());

  // matid and recover params_
  int matid;
  extract_from_pack(position, data, matid);
  params_ = nullptr;
  if (Global::Problem::Instance()->Materials() != Teuchos::null)
    if (Global::Problem::Instance()->Materials()->Num() != 0)
    {
      const int probinst = Global::Problem::Instance()->Materials()->GetReadFromProblem();
      Core::Mat::PAR::Parameter* mat =
          Global::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
        params_ = static_cast<Mat::PAR::Sutherland*>(mat);
      else
        FOUR_C_THROW("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
    }

  if (position != data.size())
    FOUR_C_THROW("Mismatch in size of data %d <-> %d", data.size(), position);
}


/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double Mat::Sutherland::ComputeViscosity(const double temp) const
{
  // previous implementation using "pow"-function appears to be extremely
  // time-consuming sometimes, at least on the computing cluster
  // const double visc =
  // std::pow((temp/RefTemp()),1.5)*((RefTemp()+SuthTemp())/(temp+SuthTemp()))*RefVisc();
  const double visc = sqrt((temp / RefTemp()) * (temp / RefTemp()) * (temp / RefTemp())) *
                      ((RefTemp() + SuthTemp()) / (temp + SuthTemp())) * RefVisc();

  return visc;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double Mat::Sutherland::ComputeDiffusivity(const double temp) const
{
  // previous implementation using "pow"-function appears to be extremely
  // time-consuming sometimes, at least on the computing cluster
  // const double diffus =
  // std::pow((temp/RefTemp()),1.5)*((RefTemp()+SuthTemp())/(temp+SuthTemp()))*RefVisc()/PraNum();
  const double diffus = sqrt((temp / RefTemp()) * (temp / RefTemp()) * (temp / RefTemp())) *
                        ((RefTemp() + SuthTemp()) / (temp + SuthTemp())) * RefVisc() / PraNum();

  return diffus;
}

/*----------------------------------------------------------------------*/
/*----------------------------------------------------------------------*/
double Mat::Sutherland::ComputeDensity(const double temp, const double thermpress) const
{
  const double density = thermpress / (GasConst() * temp);

  return density;
}

FOUR_C_NAMESPACE_CLOSE