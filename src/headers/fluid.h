/*!---------------------------------------------------------------------
\file
\brief fluid dynamic data

------------------------------------------------------------------------*/
/*!----------------------------------------------------------------------
\brief integration parameters                                              

<pre>                                                         genk 03/02  

In this structure the coordinates and weights used by gauss integration
are stored.

</pre>

------------------------------------------------------------------------*/
typedef struct _FLUID_DATA
{
DOUBLE        qxg[MAXQINTP][MAXQINTC];    /*!< coordinates for QUADS and HEX */
DOUBLE        qwgt[MAXQINTP][MAXQINTC];   /*!< weights for QUADS and HEX */

DOUBLE        txgr[MAXTINTP][MAXTINTC];   /*!< coordinates in r for TRIS and TETS */
DOUBLE        txgs[MAXTINTP][MAXTINTC];   /*!< coordinates in s for TRIS and TETS*/
DOUBLE        txgt[MAXTINTP][MAXTINTC];   /*!< coordinates in t for TRIS and TETS*/
DOUBLE        twgt[MAXTINTP][MAXTINTC];   /*!< weights for TRIS and TETS*/
} FLUID_DATA;


/*!----------------------------------------------------------------------
\brief calculation parameter                                              

<pre>                                                         genk 03/02  

In this structure all parameters used during the element evaluation are
stored.

</pre>

------------------------------------------------------------------------*/
typedef struct _FLUID_DYN_CALC                
{
DOUBLE dta;      /*!< actual time increment */
DOUBLE thsl;     /*!< theta-s,l: const. for "stiffness" terms LHS */
DOUBLE thsr;     /*!< theta-s,r: const. for "stiffness" terms RHS */
DOUBLE thpl;     /*!< theta-p,l: const. for "pressure" terms LHS  */
DOUBLE thpr;     /*!< theta-p,r: const. for "pressure" terms RHS  */
DOUBLE thnr;	 /*!< additional part of thsr needed for gen_alpha*/
DOUBLE omt;      /*!< ONE-theta                                   */
DOUBLE alpha;	 /*!< alpha_m of generalised alpha method	  */
DOUBLE acttime;  /*!< actual time */
DOUBLE velmax;   /*!< max. velocity, needed for stabilisaton parameter */
DOUBLE tau[3];   /*!< array for stabilitity parameter */
DOUBLE tau_tu;   /*!< array for stabilitity parameter for turbulence*/
DOUBLE tau_tu_dc;/*!< array for DISCONTINUITY CAPTURING for turbulence*/
DOUBLE sigma;    /*!< const. for nonlinear iteration */   
DOUBLE theta;    /*!< integration parameter */
DOUBLE washvel;  /*!< wall shear velocity */   
DOUBLE totarea;  /*!< total area of fluid field */
DOUBLE coord_scale[2];  /*!<coordinates for scaling the turbulence variables */   
INT    gen_alpha; /*!< general alpha time integration */
INT    iprerhs;  /*!< treatment of pressure in time discr. */
INT    surftens; /*!< include surface tension effects */
INT    fsstnif; 
INT    fsstnii;
INT    nik; 	 /*!< EVALUATION OF LHS-MATRICES (w/o NONLINEAR TERM)*/
INT    nic;	 /*!< EVALUATION OF NONLINEAR LHS N-CONVECTIVE       */
INT    nir;	 /*!< EVALUATION OF NONLINEAR LHS N-REACTION	     */
INT    nie;	 /*!< EVALUATE ONLY LHS-TERMS FOR EXPLICIT VELOCITY  */
INT    nil;	 /*!< EVALUATION OF LUMPED MASS MATRIX (Mvv-lumped)  */
INT    nif;	 /*!< EVALUATION OF "TIME - RHS"           	     */
INT    nii;	 /*!< EVALUATION OF "ITERATION - RHS"		     */
INT    nis;	 /*!< STATIONARY CASE (NO TIMEDEPENDENT TERMS)       */
/*------------------------------------------------ projection method */
INT    pro_calmat;  /*!< a flag that switches matrix calc.           */
INT    pro_calrhs;  /*!< a flag that switches rhs calculation        */
INT    pro_calveln; /*!< a flag that switches calculation of vel at time level n     */
INT    pro_kvv;     /*!< a flag that switches calculation of Kvv     */
INT    pro_mvv;     /*!< a flag that switches calculation of Mvv     */
INT    pro_gra;     /*!< a flag that switches calculation of C       */
INT    pro_lum;     /*!< a flag that switches lumping of Mvv         */
INT    pro_gra_opt; /*!< a flag that switches for grad. calculation  */
INT    pro_profile; /*!< a flag that switches for velocity profile   */
INT    pro_caldirich;
INT    niturbu_pro;  /*!< EVALUATION OF "TIME - RHS" for turbulence-model */
INT    niturbu_n;    /*!< EVALUATION OF "TIME - RHS" for turbulence-model */
INT    kapeps_flag;  /*!< kappa or epsilon equation                       */
INT    kapomega_flag;/*!< kappa or omega equation                         */
INT    kappan;       /*!< kappan for production-term                      */
INT    dis_capt;     /*!< flag for DISCONTINUITY CAPTURING for turbulence model */
INT    ishape;   /*!< flag for new element shape                     */
INT    ncols;        /*!< number of columns in solution history */
struct  _FLUID_DATA data;
} FLUID_DYN_CALC;

