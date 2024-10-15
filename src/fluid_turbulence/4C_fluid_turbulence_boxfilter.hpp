/*----------------------------------------------------------------------*/
/*! \file

\brief Filter methods for the dynamic Smagorinsky, dynamic Vreman and any other model requiring a
box filter

References are

    M. Germano, U. Piomelli, P. Moin, W.H. Cabot:
    A dynamic subgrid-scale eddy viscosity model
    (Phys. Fluids 1991)

    or

    D.K. Lilly:
    A proposed modification of the Germano subgrid-scale closure method
    (Phys. Fluids 1992)

    or
    A.E. Tejada-Martinez
    Dynamic subgrid-scale modeling for large eddy simulation of turbulent
    flows with a stabilized finite element method
    (Phd thesis, Rensselaer Polytechnic Institute, Troy, New York)


\level 2

*/
/*----------------------------------------------------------------------*/

#ifndef FOUR_C_FLUID_TURBULENCE_BOXFILTER_HPP
#define FOUR_C_FLUID_TURBULENCE_BOXFILTER_HPP


#include "4C_config.hpp"

#include "4C_fem_discretization.hpp"
#include "4C_fem_general_node.hpp"
#include "4C_inpar_fluid.hpp"
#include "4C_inpar_scatra.hpp"
#include "4C_linalg_utils_sparse_algebra_manipulation.hpp"
#include "4C_linalg_vector.hpp"
#include "4C_utils_parameter_list.fwd.hpp"

#include <Teuchos_RCP.hpp>
#include <Teuchos_TimeMonitor.hpp>

FOUR_C_NAMESPACE_OPEN


namespace FLD
{
  class Boxfilter
  {
   public:
    /*!
    \brief Standard Constructor (public)

    */
    Boxfilter(Teuchos::RCP<Core::FE::Discretization> actdis, Teuchos::ParameterList& params);

    /*!
    \brief Destructor

    */
    virtual ~Boxfilter() = default;

    void add_scatra(Teuchos::RCP<Core::FE::Discretization> scatradis);

    void initialize_vreman_scatra(Teuchos::RCP<Core::FE::Discretization> scatradis);

    /*!
    \brief Perform box filter operation, compare filtered quantities
    to solution to get an estimate for Cs (using clpping), average
    over element layers in turbulent channel flows.

    This method initialises element quantities (standard case) or
    provides information for the element via the parameter list
    (in plane averaging for channel flow)

    \param solution     (in) velocity field to filter and to
                             determine Cs from
    \param dirichtoggle (in) information on dirichlet dofs to be
                             able to exclude boundary nodes from
                             filtering

    */

    void initialize_vreman();

    void apply_filter(const Teuchos::RCP<const Core::LinAlg::Vector<double>> velocity,
        const Teuchos::RCP<const Core::LinAlg::Vector<double>> scalar, const double thermpress,
        const Teuchos::RCP<const Core::LinAlg::Vector<double>> dirichtoggle);

    void apply_filter_scatra(const Teuchos::RCP<const Core::LinAlg::Vector<double>> scalar,
        const double thermpress,
        const Teuchos::RCP<const Core::LinAlg::Vector<double>> dirichtoggle, const int ndsvel);


