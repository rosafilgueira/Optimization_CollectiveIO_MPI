
N=$1
LIST=$2
SERVERLIST=$3
CLIENTLIST=$4

rm -f $SERVERLIST $CLIENTLIST

MACHINES=`cat $LIST`


J=0
for I in $MACHINES;
do
        #echo "m = $I"^M
        if [ $J -lt $N ];
        then
                echo "$I">>$SERVERLIST
                J=`expr $J + 1`
        else
                echo "$I">>$CLIENTLIST
        fi
done

