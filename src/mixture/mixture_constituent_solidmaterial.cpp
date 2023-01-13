/*----------------------------------------------------------------------*/
/*! \file

\brief Implementation of the general solid material constituent

\level 3


*/
/*----------------------------------------------------------------------*/

#include "mixture_constituent_solidmaterial.H"
#include <Teuchos_RCPDecl.hpp>
#include "mat_service.H"
#include "lib_globalproblem.H"
#include "mat_par_bundle.H"
#include "mat_mixture.H"

// Constructor for the parameter class
MIXTURE::PAR::MixtureConstituent_SolidMaterial::MixtureConstituent_SolidMaterial(
    const Teuchos::RCP<MAT::PAR::Material>& matdata)
    : MixtureConstituent(matdata), matid_(matdata->GetInt("MATID"))
{
}

// Create an instance of MIXTURE::MixtureConstituent_SolidMaterial from the parameters
std::unique_ptr<MIXTURE::MixtureConstituent>
MIXTURE::PAR::MixtureConstituent_SolidMaterial::CreateConstituent(int id)
{
  return std::unique_ptr<MIXTURE::MixtureConstituent_SolidMaterial>(
      new MIXTURE::MixtureConstituent_SolidMaterial(this, id));
}

// Constructor of the constituent holding the material parameters
MIXTURE::MixtureConstituent_SolidMaterial::MixtureConstituent_SolidMaterial(
    MIXTURE::PAR::MixtureConstituent_SolidMaterial* params, int id)
    : MixtureConstituent(params, id), params_(params), material_()
{
  // take the matid (i.e. here the id of the solid material), read the type and
  // create the corresponding material
  auto mat = MAT::Material::Factory(params_->matid_);

  // cast to an So3Material
  material_ = Teuchos::rcp_dynamic_cast<MAT::So3Material>(mat);

  // ensure cast was successfull
  if (Teuchos::is_null(mat))
    dserror(
        "The solid material constituent with ID %d needs to be an So3Material.", params_->matid_);

  if (material_->Density() - 1.0 > 1e-16)
    dserror(
        "Please set the density of the solid material constituent with ID %d to 1.0 and prescribe "
        "a combined density for the entire mixture material.",
        material_->Parameter()->Id());
}

INPAR::MAT::MaterialType MIXTURE::MixtureConstituent_SolidMaterial::MaterialType() const
{
  return INPAR::MAT::mix_solid_material;
}

void MIXTURE::MixtureConstituent_SolidMaterial::PackConstituent(DRT::PackBuffer& data) const
{
  // pack constituent data
  MixtureConstituent::PackConstituent(data);

  // add the matid of the Mixture_SolidMaterial
  int matid = -1;
  if (params_ != nullptr) matid = params_->Id();  // in case we are in post-process mode
  DRT::ParObject::AddtoPack(data, matid);

  // pack data of the solid material
  material_->Pack(data);
}

void MIXTURE::MixtureConstituent_SolidMaterial::UnpackConstituent(
    std::vector<char>::size_type& position, const std::vector<char>& data)
{
  // unpack constituent data
  MixtureConstituent::UnpackConstituent(position, data);

  // make sure we have a pristine material
  params_ = nullptr;
  material_ = Teuchos::null;

  // extract the matid of the Mixture_SolidMaterial
  int matid;
  DRT::ParObject::ExtractfromPack(position, data, matid);

  // recover the params_ of the Mixture_SolidMaterial
  if (DRT::Problem::Instance()->Materials() != Teuchos::null)
  {
    if (DRT::Problem::Instance()->Materials()->Num() != 0)
    {
      const unsigned int probinst = DRT::Problem::Instance()->Materials()->GetReadFromProblem();
      MAT::PAR::Parameter* mat =
          DRT::Problem::Instance(probinst)->Materials()->ParameterById(matid);
      if (mat->Type() == MaterialType())
      {
        params_ = dynamic_cast<MIXTURE::PAR::MixtureConstituent_SolidMaterial*>(mat);
      }
      else
      {
        dserror("Type of parameter material %d does not fit to calling type %d", mat->Type(),
            MaterialType());
      }
    }
  }

  // unpack the data of the solid material
  if (params_ != nullptr)
  {
    auto so3mat = MAT::Material::Factory(params_->matid_);
    material_ = Teuchos::rcp_dynamic_cast<MAT::So3Material>(so3mat);
    if (Teuchos::is_null(so3mat)) dserror("Failed to allocate");

    // solid material packed: 1. the data size, 2. the packed data of size sm
    // ExtractFromPack extracts a sub_vec of size sm from data and updates the position vector
    std::vector<char> sub_vec;
    DRT::ParObject::ExtractfromPack(position, data, sub_vec);
    material_->Unpack(sub_vec);
  }
}

INPAR::MAT::MaterialType MaterialType() { return INPAR::MAT::mix_solid_material; }

void MIXTURE::MixtureConstituent_SolidMaterial::ReadElement(
    int numgp, DRT::INPUT::LineDefinition* linedef)
{
  MixtureConstituent::ReadElement(numgp, linedef);
  material_->Setup(numgp, linedef);
}

void MIXTURE::MixtureConstituent_SolidMaterial::Update(LINALG::Matrix<3, 3> const& defgrd,
    Teuchos::ParameterList& params, const int gp, const int eleGID)
{
  material_->Update(defgrd, gp, params, eleGID);
}

void MIXTURE::MixtureConstituent_SolidMaterial::UpdatePrestress(LINALG::Matrix<3, 3> const& defgrd,
    Teuchos::ParameterList& params, const int gp, const int eleGID)
{
  material_->UpdatePrestress(defgrd, gp, params, eleGID);
}

void MIXTURE::MixtureConstituent_SolidMaterial::Evaluate(const LINALG::Matrix<3, 3>& F,
    const LINALG::Matrix<6, 1>& E_strain, Teuchos::ParameterList& params,
    LINALG::Matrix<6, 1>& S_stress, LINALG::Matrix<6, 6>& cmat, const int gp, const int eleGID)
{
  material_->Evaluate(&F, &E_strain, params, &S_stress, &cmat, gp, eleGID);
}

void MIXTURE::MixtureConstituent_SolidMaterial::RegisterVtkOutputDataNames(
    std::unordered_map<std::string, int>& names_and_size) const
{
  material_->RegisterVtkOutputDataNames(names_and_size);
}

bool MIXTURE::MixtureConstituent_SolidMaterial::EvaluateVtkOutputData(
    const std::string& name, Epetra_SerialDenseMatrix& data) const
{
  return material_->EvaluateVtkOutputData(name, data);
}