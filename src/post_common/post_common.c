/*!
\file
\brief Code that is common to all filters.

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

All filters need to read the ccarat output. Thus there are some common
functions.

\author u.kue
\date 10/04

*/

#include <assert.h>
#include <strings.h>

#include "post_common.h"


/* There are some global variables in ccarat that are needed by the
 * service functions. We need to specify them here and set them up
 * properly. */
struct _FILES           allfiles;
struct _PAR     par;

#ifdef DEBUG
struct _CCA_TRACE         trace;
#endif


/*----------------------------------------------------------------------*/
/*!
  \brief All fields names.

  \author u.kue
  \date 08/04
*/
/*----------------------------------------------------------------------*/
CHAR* fieldnames[] = FIELDNAMES;

/*----------------------------------------------------------------------*/
/*!
  \brief All dis type names.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
CHAR* distypenames[] = DISTYPENAMES;

/*----------------------------------------------------------------------*/
/*!
  \brief All element type names.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
CHAR* elementnames[] = ELEMENTNAMES;


/*----------------------------------------------------------------------*/
/*!
  \brief This is ccarat setup in a hurry.

  Initialize ds tracer. Open log and control file. Read control file.

  \param output_name   (i) control file name as given at the command line
  \param control_table (o) map for the control file's contents
  \param basename      (o) the control file's name

  \author u.kue
  \date 09/04
*/
/*----------------------------------------------------------------------*/
void setup_filter(CHAR* output_name, MAP* control_table, CHAR* basename)
{
  INT length;
  CHAR control_file_name[100];

  par.myrank = 0;
  par.nprocs = 1;

  /* Do we want to have tracing? We could. */
#ifdef DEBUG
  dsinit();

  /* We need to take two steps back. dsinit is too close to ccarat. */
  dstrc_exit();
  dstrc_exit();
  dstrc_enter("setup_filter");
  trace.trace_on = 1;
#endif

  /* The warning system is not set up. It's rather stupid anyway. */

  /* We need to open the error output file. The other ones are not
   * important. */
  length = strlen(output_name);
  if ((length > 8) && (strcmp(output_name+length-8, ".control")==0)) {
    /* dsassert isn't working yet. */
    assert(length-8+13 < 100);
    strcpy(allfiles.outputfile_name, output_name);
    strcpy(allfiles.outputfile_name+length-8, ".post.log");
    strcpy(control_file_name, output_name);
    strcpy(basename, output_name);
    basename[length-8] = '\0';
  }
  else {
    /* dsassert isn't working yet. */
    assert(length+13 < 100);
    strcpy(allfiles.outputfile_name, output_name);
    strcpy(allfiles.outputfile_name+length, ".post.log");
    strcpy(control_file_name, output_name);
    strcpy(control_file_name+length, ".control");
    strcpy(basename, output_name);
  }
  allfiles.out_err = fopen(allfiles.outputfile_name, "w");

  parse_control_file(control_table, control_file_name);

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief A Hack.

  This a yet another hack. This function is called by dserror and
  closes all open files --- in ccarat. The filters are not that
  critical. Thus we do nothing here. We just have to have this
  function.

  \author u.kue
  \date 12/04
*/
/*----------------------------------------------------------------------*/
void io_emergency_close_files()
{
}


/*----------------------------------------------------------------------*/
/*!
  \brief Init a translation table.

  The purpose of these tables is to find the internal enum value to an
  external number read from the data file. We need to translate each
  enum value we read. Otherwise the number written would be an
  implementation detail, thus we could not change our implementation
  once we had some binary files.

  \param table (o) table to be initialized
  \param group (i) control file group that defines the external values
  \param names (i) list of names ordered by internal number, NULL terminated

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void init_translation_table(TRANSLATION_TABLE* table,
                            MAP* group,
                            CHAR** names)
{
  INT i;
  INT count;

#ifdef DEBUG
  dstrc_enter("init_translation_table");
#endif

  /*
   * It seems to be more convenient to require the list of names to be
   * null terminated and not to ask for it's length in advance. Thus
   * we have to count the names. But we can easily to this because
   * this function is never called in the inner loop. We've got the
   * time. ;) */
  count = 0;
  for (count=0; names[count] != NULL; ++count)
  {}

  table->group = group;
  table->table = CCACALLOC(count, sizeof(INT));
  table->length = count;
  for (i=0; i<count; ++i)
  {
    if (map_symbol_count(group, names[i]) > 0)
    {
      INT num;
      num = map_read_int(group, names[i]);
      if ((num < 0) || (num >= count))
      {
        dserror("illegal external number for name '%s': %d", names[i], num);
      }

      /* extern -> intern translation */
      table->table[num] = i;
    }
  }

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Clear the memory occupied by the translation table.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void destroy_translation_table(TRANSLATION_TABLE* table)
{
#ifdef DEBUG
  dstrc_enter("destroy_translation_table");
#endif

  CCAFREE(table->table);

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Open data files.

  \param result     (i/o) result those files are to be opened
  \param field_info (i)   group that contains the filename definitions
  \param prefix     (i)   file name prefix

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
static void open_data_files(RESULT_DATA* result, MAP* field_info, CHAR* prefix)
{
  CHAR* filename;
  CHAR var_name[100];
  PROBLEM_DATA* problem;

#ifdef DEBUG
  dstrc_enter("open_data_files");
#endif

  problem = result->field->problem;

  sprintf(var_name, "%s_value_file", prefix);
  filename = map_read_string(field_info, var_name);

  /* It's misleading to look in the current directory by default. */

  /*result->value_file = fopen(filename, "rb");
    if (result->value_file == NULL)*/
  {
    CHAR buf[100];
    strcpy(buf, problem->input_dir);
    strcat(buf, filename);

    /* windows asks for a binary flag here... */
    result->value_file = fopen(buf, "rb");
    if (result->value_file == NULL)
    {
      dserror("failed to open file '%s'", filename);
    }

    fprintf(allfiles.out_err, "open file: '%s'\n", buf);
  }

  sprintf(var_name, "%s_size_file", prefix);
  filename = map_read_string(field_info, var_name);
  /*result->size_file = fopen(filename, "rb");
    if (result->size_file == NULL)*/
  {
    CHAR buf[100];
    strcpy(buf, problem->input_dir);
    strcat(buf, filename);

    /* windows asks for a binary flag here... */
    result->size_file = fopen(buf, "rb");
    if (result->size_file == NULL)
    {
      dserror("failed to open file '%s'", filename);
    }

    fprintf(allfiles.out_err, "open file: '%s'\n", buf);
  }

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Extract one discretization's data.

  \author u.kue
  \date 09/04
*/
/*----------------------------------------------------------------------*/
void init_field_data(PROBLEM_DATA* problem, FIELD_DATA* field, MAP* field_info)
{
  INT i;
  CHAR* type;

#ifdef DEBUG
  dstrc_enter("init_field_data");
#endif

  field->problem = problem;

  field->group = field_info;
  field->field_pos = map_read_int(field_info, "field_pos");
  field->disnum = map_read_int(field_info, "discretization");
  field->numele = map_read_int(field_info, "numele");
  field->numnp = map_read_int(field_info, "numnp");
  field->numdf = map_read_int(field_info, "numdof");

  field->name = map_read_string(field_info, "field");
  for (i=0; fieldnames[i]!=NULL; ++i)
  {
    if (strcmp(fieldnames[i], field->name)==0)
    {
      field->type = i;
      break;
    }
  }
  if (fieldnames[i]==NULL)
  {
    dserror("unknown field type '%s'", type);
  }

  /*--------------------------------------------------------------------*/
  /* Open the data files. */

  /* The fake variables. */
  field->head.pos = -1;
  field->head.field = field;
  field->head.group = field_info;

  open_data_files(&(field->head), field_info, "mesh");

  /*--------------------------------------------------------------------*/
  /* setup chunk structures */

  init_chunk_data(&(field->head), &(field->ele_param), "ele_param");
  init_chunk_data(&(field->head), &(field->mesh), "mesh");
  init_chunk_data(&(field->head), &(field->coords), "coords");

  /*--------------------------------------------------------------------*/
  /* special problems demand special attention. */

#ifdef D_SHELL8
  if (map_has_string(field_info, "shell8_problem", "yes"))
  {
    field->is_shell8_problem = 1;
  }
  else
  {
    field->is_shell8_problem = 0;
  }
#endif

#ifdef D_SHELL9
  if (map_has_string(field_info, "shell9_problem", "yes"))
  {
    /* This is a shell9 problem. There is guaranteed to be just one
     * type of element. The element_type flags are ignored. */

    field->is_shell9_problem = 1;

    field->s9_smooth_results = map_has_string(field_info, "shell9_smoothed", "yes");
    field->s9_layers = map_read_int(field_info, "shell9_layers");
  }
  else {
    field->is_shell9_problem = 0;
  }
#endif
#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Extract the problem's data.

  \param problem       (o) problem object to be initialized
  \param argc          (i) number of command line arguments
  \param argv          (i) command line arguments

  \author u.kue
  \date 09/04
*/
/*----------------------------------------------------------------------*/
void init_problem_data(PROBLEM_DATA* problem, INT argc, CHAR** argv)
{
  CHAR* problem_names[] = PROBLEMNAMES;
  CHAR* type;
  INT i;
  SYMBOL* symbol;
  CHAR* separator;
  MAP* control_table;
  CHAR* filename;

#ifdef DEBUG
  dstrc_enter("init_problem_data");
#endif

  /*--------------------------------------------------------------------*/
  /* default values */

  problem->start = 0;
  problem->end = -1;
  problem->step = 1;

  /*--------------------------------------------------------------------*/
  /* process command line arguments */

  if (argc < 2)
  {
    printf("usage: %s [options] control-file\n", argv[0]);
    exit(1);
  }

  for (i=1; i<argc-1; ++i)
  {
    CHAR* arg;

    arg = argv[i];

    if (arg[0] == '-')
    {
      switch (arg[1])
      {
      case 's':                 /* slices */
      {
        if (arg[2] != '\0')
        {
          arg = &(arg[2]);
        }
        else
        {
          i += 1;
          if (i == argc-1)
          {
            dserror("option '-s' must be followed by a slice like this: 'beg:end[:step]'");
          }
          arg = argv[i];
        }

        /* simple parsing, only limited error checking */
        problem->start = atoi(arg);
        arg = strstr(arg, ":");
        if (arg == NULL)
        {
          dserror("option '-s' must be followed by a slice like this: 'beg:end[:step]'");
        }

        /* we support things like 'beg::step' and 'beg:' */
        if ((arg[1] != ':') && (arg[1] != '\0'))
        {
          problem->end = atoi(arg+1);
        }
        arg = strstr(arg+1, ":");
        if (arg != NULL)
        {
          problem->step = atoi(arg+1);
        }
        break;
      }
      default:
        dserror("unsupported option '%s'", arg);
      }
    }
  }

  /*--------------------------------------------------------------------*/
  /* setup fake ccarat environment and read control file */

  control_table = &(problem->control_table);

  setup_filter(argv[argc-1], control_table, problem->basename);

  dsassert(map_has_string(control_table, "version", "0.2"),
           "expect version 0.2 control file");

  /* Debug output */
  /*map_print(stdout, &control_table, 0);*/

  /*--------------------------------------------------------------------*/
  /* read general information */

  problem->ndim = map_read_int(control_table, "ndim");
  dsassert((problem->ndim == 2) || (problem->ndim == 3), "illegal dimension");

  type = map_read_string(control_table, "problem_type");
  for (i=0; problem_names[i] != NULL; ++i)
  {
    if (strcmp(type, problem_names[i])==0)
    {
      problem->type = i;
      break;
    }
  }
  if (problem_names[i] == NULL)
  {
    dserror("unknown problem type '%s'", type);
  }

  /*--------------------------------------------------------------------*/
  /* Find the input directory by looking at the control file
   * name. We look in the current directory for data files and if that
   * fails we look in the input directory. */

  /* This is the unix version. Different input directories are not
   * supported on windows. */
  filename = problem->basename;
  separator = rindex(filename, '/');
  if (separator == NULL)
  {
    problem->input_dir[0] = '\0';
  }
  else
  {
    INT n;

    /* 'separator-filename' gives the number of chars before the
     * separator. But we want to copy the separator as well. */
    n = separator-filename+1;
    dsassert(n < 100, "file name overflow");
    strncpy(problem->input_dir, filename, n);
    problem->input_dir[n] = '\0';
  }

  /*--------------------------------------------------------------------*/
  /* get the meaning of the elements' chunks */

  setup_element_variables_map(control_table);

  /*--------------------------------------------------------------------*/
  /* collect all result groups */

  problem->num_results = map_symbol_count(control_table, "result");
  if (problem->num_results == 0)
  {
    dserror("no results found");
  }
  problem->result_group = (MAP**)CCACALLOC(problem->num_results, sizeof(MAP*));

  /* find the first result group */
  symbol = map_find_symbol(control_table, "result");

  /* We rely on the fact that groups are linked in reverse order. */
  /* That is results are written ordered by time step. */
  for (i=problem->num_results-1; i>=0; --i)
  {
    if (!symbol_is_map(symbol))
    {
      dserror("failed to get result group");
    }

    problem->result_group[i] = symbol_map(symbol);

    symbol = symbol->next;
  }

  /*--------------------------------------------------------------------*/
  /* setup all fields */

  /* We need to output each field separately. */
  problem->num_discr = map_symbol_count(control_table, "field");
  if (problem->num_discr==0)
  {
    dserror("no field group found");
  }
  problem->discr = (FIELD_DATA*)CCACALLOC(problem->num_discr, sizeof(FIELD_DATA));

  /* find the first field (the last one that has been written) */
  symbol = map_find_symbol(control_table, "field");

  /* read all fields headers, open the data files */
  for (i=0; i<problem->num_discr; ++i)
  {
    if (!symbol_is_map(symbol))
    {
      dserror("failed to get field group");
    }

    init_field_data(problem, &(problem->discr[i]), symbol_map(symbol));

    symbol = symbol->next;
  }

  /*--------------------------------------------------------------------*/
  /* setup the translation tables */

  init_translation_table(&(problem->element_type),
                         map_read_map(control_table, "element_names"),
                         elementnames);

  init_translation_table(&(problem->distype),
                         map_read_map(control_table, "distype_names"),
                         distypenames);

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Tell whether a given result group belongs to this field.

  \author u.kue
  \date 10/04
*/
/*----------------------------------------------------------------------*/
INT match_field_result(FIELD_DATA *field, MAP *result_group)
{
  return (strcmp(map_read_string(result_group, "field"),
                 fieldnames[field->type]) == 0) &&
    (map_read_int(result_group, "field_pos") == field->field_pos) &&
    (map_read_int(result_group, "discretization") == field->disnum);
}


/*----------------------------------------------------------------------*/
/*!
  \brief Initialize the result data.

  You need to call \a next_result to get to the first result of this
  discretization.

  \author u.kue
  \date 11/04
  \sa next_result
*/
/*----------------------------------------------------------------------*/
void init_result_data(FIELD_DATA* field, RESULT_DATA* result)
{
#ifdef DEBUG
  dstrc_enter("init_result_data");
#endif

  result->field = field;
  result->pos = -1;
  result->value_file = NULL;
  result->size_file = NULL;

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Cleanup result data.

  Please note that there must not be any chunk data on this result
  after this function has been called.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void destroy_result_data(RESULT_DATA* result)
{
#ifdef DEBUG
  dstrc_enter("destroy_result_data");
#endif

  if (result->value_file != NULL)
  {
    fclose(result->value_file);
    fclose(result->size_file);
  }

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Go to the next result of this discretization.

  \param result (i/o) on input the current result data

  \return zero if there is no further result. non-zero otherwise.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
INT next_result(RESULT_DATA* result)
{
  PROBLEM_DATA* problem;
  INT i;
  INT ret = 0;

#ifdef DEBUG
  dstrc_enter("next_result");
#endif

  problem = result->field->problem;

  for (i=result->pos+1; i<problem->num_results; ++i)
  {
    INT step;
    MAP* map;
    map = problem->result_group[i];

    if (match_field_result(result->field, map))
    {

      /*
       * Open the new files if there are any.
       *
       * If one of these files is here the other one has to be
       * here, too. If it's not, it's a bug in the input. */
      if ((map_symbol_count(map, "result_value_file") > 0) ||
          (map_symbol_count(map, "result_size_file") > 0))
      {
        if (result->value_file != NULL)
        {
          fclose(result->value_file);
          fclose(result->size_file);
        }
        open_data_files(result, map, "result");
      }

      step = map_read_int(map, "step");

      /* we are only interessted if the result matches the slice */
      if ((step >= problem->start) &&
          ((step < problem->end) || (problem->end == -1)) &&
          ((step - problem->start) % problem->step == 0))
      {
        result->pos = i;
        result->group = map;
        ret = 1;
        break;
      }
    }
  }

#ifdef DEBUG
  dstrc_exit();
#endif
  return ret;
}


/*======================================================================*/
/*======================================================================*/


  /* on little endian machines we have to convert */
  /* We have 8 byte doubles and 4 byte integer by definition. Nothing
   * else. */

#ifdef IS_LITTLE_ENDIAN

  /* very specific swap macro */
#define SWAP_CHAR(c1,c2) { CHAR t; t=c1; c1=c2; c2=t; }


/*----------------------------------------------------------------------*/
/*!
  \brief Convert the read big endian values to little endian.

  \author u.kue
  \date 12/04
*/
/*----------------------------------------------------------------------*/
static void byteswap_doubles(DOUBLE* data, INT length)
{
  INT i;

  for (i=0; i<length; ++i)
  {
    CHAR* ptr;

    ptr = (CHAR*)&(data[i]);
    SWAP_CHAR(ptr[0], ptr[7]);
    SWAP_CHAR(ptr[1], ptr[6]);
    SWAP_CHAR(ptr[2], ptr[5]);
    SWAP_CHAR(ptr[3], ptr[4]);
  }
}


/*----------------------------------------------------------------------*/
/*!
  \brief Convert the read big endian values to little endian.

  \author u.kue
  \date 12/04
*/
/*----------------------------------------------------------------------*/
static void byteswap_ints(INT* data, INT length)
{
  INT i;
  for (i=0; i<length; ++i)
  {
    CHAR* ptr;

    ptr = (CHAR*)&(data[i]);
    SWAP_CHAR(ptr[0], ptr[3]);
    SWAP_CHAR(ptr[1], ptr[2]);
  }
}

#undef SWAP_CHAR

#else

/* noops to make life easier further down */
#define byteswap_doubles(data, length)
#define byteswap_ints(data, length)

#endif


/*----------------------------------------------------------------------*/
/*!
  \brief Set up the chunk structure to iterate the chunk's entries.

  \param result (i) on input the current result data
  \param chunk  (o) the chunk to be initialized
  \param name   (i) the name of the group in the control file

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void init_chunk_data(RESULT_DATA* result, CHUNK_DATA* chunk, CHAR* name)
{
#ifndef LOWMEM
  CHAR* type;
#endif

#ifdef DEBUG
  dstrc_enter("init_chunk_data");
#endif

  chunk->result = result;
  chunk->group = map_read_map(result->group, name);

  chunk->value_entry_length = map_read_int(chunk->group, "value_entry_length");
  chunk->value_offset = map_read_int(chunk->group, "value_offset");

  chunk->size_entry_length = map_read_int(chunk->group, "size_entry_length");
  chunk->size_offset = map_read_int(chunk->group, "size_offset");

#ifdef LOWMEM

  /* Low memory! We read one entry only. This way we have to reread
   * some entries many times. */

  if (chunk->value_entry_length > 0)
  {
    chunk->value_buf = (DOUBLE*)CCACALLOC(chunk->value_entry_length, sizeof(DOUBLE));
  }
  else
  {
    chunk->value_buf = NULL;
  }

  if (chunk->size_entry_length > 0)
  {
    chunk->size_buf = (INT*)CCACALLOC(chunk->size_entry_length, sizeof(INT));
  }
  else
  {
    chunk->size_buf = NULL;
  }

#else

  /* More memory (smaller problem size). We read the whole chunk at
   * once. This is supposed to be fast. */

  type = map_read_string(chunk->group, "type");

  if (strcmp(type, "element") == 0)
  {
    INT length = chunk->value_entry_length*result->field->numele;

    if (length > 0)
    {
      chunk->value_data = (DOUBLE*)CCACALLOC(length, sizeof(DOUBLE));
      fseek(chunk->result->value_file, chunk->value_offset, SEEK_SET);
      if (fread(chunk->value_data, sizeof(DOUBLE),
                length,
                chunk->result->value_file) != length)
      {
        dserror("failed to read value file of field '%s'", result->field->name);
      }
      byteswap_doubles(chunk->value_data, length);
    }
    else
    {
      chunk->value_data = NULL;
    }

    length = chunk->size_entry_length*result->field->numele;

    if (length > 0)
    {
      chunk->size_data = (INT*)CCACALLOC(length, sizeof(INT));
      fseek(chunk->result->size_file, chunk->size_offset, SEEK_SET);
      if (fread(chunk->size_data, sizeof(INT),
                length,
                chunk->result->size_file) != length)
      {
        dserror("failed to read size file of field '%s'", result->field->name);
      }
      byteswap_ints(chunk->size_data, length);
    }
    else
    {
      chunk->size_data = NULL;
    }
  }
  else if (strcmp(type, "node") == 0)
  {
    INT length = chunk->value_entry_length*result->field->numnp;

    if (length > 0)
    {
      INT length_read;
      chunk->value_data = (DOUBLE*)CCACALLOC(length, sizeof(DOUBLE));
      fseek(chunk->result->value_file, chunk->value_offset, SEEK_SET);
      length_read = fread(chunk->value_data, sizeof(DOUBLE),
                          length,
                          chunk->result->value_file);
      if (length_read != length)
      {
        dserror("failed to read value file of field '%s'", result->field->name);
      }
      byteswap_doubles(chunk->value_data, length);
    }
    else
    {
      chunk->value_data = NULL;
    }

    length = chunk->size_entry_length*result->field->numnp;

    if (length > 0)
    {
      INT length_read;
      chunk->size_data = (INT*)CCACALLOC(length, sizeof(INT));
      fseek(chunk->result->size_file, chunk->size_offset, SEEK_SET);
      length_read = fread(chunk->size_data, sizeof(INT),
                          length,
                          chunk->result->size_file);
      if (length_read != length)
      {
        dserror("failed to read size file of field '%s'", result->field->name);
      }
      byteswap_ints(chunk->size_data, length);
    }
    else
    {
      chunk->size_data = NULL;
    }
  }
  else
  {
    dserror("chunk type '%s' not supported", type);
  }

#endif

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Free the chunk data.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void destroy_chunk_data(CHUNK_DATA* chunk)
{
#ifdef DEBUG
  dstrc_enter("destroy_chunk_data");
#endif

#ifdef LOWMEM

  if (chunk->value_buf != NULL)
    CCAFREE(chunk->value_buf);

  if (chunk->size_buf != NULL)
    CCAFREE(chunk->size_buf);

#else

  if (chunk->value_data != NULL)
    CCAFREE(chunk->value_data);

  if (chunk->size_data != NULL)
    CCAFREE(chunk->size_data);

#endif

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Read one size entry form the file and store it to this
  chunk_data's internal buffer.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void chunk_read_size_entry(CHUNK_DATA* chunk, INT id)
{
#ifdef DEBUG
  dstrc_enter("chunk_read_size_entry");
#endif

  dsassert(chunk->size_entry_length > 0, "cannot read empty entry");

#ifdef LOWMEM

  fseek(chunk->result->size_file,
        chunk->size_offset + chunk->size_entry_length*id*sizeof(INT),
        SEEK_SET);
  if (fread(chunk->size_buf, sizeof(INT),
            chunk->size_entry_length,
            chunk->result->size_file) != chunk->size_entry_length)
  {
    dserror("failed to read size file of field '%s'", chunk->result->field->name);
  }
  byteswap_ints(chunk->size_buf, chunk->size_entry_length);

#else

  chunk->size_buf = &(chunk->size_data[chunk->size_entry_length*id]);

#endif

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Read one value entry form the file and store it to this
  chunk_data's internal buffer.

  \author u.kue
  \date 11/04
*/
/*----------------------------------------------------------------------*/
void chunk_read_value_entry(CHUNK_DATA* chunk, INT id)
{
#ifdef DEBUG
  dstrc_enter("chunk_read_value_entry");
#endif

  dsassert(chunk->value_entry_length > 0, "cannot read empty entry");

#ifdef LOWMEM

  fseek(chunk->result->value_file,
        chunk->value_offset + chunk->value_entry_length*id*sizeof(DOUBLE),
        SEEK_SET);
  if (fread(chunk->value_buf, sizeof(DOUBLE),
            chunk->value_entry_length,
            chunk->result->value_file) != chunk->value_entry_length)
  {
    dserror("failed to read value file of field '%s'", chunk->result->field->name);
  }
  byteswap_doubles(chunk->value_buf, chunk->value_entry_length);

#else

  chunk->value_buf = &(chunk->value_data[chunk->value_entry_length*id]);

#endif

#ifdef DEBUG
  dstrc_exit();
#endif
}


/*----------------------------------------------------------------------*/
/*!
  \brief Read the element parameters common to all elements.

  This is just a service to make life easier and yet to do extensive
  error checking.

  \author u.kue
  \date 12/04
*/
/*----------------------------------------------------------------------*/
void get_element_params(FIELD_DATA* field,
                        INT i,
                        INT* Id, INT* el_type, INT* dis, INT* numnp)
{
#ifdef DEBUG
  dstrc_enter("get_element_params");
#endif

  chunk_read_size_entry(&(field->ele_param), i);

  *Id      = field->ele_param.size_buf[element_variables.ep_size_Id];

  *el_type = field->ele_param.size_buf[element_variables.ep_size_eltyp];
  if ((*el_type < 0) || (*el_type >= field->problem->element_type.length))
  {
    dserror("element type %d exceeds range", *el_type);
  }

  /* translate to internal value */
  *el_type = field->problem->element_type.table[*el_type];

  *dis     = field->ele_param.size_buf[element_variables.ep_size_distyp];
  if ((*dis < 0) || (*dis >= field->problem->distype.length))
  {
    dserror("element dis %d exceeds range", *dis);
  }

  *numnp   = field->ele_param.size_buf[element_variables.ep_size_numnp];

#ifdef DEBUG
  dstrc_exit();
#endif
}

#ifdef D_FSI

/*----------------------------------------------------------------------*/
/*!
  \brief Find the connection between ale and fluid nodes.

  \param problem       (i) The problem data
  \param struct_field  (i) struct field data; might be NULL
  \param fluid_field   (i) fluid field data
  \param ale_field     (i) ale field data
  \param _fluid_struct_connect  (o) per fluid node array that gives
                                    the corresponding structure nodes
  \param _fluid_ale_connect     (o) gives the corresponding ale nodes

  \warning The connection array are allocated here but must be freed
  by the caller.

  \author u.kue
  \date 10/04
*/
/*----------------------------------------------------------------------*/
void post_find_fsi_coupling(PROBLEM_DATA *problem,
                            FIELD_DATA *struct_field,
                            FIELD_DATA *fluid_field,
                            FIELD_DATA *ale_field,
                            INT **_fluid_struct_connect,
                            INT **_fluid_ale_connect)
{
  INT i;

  INT *fluid_struct_connect = NULL;
  INT *fluid_ale_connect;

#ifdef DEBUG
  dstrc_enter("post_find_fsi_coupling");
#endif

  /* read the fluid node coordinates one by one and search for
   * matching struct and ale nodes. */

  /* We have to allocate flags for all fluid nodes. */

  fluid_ale_connect = (INT*)CCACALLOC(fluid_field->numnp, sizeof(INT));
  if (struct_field != NULL) {
    fluid_struct_connect = (INT*)CCACALLOC(fluid_field->numnp, sizeof(INT));
  }

  /* This is a quadratic loop. If it turns out to be too slow one
   * could implement some quad- or octtree algorithm. */
  for (i=0; i<fluid_field->numnp; ++i) {
    INT n_ale;

    chunk_read_value_entry(&(fluid_field->coords), i);

    /* search the structure nodes */
    if (struct_field != NULL) {
      INT n_struct;

      /* no corresponding struct node by default */
      fluid_struct_connect[i] = -1;

      /* search the struct node */
      for (n_struct=0; n_struct<struct_field->numnp; ++n_struct) {
        INT k;
        DOUBLE diff = 0;

        chunk_read_value_entry(&(struct_field->coords), n_struct);

        /* quadratic error norm */
        for (k=0; k<problem->ndim; ++k) {
          DOUBLE d = fluid_field->coords.value_buf[k] - struct_field->coords.value_buf[k];
          diff += d*d;
        }

        /*
         * If the difference is very small we've found the corresponding
         * node. The tolerance here might be too big for very fine
         * meshes. I don't know. (ccarat uses the same tolerance but
         * applies it to the sqare root. I don't want to waste the time
         * to calculate it...)
         *
         * In fluid_struct_connect we store the local indices, that is
         * no real ids. */
        if (diff < 1e-10) {
          fluid_struct_connect[i] = n_struct;
          break;
        }
      }
    }

    /* search the ale nodes */

    /* no corresponding ale node by default */
    fluid_ale_connect[i] = -1;

    /* search the ale node */
    for (n_ale=0; n_ale<ale_field->numnp; ++n_ale) {
      INT k;
      DOUBLE diff = 0;

      chunk_read_value_entry(&(ale_field->coords), n_ale);

      /* quadratic error norm */
      for (k=0; k<problem->ndim; ++k) {
        DOUBLE d = fluid_field->coords.value_buf[k] - ale_field->coords.value_buf[k];
        diff += d*d;
      }

      /*
       * If the difference is very small we've found the corresponding
       * node. The tolerance here might be too big for very fine
       * meshes. I don't know. (ccarat uses the same tolerance but
       * applies it to the sqare root. I don't want to waste the time
       * to calculate it...)
       *
       * In fluid_ale_connect we store the local indices, that is no
       * real ids. */
      if (diff < 1e-10) {
        fluid_ale_connect[i] = n_ale;
        break;
      }
    }
  }

  *_fluid_struct_connect = fluid_struct_connect;
  *_fluid_ale_connect = fluid_ale_connect;

#ifdef DEBUG
  dstrc_exit();
#endif
}

#endif



/*----------------------------------------------------------------------*/
/*!
  \brief Set up a (fake) discretization.

  Create the node and element arrays, read node coordinates and mesh
  connectivity.

  \param discret       (o) Uninitialized discretization object
  \param problem       (i) problem data
  \param field         (i) general field data

  \author u.kue
  \date 10/04
*/
/*----------------------------------------------------------------------*/
void init_post_discretization(POST_DISCRETIZATION* discret,
                              PROBLEM_DATA* problem,
                              FIELD_DATA* field)
{
  INT i;
  INT j;

#ifdef DEBUG
  dstrc_enter("init_post_discretization");
#endif

  discret->field = field;

  discret->node = (NODE*)CCACALLOC(field->numnp, sizeof(NODE));
  discret->element = (ELEMENT*)CCACALLOC(field->numele, sizeof(ELEMENT));

  /*--------------------------------------------------------------------*/
  /* read the node coordinates */

  for (i=0; i<field->numnp; ++i)
  {
    discret->node[i].Id_loc = i;
    discret->node[i].proc = 0;

    chunk_read_size_entry(&(field->coords), i);
    discret->node[i].Id = field->coords.size_buf[element_variables.ep_size_Id];

    chunk_read_value_entry(&(field->coords), i);
    for (j=0; j<field->coords.value_entry_length; ++j)
      discret->node[i].x[j] = field->coords.value_buf[j];
  }

  /*--------------------------------------------------------------------*/
  /* read the mesh */

  for (i=0; i<field->numele; ++i)
  {
    INT Id;
    INT el_type;
    INT numnp;
    INT distype;
    INT j;

    chunk_read_size_entry(&(field->ele_param), i);

    Id      = field->ele_param.size_buf[element_variables.ep_size_Id];
    el_type = field->ele_param.size_buf[element_variables.ep_size_eltyp];
    distype = field->ele_param.size_buf[element_variables.ep_size_distyp];
    numnp   = field->ele_param.size_buf[element_variables.ep_size_numnp];

    /* external >= internal */
    el_type = field->problem->element_type.table[el_type];
    distype = field->problem->distype.table[distype];

    chunk_read_size_entry(&(field->mesh), i);

    discret->element[i].Id = Id;
    discret->element[i].Id_loc = i;
    discret->element[i].proc = 0;
    discret->element[i].node = (NODE**)CCACALLOC(numnp, sizeof(NODE*));
    discret->element[i].numnp = numnp;
    discret->element[i].eltyp = el_type;
    discret->element[i].distyp = distype;
    for (j=0; j<numnp; ++j) {
      discret->element[i].node[j] = &(discret->node[field->mesh.size_buf[j]]);
    }
  }

#ifdef DEBUG
  dstrc_exit();
#endif
}
