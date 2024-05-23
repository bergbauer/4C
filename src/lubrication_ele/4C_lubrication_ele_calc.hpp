/*--------------------------------------------------------------------------*/
/*! \file

\brief main file containing routines for calculation of lubrication element

\level 3

*/
/*--------------------------------------------------------------------------*/
#ifndef FOUR_C_LUBRICATION_ELE_CALC_HPP
#define FOUR_C_LUBRICATION_ELE_CALC_HPP


#include "4C_config.hpp"

#include "4C_discretization_fem_general_utils_boundary_integration.hpp"
#include "4C_discretization_fem_general_utils_fem_shapefunctions.hpp"
#include "4C_discretization_fem_general_utils_integration.hpp"
#include "4C_discretization_fem_general_utils_local_connectivity_matrices.hpp"
#include "4C_lib_element.hpp"
#include "4C_lubrication_ele_action.hpp"
#include "4C_lubrication_ele_interface.hpp"

FOUR_C_NAMESPACE_OPEN



namespace DRT
{
  namespace ELEMENTS
  {
    // forward declaration
    class LubricationEleParameter;

    class LubricationEleViscManager;
    template <int NSD, int NEN>
    class LubricationEleInternalVariableManager;

    /// Lubrication element implementation
    /*!
      This internal class keeps all the working arrays needed to
      calculate the Lubrication element. Additionally, the method Sysmat()
      provides a clean and fast element implementation.

      <h3>Purpose</h3>

      The idea is to separate the element maintenance (class Lubrication) from the
      mathematical contents (this class). There are different
      implementations of the Lubrication element, this is just one such
      implementation.

      The Lubrication element will allocate exactly one object of this class for all
      Lubrication elements with the same number of nodes in the mesh. This
      allows us to use exactly matching working arrays (and keep them
      around.)

      The code is meant to be as clean as possible. This is the only way
      to keep it fast. The number of working arrays has to be reduced to
      a minimum so that the element fits into the cache. (There might be
      room for improvements.)

      <h3>Usability</h3>

      The calculations are done by the Evaluate() method. There are two
      version. The virtual method that is inherited from LubricationEleInterface
      (and called from Lubrication) and the non-virtual one that does the actual
      work. The non-virtual Evaluate() method must be callable without an actual
      Lubrication object.
    */

    template <CORE::FE::CellType distype, int probdim = CORE::FE::dim<distype>>
    class LubricationEleCalc : public LubricationEleInterface
    {
     protected:
      /// (private) protected constructor, since we are a Singleton.
      /// this constructor is called from a derived class
      /// -> therefore, it has to be protected instead of private
      LubricationEleCalc(const std::string& disname);

     public:
      //! singleton access method
      static LubricationEleCalc<distype, probdim>* Instance(const std::string& disname);

      /// In this class we do not define a static LubricationEle...* Instance
      /// since only derived child classes are free to be allocated!!

      /// Setup element evaluation
      int SetupCalc(DRT::Element* ele, DRT::Discretization& discretization) override;

      /// Evaluate the element
      /*!
        Generic virtual interface function. Called via base pointer.
       */
      int Evaluate(DRT::Element* ele, Teuchos::ParameterList& params,
          DRT::Discretization& discretization, DRT::Element::LocationArray& la,
          CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
          CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
          CORE::LINALG::SerialDenseVector& elevec1_epetra,
          CORE::LINALG::SerialDenseVector& elevec2_epetra,
          CORE::LINALG::SerialDenseVector& elevec3_epetra) override;

      // Evaluate the off-diagonal coupling block of monotlitic EHL matrix
      int EvaluateEHLMon(DRT::Element* ele, Teuchos::ParameterList& params,
          DRT::Discretization& discretization, DRT::Element::LocationArray& la,
          CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
          CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
          CORE::LINALG::SerialDenseVector& elevec1_epetra,
          CORE::LINALG::SerialDenseVector& elevec2_epetra,
          CORE::LINALG::SerialDenseVector& elevec3_epetra) override;

