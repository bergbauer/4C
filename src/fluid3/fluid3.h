/*!---------------------------------------------------------------------
\file
\brief The 3D fluid element

<pre>
Maintainer: Steffen Genkinger
            genkinger@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/genkinger/
            0711 - 685-6127
</pre>

------------------------------------------------------------------------*/
/*!---------------------------------------------------------------------
\brief fluid2                                                 

<pre>                                                         genk 03/02 

In this structure all variables used for element evaluation by the 3D
fluid element fluid2 are stored.

</pre>

--------------------------------------------------------------------------*/
#ifdef D_FLUID3
typedef struct _FLUID3
{

INT                ntyp;     /*!< flag for element type: 1=quad; 2=tri    */
INT                nGP[3];   /*!< number of gaussian points in rs direct. */
INT                is_ale;   /*!< flag whether there is ale to me or not  */
struct _ELEMENT   *my_ale;   /*!< pointer to my ale ele, otherwise NULL   */

/*---------------------------------------------------- stabilisation ---*/
enum _STABILISATION_TYP	stab_type;	/* enum of stabilisation	*/
union 
{
  struct _STAB_PAR_GLS  *gls;	/*! pointer to stabilisation parameters	*/
/*  struct _STAB_PRES_PRO *pp; */
} stabi;

/*--------------------------------- element sizes for stability parameter */
DOUBLE             hk[3];    /*!< vel/pres/cont                           */

/*------------------------------------------------ free surface parameter */
INT                fs_on;    /*! element belongs to free surface          */

/*-------------------------------------------------------- stress results */
struct _ARRAY      stress_ND; /*!< nodal stresses                         */

/*------------------------------------------------- structure for submesh */
INT                smisal;        /* flag for element submesh creation    */
DOUBLE             smcml;	  /* charact. mesh length for submesh 	  */
struct _ARRAY      xyzsm;         /* coordinates of submesh nodes         */
struct _ARRAY      solsm;         /* sol. of current and last timestep    */
struct _ARRAY      solsmn;        /* sol. of last timestep                */
        
/*--------------------------------------------- structure for sub-submesh */
struct _ARRAY      xyzssm;        /* coordinates of sub-submeshnodes      */
} FLUID3;

#endif