    /*!
    \brief Output of averaged velocity vector for paraview IO

    \param outvec  (in/out) vector in dofrowmap-format to use for
                            output of averaged solution


    */
    /*
    void OutputofAveragedVel(
      Teuchos::RCP<Core::LinAlg::Vector<double>> outvec
      )
      {
        // loop all elements on this proc (including ghosted ones)
        for (int n=0;n<discret_->NumMyRowNodes();++n)
        {
          // get the node
          Core::Nodes::Node* node = discret_->lRowNode(n);

          std::vector<int> dofs= discret_->Dof(node);

          for(int d=0;d<discret_->NumDof(node)-1;++d)
          {
            double val=(*((*filtered_vel_)(d)))[n];

            int    id =dofs[d];

            (outvec)->ReplaceGlobalValues(1,&val,&id);
          }
        }
        return;
      }


    void outputof_fine_scale_vel(
      Teuchos::RCP<Core::LinAlg::Vector<double>> outvec
      )
      {
        // loop all elements on this proc (including ghosted ones)
        for (int n=0;n<discret_->NumMyRowNodes();++n)
        {
          // get the node
          Core::Nodes::Node* node = discret_->lRowNode(n);

          std::vector<int> dofs= discret_->Dof(node);

          for(int d=0;d<discret_->NumDof(node)-1;++d)
          {
            double val=(*((*fs_vel_)(d)))[n];

            int    id =dofs[d];

            (outvec)->ReplaceGlobalValues(1,&val,&id);
          }
        }
        return;
      }


    void FilteredVelComp(
      Teuchos::RCP<Core::LinAlg::Vector<double>> outvec,
      const int                   i,
      const int                   j
      )
      {
        // loop all elements on this proc (including ghosted ones)
        for (int nid=0;nid<discret_->NumMyRowNodes();++nid)
        {
          // get the node
          Core::Nodes::Node* node = discret_->lRowNode(nid);
          int id = node->Id();

          double vali=(*((*filtered_vel_)(i-1)))[nid];
          double valj=(*((*filtered_vel_)(j-1)))[nid];
          double val = vali*valj;

          (outvec)->ReplaceGlobalValues(1,&val,&id);
        }
        return;
      }
  */
    /*!
    \brief Output of averaged reynoldsstress vector for paraview IO (one component)

    \param outvec  (in/out) vector in dofrowmap-format to use for
                            output of averaged solution
    \param i       which component?
    \param j       which component?


    */
    /*
    void FilteredReyStrComp(
      Teuchos::RCP<Core::LinAlg::Vector<double>> outvec,
      const int                   i,
      const int                   j
      )
      {
        // loop all elements on this proc (including ghosted ones)
        int ij = 3*(i-1)+(j-1);
        for (int nid=0;nid<discret_->NumMyRowNodes();++nid)
        {
          // get the node
          Core::Nodes::Node* node = discret_->lRowNode(nid);
          int id = node->Id();

          double val = (*((*filtered_reynoldsstress_) (ij)))[nid];

          (outvec)->ReplaceGlobalValues(1,&val,&id);
        }
        return;
      }

  */
    void get_filtered_velocity(Core::LinAlg::MultiVector<double>& velocity)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          double val = ((*col_filtered_vel_)(i))[nid];
          int err = ((velocity)(i)).ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }



