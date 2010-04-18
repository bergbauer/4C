/*!---------------------------------------------------------------------
\file
\brief

<pre>
Maintainer: Malte Neumann
            neumann@statik.uni-stuttgart.de
            http://www.uni-stuttgart.de/ibs/members/neumann/
            0711 - 685-6121
</pre>

---------------------------------------------------------------------*/

#ifndef CCADISCRET

#ifndef PARALLEL_PROTOTYPES_H
#define PARALLEL_PROTOTYPES_H

/*----------------------------------------------------------------------*
 |  par_assignmesh.c                                     m.gee 11/01    |
 *----------------------------------------------------------------------*/
void part_assignfield(void);
/*----------------------------------------------------------------------*
 |  par_initmetis.c                                      m.gee 11/01    |
 *----------------------------------------------------------------------*/
void part_fields(void);
/*----------------------------------------------------------------------*
 |  par_make_comm.c                                      m.gee 11/01    |
 *----------------------------------------------------------------------*/
void create_communicators(void);


#endif

#endif
