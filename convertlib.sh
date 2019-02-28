#!/bin/sh

mkdir -p samples run

cd run
for i in `seq 424` ; do
  m=`gamslib $i | grep -o "[^ ]*\.gms"`
  
  [ $m == slvtest.gms ] && continue
  [ $m == gmstest.gms ] && continue
  [ $m == deploy.gms ] && continue
  [ $m == schulz.gms ] && continue
  
  out=../samples/${m/gms/mosdex}
  echo
  echo $m
  
  rm -rf loadgms.tmp  
  ../gams2mosdex $m > $out || rm $out

done