    void get_filtered_dens_velocity(Core::LinAlg::MultiVector<double>& densvelocity)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          double val = ((*col_filtered_dens_vel_)(i))[nid];
          int err = ((densvelocity)(i)).ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_filtered_dens_velocity_temp(Core::LinAlg::MultiVector<double>& densvelocity)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          double val = ((*col_filtered_dens_vel_temp_)(i))[nid];
          int err = ((densvelocity)(i)).ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_filtered_dens_rateofstrain_temp(Core::LinAlg::MultiVector<double>& densvelocity)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          double val = ((*col_filtered_dens_rateofstrain_temp_)(i))[nid];
          int err = ((densvelocity)(i)).ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_filtered_reynolds_stress(Core::LinAlg::MultiVector<double>& reystr)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            const int ij = 3 * i + j;
            double val = ((*col_filtered_reynoldsstress_)(ij))[nid];
            int err = ((reystr)(ij)).ReplaceMyValues(1, &val, &nid);
            if (err != 0) FOUR_C_THROW("dof not on proc");
          }
        }
      }
      return;
    }

    void get_filtered_modeled_subgrid_stress(Core::LinAlg::MultiVector<double>& mosubstr)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            const int ij = 3 * i + j;
            double val = ((*col_filtered_modeled_subgrid_stress_)(ij))[nid];
            int err = ((mosubstr)(ij)).ReplaceMyValues(1, &val, &nid);
            if (err != 0) FOUR_C_THROW("dof not on proc");
          }
        }
      }
      return;
    }



    void get_filtered_vreman_strainrate(Core::LinAlg::MultiVector<double>& strainr)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            const int ij = 3 * i + j;
            double val = ((*col_filtered_strainrate_)(ij))[nid];
            int err = ((strainr)(ij)).ReplaceMyValues(1, &val, &nid);
            if (err != 0) FOUR_C_THROW("dof not on proc");
          }
        }
      }
      return;
    }

    void get_filtered_vreman_alphaij(Core::LinAlg::MultiVector<double>& alphij)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            const int ij = 3 * i + j;
            double val = ((*col_filtered_alphaij_)(ij))[nid];
            int err = ((alphij)(ij)).ReplaceMyValues(1, &val, &nid);
            if (err != 0) FOUR_C_THROW("dof not on proc");
          }
        }
      }
      return;
    }
    void get_filtered_vreman_alphaijsc(Core::LinAlg::MultiVector<double>& alphijsc)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          for (int j = 0; j < 3; ++j)
          {
            const int ij = 3 * i + j;
            double val = ((*col_filtered_alphaijsc_)(ij))[nid];
            int err = ((alphijsc)(ij)).ReplaceMyValues(1, &val, &nid);
            if (err != 0) FOUR_C_THROW("dof not on proc");
          }
        }
      }
      return;
    }


    void get_expression(Core::LinAlg::Vector<double>& expr)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        double val = (*col_filtered_expression_)[nid];
        int err = expr.ReplaceMyValues(1, &val, &nid);
        if (err != 0) FOUR_C_THROW("dof not on proc");
      }
      return;
    }


    void get_density(Core::LinAlg::Vector<double>& dens)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_dens_)[nid];
          int err = dens.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_temp(Core::LinAlg::Vector<double>& dens)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_temp_)[nid];
          int err = dens.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_dens_temp(Core::LinAlg::Vector<double>& dens)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_dens_temp_)[nid];
          int err = dens.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }


    void get_density_strainrate(Core::LinAlg::Vector<double>& densstrainr)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_dens_strainrate_)[nid];
          int err = densstrainr.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_alpha2(Core::LinAlg::Vector<double>& alph2)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_alpha2_)[nid];
          int err = alph2.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }



    void get_fine_scale_velocity(Core::LinAlg::MultiVector<double>& velocity)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          double val = ((*col_fs_vel_)(i))[nid];
          int err = ((velocity)(i)).ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }


    void filtered_rey_str_comp(Core::LinAlg::Vector<double>& outvec, const int i, const int j)
    {
      // loop all elements on this proc (including ghosted ones)
      int ij = 3 * (i - 1) + (j - 1);
      for (int nid = 0; nid < discret_->num_my_row_nodes(); ++nid)
      {
        // get the node
        Core::Nodes::Node* node = discret_->l_row_node(nid);
        int id = node->id();

        double val = ((*filtered_reynoldsstress_)(ij))[nid];

        (outvec).ReplaceGlobalValues(1, &val, &id);
      }
      return;
    }

    void filtered_vel_comp(Core::LinAlg::Vector<double>& outvec, const int i, const int j)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_row_nodes(); ++nid)
      {
        // get the node
        Core::Nodes::Node* node = discret_->l_row_node(nid);
        int id = node->id();

        double vali = ((*filtered_vel_)(i - 1))[nid];
        double valj = ((*filtered_vel_)(j - 1))[nid];
        double val = vali * valj;

        (outvec).ReplaceGlobalValues(1, &val, &id);
      }
      return;
    }

    void outputof_fine_scale_vel(Core::LinAlg::Vector<double>& outvec)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int n = 0; n < discret_->num_my_row_nodes(); ++n)
      {
        // get the node
        Core::Nodes::Node* node = discret_->l_row_node(n);

        std::vector<int> dofs = discret_->dof(node);

        for (int d = 0; d < discret_->num_dof(node) - 1; ++d)
        {
          double val = ((*fs_vel_)(d))[n];

          int id = dofs[d];

          (outvec).ReplaceGlobalValues(1, &val, &id);
        }
      }
      return;
    }

    void outputof_averaged_vel(Core::LinAlg::Vector<double>& outvec)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int n = 0; n < discret_->num_my_row_nodes(); ++n)
      {
        // get the node
        Core::Nodes::Node* node = discret_->l_row_node(n);

        std::vector<int> dofs = discret_->dof(node);

        for (int d = 0; d < discret_->num_dof(node) - 1; ++d)
        {
          double val = ((*filtered_vel_)(d))[n];

          int id = dofs[d];

          (outvec).ReplaceGlobalValues(1, &val, &id);
        }
      }
      return;
    }

    void get_filtered_phi(Core::LinAlg::MultiVector<double>& phi)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        for (int i = 0; i < 3; ++i)
        {
          double val = ((*col_filtered_phi_)(i))[nid];
          int err = ((phi)(i)).ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_filtered_phi2(Core::LinAlg::Vector<double>& phi2)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_phi2_)[nid];
          int err = phi2.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }

    void get_filtered_phiexpression(Core::LinAlg::Vector<double>& phiexpression)
    {
      // loop all elements on this proc (including ghosted ones)
      for (int nid = 0; nid < discret_->num_my_col_nodes(); ++nid)
      {
        {
          double val = (*col_filtered_phiexpression_)[nid];
          int err = phiexpression.ReplaceMyValues(1, &val, &nid);
          if (err != 0) FOUR_C_THROW("dof not on proc");
        }
      }
      return;
    }


   private:
    /*!
    \brief perform box filtering in five steps

    1) Integrate element Heaviside functions against the quantities
       which are filtered. Add the result to the nodevectors
       (we get a contribution for every node of the element)
       This is an element call!
    2) send/add values from slaves to masters
    3) zero out dirichlet nodes
    4) do normalization by division by the patchvolume
       (Heaviside function -> box filter function)
    5) Communication part: Export filtered quantities from row to
       column map

       \param velocity     (i) the velocity defining the
                               unfiltered quantities
       \param dirichtoggle (i) specifying which nodes have to be
                               set to zero

    */



    void apply_box_filter(const Teuchos::RCP<const Core::LinAlg::Vector<double>> velocity,
        const Teuchos::RCP<const Core::LinAlg::Vector<double>> scalar, const double thermpress,
        const Core::LinAlg::Vector<double>& dirichtoggle);

    void apply_box_filter_scatra(const Teuchos::RCP<const Core::LinAlg::Vector<double>> scalar,
        const double thermpress, const Core::LinAlg::Vector<double>& dirichtoggle,
        const int ndsvel);



    //! @name input arguments of the constructor
    //
    //! the discretization
    Teuchos::RCP<Core::FE::Discretization> discret_;
    //! parameterlist including time params, stabilization params and turbulence sublist
    Teuchos::ParameterList& params_;
    //! flag for physical type of fluid flow
    Inpar::FLUID::PhysicalType physicaltype_;
    //@}

    //! @name control parameters
    bool apply_dynamic_smagorinsky_;
    bool vreman_dynamic_;
    bool apply_box_filter_;
    bool dynsmag_loma_on_;
    bool loma_;
    bool incomp_;
    // bool for field values
    bool velocity_;
    bool reynoldsstress_;
    bool modeled_subgrid_stress_;
    bool expression_;
    bool strainrate_;
    bool alphaij_;
    bool alpha2_;
    bool finescale_velocity_;
    bool densvelocity_;
    bool densstrainrate_;
    bool density_;
    bool phi_;
    bool phi2_;
    bool phiexpression_;
    bool alphaijsc_;

    //@}

    //! @name special scatra variables
    //! the discretization
    Teuchos::RCP<Core::FE::Discretization> scatradiscret_;
    //@}

    //! @name vectors used for filtering (for dynamic Smagorinsky model)
    //        --------------------------

    //! the box filtered velocities in nodes (3 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_vel_;
    //! the box filtered reynoldsstresses in nodes (9 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_reynoldsstress_;
    //! the modeled subgrid stress in nodes (9 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_modeled_subgrid_stress_;
    //! the filtered velocities times rho (3 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_dens_vel_;
    //! the filtered density (vector)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_dens_;
    //! the filtered strainrate times rho (vector)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_dens_strainrate_;
    //! the modeled fine scale velocities in nodes (3 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> fs_vel_;
    //! Vreman model: strain rate in nodes (9 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_strainrate_;
    //! Vreman model: expression (Vector)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_expression_;
    //! Vreman model: alphaij in nodes (9 vectors)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_alphaij_;
    //! Vreman model: alpha2 (Vector)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_alpha2_;
    //! the filtered density times temperature times velocity (scalar)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_dens_vel_temp_;
    //! the filtered density times temperature gradient times rate of strain (scalar)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_dens_rateofstrain_temp_;
    //  //! the filtered temperature gradient (scalar)
    //  Teuchos::RCP<Core::LinAlg::MultiVector<double>>      filtered_gradtemp_;
    //! the filtered temperature (scalar)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_temp_;
    //! the filtered density times temperature (scalar)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_dens_temp_;
    //! filtered phi: gradient
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_phi_;
    //! filtered grad(phi_j)*grad(phi_j)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_phi2_;
    //! filtered visc*grad(phi_j)*grad(phi_j)
    Teuchos::RCP<Core::LinAlg::Vector<double>> filtered_phiexpression_;
    //! alphaij tensor for scatra
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> filtered_alphaijsc_;
    //! the filtered vel exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_vel_;
    //! the filtered reystress exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_reynoldsstress_;
    //! the modeled subgrid stresses exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_modeled_subgrid_stress_;
    //! the filtered velocities times rho exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_dens_vel_;
    //! the filtered density exported to column map
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_dens_;
    //! the filtered strainrate times rho exported to column map
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_dens_strainrate_;
    //! the modeled fine scale velocities exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_fs_vel_;
    //! Vreman model: strain rate exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_strainrate_;
    //! Vreman model: expression (Vector) exported to column map
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_expression_;
    //! Vreman model: alphaij in nodes (9 vectors) exported to column map
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_alphaij_;
    //! Vreman model: alpha2 (Vector) exported to column map
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_alpha2_;
    //! the filtered density times temperature times velocity exported to column map (scalar)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_dens_vel_temp_;
    //! the filtered density times temperature gradient times rate of strain exported to column map
    //! (scalar)
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_dens_rateofstrain_temp_;
    //  //! the filtered temperature gradient exported to column map (scalar)
    //  Teuchos::RCP<Core::LinAlg::MultiVector<double>>      col_filtered_gradtemp_;
    //! the filtered temperature exported to column map (scalar)
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_temp_;
    //! the filtered density times temperature exported to column map (scalar)
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_dens_temp_;
    //! filtered phi: gradient
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_phi_;
    //! filtered grad(phi_j)*grad(phi_j)
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_phi2_;
    //! filtered visc*grad(phi_j)*grad(phi_j)
    Teuchos::RCP<Core::LinAlg::Vector<double>> col_filtered_phiexpression_;
    //! alphaij tensor for scatra
    Teuchos::RCP<Core::LinAlg::MultiVector<double>> col_filtered_alphaijsc_;
    //@}

    //! @name homogeneous flow specials
    //        -------------------------------

    //! the direction coordinates for the above mentioned averaging procedure
    Teuchos::RCP<std::vector<double>> dir1coords_;
    Teuchos::RCP<std::vector<double>> dir2coords_;
    //@}

  };  // end class Boxfilter

}  // end namespace FLD

FOUR_C_NAMESPACE_CLOSE

#endif
