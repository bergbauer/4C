#include "line_integration.H"
#include "boundarycell_integration.H"

#include "cut_boundarycell.H"


//compute the equation of the plane Ax+By+Cz=D with the local coordinates of corner points
std::vector<double> GEO::CUT::FacetIntegration::equation_plane(const std::vector<std::vector<double> > cornersLocal)
{
  std::vector<double> eqn_plane(4);

  double x1[3]={0.0,0.0,0.0},y1[3]={0.0,0.0,0.0},z1[3]={0.0,0.0,0.0};
  unsigned mm=0;
  for(std::vector<std::vector<double> >::const_iterator k=cornersLocal.begin();k!=cornersLocal.end();k++)
  {
    const std::vector<double> coords = *k;

    x1[mm] = coords[0];
    y1[mm] = coords[1];
    z1[mm] = coords[2];

//make sure all three points do not lie on a line
//construct a vector pt2-pt1 and pt3-pt1. If the cross product is zero then all are in same line
//go to next point in such cases
    if(mm==2)
    {
      double pt1pt2[3],pt1pt3[3],cross[3];
      pt1pt2[0] = x1[1]-x1[0];pt1pt3[0]=x1[2]-x1[0];
      pt1pt2[1] = y1[1]-y1[0];pt1pt3[1]=y1[2]-y1[0];
      pt1pt2[2] = z1[1]-z1[0];pt1pt3[2]=z1[2]-z1[0];
      cross[0] = fabs(pt1pt2[1]*pt1pt3[2]-pt1pt2[2]*pt1pt3[1]);
      cross[1] = fabs(pt1pt2[0]*pt1pt3[2]-pt1pt2[2]*pt1pt3[0]);
      cross[2] = fabs(pt1pt2[1]*pt1pt3[0]-pt1pt2[0]*pt1pt3[1]);
        if(cross[0]<0.000001 && cross[1]<0.000001 && cross[2]<0.000001)
          continue;
        else
          mm++;
      }
      else
        mm++;
      //3 points are sufficient to find the equation of plane
      if(mm==3)
        break;
      if(mm==cornersLocal.size())
        dserror("All points of a facet are on a line");
  }
  eqn_plane[0] = y1[0]*(z1[1]-z1[2])+y1[1]*(z1[2]-z1[0])+y1[2]*(z1[0]-z1[1]);
  eqn_plane[1] = z1[0]*(x1[1]-x1[2])+z1[1]*(x1[2]-x1[0])+z1[2]*(x1[0]-x1[1]);
  eqn_plane[2] = x1[0]*(y1[1]-y1[2])+x1[1]*(y1[2]-y1[0])+x1[2]*(y1[0]-y1[1]);
  eqn_plane[3] = x1[0]*(y1[1]*z1[2]-y1[2]*z1[1])+x1[1]*(y1[2]*z1[0]-y1[0]*z1[2])+x1[2]*(y1[0]*z1[1]-y1[1]*z1[0]);

  return eqn_plane;
}