      //! evaluate action
      virtual int EvaluateAction(DRT::Element* ele, Teuchos::ParameterList& params,
          DRT::Discretization& discretization, const LUBRICATION::Action& action,
          DRT::Element::LocationArray& la, CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
          CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
          CORE::LINALG::SerialDenseVector& elevec1_epetra,
          CORE::LINALG::SerialDenseVector& elevec2_epetra,
          CORE::LINALG::SerialDenseVector& elevec3_epetra);

      //! evaluate service routine
      int EvaluateService(DRT::Element* ele, Teuchos::ParameterList& params,
          DRT::Discretization& discretization, DRT::Element::LocationArray& la,
          CORE::LINALG::SerialDenseMatrix& elemat1_epetra,
          CORE::LINALG::SerialDenseMatrix& elemat2_epetra,
          CORE::LINALG::SerialDenseVector& elevec1_epetra,
          CORE::LINALG::SerialDenseVector& elevec2_epetra,
          CORE::LINALG::SerialDenseVector& elevec3_epetra) override;

      /*========================================================================*/
      //! @name static member variables
      /*========================================================================*/

      //! number of element nodes (nomenclature: T. Hughes, The finite element method)
      static constexpr int nen_ = CORE::FE::num_nodes<distype>;

      //! number of space dimensions
      static constexpr int nsd_ = probdim;

      //! space dimension of Lubrication element (only for flat domains nsd_ele_ = nsd_)
      static constexpr int nsd_ele_ = CORE::FE::dim<distype>;

     protected:
      /*========================================================================*/
      //! @name general framework
      /*========================================================================*/

      //! extract element based or nodal values
      //  return extracted values of prenp
      virtual void extract_element_and_node_values(DRT::Element* ele,
          Teuchos::ParameterList& params, DRT::Discretization& discretization,
          DRT::Element::LocationArray& la);

      //! calculate matrix and rhs. Here the whole thing is hidden.
      virtual void Sysmat(DRT::Element* ele,      //!< the element we are dealing with
          CORE::LINALG::SerialDenseMatrix& emat,  //!< element matrix to calculate
          CORE::LINALG::SerialDenseVector& erhs   //!< element rhs to calculate
      );

      //! calculate element off-diagonal-matrix for height-linearization in monolithic EHL
      virtual void MatrixforEHLMon(DRT::Element* ele,  //!< the element we are dealing with
          CORE::LINALG::SerialDenseMatrix&
              ematheight,  //!< element matrix associated with the linearization of the film height
          CORE::LINALG::SerialDenseMatrix&
              ematvel  //!< element matrix associated with the linearization of the velocities
      );

      //! Calculate the height of the lubrication at the Int point
      virtual void calc_height_at_int_point(double& heightint  //!< lubrication height at Int point
      );

      //! Calculate the heightDot (time derivative) of the film-thickness at the Int point
      virtual void calc_height_dot_at_int_point(
          double& heightdotint  //!< lubrication heightDot at Int point
      );

      //! Calculate the average velocity of the contacting bodies at the Int point
      virtual void calc_avr_vel_at_int_point(
          CORE::LINALG::Matrix<nsd_, 1>& avrvel  //!< average surface velocity at Int point
      );

      //! Calculate the relative velocity of the contacting bodies at the Int point
      virtual void calc_rel_vel_at_int_point(
          CORE::LINALG::Matrix<nsd_, 1>& relvel  //!< relative surface velocity at Int point
      );

      //! read element coordinates
      virtual void read_element_coordinates(const DRT::Element* ele);

      //! evaluate shape functions and their derivatives at current integration point
      double eval_shape_func_and_derivs_at_int_point(
          const CORE::FE::IntPointsAndWeights<nsd_ele_>& intpoints,  //!< integration points
          const int iquad                                            //!< id of current Gauss point
      );

      //! evaluate shape functions and their derivatives at current integration point
      double eval_shape_func_and_derivs_in_parameter_space();

