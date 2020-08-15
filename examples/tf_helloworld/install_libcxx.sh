if [ ! -d libcxx ]; then
  current_dir=`pwd`
  git clone https://bitbucket.org/acassis/libcxx --depth=1
  (cd libcxx; ./install.sh $current_dir/libcxx/../../../nuttx)
  cp patches/optional.cxx ../../nuttx/libs/libxx/libcxx
  rm -rf libcxx
fi

