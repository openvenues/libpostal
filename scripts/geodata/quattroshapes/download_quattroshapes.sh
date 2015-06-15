BASE_URL="http://static.quattroshapes.com"

FILENAMES=("qs_adm0.zip" "qs_adm1.zip" "qs_adm2.zip" "qs_localadmin.zip" "qs_localities.zip", "qs_neighborhoods.zip")

for filename in ${FILENAMES[*]};
do wget $BASE_URL/$filename
done;