      //! set internal variables
      virtual void set_internal_variables_for_mat_and_rhs();

      //! evaluate pressure and shear flow factors for modified reynolds equation
      virtual void calc_p_flow_fac_at_int_point(CORE::LINALG::Matrix<nsd_, 1>& pflowfac,
          CORE::LINALG::Matrix<nsd_, 1>& pflowfacderiv, const double heightint);

      //! evaluate pressure and shear flow factors for modified reynolds equation
      virtual void calc_s_flow_fac_at_int_point(
          double& sflowfac, double& sflowfacderiv, const double heightint);

      /*========================================================================*/
      //! @name routines for additional element evaluations (called from EvaluateAction)
      /*========================================================================*/

      //! calculate error of numerical solution with respect to analytical solution
      void cal_error_compared_to_analyt_solution(
          const DRT::Element* ele,                 //!< the element we are dealing with
          Teuchos::ParameterList& params,          //!< parameter list
          CORE::LINALG::SerialDenseVector& errors  //!< vector containing L2-error norm
      );

      //! calculate pressure(s) and domain integral
      virtual void CalculatePressures(const DRT::Element* ele,  //!< the element we are dealing with
          CORE::LINALG::SerialDenseVector& pressures,           //!< pressure to be computed
          const bool inverting                                  //!< bool indicating inversion
      );


      /*========================================================================*/
      //! @name material and related functions
      /*========================================================================*/

      //! get the material parameters
      virtual void GetMaterialParams(const DRT::Element* ele,  //!< the element we are dealing with
          double& densn,                                       //!< density at t_(n)
          double& densnp,       //!< density at t_(n+1) or t_(n+alpha_F)
          double& densam,       //!< density at t_(n+alpha_M)
          double& visc,         //!< fluid viscosity
          double& dvisc,        //!< derivative of the fluid viscosity
          const int iquad = -1  //!< id of current gauss point (default = -1)
      );

      //! evaluate material
      virtual void Materials(
          const Teuchos::RCP<CORE::MAT::Material> material,  //!< pointer to current material
          double& densn,                                     //!< density at t_(n)
          double& densnp,       //!< density at t_(n+1) or t_(n+alpha_F)
          double& densam,       //!< density at t_(n+alpha_M)
          double& visc,         //!< fluid viscosity
          double& dvisc,        //!< derivative of the fluid viscosity
          const int iquad = -1  //!< id of current gauss point (default = -1)
      );

      //! material Lubrication
      virtual void MatLubrication(
          const Teuchos::RCP<CORE::MAT::Material> material,  //!< pointer to current material
          double& densn,                                     //!< density at t_(n)
          double& densnp,       //!< density at t_(n+1) or t_(n+alpha_F)
          double& densam,       //!< density at t_(n+alpha_M)
          double& visc,         //!< fluid viscosity
          double& dvisc,        //!< derivative of the fluid viscosity
          const int iquad = -1  //!< id of current gauss point (default = -1)
      );
      /*========================================================================*/
      //! @name methods for evaluation of individual terms
      /*========================================================================*/

      //! calculate linearization of the Laplacian (weak form) for element integration
      virtual void get_laplacian_weak_form(double& val, const int vi, const int ui);
      //! calculate linearization of the Laplacian (weak form) for element integration
      virtual void get_laplacian_weak_form(
          double& val, const int vi, const int ui, const CORE::LINALG::Matrix<nsd_, 1> pflowfac);
      //! calculate the Laplacian (weak form)
      virtual void get_laplacian_weak_form_rhs(
          double& val, const CORE::LINALG::Matrix<nsd_, 1>& gradpre, const int vi);
      //! calculate the Laplacian (weak form)
      virtual void get_laplacian_weak_form_rhs(double& val,
          const CORE::LINALG::Matrix<nsd_, 1>& gradpre, const int vi,
          const CORE::LINALG::Matrix<nsd_, 1> pflowfac);
      //! calculation of Poiseuille contribution to element matrix
      virtual void CalcMatPsl(CORE::LINALG::SerialDenseMatrix& emat, const double timefacfac,
          const double viscosity, const double height);