//compute only the x-component of unit-normal vector which is used in further computations
//also determine whether the plane is numbered in clockwise or anticlockwise sense when seen away from the face
void GEO::CUT::FacetIntegration::IsClockwise( const std::vector<double> eqn_plane,
                                              const std::vector<std::vector<double> > cornersLocal )
{
#if 0 //new generalized method
  clockwise_ = 0;
  Side* parent = face1_->ParentSide();
  const std::vector<Node*> &par_nodes = parent->Nodes();
  std::vector<vector<double> > corners(par_nodes.size());
  int mm=0;
  //std::cout<<"parent side\t"<<"element id = "<<elem1_->Id()<<"\n";
  for(std::vector<Node*>::const_iterator i=par_nodes.begin();i!=par_nodes.end();i++)
  {
    Node *nod = *i;
    double x1[3];
    nod->Coordinates(x1);
    LINALG::Matrix<3,1> glo,loc;
    std::vector<double> pt_local(3);
    for(int nodno=0;nodno<3;nodno++)
      glo(nodno,0) = x1[nodno];

     elem1_->LocalCoordinates(glo,loc);

     pt_local[0] = loc(0,0);
     pt_local[1] = loc(1,0);
     pt_local[2] = loc(2,0);

    //std::cout<<loc(0,0)<<"\t"<<loc(1,0)<<"\t"<<loc(2,0)<<"\n";

     corners[mm] = pt_local;
     mm++;
    }
//        std::cout<<"\n";
    std::vector<double> eqn_par = equation_plane(corners);

    bool iscut = face1_->OnCutSide();
    if(iscut)
    {
      if(position_==-2)
      {
        if(eqn_plane[0]*eqn_par[0]<0.0)
          clockwise_ = 1;
      }
      else
      {
        if(eqn_plane[0]*eqn_par[0]>0.0)
          clockwise_ = 1;
      }
    }
    else
    {
      if(eqn_plane[0]*eqn_par[0]<0.0)
        clockwise_ = 1;
    }
    //std::cout<<"clockwise = "<<clockwise_<<"\t"<<"is cut side = "<<iscut<<"\n";
#endif

#if 1 //old method of checking the ordering - works only for hexagonal elements
  std::string ordering;

  if(cornersLocal.size()==3 || cornersLocal.size()==4)
  {
    if(eqn_plane[0]>0.0)
      ordering = "acw";
    else
      ordering = "cw";
  }
#if 1
  else
  {
    double crossProd = 0.0;
    for(unsigned i=0;i<cornersLocal.size();i++)
    {
      unsigned j = (i+1)%cornersLocal.size();
      crossProd += (cornersLocal[j][1]-cornersLocal[i][1])*(cornersLocal[j][2]+cornersLocal[i][2]);
    }
    if(crossProd>0)
      ordering = "cw";
    else if(crossProd<0)
      ordering = "acw";
    else
      dserror("the points in the facet are neither ordered in clockwise not anti-clockwise or all collinear"
          " point in this facet");
  }
#else
  else
  {
    double crossProd = 0.0;
    int count=0;
    for(unsigned i=0;i<cornersLocal.size();i++)
    {
      unsigned j = (i+1)%cornersLocal.size();
      unsigned k = (i+2)%cornersLocal.size();
      crossProd = (cornersLocal[j][1]-cornersLocal[i][1])*(cornersLocal[k][2]-cornersLocal[j][2]);
      crossProd -= (cornersLocal[j][2]-cornersLocal[i][2])*(cornersLocal[k][1]-cornersLocal[j][1]);
      std::cout<<"cross = "<<crossProd<<"\n";
      if(crossProd<0)
        count--;
      else if(crossProd>0)
        count++;

    }
    if(count>0)
      ordering = "acw";
    else if(count<0)
      ordering = "cw";
    else
      dserror("the points in the facet are neither ordered in clockwise not anti-clockwise or all collinear"
          " point in this facet");
  }
#endif

  clockwise_ = 0;
  Side* parent = face1_->ParentSide();
  const std::vector<Node*> &par_nodes = parent->Nodes();
  std::vector<vector<double> > corners(par_nodes.size());
  int mm=0;

  for(std::vector<Node*>::const_iterator i=par_nodes.begin();i!=par_nodes.end();i++)
  {
    Node *nod = *i;
    double x1[3];
    nod->Coordinates(x1);
    LINALG::Matrix<3,1> glo,loc;
    std::vector<double> pt_local(3);
    for(int nodno=0;nodno<3;nodno++)
      glo(nodno,0) = x1[nodno];

     elem1_->LocalCoordinates(glo,loc);

     pt_local[0] = loc(0,0);
     pt_local[1] = loc(1,0);
     pt_local[2] = loc(2,0);

     corners[mm] = pt_local;
     mm++;
    }

    std::vector<double> eqn_par = equation_plane(corners);

    bool iscut = face1_->OnCutSide();
    if(iscut)
    {
			if(position_==-2)
			{
        if(eqn_plane[0]*eqn_par[0]<0.0)
          clockwise_ = 1;
			}
			else
			{
        if(eqn_plane[0]*eqn_par[0]>0.0)
          clockwise_ = 1;
			}
    }
    else
    {
			const std::vector<Side*> &ele_sides = elem1_->Sides();

			int parentSideno = 0;
			for(std::vector<Side*>::const_iterator i=ele_sides.begin();i!=ele_sides.end();i++)
			{
        Side*sss = *i;
        if(sss==parent)
        {
          break;
        }
        parentSideno++;

			}

//should check whether this is sufficient or do we need to find the number of side in the element and
//use their orientation to get clockwise ordering
//                std::cout<<"parent side no = "<<parentSideno<<"\n";
//			std::cout<<"parentSideno = "<<parentSideno<<"corner = "<<corners[0][0]<<"\n";
//ParentSideno=1 is x=1 face and 3 is x=-1 face
#if 0
			if(parentSideno==1 && eqn_plane_[0]<0.0)
				clockwise_ = 1;
			if(parentSideno==3 && eqn_plane_[0]>0.0)
				clockwise_ = 1;
#endif
    if(parentSideno==1 && ordering=="cw")
      clockwise_ = 1;
    if(parentSideno==3 && ordering=="acw")
      clockwise_ = 1;

			/*if(corners[0][0]==1 && eqn_plane_[0]<0.0)
				clockwise_ = 1;
			if(corners[0][0]==-1 && eqn_plane_[0]>0.0)
				clockwise_ = 1;*/

  }

#endif

}

