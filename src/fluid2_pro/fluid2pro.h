/*!---------------------------------------------------------------------
\file
\brief The 2D fluid element used for projection methods

------------------------------------------------------------------------*/

/*!---------------------------------------------------------------------
\brief fluid2                                                 

<pre>                                                         genk 03/02 

In this structure all variables used for element evaluation by the 2D
fluid element fluid2_pro are stored.

</pre>

--------------------------------------------------------------------------*/
typedef struct _FLUID2_PRO
{

int                ntyp;     /*!< flag for element type: 1=quad; 2=tri    */
int                nGP[2];   /*!< number of gaussian points in rs direct. */
int                is_ale;   /*!< flag whether there is ale to me or not  */
struct _ELEMENT   *my_ale;   /*!< pointer to my ale ele, otherwise NULL   */
enum   _DISMODE    dm;                            

} FLUID2_PRO;