      //! calculation of Poiseuille contribution to element matrix in modified Rey. Equ.
      virtual void CalcMatPsl(CORE::LINALG::SerialDenseMatrix& emat, const double timefacfac,
          const double viscosity, const double height,
          const CORE::LINALG::Matrix<nsd_, 1> pflowfac);

      //! calculation of Poiseuille-Viscosity contribution to element matrix in modified Rey. Equ.
      virtual void CalcMatPslVis(CORE::LINALG::SerialDenseMatrix& emat, const double timefacfac,
          const double viscosity, const double height, const double dviscosity_dp);

      //! calculation of Poiseuille contribution to RHS matrix
      virtual void CalcRhsPsl(CORE::LINALG::SerialDenseVector& erhs, const double rhsfac,
          const double viscosity, const double height);

      //! calculation of Poiseuille contribution to RHS matrix in modified Rey. Equ.
      virtual void CalcRhsPsl(CORE::LINALG::SerialDenseVector& erhs, const double rhsfac,
          const double viscosity, const double height,
          const CORE::LINALG::Matrix<nsd_, 1> pflowfac);

      //! calculation of Wedge contribution to RHS matrix
      virtual void CalcRhsWdg(CORE::LINALG::SerialDenseVector& erhs, const double rhsfac,
          const double height, const CORE::LINALG::Matrix<nsd_, 1> velocity);

      //! calculation of Squeeze contribution to RHS matrix
      virtual void CalcRhsSqz(
          CORE::LINALG::SerialDenseVector& erhs, const double rhsfac, const double heightdot);

      //! calculation of Shear contribution to RHS matrix
      virtual void CalcRhsShear(CORE::LINALG::SerialDenseVector& erhs, const double rhsfac,
          const CORE::LINALG::Matrix<nsd_, 1> velocity, const double sflowfac);

      /*========================================================================*/
      //! @name parameter lists
      /*========================================================================*/

      //! pointer to general lubrication parameter class
      DRT::ELEMENTS::LubricationEleParameter* lubricationpara_;

      /*========================================================================*/
      //! @name pressure degrees of freedom and related
      /*========================================================================*/

      //! state variables at t_(n+1) or t_(n+alpha_F)
      CORE::LINALG::Matrix<nen_, 1> eprenp_;

      /*========================================================================*/
      //! @name Galerkin approximation and related
      /*========================================================================*/

      //! coordinates of current integration point in reference coordinates
      CORE::LINALG::Matrix<nsd_ele_, 1> xsi_;
      //! node coordinates
      CORE::LINALG::Matrix<nsd_, nen_> xyze_;
      //! array for shape functions
      CORE::LINALG::Matrix<nen_, 1> funct_;
      //! array for shape function derivatives w.r.t r,s,t
      CORE::LINALG::Matrix<nsd_, nen_> deriv_;
      //! global derivatives of shape functions w.r.t x,y,z
      CORE::LINALG::Matrix<nsd_, nen_> derxy_;

      //! transposed jacobian "dx/ds"
      CORE::LINALG::Matrix<nsd_, nsd_> xjm_;
      //! inverse of transposed jacobian "ds/dx"
      CORE::LINALG::Matrix<nsd_, nsd_> xij_;

      //!  array for element nodal film height at time n+1, (same scalar value for all space
      //!  dimensions)
      CORE::LINALG::Matrix<nsd_, nen_> eheinp_;

      //! array for the element nodal film height time derivative at time n+1,
      CORE::LINALG::Matrix<nsd_, nen_> eheidotnp_;

      //! Average tangential interface velocity
      CORE::LINALG::Matrix<nsd_, nen_> eAvTangVel_;

      //! Relative tangential interface velocity
      CORE::LINALG::Matrix<nsd_, nen_> eRelTangVel_;

