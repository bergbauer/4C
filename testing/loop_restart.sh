# loop this list
for file in $liste; do

  # write time and filename to the srceen
  number=`expr $number + 1`
  echo ' '
  echo '========================================'
  echo 'Testing ' $file '   ('$number'/'$total')'


  file_ohne=`basename $file`
  inputfile=$file

  # create Makefile
  echo
  echo '  Making Makefile ...'
  echo "  NODEPS=$NODEPS ./configure $configfile $definefile"

  . $SRC/scripts/extract-defines.sh
  . $SRC/scripts/setup-variables.sh
  . $SRC/scripts/setup-libraries.sh
  . $SRC/scripts/setup-objects.sh
  . $SRC/scripts/build-define-header.sh
  . $SRC/scripts/build-makefile.sh

  exe=$PROGRAMNAME

  # remove executable
  rm -f $exe

  # get the start-time in seconds:
  h=`date +"%H"`
  m=`date +"%M"`
  s=`date +"%S"`
  s_time=`expr $s + 60 \* $m + 60 \* 60 \* $h`

  # make the executable
  echo
  echo '  Making executable ...'
  make -f $makefile clean 2>&1 | cat > make.log
  make -f $makefile 2>&1 | cat >> make.log

  # get the end-time in seconds:
  h=`date +"%H"`
  m=`date +"%M"`
  s=`date +"%S"`
  e_time=`expr $s + 60 \* $m + 60 \* 60 \* $h`
  time=`expr $e_time - $s_time`
  mins=`expr $time / 60`
  secs=`expr $time % 60`
  if [ $secs -le 9 ]; then
    echo '    '$mins':0'$secs' mins:secs'
  else
    echo '    '$mins':'$secs' mins:secs'
  fi


  # check the success of the make
  # (look for the executable)
  if [ -a $exe ]; then
    echo '    OK'
    pass1=`expr $pass1 + 1`
    chmod 755 $exe
  else
    echo '    Failed!!'

    # save the log on failture
    mv make.log $file_ohne.make.log
    cat $file_ohne.make.log > /dev/stderr

    fail1=`expr $fail1 + 1`
    continue
  fi





  # get the start-time in seconds:
  h=`date +"%H"`
  m=`date +"%M"`
  s=`date +"%S"`
  s_time=`expr $s + 60 \* $m + 60 \* 60 \* $h`




  if [ "x$PARALLEL" = "xno" ]; then
    # run the executable with the inputfile
    echo
    echo '  Running Input-file...'
    ./$exe $inputfile test_out >test.tmp

  else
    # parallel run
    echo
    echo '  Running Input-file in parallel...'
    $MPIRUN -np 2 $MPIRUNARGS ./$exe $inputfile test_out >test.tmp
    killall $exe >& /dev/null

  fi


  # get the end-time in seconds:
  h=`date +"%H"`
  m=`date +"%M"`
  s=`date +"%S"`
  e_time=`expr $s + 60 \* $m + 60 \* 60 \* $h`
  time=`expr $e_time - $s_time`
  mins=`expr $time / 60`
  secs=`expr $time % 60`
  if [ $secs -le 9 ]; then
    echo '    '$mins':0'$secs' mins:secs'
  else
    echo '    '$mins':'$secs' mins:secs'
  fi



  # evaluate the output
  # (look for the word 'normally' in the output
  count=`cat test.tmp | grep -c "normally"`
  if [ $count -ge 1 ]; then
    echo '    OK'
  else
    fail2=`expr $fail2 + 1`
    echo '    Failed!!'
    # copy screen output to $file_ohne.scr
    mv test.tmp $file_ohne.scr
    cat $file_ohne.scr > /dev/stderr
    continue
  fi



  # get the start-time in seconds:
  h=`date +"%H"`
  m=`date +"%M"`
  s=`date +"%S"`
  s_time=`expr $s + 60 \* $m + 60 \* 60 \* $h`




  if [ "x$PARALLEL" = "xno" ]; then
    # run the executable with the inputfile

    # modify the file
    sed -e 's/^RESTART/\/\/RESTART/g' -e 's/\/\/\!RESTART/RESTART/g' $inputfile > restart_input.dat

    echo '  Running Restart of Input-file...'
    ./$exe restart_input.dat test_out restart >test2.tmp

  else
    # parallel run

    # modify the file
    sed -e 's/^RESTART/\/\/RESTART/g' -e 's/\/\/\!RESTART/RESTART/g' $inputfile > restart_input.dat

    echo '  Running Restart of Input-file in parallel...'
    $MPIRUN -np 2 $MPIRUNARGS ./$exe restart_input.dat test_out restart >test2.tmp
    killall $exe >& /dev/null

  fi



  # get the end-time in seconds:
  h=`date +"%H"`
  m=`date +"%M"`
  s=`date +"%S"`
  e_time=`expr $s + 60 \* $m + 60 \* 60 \* $h`
  time=`expr $e_time - $s_time`
  mins=`expr $time / 60`
  secs=`expr $time % 60`
  if [ $secs -le 9 ]; then
    echo '    '$mins':0'$secs' mins:secs'
  else
    echo '    '$mins':'$secs' mins:secs'
  fi



  # evaluate the output
  # (look for the word 'normally' in the output
  count=`cat test2.tmp | grep -c "normally"`
  if [ $count -ge 1 ]; then
    echo '    OK'
    pass2=`expr $pass2 + 1`
  else
    echo '    Failed!!'
    fail2=`expr $fail2 + 1`
    # copy screen output to $file_ohne.scr
    mv test2.tmp $file_ohne.scr_res
    cat $file_ohne.scr_res > /dev/stderr
    continue
  fi



done
# end of the loop over all input files