//computes x=a1+a2y+a3z from the plane equation
//equation of this form is used to replace x in the line integral
std::vector<double> GEO::CUT::FacetIntegration::compute_alpha( std::vector<double> eqn_plane,
                                                               std::string intType )
{
  std::vector<double> alfa(3);
  double a = eqn_plane[0];
  double b = eqn_plane[1];
  double c = eqn_plane[2];
  double d = eqn_plane[3];


  if(intType=="x")
  {
    alfa[0] = d/a;
    alfa[1] = -1.0*b/a;
    alfa[2] = -1.0*c/a;
  }
  else if(intType=="y")
  {
        alfa[0] = d/b;
    alfa[1] = -1.0*c/b;
    alfa[2] = -1.0*a/b;
  }
  else if(intType=="z")
  {
    alfa[0] = d/c;
    alfa[1] = -1.0*a/c;
    alfa[2] = -1.0*b/c;
  }
  else
  {
    dserror("The facet integration type undefined");
    exit(1);
  }
  return alfa;
}

//return the absolute normal of the facet in a particular direction
double GEO::CUT::FacetIntegration::getNormal(std::string intType)
{
  double normalScale=0.0;
  for(unsigned i=0;i<3;i++)
    normalScale += eqn_plane_[i]*eqn_plane_[i];
  normalScale = sqrt(normalScale);

  if(intType=="x")
    return eqn_plane_[0]/normalScale;
  else if(intType=="y")
    return eqn_plane_[1]/normalScale;
  else if(intType=="z")
    return eqn_plane_[2]/normalScale;
  else
  {
    dserror("The normal direction is unspecified");
    return 0.0;
  }
}

