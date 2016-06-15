#/bin/sh
set -x
NUMBER=$1
FILE=$2

FILESERVER=pvfs_machines_orig
FILECLIENT=mpi_machines

rm -f $FILECLIENT
rm -f $FILESERVER


N=`cat $FILE | wc -l`

if [ $N -l $NUMBER ]
then
         echo "Thee are less machines availables ("$N") that the number requested ("$NUMBER")"
         $FILESERVER=$FILE
    exit
fi
./split.sh $NUMBER $FILE $FILESERVER $FILECLIENT
