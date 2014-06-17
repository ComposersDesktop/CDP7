#!/bin/bash

lowercase(){
    echo "$1" | sed "y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/"
}

OS=`lowercase \`uname\``

if  [ "$OS" == "darwin" ]; then
    MAKE=Makefile.osx
elif [ "$OS" == "linux" ]; then
    MAKE=Makefile.linux
else
    MAKE=Makefile.mingw
fi

# NB this does not clean lib projects

targets=(blur cdparams cdparams_other cdparse combine distort editsf env extend filter focus formants \
grain hfperm hilite houskeep misc modify morph new pagrab paview pitch pitchinfo pv pview repitch  \
sfutils sndinfo spec specinfo standalone strange stretch submix synth tabedit texture)

for target in ${targets[@]}
do
    cd ${target}
    if [ -e $MAKE ]; then 
        make veryclean -f $MAKE; 
    fi
    cd ..
done

cd externals
cd fastconv; make veryclean -f $MAKE; cd ..
#cd portsf; make veryclean -f $MAKE; cd ..
cd reverb; make veryclean -f $MAKE; cd ..
cd paprogs
cd listaudevs; make veryclean -f $MAKE; cd ..
cd paplay; make veryclean -f $MAKE; cd ..
cd pvplay; make veryclean -f $MAKE; cd ..
cd recsf; make veryclean -f $MAKE; cd ..
cd ..
cd ..