//perform integration over the facet
double GEO::CUT::FacetIntegration::integrate_facet()
{
		std::vector<std::vector<double> > cornersLocal = face1_->CornerPointsLocal(elem1_);
		if(global_==true)
		{
		  std::vector<Point*>co =  face1_->CornerPoints();
      for(std::vector<Point*>::iterator i=co.begin();i!=co.end();i++)
      {
        Point* po = *i;
        double xo[3];
        po->Coordinates(xo);
        for(unsigned j=0;j<3;j++)
          cornersLocal[i-co.begin()][j] = xo[j];
      }
		}

    eqn_plane_ = equation_plane(cornersLocal);

// the face is in the x-y or in y-z plane which gives zero facet integral
    if(fabs(eqn_plane_[0])<0.0000001 && bcellInt_==false)
      return 0.0;
//x=0 plane which also do not contribute to facet integral
    if(fabs(eqn_plane_[1])<0.0000001 && fabs(eqn_plane_[2])<0.0000001 && fabs(eqn_plane_[3])<0.0000001 && bcellInt_==false)
      return 0.0;

    if(bcellInt_==true) // the integral value of boundarycell will not change w.r.t the ordering of vertices
      clockwise_ = false;
    else
      IsClockwise(eqn_plane_,cornersLocal);

    //integrating over each line of the facet
    double facet_integ = 0.0;
    if(bcellInt_==false)
    {
      std::vector<double> alpha;
      alpha = compute_alpha(eqn_plane_,"x");
			for(std::vector<std::vector<double> >::const_iterator k=cornersLocal.begin();k!=cornersLocal.end();k++)
			{
				const std::vector<double> coords1 = *k;
				std::vector<double> coords2;
				//for the last line the end point is the first point of the facet
				if(k!=(cornersLocal.end()-1))
						coords2 = *(k+1);
				else
						coords2= *(cornersLocal.begin());

  //first index decides the x or y coordinate, second index decides the start point or end point
				LINALG::Matrix<2,2> coordLine;

	//The facet is projected over y-z plane and then the integration is performed
	//so only y- and z-coordinates are passed to make the lines
	//[0]-- indicates y and [1] indicate z because we are now in y-z plane
				coordLine(0,0) = coords1[1];
				coordLine(1,0) = coords1[2];
				coordLine(0,1) = coords2[1];
				coordLine(1,1) = coords2[2];

				LineIntegration line1(coordLine,inte_num_,alpha,bcellInt_);
				facet_integ += line1.integrate_line();
			}
    }
    else
    {
//to reduce the truncation error introduced during the projection of plane,
//the plane, along which the normal component is maximum, is chosen
      std::string projType;
			if(fabs(eqn_plane_[0])<1e-8)
			{
				if(fabs(eqn_plane_[1])<1e-8)
					projType = "z";
				else if(fabs(eqn_plane_[2])<1e-8)
					projType = "y";
				else
				{
					if(fabs(eqn_plane_[1])>fabs(eqn_plane_[2]))
						projType = "y";
					else
						projType = "z";
				}
			}
			else if(fabs(eqn_plane_[1])<1e-8)
			{
				if(fabs(eqn_plane_[2])<1e-8)
					projType = "x";
				else
				{
					if(fabs(eqn_plane_[0])>fabs(eqn_plane_[2]))
						projType = "x";
					else
						projType = "z";
				}
			}
			else if(fabs(eqn_plane_[2])<1e-8)
			{
				if(fabs(eqn_plane_[1])>fabs(eqn_plane_[0]))
					projType = "y";
				else
					projType = "x";
			}
			else
			{
				if(fabs(eqn_plane_[0])>=fabs(eqn_plane_[1]) && fabs(eqn_plane_[0])>=fabs(eqn_plane_[2]))
					projType = "x";
				else if(fabs(eqn_plane_[1])>=fabs(eqn_plane_[2]) && fabs(eqn_plane_[1])>=fabs(eqn_plane_[0]))
					projType = "y";
				else
					projType = "z";
			}

			if(projType!="x" && projType!="y" && projType!="z")
			{
				dserror("projection plane is not defined");
				exit(1);
			}

			BoundaryFacetIntegration(cornersLocal,facet_integ,projType);

    }

//this condition results in negative normal for all the lines in the line integral
    if(clockwise_ && bcellInt_==false)
    {
      facet_integ = -1.0*facet_integ;
      for(int i=0;i<4;i++)
        eqn_plane_[i] = -1.0*eqn_plane_[i];
    }

    return facet_integ;
}

