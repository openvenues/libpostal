if [ "$#" -ge 1 ]; then
    DATA_DIR=$1
else
    DATA_DIR=$(pwd)
fi

PWD=$(pwd)
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

python $SCRIPT_DIR/chains_tsv.py $DATA_DIR/planet-venues.osm $DATA_DIR/chains.tsv

cd $DATA_DIR
split -d -C524200 chains.tsv chains.split.

for filename in chains.split.*; do 
    extension="${filename##*.0}"
    name="${filename%%.*}"
    echo -e "name_lower\tname\tcanonical\tknown_chain\tcount" | cat - $filename > /tmp/out
    mv /tmp/out $name.$extension.tsv
    rm $filename
done

cd $PWD
