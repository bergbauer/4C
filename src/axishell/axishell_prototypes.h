/*!----------------------------------------------------------------------
\file
\brief contains all prototypes for axisymmetric shell element

<pre>
Maintainer: Malte Neumann
            neumann@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/neumann/
            0711 - 685-6121
</pre>

*----------------------------------------------------------------------*/
#ifdef D_AXISHELL

/*! 
\addtogroup AXISHELL
*//*! @{ (documentation module open)*/

/*-----------------------------------------------------------------------*
|  saxi_init.c                                                 mn 05/03  |
*-----------------------------------------------------------------------*/
void saxiinit(PARTITION *actpart);
/*-----------------------------------------------------------------------*
|  saxi_static_ke.c                                            mn 05/03  |
*-----------------------------------------------------------------------*/
void saxistatic_ke(ELEMENT *ele, SAXI_DATA *data, MATERIAL *mat,
                   ARRAY *estif_global, INT init);    
/*-----------------------------------------------------------------------*
|  saxi_B.c                                                    mn 05/03  |
*-----------------------------------------------------------------------*/
void saxi_B(DOUBLE **B, DOUBLE xsi, DOUBLE r, DOUBLE dl, DOUBLE cosa, 
            DOUBLE sina);  
/*-----------------------------------------------------------------------*
|  saxi_mat.c                                                  mn 05/03  |
*-----------------------------------------------------------------------*/
void saxi_mat(STVENANT *mat, DOUBLE **D, DOUBLE h);    
/*-----------------------------------------------------------------------*
|  saxi_call_stiff.c                                           mn 05/03  |
*-----------------------------------------------------------------------*/
void saxi_keku(DOUBLE **s, DOUBLE **b, DOUBLE **d, DOUBLE **work1,
               DOUBLE **work2, DOUBLE fac, DOUBLE r, DOUBLE dl);  
/*-----------------------------------------------------------------------*
|  saxi_static_ke.c                                            mn 05/03  |
*-----------------------------------------------------------------------*/
void saxi_statcond(ELEMENT *ele, DOUBLE **estiff, DOUBLE **estiff_7,
                   DOUBLE cosa, DOUBLE sina);   
/*-----------------------------------------------------------------------*
|  saxi_cal_stress.c                                           mn 05/03  |
*-----------------------------------------------------------------------*/
void saxi_cal_stress(ELEMENT *ele, SAXI_DATA *data, MATERIAL *mat, INT init);                  
/*-----------------------------------------------------------------------*
|  saxi_intg.c                                                 mn 05/03  |
*-----------------------------------------------------------------------*/
void saxiintg(SAXI_DATA *data);          
/*-----------------------------------------------------------------------*
 | saxi_cal_fext.c                                             mn 05/03  |
 *----------------------------------------------------------------------*/
void saxi_eleload(ELEMENT *ele, SAXI_DATA  *data, DOUBLE *loadvec,INT init);                
/*----------------------------------------------------------------------*/
#endif /*D_AXISHELL*/
/*! @} (documentation module close)*/