//Performs integration over the boundarycell
void GEO::CUT::FacetIntegration::BoundaryFacetIntegration( const std::vector<std::vector<double> > cornersLocal,
                                                           double &facet_integ,
                                                           std::string intType )
{
	std::vector<double> alpha;
	double abs_normal=0.0;

	for(std::vector<std::vector<double> >::const_iterator k=cornersLocal.begin();k!=cornersLocal.end();k++)
	{
		const std::vector<double> coords1 = *k;
		std::vector<double> coords2;

		//for the last line the end point is the first point of the facet
		if(k!=(cornersLocal.end()-1))
      coords2 = *(k+1);
		else
      coords2= *(cornersLocal.begin());

		//first index decides the x or y coordinate, second index decides the start point or end point
		LINALG::Matrix<2,2> coordLine;
		if(intType=="x")
		{
			if(k==cornersLocal.begin())
			{
				alpha = compute_alpha(eqn_plane_,"x");
				abs_normal = getNormal(intType);
			}
	//The facet is projected over y-z plane and then the integration is performed
	//so only y- and z-coordinates are passed to make the lines
	//(0,i)-- indicates y and (1,i) indicate z because we are now in y-z plane
      coordLine(0,0) = coords1[1];
      coordLine(1,0) = coords1[2];
      coordLine(0,1) = coords2[1];
      coordLine(1,1) = coords2[2];

			LineIntegration line1(coordLine,inte_num_,alpha,bcellInt_);
			line1.set_integ_type("x");
			facet_integ += line1.integrate_line();
		}
		else if(intType=="y")
		{
			if(k==cornersLocal.begin())
			{
				alpha = compute_alpha(eqn_plane_,"y");
				abs_normal = getNormal(intType);
			}
	//The facet is projected over y-z plane and then the integration is performed
	//so only y- and z-coordinates are passed to make the lines
	//(0,i)-- indicates y and (0,i) indicate z because we are now in y-z plane
      coordLine(0,0) = coords1[2];
      coordLine(1,0) = coords1[0];
      coordLine(0,1) = coords2[2];
      coordLine(1,1) = coords2[0];
			LineIntegration line1(coordLine,inte_num_,alpha,bcellInt_);
			line1.set_integ_type("y");
			facet_integ += line1.integrate_line();
		}

		else if(intType=="z")
		{
			if(k==cornersLocal.begin())
			{
				alpha = compute_alpha(eqn_plane_,"z");
				abs_normal = getNormal(intType);
			}
	//The facet is projected over y-z plane and then the integration is performed
	//so only y- and z-coordinates are passed to make the lines
	//(0,i)-- indicates y and (0,i) indicate z because we are now in y-z plane
      coordLine(0,0) = coords1[0];
      coordLine(1,0) = coords1[1];
      coordLine(0,1) = coords2[0];
      coordLine(1,1) = coords2[1];
			LineIntegration line1(coordLine,inte_num_,alpha,bcellInt_);
			line1.set_integ_type("z");
			facet_integ += line1.integrate_line();
		}
		else
		{
			dserror("The facet integration type not supported");
			exit(1);
		}
	}

	facet_integ = facet_integ/abs_normal;
}

//Generate integration rule for the facet if the divergence theorem is used
//directly to generate Gauss integration rule for the facet
void GEO::CUT::FacetIntegration::DivergenceIntegrationRule(Mesh &mesh)
{
  plain_boundarycell_set divCells;

  //the last two parameter has no influence when called from the first parameter is set to true
  GenerateIntegrationRuleDivergence(true, mesh, divCells);

  double normalX = getNormal("x");//make sure eqn of plane is available before calling this
  if(clockwise_) //because if ordering is clockwise the contribution of this facet must be subtracted
    normalX = -1.0*normalX;

  //std::cout<<"size of the divergenceCells = "<<divCells.size()<<"\n";
  Teuchos::RCP<DRT::UTILS::CollectedGaussPoints> cgp = Teuchos::rcp( new DRT::UTILS::CollectedGaussPoints(0) );

  //DRT::UTILS::GaussIntegration gi_temp;
  for(plain_boundarycell_set::iterator i=divCells.begin();i!=divCells.end();i++)
  {
    BoundaryCell* bcell = *i;
    DRT::UTILS::GaussIntegration gi_temp = DRT::UTILS::GaussIntegration( bcell->Shape(), 9 );

    for ( DRT::UTILS::GaussIntegration::iterator iquad=gi_temp.begin(); iquad!=gi_temp.end(); ++iquad )
    {
      double drs = 0.0; // transformation factor between reference cell and linearized boundary cell
      LINALG::Matrix<3,1> x_gp_glo(true), x_gp_loc(true), normal(true); // gp in xyz-system on linearized interface
      const LINALG::Matrix<2,1> eta( iquad.Point() ); // eta-coordinates with respect to cell

      //get normal vector on linearized boundary cell, x-coordinates of gaussian point and surface transformation factor
      switch ( bcell->Shape() )
      {
        case DRT::Element::tri3:
        {
          bcell->Transform<DRT::Element::tri3>(eta, x_gp_glo, normal, drs);
          break;
        }
        case DRT::Element::quad4:
        {
          bcell->Transform<DRT::Element::quad4>(eta, x_gp_glo, normal, drs);
          break;
        }
        default:
          throw std::runtime_error( "unsupported integration cell type" );
      }
      double wei = iquad.Weight()*drs;

      cgp->Append(x_gp_glo,wei);



      elem1_->LocalCoordinates(x_gp_glo,x_gp_loc);

      FacetIntegration bcellLocal(face1_,elem1_,position_,true,false);
      bcellLocal.set_integ_number(1);
      double areaLocal = bcellLocal.integrate_facet();

      FacetIntegration bcellGlobal(face1_,elem1_,position_,true,true);
      bcellGlobal.set_integ_number(1);
      double areaGlobal = bcellGlobal.integrate_facet();
      double jaco = areaLocal/areaGlobal;

      double weiActual = wei*jaco*normalX;

      cgp->Append(x_gp_loc,weiActual);
    }
  }

  DRT::UTILS::GaussIntegration gi (cgp);
  std::cout<<"num pts = "<<gi.NumPoints();
}

