BASE_URL="http://static.quattroshapes.com"

FILENAMES=("qs_adm0.zip" "qs_adm1.zip" "qs_adm1_region.zip" "qs_adm2.zip" "qs_adm2_region.zip" "qs_localadmin.zip" "qs_localities.zip" "qs_neighborhoods.zip")

DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

PREV_DIR=`pwd`

if [ "$#" -eq 1 ]; then
    OUT_DIR=$1
else
    OUT_DIR = $DIR/../../../data/quattroshapes
fi


mkdir -p $OUT_DIR

cd $OUT_DIR

for filename in ${FILENAMES[*]};
do wget $BASE_URL/$filename;
unzip $filename;
done;

cd $PREV_DIR