      //!  array of element nodal displacement at time n+1
      CORE::LINALG::Matrix<nsd_, nen_> edispnp_;

      /*========================================================================*/
      //! @name manager classes for efficient application to various problems
      /*========================================================================*/

      //! manager for viscosity
      Teuchos::RCP<LubricationEleViscManager> viscmanager_;

      //! variable manager for Gauss point values
      Teuchos::RCP<LubricationEleInternalVariableManager<nsd_, nen_>> lubricationvarmanager_;

      /*========================================================================*/
      //! @name can be very useful
      /*========================================================================*/

      //! global element id
      int eid_;
      //! current element
      DRT::Element* ele_;
      //! time step
      double Dt_;

      /// Pressure flow factor, initialized to zero
      CORE::LINALG::Matrix<nsd_, 1> pflowfac_;

      /// Pressure flow factor derivative, initialized to zero
      CORE::LINALG::Matrix<nsd_, 1> pflowfacderiv_;

      /// shear flow factor
      double sflowfac_;

      /// shear flow factor derivative
      double sflowfacderiv_;
    };

    /// LubricationEleInternalVariableManager implementation
    /*!
      This class manages all internal variables needed for the evaluation of an element.

      All formulation-specific internal variables are stored and managed by a class derived from
      this class.
    */
    template <int NSD, int NEN>
    class LubricationEleInternalVariableManager
    {
     public:
      LubricationEleInternalVariableManager() : prenp_(0.0), gradpre_(true) { return; }

      virtual ~LubricationEleInternalVariableManager() = default;

      // compute and set internal variables
      void set_internal_variables(
          CORE::LINALG::Matrix<NEN, 1>& funct,  //! array for shape functions
          CORE::LINALG::Matrix<NSD, NEN>&
              derxy,  //! global derivatives of shape functions w.r.t x,y,z
          CORE::LINALG::Matrix<NEN, 1>& eprenp  //! pressure at t_(n+1) or t_(n+alpha_F)
      )
      {
        // calculate pressure at t_(n+1) or t_(n+alpha_F)
        prenp_ = funct.Dot(eprenp);
        // spatial gradient of current pressure value
        gradpre_.Multiply(derxy, eprenp);

        return;
      };

      /*========================================================================*/
      //! @name return methods for internal variables
      /*========================================================================*/

      //! return pressure values at t_(n+1) or t_(n+alpha_F)
      virtual const double& Prenp() const { return prenp_; };
      //! return spatial gradient of all pressure values
      virtual const CORE::LINALG::Matrix<NSD, 1>& GradPre() const { return gradpre_; };

     protected:
      /*========================================================================*/
      //! @name internal variables evaluated at element center or Gauss point
      /*========================================================================*/

      //! pressure
      double prenp_;
      //! spatial gradient of current pressure value
      CORE::LINALG::Matrix<NSD, 1> gradpre_;
    };

    /// Lubrication diffusion manager
    /*!
        This is a basic class to handle diffusion. It exclusively contains
        the isotropic diffusion coefficient. For anisotropic diffusion or
        more advanced diffusion laws, e.g., nonlinear ones, a derived class
        has to be constructed in the problem-dependent subclass for element
        evaluation.
    */
    class LubricationEleViscManager
    {
     public:
      LubricationEleViscManager() : visc_(0.0) { return; }

      virtual ~LubricationEleViscManager() = default;

      //! Set the isotropic diffusion coefficient
      virtual void SetIsotropicVisc(const double visc)
      {
        if (visc < 0.0) FOUR_C_THROW("negative (physical) viscosity: %f", 0, visc);

        visc_ = visc;
        return;
      }

      //! Return the stored isotropic diffusion coefficients
      virtual double GetIsotropicDiff() { return visc_; }

     protected:
      //! lubricant viscosity
      double visc_;
    };

  }  // namespace ELEMENTS
}  // namespace DRT

FOUR_C_NAMESPACE_CLOSE

#endif