//generates integration rule for the considered facet
void GEO::CUT::FacetIntegration::GenerateIntegrationRuleDivergence( bool divergenceRule, //if called to generate direct divergence rule
                                                                    Mesh &mesh,
                                                                    plain_boundarycell_set & divCells)
{
  std::vector<std::vector<double> > cornersLocal = face1_->CornerPointsLocal(elem1_);

  eqn_plane_ = equation_plane(cornersLocal);

// the face is in the x-y or in y-z plane which will not be considered when divergence theorem is applied
  if(divergenceRule)
  {
    if(fabs(eqn_plane_[0])<0.0000001)
      return;
  }

  if(!divergenceRule && !face1_->OnCutSide())
    return;

  IsClockwise(eqn_plane_,cornersLocal);

  std::vector<Point*> corners = face1_->CornerPoints();
  if(clockwise_)
    std::reverse(corners.begin(),corners.end());

  if(divergenceRule)
  {
    if(corners.size()==3)
      TemporaryTri3(corners, divCells);

   /* else if(corners.size()==4)
      TemporaryQuad4(corners, divCells);*/

    else
    {
      if(!face1_->IsTriangulated())
        face1_->DoTriangulation( mesh, corners );
      const std::vector<std::vector<Point*> > & triangulation = face1_->Triangulation();
      for ( std::vector<std::vector<Point*> >::const_iterator j=triangulation.begin();
                        j!=triangulation.end(); ++j )
      {
        std::vector<Point*> tri = *j;
        if(clockwise_)
          std::reverse(tri.begin(),tri.end());
        if(tri.size()==3)
          TemporaryTri3(corners, divCells);
        else if(tri.size()==4)
          TemporaryQuad4(corners, divCells);
        else
          dserror("Triangulation created neither tri3 or quad4");
      }
    }
  }
}

//temporarily create a tri3 cell
void GEO::CUT::FacetIntegration::TemporaryTri3( std::vector<Point*>& corners,
                                                plain_boundarycell_set& divCells )
{
  Epetra_SerialDenseMatrix xyz( 3, 3 );
  for ( int i=0; i<3; ++i )
    corners[i]->Coordinates( &xyz( 0, i ) );
  Tri3BoundaryCell * bc = new Tri3BoundaryCell( xyz, face1_, corners );
  divCells.insert( bc );
}

//temporarily create a quad4 cell
void GEO::CUT::FacetIntegration::TemporaryQuad4( std::vector<Point*>& corners,
                                                 plain_boundarycell_set& divCells )
{
  Epetra_SerialDenseMatrix xyz( 3, 4 );
  for ( int i=0; i<4; ++i )
    corners[i]->Coordinates( &xyz( 0, i ) );
  Quad4BoundaryCell * bc = new Quad4BoundaryCell( xyz, face1_, corners );
  divCells.insert( bc );
}


