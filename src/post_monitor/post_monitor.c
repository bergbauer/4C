/*!
\file
\brief Postprocessing utility that generates a file with selected dofs
suitable for gnuplot.

<pre>
Maintainer: Ulrich Kuettler
            kuettler@lnm.mw.tum.de
            http://www.lnm.mw.tum.de/Members/kuettler
            089 - 289-15238
</pre>

Filters like this one are special inhabitants of the ccarat
world. They are always single processor applications yet they share
some code with ccarat and are closely linked to ccarat internals.

The general idea is that we cannot load the whole result data into
memory at once.

Filters are independent programs, thus they have their own main
function. This filter's main function is the last function in this
file (to keep the number of needed function prototypes as small as
possible). You might want to start reading the file from there.

\author u.kue
\date 10/04

*/

#include "post_monitor.h"


/*---------------------------------------------------------------------*
 | monotoring informations                                  genk 01/03 |
 *---------------------------------------------------------------------*/
struct _MONITOR *moni;

/* We need to remember the group names to each node. */
static CHAR*** group_names;


/*----------------------------------------------------------------------*/
/*!
  \brief The filter's main function.

  This is a very simple filter and thus this function does a major
  part of the work. The special thing about this filter is that it
  needs input. Its purpose is to extract certain (node) values from
  all time steps and put them into a (gnuplot style) text file. Of
  course the filter needs to be told which values to extract.

  In order to do this a second file needs to be given at the command
  line. The first file is, as usual, the control file of the ccarat
  output. The second file comes with the same flavor, follows the same
  syntax, and contains groups like the following:

  <pre>

  monitor:
      field = "fluid"
      field_pos = 0
      discretization = 0
      node = 440
      group = "velocity"
      dof = 0
      dof = 1

  </pre>

  This group tells that we are interested in the node 440 (global ID,
  counting from zero) from the discretization given by field,
  field_pos, and discretization. In particular we want to know the
  dofs 0 and 1 of the velocity output of this node.

  There can be many of these groups.

  \author u.kue
  \date 10/04
*/
/*----------------------------------------------------------------------*/
int main(int argc, char** argv)
{
  CHAR basename[100];
  MAP control_table;
  MAP monitor_table;
  PROBLEM_DATA problem;
  INT i;
  SYMBOL* sym_monitor;
  INT mone = -1;

  if (argc != 3) {
    printf("usage: %s control-file monitor-descr-file\n", argv[0]);
    return 1;
  }

  setup_filter(argv[1], &control_table, basename);

  dsassert(map_has_string(&control_table, "version", "0.1"),
           "expect version 0.1 control file");

  /* Debug output */
  /*map_print(stdout, &control_table, 0);*/

  init_problem_data(&problem, &control_table);

  /* We use the number of discretizations here. Maybe this will be a
   * problem once there are more discretizations per field. */
  moni = (MONITOR*)CCACALLOC(problem.num_discr, sizeof(MONITOR));

  group_names = (CHAR***)CCACALLOC(problem.num_discr, sizeof(CHAR**));

  /*--------------------------------------------------------------------*/
  /* read the control information what to monitor */

  /* count the nodes and dofs */
  /* For each node that is to be watched we need to find the
   * corresponding field. If the field description does not match any
   * field the request will be ignored. */
  parse_control_file(&monitor_table, argv[2]);
  sym_monitor = map_find_symbol(&monitor_table, "monitor");
  while (sym_monitor != NULL) {
    MAP* monitor;
    monitor = symbol_map(sym_monitor);

    for (i=0; i<problem.num_discr; ++i) {
      FIELD_DATA* field;
      field = &(problem.discr[i]);

      if (match_field_result(field, monitor)) {
        moni[i].numnp++;
        moni[i].numval += map_symbol_count(monitor, "dof");
        break;
      }
    }

    if (i==problem.num_discr) {
      printf("%s: %s: Unknown field. Ignore request.\n", argv[0], argv[2]);
    }

    sym_monitor = sym_monitor->next;
  }

  /* initialize monitor structure */
  /*
   * Now that the number of nodes to be watched is known we initialize
   * some auxiliary arrays and open the output files. */
  for (i=0; i<problem.num_discr; ++i) {
    CHAR buf[100];

    amdef("monnodes",&(moni[i].monnodes),moni[i].numnp,2,"IA");
    aminit(&(moni[i].monnodes),&mone);
    amdef("onoff",&(moni[i].onoff),moni[i].numnp,MAXDOFPERNODE,"IA");
    aminit(&(moni[i].onoff),&mone);

    group_names[i] = (CHAR**)CCACALLOC(moni[i].numnp, sizeof(CHAR*));

    switch (problem.discr[i].type) {
      case structure:
        sprintf(buf, "%s.structure.%d.mon", basename, problem.discr[i].field_pos);
        if ( (allfiles.out_smoni=fopen(buf,"w"))==NULL)
          dserror("failed to open output file");
        break;
      case fluid:
        sprintf(buf, "%s.fluid.%d.mon", basename, problem.discr[i].field_pos);
        if ( (allfiles.out_fmoni=fopen(buf,"w"))==NULL)
          dserror("failed to open output file");
        break;
      case ale:
        sprintf(buf, "%s.ale.%d.mon", basename, problem.discr[i].field_pos);
        if ( (allfiles.out_amoni=fopen(buf,"w"))==NULL)
          dserror("failed to open output file");
        break;
      default:
        dserror("unknown discretization type %d", problem.discr[i].type);
    }
  }

  /* read in global node Ids */
  for (i=0; i<problem.num_discr; ++i) {
    INT j = 0;
    FIELD_DATA* field;
    field = &(problem.discr[i]);

    sym_monitor = map_find_symbol(&monitor_table, "monitor");
    while (sym_monitor != NULL) {
      MAP* monitor;
      monitor = symbol_map(sym_monitor);

      if (match_field_result(field, monitor)) {
        INT node;
        SYMBOL* sym_dof;

        node = map_read_int(monitor, "node");
        group_names[i][j] = map_read_string(monitor, "group");

        /* remember the global node id */
        moni[i].monnodes.a.ia[j][0] = node;

        /* mark the dof */
        sym_dof = map_find_symbol(monitor, "dof");
        while (sym_dof != NULL) {
          INT dof;

          dof = symbol_int(sym_dof);
          if ((dof < 0) || (dof >= MAXDOFPERNODE)) {
            dserror("dof out of range");
          }
          moni[i].onoff.a.ia[j][dof] = 1;

          sym_dof = sym_dof->next;
        }

        j += 1;
        dsassert(j <= moni[i].numnp, "node count inconsistency");
      }

      sym_monitor = sym_monitor->next;
    }
  }

  /* determine local node Ids */
  for (i=0; i<problem.num_discr; ++i) {
    FIELD actfield;
    INT j;
    INT k;
    INT counter;
    FIELD_DATA* field;

    field = &(problem.discr[i]);
    amdef("val",&(moni[i].val),moni[i].numval,1,"DA");

    /* find the local ids */
    for (k=0;k<moni[i].numnp;k++) {
      for (j=0;j<field->numnp;j++) {
        if (moni[i].monnodes.a.ia[k][0] == field->node_ids[j]) {
          moni[i].monnodes.a.ia[k][1] = j;
          break;
        }
      }
      if (j == field->numnp) {
        dserror("no node %d in field", moni[i].monnodes.a.ia[k][0]);
      }
    }

    /* plausibility check */
    for (j=0;j<moni[i].numnp;j++) {
      if (moni[i].monnodes.a.ia[j][1]==-1)
        dserror("Monitoring Id not existing in field!");
    }

    /* give each watched dof an internal number */
    counter = 0;
    for (k=0;k<moni[i].numnp; k++) {
      for (j=0;j<MAXDOFPERNODE; j++) {
        if (moni[i].onoff.a.ia[k][j] != -1) {
          moni[i].onoff.a.ia[k][j] = counter;
          counter++;
        }
      }
    }
    dsassert(counter == moni[i].numval, "ndof mismatch");

    /* This is fake! But needed for the following function. */
    actfield.fieldtyp = field->type;

    /* initialize the output (print header) */
    out_monitor(&actfield,i,0.0,1);
  }

  /*--------------------------------------------------------------------*/
  /* read the data and write it */

  /* Visit all discretizations and all results to those. */
  for (i=0; i<problem.num_discr; ++i) {
    INT res;
    FIELD_DATA* field;
    field = &(problem.discr[i]);

    /* Iterate all results. */
    for (res=0; res<problem.num_results; ++res) {
      MAP* result_group;
      result_group = problem.result_group[res];

      /* We iterate the list of all results. Here we are interested in
       * the results of this discretization. */
      if (match_field_result(field, result_group)) {
        FIELD actfield;
        CHUNK_DATA chunk;
        INT l;
        DOUBLE time;

        time = map_read_real(result_group, "time");

        /* This is fake! But needed for now. */
        actfield.fieldtyp = field->type;

        /*
         * For each result search the nodes to watch and check whether
         * there are watched nodes in here. */
        for (l=0;l<moni[i].numnp;l++) {
          INT k;

          if (!read_chunk_group(&chunk, result_group, group_names[i][l])) {
            dserror("no group '%s' in result", group_names[i][l]);
          }

          /*
           * Collect the values to each dof by reading them directly
           * into the appropriate array location. */
          for (k=0;k<MAXDOFPERNODE;k++) {
            INT numr;
            INT nodepos;

            numr = moni[i].onoff.a.ia[l][k];
            if (numr==-1)
              continue;
            nodepos = moni[i].monnodes.a.ia[l][1];

            if (k >= chunk.value_entry_length) {
              dserror("dof %d does not exist", k);
            }

            fseek(field->value_file,
                  chunk.value_offset + (nodepos*chunk.value_entry_length + k)*sizeof(DOUBLE),
                  SEEK_SET);
            if (fread(&(moni[i].val.a.dv[numr]),
                      sizeof(DOUBLE),
                      1,
                      field->value_file) != 1) {
              dserror("reading value file of discretization %s failed", field->name);
            }
          }
        }

        /* output this time step */
        out_monitor(&actfield,i,time,0);
      }
    }
  }

  fprintf(allfiles.out_err, "Done.\n");
  return 0;
}
