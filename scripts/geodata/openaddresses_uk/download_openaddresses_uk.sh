if [ "$#" -ge 1 ]; then
    OUT_DIR=$1
else
    OUT_DIR=`pwd`
fi

set -e

OPENADDRESSES_UK_DATA_URL="https://alpha.openaddressesuk.org/addresses/download.csv?split=false&provenance=false&torrent=false"
OPENADDRESSES_UK_CSV_FILE=openaddresses_uk_download.csv.zip

wget --no-check-certificate --quiet $OPENADDRESSES_UK_DATA_URL -O $OUT_DIR/$OPENADDRESSES_UK_CSV_FILE

cd $OUT_DIR
unzip $OPENADDRESSES_UK_CSV_FILE
