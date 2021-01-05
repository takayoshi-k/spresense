current_dir=`pwd`
copy_dir=working
target_dir=tensorflow/tensorflow/lite

cd $copy_dir
copy_things=( "`tree -fi micro`" )
cd $current_dir

for f_or_d in $copy_things
do
  if [ -d $copy_dir/$f_or_d ]; then
    if [ ! -d $target_dir/$f_or_d ]; then
      echo make dir $target_dir/$f_or_d
      mkdir -p $target_dir/$f_or_d
    fi
  elif [ -f $copy_dir/$f_or_d ]; then
    if [ ! -f $target_dir/$f_or_d ]; then
      echo copy file to  $target_dir/$f_or_d
      cp $copy_dir/$f_or_d $target_dir/$f_or_d
    fi
  fi

done