/*!----------------------------------------------------------------------
\brief fluid input parameter                                              

<pre>                                                         genk 03/02  

In this structure all fluid-dynamic variables from the input file are
stored. 

</pre>

------------------------------------------------------------------------*/
typedef struct _FLUID_DYNAMIC               
{
INT                dyntyp;       /*!< dynamictype */
INT                numdf;        /*!< number of dofs of the fluid elements */
INT                iop;          /* !<time integration method */
INT                numcont;      /*!< number of continuation steps */
INT                uppss;        /*!< update pss file every n steps */
INT                upout;        /*!< store results every n steps */      
INT                upres;        /*!< store results in .flavia.res every n steps */      
INT                res_write_evry; /*!< write restart every n steps */
INT                nstep;        /*!< number of timesteps */
INT                resstep;      /*!< restart step */
INT                step;         /*!< the actual step */
INT                stepke;       /*!< the actual step for kappa-epsilon*/
INT                ite;          /*!< nonlinear iteration scheme */
INT                itemax;       /*!< number of nonlin. iterations */
INT                itemax_ke;    /*!< number of nonlin. iterations for kappa-eps */
INT                itchk;        /*!< convergence check during nonlin. iteration */
INT                itnorm;       /*!< norm for conv. check d. nonlin. iteration */
INT                stchk;        /*!< steady state check every n steps */
INT                stnorm;       /*!< norm for steady state check */
INT                iops;         /*!< starting algorithm */
INT                nums;         /*!< number of starting algorithm steps */
INT                init;         /*!< initialisation of starting field */
INT                iprerhs;      /*!< treatment of pressure in time discr. */
INT                viscstr;      /*!< flag for calculation of viscos stresses */
INT                freesurf;     /*!< treatment of free surface */
INT                surftens;     /*!< include surface tension effects */
INT                checkarea;    /*!< check total area of fluid field */
INT                turbu;        /*!< the type of turbulence-model */
INT                dis_capt;     /*!< flag for DISCONTINUITY CAPTURING for turbulence model */
DOUBLE             lenght;       /*!< internal lenght of problem */
DOUBLE             rought;       /*!< roughtness of solid boundaries */
DOUBLE      coord_scale[2];      /*!< coordinates for scaling the turbulence variables */   
DOUBLE             maxtime;      /*!< maximal simulation time */
DOUBLE             time;         /*!< actual time */
DOUBLE             dt;           /*!< time increment */
DOUBLE             theta;        /*!< time integration constant */
DOUBLE             alpha_m;      /*!< time integration constant */
DOUBLE             alpha_f;      /*!< time integration constant */
DOUBLE             ittol;        /*!< tolerance for iteration convergence check */
DOUBLE             sttol;        /*!< tolerance for steady state check */
DOUBLE             thetas;       /*!< constant for starting algorithm) */
struct _FLUID_DYN_CALC dynvar;
} FLUID_DYNAMIC;

/*!----------------------------------------------------------------------
\brief fluid parameters                                              

<pre>                                                         he  06/03  

In this structure all fluid variables needed on node are stored.

</pre>

------------------------------------------------------------------------*/
typedef struct _FLUID_VARIA               
{
DOUBLE             c_f_shear;     /*!< dim. shearstress c_f of node     */
} FLUID_VARIA;

