/*!----------------------------------------------------------------------
\file
\brief

<pre>
Maintainer: Malte Neumann
            neumann@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/neumann/
            0711 - 685-6121
</pre>

------------------------------------------------------------------------*/

#ifndef GID_H
#define GID_H

/*----------------------------------------------------------------------*
 | structure for gid output data                         m.gee 12/01    |
 *----------------------------------------------------------------------*/
typedef struct _GIDSET
{
     FIELDTYP                   fieldtyp;               /* type of field */
     INT                        fieldnamelenght;        /* lenght of fieldname */
     char                      *fieldname;              /* name of the field in characters */
     char                       standardrangetable[18]; /* name of standardrangetable */

     INT                        is_shell8_4_22;           /* 4-noded shell8 2x2 GP */
     char                      *shell8_4_22_name;
     INT                        is_shell8_4_33;           /* 4-noded shell8 3x3 GP */
     char                      *shell8_4_33_name;
     INT                        is_shell8_3_11;           /* 3-noded shell8 1 GP */
     char                      *shell8_3_11_name;
     INT                        is_shell8_6_33;           /* 6-noded shell8 3 GP */
     char                      *shell8_6_33_name;
     INT                        is_shell8_8_22;           /* 8-noded shell8 2x2 GP */
     char                      *shell8_8_22_name;
     INT                        is_shell8_8_33;           /* 8-noded shell8 3x3 GP */
     char                      *shell8_8_33_name;
     INT                        is_shell8_9_22;           /* 9-noded shell8 2x2 GP */
     char                      *shell8_9_22_name;
     INT                        is_shell8_9_33;           /* 9-noded shell8 3x3 GP */
     char                      *shell8_9_33_name;

     INT                        is_shell9_4_22;         /* 4-noded shell9 2x2 GP */
     char                      *shell9_4_22_name;
     INT                        is_shell9_4_33;         /* 4-noded shell9 3x3 GP */
     char                      *shell9_4_33_name;
     INT                        is_shell9_8_22;         /* 8-noded shell9 2x2 GP */
     char                      *shell9_8_22_name;
     INT                        is_shell9_8_33;         /* 8-noded shell9 3x3 GP */
     char                      *shell9_8_33_name;
     INT                        is_shell9_9_22;         /* 9-noded shell9 2x2 GP */
     char                      *shell9_9_22_name;
     INT                        is_shell9_9_33;         /* 9-noded shell9 3x3 GP */
     char                      *shell9_9_33_name;
     INT                        is_wall1_11;            /* 3-noded wall1 1x1 GP */
     char                      *wall1_11_name;
     INT                        is_wall1_22;            /* 4-noded wall1 2x2 GP */
     char                      *wall1_22_name;
     INT                        is_wall1_33;            /* 8/9-noded wall1 3x3 GP */
     char                      *wall1_33_name;
     INT                        is_wall1_tri_31;            /* 6-noded wall1 3x1 GP */
     char                      *wall1_tri_31_name;
     INT                        is_brick1_222;          /* 8-noded brick1 2x2x2 GP */
     char                      *brick1_222_name;
     INT                        is_brick1_333;          /* 20/27 noded brick1 3x3x3 GP */
     char                      *brick1_333_name;

     INT                        is_fluid2_22;           /* 4-noded fluid2 2x2 GP */
     char                      *fluid2_22_name;
     INT                        is_fluid2_33;           /* 8/9-noded fluid2 3x3 GP */
     char                      *fluid2_33_name;

     INT                        is_fluid2_tu_22;           /* 4-noded fluid2 2x2 GP */
     char                      *fluid2_tu_22_name;
     INT                        is_fluid2_tu_33;           /* 8/9-noded fluid2 3x3 GP */
     char                      *fluid2_tu_33_name;

     INT                        is_fluid2_tri3;            /* 3 noded fluid2 triangle */
     char                      *fluid2_tri3_name;
     INT                        is_fluid2_6;            /* 6 noded fluid2 triangle */
     char                      *fluid2_6_name;

     INT                        is_fluid2_pro_22;       /* 4-noded fluid2_pro 2x2 GP */
     char                      *fluid2_pro_22_name;
     INT                        is_fluid2_pro_33;       /* 8/9-noded fluid2 3x3 GP */
     char                      *fluid2_pro_33_name;

     INT                        is_fluid2_is_22;       /* 4-noded fluid2_is 2x2 GP */
     char                      *fluid2_is_22_name;
     INT                        is_fluid2_is_33;       /* 8/9-noded fluid2 3x3 GP */
     char                      *fluid2_is_33_name;

     INT                        is_fluid3_222;          /* 8-noded fluid3 2x2x2 GP */
     char                      *fluid3_222_name;
     INT                        is_fluid3_333;          /* 20/27-noded fluid3 3x3x3 GP */
     char                      *fluid3_333_name;

     INT                        is_fluid3_pro_222;      /* 8-noded fluid3 2x2x2 GP */
     char                      *fluid3_pro_222_name;
     INT                        is_fluid3_pro_333;      /* 20/27-noded fluid3 3x3x3 GP */
     char                      *fluid3_pro_333_name;

     INT                        is_fluid3_is_222;      /* 8-noded fluid3 2x2x2 GP */
     char                      *fluid3_is_222_name;
     INT                        is_fluid3_is_333;      /* 20/27-noded fluid3 3x3x3 GP */
     char                      *fluid3_is_333_name;

     INT                        is_fluid3_tet4;
     char                      *fluid3_tet4_name;
     INT                        is_fluid3_tet10;
     char                      *fluid3_tet10_name;

     INT                        is_f3f_4_4;             /* 8-noded fluid3_fast 2x2x2 GP */
     char                      *f3f_4_4_name;

     INT                        is_f3f_8_222;             /* 8-noded fluid3_fast 2x2x2 GP */
     char                      *f3f_8_222_name;
     INT                        is_f3f_8_333;             /* 8-noded fluid3_fast 3x3x3 GP */
     char                      *f3f_8_333_name;
     INT                        is_f3f_20_222;             /* 20-noded fluid3_fast 2x2x2 GP */
     char                      *f3f_20_222_name;
     INT                        is_f3f_20_333;             /* 20-noded fluid3_fast 3x3x3 GP */
     char                      *f3f_20_333_name;

     INT                        is_ale_11;              /* 4-noded ale 1x1 GP */
     char                      *ale_11_name;
     INT                        is_ale_22;              /* 4-noded ale 2x2 GP */
     char                      *ale_22_name;

     INT                        is_ale_8_111;             /* 8-noded ale 1x1x1 GP */
     char                      *ale_8_111_name;
     INT                        is_ale_8_222;             /* 8-noded ale 2x2x2 GP */
     char                      *ale_8_222_name;
     INT                        is_ale_8_333;             /* 8-noded ale 3x3x3 GP */
     char                      *ale_8_333_name;
     INT                        is_ale_20_111;             /* 20-noded ale 1x1x1 GP */
     char                      *ale_20_111_name;
     INT                        is_ale_20_222;             /* 20-noded ale 2x2x2 GP */
     char                      *ale_20_222_name;
     INT                        is_ale_20_333;             /* 20-noded ale 3x3x3 GP */
     char                      *ale_20_333_name;
     INT                        is_ale_27_111;             /* 27-noded ale 1x1x1 GP */
     char                      *ale_27_111_name;
     INT                        is_ale_27_222;             /* 27-noded ale 2x2x2 GP */
     char                      *ale_27_222_name;
     INT                        is_ale_27_333;             /* 27-noded ale 3x3x3 GP */
     char                      *ale_27_333_name;

     INT			is_beam3_21;		/* 2-noded beam3 1 GP */
     char		       *beam3_21_name;
     INT			is_beam3_22;		/* 2-noded beam3 2 GP */
     char		       *beam3_22_name;
     INT			is_beam3_32;		/* 3-noded beam3 2 GP */
     char		       *beam3_32_name;
     INT			is_beam3_33;		/* 3-noded beam3 3 GP */
     char		       *beam3_33_name;

     INT                        is_ale_tri_1;           /* 3-noded tri ale 1 GP */
     char                      *ale_tri_1_name;
     INT                        is_ale_tri_3;           /* 3-noded tri ale 3 GP */
     char                      *ale_tri_3_name;
     INT                        is_ale_tet_1;           /* 4-noded tet ale 1 GP */
     char                      *ale_tet_1_name;
     INT                        is_ale_tet_4;           /* 4-noded tet ale 4 GP */
     char                      *ale_tet_4_name;
     INT                        is_axishell;            /* 2-noded axishell */
     char                      *axishell_name;
     INT                        is_interf_22;           /* interface 2x2 GP */
     char                      *interf_22_name;
     INT                        is_interf_33;           /* interface 3x3 GP */
     char                      *interf_33_name;
     INT                        is_wallge_22;           /* gradient enhanced wall 2x2 GP */
     char                      *wallge_22_name;
     INT                        is_wallge_33;           /* gradient enhanced wall 3x3 GP */
     char                      *wallge_33_name;

     INT                        is_therm2_q4_11;            /* 4-noded therm2 1x1 GP */
     char                      *therm2_q4_11_name;
     INT                        is_therm2_q4_22;            /* 4-noded wall1 2x2 GP */
     char                      *therm2_q4_22_name;
     INT                        is_therm2_q8_33;            /* 8-noded wall1 3x3 GP */
     char                      *therm2_q8_33_name;
     INT                        is_therm2_q9_33;            /* 9-noded wall1 3x3 GP */
     char                      *therm2_q9_33_name;
     INT                        is_therm2_t3_1;            /* 3-noded wall1 1 GP */
     char                      *therm2_t3_1_name;
     INT                        is_therm2_t6_3;            /* 6-noded wall1 3 GP */
     char                      *therm2_t6_3_name;

     INT                        is_therm3_h8_222;          /* 8-noded therm3 2x2x2 GP */
     char                      *therm3_h8_222_name;
     INT                        is_therm3_h20_333;          /* 20-noded therm3 3x3x3 GP */
     char                      *therm3_h20_333_name;
     INT                        is_therm3_h27_333;          /* 27-noded therm3 3x3x3 GP */
     char                      *therm3_h27_333_name;
     INT                        is_therm3_t4_1;          /* 4-noded therm3 1 GP */
     char                      *therm3_t4_1_name;
     INT                        is_therm3_t10_4;          /* 10-noded therm3 4 GP */
     char                      *therm3_t10_4_name;

     INT                        is_solid3_h8_222;          /* 8-noded solid3 2x2x2 GP */
     char                      *solid3_h8_222_name;
     INT                        is_solid3_h20_333;          /* 20-noded solid3 3x3x3 GP */
     char                      *solid3_h20_333_name;
     INT                        is_solid3_h27_333;          /* 27-noded solid3 3x3x3 GP */
     char                      *solid3_h27_333_name;
     INT                        is_solid3_t4_1;          /* 4-noded solid3 1 GP */
     char                      *solid3_t4_1_name;
     INT                        is_solid3_t10_4;          /* 10-noded solid3 4 GP */
     char                      *solid3_t10_4_name;

} GIDSET;



/*------------------------ global variable needed by gid postprocessing */
extern GIDSET **gid;



#ifdef D_MLSTRUCT
/*----------------------------- the same for submesh gid postprocessing */
extern GIDSET *sm_gid;
#endif /* D_MLSTRUCT */



#endif



