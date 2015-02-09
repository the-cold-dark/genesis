#!/bin/sh

user=$1
mode=$2
install=$3
prefix=$4

if [ "$user" = "" ]; then
    user=$USER
fi

if [ "$mode" = "" ]; then
    echo "install: No mode specified, aborting.."
    exit
fi

if [ "$install" = "" ]; then
    echo "install: No install directory specified, aborting.."
    exit
fi

if [ "$prefix" = "" ]; then
    echo "install: No prefix directory specified, aborting.."
    exit
fi

cd "$prefix/src"

inst_prog () {
    what=$1
    pre=$2
    if [ -f "$install/$what" ]; then
        echo "Moving $what to ${what}-old"
        mv "$install/$what" "$install/${what}-old"
    fi
    echo "Installing ${what}.."
    cp $pre/$what $install
    if [ "$user" != "$USER" ]; then
        chown $user "$install/$what"
    fi
    chmod $mode "$install/$what"
}

for b in genesis coldcc; do
    inst_prog $b "."
done

for b in `ls -1 $prefix/bin`; do
    inst_prog $b "$prefix/bin"
done

